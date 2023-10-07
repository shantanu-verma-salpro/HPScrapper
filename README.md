# HBScraper

The HBScraper offers an asynchronous, event-driven mechanism to fetch and process web content. This utility is designed for efficiency, capable of handling multiple requests concurrently.

[![Build](https://github.com/shantanu-verma-salpro/HPScrapper/actions/workflows/build.yml/badge.svg)](https://github.com/shantanu-verma-salpro/HPScrapper/actions/workflows/build.yml)

## Features

 - [x] Wrapper over lexbor for HTML DOM JS like API
 - [x] Asynchronous
 - [x] Supports HTTP/1 , HTTP/1.1 and HTTP/2
 - [x] Supports Proxies
 - [x] Supports Authentication
 - [x] Supports Post Requests
 - [x] Preallocated Connection Pool
 - [x] Simple API
 - [x] Cross platform
 - [x] Uses epoll, kqueue, Windows IOCP, Solaris event ports and Linux io_uring via libuv
 - [x] Wrappers for curl , libuv , lexbor to be used independently
 - [x] Uses modern C++  for memory management and user friendly API

## Table of Contents

-   [Prerequisites](#prerequisites)
-   [Getting Started](#getting-started)
-   [Advanced Options](#advanced-options)
-   [Contributing](#contributing)
-   [License](#license)

## Prerequisites

Before you begin, ensure you have the following libraries:

-   libcurl
-   libuv
-   liblexbor

## Getting Started

### 1. Compilation:

bashCopy code

`$ g++ your_source_file.cpp -o scraper -lcurl -luv -llexbor -Ofast` 

### 2. Initialization

Create an `Async` instance:


    constexpr int concurrent_connections = 200, max_host_connections = 10;
    std::unique_ptr<Async> scraper = std::make_unique<Async>(concurrent_connections, max_host_connections);

 

### 3. Setting Options

Various options can be adjusted as per the requirement:

    scraper->setUserAgent("Scraper/ 1.1");
    scraper->setMultiplexing(true);
    scraper->setHttpVersion(HTTP::HTTP2);` 

Optional settings:


    scraper->setVerbose(true);
    scraper->setProxy("188.87.102.128", 3128);

 

### 4. Seed URL

Kick off the scraping:

`scraper->seed("https://www.google.com/");` 

### 5. Event Handling

Set up your custom event handlers:


    scraper->onSuccess([](const CurlEasyHandle::Response& response, Async& instance, Document& page) {
        // Process...
    });



### 6. Execution


`scraper->run();` 

## Advanced Options

Our scraper provides extensive customization:

-   `setMultiplexing(bool)`: Toggle HTTP/2 multiplexing.
-   `setHttpVersion(HTTP)`: Choose your desired HTTP version.
-   And many more... See the documentation for further details.

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

## License

[MIT](https://choosealicense.com/licenses/mit/)
