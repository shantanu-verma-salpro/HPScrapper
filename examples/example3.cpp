#include <iostream>

#include "../include/async/EventLoop.hpp"
#include "../include/async/TimerWrapper.hpp"
#include "../include/async/CheckWrapper.hpp"
#include "../include/async/IdleWrapper.hpp"
#include "../include/async/PrepareWrapper.hpp"
#include "../include/async/PollWrapper.hpp"

int event_loop_example() {
   
    EventLoop eventLoop;

    TimerWrapper timer(eventLoop);
    timer.on<TimerEvent, TimerWrapper>([](const TimerEvent&, TimerWrapper&) {
        std::cout << "Timer triggered after 2 seconds!" << std::endl;
    });
    timer.start(2000, 0);  // Timeout of 2 seconds, no repeat

   
    IdleWrapper idle(eventLoop);
    idle.on<IdleEvent, IdleWrapper>([](const IdleEvent&, IdleWrapper&) {
        std::cout << "Idle handler running..." << std::endl;
    });
    idle.start();

   
    TimerWrapper stopTimer(eventLoop);
    stopTimer.on<TimerEvent, TimerWrapper>([&eventLoop](const TimerEvent&, TimerWrapper&) {
        std::cout << "Stopping event loop after 5 seconds..." << std::endl;
        eventLoop.stop();
    });
    stopTimer.start(5000, 0);  


    eventLoop.run();

    return 0;
}
