liblifthttp - Safe Easy to use C++17 HTTP client library.
=========================================================

[![CircleCI](https://circleci.com/gh/jbaldwin/liblifthttp/tree/master.svg?style=svg)](https://circleci.com/gh/jbaldwin/liblifthttp/tree/master)

You're using curl? Do you even lift?

Copyright (c) 2017-2019, Josh Baldwin

https://github.com/jbaldwin/liblifthttp

**liblifthttp** is an HTTP C++17 client library that provides an easy to use API for both synchronous _and_ asynchronous requests.  It is built upon the rock solid libcurl and libuv libraries.

**liblifthttp** is licensed under the Apache 2.0 license.

# Overview #
* Easy to use Synchronous and Asynchronous HTTP Request APIs.
* Safe C++17 Client library API, modern memory move semantics.
* Optional background IO thread(s) for sending and receiving Async HTTP requests.
* Request pooling for re-using HTTP requests.

# Usage #

## Requirements
    C++17 compiler (g++/clang++)
    CMake
    make and/or ninja
    pthreads/std::thread
    libcurl devel
    libuv devel

## Instructions

### Building
    # This will produce a static library to link against your project.
    mkdir Release && cd Release
    cmake -DCMAKE_BUILD_TYPE=Release ..
    cmake --build .

### CMake Projects
To use within your cmake project you can checkout the code locally and then use
`add_subdirectory(liblifthttp)` to include the project.  

Note that by default liblifthttp will attempt to use system versions of `libcurl` 
and `libuv`.  If your project, like some of mine do, require a custom built version 
of `libcurl` then you can specify the following cmake variables to override where liblifthttp
will link `libcurl` development libraries: (custom libuv not supported yet)

    ${CURL_INCLUDE} # The curl.h header location, default is empty.
    ${LIBSSL}       # The ssl library to link against, default is empty.
    ${LIBCRYPTO}    # The crypto library to link against, default is empty.
    ${LIBCURL}      # The curl library to link against, default is '-lcurl'.
    ${LIBCARES}     # The cares (dns) library to link against, default is empty.

## Examples

See all of the examples under the examples/ directory.  Below are some simple examples
to get your started on using liblifthttp with both the synchronous and asynchronous APIs.

```C++
// libcurl requires some global functions to be called before being used.
// LibLiftHttp will call these appropriately if you place the following in
// the projects main.cpp file(s) where necessary.
static lift::GlobalScopeInitializer g_lifthttp_gsi{};
```

### Simple Synchronous
```C++
// Requests are always produced from a pool and return to the pool upon completion.
lift::RequestPool pool{};
auto request = pool.Produce("http://www.example.com");

request->Perform();  // This call is the blocking synchronous HTTP call.

std::cout << request->GetResponseData() << "\n";
```

### Simple Asynchronous
```C++
// Event loops in Lift come with their own RequestPool, no need to provide one.
// Creating the event loop starts it immediately, it spawns a background thread for executing requests.
lift::EventLoop loop{};
auto& pool = loop.GetRequestPool();

// Create the request just like we did in the sync version, but now provide a lambda for on completion.
// NOTE: that the Lambda is executed ON the Lift event loop background thread.  If you want to handle 
// on completion processing on this main thread you need to std::move it back via a queue or inter-thread 
// communication.  This is imporant if any resources are shared between the threads.
auto request = pool.Produce(
    "http://www.example.com",
    [](lift::RequestHandle r) { std::cout << r->GetResponseData(); }, // on destruct 'r' will return to the pool.
    10s, // Give the request 10 seconds to complete or timeout.
);

// Now inject the request into the event to be executed.  Moving into the event loop is required,
// this passes ownership of the request to the event loop background worker thread.
loop.StartRequest(std::move(request));

// Block on this main thread until the lift event loop background thread has completed the request, or timed out.
while(loop.GetActiveRequestCount() > 0) {
    std::this_thread::sleep_for(10ms);
}

// When loop goes out of scope here it will automatically stop the background thread and cleanup all resources.
```

## Benchmarks

WIP

## Testing

This project has a simple [CircleCI](https://circleci.com/) implementation to compile and run tests
against Ubuntu g++ and clang++.  More distros might be added in the future.

Any patchests or features added should include relevant tests to increase the coverage of the library.
Examples are also welcome if they are interesting or more difficult to understand how to use the feature.

CMake is setup to understand how to run the tests.  A simple build and then running `ctest` will
execute the tests locally.  Note that the integration tests that make HTTP calls require a webserver
on http://localhost:80/ that will respond with a 200 on the root directory and 404 on any other url.
A future iteration might include an embedded server that responds with more sophisticated tests.

```bash
apt-get install nginx
systemctl start nginx
mkdir Release && cd Release
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
ctest -V
```

## Support

File bug reports, feature requests and questions using [GitHub Issues](https://github.com/jbaldwin/liblifthttp/issues)
