#include <lift/Lift.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>

static uint64_t timeout_count{0};
static uint64_t response_count{0};

static auto on_complete(lift::RequestHandle request_handle) -> void
{
    using namespace std::chrono_literals;

    auto& request = *request_handle;
    std::cout << "For request with url " << request.GetUrl() << ", ";
    if (request.GetCompletionStatus() == lift::RequestStatus::SUCCESS) {
        ++response_count;
        std::cout << "requested was successfully completed in " << request.GetTotalTime().value_or(0ms).count() << " ms" << std::endl;
        std::cout << "Received response body was: " << request.GetResponseData() << std::endl;
    } else {
        ++timeout_count;
        std::cout << "request was not successfully completed, with error: " << to_string(request.GetCompletionStatus()) << std::endl;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 5)
    {
        std::cout
            << "Please provide URL as the first argument, "
            << "millisecond request timeout as the second argument, "
            << "millisecond request curl timeout time as the third argument, and "
            << "the number of requests to send as the fourth argument."
            << std::endl;
        
        return 0;
    }
    
    using namespace std::chrono_literals;
    
    // Initialize must be called first before using the LiftHttp library.
    lift::GlobalScopeInitializer lift_init{};
    
    std::string url{argv[1]};
    std::chrono::milliseconds timeout_time{std::stoi(argv[2])};
    std::chrono::milliseconds curl_timeout_time{std::stoi(argv[3])};
    std::size_t num_requests{static_cast<std::size_t>(std::stoi(argv[4]))};
    
    {
        lift::EventLoop event_loop;
        // EventLoops create their own request pools -- grab it to start creating requests.
        auto& request_pool = event_loop.GetRequestPool();
        
        std::cout
            << "Going to make " << num_requests << " requests to "
            << url << " with each request having a timeout of " << timeout_time.count()
            << std::endl << std::endl;
        
        for (std::size_t count = 0; count < num_requests; ++count)
        {
            lift::RequestHandle request = request_pool.Produce(url, on_complete, curl_timeout_time, timeout_time);
            event_loop.StartRequest(std::move(request));
        }
        while (event_loop.HasUnfinishedRequests())
        {
            std::this_thread::sleep_for(1ms);
        }
    } // EventLoop destructor blocks and handles events until requests are finished
    
    std::cout << "Timeout count " << timeout_count << std::endl;
    std::cout << "Response count " << response_count << std::endl;
    
    return 0;
}
