#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "globals.h"
#include "wad_types.h"

void freeWad(struct WAD *wad) {
    free(wad->lumpData);
    free(wad->directory);
}

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


int isMonster(int16_t type)
{
    for (size_t i = 0; i < sizeof(monsterTypes) / sizeof(monsterTypes[0]); ++i) {
        if (type == monsterTypes[i]) {
            return 1;
        }
    }
    return 0;
}

void printFlags(int16_t flags)
{
    printf("0b");
    for (ssize_t i = 15; i >= 0; --i) {
        char c = (flags & 1 << i) ? '1' : '0';
        putchar(c);
    }
}

void printFileLumpName(const struct FileLump *fileLump)
{
    for (size_t i = 0; i < 8; ++i) {
        if (fileLump->name[i] == '\0') break;
        putchar(fileLump->name[i]);
    }
}

_Bool isMapLump(const struct FileLump *lump)
{
    if (lump->name[0] == 'E' &&
            lump->name[1] != '\0' &&
            lump->name[2] == 'M' &&
            lump->name[3] != '\0' &&
            lump->name[4] == '\0') {
        // DOOM 1 Map Lump
        return 1;
    } else if (lump->name[0] == 'M' &&
            lump->name[1] == 'A' &&
            lump->name[2] == 'P' &&
            lump->name[3] != '\0' &&
            lump->name[4] != '\0' &&
            lump->name[5] == '\0') {
        // DOOM 2 Map Lump
        return 1;
    }

    return 0;
}

int writeWad(FILE *file, struct WAD *wad)
{
    size_t writtenNum = fwrite(&wad->wadInfo, sizeof(wad->wadInfo), 1, file);
    if (writtenNum != 1) {
        fprintf(stderr, "[%s] error: unable to write WAD header to output file\n", progname);
        return 1;
    }

    writtenNum = fwrite(wad->lumpData, 1, wad->lumpDataSize, file);
    if (writtenNum != wad->lumpDataSize) {
        fprintf(stderr, "[%s] error: unable to write lump data to output file\n", progname);
        return 1;
    }

    writtenNum = fwrite(wad->directory, sizeof(*wad->directory), wad->wadInfo.numlumps, file);
    if (writtenNum != (size_t) wad->wadInfo.numlumps) {
        fprintf(stderr, "[%s] error: unable to write directory to output file\n", progname);
        return 1;
    }

    return 0;
}

int loadThings(FILE *file, struct WAD *wad, const struct FileLump *thingLump)
{
    memcpy(&wad->directory[wad->directoryLen], thingLump, sizeof(*thingLump));
    wad->directory[wad->directoryLen].filepos = wad->lumpDataSize + sizeof(struct WadInfo);
    ++wad->directoryLen;

    int fseekRes = fseek(file, thingLump->filepos, SEEK_SET);
    if (fseekRes != 0) {
        fprintf(stderr, "[%s] error: unable to seek THINGS lump\n", progname);
        return 1;
    }

    uint8_t *thingPtr = wad->lumpData + wad->lumpDataSize;
    size_t readNum = fread(thingPtr, 1, thingLump->size, file);
    if (readNum != (size_t) thingLump->size) {
        fprintf(stderr, "[%s] error: unable to read THINGS lump\n", progname);
        return 1;
    }
    wad->lumpDataSize += thingLump->size;

    struct Thing tempThing = { 0 };
    uint8_t *endPtr = thingPtr + thingLump->size;
    for (; thingPtr < endPtr; thingPtr += sizeof(tempThing)) {
        // memcpy to avoid violating strict aliasing
        memcpy(&tempThing, thingPtr, sizeof(tempThing));

        if (!isMonster(tempThing.type)) {
            continue;
        }

        if (verbose) {
            printf("  Thing: (%d,%d; %d) 0x%x ",
                    tempThing.x,
                    tempThing.y,
                    tempThing.angle,
                    tempThing.type);

            printFlags(tempThing.flags);
        }

        if (removeEnemies == 1 && (tempThing.flags & 0x0010) != 0) {
            // We want to remove enemies and the Monster is Multiplayer only
            if (verbose) printf(" - removing");

            // Unset EASY, MEDIUM and HARD flags to disable Multiplayer enemies
            const uint16_t MASK = 0xfff8;
            tempThing.flags &= MASK;

            memcpy(thingPtr, &tempThing, sizeof(tempThing));
        }

        if (verbose) printf("\n");
    }

    return 0;
}

int loadWad(FILE *file, struct WAD *wad)
{
    // Set pointers to zero to not free random pointers on error
    wad->lumpData = NULL;
    wad->directory = NULL;

    struct FileLump *inputLumps = NULL;

    // Read header
    size_t readNum = fread(&wad->wadInfo, sizeof(wad->wadInfo), 1, file);
    if (readNum != 1) {
        fprintf(stderr, "[%s] error: unable to read header\n", progname);
        goto error;
    }

    // Allocate inputLumps for buffering all the Lumps and
    // allocate wad->directory pessimistically to hold the Lumps we want to keep
    //
    // This wastes memory but is very inconsequeantial regarding typical DOOM WAD sizes
    inputLumps = malloc(wad->wadInfo.numlumps * sizeof(*inputLumps));
    wad->directory = malloc(wad->wadInfo.numlumps * sizeof(*wad->directory));
    if (inputLumps == NULL || wad->directory == NULL) {
        fprintf(stderr, "[%s] error: unable to allocate memory for directory\n", progname);
        goto error;
    }

    struct stat statbuf = { 0 };
    int fstatRes = fstat(fileno(file), &statbuf);
    if (fstatRes != 0) {
        fprintf(stderr, "[%s] error: unable to stat input file\n", progname);
        goto error;
    }

    // Allocate pessimistically the max size it can be
    const size_t MAX_LUMP_DATA_SIZE = statbuf.st_size - sizeof(struct WadInfo) - wad->wadInfo.numlumps * sizeof(struct FileLump);
    wad->lumpData = malloc(MAX_LUMP_DATA_SIZE);
    if (wad->lumpData == NULL) {
        fprintf(stderr, "[%s] error: unable to allocate memory for lump data\n", progname);
        goto error;
    }

    int fseekRes = fseek(file, wad->wadInfo.infotableofs, SEEK_SET);
    if (fseekRes != 0) {
        fprintf(stderr, "[%s] error: unable to seek to directory\n", progname);
        goto error;
    }

    readNum = fread(inputLumps, sizeof(*inputLumps), wad->wadInfo.numlumps, file);
    if (readNum != (size_t) wad->wadInfo.numlumps) {
        fprintf(stderr, "[%s] error: unable to read directory\n", progname);
        goto error;
    }


    wad->lumpDataSize = 0;
    wad->directoryLen = 0;

    // Reset variables for incomplete Maps
    size_t resetLumpDataSize = 0;
    size_t resetDirectoryLen = 0;

    for (int32_t i = 0; i < wad->wadInfo.numlumps; ++i) {
        if (verbose) {
            printFileLumpName(&inputLumps[i]);
            printf(" (%d)\n", inputLumps[i].size);
        }

        if (!isMapLump(&inputLumps[i])) {
            continue;
        }

        if (i == wad->wadInfo.numlumps - 10) {
            fprintf(stderr, "[%s] error: unexpected end of WAD\n", progname);
            goto error;
        }

        resetLumpDataSize = wad->lumpDataSize;
        resetDirectoryLen = wad->directoryLen;

        memcpy(&wad->directory[wad->directoryLen], &inputLumps[i], sizeof(inputLumps[i]));
        if (wad->directory[wad->directoryLen].size != 0) {
            // Unlikely but possible

            // Seek to Lump
            fseekRes = fseek(file, wad->directory[wad->directoryLen].filepos, SEEK_SET);
            if (fseekRes != 0) {
                fprintf(stderr, "[%s] error: unable to seek to directory\n", progname);
                goto error;
            }

            // Read Lump
            readNum = fread(&wad->lumpData[wad->lumpDataSize], 1, wad->directory[wad->directoryLen].size, file);
            if (readNum != (size_t) wad->directory[wad->directoryLen].size) {
                fprintf(stderr, "[%s] error: unable to read from input file\n", progname);
                goto error;
            }

            // Set filepos to new filepos
            wad->directory[wad->directoryLen].filepos = wad->lumpDataSize + sizeof(struct WadInfo);
            wad->lumpDataSize += wad->directory[wad->directoryLen].size;
        }

        ++wad->directoryLen; // Count the previously loaded FileLump
        ++i; // Move to THINGS lump

        // Print things lump
        if (verbose) {
            printFileLumpName(&inputLumps[i]);
            printf(" (%d)\n", inputLumps[i].size);
        }

        if (memcmp(inputLumps[i].name, "THINGS", 6) != 0) {
            fprintf(stderr, "[%s] warning: MAP Lump without associated THINGS Lump encountered; skipping\n", progname);
            wad->directoryLen = resetDirectoryLen; // Remove MAP Lump from directory
            wad->lumpDataSize = resetLumpDataSize; // Remmove MAP Lump data from lumpData
            continue;
        }

        // Process THINGS lump specially as we want to remove multiplayer enemies
        int loadThingsRes = loadThings(file, wad, &inputLumps[i]);
        if (loadThingsRes != 0) {
            fprintf(stderr, "[%s] error: unable to load things\n", progname);
            goto error;
        }

        // Copy the rest of the map Lumps
        const char *REMAINING_MAP_LUMPS[] = { "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS", "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP" };
        const size_t REMAINING_MAP_LUMPS_LEN = 9;
        for (size_t j = 0; j < REMAINING_MAP_LUMPS_LEN; ++j) {
            ++i; // Move to next Map lump

            if (verbose) {
                printFileLumpName(&inputLumps[i]);
                printf(" (%d)\n", inputLumps[i].size);
            }

            if (memcmp(inputLumps[i].name, REMAINING_MAP_LUMPS[j], strlen(REMAINING_MAP_LUMPS[j])) != 0) {
                fprintf(stderr, "[%s] warning: MAP Lump without associated %s Lump encountered; skipping\n", progname, REMAINING_MAP_LUMPS[j]);
                wad->directoryLen = resetDirectoryLen; // Remove MAP Lump from directory
                wad->lumpDataSize = resetLumpDataSize; // Remmove MAP Lump data from lumpData
                break;
            }

            memcpy(&wad->directory[wad->directoryLen], &inputLumps[i], sizeof(inputLumps[i]));
            if (wad->directory[wad->directoryLen].size != 0) {
                // Seek to Lump
                fseekRes = fseek(file, wad->directory[wad->directoryLen].filepos, SEEK_SET);
                if (fseekRes != 0) {
                    fprintf(stderr, "[%s] error: unable to seek to %s lump\n", progname, REMAINING_MAP_LUMPS[j]);
                    goto error;
                }

                // Read lump
                readNum = fread(&wad->lumpData[wad->lumpDataSize], 1, wad->directory[wad->directoryLen].size, file);
                if (readNum != (size_t) wad->directory[wad->directoryLen].size) {
                    fprintf(stderr, "[%s] error: unable to read %s lump\n", progname, REMAINING_MAP_LUMPS[j]);
                    goto error;
                }

                // update FileLump info
                wad->directory[wad->directoryLen].filepos = wad->lumpDataSize + sizeof(struct WadInfo);
                wad->lumpDataSize += wad->directory[wad->directoryLen].size;
            }

            ++wad->directoryLen;
        }
    }

    // Update header
    memcpy(&wad->wadInfo.identification, "PWAD", 4);
    wad->wadInfo.numlumps = wad->directoryLen;
    wad->wadInfo.infotableofs = wad->lumpDataSize + sizeof(struct WadInfo);

    free(inputLumps);
    return 0;

error:
    free(wad->lumpData);
    free(inputLumps);
    free(wad->directory);
    return 1;
}
