#pragma once

#include <memory>

namespace lift {

class RequestPool;
class Request;

class SharedRequest
{
public:
    SharedRequest(RequestPool* request_pool, std::unique_ptr<Request> request_handle)
        : m_request_pool(request_pool), m_request_handle(std::move(request_handle)) {}
    SharedRequest(const SharedRequest&) = delete;
    SharedRequest(SharedRequest&&) = delete;
    auto operator=(const SharedRequest&) = delete;
    auto operator=(SharedRequest&&) = delete;
    ~SharedRequest();
    
    /**
     * @return Access to the underlying request handle.
     * @{
     */
    [[nodiscard]]
    auto GetAsReference() -> Request&;
    [[nodiscard]]
    auto GetAsReference() const -> const Request&;
    [[nodiscard]]
    auto GetAsPointer() -> Request*;
    [[nodiscard]]
    auto GetAsPointer() const -> const Request*;
    /** @} */
private:
    /// The request pool that owns this request.
    RequestPool* m_request_pool;
    
    /// The actual underlying request object.
    std::unique_ptr<Request> m_request_handle;
};

} // namespace lift
