// manual.cpp
// Manual benchmark driver.
//
// Adapated from original code by Gor Nishanov.
// https://github.com/GorNishanov/await/tree/master/2018_CppCon

#include "rng.hpp"
#include "vanilla.hpp"
#include "coroutine.hpp"
#include "state_machine.hpp"

#include <ratio>
#include <chrono>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <string_view>

struct State 
{
    using hrc_clock = std::chrono::high_resolution_clock;

    // The dataset in which lookups will be performed.
    std::vector<int> dataset;

    // The queries into the dataset.
    std::vector<int> lookups;

    // The size of the dataset, in items (ints).
    size_t count;

    // The number of times to repeat the multi-lookup under current parameters.
    size_t n_repeat;

    // The number of streams to use in multi-lookup.
    size_t n_streams;

    // The name of the algorithm used to perform multi-lookup.
    char const* algo_name;

    // The start time for an instance of a test run.
    hrc_clock::time_point start_time;

    State(
        char const* algo, 
        size_t byte_count, 
        size_t lookup_count, 
        size_t n_repeat_) 
        : n_repeat{n_repeat_} 
    {
        // seed for RNG
        auto const seed = 0;

        algo_name = algo;

        // the number of items in the dataset vector
        count = (byte_count / sizeof(int));

        dataset.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            dataset.push_back(i + i);
        }

        lookups.reserve(lookup_count);
        for (auto i : rng<int>(seed, 0, count + count, lookup_count))
        {
            lookups.push_back(i);
        }
    }

    // initiate a test run
    void start(size_t streams_, const char* algo_name_) 
    {
        n_streams  = streams_;
        algo_name  = algo_name_;
        start_time = hrc_clock::now();
    }

    // complete a test run
    void stop() 
    {
        auto const stop_time = hrc_clock::now();
        std::chrono::duration<double, std::nano> elapsed = (stop_time - start_time);

        auto divby = log2((double)dataset.size());
        auto perop = elapsed.count() / divby / lookups.size() / n_repeat;

        printf("[+] Test complete: %g ns per lookup/log2(size)\n", perop);
    }

    void print_config() const noexcept
    {
        printf("[+] Test configuration:\n[+]\tAlgorithm: %s Count: %zu Lookups: %zu Repeat %zu\n", 
            algo_name,
            count, 
            lookups.size(), 
            n_repeat);
    }
};

using test_fn_f = size_t (*)(State& s);

struct TestParams
{
    size_t size_in_bytes;
    size_t n_lookups;
    size_t n_repeat;
};

struct TestConfig
{
    TestConfig() 
        : valid{false}
        , params{}
        , n_streams{0}
        , algo_name{nullptr}
        , runner{nullptr} 
    {}

    // Is this a valid test configuration?
    bool valid;

    // The parameters for the test.
    TestParams params;

    // The number of streams to utilize in the test.
    size_t n_streams;

    // The name of the algorithm in use.
    char const* algo_name;

    // The test runner function to invoke.
    test_fn_f runner;
};

static size_t test_vanilla(State& s);
static size_t test_state_machine(State& s);
static size_t test_coro(State& s);

static TestConfig parse_args(int argc, char* argv[]);
static void usage(const char* msg = nullptr);

int main(int argc, char* argv[]) 
{
    auto config = parse_args(argc, argv);
    if (!config.valid)
    {
        return EXIT_FAILURE;
    }

    State state{
        config.algo_name,
        config.params.size_in_bytes, 
        config.params.n_lookups, 
        config.params.n_repeat};

    state.print_config();

    state.start(config.n_streams, config.algo_name);

    size_t successful_lookups = 0;
    for (size_t i = 0; i < state.n_repeat; ++i) 
    {
        successful_lookups += (*config.runner)(state);
    }

    state.stop();

    printf("[+] Total successful lookups: %zu\n", successful_lookups);

    return EXIT_SUCCESS;
}

// a vanilla binary search, without multi-lookup support
static size_t test_vanilla(State& s) 
{
    size_t found = 0;
    
    auto const beg = s.dataset.begin();
    auto const end = s.dataset.end();

    for (int key : s.lookups)
    {
        if (vanilla_binary_search(beg, end, key))
        {
            ++found;
        }
    }

    return found;
}

// a multi-lookup test implemented via hand-crafted state machine
static size_t test_state_machine(State& s) 
{
    return state_machine_multi_lookup(s.dataset, s.lookups, s.n_streams);
}

// a multi-lookup test implemented via coroutines
static size_t test_coro(State& s)
{
    return coro_multi_lookup(s.dataset, s.lookups, s.n_streams);
}

static TestConfig parse_args(int argc, char* argv[])
{
    using namespace std;

    TestConfig config{};

    if (argc != 4)
    {
        usage();
        return config;
    }

    if (argv[1] == "vanilla"sv)
    {
        config.algo_name = "vanilla";
        config.runner    = &test_vanilla;
    }
    else if (argv[1] == "sm"sv)
    {   
        config.algo_name = "state machine";
        config.runner    = &test_state_machine;
    }
    else if (argv[1] == "coro"sv)
    {
        config.algo_name = "coroutine";
        config.runner    = &test_coro;
    } 
    else 
    {
        usage("invalid algorithm name");
        return config;
    }

    if (argv[2] == "quick"sv)
    {
        config.params = TestParams{ 16*1024, 1024, 1};
    }
    else if (argv[2] == "l1"sv)
    {
        config.params = TestParams{16*1024, 1024, 10000};
    }
    else if (argv[2] == "l2"sv)
    {
          config.params = TestParams{200*1024, 1024*1024, 50};
    }
    else if (argv[2] == "l3"sv) 
    {
        config.params = TestParams{6*1024*1024, 1024*1024, 50};
    }
    else if (argv[2] == "big"sv) 
    {
        config.params = TestParams{256*1024*1024, 1024*1024, 5};
    }
    else 
    {
        usage("invalid size");
        return config;
    }

    auto const n_streams = atoi(argv[3]);
    if (n_streams < 1)
    {
        usage("invalid stream count");
        return config;
    }

    config.n_streams = n_streams;
    config.valid     = true;

    return config;
}

static void usage(char const* msg) 
{
    if (msg)
    {
        printf("[-] %s\n", msg);
    }

    printf("[-] driver <ALGO> <SIZE> <N_STREAMS>\n"
           "\t<ALGO>:      vanilla sm coro\n"
           "\t<SIZE>:      quick l1 l2 l3 big\n"
           "\t<N_STREAMS>: 1 - n\n\n");
}