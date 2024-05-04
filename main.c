#include <assert.h>
#include <getopt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct __attribute__((packed)) WadInfo {
    int32_t identification;
    int32_t numlumps;
    int32_t infotableofs;
};

struct __attribute__((packed)) FileLump {
    int32_t filepos;
    int32_t size;
    char name[8];
};

struct __attribute__((packed)) Thing {
    int16_t x;
    int16_t y;
    int16_t angle;
    int16_t type;
    int16_t flags;
};

static_assert(sizeof(struct WadInfo) == 12, "WadInfo wrong size");
static_assert(sizeof(struct FileLump) == 16, "FileLump wrong size");
static_assert(sizeof(struct Thing) == 10, "Thing wrong size");

int16_t monsterTypes[19] = {
    0x007, // Spiderdemon
    0x009, // Shotgun Guy
    0x010, // Cyberdemon
    0x03A, // Spectre
    0x040, // Archie
    0x041, // Chaingunner
    0x042, // Rev
    0x043, // Manc
    0x044, // Arachnotron
    0x045, // Hell Knight
    0x047, // Pain Elemental
    0x048, // Keen
    0x054, // SS
    0xBB9, // Imp
    0xBBA, // Pinky
    0xBBB, // Baron
    0xBBC, // Zombie
    0XBBD, // Caco
    0XBBE, // Lost Soul
};

char *progname = NULL;
_Bool removeEnemies = 0;

int isMonster(int16_t type)
{
    for (size_t i = 0; i < sizeof(monsterTypes) / sizeof(monsterTypes[0]); ++i) {
        if (type == monsterTypes[i]) {
            return 1;
        }
    }
    return 0;
}

enum FLAGS {
    FLAG_SKILL_EASY,
    FLAG_SKILL_MEDIUM,
    FLAG_SKILL_HARD,
    FLAG_AMBUSH,
    FLAG_NOT_IN_SP,
    FLAG_NOT_IN_DM,
    FLAG_NOT_IN_COOP,
    FLAG_FRIENDLY_MONSTER,
};

void printFlags(int16_t flags) {
    printf("0b");
    for (ssize_t i = 15; i >= 0; --i) {
        char c = (flags & 1 << i) ? '1' : '0';
        putchar(c);
    }
}

void iterateThings(FILE *file, const struct FileLump *fileLump)
{
    struct Thing *things = malloc(fileLump->size);
    if (!things) {
        return;
    }

    int fseekRes = fseek(file, fileLump->filepos, SEEK_SET);
    if (fseekRes != 0) {
        fprintf(stderr, "[%s] error: unable to seek in file\n", progname);
        free(things);
        return;
    }

    size_t readNum = fread(things, 1, fileLump->size, file);
    if (readNum != (size_t) fileLump->size) {
        fprintf(stderr, "[%s] error: unable to read from file\n", progname);
        free(things);
        return;
    }

    for (int32_t i = 0; i < (int32_t) (fileLump->size / sizeof(*things)); ++i) {
        if (!isMonster(things[i].type)) {
            // continue;
        }

        printf("  Thing: (%d,%d; %d) 0x%x ",
                things[i].x,
                things[i].y,
                things[i].angle,
                things[i].type);

        printFlags(things[i].flags);

        if (removeEnemies == 1 && (things[i].flags & 0x0010) != 0) {
            // We want to remove enemies and the Monster is Multiplayer only
            printf(" - removing");

            // Unset EASY, MEDIUM and HARD flags to disable Multiplayer enemies
            const uint16_t MASK = 0xfff8;
            things[i].flags &= MASK;

            fseekRes = fseek(file, fileLump->filepos + i * sizeof(things[i]), SEEK_SET);
            if (fseekRes != 0) {
                fprintf(stderr, "[%s] error: unable to seek in file\n", progname);
                free(things);
                return;
            }

            size_t writtenNum = fwrite(&things[i], sizeof(things[i]), 1, file);
            if (writtenNum != 1) {
                fprintf(stderr, "[%s] error: unable to write to file\n", progname);
                free(things);
                return;
            }
        }

        printf("\n");
    }

    free(things);
}

void printFileLumpName(const struct FileLump *fileLump)
{
    for (size_t i = 0; i < 8; ++i) {
        if (fileLump->name[i] == '\0') break;
        putchar(fileLump->name[i]);
    }
}

void iterateLumps(FILE *file, const struct WadInfo *wadInfo)
{
    struct FileLump *fileLumps = malloc(sizeof(*fileLumps) * wadInfo->numlumps);
    if (!fileLumps) {
        return;
    }

    int fseekRes = fseek(file, wadInfo->infotableofs, SEEK_SET);
    if (fseekRes != 0) {
        fprintf(stderr, "[%s] error: unable to seek in file\n", progname);
        free(fileLumps);
        return;
    }

    size_t readNum = fread(fileLumps, sizeof(*fileLumps), wadInfo->numlumps, file);
    if (readNum != (size_t) wadInfo->numlumps) {
        fprintf(stderr, "[%s] error: unable to read from file\n", progname);
        free(fileLumps);
        return;
    }

    for (int32_t i = 0; i < wadInfo->numlumps; ++i) {
        printFileLumpName(&fileLumps[i]);
        printf(" (%d)\n", fileLumps[i].size);

        if (memcmp(fileLumps[i].name, "THINGS", 6) == 0) {
            iterateThings(file, &fileLumps[i]);
        }
    }

    free(fileLumps);
}

const char shortopts[] = "hi:o:r";
const struct option longopts[] = {
    { "help", no_argument, NULL, 'h' },
    { "input-file", required_argument, NULL, 'i' },
    { "output-file", required_argument, NULL, 'o' },
    { "remove-multiplayer-enemies", no_argument, NULL, 'r' },
    { 0, 0, 0, 0 }
};

void printUsage(const char *progname)
{
    printf("usage: %s [OPTIONS]\n", progname);
    printf("OPTIONS:\n");
    printf("  -h, --help                    Prints this help text\n");
    printf("  -i, --input-file  [FILE]      WAD file to read from\n");
    printf("  -o, --output-file [FILE]      WAD file to write to\n");
}

int dumpData(const char* ifname)
{
    int ret = 0;

    FILE *iFile = fopen(ifname, "r");
    if (!iFile) {
        fprintf(stderr, "[%s] error: unable to open \"%s\"\n", progname, ifname);
        return 1;
    }

    struct WadInfo wadInfo = { 0 };
    size_t readNum = fread(&wadInfo, sizeof(wadInfo), 1, iFile);
    if (readNum != 1) {
        ret = 1;
        goto cleanup;
    }

    char idString[5] = { 0 };
    memcpy(idString, &wadInfo.identification, sizeof(wadInfo.identification));
    printf("WadInfo:\n");
    printf("  identification: %s\n", idString);
    printf("  numlumps: %d\n", wadInfo.numlumps);
    printf("  infotableofs: %d\n", wadInfo.infotableofs);

    iterateLumps(iFile, &wadInfo);

cleanup:
    fclose(iFile);
    return ret;
}

int copyFile(FILE *iFile, FILE *oFile)
{
    int ret = 0;
    struct stat statbuf = { 0 };
    int fstatRet = fstat(fileno(iFile), &statbuf);
    if (fstatRet != 0) {
        fprintf(stderr, "[%s] error: unable to stat input file \n", progname);
        return 1;
    }

    // We explicitly overwrite the identification as we will always create a PWAD
    char *identification = "PWAD";
    size_t idLen = strlen(identification);
    size_t writtenBytes = fwrite(identification, 1, idLen, oFile);
    if (writtenBytes != idLen) {
        fprintf(stderr, "[%s] error: unable to write identification to output file\n", progname);
        return 1;
    }

    size_t remainingSize = statbuf.st_size - idLen;
    uint8_t *buf = malloc(remainingSize);
    if (!buf) {
        fprintf(stderr, "[%s] error: unable to allocate memory for input file\n", progname);
        return 1;
    }

    // Don't read the id
    int fseekRes = fseek(iFile, idLen, SEEK_SET);
    if (fseekRes != 0) {
        fprintf(stderr, "[%s] error: unable to seek in file\n", progname);
        ret = 1;
        goto cleanup;
    }

    size_t readBytes = fread(buf, 1, remainingSize, iFile);
    if (readBytes != remainingSize) {
        fprintf(stderr, "[%s] error: unable to read input file\n", progname);
        ret = 1;
        goto cleanup;
    }

    writtenBytes = fwrite(buf, 1, remainingSize, oFile);
    if (writtenBytes != remainingSize) {
        fprintf(stderr, "[%s] error: unable to write output file\n", progname);
        ret = 1;
        goto cleanup;
    }

    rewind(oFile);

cleanup:
    free(buf);
    return ret;
}


int changeData(const char *ifname, const char *ofname)
{
    int ret = 0;

    FILE *iFile = fopen(ifname, "r");
    if (!iFile) {
        fprintf(stderr, "[%s] error: unable to open \"%s\"\n", progname, ifname);
        return 1;
    }

    FILE *oFile = fopen(ofname, "w+");
    if (!oFile) {
        fprintf(stderr, "[%s] error: unable to open \"%s\"\n", progname, ofname);
        fclose(iFile);
        return 1;
    }

    if (copyFile(iFile, oFile) != 0) {
        ret = 1;
        goto cleanup;
    }

    struct WadInfo wadInfo = { 0 };
    size_t readNum = fread(&wadInfo, sizeof(wadInfo), 1, oFile);
    if (readNum != 1) {
        fprintf(stderr, "[%s] error: unable to read \"%s\"\n", progname, ofname);
        ret = 1;
        goto cleanup;
    }

    char idString[5] = { 0 };
    memcpy(idString, &wadInfo.identification, sizeof(wadInfo.identification));
    printf("WadInfo:\n");
    printf("  identification: %s\n", idString);
    printf("  numlumps: %d\n", wadInfo.numlumps);
    printf("  infotableofs: %d\n", wadInfo.infotableofs);

    iterateLumps(oFile, &wadInfo);

cleanup:
    fclose(oFile);
    fclose(iFile);
    return ret;
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

    if (removeEnemies == 0) {
        return dumpData(ifname);
    } else {
        return changeData(ifname, ofname);
    }
}
