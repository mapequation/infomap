#define DOCTEST_CONFIG_IMPLEMENT
#include "vendor/doctest.h"

#include <cfenv>
#include <cstdint>

#if defined(__APPLE__) && defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace {

// Trap invalid operations (a NaN from 0/0, inf - inf, log(-x), ...) and
// division by zero, so the test fails where it happens instead of carrying a
// wrong codelength forward. Test runner only, with -DINFOMAP_FP_TRAPS=ON; the
// CLI does not trap. The trap is per thread: OpenMP workers inherit it on glibc,
// not on macOS. Other platforms (Windows, other libcs) run without traps.
void enableFloatingPointTraps()
{
#if !defined(INFOMAP_TEST_FP_TRAPS)
#elif defined(__GLIBC__)
  feenableexcept(FE_INVALID | FE_DIVBYZERO);
#elif defined(__APPLE__) && defined(__aarch64__)
  // No feenableexcept on macOS. FPCR bits 8 (IOE) and 9 (DZE); a hit raises SIGILL.
  std::uint64_t fpcr = 0;
  __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
  fpcr |= (1U << 8) | (1U << 9);
  __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__APPLE__) && defined(__x86_64__)
  _mm_setcsr(_mm_getcsr() & ~(_MM_MASK_INVALID | _MM_MASK_DIV_ZERO));
#endif
}

} // namespace

int main(int argc, char** argv)
{
  enableFloatingPointTraps();
  return doctest::Context(argc, argv).run();
}
