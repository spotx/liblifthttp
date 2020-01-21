#pragma once

#include "lift/Header.h"
#include "lift/Http.h"
#include "lift/RequestStatus.h"

#include <curl/curl.h>
#include <uv.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace lift {
class RequestHandle;
class RequestPool;
class EventLoop;
class SharedRequest;

class ResponseWaitTimeWrapper;

class Request {
    friend class EventLoop;
    friend class RequestPool;

public:
    ~Request();

    /**
     * Do not move or copy these objects anywhere, they should always be wrapped in
     * a unique_ptr for cheapness of moving their internals as well as maintaining
     * the life time of these objects in the pool
     * @{
     */
    Request(const Request&) = delete;
    Request(Request&&) = delete;
    auto operator=(const Request&) = delete;
    auto operator=(Request &&) -> Request& = delete;
    /** @} */

    /**
     * @param on_complete_handler When this request completes this handle is called.
     */
    auto SetOnCompleteHandler(std::function<void(RequestHandle)> on_complete_handler) -> void;

    /**
     * @param url The URL of the HTTP request.
     * @return True if the url was set.
     */
    auto SetUrl(const std::string& url) -> bool;

    /**
     * @return The currently set URL for this HTTP request.
     */
    [[nodiscard]]
    auto GetUrl() const -> std::string_view;

    /**
     * @param http_method Sets the HTTP method for this request.
     */
    auto SetMethod(
        http::Method http_method) -> void;

    /**
     * @param http_version Sets the HTTP version for this request.
     */
    auto SetVersion(
        http::Version http_version) -> void;

    /**
     * Sets the full timeout for this HTTP request.  This should be set before Perform() is called
     * or if this is an AsyncRequest before adding to an EventLoop.
     * NOTE: If you have also set the response wait time, this is should be longer than than the
     * response wait time and should be used to keep connections alive. Otherwise, this is the only
     * timeout that will be used
     * @param timeout The timeout for the request.
     * @return True if the timeout was set.
     */
    auto SetCurlTimeout(std::chrono::milliseconds timeout) -> bool;

    /**
     * Get/set optional chrono milliseconds indicating how long the request has
     * before it times out. This should only be set when you want to give the request
     * a longer timeout to accommodate keep-alive connections that may have a slow
     * response.
     * @{
     */
    [[nodiscard]]
    auto GetResponseWaitTime() const -> const std::optional<std::chrono::milliseconds>&
    {
        return m_response_wait_time;
    }

    auto SetResponseWaitTime(std::chrono::milliseconds timeout) -> void
    {
        m_response_wait_time.emplace(timeout);
    }

    /** @} */

    /**
     * Sets the maximum number of bytes of data to write.
     *
     * To download a full file, set max_download_bytes to -1.
     *
     * The number of bytes downloaded may be greater than the set amount,
     * but the number of bytes written for the response's data will not
     * exceed this amount.
     * @param max_download_bytes The maximum number of bytes to be written for this request.
     */
    auto SetMaxDownloadBytes(
        ssize_t max_download_bytes) -> void;

    /**
     * Sets if this request should follow redirects.  By default following redirects is
     * enabled.
     * @param follow_redirects True to follow redirects, false to stop.
     * @param max_redirects The maximum number of redirects to follow, -1 is infinite, 0 is none.
     * @return True if the follow redirects was set.
     */
    auto SetFollowRedirects(
        bool follow_redirects,
        int64_t max_redirects = -1) -> bool;

    /**
     * Adds a request header with an empty value.  This can be useful to manipulate cURL.
     * @param name The name of the header, e.g. 'Accept'.
     */
    auto AddHeader(
        std::string_view name) -> void;

    /**
     * Adds a request header with its value.
     * @param name The name of the header, e.g. 'Connection'.
     * @param value The value of the header, e.g. 'Keep-Alive'.
     */
    auto AddHeader(
        std::string_view name,
        std::string_view value) -> void;

    /**
     * @return The list of headers applied to this request.
     */
    [[nodiscard]]
    auto GetRequestHeaders() const -> const std::vector<Header>&;

    /**
     * Sets the request to HTTP POST and the body of the request
     * to the provided data.
     *
     * NOTE: this is mutually exclusive with using AddMimeField or AddMimeFileField,
     * as you cannot include traditional POST data in a mime-type form submission.
     *
     * @param data The request data to send in the HTTP POST.
     *
     * @throws std::logic_error If called after using AddMimeField or AddMimeFileField
     */
    auto SetRequestData(
        std::string data) -> void;

    /**
     * @return The request data.  If never set an empty string is returned.
     */
    [[nodiscard]]
    auto GetRequestData() const -> const std::string&;

    /**
     * Adds an additional mime field to the request. This is only valid for POST
     * requests, and for submitting HTML form-like data
     *
     * NOTE: this is mutually exclusive with using SetRequestData, as you cannot
     * include traditional POST data in a mime-type form submission.
     *
     * NOTE: Fields are specifically const std::string& because the underlying curl library
     * only takes a char* that is required to be null terminated.
     *
     * @param field_name The name of the form field.
     * @param field_value The value for the form field.
     *
     * @throws std::logic_error If called after using SetRequestData.
     */
    auto AddMimeField(
        const std::string& field_name,
        const std::string& field_value) -> void;

    /**
     * Adds an additional mime field (as a file) to the request. This is only valid
     * for POST requests, and for submitting HTML form-like data
     *
     * NOTE: this is mutually exclusive with using SetRequestData, as you cannot
     * include traditional POST data in a mime-type form submission.
     *
     * @param field_name The name of the form field, this will be the filename as received
     * by the other side.
     * @param field_filepath The path value for the form field, a file path of the file to treat
     * as a form file upload. This file must exist and be readable when this Request is
     * actually performed (e.g. the file data is streamed on demand, and isn't loaded
     * when this function is called). This path is only used on the request-side to read
     * the data, the field_name .
     *
     * @throws std::logic_error If called after using SetRequestData.
     * @throws std::runtime_error If the file from `field_filepath` doesn't exist.
     */
    auto AddMimeField(
        const std::string& field_name,
        const std::filesystem::path& field_filepath) -> void;

    /**
     * Performs the HTTP request synchronously.  This call will block the calling thread.
     * @return True if the request was successful.
     */
    auto Perform() -> bool;

    /**
     * @return The HTTP response status code.
     */
    [[nodiscard]]
    auto GetResponseStatusCode() const -> http::StatusCode;

    /**
     * @return The HTTP response headers.
     */
    [[nodiscard]]
    auto GetResponseHeaders() const -> const std::vector<Header>&;

    /**
     * @return The HTTP download payload.
     */
    [[nodiscard]]
    auto GetResponseData() const -> const std::string&;

    /**
     * @return The total HTTP request time in milliseconds as an optional (will be empty
     *          if the request has not finished yet).
     */
    [[nodiscard]]
    auto GetTotalTime() const -> const std::optional<std::chrono::milliseconds>&
    {
        return m_total_time;
    }

    /**
     * The completion status is how the request ended up in the event loop.
     * It might have completed successfully, or timed out, or had an SSL error, etc.
     *
     * This is not the HTTP status code returned by the remote server.
     *
     * @return Gets the request completion status.
     */
    [[nodiscard]]
    auto GetCompletionStatus() const -> RequestStatus;

    /**
     * @return  the number of connections made to make this request
     */
     [[nodiscard]]
    auto GetNumConnects() const -> uint64_t;

    /**
     * Set the verify behavior of the CURLOPT_SSL_VERIFYPEER on the curl_handle
     * @param verify Bool indicating whether we should require cURL to verify the SSL peer (true) or not (false)
     */
    auto SetVerifySSLPeer(bool verify) -> void;

    /**
     * Set the verify behavior of the CURLOPT_SSL_VERIFYHOST on the curl_handle
     * @param verify Bool indicating whether we should require cURL to verify the SSL host (true) or not (false)
     */
    auto SetVerifySSLHost(bool verify) -> void;

    /**
     * Tells cURL to use an Accept-Encoding header that includes all built-in
     * supported encodings in a comma-separated list.
     * IMPORTANT: Using this is mutually exclusive with adding your own Accept-Encoding header.
     */
    auto SetAcceptAllEncoding() -> void;

    /**
     * Resets the request to be re-used.  This will clear everything on the request.
     */
    auto Reset() -> void;

    /**
     * The redirect count is the number of redirections that were actually followed by the request.
     *
     * @return Gets the request redirect count.
     */
    [[nodiscard]]
    auto GetRedirectCount() const -> uint64_t;

private:
    /**
     * Private constructor -- only the RequestPool can create new Requests.
     * @param request_pool The request pool that generated this handle.
     * @param url          The url for the request.
     * @param curl_timeout The maximum time to wait before quitting, calling the on_complete_handler
     *                           and closing the connection.
     * @param response_wait_time Optional chrono milliseconds that indicate the maximum time to wait before
     *          calling the on complete callback -- the request will still wait for the connection to
     *          return until the curl_timeout, but the Request will no longer be accessible.
     *          If response_wait_time does not have a value, only curl_timeout is used and the functionality
     *          around response_wait_time will not be used.
     * @param on_complete_handler   Function to be called when the CURL request finishes.
     * @param max_download_bytes    The maximum number of bytes to download, if -1, will download entire file.
     */
    explicit Request(
        RequestPool& request_pool,
        const std::string& url,
        std::chrono::milliseconds curl_timeout,
        std::optional<std::chrono::milliseconds> response_wait_time,
        std::function<void(RequestHandle)> on_complete_handler = nullptr,
        ssize_t max_download_bytes = -1);

    auto init() -> void;

    /**
     * Sets the timepoint for when the request started -- we want this is set so we can calculate how long the
     * request took in the event there is a response wait time timeout.
     * @param start_time uint64_t indicating the timepoint when the request was started. Using a uint64_t as
     *                   this is the type libuv uses for time points.
     */
    auto setStartTime(uint64_t start_time) -> void
    {
        m_start_time = start_time;
    }

    /**
     * Sets m_total_time by either getting the total time from cURL (when the request did NOT
     * time out due to response wait time) or by finding the druation from when the request started
     * to now (when the request did NOT time out due to response wait time).
     * @param finish_time Optional that will contain a uint64_t indicating the timepoint when
     *                    the request was timed out while waiting for the response time.
     *                    If the request received a response or timed out via cURL, this will be
     *                    empty and we'll get the total time from the cURL handle.
     */
    auto setTotalTime(std::optional<uint64_t> finish_time) -> void;

    /// The onComplete() handler for asynchronous requests.
    std::function<void(RequestHandle)> m_on_complete_handler;

    /// The request pool this request was produced from.
    RequestPool& m_request_pool;

    /// The cURL handle for this request.
    CURL* m_curl_handle { curl_easy_init() };

    /// A view into the curl url.
    std::string_view m_url;
    /// The request headers.
    std::string m_request_headers {};
    /// The request headers index.  Used to generate the curl slist.
    std::vector<Header> m_request_headers_idx {};
    /// The curl request headers.
    curl_slist* m_curl_request_headers { nullptr };
    /// Have the headers been committed into cURL?
    bool m_headers_committed { false };
    /// The request data if any. Mutually exclusive with m_mime_handle.
    std::string m_request_data {};
    /// The mime handle, if any (only created when needed). Mutually exclusive with m_request_data.
    curl_mime* m_mime_handle { nullptr };

    /// The status of this HTTP request.
    RequestStatus m_status_code { RequestStatus::BUILDING };
    /// The response headers.
    std::string m_response_headers {};
    /// Views into each header.
    std::vector<Header> m_response_headers_idx {};
    /// The response data if any.
    std::string m_response_data {};

    /// Maximum number of bytes to be written.
    ssize_t m_max_download_bytes { 0 };
    /// Number of bytes that have been written so far.
    ssize_t m_bytes_written { 0 };

    /**
     * The timepoint for when the request started -- we want this is set so we can calculate how long the
     * request took in the event there is a response wait time timeout.
     * We're using a uint64_t since we get the timepoint from libuv and it uses uint64_t.
     */
    uint64_t  m_start_time{0};

    /// The total time it took to receive a response -- will only be set once we receive a response.
    std::optional<std::chrono::milliseconds> m_total_time;

    /**
     * Bool indicating whether or not onComplete has been called (true) or not (false) so if a request exceeds
     * its response wait time, its on complete handler can be called only once.
     */
    std::atomic_bool m_on_complete_has_been_called{false};

    /**
     * Optional milliseconds indicating the response wait time for the request. If it is set, the request's
     * on complete handler will be called even if we have not received a response and the status code will be
     * set to indicate a response wait time timeout.
     */
    std::optional<std::chrono::milliseconds> m_response_wait_time;

    /**
     * Optional iterator to the location in the EventLoop's multiset where the corresponding ResponseWaitTimeWrapper.
     * This will only be set if the response wait time is used.
     */
    std::optional<std::multiset<ResponseWaitTimeWrapper>::iterator> m_response_wait_time_set_iterator;

    /// HTTP status code for request -- will either be set from curl response or use default of 504 (indicating a timeout)
    http::StatusCode m_http_status_code{http::StatusCode::HTTP_UNKNOWN};

    /**
     * Prepares the request to be performed.  This is called on a request
     * before it is sync or async executed.
     *
     * Commits the request headers if any are available.
     */
    auto prepareForPerform() -> void;

    /**
     * Clears response buffers unrelated to curl.  This is useful if you want
     * to make the exact same request multiple times.
     */
    auto clearResponseBuffers() -> void;

    /**
     * Converts a CURLcode into a RequestStatus.
     * @param curl_code The CURLcode to convert.
     * @return The lift RequestStatus that corresponds to the CURLcode
     */
    auto convertCompletionStatus(CURLcode curl_code) -> RequestStatus;

    /**
     * @param event_loop Reference to the EventLoop that is calling onComplete (so requests that have
     *          response wait times can be removed from the multiset of ResponseWaitTimeWrapper)
     * @param completion_status RequestStatus enum indicating the status of the lift Request when it has completed
     *                          (Success, error, etc.).
     * @param shared_request Shared pointer to the SharedRequest that owns this Request, so it can be used to create
     *                       a RequestHandle and return the Request to the RequestPool if necessary.
     * @param finish_time Optional that will contain a uint64_t indicating the timepoint when request was timed out
     *                    while waiting for the response time.  If the request received a response or timed out via
     *                    cURL, this will be empty and we'll get the total time from the cURL handle.
     *                    Default is an empty optional.
     */
    auto onComplete(EventLoop& event_loop, RequestStatus completion_status, std::shared_ptr<SharedRequest> shared_request, std::optional<uint64_t> finish_time = std::nullopt) -> void;

    /**
     * Helper function to find how many bytes are left to be downloaded for a request
     * @return ssize_t found by subtracting total number of downloaded bytes from max_download_bytes
     */
    auto getRemainingDownloadBytes() -> ssize_t;

    /**
     * @param set_location The iterator from the set of ResponseWaitTimeWrapper used to "time out"
     * requests who have not received their responses within the response wait time.
     */
    auto setTimeoutIterator(std::multiset<ResponseWaitTimeWrapper>::iterator set_location) -> void
    {
        m_response_wait_time_set_iterator.emplace(set_location);
    }

    /**
     * Updates the curl handle (m_curl_handle) so that its private data points to the shared_ptr<SharedRequest>
     * on the heap in order to maintain the lifetime of SharedRequest.
     * @param shared_request Pointer to the shared pointer to the SharedRequest that this curl handle should
     *          use in callbacks.
     */
    auto setSharedPointerOnCurlHandle(std::shared_ptr<SharedRequest>* shared_request) -> void
    {
        curl_easy_setopt(m_curl_handle, CURLOPT_PRIVATE, shared_request);
    }

    /**
     * Sets the HTTP status code (m_http_status_code) using the cURL request -- this will be called either in
     * onComplete if the request did not time out waiting for a response or in Perform when a synchronous request
     * is complete.
     */
    auto setHttpStatusCodeFromCurl() -> void;

    /// libcurl will call this function when a header is received for the HTTP request.
    friend auto curl_write_header(
        char* buffer,
        size_t size,
        size_t nitems,
        void* user_ptr) -> size_t;

    /// libcurl will call this function when data is received for the HTTP request.
    friend auto curl_write_data(
        void* buffer,
        size_t size,
        size_t nitems,
        void* user_ptr) -> size_t;

    /// libuv will call this function when the AddRequest() function is called.
    friend auto requests_accept_async(
        uv_async_t* handle) -> void;

    friend auto on_response_wait_time_expired_callback(uv_timer_t* handle) -> void;
};

/**
 * Class wrapping information used by EventLoop to "time out" requests that have not
 * received a response within the expected wait time.
 */
class ResponseWaitTimeWrapper
{
public:
    ResponseWaitTimeWrapper(uint64_t timeout_time, std::shared_ptr<SharedRequest> request) : m_data{timeout_time, std::move(request)} {}

    struct Data
    {
        /**
         * Represents the time point when the corresponding request should be timed out.
         * Using uint64_t for consistency since uv_now returns time points as uint64_t.
         */
        uint64_t m_timeout_time;
        /// Reference to the Request associated with this wrapper.
        std::shared_ptr<SharedRequest> m_shared_request_ptr_pointer;
    };

    /**
     * @return Reference to the const Data struct associated with this wrapper.
     */
    [[nodiscard]]
    auto GetData() const -> const Data& { return m_data; }

    /**
     * Less than operator used by the multiset of ResponseWaitTimeWrapper to find the correct slot to insert this into.
     * @param other Reference to const ResponseWaitTimeWrapper to use for comparison.
     * @return Bool indicating if this wrapper is less than the other wrapper (true) or not (false)
     */
    inline auto operator<(const ResponseWaitTimeWrapper& other) const -> bool
    {
        return m_data.m_timeout_time < other.m_data.m_timeout_time;
    }

private:
    const Data m_data;
};

} // namespace lift
