#include "vendor/doctest.h"

#include "io/SafeFile.h"
#include "utils/FileURI.h"
#include "utils/Log.h"
#include "utils/Console.h"
#include "utils/Random.h"
#include "utils/convert.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {

using infomap::FileURI;

int createDirectory(const char* path)
{
#ifdef _WIN32
  return _mkdir(path);
#else
  return mkdir(path, 0700);
#endif
}

int removeDirectory(const char* path)
{
#ifdef _WIN32
  return _rmdir(path);
#else
  return rmdir(path);
#endif
}

std::string tempDirectoryRoot()
{
  for (const auto* name : { "TMPDIR", "TEMP", "TMP" }) {
    const auto* value = std::getenv(name);
    if (value != nullptr && value[0] != '\0')
      return value;
  }
#ifdef _WIN32
  return ".";
#else
  return "/tmp";
#endif
}

std::string joinPath(const std::string& directory, const std::string& filename)
{
#ifdef _WIN32
  constexpr char separator = '\\';
#else
  constexpr char separator = '/';
#endif
  if (!directory.empty() && (directory.back() == '/' || directory.back() == '\\'))
    return directory + filename;
  return directory + separator + filename;
}

class TempDir {
public:
  TempDir()
  {
    const auto root = tempDirectoryRoot();
    const auto timestamp = static_cast<unsigned long long>(std::time(nullptr));
    const auto clockTicks = static_cast<unsigned long long>(std::clock());
    int lastError = 0;

    for (unsigned int attempt = 0; attempt < 128; ++attempt) {
      const auto candidate = joinPath(root,
          "infomap-tests-" + infomap::io::stringify(timestamp) + "-" +
              infomap::io::stringify(clockTicks) + "-" + infomap::io::stringify(attempt));
      errno = 0;
      if (createDirectory(candidate.c_str()) == 0) {
        m_path = candidate;
        return;
      }
      lastError = errno;
      if (lastError != EEXIST)
        break;
    }

    REQUIRE_MESSAGE(false, std::strerror(lastError));
  }

  ~TempDir()
  {
    removeDirectory(m_path.c_str());
  }

  const std::string& path() const { return m_path; }

private:
  std::string m_path;
};

TEST_CASE("convert parses unsigned integer values strictly [fast][core][utils][io]")
{
  unsigned int value = 0;
  CHECK(infomap::io::stringToValue("42", value));
  CHECK(value == 42u);

  CHECK_FALSE(infomap::io::stringToValue("", value));
  CHECK_FALSE(infomap::io::stringToValue("-1", value));
  CHECK_FALSE(infomap::io::stringToValue("12abc", value));
  CHECK_FALSE(infomap::io::stringToValue("4294967296", value));
}

TEST_CASE("convert parses unsigned long values strictly [fast][core][utils][io]")
{
  unsigned long value = 0;
  CHECK(infomap::io::stringToValue("42", value));
  CHECK(value == 42ul);

  CHECK_FALSE(infomap::io::stringToValue("", value));
  CHECK_FALSE(infomap::io::stringToValue("-1", value));
  CHECK_FALSE(infomap::io::stringToValue("12abc", value));

  const auto overflow = infomap::io::stringify(std::numeric_limits<unsigned long long>::max()) + "0";
  CHECK_FALSE(infomap::io::stringToValue(overflow, value));
}

TEST_CASE("convert helpers preserve current tokenization behavior [fast][core][utils][io]")
{
  const auto parts = infomap::io::split("alpha,,beta,gamma", ',');
  CHECK(parts == std::vector<std::string> { "alpha", "beta", "gamma" });
  CHECK(infomap::io::firstWord("  one two three") == "one");
  CHECK(infomap::io::stringify(parts, "|") == "alpha|beta|gamma");
  CHECK(infomap::io::tolower("MiXeD") == "mixed");
  CHECK(infomap::io::InsensitiveCompare {}("abc", "ABD"));
}

TEST_CASE("Console formats plain output without ANSI and clamps tiny percentages [fast][core][utils][io]")
{
  std::ostringstream output;
  infomap::Log::setOutputStream(output);
  infomap::Log::init(0, false, 9);

#ifndef _WIN32
  setenv("NO_COLOR", "1", 1);
#endif

  infomap::Console console;
  console.section("Flow");
  console.metric("Model", "directed");
  console.status("Recursive", infomap::Console::percent(-1.0e-12));

  CHECK(output.str().find("\033[") == std::string::npos);
  CHECK(output.str().find("Flow") != std::string::npos);
  CHECK(output.str().find("0%") != std::string::npos);
  CHECK(infomap::Console::percent(-1.55696863e-14) == "0%");

#ifndef _WIN32
  unsetenv("NO_COLOR");
#endif
  infomap::Log::setOutputStream(std::cout);
  infomap::Log::init(0, false, 9);
}

TEST_CASE("Log output is level-gated and respects verbosity and silent [fast][core][utils][io]")
{
  std::ostringstream output;
  infomap::Log::setOutputStream(output);

  // Verbosity 0: level-0 lines show, level>=1 (verbose) lines are hidden.
  infomap::Log::init(0, false, 9);
  infomap::Log() << "base";
  infomap::Log(1) << "verbose";
  infomap::Log() << "important";
  CHECK(output.str().find("base") != std::string::npos);
  CHECK(output.str().find("verbose") == std::string::npos);
  CHECK(output.str().find("important") != std::string::npos);

  // Verbosity 1: level-1 diagnostics now layer on top additively.
  output.str("");
  output.clear();
  infomap::Log::init(1, false, 9);
  infomap::Log(1) << "verbose";
  CHECK(output.str().find("verbose") != std::string::npos);

  // Silent: nothing is emitted on any channel.
  output.str("");
  output.clear();
  infomap::Log::init(0, true, 9);
  infomap::Log() << "base";
  infomap::Log() << "important";
  CHECK(output.str().empty());

  infomap::Log::setOutputStream(std::cout);
  infomap::Log::init(0, false, 9);
}

TEST_CASE("PortableRandom maps integers portably [fast][core][utils]")
{
  infomap::PortableRandom random(2501);
  CHECK(random.randInt(0, 10) == 9u);
  CHECK(random.randInt(1, 3) == 3u);
  CHECK(random.randInt(7, 19) == 14u);
  CHECK(random.randInt(0, 1) == 1u);
  CHECK(random.randInt(42, 42) == 42u);
  CHECK(random.randInt(0, 999) == 695u);
  CHECK(random.randInt(0, std::numeric_limits<unsigned int>::max()) == 3859750371u);
  CHECK(random.randInt(100, 100000) == 3169u);
}

TEST_CASE("PortableRandom index vectors are portable [fast][core][utils]")
{
  infomap::PortableRandom random(2501);
  std::vector<unsigned int> order(16);
  random.getRandomizedIndexVector(order);

  CHECK(order == std::vector<unsigned int> { 9, 0, 5, 14, 7, 15, 4, 12, 3, 8, 10, 11, 13, 2, 1, 6 });
}

TEST_CASE("Random keeps the standard library distribution by default [fast][core][utils]")
{
  infomap::Random random(2501);
  CHECK(random.randInt(42, 42) == 42u);

  for (unsigned int i = 0; i < 32; ++i) {
    const auto value = random.randInt(7, 19);
    CHECK(value >= 7u);
    CHECK(value <= 19u);
  }
}

TEST_CASE("FileURI splits valid paths and rejects invalid ones [fast][core][utils][io]")
{
  const FileURI nested("dir/subdir/file.net");
  CHECK(nested.getFilename() == "dir/subdir/file.net");
  CHECK(nested.getName() == "file");
  CHECK(nested.getExtension() == "net");

  const FileURI noExt("plain-file");
  CHECK(noExt.getName() == "plain-file");
  CHECK(noExt.getExtension().empty());

  CHECK_THROWS_AS(FileURI("dir/"), std::invalid_argument);
  CHECK_THROWS_AS(FileURI("file.", true), std::invalid_argument);
}

TEST_CASE("SafeFile detects writable and missing directories [fast][core][utils][io]")
{
  const TempDir tempDir;
  CHECK(infomap::isDirectoryWritable(tempDir.path() + "/"));
  CHECK_FALSE(infomap::isDirectoryWritable(tempDir.path() + "/missing/"));
}

TEST_CASE("SafeOutFile commits atomically and overwrites by default [fast][core][utils][io]")
{
  const TempDir tempDir;
  const auto path = joinPath(tempDir.path(), "result.txt");

  {
    infomap::SafeOutFile initial(path);
    initial << "old\n";
    initial.commit();
  }

  {
    infomap::SafeOutFile replacement(path);
    replacement << "new\n";
    replacement.commit();
  }

  std::ifstream input(path.c_str());
  std::ostringstream contents;
  contents << input.rdbuf();
  CHECK(contents.str() == "new\n");
}

TEST_CASE("SafeOutFile no-overwrite preserves existing targets [fast][core][utils][io]")
{
  const TempDir tempDir;
  const auto path = joinPath(tempDir.path(), "result.txt");

  {
    infomap::SafeOutFile initial(path);
    initial << "old\n";
    initial.commit();
  }

  const auto expectedMessage = "Output file already exists: '" + path + "'";
  CHECK_THROWS_WITH_AS(
      infomap::SafeOutFile(path, std::ios_base::out, false),
      expectedMessage.c_str(),
      std::runtime_error);

  std::ifstream input(path.c_str());
  std::ostringstream contents;
  contents << input.rdbuf();
  CHECK(contents.str() == "old\n");
}

TEST_CASE("SafeOutFile removes uncommitted temp files [fast][core][utils][io]")
{
  const TempDir tempDir;
  const auto path = joinPath(tempDir.path(), "result.txt");

  {
    infomap::SafeOutFile output(path);
    output << "partial\n";
  }

  std::ifstream input(path.c_str());
  CHECK_FALSE(input.good());
}

TEST_CASE("ensureDirectoryExists creates a deep directory chain [fast][core][utils][io]")
{
  const TempDir tempDir;
  const auto leaf = joinPath(joinPath(joinPath(tempDir.path(), "a"), "b"), "c");

  infomap::ensureDirectoryExists(leaf);

  CHECK(infomap::isDirectory(leaf));
  // Idempotent: a second call on an existing chain is a no-op, not an error.
  CHECK_NOTHROW(infomap::ensureDirectoryExists(leaf));
}

// Characterization test locking the byte-for-byte equivalence claim in
// convert.cpp: io::toPrecision now formats with {fmt} ({:.{}g} / {:.{}f})
// instead of std::ostringstream. This reference reproduces the *previous*
// iostream implementation verbatim; the matrix below asserts fmt reproduces it
// exactly so a future fmt bump or refactor can't silently shift Infomap's
// numeric output. (Both run under the default "C" locale here, matching the
// CLI, so locale divergence is out of scope.)
std::string toPrecisionLegacy(double value, unsigned int precision, bool fixed)
{
  std::ostringstream o;
  if (fixed)
    o << std::fixed << std::setprecision(static_cast<int>(precision));
  else
    o << std::setprecision(static_cast<int>(precision));
  o << value;
  return o.str();
}

TEST_CASE("toPrecision reproduces the legacy iostream output byte-for-byte [fast][core][utils][io]")
{
  const std::vector<double> values = {
    0.0,
    -0.0,
    1.0,
    -1.0,
    0.5,
    1.0 / 3.0,
    2.0 / 3.0,
    3.14159265358979,
    0.1,
    0.125,
    0.15, // classic banker's-rounding edge
    2.675, // classic float-representation rounding edge
    9.9999999999,
    1234.5678,
    123456789.123456789,
    -42.0,
    1e-9,
    1e9,
    1e-300,
    1e300,
    std::numeric_limits<double>::min(), // smallest normal
    std::numeric_limits<double>::denorm_min(), // subnormal
    std::numeric_limits<double>::max(),
  };
  const std::vector<unsigned int> precisions = { 0, 1, 2, 3, 6, 9, 10, 15, 17 };

  for (const double value : values) {
    for (const unsigned int precision : precisions) {
      for (const bool fixed : { false, true }) {
        const std::string expected = toPrecisionLegacy(value, precision, fixed);
        const std::string actual = infomap::io::toPrecision(value, precision, fixed);
        INFO("value=" << value << " precision=" << precision << " fixed=" << fixed);
        CHECK(actual == expected);
      }
    }
  }
}

} // namespace
