#include "lift/SharedRequest.h"
#include "lift/Request.h"
#include "lift/RequestPool.h"

namespace lift
{

SharedRequest::SharedRequest(RequestPool* request_pool, std::unique_ptr<Request> request_handle)
    : m_request_pool(request_pool), m_request(std::move(request_handle))
{
}

SharedRequest::~SharedRequest()
{
    /**
     * Only move the request handle into the pool if this is the 'valid'
     * request object that still owns the data.
     */
    if (m_request != nullptr && m_request_pool != nullptr)
    {
        m_request_pool->returnRequest(std::move(m_request));
        m_request_pool = nullptr;
        m_request = nullptr;
    }
}

}
