#include "Infomap.h"

namespace infomap {

    Infomap *NewInfomap(const char *flags) { return new Infomap(flags); };

    void DestroyInfomap(Infomap *im) { im->~Infomap(); };

    void InfomapAddLink(Infomap *im, unsigned int sourceId, unsigned int targetId, double weight) {
        im->addLink(sourceId, targetId, weight);
    };

    void InfomapRun(struct Infomap *im) { im->run(); };

    double Codelength(struct Infomap *im) { return im->tree.codelength(); }

    unsigned int NumModules(struct Infomap *im) { return im->tree.numTopModules(); }

    struct LeafIterator *NewIter(struct Infomap *im) { return new LeafIterator(&(im->tree.getRootNode())); }

    void DestroyIter(struct LeafIterator *it) { it->~LeafIterator(); }

    bool IsEnd(struct LeafIterator *it) { return it->isEnd(); }

    void Next(struct LeafIterator *it) { it->stepForward(); }

    unsigned int Depth(struct LeafIterator *it) { return it->base()->originalLeafIndex; }

    unsigned int NodeIndex(struct LeafIterator *it) { return it->base()->originalLeafIndex; }

    unsigned int ModuleIndex(struct LeafIterator *it) { return it->moduleIndex(); }

    double Flow(struct LeafIterator *it) { return it->base()->data.flow; }
}
