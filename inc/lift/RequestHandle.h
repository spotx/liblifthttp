#pragma once

#include "lift/Request.h"
#include "lift/SharedRequest.h"

#include <memory>

namespace lift {

class RequestPool;
class EventLoop;

/**
 * This is a proxy object to automatically reclaim finished requests
 * into the RequestPool.  The user simply uses it like a std::unique_ptr
 * by accessing the underlying RequestHandle via the * or -> operators.
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
    
    [[nodiscard]]
    auto createSharedRequestOnHeap() const -> std::unique_ptr<std::shared_ptr<SharedRequest>>;
    
    [[nodiscard]]
    auto createCopyOfSharedRequest() const -> std::shared_ptr<SharedRequest>
    {
        return m_shared_request;
    }
    
    std::shared_ptr<SharedRequest> m_shared_request;

    /// Friend so it can release the m_request_handle appropriately.
    friend auto requests_accept_async(
        uv_async_t* handle) -> void;
};

} // lift
