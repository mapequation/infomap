/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_INFOMAPC_H
#define INFOMAP_INFOMAPC_H

#ifdef __cplusplus

#include "Infomap.h"

namespace infomap {

extern "C" {
#else
#include <stdint.h>
#include <stdbool.h>
struct Infomap;
struct InfomapLeafIterator;
#endif

#ifndef SWIG

////////////////////////////////
// Implementation of c bindings
////////////////////////////////

InfomapWrapper* NewInfomap(const char* flags)
{
  return new InfomapWrapper(flags);
}

void DestroyInfomap(InfomapWrapper* im) { im->~InfomapWrapper(); }

void InfomapAddLink(InfomapWrapper* im, unsigned int sourceId, unsigned int targetId, double weight)
{
  im->addLink(sourceId, targetId, weight);
}

void InfomapRun(struct InfomapWrapper* im) { im->run(); }

double Codelength(struct InfomapWrapper* im) { return im->getHierarchicalCodelength(); }

unsigned int NumModules(struct InfomapWrapper* im) { return im->numTopModules(); }

struct InfomapLeafIterator* NewIter(struct InfomapWrapper* im) { return new InfomapLeafIterator(&(im->root())); }

void DestroyIter(struct InfomapLeafIterator* it) { it->~InfomapLeafIterator(); }

bool IsEnd(struct InfomapLeafIterator* it) { return it->isEnd(); }

void Next(struct InfomapLeafIterator* it) { it->stepForward(); }

unsigned int Depth(struct InfomapLeafIterator* it) { return it->depth(); }

unsigned int NodeId(struct InfomapLeafIterator* it) { return it->current()->id(); }

unsigned int ModuleIndex(struct InfomapLeafIterator* it) { return it->moduleIndex(); }

double Flow(struct InfomapLeafIterator* it) { return it->current()->data.flow; }

#endif // SWIG

#ifdef __cplusplus
}; // extern "C"
} // namespace infomap
#endif

#endif // INFOMAP_INFOMAPC_H
