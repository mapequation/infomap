/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef FLOW_LINK_H_
#define FLOW_LINK_H_

namespace infomap {
namespace detail {

  struct FlowLink {
    unsigned int source;
    unsigned int target;
    double flow;
  };

} // namespace detail
} // namespace infomap

#endif // FLOW_LINK_H_
