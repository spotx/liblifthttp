#pragma once

#include "lift/Request.h"
#include "lift/RequestHandle.h"

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

namespace lift {

class CurlPool;

class RequestPool {
    friend class SharedRequest;

public:
    RequestPool() = default;
    ~RequestPool() = default;

    RequestPool(const RequestPool&) = delete;
    RequestPool(RequestPool&&) = delete;
    auto operator=(const RequestPool&) -> RequestPool& = delete;
    auto operator=(RequestPool &&) -> RequestPool& = delete;

    /**
     * This can be useful to pre-allocate a large number of Request objects to be used
     * immediately and incur expensives start up (like CURL* handles) before starting requests.
     * @param count Pre-allocates/reserves this many request objects.
     */
    auto Reserve(
        size_t count) -> void;

    /**
     * Produces a new Request with infinite timeout.
     *
     * This produce method is best used for synchronous requests.
     *
     * @param url The url of the Request.
     * @return A request object setup for the URL.
     */
    auto Produce(
        const std::string& url) -> RequestHandle;

    /**
     * Produces a new Request with the specified curl_timeout.
     *
     * This produce method is best used for synchronous requests.
     *
     * @param url The url of the Request.
     * @param curl_timeout The maximum time to wait before quitting, calling the
     *          on_complete_handler and closing the connection.
     * @return A Request object setup for the URL + Timeout.
     */
    auto Produce(
        const std::string& url,
        std::chrono::milliseconds curl_timeout) -> RequestHandle;

    /**
     * Produces a new Request.  This function is thread safe.
     *
     * This produce method is best used for asynchronous requests.
     *
     * @param url The url of the Request.
     * @param on_complete_handler The on completion callback handler for this request.
     * @param curl_timeout The maximum time to wait before quitting, calling the
     *          on_complete_handler and closing the connection.
     * @return A Request object setup for the URL + Timeout.
     */
    auto Produce(
        const std::string& url,
        std::function<void(RequestHandle)> on_complete_handler,
        std::chrono::milliseconds curl_timeout) -> RequestHandle;
    
    /**
     * Produces a new Request with the specified curl_timeout.
     *
     * This produce method is best used for synchronous requests.
     *
     * @param url The url of the Request.
     * @param curl_timeout The maximum time to wait before quitting, calling the
     *          on_complete_handler and closing the connection.
     * @param response_wait_time The maximum time to wait before calling the on complete callback --
     *          the request will still wait for the connection to return until the curl_timeout,
     *          but the Request will no longer be accessible.
     * @return A Request object setup for the URL + Timeout.
     */
    auto Produce(
        const std::string& url,
        std::chrono::milliseconds curl_timeout,
        std::chrono::milliseconds response_wait_time) -> RequestHandle;
    
    /**
     * Produces a new Request.  This function is thread safe.
     *
     * This produce method is best used for asynchronous requests.
     *
     * @param url The url of the Request.
     * @param on_complete_handler The on completion callback handler for this request.
     * @param curl_timeout The maximum time to wait before quitting, calling the
     *          on_complete_handler and closing the connection.
     * @param response_wait_time The maximum time to wait before calling the on complete callback --
     *          the request will still wait for the connection to return until the curl_timeout,
     *          but the Request will no longer be accessible.
     * @return A Request object setup for the URL + Timeout.
     */
    auto Produce(
        const std::string& url,
        std::function<void(RequestHandle)> on_complete_handler,
        std::chrono::milliseconds curl_timeout,
        std::optional<std::chrono::milliseconds> response_wait_time) -> RequestHandle;
    
private:
    /// Used for thread safe calls.
    std::mutex m_mutex {};
    /// Pool of un-used Request handles.
    std::deque<std::unique_ptr<Request>> m_requests {};

    /**
     * Returns a Request object to the pool to be re-used.
     *
     * This function is thread safe.
     *
     * @param request The request to return to the pool to be re-used.
     */
    auto returnRequest(
        std::unique_ptr<Request> request) -> void;
};

} // lift
