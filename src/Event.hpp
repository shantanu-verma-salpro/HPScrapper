#ifndef EVENTS
#define EVENTS

class Event {
public:
    virtual ~Event() noexcept = default;
};

class IdleEvent final : public Event {};
class TimerEvent final : public Event {};
class CheckEvent final : public Event {};
class PrepareEvent final : public Event {};
class PollEvent final : public Event {
public:
    int status{};
    int events{};
};

#endif