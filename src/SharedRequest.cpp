#include "lift/SharedRequest.h"
#include "lift/Request.h"
#include "lift/RequestPool.h"

namespace lift
{

SharedRequest::SharedRequest(RequestPool& request_pool, std::unique_ptr<Request> request_handle)
    : m_request_pool(request_pool), m_request(std::move(request_handle))
{
}

SharedRequest::~SharedRequest()
{
    /**
     * On destruction, we return the Request object back to the pool so it can be reused.
     * Because SharedRequest is always stored in an shared pointer, this allows us to ensure
     * that the request will only be returned to the pool when all references to it have gone away.
     *
     * This is a concern in cases where we have a timeout due to response wait time -- it is
     * possible for us to be in the user's callback when we get an actual response, which could
     * clean up before the user's callback finishes, resulting in the user's callback working on
     * a request that has been reset and returned to the pool.
     */
    m_request_pool.returnRequest(std::move(m_request));
}

}
