#ifndef EVENTD
#define EVENTD

#include <functional>
#include <unordered_map>
#include "Event.hpp"

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

#endif