#include "../include/net/CurlEasyHandle.hpp"


int fetcher_example() {

    int buffer_size = 1024;
    long timeout = 5000;
    CurlEasyHandle curlHandle(buffer_size, timeout); 

    curlHandle.setUrl("www.google.com");
    curlHandle.fetch([](CurlEasyHandle::Response* response){
        std::cout << response->message;
    });
    return 0;
}
