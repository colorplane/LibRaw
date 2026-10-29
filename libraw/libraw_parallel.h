/* Colorplane's bounded shared-memory row execution for WebAssembly. */
#ifndef LIBRAW_PARALLEL_H
#define LIBRAW_PARALLEL_H

#include <algorithm>
#include <cstddef>
#include <exception>
#include <vector>
#ifdef __EMSCRIPTEN_PTHREADS__
#include <emscripten/threading.h>
#include <thread>
#endif

inline unsigned libraw_parallel_count(unsigned rows, size_t bytes)
{
#ifdef __EMSCRIPTEN_PTHREADS__
  if (bytes >= 256 * 1024 && rows > 1)
    return std::min(rows, unsigned(std::max(1, emscripten_num_logical_cores())));
#endif
  return 1;
}

// Only the owner accesses LibRaw's stateful input/callbacks. The operation must
// read immutable inputs and write exclusively to its assigned output rows.
template <typename Operation>
unsigned libraw_parallel_rows(unsigned rows, size_t bytes, const Operation &operation)
{
  const unsigned count = libraw_parallel_count(rows, bytes);
#ifdef __EMSCRIPTEN_PTHREADS__
  if (count > 1)
  {
    std::vector<std::thread> threads;
    threads.reserve(count - 1);
    std::vector<std::exception_ptr> errors(count);
    auto run = [&](unsigned index) {
      try { operation(size_t(rows) * index / count, size_t(rows) * (index + 1) / count); }
      catch (...) { errors[index] = std::current_exception(); }
    };
    for (unsigned i = 1; i < count; ++i)
    {
      try { threads.emplace_back(run, i); }
      catch (const std::system_error &) { run(i); }
    }
    run(0);
    for (auto &thread : threads) thread.join();
    for (const auto &error : errors) if (error) std::rethrow_exception(error);
    return unsigned(threads.size()) + 1;
  }
#endif
  operation(0, rows);
  return 1;
}
#endif
