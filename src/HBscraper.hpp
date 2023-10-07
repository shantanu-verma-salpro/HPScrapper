#ifndef ASYNC
#define ASYNC

#include <iostream>
#include <functional>
#include <vector>
#include <memory>
#include <stdexcept>

#include "../include/async/EventLoop.hpp"
#include "../include/async/TimerWrapper.hpp"
#include "../include/async/CheckWrapper.hpp"
#include "../include/async/IdleWrapper.hpp"
#include "../include/async/PrepareWrapper.hpp"
#include "../include/async/PollWrapper.hpp"

#include "../include/net/CurlHandlePool.hpp"
#include "../include/net/CurlMultiWrapper.hpp"
#include "../include/net/URLRequestManager.hpp"

#include "../include/parser/Document.hpp"
#include "../include/parser/Parser.hpp"


class Async{
    std::size_t curl_pool_sz , curl_buf_sz;
    int delay_exit { 0 };
    long total_connection , total_host_connection , timeout; 
    EventLoop loop { };
    CheckWrapper idler { loop };
    TimerWrapper timer { loop };
    TimerWrapper delay_timer { loop };
    URLRequestManager url_manager {};
    CurlHandlePool pool;
    CurlMultiWrapper multi;
    Parser parser {};
    bool print_req_info { true };
    std::ostream* out { &std::cout };

    using Sclb = std::function<void(const CurlEasyHandle::Response& response , Async& , Document&)>;
    using Fclb = std::function<void(const CurlEasyHandle::Response& response , Async&)>;
    using Iclb = std::function<void(long pending , Async&)>;
    using Eclb = std::function<void(const std::exception& e , Async&)>;

    Eclb onExceptionclb;
    Sclb onSuccessclb;
    Fclb onFailureclb;
    Iclb onIdleclb;

    static int timeout_function(CURLM *multi, long timeout_ms, void *userp) {
        auto self = static_cast<Async*>(userp);
        if (timeout_ms < 0) self->timer.stop();
        else self->timer.start(timeout_ms ? timeout_ms : 1 , 0);
        return 0;
    }

    static void processSuccessfulRequest(const CurlEasyHandle::Response& response, Async* self){    
        if (response.responseCode != 200)
            return;
        Document dom = self->parser.createDOM(response.message);
        if(self->onSuccessclb) self->onSuccessclb(response, *self, dom);     
    }

    static void processFailedRequest(const CurlEasyHandle::Response& response , CURLMsg *m , Async* self){
        const std::string message ("Connection failure (" + std::string(curl_easy_strerror(m->data.result)) + "): " + response.url);
        *(self->out) << message << '\n';
        if(self->onFailureclb) self->onFailureclb(response, *self);
    }

    static void process_curl(Async* self){
        CURLMsg *message = nullptr;
        int pending = 0;
        CurlEasyHandle *ctx = nullptr;
        while ((message = self->multi.readMulti(pending))){
            if( message->msg == CURLMSG_DONE){   
                curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &ctx);
                std::unique_ptr<CurlEasyHandle> handle(ctx);
                auto response = handle->response();

                if(message->data.result == CURLE_OK) processSuccessfulRequest(*response.get(), self);
                else processFailedRequest(*response.get(), message, self);

                self->multi.removeHandle(handle->get());
                self->pool.release(std::move(handle));  
            }
        }
    }

    void processURLs() {
        CurlEasyHandle* handle;
        while (url_manager.hasURLs() && !pool.isEmpty()) {
            auto handle = pool.acquire();
            auto url_pair = url_manager.popURL();
            handle->setUrl(url_pair.first.c_str() , url_pair.second);
            CurlEasyHandle* h = handle.release();
            multi.addHandle(h->get());
        }
    }

    static int socket_function(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp) {
        auto self = static_cast<Async*>(userp);
        PollWrapper* poll = static_cast<PollWrapper*>(socketp);

        if(action == CURL_POLL_REMOVE && poll){
            poll->close([](PollWrapper* poll){ delete poll; });
            self->multi.assign(s, nullptr);
            return 0;
        }

        const int ev =(action == CURL_POLL_IN) ? UV_READABLE :(action == CURL_POLL_OUT) ? UV_WRITABLE: (action == CURL_POLL_INOUT) ? (UV_READABLE | UV_WRITABLE) : 0;
        if(!poll && ev){
            poll = new PollWrapper(self->loop , s);
            poll->on<PollEvent,PollWrapper>([self](const PollEvent& event, PollWrapper& wrapper){
                int flags = 0;
                if(event.status < 0) flags = CURL_CSELECT_ERR;
                if(!event.status && event.events & UV_READABLE) flags |= CURL_CSELECT_IN;
                if(!event.status && event.events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;

                self->multi.socketAction(wrapper.getFd() , flags);
                process_curl(self);
            });

            self->multi.assign(s , poll);
        }
        if (ev) poll->start(ev);
        return 0;
    }

    void initDispatchers(){
         idler.on<CheckEvent,CheckWrapper>([self = this](const CheckEvent& , CheckWrapper& wrapper){
            self->processURLs(); 
            if(self->onIdleclb) self->onIdleclb(self->multi.getPending() ,*self);
            if(!self->url_manager.hasURLs() && self->multi.getPending() == 0 && !self->delay_timer.isActive()) {
                self->delay_timer.start( 2000 + self->delay_exit , 0);
            }
        });

        delay_timer.on<TimerEvent,TimerWrapper>([self = this](const TimerEvent& , TimerWrapper& wrapper){
            if(!self->url_manager.hasURLs() && self->multi.getPending() == 0) {
                self->closeProcessing();
            }
        });

        timer.on<TimerEvent,TimerWrapper>([self = this](const TimerEvent& , TimerWrapper& wrapper ){
            self->multi.socketAction(CURL_SOCKET_TIMEOUT, 0);
            process_curl(self);
        });
        
    }

public:
    Async(long tc = 10 , long hc = 10 , std::size_t bz = 1024 , long tm = 50000) :
    total_connection(tc) ,
    total_host_connection(hc) ,
    curl_pool_sz (tc) , 
    curl_buf_sz(bz) , 
    timeout(tm),
    multi(tc,hc),
    pool(tc,bz,tm)
    {
        curl_global_init(CURL_GLOBAL_ALL);
        multi.addSocketCallbackData(socket_function , static_cast<void*>(this));
        multi.addTimeoutCallbackData(timeout_function , static_cast<void*>(this));
        initDispatchers();
    }

    void closeProcessing(){
        if(idler.isActive()) idler.stop();
    }

    Async(const Async&) = delete;
    Async& operator=(const Async&) = delete;
    Async(Async&&) = default;
    Async& operator=(Async&&) = default;

    ~Async(){
        curl_global_cleanup();
    }

    void setRequestLogStream(std::ostream& _stream = std::cout){
        out = &_stream;
    }

    void setMultiplexing(bool val){
        multi.setMultiplex(val);
        pool.propagateMultiplexing(val);
    }

    void setUserAgent(const std::string& ua){
        pool.propagateUserAgent(ua);
    }

    void setMaxRedirections(const long l){
        pool.propagateMaxRedirections(l);
    }

    void setFollowRedirects(bool follow) noexcept {
        pool.propagateFollowRedirects(follow);
    }

    void setReferer(const std::string& referer) noexcept {
        pool.propagateReferer(referer);
    }

    void setCookieFile(const std::string& cookieFile) noexcept {
        pool.propagateCookieFile(cookieFile);
    }

    void setCookieJar(const std::string& cookieJar) noexcept {
        pool.propagateCookieJar(cookieJar);
    }

    void setCookies(const std::string& ck){
        pool.propagateCookies(ck);
    }

    void setVerbose(bool verbose) noexcept {
        pool.propagateVerbose(verbose);
    }

    void setBearerToken(const std::string& token) noexcept {
        pool.propagateBearerToken(token);
    }

    void setPostFields(const std::string& postFields) noexcept {
        pool.propagatePostFields(postFields);
    }

    void setInterface(const std::string& inter) noexcept {
        pool.propagateInterface(inter);
    }

    void setSSLUsage(bool useSSL) noexcept {
        pool.propagateSSLUsage(useSSL);
    }

    void setVerify(bool verify) noexcept {
        pool.propagateVerify(false);
    }

    void setBasicAuth(const std::string& username, const std::string& password) noexcept {
        pool.propagateBasicAuth(username,password);
    }

    void setDigestAuth(const std::string& username, const std::string& password) noexcept {
        pool.propagateDigestAuth(username,password);
    }

    void setNTLMAuth(const std::string& domain, const std::string& username, const std::string& password) noexcept {
        pool.propagateNTLMAuth(domain,username,password);
    }

    void setClientCertificate(const std::string& certPath, const std::string& keyPath) noexcept {
        pool.propagateClientCertificate(certPath,keyPath);
    }

    void setProxy(const std::string& proxyUrl, int proxyPort = 0) noexcept {
        pool.propagateProxy(proxyUrl,proxyPort);
    }

    void setProxyAuth(const std::string& username, const std::string& password) noexcept {
        pool.propagateProxyAuth(username,password);
    }

    void setLimitMBytesPerSec(double mbytesPerSec) noexcept {
        pool.propagateLimitMBytesPerSec(mbytesPerSec);
    }

    void setHttpVersion(const HTTP ver){
        pool.propagateHttpVersion(ver);
    }

    void setHeader(const std::string& head){
        pool.propagateHeader(head);
    }

    void setHeaders(const std::vector<std::string>& head){
        pool.propagateHeaders(head);
    }

    void clearHeaders(){
        pool.propogateClearHeaders();
    }

    void resetPool(){
        pool.propagateReset();
    }

    void forceGetRequests(const bool val){
        pool.propogateGet(val);
    }

    void forcePostRequests(const bool val){
        pool.propogatePost(val);
    }

    void setPoolBufferSize(const std::size_t sz){
        pool.propogateBufferSize(sz);
    }

    void setConnectionTimeout(const long tm){
        pool.propogateTimeout(tm);
    }

    inline void run() {
        try {
            idler.start();
            loop.run();
        } catch (const std::exception& e) {
            if(onExceptionclb) {
                onExceptionclb(e,*this);
            } else {
                throw; 
            }
        }
    }

    inline void clearQueue(){
        url_manager.clear();
    }

    inline const std::size_t pendingUrlsQueueSize() const noexcept{
        return url_manager.getPendingUrlQueueSize();
    }

    const std::unordered_set<std::string>& getVisitedUrls(){
        return url_manager.getVisited();
    }

    void onSuccess(const Sclb& clb) noexcept{
        onSuccessclb = clb;
    }

    void onFailure(const Fclb& clb) noexcept{
        onFailureclb = clb;
    }

    void setShowRequestInfo(const bool val) noexcept{
        print_req_info = val;
    }

    void resetOptions(){
        pool.propagateReset();
    }

    void onException(const Eclb& clb) noexcept{
        onExceptionclb = clb;
    }

    void onIdle(const Iclb& clb) noexcept{
        onIdleclb = clb;
    }

    void setDelayExitMs(int ms)  noexcept{
        delay_exit = ms;
    }

    void addURL(const std::string& url,const std::size_t depth){
        url_manager.addURL(url,depth);
    }

    void seed(const std::string& url){
        url_manager.addURL(url,0);
        processURLs();
    }

   
};

#endif