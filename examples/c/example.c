#include <Infomap.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    struct Infomap* im = NewInfomap("--two-level -v --preferred-number-of-modules 3");

    InfomapAddLink(im, 0, 2, 1.0);
    InfomapAddLink(im, 0, 3, 1.0);
    InfomapAddLink(im, 1, 0, 1.0);
    InfomapAddLink(im, 1, 2, 1.0);
    InfomapAddLink(im, 2, 1, 1.0);
    InfomapAddLink(im, 2, 0, 1.0);
    InfomapAddLink(im, 3, 0, 1.0);
    InfomapAddLink(im, 3, 4, 1.0);
    InfomapAddLink(im, 3, 5, 1.0);
    InfomapAddLink(im, 4, 3, 1.0);
    InfomapAddLink(im, 4, 5, 1.0);
    InfomapAddLink(im, 5, 4, 1.0);
    InfomapAddLink(im, 5, 3, 1.0);

    InfomapRun(im);

    printf("Found %d modules with codelength %f\n", NumModules(im), Codelength(im));

    struct InfomapLeafIterator* it = NewIter(im);

    printf("#node module flow\n");
    while (!IsEnd(it)) {
        printf("%d %d %f\n", NodeId(it), ModuleIndex(it), Flow(it));
        Next(it);
    }

    DestroyIter(it);

    DestroyInfomap(im);
}
