#include <assert.h>
#include <getopt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "wad_types.h"
#include "wad_utils.h"

char *progname = NULL;
_Bool removeEnemies = 0;
_Bool verbose = 0;

const char shortopts[] = "hi:o:rv";
const struct option longopts[] = {
    { "help", no_argument, NULL, 'h' },
    { "input-file", required_argument, NULL, 'i' },
    { "output-file", required_argument, NULL, 'o' },
    { "remove-multiplayer-enemies", no_argument, NULL, 'r' },
    { "verbose", no_argument, NULL, 'v' },
    { 0, 0, 0, 0 }
};

void printUsage(const char *progname)
{
    printf("usage: %s [OPTIONS]\n", progname);
    printf("OPTIONS:\n");
    printf("  -h, --help                        Prints this help text\n");
    printf("  -i, --input-file  [FILE]          WAD file to read from\n");
    printf("  -o, --output-file [FILE]          WAD file to write to\n");
    printf("  -r, --remove-multiplayer-enemies  Enables removal of multiplayer enemies\n");
    printf("  -v, --verbose                     Enable verbose output\n");
}

int main(int argc, char *argv[])
{
    progname = argv[0];
    if (argc < 2) {
        printUsage(progname);
        return 1;
    }

    const char *ifname = NULL;
    const char *ofname = NULL;

    int c;
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
        case 'i':
            ifname = optarg;
            break;
        case 'o':
            ofname = optarg;
            break;
        case 'r':
            removeEnemies = 1;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'h':
            printUsage(progname);
            return 0;
        default:
            printUsage(progname);
            return 1;
        }
    }

    if (ifname == NULL) {
        fprintf(stderr, "[%s] error: input file needs to specified\n", progname);
        printUsage(progname);
        return 1;
    }

    if (removeEnemies == 1 && ofname == NULL) {
        fprintf(stderr, "[%s] error: to remove enemies an output file has to be specified\n", progname);
        printUsage(progname);
        return 1;
    }

    if (removeEnemies == 0 && ofname != NULL) {
        fprintf(stderr, "[%s] warning: output file ignored when not removing enemies\n", progname);
    }

    FILE *iFile = fopen(ifname, "r");
    if (!iFile) {
        fprintf(stderr, "[%s] error: unable to open \"%s\"\n", progname, ifname);
        return 1;
    }

    struct WAD wad;
    int res = loadWad(iFile, &wad);
    if (res != 0) {
        fprintf(stderr, "[%s] error: unable to load wad\n", progname);
        fclose(iFile);
        // WAD should not be freed in this case
        return res;
    }
    fclose(iFile);

    if (ofname != NULL) {
        FILE *oFile = fopen(ofname, "w");
        if (!oFile) {
            fprintf(stderr, "[%s] error: unable to open \"%s\"\n", progname, ofname);
            res = 1;
            goto cleanup;
        }

        res = writeWad(oFile, &wad);
        if (res != 0) {
            fprintf(stderr, "[%s] error: unable to write modified WAD\n", progname);
        }

        fclose(oFile);
    }

cleanup:
    freeWad(&wad);
    return res;
}
