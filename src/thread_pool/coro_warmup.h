#pragma once

#include <coroutine>
#include <exception>
#include <iostream>
#include <thread>
#include "thread_pool_fast.h"


// Define a coroutine return object
struct Task {
    struct promise_type {
        Task get_return_object() {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}

        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}

    ~Task() {
        if (handle)
            handle.destroy();
    }

    void resume() {
        if (handle && !handle.done()) {
            handle.resume();
        }
    }

    bool done() const { return !handle || handle.done(); }
};

// Awaitable to run on the thread pool
struct ScheduleOn {
    ThreadPoolFast* pool;

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        pool->submit([h]() mutable { h.resume(); });
    }

    void await_resume() {}
};
