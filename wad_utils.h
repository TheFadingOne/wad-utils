#ifndef WAD_UTILS_H
#define WAD_UTILS_H

#include <inttypes.h>
#include <stdio.h>


int loadWad(FILE *file, struct WAD *wad);
int writeWad(FILE *file, struct WAD *wad);
void freeWad(struct WAD *wad);

#endif
