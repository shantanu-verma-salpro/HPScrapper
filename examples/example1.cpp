#pragma GCC optimize("Ofast")
#pragma GCC target("avx,avx2,fma")

#include <iostream>
#include "../src/HBscraper.hpp"


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
