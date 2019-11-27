#pragma once

#include <memory>

namespace lift {

class RequestPool;
class Request;

class SharedRequest
{
public:
    SharedRequest(RequestPool& request_pool, std::unique_ptr<Request> request_handle);
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
    auto GetAsReference() -> Request& { return *m_request; }
    [[nodiscard]]
    auto GetAsReference() const -> const Request& { return *m_request; }
    [[nodiscard]]
    auto GetAsPointer() -> Request* { return m_request.get(); }
    [[nodiscard]]
    auto GetAsPointer() const -> const Request* { return m_request.get(); }
    /** @} */
private:
    /// The request pool that owns this request.
    RequestPool& m_request_pool;
    
    /// The actual underlying request object.
    std::unique_ptr<Request> m_request;
};

} // namespace lift
