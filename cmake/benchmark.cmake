# benchmark.cmake
# Helper to make linking with Google Benchmark slightly easier.

# disable internal tests in Google Benchmark;
# not really necessary, I just hate having to add the flag
# -DBENCHMARK_ENABLE_TESTING=OFF every time I invoke cmake.
set(BENCHMARK_ENABLE_TESTING OFF)