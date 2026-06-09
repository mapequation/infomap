/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_FORMAT_CORE_H_
#define INFOMAP_FORMAT_CORE_H_

// Lightweight {fmt} entry point: the core API only (format_string, string_view,
// make_format_args, formatter). Include this in widely-included headers (e.g.
// utils/Log.h) so they get fmt's typed-argument machinery WITHOUT pulling the
// heavy fmt/format.h into every translation unit; pair it with utils/format.h
// in the .cpp that actually renders. FMT_HEADER_ONLY is pinned identically to
// format.h to avoid ODR mismatches. Never include <fmt/...> directly elsewhere;
// see utils/format.h.

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY 1
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <fmt/core.h> // IWYU pragma: export

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // INFOMAP_FORMAT_CORE_H_
