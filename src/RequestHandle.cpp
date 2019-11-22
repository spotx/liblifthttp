#include "lift/RequestHandle.h"
#include "lift/RequestPool.h"

#include "lift/EventLoop.h"

namespace lift {

RequestHandle::RequestHandle(
    RequestPool* request_pool,
    std::unique_ptr<Request> request_handle)
    : m_shared_request(std::make_shared<SharedRequest>(request_pool, std::move(request_handle)))
{}

RequestHandle::RequestHandle(std::shared_ptr<SharedRequest> shared_request)
    : m_shared_request(std::move(shared_request)) {}

auto RequestHandle::createSharedRequestOnHeap() const -> std::shared_ptr<SharedRequest>*
{
    auto* result = new std::shared_ptr<SharedRequest>(m_shared_request);
    return result;
}

auto RequestHandle::operator*() -> Request&
{
    return m_shared_request->GetAsReference();
}

auto RequestHandle::operator*() const -> const Request&
{
    return m_shared_request->GetAsReference();
}

auto RequestHandle::operator-> () -> Request*
{
    return m_shared_request->GetAsPointer();
}

auto RequestHandle::operator-> () const -> const Request*
{
    return m_shared_request->GetAsPointer();
}


} // lift
