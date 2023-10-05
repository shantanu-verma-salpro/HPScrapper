#pragma GCC optimize("Ofast")
#pragma GCC target("avx,avx2,fma")

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <functional>
#include <vector>
#include <memory>
#include <regex>
#include <stdexcept>
#include <curl/curl.h>
#include <uv.h>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>

class EventLoop {
    private:
    uv_loop_t* loop;
public:
    EventLoop() noexcept {
        loop = uv_default_loop();
    }

    explicit EventLoop(uv_loop_t* custom_loop) noexcept : loop(custom_loop) {}

    ~EventLoop() noexcept {
        if (int result = uv_loop_close(loop); result == UV_EBUSY) {
            uv_walk(loop, on_uv_walk, nullptr);
            uv_loop_close(loop);
        }
    }

    inline uv_loop_t* getLoop() const noexcept {
        return loop;
    }

    inline void run(uv_run_mode mode = UV_RUN_DEFAULT) noexcept {
        uv_run(loop, mode);
    }

    template<typename... Args>
    int configure(uv_loop_option option, Args... args) {
        return uv_loop_configure(loop, option, args...);
    }

    inline void stop() noexcept {
        uv_stop(loop);
    }

    inline void update_time() noexcept {
        uv_update_time(loop);
    }

    inline const uint64_t now() const noexcept {
        return uv_now(loop);
    }

    inline int backend_fd() const noexcept {
        return uv_backend_fd(loop);
    }

    inline int backend_timeout() const noexcept {
        return uv_backend_timeout(loop);
    }

    inline void* data() const noexcept {
        return uv_loop_get_data(loop);
    }

    inline void set_data(void* data) noexcept {
        uv_loop_set_data(loop, data);
    }

private:

    static inline void on_uv_walk(uv_handle_t* handle, void* arg) noexcept {
        uv_close(handle, on_uv_close);
    }

    static inline void on_uv_close(uv_handle_t* handle) noexcept {
        // if(handle) delete handle;
    }
};

class Event {
public:
    virtual ~Event() noexcept = default;
};

class IdleEvent final : public Event {};
class TimerEvent final : public Event {};
class CheckEvent final : public Event {};
class PrepareEvent final : public Event {};
class PollEvent final : public Event {
public:
    int status{};
    int events{};
};

class HandleWrapperBase;

class EventDispatcher {

    using CallbackType = std::function<void(const Event&, HandleWrapperBase&)>;

public:
    
    static EventDispatcher& getInstance() {
        static EventDispatcher instance;
        return instance;
    }

    void on(HandleWrapperBase* handle, CallbackType callback) noexcept {
        callbacks.emplace(handle, std::move(callback));
    }

    void dispatch(const Event& event, HandleWrapperBase& handle) const noexcept {
        auto it = callbacks.find(&handle);
        if (it != callbacks.end()) {
            it->second(event, handle);
        }
    }

    void remove(HandleWrapperBase* handle) noexcept {
        callbacks.erase(handle);
    }

private:

    EventDispatcher() = default;
    std::unordered_map<HandleWrapperBase*, CallbackType> callbacks;
};

class HandleWrapperBase {

protected:
    static inline void check_uv_error(const int ret) { if (ret != 0) throw std::runtime_error(uv_strerror(ret)); }

public:

    virtual ~HandleWrapperBase() noexcept {
        EventDispatcher::getInstance().remove(this);
    }

    virtual const bool isActive() const noexcept = 0;
    virtual const bool isClosing() const noexcept = 0;

    template<typename EventType, typename DerivedType, typename Callback>
    void on(Callback&& callback) noexcept {
        auto adapter = [callback = std::forward<Callback>(callback)] (const Event& event, HandleWrapperBase& handle) noexcept {
            callback(static_cast<const EventType&>(event), static_cast<DerivedType&>(handle));
        };
        EventDispatcher::getInstance().on(this, adapter);
    }
};

class IdleWrapper final : public HandleWrapperBase {

public:
    explicit IdleWrapper(const EventLoop& loop) noexcept {
        check_uv_error(uv_idle_init(loop.getLoop(), &idle_handle));
        idle_handle.data = this;
    }

    ~IdleWrapper() noexcept override {
        close();
    }

    void close(std::function<void(IdleWrapper*)> closeCb = nullptr) noexcept {
        if (!isClosing()) {
            close_callback = closeCb;  
            uv_close(reinterpret_cast<uv_handle_t*>(&idle_handle), uv_close_callback);
        }
    }

    void start() noexcept  { check_uv_error(uv_idle_start(&idle_handle, uv_idle_callback)); }
    void stop() noexcept { check_uv_error(uv_idle_stop(&idle_handle)); }
    const bool isActive() const noexcept override { return uv_is_active(reinterpret_cast<const uv_handle_t*>(&idle_handle)); }
    const bool isClosing() const noexcept override { return uv_is_closing(reinterpret_cast<const uv_handle_t*>(&idle_handle)); }

private:
    uv_idle_t idle_handle{};
    std::function<void(IdleWrapper*)> close_callback{};

    static void uv_idle_callback(uv_idle_t* handle) noexcept {
        auto* self = static_cast<IdleWrapper*>(handle->data);
        IdleEvent event;
        EventDispatcher::getInstance().dispatch(event, *self);
    }

    static void uv_close_callback(uv_handle_t* handle) noexcept {
        auto* self = static_cast<IdleWrapper*>(handle->data);
        EventDispatcher::getInstance().remove(self);
        if (self->close_callback) {
            self->close_callback(self);
        }
    }
};

class TimerWrapper final : public HandleWrapperBase {

public:
    explicit TimerWrapper(const EventLoop& loop) noexcept {
        check_uv_error(uv_timer_init(loop.getLoop(), &timer_handle));
        timer_handle.data = this;
    }

    ~TimerWrapper() noexcept override {
        close();
    }

    void close(std::function<void(TimerWrapper*)> closeCb = nullptr) noexcept {
        if (!isClosing()) {
            close_callback = closeCb;  
            uv_close(reinterpret_cast<uv_handle_t*>(&timer_handle), uv_close_callback);
        }
    }

    void start(uint64_t timeout, uint64_t repeat) noexcept{
        check_uv_error(uv_timer_start(&timer_handle, uv_timer_callback, timeout, repeat));
    }

    void stop() noexcept { check_uv_error(uv_timer_stop(&timer_handle)); }
    void again() noexcept { check_uv_error(uv_timer_again(&timer_handle));}
    void setRepeat(uint64_t repeat) noexcept { uv_timer_set_repeat(&timer_handle, repeat); }
    const uint64_t getRepeat() const noexcept { return uv_timer_get_repeat(&timer_handle); }
    const bool isActive() const noexcept override { return uv_is_active(reinterpret_cast<const uv_handle_t*>(&timer_handle)); }
    const bool isClosing() const noexcept override { return uv_is_closing(reinterpret_cast<const uv_handle_t*>(&timer_handle)); }

private:
    uv_timer_t timer_handle{};
    std::function<void(TimerWrapper*)> close_callback{};

    static void uv_timer_callback(uv_timer_t* handle) noexcept {
        auto* self = static_cast<TimerWrapper*>(handle->data);
        TimerEvent event;
        EventDispatcher::getInstance().dispatch(event, *self);
    }

    static void uv_close_callback(uv_handle_t* handle) noexcept {
        auto* self = static_cast<TimerWrapper*>(handle->data);
        EventDispatcher::getInstance().remove(self);
        if (self->close_callback) {
            self->close_callback(self);
        }
    }
};

class CheckWrapper final : public HandleWrapperBase {

public:
    explicit CheckWrapper(const EventLoop& loop) noexcept {
        check_uv_error(uv_check_init(loop.getLoop(), &check_handle));
        check_handle.data = this;
    }

    ~CheckWrapper() noexcept override {
        close();
    }

    void close(std::function<void(CheckWrapper*)> closeCb = nullptr) noexcept {
        if (!isClosing()) {
            close_callback = closeCb;  
            uv_close(reinterpret_cast<uv_handle_t*>(&check_handle), uv_close_callback);
        }
    }

    void start() noexcept  {
        check_uv_error(uv_check_start(&check_handle, uv_check_callback));
    }

    void stop() noexcept {
        check_uv_error(uv_check_stop(&check_handle));
    }

    const bool isActive() const noexcept override {
        return uv_is_active(reinterpret_cast<const uv_handle_t*>(&check_handle));
    }

    const bool isClosing() const noexcept override {
        return uv_is_closing(reinterpret_cast<const uv_handle_t*>(&check_handle));
    }

private:
    uv_check_t check_handle{};
    std::function<void(CheckWrapper*)> close_callback{};

    static void uv_check_callback(uv_check_t* handle) noexcept {
        auto* self = static_cast<CheckWrapper*>(handle->data);
        CheckEvent event;
        EventDispatcher::getInstance().dispatch(event, *self);
    }

    static void uv_close_callback(uv_handle_t* handle) noexcept {
        auto* self = static_cast<CheckWrapper*>(handle->data);
        EventDispatcher::getInstance().remove(self);
        if (self->close_callback) {
            self->close_callback(self);
        }
    }
};

class PrepareWrapper final : public HandleWrapperBase {

public:
    explicit PrepareWrapper(const EventLoop& loop) noexcept {
        check_uv_error(uv_prepare_init(loop.getLoop(), &prepare_handle));
        prepare_handle.data = this;
    }

    ~PrepareWrapper() noexcept override {
        close();
    }

    void close(std::function<void(PrepareWrapper*)> closeCb = nullptr) noexcept {
        if (!isClosing()) {
            close_callback = closeCb;  
            uv_close(reinterpret_cast<uv_handle_t*>(&prepare_handle), uv_close_callback);
        }
    }

    void start() noexcept {
        check_uv_error(uv_prepare_start(&prepare_handle, uv_prepare_callback));
    }

    void stop() noexcept {
        check_uv_error(uv_prepare_stop(&prepare_handle));
    }

    const bool isActive() const noexcept override {
        return uv_is_active(reinterpret_cast<const uv_handle_t*>(&prepare_handle));
    }

    const bool isClosing() const noexcept override {
        return uv_is_closing(reinterpret_cast<const uv_handle_t*>(&prepare_handle));
    }

private:
    uv_prepare_t prepare_handle{};
    std::function<void(PrepareWrapper*)> close_callback{};

    static void uv_prepare_callback(uv_prepare_t* handle) noexcept {
        auto* self = static_cast<PrepareWrapper*>(handle->data);
        PrepareEvent event;
        EventDispatcher::getInstance().dispatch(event, *self);
    }

    static void uv_close_callback(uv_handle_t* handle) noexcept {
        auto* self = static_cast<PrepareWrapper*>(handle->data);
        EventDispatcher::getInstance().remove(self);
        if (self->close_callback) {
            self->close_callback(self);
        }
    }
};

class PollWrapper final : public HandleWrapperBase {

public:
    explicit PollWrapper(const EventLoop& loop, int fd) noexcept {
        this->fd = fd;
        check_uv_error(uv_poll_init_socket(loop.getLoop(), &poll_handle, fd));
        poll_handle.data = this;
    }

    ~PollWrapper() noexcept override {
        close();
    }

    void close(std::function<void(PollWrapper*)> closeCb = nullptr) noexcept {
        if (!isClosing()) {
            close_callback = closeCb;  
            uv_close(reinterpret_cast<uv_handle_t*>(&poll_handle), uv_close_callback);
        }
    }

    const auto& getFd() const noexcept{
        return fd;
    }

    void start(int events) noexcept {
        check_uv_error(uv_poll_start(&poll_handle, events, uv_poll_callback));
    }

    void stop() noexcept {
        check_uv_error(uv_poll_stop(&poll_handle));
    }

    const bool isActive() const noexcept override {
        return uv_is_active(reinterpret_cast<const uv_handle_t*>(&poll_handle));
    }

    const bool isClosing() const noexcept override {
        return uv_is_closing(reinterpret_cast<const uv_handle_t*>(&poll_handle));
    }

private:
    curl_socket_t fd {};
    uv_poll_t poll_handle{};
    std::function<void(PollWrapper*)> close_callback{};

    static void uv_poll_callback(uv_poll_t* handle, int status, int events) noexcept {
        auto* self = static_cast<PollWrapper*>(handle->data);
        PollEvent event;
        event.status = status;
        event.events = events;
        EventDispatcher::getInstance().dispatch(event, *self);
    }

    static void uv_close_callback(uv_handle_t* handle) noexcept {
        auto* self = static_cast<PollWrapper*>(handle->data);
        EventDispatcher::getInstance().remove(self);
        if (self->close_callback) {
            self->close_callback(self);
        }
    }
};

enum HTTP {
    HTTP1 = CURL_HTTP_VERSION_1_0,
    HTTP1_1 = CURL_HTTP_VERSION_1_1,
    #ifdef CURL_VERSION_HTTP2
    HTTP2 = CURL_HTTP_VERSION_2_0,
    #endif
    #ifdef CURL_VERSION_HTTP3
    HTTP3 = CURL_HTTP_VERSION_3,
    #endif
};

class CurlEasyHandle {
    std::size_t curl_buffer_sz;
    long curl_mstimeout;
    std::size_t depth;
    std::string buf;
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl_handle_ ;
    std::unique_ptr<struct curl_slist, decltype(&curl_slist_free_all)> headers_;
    
    static inline void checkSetOpt(const CURLcode&& res, const char* opt_name) noexcept{
        if(res != CURLE_OK) std::cerr << "Failed to set " << opt_name << ": " << curl_easy_strerror(res) << '\n';
    }

    template<typename T>
    inline void setOption(const CURLoption option, const T value, const char* opt_name) noexcept{
        checkSetOpt(curl_easy_setopt(curl_handle_.get(), option, value), opt_name);
    }

    static std::size_t write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
        const std::size_t totalSize = size * nmemb;
        static_cast<std::string*>(userdata)->append(ptr, totalSize);
        return totalSize;
    }

    void setInternalOptions() noexcept{
        setOption(CURLOPT_PRIVATE,static_cast<void*>(this), "CURLOPT_PRIVATE");
        setOption(CURLOPT_MAXFILESIZE, 2 * 1024 * 1024,"CURLOPT_MAXFILESIZE");
        setOption(CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4, "CURLOPT_IPRESOLVE");
        setOption(CURLOPT_TCP_NODELAY, 1L, "CURLOPT_TCP_NODELAY");
        setOption(CURLOPT_CONNECTTIMEOUT_MS, 6000, "CURLOPT_CONNECTTIMEOUT_MS");
        setOption(CURLOPT_EXPECT_100_TIMEOUT_MS, 0L, "CURLOPT_EXPECT_100_TIMEOUT_MS");
        setOption(CURLOPT_AUTOREFERER, 1L, "CURLOPT_AUTOREFERER");
        #if LIBCURL_VERSION_NUM >= 0x071900
            setOption(CURLOPT_TCP_KEEPALIVE, 1L, "CURLOPT_TCP_KEEPALIVE");
        #endif
    }

    void initialiseInitialOptions() noexcept{
        setInternalOptions();
        setWriteCallback(write_callback , &buf);
        setHTTPVersion(HTTP::HTTP1_1);
        setBufferSize(curl_buffer_sz);
        setAcceptEncoding("");
        setFollowRedirects(true);
        setMaxRedirections(1);
        setTimeoutMs(curl_mstimeout);
        setCookieFile("");
        setUserAgent("Scraper / 1.1");
        setVerify(false);
    }

public:

    CurlEasyHandle(const std::size_t buffer_sz, const long timeout): curl_buffer_sz(buffer_sz), curl_mstimeout(timeout),depth(0),
    curl_handle_(curl_easy_init(), &curl_easy_cleanup),
    headers_(nullptr, &curl_slist_free_all)
    {
            buf.reserve(buffer_sz);
            if (!curl_handle_) throw std::runtime_error("Failed to initialize curl handle.");
                initialiseInitialOptions();
    }

    CurlEasyHandle(const CurlEasyHandle&) = delete;
    CurlEasyHandle& operator=(const CurlEasyHandle&) = delete;
    CurlEasyHandle(CurlEasyHandle&& other) =  delete;
    CurlEasyHandle& operator=(CurlEasyHandle&& other) = delete;

    struct Response {
        std::string contentType;
        std::string httpMethod;
        std::string url;
        std::string httpVersion;
        double totalTime;
        curl_off_t bytesRecieved;
        curl_off_t bytesSent;
        curl_off_t bytesPerSecondR;
        curl_off_t bytesPerSecondS;
        long headerSize;
        long requestSize;
        long responseCode;
        const std::size_t depth;
        const std::string& message;

        Response(CURL* handle , const std::size_t d, const std::string& buf) : depth(d), message(buf) {
            char* tempStr; 

            if (CURLE_OK == curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &tempStr) && tempStr)
                contentType = tempStr;

            if (CURLE_OK == curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &tempStr) && tempStr)
                url = tempStr;

            if (CURLE_OK == curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_METHOD, &tempStr) && tempStr)
                httpMethod = tempStr;

            long httpVer;
            if (CURLE_OK == curl_easy_getinfo(handle, CURLINFO_HTTP_VERSION, &httpVer)) {
                switch (httpVer) {
                    case CURL_HTTP_VERSION_1_0: httpVersion = "HTTP/1.0"; break;
                    case CURL_HTTP_VERSION_1_1: httpVersion = "HTTP/1.1"; break;
                    case CURL_HTTP_VERSION_2_0: httpVersion = "HTTP/2"; break;
                    case CURL_HTTP_VERSION_3: httpVersion = "HTTP/3"; break;
                    default: httpVersion = "Unknown";
                }
            }
            curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME, &totalTime);
            curl_easy_getinfo(handle, CURLINFO_SIZE_DOWNLOAD_T, &bytesRecieved);
            curl_easy_getinfo(handle, CURLINFO_SIZE_UPLOAD_T, &bytesSent);
            curl_easy_getinfo(handle, CURLINFO_SPEED_DOWNLOAD_T, &bytesPerSecondR);
            curl_easy_getinfo(handle, CURLINFO_SPEED_UPLOAD_T, &bytesPerSecondS);
            curl_easy_getinfo(handle, CURLINFO_HEADER_SIZE, &headerSize);
            curl_easy_getinfo(handle, CURLINFO_REQUEST_SIZE, &requestSize);
            curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &responseCode);
        }
    };

    template<typename T>
    void setWriteCallback(const curl_write_callback cb, T buffer) noexcept {
        static_assert(!std::is_fundamental<T>::value,"Buffer of fundamental type is not allowed");
        setOption(CURLOPT_WRITEFUNCTION, cb, "CURLOPT_WRITEFUNCTION");
        setOption(CURLOPT_WRITEDATA, static_cast<void*>(buffer), "CURLOPT_WRITEDATA");
    }

    template<typename T>
    void setReadCallback(const curl_read_callback cb, T buffer) noexcept {
        static_assert(!std::is_fundamental<T>::value,"Buffer of fundamental type is not allowed");
        setOption(CURLOPT_READFUNCTION, cb, "CURLOPT_READFUNCTION");
        setOption(CURLOPT_READDATA, static_cast<void*>(buffer), "CURLOPT_READDATA");
    }

    template<typename T>
    void setProgressCallback(const curl_progress_callback cb, T clientData) noexcept {
        static_assert(!std::is_fundamental<T>::value,"Data of fundamental type is not allowed for progress callback");
        if (cb) setOption(CURLOPT_XFERINFOFUNCTION, cb, "CURLOPT_XFERINFOFUNCTION");
        setOption(CURLOPT_XFERINFODATA, static_cast<void*>(clientData), "CURLOPT_XFERINFODATA");
        setOption(CURLOPT_NOPROGRESS, 0L, "CURLOPT_NOPROGRESS");
    }

    template<typename T>
    void setHeaderCallback(size_t (*cb)(char*, size_t, size_t, void*), T buffer) noexcept {
        static_assert(!std::is_fundamental<T>::value, "Buffer of fundamental type is not allowed");
        if (cb) setOption(CURLOPT_HEADERFUNCTION, cb, "CURLOPT_HEADERFUNCTION");
        setOption(CURLOPT_HEADERDATA, static_cast<void*>(buffer), "CURLOPT_HEADERDATA");
    }

    void setHTTPVersion(const HTTP ver) noexcept{
        setOption(CURLOPT_HTTP_VERSION, static_cast<long>(ver) ,"CURLOPT_HTTP_VERSION");
    }

    void setBufferSize(const std::size_t sz) noexcept{
        curl_buffer_sz = sz; 
        setOption(CURLOPT_BUFFERSIZE, sz, "CURLOPT_BUFFERSIZE");
    }

    void addHeader(const std::string& header) noexcept {
        struct curl_slist* new_headers = curl_slist_append(headers_.get(), header.c_str());
        if (!new_headers) {
            std::cerr << "Failed to append header: " << header << '\n';
            return;
        }
        headers_.release(); 
        headers_.reset(new_headers);  
        setOption(CURLOPT_HTTPHEADER, headers_.get(), "CURLOPT_HTTPHEADER");
    }

    void addHeaders(const std::vector<std::string>& headers) noexcept {
        for (const auto& header : headers) addHeader(header);
    }

    void clearHeaders() noexcept {
        headers_.reset(nullptr);
        setOption(CURLOPT_HTTPHEADER, nullptr, "CURLOPT_HTTPHEADER");
    }

    void setAcceptEncoding(const std::string& enc) noexcept{
        setOption(CURLOPT_ACCEPT_ENCODING, enc.c_str(), "CURLOPT_ACCEPT_ENCODING");
    }

    void setTimeoutMs(const long tm) noexcept{
        curl_mstimeout = tm;
        setOption(CURLOPT_TIMEOUT_MS, tm, "CURLOPT_TIMEOUT_MS");
    }

    void setDepth(const std::size_t d) noexcept{ depth = d; }

    std::size_t getDepth() const noexcept{ return depth; } 

    void setGet(const bool val) noexcept{
        setOption(CURLOPT_HTTPGET, val, "CURLOPT_HTTPGET");
	}
	
	void setPost(const bool val) noexcept{
        setOption(CURLOPT_POST, val, "CURLOPT_POST");
	}

    void reset() noexcept{
        curl_easy_reset(curl_handle_.get());
        buf.clear();
        depth = 0;
        initialiseInitialOptions();
    }

    void setMultiplexing(bool val){
        setOption(CURLOPT_PIPEWAIT, val ? 1L : 0L ,"CURLOPT_PIPEWAIT");
    }

    void setUserAgent(const std::string& ua){
        setOption(CURLOPT_USERAGENT, ua.c_str(), "CURLOPT_USERAGENT");
    }

    void setMaxRedirections(const long l){
        setOption(CURLOPT_MAXREDIRS, l, "CURLOPT_MAXREDIRS");
    }

    void setFollowRedirects(bool follow) noexcept {
        setOption(CURLOPT_FOLLOWLOCATION, follow ? 1L : 0L, "CURLOPT_FOLLOWLOCATION");
    }

    void setReferer(const std::string& referer) noexcept {
        setOption(CURLOPT_REFERER, referer.c_str(), "CURLOPT_REFERER");
    }

    void setCookieFile(const std::string& cookieFile) noexcept {
        setOption(CURLOPT_COOKIEFILE, cookieFile.c_str(), "CURLOPT_COOKIEFILE");
    }

    void setCookieJar(const std::string& cookieJar) noexcept {
        setOption(CURLOPT_COOKIEJAR, cookieJar.c_str(), "CURLOPT_COOKIEJAR");
    }

    void setCookies(const std::string& ck){
        setOption(CURLOPT_COOKIE, ck.c_str(), "CURLOPT_COOKIE");
    }

    void setVerbose(bool verbose) noexcept {
        setOption(CURLOPT_VERBOSE, verbose ? 1L : 0L, "CURLOPT_VERBOSE");
    }

    void setBearerToken(const std::string& token) noexcept {
        setOption(CURLOPT_HTTPAUTH, CURLAUTH_BEARER, "CURLOPT_XOAUTH2_BEARER");
        setOption(CURLOPT_XOAUTH2_BEARER, token.c_str(), "CURLOPT_XOAUTH2_BEARER");
    }

    void setPostFields(const std::string& postFields) noexcept {
        setOption(CURLOPT_POSTFIELDSIZE, postFields.length() + 1, "CURLOPT_POSTFIELDS");
        setOption(CURLOPT_COPYPOSTFIELDS, postFields.c_str(), "CURLOPT_POSTFIELDS");
        
    }

    void setInterface(const std::string& inter) noexcept {
        setOption(CURLOPT_INTERFACE, inter.c_str(), "CURLOPT_INTERFACE");
    }

    void setSSLUsage(bool useSSL) noexcept {
        curl_usessl sslValue = useSSL ? CURLUSESSL_CONTROL : CURLUSESSL_NONE;
        setOption(CURLOPT_USE_SSL, sslValue, "CURLOPT_USE_SSL");
    }

    void setVerify(bool verify) noexcept {
        setOption(CURLOPT_SSL_VERIFYPEER, verify ? 1L : 0L, "CURLOPT_SSL_VERIFYPEER");
        setOption(CURLOPT_SSL_VERIFYHOST, verify ? 2L : 0L, "CURLOPT_SSL_VERIFYHOST");
    }

    void setBasicAuth(const std::string& username, const std::string& password) noexcept {
        std::string userpwd = username + ":" + password;
        setOption(CURLOPT_HTTPAUTH, CURLAUTH_BASIC, "CURLAUTH_BASIC");
        setOption(CURLOPT_USERPWD, userpwd.c_str(), "CURLOPT_USERPWD");
    }

    void setDigestAuth(const std::string& username, const std::string& password) noexcept {
        std::string userpwd = username + ":" + password;
        setOption(CURLOPT_HTTPAUTH, CURLAUTH_DIGEST, "CURLAUTH_DIGEST");
        setOption(CURLOPT_USERPWD, userpwd.c_str(), "CURLOPT_USERPWD");
    }

    void setNTLMAuth(const std::string& domain, const std::string& username, const std::string& password) noexcept {
        std::string userpwd = domain + "\\" + username + ":" + password;
        setOption(CURLOPT_HTTPAUTH, CURLAUTH_NTLM, "CURLAUTH_NTLM");
        setOption(CURLOPT_USERPWD, userpwd.c_str(), "CURLOPT_USERPWD");
    }

    void setClientCertificate(const std::string& certPath, const std::string& keyPath) noexcept {
        setOption(CURLOPT_SSLCERT, certPath.c_str(), "CURLOPT_SSLCERT");
        setOption(CURLOPT_SSLKEY, keyPath.c_str(), "CURLOPT_SSLKEY");
    }

    void setProxy(const std::string& proxyUrl, int proxyPort = 0) noexcept {
        setOption(CURLOPT_PROXY, proxyUrl.c_str(), "CURLOPT_PROXY");
        if (proxyPort > 0) {
            setOption(CURLOPT_PROXYPORT, proxyPort, "CURLOPT_PROXYPORT");
        }
        setOption(CURLOPT_HTTPPROXYTUNNEL, 1L, "CURLOPT_HTTPPROXYTUNNEL");
    }

    void setProxyAuth(const std::string& username, const std::string& password) noexcept {
        std::string userpwd = username + ":" + password;
        setOption(CURLOPT_PROXYAUTH, CURLAUTH_ANY, "CURLAUTH_ANY");  
        setOption(CURLOPT_PROXYUSERPWD, userpwd.c_str(), "CURLOPT_PROXYUSERPWD");
    }

    void setLimitMBytesPerSec(double mbytesPerSec) noexcept {
        curl_off_t bytes_per_second = static_cast<curl_off_t>(mbytesPerSec * (1024 * 1024));  
        setOption(CURLOPT_MAX_RECV_SPEED_LARGE, bytes_per_second, "CURLOPT_MAX_RECV_SPEED_LARGE");
    }

    void setUrl(const std::string& url , const std::size_t d = 0) noexcept{
        buf.clear();
        depth = d;
        setOption(CURLOPT_URL, url.c_str(), "CURLOPT_URL");
    }

    const std::string& getBuffer() const noexcept{
        return buf;
    }

    CURL* get() {
        return curl_handle_.get();
    }

    std::unique_ptr<Response> response() noexcept {
        return std::make_unique<Response>(get(),depth,buf);
    }

    CURLcode perform() noexcept {
        CURLcode res = curl_easy_perform(curl_handle_.get());
        if (res != CURLE_OK) {
            std::cerr << "Curl request failed: " << curl_easy_strerror(res) << std::endl;
        }
        return res;
    }

};

class CurlHandlePool {
private:
    std::vector<std::unique_ptr<CurlEasyHandle>> pool;
    std::size_t max_sz;
public:
    CurlHandlePool(std::size_t initial_size , long buf_sz , long timeout) : max_sz(initial_size) {
        pool.reserve(initial_size);
        for (std::size_t i = 0; i < initial_size; ++i) {
            pool.emplace_back(std::make_unique<CurlEasyHandle>(buf_sz , timeout));
        }
    }

    const bool isEmpty() const noexcept {
        return pool.empty();
    }

    const bool isFull() const noexcept{
        return pool.size() == max_sz;
    }

    std::unique_ptr<CurlEasyHandle> acquire() {
        if (isEmpty()) {
            throw std::underflow_error("Pool is empty, cannot acquire handle");
        }

        auto handle = std::move(pool.back());
        pool.pop_back();
        return handle;
    }

    void release(std::unique_ptr<CurlEasyHandle> handle) {
        if (isFull()) {
            throw std::overflow_error("Pool is full, cannot release more handles");
        }
        pool.emplace_back(std::move(handle));
    }

    template<typename T>
    void propogateWriteCallback(const curl_write_callback cb, T buffer) noexcept {
        for (auto& handle : pool) {
            handle->setWriteCallback<T>(cb,buffer);
        }
    }

    template<typename T>
    void propogateReadCallback(const curl_read_callback cb, T buffer) noexcept {
        for (auto& handle : pool) {
            handle->setReadCallback<T>(cb,buffer);
        }
    }

    template<typename T>
    void propogateProgressCallback(const curl_progress_callback cb, T clientData) noexcept {
        for (auto& handle : pool) {
            handle->setProgressCallback<T>(cb,clientData);
        }
    }

    template<typename T>
    void propogateHeaderCallback(size_t (*cb)(char*, size_t, size_t, void*), T buffer) noexcept {
        for (auto& handle : pool) {
            handle->setHeaderCallback<T>(cb,buffer);
        }
    }

    void propogateClearHeaders() noexcept{
        for (auto& handle : pool) {
            handle->clearHeaders();
        }
    }

    void propogateBufferSize(const std::size_t sz) noexcept{
        for (auto& handle : pool) {
            handle->setBufferSize(sz);
        }
    }

    void propogateTimeout(const long tm) noexcept{
        for (auto& handle : pool) {
            handle->setTimeoutMs(tm);
        }
    }

    void propogateAcceptEncoding(const std::string& val) noexcept{
        for (auto& handle : pool) {
            handle->setAcceptEncoding(val);
        }
    }

    void propogateGet(const bool val) noexcept{
        for (auto& handle : pool) {
            handle->setGet(val);
        }
    }

    void propogatePost(const bool val) noexcept{
        for (auto& handle : pool) {
            handle->setPost(val);
        }
    }

    std::unique_ptr<std::vector<CURLcode>> propogatePerform() noexcept{
        std::vector<CURLcode> v;
        v.reserve(pool.size());
        for (auto& handle : pool) {
            v.emplace_back(handle->perform());
        }
        return std::make_unique<std::vector<CURLcode>>(v);
    }

    void propagateHttpVersion(const HTTP ver) {
        for (auto& handle : pool) {
            handle->setHTTPVersion(ver);
        }
    }

    void propagateMultiplexing(bool val) {
        for (auto& handle : pool) {
            handle->setMultiplexing(val);
        }
    }

    void propagateUserAgent(const std::string& ua) {
        for (auto& handle : pool) {
            handle->setUserAgent(ua);
        }
    }

    void propagateMaxRedirections(const long l) {
        for (auto& handle : pool) {
            handle->setMaxRedirections(l);
        }
    }

    void propagateHeader(const std::string& head){
        for (auto& handle : pool) {
            handle->addHeader(head);
        }
    }

    void propagateHeaders(const std::vector<std::string>& head){
        for (auto& handle : pool) {
            handle->addHeaders(head);
        }
    }

    void propagateFollowRedirects(bool follow) {
        for (auto& handle : pool) {
            handle->setFollowRedirects(follow);
        }
    }

    void propagateReferer(const std::string& referer) {
        for (auto& handle : pool) {
            handle->setReferer(referer);
        }
    }

    void propagateProxy(const std::string& proxyUrl, int proxyPort = 0) {
        for (auto& handle : pool) {
            handle->setProxy(proxyUrl, proxyPort);
        }
    }

    void propagateProxyAuth(const std::string& username, const std::string& password) {
        for (auto& handle : pool) {
            handle->setProxyAuth(username, password);
        }
    }

    void propagateLimitMBytesPerSec(double mbytesPerSec) {
        for (auto& handle : pool) {
            handle->setLimitMBytesPerSec(mbytesPerSec);
        }
    }

    void propagateReset() {
        for (auto& handle : pool) {
            handle->reset();
        }
    }

    void propagateCookieFile(const std::string& cookieFile) {
        for (auto& handle : pool) {
            handle->setCookieFile(cookieFile);
        }
    }

    void propagateCookieJar(const std::string& cookieJar) {
        for (auto& handle : pool) {
            handle->setCookieJar(cookieJar);
        }
    }

    void propagateCookies(const std::string& ck) {
        for (auto& handle : pool) {
            handle->setCookies(ck);
        }
    }

    void propagateVerbose(bool verbose) {
        for (auto& handle : pool) {
            handle->setVerbose(verbose);
        }
    }

    void propagateBearerToken(const std::string& token) {
        for (auto& handle : pool) {
            handle->setBearerToken(token);
        }
    }

    void propagatePostFields(const std::string& postFields) {
        for (auto& handle : pool) {
            handle->setPostFields(postFields);
        }
    }

    void propagateInterface(const std::string& inter) {
        for (auto& handle : pool) {
            handle->setInterface(inter);
        }
    }

    void propagateSSLUsage(bool useSSL) {
        for (auto& handle : pool) {
            handle->setSSLUsage(useSSL);
        }
    }

    void propagateVerify(bool verify) {
        for (auto& handle : pool) {
            handle->setVerify(verify);
        }
    }

    void propagateBasicAuth(const std::string& username, const std::string& password) {
        for (auto& handle : pool) {
            handle->setBasicAuth(username, password);
        }
    }

    void propagateDigestAuth(const std::string& username, const std::string& password) {
        for (auto& handle : pool) {
            handle->setDigestAuth(username, password);
        }
    }

    void propagateNTLMAuth(const std::string& domain, const std::string& username, const std::string& password) {
        for (auto& handle : pool) {
            handle->setNTLMAuth(domain, username, password);
        }
    }

    void propagateClientCertificate(const std::string& certPath, const std::string& keyPath) {
        for (auto& handle : pool) {
            handle->setClientCertificate(certPath, keyPath);
        }
    }
};

class CurlMultiWrapper {
private:
    std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)> multi_handle;
    int pending {0};

public:
    CurlMultiWrapper(const long tc = 10  ,const long hc = 10)
        : multi_handle(curl_multi_init(), &curl_multi_cleanup){

        if (!multi_handle) {
            throw std::runtime_error("Failed to initialize cURL multi handle");
        }
        
        setNumConnections(tc,hc);
        setMultiplex(true);
    }

    template<typename T>
    void setOption(CURLMoption option, T value) {
        const CURLMcode res = curl_multi_setopt(multi_handle.get(), option, value);
        if (res != CURLM_OK) {
            throw std::runtime_error("Failed to set cURL multi option");
        }
    }

    void setMultiplex(const bool val) noexcept{
        setOption(CURLMOPT_PIPELINING, val ? CURLPIPE_MULTIPLEX : CURLPIPE_NOTHING);
    }

    void setNumConnections(const long tc , const long hc = 10) noexcept{
        setOption(CURLMOPT_MAX_TOTAL_CONNECTIONS, tc);
        setOption(CURLMOPT_MAX_HOST_CONNECTIONS, hc);
    }

    template<typename T>
    void addSocketCallbackData(curl_socket_callback clb, T data) noexcept {
        setOption(CURLMOPT_SOCKETDATA, static_cast<void*>(data));
        setOption(CURLMOPT_SOCKETFUNCTION, clb);
    }

    inline int getPending() const noexcept{
        return pending;
    } 

    template<typename T>
    void addTimeoutCallbackData(curl_multi_timer_callback clb , T data) noexcept{
        setOption(CURLMOPT_TIMERDATA, static_cast<void*>(data));
        setOption(CURLMOPT_TIMERFUNCTION, clb);
    }

    void addHandle(CURL* easy_handle) {
        if(!easy_handle) throw std::runtime_error("Adding empty handle in curlmulti");
        const CURLMcode res = curl_multi_add_handle(multi_handle.get(), easy_handle);
        if (res != CURLM_OK) {
            throw std::runtime_error("Failed to add cURL easy handle to multi handle");
        }
        
    }

    void socketAction(const curl_socket_t sockfd, const int ev_bitmask ) {
        const CURLMcode res = curl_multi_socket_action(multi_handle.get(), sockfd, ev_bitmask, &this->pending);
        if (res != CURLM_OK) {
            throw std::runtime_error("cURL multi socket action error");
        }
    }

    CURLMsg* readMulti(int &pending) noexcept{
        return curl_multi_info_read(multi_handle.get() , &pending);
    }

    void removeHandle(CURL* easy_handle)  {
        if(!easy_handle) throw std::runtime_error("Removing empty handle in curlmulti");
        const CURLMcode res = curl_multi_remove_handle(multi_handle.get(), easy_handle);
        if (res != CURLM_OK) {
            throw std::runtime_error("Failed to add cURL easy handle to multi handle");
        }
    }

    void assign(const curl_socket_t sockfd, void *sockp){
        const CURLMcode res = curl_multi_assign(multi_handle.get(), sockfd, sockp);
        if (res != CURLM_OK) {
            throw std::runtime_error("Failed to add cURL easy handle to multi handle");
        }
    }

    ~CurlMultiWrapper() = default;
    CurlMultiWrapper(const CurlMultiWrapper&) = delete;
    CurlMultiWrapper& operator=(const CurlMultiWrapper&) = delete;
};

class URLRequestManager {
    std::deque<std::pair<std::string, size_t>> url_queue;
    std::unordered_set<std::string> visited_urls;

public:
    explicit URLRequestManager() = default;
    URLRequestManager(const URLRequestManager&) = delete;
    URLRequestManager& operator=(const URLRequestManager&) = delete;

    const bool addURL(const std::string& url, const size_t depth = 0) {
        if (visited_urls.find(url) == visited_urls.end()) {
            visited_urls.insert(url); 
            url_queue.emplace_front(url, depth);
            return true; 
        }
        return false; 
    }

    inline const std::unordered_set<std::string>& getVisited() const noexcept{
        return visited_urls;
    }

    const std::pair<std::string, size_t> popURL() noexcept{ 
        const auto url = std::move(url_queue.back()); 
        url_queue.pop_back(); 
        return url; 
    }
    void clear() noexcept{
        url_queue.clear();
    }

    const std::size_t getPendingUrlQueueSize() const noexcept{
        return url_queue.size();
    }

    const std::size_t getVisitedUrlSize() const noexcept{
        return visited_urls.size();
    }

    const bool hasURLs() const noexcept { return !url_queue.empty(); }

};

class Node;
class Document;
class NodeList;
class Parser;

class DomCollection {
public:
    DomCollection(lxb_html_document_t* document) 
        : collection_(lxb_dom_collection_make(&document->dom_document, 128), &DomCollection::deleter) {
        if (!collection_) {
            throw std::runtime_error("Failed to create DOM collection");
        }
    }
    
    lxb_dom_collection_t* get() const {
        return collection_.get();
    }

    void clean() const {
        lxb_dom_collection_clean(collection_.get());
    }

private:
    static void deleter(lxb_dom_collection_t* collection) {
        lxb_dom_collection_destroy(collection, true);
    }

    std::unique_ptr<lxb_dom_collection_t, decltype(&deleter)> collection_;
};

class NodeList {
    std::vector<lxb_dom_node_t*> nodePtr;
public:
    NodeList(DomCollection& collection, lxb_html_document_t* document)
        : collection_(collection), document_(document) {
            std::size_t len = length();
            nodePtr.reserve(len); 
            lxb_dom_node_t* node { nullptr };
            for(std::size_t i = 0; i < len; ++i){
                node = lxb_dom_collection_node(collection_.get(), i);
                if(!lxb_dom_node_is_empty(node)) nodePtr.emplace_back(node);
            }
        }

    const std::size_t length() const {
        return lxb_dom_collection_length(collection_.get());
    }

    std::unique_ptr<Node> item(const std::size_t index) const {
        if(index >= nodePtr.size()) return nullptr; 
        return std::make_unique<Node>(nodePtr.at(index), document_, collection_);
    }

private:
    DomCollection& collection_;
    lxb_html_document_t* document_;
};

class Node {

    bool isValidElement(lxb_dom_node_t* node) const {
        return node_->type == LXB_DOM_NODE_TYPE_ELEMENT && !lxb_dom_node_is_empty(node_);
    }

public:
    Node(lxb_dom_node_t* node, lxb_html_document_t* document , DomCollection&  col)
        : node_(node), document_(document),collection_(col) {}


    std::unique_ptr<Node> firstChild() const {
        if(!isValidElement(node_)) return nullptr;
        lxb_dom_node_t* childNode = lxb_dom_node_first_child(node_);
        while(childNode!=nullptr){
            if(!lxb_dom_node_is_empty(childNode)) break;
            childNode = lxb_dom_node_next(childNode);
        }
        if(childNode == nullptr) return nullptr;
        return std::make_unique<Node>(childNode,document_,collection_);
    }

    std::unique_ptr<Node> lastChild() const {
        if(!isValidElement(node_)) return nullptr;
        lxb_dom_node_t* childNode = lxb_dom_node_last_child(node_);
        while(childNode!=nullptr){
            if(!lxb_dom_node_is_empty(childNode)) break;
            childNode = lxb_dom_node_prev(childNode);
        }
        if(childNode == nullptr) return nullptr;
        return std::make_unique<Node>(childNode,document_,collection_);
    }

    std::unique_ptr<Node> nextSibling() const {
        if(!isValidElement(node_)) return nullptr;
        lxb_dom_node_t* siblingNode = lxb_dom_node_next(node_);
        while(siblingNode!=nullptr){
            if(!lxb_dom_node_is_empty(siblingNode)) break;
            siblingNode = lxb_dom_node_next(siblingNode);
        }
        if(siblingNode== nullptr) return nullptr;
        return std::make_unique<Node>(siblingNode,document_,collection_);
    }

    std::unique_ptr<Node> prevSibling() const {
        if(!isValidElement(node_)) return nullptr;
        lxb_dom_node_t* siblingNode = lxb_dom_node_prev(node_);
        while(siblingNode!=nullptr){
            if(!lxb_dom_node_is_empty(siblingNode)) break;
            siblingNode = lxb_dom_node_prev(siblingNode);
        }
        if(siblingNode== nullptr) return nullptr;
        return std::make_unique<Node>(siblingNode,document_,collection_);
    }

    const std::string getAttribute(const std::string& name) const {
        if(!isValidElement(node_)) return {};
        auto* element = lxb_dom_interface_element(node_);
        if (!element) {
            throw std::runtime_error("Node is not an element");
        }
        auto* attr = lxb_dom_element_attr_by_name(element, reinterpret_cast<const lxb_char_t*>(name.c_str()), name.length());
        if (!attr) {
            return "";  // Attribute not found
        }
        std::size_t len;
        const char* str = reinterpret_cast<const char*>(lxb_dom_attr_value(attr, &len));
        return std::string(str,len);
    }

    std::unique_ptr<NodeList> getChildElements() const {
        collection_.clean();
        return getElementsByTagName("*");
    }

    const bool hasAttributes() const {
        if(!isValidElement(node_)) return false;
        return lxb_dom_element_has_attributes(lxb_dom_interface_element(node_));
    }

    const bool hasAttribute(const std::string& attr) const{
        if(!isValidElement(node_)) return {};
        auto* element = lxb_dom_interface_element(node_);
        const lxb_char_t* name = reinterpret_cast<const lxb_char_t*>(attr.c_str());
        return lxb_dom_element_has_attribute(element ,name , attr.length());
    }

    std::unique_ptr<std::unordered_map<std::string, std::string>> getAttributes() const {
        auto mp = std::make_unique<std::unordered_map<std::string, std::string>>();
        if(!isValidElement(node_)) return mp;
        auto* element = lxb_dom_interface_element(node_);
        lxb_dom_attr_t* attr = lxb_dom_element_first_attribute(element);
        std::size_t len = 0;
        const lxb_char_t *temp = nullptr;
        
        while (attr != nullptr) {
            std::string key , value;
            temp = lxb_dom_attr_qualified_name(attr, &len);
            if (temp != nullptr) {
                key = std::string(reinterpret_cast<const char*>(temp), len); 
            }
            temp = lxb_dom_attr_value(attr, &len);
            if (temp != nullptr) {
                value = std::string(reinterpret_cast<const char*>(temp), len); 
            }
            mp->emplace(key, value);
            attr = lxb_dom_element_next_attribute(attr);
        }
        return mp;
    }

    const std::string text() const{
        std::string str;
        lxb_char_t * s {nullptr};
        std::size_t len = 0;
        if(node_->type == LXB_DOM_NODE_TYPE_TEXT) s = lxb_dom_node_text_content(node_,&len);
        return s ? std::string(reinterpret_cast<const char*>(s),len) : "";
    }

    std::unique_ptr<NodeList> getElementsByClassName(const std::string& className) const {
        collection_.clean();
        if(!isValidElement(node_)) return nullptr;
        auto* element = lxb_dom_interface_element(node_);
        const lxb_char_t * name = reinterpret_cast<const lxb_char_t *>(className.c_str());
        lxb_status_t status = lxb_dom_elements_by_class_name(element, collection_.get(), name, className.length());
        if (status != LXB_STATUS_OK) {
            throw std::runtime_error("Failed to collect child elements by tag name");
        }
        return std::make_unique<NodeList>(collection_, document_);
    }

    std::unique_ptr<NodeList> getElementsByTagName(const std::string& className) const {
        collection_.clean();
        if(!isValidElement(node_)) return nullptr;
        auto* element = lxb_dom_interface_element(node_);
        const lxb_char_t * name = reinterpret_cast<const lxb_char_t *>(className.c_str());
        lxb_status_t status = lxb_dom_elements_by_tag_name(element, collection_.get(), name, className.length());
        if (status != LXB_STATUS_OK) {
            throw std::runtime_error("Failed to collect child elements by tag name");
        }
        return std::make_unique<NodeList>(collection_, document_);
    }

    std::unique_ptr<NodeList> getElementsByAttribute(const std::string& key, const std::string& value) const {
        collection_.clean();
        if(!isValidElement(node_)) return nullptr;
        auto* element = lxb_dom_interface_element(node_);
        const lxb_char_t * name = reinterpret_cast<const lxb_char_t *>(key.c_str());
        const lxb_char_t * val = reinterpret_cast<const lxb_char_t *>(value.c_str());
        lxb_status_t status = lxb_dom_elements_by_attr(element, collection_.get(), name, key.length(), val, value.length(), true);
        if (status != LXB_STATUS_OK) {
            throw std::runtime_error("Failed to collect child elements by tag name");
        }
        return std::make_unique<NodeList>(collection_, document_);
    }

    std::unique_ptr<std::vector<std::string>> getLinksMatching(const std::string& pattern = "") const {
        std::regex regexPattern(pattern);
        std::vector<std::string> matchingLinks;
        
        auto linksNodeList = this->getElementsByTagName("a");
        if (linksNodeList) {
            for (std::size_t i = 0; i < linksNodeList->length(); ++i) {
                auto linkNode = linksNodeList->item(i);
                if (linkNode) {
                    std::string url = linkNode->getAttribute("href");
                    if (pattern.empty() || std::regex_match(url, regexPattern)) {
                        matchingLinks.push_back(url);
                    }
                }
            }
        }
        return std::make_unique<std::vector<std::string>>(matchingLinks);
    }

private:
    lxb_dom_node_t* node_;
    lxb_html_document_t* document_;
    DomCollection& collection_;
};

class Document {
public:

    Document(std::unique_ptr<lxb_html_document_t, decltype(&lxb_html_document_destroy)>&& doc)
        : doc_(std::move(doc)), collection_(doc_.get()) {}

    std::unique_ptr<Node> rootElement() {
        lxb_html_body_element_t* bodyElement = lxb_html_document_body_element(doc_.get());
        if (!bodyElement) {
            throw std::runtime_error("Failed to obtain body element");
        }
        return std::make_unique<Node>(lxb_dom_interface_node(bodyElement), doc_.get(), collection_);
    }

private:
    static inline void deleter(lxb_dom_collection_t* collection){
        lxb_dom_collection_destroy(collection,true);
    }
    std::unique_ptr<lxb_html_document_t, decltype(&lxb_html_document_destroy)> doc_;
    DomCollection collection_;
};

class Parser {
    std::unique_ptr<lxb_html_parser_t, decltype(&lxb_html_parser_destroy)>
        parser {lxb_html_parser_create(), &lxb_html_parser_destroy};

    static void check(const lxb_status_t status, const char* errMsg) {
        if (status != LXB_STATUS_OK) {
            throw std::runtime_error(errMsg);
        }
    }

public:
    Parser() {
        lxb_status_t status = lxb_html_parser_init(parser.get());
        check(status, "Failed to initialize HTML parser");
    }

    Document createDOM(const std::string& msg) {
        auto* document_ptr = lxb_html_parse(parser.get(),
                                            reinterpret_cast<const lxb_char_t *>(msg.c_str()),
                                            msg.length());
        if (!document_ptr) {
            throw std::runtime_error("Failed to parse HTML");
        }
        return Document(std::unique_ptr<lxb_html_document_t, decltype(&lxb_html_document_destroy)>(document_ptr, &lxb_html_document_destroy));
    }
};

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


int main(){

    constexpr int concurrent_connections = 200 , max_host_connections = 10 ;
    std::unique_ptr<Async> scraper = std::make_unique<Async>(concurrent_connections , max_host_connections);
    scraper->setUserAgent("Scraper/ 1.1");
    scraper->setMultiplexing(true);
    scraper->setHttpVersion(HTTP::HTTP2);
    //scraper->setVerbose(true);
    //scraper->setProxy("188.87.102.128",3128);
    scraper->seed("https://www.google.com/");

    scraper->onSuccess([](const CurlEasyHandle::Response& response, Async& instance , Document& page){
        std::cout << "URL: " << response.url << '\n';
        std::cout << "Received: " << response.bytesRecieved << " bytes\n";
        std::cout << "Content Type: " << response.contentType << '\n';
        std::cout << "Total Time: " << response.totalTime << '\n';
        std::cout << "HTTP Version: " << response.httpVersion << '\n';
        std::cout << "HTTP Method: " << response.httpMethod << '\n';
        std::cout << "Download speed: " << response.bytesPerSecondR << " bytes/sec\n";
        std::cout << "Header Size: " << response.headerSize << " bytes\n";

        auto body = page.rootElement();
        auto div = body->getElementsByTagName("div")->item(0);
        auto links = div->getLinksMatching("");

        for(auto i: *links.get()) std::cout<<i<<'\n';
    });

    scraper->onIdle([](long pending , Async& instance ){

    });

    scraper->onException([](const std::exception& e , Async& instance) {
        std::cerr << "Exception encountered: " << e.what() << std::endl;    
    });

    scraper->onFailure([](const CurlEasyHandle::Response& response , Async& instance){

    });

    scraper->run();
}
