#include "vendor/doctest.h"

#include "Infomap.h"
#include "utils/Log.h"

#include <iostream>
#include <sstream>

TEST_CASE("Silent mode produces zero stdout/stderr output")
{
  // Capture stdout
  std::ostringstream capturedOut;
  auto* origOutBuf = std::cout.rdbuf(capturedOut.rdbuf());

  // Capture stderr
  std::ostringstream capturedErr;
  auto* origErrBuf = std::cerr.rdbuf(capturedErr.rdbuf());

  {
    infomap::InfomapWrapper im("--silent --num-trials 1 --seed 42");
    im.addLink(0, 1);
    im.addLink(1, 2);
    im.addLink(2, 0);
    im.addLink(3, 4);
    im.addLink(4, 5);
    im.addLink(5, 3);
    im.run();
  }

  // Restore original buffers before any assertions
  std::cout.rdbuf(origOutBuf);
  std::cerr.rdbuf(origErrBuf);

  CHECK(capturedOut.str().empty());
  CHECK(capturedErr.str().empty());
}

TEST_CASE("setNoOutput produces zero stdout/stderr output")
{
  // Capture stdout
  std::ostringstream capturedOut;
  auto* origOutBuf = std::cout.rdbuf(capturedOut.rdbuf());

  // Capture stderr
  std::ostringstream capturedErr;
  auto* origErrBuf = std::cerr.rdbuf(capturedErr.rdbuf());

  {
    infomap::Log::setNoOutput();
    infomap::InfomapWrapper im("--num-trials 1 --seed 42");
    im.addLink(0, 1);
    im.addLink(1, 2);
    im.addLink(2, 0);
    im.addLink(3, 4);
    im.addLink(4, 5);
    im.addLink(5, 3);
    im.run();
  }

  // Restore original buffers and Log state before assertions
  std::cout.rdbuf(origOutBuf);
  std::cerr.rdbuf(origErrBuf);
  infomap::Log::setOutputStream(std::cout);

  CHECK(capturedOut.str().empty());
  CHECK(capturedErr.str().empty());
}
