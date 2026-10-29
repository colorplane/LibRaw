#include "libraw/libraw_parallel.h"
#include <cassert>
#include <cstdio>
#include <set>
#include <stdexcept>

int main()
{
  const unsigned rows = 97;
  std::vector<unsigned> visits(rows);
  std::vector<std::thread::id> owners(rows);
  const unsigned used = libraw_parallel_rows(rows, 1024 * 1024,
    [&](unsigned first, unsigned last) {
      for (unsigned i = first; i < last; ++i) {
        ++visits[i];
        owners[i] = std::this_thread::get_id();
      }
    });
  for (unsigned n : visits) assert(n == 1);
  const std::set<std::thread::id> distinct(owners.begin(), owners.end());
  assert(used == libraw_parallel_count(rows, 1024 * 1024));
  assert(distinct.size() == used);
  bool caught = false;
  try {
    libraw_parallel_rows(rows, 1024 * 1024, [&](unsigned first, unsigned) {
      if (first == 0) throw std::runtime_error("row failure");
    });
  } catch (const std::runtime_error &) { caught = true; }
  assert(caught);
  assert(libraw_parallel_rows(3, 6, [](unsigned first, unsigned last) {
    assert(first == 0 && last == 3);
  }) == 1);
  std::printf("Parallel rows: %u distinct threads, exact coverage, joined exceptions, small-input fallback passed.\n", used);
}
