#include "vendor/doctest.h"

#include "Infomap.h"
#include "utils/Log.h"

#include <iostream>
#include <sstream>

namespace {

struct StreamCapture {
  std::ostringstream out;
  std::ostringstream err;
  std::streambuf* origOut;
  std::streambuf* origErr;

  StreamCapture()
      : origOut(std::cout.rdbuf(out.rdbuf())),
        origErr(std::cerr.rdbuf(err.rdbuf())) { }

  ~StreamCapture()
  {
    std::cout.rdbuf(origOut);
    std::cerr.rdbuf(origErr);
  }
};

void buildAndRun(infomap::InfomapWrapper& im)
{
  im.addLink(0, 1);
  im.addLink(1, 2);
  im.addLink(2, 0);
  im.addLink(3, 4);
  im.addLink(4, 5);
  im.addLink(5, 3);
  im.run();
}

} // namespace

TEST_CASE("Silent mode produces zero stdout/stderr output")
{
  StreamCapture capture;

  {
    infomap::InfomapWrapper im("--silent --num-trials 1 --seed 42");
    buildAndRun(im);
  }

  CHECK(capture.out.str().empty());
  CHECK(capture.err.str().empty());
}

TEST_CASE("setNoOutput produces zero stdout/stderr output")
{
  StreamCapture capture;

  {
    infomap::Log::setNoOutput();
    infomap::InfomapWrapper im("--num-trials 1 --seed 42");
    buildAndRun(im);
  }

  // Restore Log state so subsequent tests are unaffected
  infomap::Log::setOutputStream(std::cout);
  infomap::Log::setSilent(false);

  CHECK(capture.out.str().empty());
  CHECK(capture.err.str().empty());
}
