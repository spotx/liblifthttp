#include "lift/SharedRequest.h"
#include "lift/Request.h"
#include "lift/RequestPool.h"

namespace lift
{

SharedRequest::~SharedRequest()
{
    /**
     * Only move the request handle into the pool if this is the 'valid'
     * request object that still owns the data.
     */
    if (m_request_handle != nullptr && m_request_pool != nullptr)
    {
        m_request_pool->returnRequest(std::move(m_request_handle));
        m_request_pool = nullptr;
        m_request_handle = nullptr;
    }
}

auto SharedRequest::GetAsReference() -> Request&
{
    return *m_request_handle;
}

auto SharedRequest::GetAsReference() const -> const Request&
{
    return *m_request_handle;
}

auto SharedRequest::GetAsPointer() -> Request*
{
    return m_request_handle.get();
}

auto SharedRequest::GetAsPointer() const -> const Request*
{
    return m_request_handle.get();
}

}
