
# HBScraper

HBScraper is a high-performance, event-driven utility designed to asynchronously fetch and process web content. Equipped to manage multiple requests concurrently, it stands out with its efficiency and robustness.

[![Build](https://github.com/shantanu-verma-salpro/HPScrapper/actions/workflows/build.yml/badge.svg)](https://github.com/shantanu-verma-salpro/HPScrapper/actions/workflows/build.yml)

## 🌟 Features

-   **Flexible DOM Handling**: Offers a wrapper over `lexbor`, providing a JS-like API for HTML DOM.
-   **Highly Asynchronous**: Designed for non-blocking operations.
-   **Protocol Support**: Works with HTTP/1, HTTP/1.1, and HTTP/2.
-   **Advanced Network Features**: Supports proxies, authentication, and post requests.
-   **Optimized**: Benefits from a preallocated connection pool.
-   **User-Friendly**: Clean and straightforward API.
-   **Platform Compatibility**: Cross-platform support.
-   **Highly Efficient**: Uses epoll, kqueue, Windows IOCP, Solaris event ports, and Linux io_uring via `libuv`.
-   **Extensible**: Provides wrappers for `curl`, `libuv`, and `lexbor` for independent use.
-   **Modern C++**: Harnesses modern C++ capabilities for optimized memory management and a more intuitive API.


## 🚀 Remaining Tasks

Here's our prioritized roadmap:

1.  ⚙️ **Error Management**: Better error insights and debugging.
2.  🌐 **Multi-Processing**: Boost performance through parallel processing.
3.  🌍 **Headless Chrome**: Access JS-rendered pages efficiently.
4.  📚 **Expand Documentation**: Cover all features and use-cases.
5.  🧪 **CI/CD Improvements**: Streamline updates.
6.  🏁 **Performance Benchmarks**: Compare against competitors.

## 📚 Table of Contents

-   [Prerequisites](#prerequisites)
-   [Getting Started](#getting-started)
-   [Advanced Options](#advanced-options)
-   [Contributing](#contributing)
-   [License](#license)

## 🛠 Prerequisites

Before diving in, ensure you have installed the following libraries:

-   `libcurl`
-   `libuv`
-   `liblexbor`

## 🚀 Getting Started

### 1. **Compilation**:

` $ g++ your_source_file.cpp -o scraper -lcurl -luv -llexbor -Ofast ` 

### 2. **Initialization**:

Initialize the `Async` instance:

```cpp
constexpr int concurrent_connections = 200, max_host_connections = 10;
std::unique_ptr<Async> scraper = std::make_unique<Async>(concurrent_connections, max_host_connections);
``` 

### 3. **Configuration**:

Customize as needed:

```cpp
scraper->setUserAgent("Scraper/ 1.1");
scraper->setMultiplexing(true);
scraper->setHttpVersion(HTTP::HTTP2);
``` 

For additional settings:

 ```cpp
scraper->setVerbose(true);
scraper->setProxy("188.87.102.128", 3128);`
```

### 4. **Seed URL**:

Start your scraping journey:

```cpp
scraper->seed("https://www.google.com/");
```

### 5. **Event Management**:

Incorporate custom event handlers:

```cpp
scraper->onSuccess([](const CurlEasyHandle::Response& response, Async& instance, Document& page) {
    // Process the response...
});
``` 

### 6. **Execution**:

Get the scraper running:

```cpp
scraper->run();
```

## ⚙️ Advanced Options

HBScraper offers a myriad of options to fine-tune your scraping experience:

-   `setMultiplexing(bool)`: Enable or disable HTTP/2 multiplexing.
-   `setHttpVersion(HTTP)`: Opt for your preferred HTTP version.
-   More options available in our detailed documentation.

## 📌 Example Usage

### Using Scraper 

```cpp
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
```

### Using Parser

```cpp
int main() {
    std::string htmlContent = R"(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Test Page</title>
        </head>
        <body>
            <div class="col-md">
                <div>Text 1 inside col-md</div>
                <a href="http://example.com">Example Link</a>
                <div data-custom="value">Text 2 inside col-md</div>
            </div>
            <div class="col-md">
                <div>Text 3 inside col-md</div>
            </div>
        </body>
        </html>
    )";

    Parser parser;
    Document doc = parser.createDOM(htmlContent);

    auto root = doc.rootElement();
    auto colMdElements = root->getElementsByClassName("col-md");

    for (std::size_t i = 0; i < colMdElements->length(); ++i) {
        auto colMdNode = colMdElements->item(i);
        auto divElements = colMdNode->getElementsByTagName("div");

        for (std::size_t j = 0; j < divElements->length(); ++j) {
            auto divNode = divElements->item(j);
            std::cout << divNode->text() << '\n';

            if(divNode->hasAttributes()) {
                auto attributes = divNode->getAttributes();
                for(const auto& [attr, value] : *attributes) {
                    std::cout << "Attribute: " << attr << ", Value: " << value << std::endl;
                }
            }

            if(divNode->hasAttribute("data-custom")) {
                std::cout << "Data-custom attribute: " << divNode->getAttribute("data-custom") << '\n';
            }
        }

        if (colMdNode->hasChildElements()) {
            auto firstChild = colMdNode->firstChild();
            auto lastChild = colMdNode->lastChild();

            std::cout << "First child's text content: " << firstChild->text() << '\n';
            std::cout << "Last child's text content: " << lastChild->text() << '\n';
        }

        auto links = colMdNode->getLinksMatching("http://example.com");
        for(const auto& link : *links) {
            std::cout << "Matching Link: " << link << '\n';
        }
    }

    return 0;
}
```

## 🤝 Contributing

We appreciate contributions! If you're considering significant modifications, kindly initiate a discussion by opening an issue first.

## 📄 License

HBScraper is licensed under the [MIT](https://choosealicense.com/licenses/mit/)

Acknowledgements
This software uses the following libraries:

`libcurl`  : Licensed under the MIT License.

`libuv`    : Licensed under the MIT License.

`liblexbor`: Licensed under the Apache License, Version 2.0.

When using `HBScraper`, please ensure you comply with the requirements and conditions of all included licenses.
