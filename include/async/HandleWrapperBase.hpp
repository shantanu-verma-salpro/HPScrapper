#ifndef HANDLEW
#define HANDLEW

#include "EventDispatcher.hpp"
#include <uv.h>
#include <stdexcept>

class HandleWrapperBase {

protected:
    static inline void check_uv_error(const int ret) { if (ret != 0) throw std::runtime_error(uv_strerror(ret)); }

public:

    virtual ~HandleWrapperBase() noexcept {
        EventDispatcher::getInstance().remove(this);
    }

    virtual const bool isActive() const noexcept = 0;
    virtual const bool isClosing() const noexcept = 0;

    template<typename EventType, typename DerivedType, typename Callback>
    void on(Callback&& callback) noexcept {
        auto adapter = [callback = std::forward<Callback>(callback)] (const Event& event, HandleWrapperBase& handle) noexcept {
            callback(static_cast<const EventType&>(event), static_cast<DerivedType&>(handle));
        };
        EventDispatcher::getInstance().on(this, adapter);
    }
};

#endif