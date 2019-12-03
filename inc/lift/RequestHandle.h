#pragma once

#include "lift/Request.h"
#include "lift/SharedRequest.h"

#include <memory>

namespace lift {

class RequestPool;
class EventLoop;

/**
 * This is a proxy object wrapping a shared pointer to a SharedRequest, so the SharedRequest
 * can be automatically cleaned up when appropriate (by releasing it into the RequestPool).
 * The user simply uses it like a std::unique_ptr, accessing the underlying Request held by
 * the SharedRequest via the * or -> operators.
 */
class RequestHandle {
    friend class RequestPool;
    friend class Request;
    friend class EventLoop;

public:
    ~RequestHandle() = default;
    RequestHandle(const RequestHandle&) = delete;
    RequestHandle(RequestHandle&& from) = default;
    auto operator=(const RequestHandle&) = delete;
    auto operator=(RequestHandle &&) -> RequestHandle& = default;

    /**
     * @return Access to the underlying request handle.
     * @{
     */
    auto operator*() -> Request&;
    auto operator*() const -> const Request&;
    auto operator-> () -> Request*;
    auto operator-> () const -> const Request*;
    /** @} */

private:
    RequestHandle(
        RequestPool& request_pool,
        std::unique_ptr<Request> request_handle);
    
    explicit RequestHandle(std::shared_ptr<SharedRequest> shared_request);
    
    /**
     * Create and return unique pointer to a shared pointer to a SharedRequest so caller
     * can either release the pointer (and reacquire in libuv callback) or go out of scope
     * and clean up the shared pointer.
     * @return New unique pointer to a shared pointer to a SharedRequest
     */
    [[nodiscard]]
    auto createSharedRequestOnHeap() const -> std::unique_ptr<std::shared_ptr<SharedRequest>>
    {
        return std::make_unique<std::shared_ptr<SharedRequest>>(m_shared_request);
    }
    
    /**
     * Shared pointer to a SharedRequest, so when the handle goes out of the scope, the SharedRequest
     * can be cleaned up if there are no other shared pointers pointing to the underlying object.
     */
    std::shared_ptr<SharedRequest> m_shared_request;

    /// Friend so it can release the m_request_handle appropriately.
    friend auto requests_accept_async(
        uv_async_t* handle) -> void;
};

} // lift
