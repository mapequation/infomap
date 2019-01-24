#include <Infomap.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    struct Infomap* im = NewInfomap("--two-level -N2");

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

    printf("TODO: Show results.\n");

    DestroyInfomap(im);
}
