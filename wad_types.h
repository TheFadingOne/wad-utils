#ifndef WAD_TYPES_H
#define WAD_TYPES_H

#include <inttypes.h>
#include <stddef.h>

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

_Static_assert(sizeof(struct WadInfo) == 12, "WadInfo wrong size");
_Static_assert(sizeof(struct FileLump) == 16, "FileLump wrong size");
_Static_assert(sizeof(struct Thing) == 10, "Thing wrong size");

struct WAD {
    struct WadInfo wadInfo;

    uint8_t *lumpData;
    size_t lumpDataSize;

    struct FileLump *directory;
    size_t directoryLen;
};

#endif
