#include "lift/Request.h"
#include "lift/EventLoop.h"
#include "lift/RequestHandle.h"

#include <cstring>

namespace lift {
auto curl_write_header(
    char* buffer,
    size_t size,
    size_t nitems,
    void* user_ptr) -> size_t;

auto curl_write_data(
    void* buffer,
    size_t size,
    size_t nitems,
    void* user_ptr) -> size_t;

static constexpr uint64_t HEADER_DEFAULT_MEMORY_BYTES = 16'384;
static constexpr uint64_t HEADER_DEFAULT_COUNT = 16;

Request::Request(
    RequestPool& request_pool,
    const std::string& url,
    std::chrono::milliseconds curl_timeout,
    std::optional<std::chrono::milliseconds> response_wait_time,
    std::function<void(RequestHandle)> on_complete_handler,
    ssize_t max_download_bytes)
    : m_on_complete_handler(std::move(on_complete_handler))
    , m_request_pool(request_pool)
    , m_max_download_bytes(max_download_bytes)
{
    init();
    SetUrl(url);
    SetCurlTimeout(curl_timeout);
    if (response_wait_time.has_value())
    {
        SetResponseWaitTime(response_wait_time.value());
    }
}

Request::~Request()
{
    Reset();

    // Reset doesn't delete the CURL* handle since it can be re-used between requests.
    // Delete it now.
    if (m_curl_handle != nullptr) {
        curl_easy_cleanup(m_curl_handle);
        m_curl_handle = nullptr;
    }
}

auto Request::init() -> void
{
    curl_easy_setopt(m_curl_handle, CURLOPT_HEADERFUNCTION, curl_write_header);
    curl_easy_setopt(m_curl_handle, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(m_curl_handle, CURLOPT_WRITEFUNCTION, curl_write_data);
    curl_easy_setopt(m_curl_handle, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_curl_handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(m_curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

    // Set the private pointer in the curl handle to a nullptr, because the shared_request
    // it's pointing to should now be destructed.
    setSharedPointerOnCurlHandle(nullptr);

    SetMaxDownloadBytes(m_max_download_bytes);

    m_request_headers.reserve(HEADER_DEFAULT_MEMORY_BYTES);
    m_request_headers_idx.reserve(HEADER_DEFAULT_COUNT);
    m_headers_committed = false;

    m_response_headers.reserve(HEADER_DEFAULT_MEMORY_BYTES);
    m_response_headers_idx.reserve(HEADER_DEFAULT_COUNT);
    m_response_data.reserve(HEADER_DEFAULT_MEMORY_BYTES);
}

auto Request::setTotalTime(std::optional<uint64_t> finish_time) -> void
{
    if (finish_time.has_value())
    {
        m_total_time.emplace(std::chrono::milliseconds{finish_time.value() - m_start_time});
    }
    else
    {
        double total_time = 0;
        curl_easy_getinfo(m_curl_handle, CURLINFO_TOTAL_TIME, &total_time);

        // std::duration defaults to seconds, so don't need to duration_cast total time to seconds.
        m_total_time.emplace(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>{total_time}));
    }
}



auto Request::SetOnCompleteHandler(
    std::function<void(RequestHandle)> on_complete_handler) -> void
{
    m_on_complete_handler = std::move(on_complete_handler);
}

auto Request::SetUrl(const std::string& url) -> bool
{
    if (url.empty()) {
        return false;
    }

    auto error_code = curl_easy_setopt(m_curl_handle, CURLOPT_URL, url.data());
    if (error_code == CURLE_OK) {
        char* curl_url = nullptr;
        curl_easy_getinfo(m_curl_handle, CURLINFO_EFFECTIVE_URL, &curl_url);
        if (curl_url != nullptr) {
            m_url = std::string_view { curl_url, std::strlen(curl_url) };
            return true;
        }
    }

    return false;
}

auto Request::GetNumConnects() const -> uint64_t
{
    long count = 0;
    curl_easy_getinfo(m_curl_handle, CURLINFO_NUM_CONNECTS, &count);

    return static_cast<uint64_t>(count);
}

auto Request::GetUrl() const -> std::string_view
{
    return m_url;
}

auto Request::SetMethod(
    http::Method http_method) -> void
{
    switch (http_method) {
    case http::Method::GET:
        curl_easy_setopt(m_curl_handle, CURLOPT_HTTPGET, 1L);
        break;
    case http::Method::HEAD:
        curl_easy_setopt(m_curl_handle, CURLOPT_NOBODY, 1L);
        break;
    case http::Method::POST:
        curl_easy_setopt(m_curl_handle, CURLOPT_POST, 1L);
        break;
    case http::Method::PUT:
        curl_easy_setopt(m_curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
        break;
    case http::Method::DELETE:
        curl_easy_setopt(m_curl_handle, CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    case http::Method::CONNECT:
        curl_easy_setopt(m_curl_handle, CURLOPT_CONNECT_ONLY, 1L);
        break;
    case http::Method::OPTIONS:
        curl_easy_setopt(m_curl_handle, CURLOPT_CUSTOMREQUEST, "OPTIONS");
        break;
    case http::Method::PATCH:
        curl_easy_setopt(m_curl_handle, CURLOPT_CUSTOMREQUEST, "PATCH");
        break;
    }
}

auto Request::SetVersion(
    http::Version http_version) -> void
{
    switch (http_version) {
    case http::Version ::USE_BEST:
        curl_easy_setopt(m_curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_NONE);
        break;
    case http::Version::V1_0:
        curl_easy_setopt(m_curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
        break;
    case http::Version::V1_1:
        curl_easy_setopt(m_curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        break;
    case http::Version::V2_0:
        curl_easy_setopt(m_curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
        break;
    case http::Version::V2_0_TLS:
        curl_easy_setopt(m_curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
        break;
    case http::Version::V2_0_ONLY:
        curl_easy_setopt(m_curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
        break;
    }
}

auto Request::SetMaxDownloadBytes(ssize_t max_download_bytes) -> void
{
    m_max_download_bytes = max_download_bytes;
    m_bytes_written = 0;
}

auto Request::SetCurlTimeout(
    std::chrono::milliseconds timeout) -> bool
{
    int64_t timeout_ms = timeout.count();

    if (timeout_ms > 0) {
        auto error_code = curl_easy_setopt(m_curl_handle, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms));
        return (error_code == CURLE_OK);
    }
    return false;
}

auto Request::SetFollowRedirects(
    bool follow_redirects,
    int64_t max_redirects) -> bool
{
    long curl_value = (follow_redirects) ? 1L : 0L;
    auto error_code1 = curl_easy_setopt(m_curl_handle, CURLOPT_FOLLOWLOCATION, curl_value);
    auto error_code2 = curl_easy_setopt(m_curl_handle, CURLOPT_MAXREDIRS, static_cast<long>(max_redirects));
    return (error_code1 == CURLE_OK && error_code2 == CURLE_OK);
}

auto Request::AddHeader(
    std::string_view name) -> void
{
    AddHeader(name, std::string_view {});
}

auto Request::AddHeader(
    std::string_view name,
    std::string_view value) -> void
{
    m_headers_committed = false; // A new header was added, they need to be committed again.
    size_t capacity = m_request_headers.capacity();
    size_t header_len = name.length() + value.length() + 3; //": \0"
    size_t total_len = m_request_headers.size() + header_len;
    if (capacity < total_len) {
        do {
            capacity *= 2;
        } while (capacity < total_len);
        m_request_headers.reserve(capacity);
    }

    const char* start = m_request_headers.data() + m_request_headers.length();

    m_request_headers.append(name.data(), name.length());
    m_request_headers.append(": ");
    if (!value.empty()) {
        m_request_headers.append(value.data(), value.length());
    }
    m_request_headers += '\0'; // curl expects null byte, do not use string.append, it ignores null terminators!

    std::string_view full_header { start, header_len - 1 }; // subtract off the null byte
    m_request_headers_idx.emplace_back(full_header);
}

auto Request::GetRequestHeaders() const -> const std::vector<Header>&
{
    return m_request_headers_idx;
}

auto Request::SetRequestData(
    std::string data) -> void
{
    if (m_mime_handle != nullptr) {
        throw std::logic_error("Cannot SetRequestData on Request after using AddMimeField");
    }

    if(data.empty()) {
        return;
    }

    // libcurl expects the data lifetime to be longer
    // than the request so require it to be moved into
    // the lifetime of the request object.
    m_request_data = std::move(data);

    curl_easy_setopt(m_curl_handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(m_request_data.size()));
    curl_easy_setopt(m_curl_handle, CURLOPT_POSTFIELDS, m_request_data.data());
}

auto Request::GetRequestData() const -> const std::string&
{
    return m_request_data;
}

auto Request::AddMimeField(
    const std::string& field_name,
    const std::string& field_value) -> void
{
    if (!m_request_data.empty()) {
        throw std::logic_error("Cannot AddMimeField on Request after using SetRequestData");
    }

    if (m_mime_handle == nullptr) {
        m_mime_handle = curl_mime_init(m_curl_handle);
    }

    auto* field = curl_mime_addpart(m_mime_handle);

    curl_mime_name(field, field_name.data());
    curl_mime_data(field, field_value.data(), field_value.size());
}

auto Request::AddMimeField(
    const std::string& field_name,
    const std::filesystem::path& field_filepath) -> void
{
    if (!m_request_data.empty()) {
        throw std::logic_error("Cannot AddMimeField on Request after using SetRequestData");
    }

    if (!std::filesystem::exists(field_filepath)) {
        throw std::runtime_error("Filepath for AddMimeField doesn't exist");
    }

    if (m_mime_handle == nullptr) {
        m_mime_handle = curl_mime_init(m_curl_handle);
    }

    auto* field = curl_mime_addpart(m_mime_handle);

    curl_mime_filename(field, field_name.data());
    curl_mime_filedata(field, field_filepath.c_str());
}

auto Request::Perform() -> bool
{
    prepareForPerform();
    auto curl_error_code = curl_easy_perform(m_curl_handle);
    setCompletionStatus(curl_error_code);
    return (m_status_code == RequestStatus::SUCCESS);
}

auto Request::GetResponseStatusCode() const -> http::StatusCode
{
    long http_response_code = 0;
    curl_easy_getinfo(m_curl_handle, CURLINFO_RESPONSE_CODE, &http_response_code);
    return http::to_enum(static_cast<uint32_t>(http_response_code));
}

auto Request::GetResponseHeaders() const -> const std::vector<Header>&
{
    return m_response_headers_idx;
}

auto Request::GetResponseData() const -> const std::string&
{
    return m_response_data;
}

auto Request::GetCompletionStatus() const -> RequestStatus
{
    return m_status_code;
}

auto Request::SetVerifySSLPeer(bool verify) -> void
{
    if (verify)
    {
        curl_easy_setopt(m_curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
    }
    else
    {
        curl_easy_setopt(m_curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    }
}

auto Request::SetVerifySSLHost(bool verify) -> void
{
    if (verify)
    {
        // Re CURL docs (https://curl.haxx.se/libcurl/c/CURLOPT_SSL_VERIFYHOST.html),
        // depending on the version of curl used, setting CURLOPT_SSL_VERIFYHOST to 1L vs 2L will
        // have different results. To maintain forwards and backwards compatibility, using 2L
        curl_easy_setopt(m_curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    }
    else
    {
        curl_easy_setopt(m_curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
    }
}

auto Request::SetAcceptAllEncoding() -> void
{
    // From the CURL docs (https://curl.haxx.se/libcurl/c/CURLOPT_ACCEPT_ENCODING.html):
    // 'To aid applications not having to bother about what specific algorithms this particular
    // libcurl build supports, libcurl allows a zero-length string to be set ("") to ask for an
    // Accept-Encoding: header to be used that contains all built-in supported encodings.'
    curl_easy_setopt(m_curl_handle, CURLOPT_ACCEPT_ENCODING, "");
}

auto Request::Reset() -> void
{
    m_url = std::string_view {};
    m_request_headers.clear();
    m_request_headers_idx.clear();
    if (m_curl_request_headers != nullptr) {
        curl_slist_free_all(m_curl_request_headers);
        m_curl_request_headers = nullptr;
    }
    // replace rather than clear() since this buffer is 'moved' into the RequestHandle and will free up memory.
    m_request_data = std::string {};

    if (m_mime_handle != nullptr) {
        curl_mime_free(m_mime_handle);
        m_mime_handle = nullptr;
    }

    clearResponseBuffers();
    curl_easy_reset(m_curl_handle);
    init();
    m_status_code = RequestStatus::BUILDING;

    m_max_download_bytes = -1; // Set max download bytes to default to download entire file
    m_bytes_written = 0;

    m_on_complete_has_been_called.store(false);
    m_response_wait_time.reset();
    m_response_wait_time_set_iterator.reset();
}

auto Request::prepareForPerform() -> void
{
    clearResponseBuffers();
    if (!m_headers_committed && !m_request_headers_idx.empty()) {
        // Its possible the headers have been previous committed -- this will re-commit them all
        // in the event additional headers have been added between requests.
        if (m_curl_request_headers != nullptr) {
            curl_slist_free_all(m_curl_request_headers);
            m_curl_request_headers = nullptr;
        }

        for (auto header : m_request_headers_idx) {
            m_curl_request_headers = curl_slist_append(
                m_curl_request_headers,
                header.GetHeader().data());
        }

        curl_easy_setopt(m_curl_handle, CURLOPT_HTTPHEADER, m_curl_request_headers);
        m_headers_committed = true;
    }

    if (m_mime_handle != nullptr) {
        curl_easy_setopt(m_curl_handle, CURLOPT_MIMEPOST, m_mime_handle);
    }

    m_status_code = RequestStatus::EXECUTING;
}

auto Request::clearResponseBuffers() -> void
{
    m_response_headers.clear();
    m_response_headers_idx.clear();
    m_response_data.clear();
}

auto Request::setCompletionStatus(
    CURLcode curl_code) -> void
{
    // EventLoop's checkActions will call setCompletionStatus even If we have already timed out due to
    // response wait time, but want to preserve the RESPONSE_WAIT_TIME_TIMEOUT status for the user, so
    // if m_status_code is set to RESPONSE_WAIT_TIME_TIMEOUT, don't overwrite it.
    if (m_status_code == RequestStatus::RESPONSE_WAIT_TIME_TIMEOUT)
    {
        return;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
    switch (curl_code) {
    case CURLcode::CURLE_OK:
        m_status_code = RequestStatus::SUCCESS;
        break;
    case CURLcode::CURLE_GOT_NOTHING:
        m_status_code = RequestStatus::RESPONSE_EMPTY;
        break;
    case CURLcode::CURLE_OPERATION_TIMEDOUT:
        m_status_code = RequestStatus::TIMEOUT;
        break;
    case CURLcode::CURLE_COULDNT_CONNECT:
        m_status_code = RequestStatus::CONNECT_ERROR;
        break;
    case CURLcode::CURLE_COULDNT_RESOLVE_HOST:
        m_status_code = RequestStatus::CONNECT_DNS_ERROR;
        break;
    case CURLcode::CURLE_SSL_CONNECT_ERROR:
        m_status_code = RequestStatus::CONNECT_SSL_ERROR;
        break;
    case CURLcode::CURLE_WRITE_ERROR:
        /**
         * If there is a cURL write error, but the maximum number of bytes has been written,
         * then we intentionally aborted, so let's set this to success.
         * Otherwise, there was an error in the CURL write callback.
         */
        m_status_code = (getRemainingDownloadBytes() == 0)
            ? RequestStatus::SUCCESS
            : RequestStatus::DOWNLOAD_ERROR;
        break;
    case CURLcode::CURLE_SEND_ERROR:
        m_status_code = RequestStatus::ERROR_FAILED_TO_START;
        break;
    default:
        m_status_code = RequestStatus::ERROR;
        break;
    }
#pragma GCC diagnostic pop
}

auto Request::onComplete(EventLoop& event_loop, std::shared_ptr<SharedRequest> shared_request, std::optional<uint64_t> finish_time) -> void
{
    auto request_handle_ptr = RequestHandle(std::move(shared_request));

    // We only call the stored on complete function once!
    if (!m_on_complete_has_been_called.exchange(true, std::memory_order_acquire))
    {
        if (finish_time.has_value())
        {
            // But if the request did time out, set the on complete handler will know.
            m_status_code = RequestStatus::RESPONSE_WAIT_TIME_TIMEOUT;
        }

        setTotalTime(finish_time);

        if (m_on_complete_handler != nullptr)
        {
            // Call the on complete handler with a reference to the request.
            m_on_complete_handler(std::move(request_handle_ptr));
        }
    }

    // If we have an iterator, remove it from the set so we won't try to time it out and then call onComplete again.
    if (m_response_wait_time_set_iterator.has_value())
    {
        event_loop.removeTimeoutByIterator(m_response_wait_time_set_iterator.value());
    }
}

auto Request::getRemainingDownloadBytes() -> ssize_t
{
    return m_max_download_bytes - m_bytes_written;
}

auto curl_write_header(
    char* buffer,
    size_t size,
    size_t nitems,
    void* user_ptr) -> size_t
{
    auto* raw_request_ptr = static_cast<Request*>(user_ptr);

    // If we've already called the on complete handler, the request might still be in userland,
    // so we don't want to modify it.
    if (raw_request_ptr->m_on_complete_has_been_called.load())
    {
        return 0;
    }

    size_t data_length = size * nitems;

    std::string_view data_view { buffer, data_length };

    if (data_view.empty()) {
        return data_length;
    }

    // Ignore empty header lines from curl.
    if (data_view.length() == 2 && data_view == "\r\n") {
        return data_length;
    }
    // Ignore the HTTP/ 'header' line from curl.
    constexpr size_t HTTPSLASH_LEN = 5;
    if (data_view.length() >= 4 && data_view.substr(0, HTTPSLASH_LEN) == "HTTP/") {
        return data_length;
    }

    // Drop the trailing \r\n from the header.
    if (data_view.length() >= 2) {
        size_t rm_size = 0;
        if (data_view[data_view.length() - 1] == '\n') {
            ++rm_size;
        }
        if (data_view[data_view.length() - 2] == '\r') {
            ++rm_size;
        }
        data_view.remove_suffix(rm_size);
    }

    size_t capacity = raw_request_ptr->m_response_headers.capacity();
    size_t total_len = raw_request_ptr->m_response_headers.size() + data_view.length();
    if (capacity < total_len) {
        do {
            capacity *= 2;
        } while (capacity < total_len);
        raw_request_ptr->m_response_headers.reserve(capacity);
    }

    // Append the entire header into the full header buffer.
    raw_request_ptr->m_response_headers.append(data_view.data(), data_view.length());

    // Calculate and append the Header view object.
    const char* start = raw_request_ptr->m_response_headers.c_str();
    auto total_length = raw_request_ptr->m_response_headers.length();
    std::string_view request_data_view { (start + total_length) - data_view.length(), data_view.length() };
    raw_request_ptr->m_response_headers_idx.emplace_back(request_data_view);

    return data_length;
}

auto curl_write_data(
    void* buffer,
    size_t size,
    size_t nitems,
    void* user_ptr) -> size_t
{
    auto* raw_request_ptr = static_cast<Request*>(user_ptr);
    size_t data_length = size * nitems;

    // If m_max_download_bytes is greater than -1, we are performing partial download.
    if (raw_request_ptr->m_max_download_bytes > -1) {
        auto bytes_left_to_write = raw_request_ptr->getRemainingDownloadBytes();

        /**
         * If the max number of bytes left to write is less than the
         * data_length of the current frame, just set that as the data_length
         * when appending to the request's data string.
         * This will also cause the download to stop, since CURL aborts
         * when the length returned from the write callback does match the
         * length (size * nitems) passed in.
         **/
        if (bytes_left_to_write > -1) {
            auto ubytes_left_to_write = static_cast<size_t>(bytes_left_to_write);

            if (ubytes_left_to_write < data_length) {
                data_length = ubytes_left_to_write;
            }
        } else {
            /**
             * bytes_left_to_write should never be negative, but if it is,
             * stop this request, because something is wrong.
             */
            data_length = 0;
        }
    }

    raw_request_ptr->m_response_data.append(static_cast<const char*>(buffer), data_length);
    raw_request_ptr->m_bytes_written += data_length;

    return data_length;
}
} // lift
