#ifndef CURLHP
#define CURLHP

#include <vector>
#include "CurlEasyHandle.hpp"


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

#endif