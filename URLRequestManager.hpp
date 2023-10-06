#ifndef URLRM
#define URLRM

#include <deque>
#include <unordered_set>
#include <iostream>

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

#endif