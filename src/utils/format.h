/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_FORMAT_H_
#define INFOMAP_FORMAT_H_

// Single entry point for {fmt}. Always include this wrapper instead of
// <fmt/format.h> directly, so FMT_HEADER_ONLY is defined identically on every
// build surface (CLI, Python, R, JS) and we never get an ODR mismatch.
//
// Only the iostream-free formatting core is vendored (vendor/fmt). Use
// fmt::format to build strings, then route them through infomap::Log. Never use
// fmt's print/println family (they write to stdout, bypassing the Log sink and
// breaking the R/Rprintf redirect) and never include fmt/ostream.h. This is
// enforced by scripts/check_cpp_stream_policy.py.

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY 1
#endif

// fmt 10.2.1 instantiates std::char_traits<fmt::detail::char8_type>, which newer
// libc++ marks deprecated. The warning is in fmt's own headers, not our usage,
// and the build is not -Werror; silence it locally so consumers stay clean.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <fmt/format.h> // IWYU pragma: export

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // INFOMAP_FORMAT_H_
