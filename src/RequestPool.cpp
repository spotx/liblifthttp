#include "lift/RequestPool.h"

namespace lift {

auto RequestPool::Reserve(
    size_t count) -> void
{
    std::lock_guard<std::mutex> guard { m_lock };
    for (size_t i = 0; i < count; ++i) {
        // All these fields will get reset on Produce().
        auto request_handle_ptr = std::unique_ptr<Request>(
            new Request(
                *this,
                "",
                std::chrono::milliseconds { 0 },
                std::nullopt,
                [](RequestHandle r) { (void)r; }));
        m_requests.emplace_back(std::move(request_handle_ptr));
    }
}

auto RequestPool::Produce(
    std::string_view url) -> RequestHandle
{
    using namespace std::chrono_literals;
    return Produce(url, nullptr, 0ms);
}

auto RequestPool::Produce(
    std::string_view url,
    std::chrono::milliseconds connection_timeout) -> RequestHandle
{
    return Produce(url, nullptr, connection_timeout);
}

auto RequestPool::Produce(
    std::string_view url,
    std::function<void(RequestHandle)> on_complete_handler,
    std::chrono::milliseconds connection_timeout) -> RequestHandle
{
    return Produce(url, std::move(on_complete_handler), connection_timeout, std::nullopt);
}

auto RequestPool::Produce(
    std::string_view url,
    std::chrono::milliseconds connection_timeout,
    std::chrono::milliseconds response_wait_time) -> RequestHandle
{
    return Produce(url, nullptr, connection_timeout, response_wait_time);
}


auto RequestPool::Produce(
    std::string_view url,
    std::function<void(RequestHandle)> on_complete_handler,
    std::chrono::milliseconds connection_timeout,
    std::optional<std::chrono::milliseconds> response_wait_time) -> RequestHandle
{

    m_lock.lock();
    
    if (m_requests.empty()) {
        m_lock.unlock();
        
        // Cannot use std::make_unique here since Request ctor is private friend.
        auto request_handle_ptr = std::unique_ptr<Request> {
            new Request {
                *this,
                url,
                connection_timeout,
                response_wait_time,
                std::move(on_complete_handler) }
        };
        
        return RequestHandle { this, std::move(request_handle_ptr) };
    } else {
        auto request_handle_ptr = std::move(m_requests.back());
        m_requests.pop_back();
        m_lock.unlock();
        
        request_handle_ptr->SetOnCompleteHandler(std::move(on_complete_handler));
        request_handle_ptr->SetUrl(url);
        request_handle_ptr->SetConnectionTimeout(connection_timeout);
        
        return RequestHandle { this, std::move(request_handle_ptr) };
    }
}

auto RequestPool::returnRequest(
    std::unique_ptr<Request> request) -> void
{
    request->Reset(); // Reset the request if it is returned to the pool.
    {
        std::lock_guard<std::mutex> guard { m_lock };
        m_requests.emplace_back(std::move(request));
    }
}

} // lift
