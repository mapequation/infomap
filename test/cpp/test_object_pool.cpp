#include "vendor/doctest.h"

#include "core/ObjectPool.h"

#include <vector>

namespace {

using infomap::ObjectPool;

// Probe type: tracks construction/destruction and holds a heap member so
// AddressSanitizer/LeakSanitizer catch any slot whose destructor is skipped.
struct Probe {
  static int liveInstances;
  std::vector<int> payload;
  int id = 0;

  explicit Probe(int id_) : payload(8, id_), id(id_) { ++liveInstances; }
  ~Probe() { --liveInstances; }
};
int Probe::liveInstances = 0;

} // namespace

TEST_CASE("ObjectPool hands out usable, distinct objects")
{
  Probe::liveInstances = 0;
  {
    ObjectPool<Probe> pool;
    Probe* a = pool.alloc(1);
    Probe* b = pool.alloc(2);
    CHECK(a != b);
    CHECK(a->id == 1);
    CHECK(b->id == 2);
    CHECK(pool.liveCount() == 2);
    CHECK(Probe::liveInstances == 2);
    pool.free(a);
    pool.free(b);
    CHECK(pool.liveCount() == 0);
    CHECK(Probe::liveInstances == 0);
  }
  CHECK(Probe::liveInstances == 0);
}

TEST_CASE("ObjectPool addresses stay stable across growth")
{
  Probe::liveInstances = 0;
  ObjectPool<Probe> pool;
  Probe* first = pool.alloc(42);
  std::vector<Probe*> kept;
  kept.reserve(10000);
  for (int i = 0; i < 10000; ++i)
    kept.push_back(pool.alloc(i));
  // Original pointer must still be valid and unchanged after many allocations.
  CHECK(first->id == 42);
  CHECK(first->payload.front() == 42);
  CHECK(pool.chunkCount() > 1);
  pool.free(first);
  for (Probe* p : kept)
    pool.free(p);
  CHECK(pool.liveCount() == 0);
}

TEST_CASE("ObjectPool reuses freed slots")
{
  ObjectPool<Probe> pool;
  Probe* a = pool.alloc(1);
  Probe* original = a;
  pool.free(a);
  Probe* b = pool.alloc(2); // should reuse the just-freed slot
  CHECK(b == original);
  CHECK(b->id == 2);
  CHECK(pool.liveCount() == 1);
  pool.free(b);
}

TEST_CASE("ObjectPool first chunk is small, growth is geometric")
{
  ObjectPool<Probe> pool;
  CHECK(pool.chunkCount() == 0);
  Probe* p = pool.alloc(0); // forces first (small) chunk
  CHECK(pool.chunkCount() == 1);
  pool.free(p); // pool must be empty at teardown (contract enforced by assert)
}
