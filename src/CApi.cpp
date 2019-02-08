#include "Infomap.h"

namespace infomap {

    Infomap *NewInfomap(const char *flags) { return new Infomap(flags); };

    void DestroyInfomap(Infomap *im) { im->~Infomap(); };

    void InfomapAddLink(Infomap *im, unsigned int sourceId, unsigned int targetId, double weight) {
        im->addLink(sourceId, targetId, weight);
    };

    void InfomapRun(struct Infomap* im) { im->run(); };

}