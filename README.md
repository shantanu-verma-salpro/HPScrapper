
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



`$ g++ your_source_file.cpp -o scraper -lcurl -luv -llexbor -Ofast` 

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
`scraper->seed("https://www.google.com/");
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

## 🤝 Contributing

We appreciate contributions! If you're considering significant modifications, kindly initiate a discussion by opening an issue first.

## 📄 License

This project is licensed under the [MIT License](https://choosealicense.com/licenses/mit/).
