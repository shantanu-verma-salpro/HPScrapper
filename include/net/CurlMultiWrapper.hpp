#ifndef CURLMW
#define CURLMW

#include <memory>
#include <curl/curl.h>
#include <stdexcept>

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

#endif