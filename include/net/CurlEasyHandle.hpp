#ifndef CURLEH
#define CURLEH

#include <curl/curl.h>
#include <iostream>
#include <memory>
#include <vector>
#include <functional>

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
        setOption(CURLOPT_XFERINFOFUNCTION, cb, "CURLOPT_XFERINFOFUNCTION");
        setOption(CURLOPT_XFERINFODATA, static_cast<void*>(clientData), "CURLOPT_XFERINFODATA");
        setOption(CURLOPT_NOPROGRESS, 0L, "CURLOPT_NOPROGRESS");
    }

    template<typename T>
    void setHeaderCallback(size_t (*cb)(char*, size_t, size_t, void*), T buffer) noexcept {
        static_assert(!std::is_fundamental<T>::value, "Buffer of fundamental type is not allowed");
        setOption(CURLOPT_HEADERFUNCTION, cb, "CURLOPT_HEADERFUNCTION");
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
        return res;
    }

    void fetch(std::function<void (CurlEasyHandle::Response*)> fn) noexcept{
        CURLcode res = perform();
        if(res != CURLE_OK) {
            std::cerr << "Request failed: " << curl_easy_strerror(res) << '\n';
            fn(nullptr);
            return;
        }
        auto resp = response();
        fn(resp.get());
    }
};

#endif