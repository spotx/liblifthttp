#include <lift/Lift.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>

static uint64_t timeout_count{0};
static uint64_t response_count{0};

static auto on_complete(lift::RequestHandle request) -> void
{
    if (request->GetCompletionStatus() == lift::RequestStatus::SUCCESS) {
        ++response_count;
        std::cout << "Completed in " << request->GetTotalTime().count() << " ms" << std::endl;
    } else {
        ++timeout_count;
        std::cout << "Error in request: " << lift::to_string(request->GetCompletionStatus()) << std::endl;
    }
}

int main(int argc, char* argv[])
{
    {
        if (argc < 4)
        {
            std::cout
                << "Please provide URL as the first argument, "
                << "millisecond request timeout as the second argument, and"
                << "the number of requests to send as the third argument."
                << std::endl;
            
            return 0;
        }
        
        using namespace std::chrono_literals;
        
        // Initialize must be called first before using the LiftHttp library.
        lift::GlobalScopeInitializer lift_init{};
        
        std::string url{argv[1]};
        std::chrono::milliseconds timeout_time{std::stoi(argv[2])};
        std::size_t num_requests{static_cast<std::size_t>(std::stoi(argv[3]))};
        
        std::cout
            << "Going to make " << num_requests << " requests to "
            << url << " with each request having a timeout of " << timeout_time.count()
            << std::endl << std::endl;
        
        lift::EventLoop event_loop;
        // EventLoops create their own request pools -- grab it to start creating requests.
        auto& request_pool = event_loop.GetRequestPool();
        
        for (std::size_t count = 0; count < num_requests; ++count)
        {
            lift::RequestHandle request = request_pool.Produce(url, on_complete, 2'000ms);
            //request->SetRequestTimeout(timeout_time);
            event_loop.StartRequest(std::move(request));
        }
    }
    
    using namespace std::chrono_literals;
    std::cout << "Timeout count " << timeout_count << std::endl;
    std::cout << "Response count " << response_count << std::endl;
    
    return 0;
}
