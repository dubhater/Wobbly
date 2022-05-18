/*

Copyright (c) 2018, John Smith

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted, provided that the
above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/


#include "WobblyShared.h"

#include <cstdlib>

struct PluginDetectionInfo {
    const char *nice_name;
    const char *id;
    const char *function1;
    const char *function2;
};

static PluginDetectionInfo requiredPlugins[] = {
    {"VIVTC", "org.ivtc.v", "VFM", "VDecimate"},
    {"DMetrics", "com.vapoursynth.dmetrics", "DMetrics"},
    {"SCXVID", "com.nodame.scxvid", "Scxvid"},
    {"FieldHint", "com.nodame.fieldhint", "FieldHint"},
    {"TDeintMod", "com.holywu.tdeintmod", "IsCombed"},
    {"d2vsource", "com.sources.d2vsource", "Source"},
    {"L-SMASH-Works", "systems.innocent.lsmas", "LibavSMASHSource", "LWLibavSource"},
    {"DGDecNV", "com.vapoursynth.dgdecodenv", "DGSource"} };

static FilterState checkIfFiltersExists(const VSAPI *vsapi, VSCore *vscore, const char *id, const char *function_name1, const char *function_name2) {
    VSPlugin *plugin = vsapi->getPluginByID(id, vscore);
    if (!plugin)
        return FilterState::MissingPlugin;
    if (vsapi->getPluginFunctionByName(function_name1, plugin)) {
        if (!function_name2)
            return FilterState::Exists;
        if (vsapi->getPluginFunctionByName(function_name2, plugin))
            return FilterState::Exists;
    }
    return FilterState::MissingFilter;
}

std::map<std::string, FilterState> getRequiredFilterStates(const VSAPI *vsapi, VSCore *vscore) {
    std::map<std::string, FilterState> result;
    for (size_t i = 0; i < sizeof(requiredPlugins) / sizeof(requiredPlugins[0]); i++)
        result[requiredPlugins[i].nice_name] = checkIfFiltersExists(vsapi, vscore, requiredPlugins[i].id, requiredPlugins[i].function1, requiredPlugins[i].function2);
    return result;
}

uint8_t *packRGBFrame(const VSAPI *vsapi, const VSFrame *frame) {
    const uint8_t *ptrR = vsapi->getReadPtr(frame, 0);
    const uint8_t *ptrG = vsapi->getReadPtr(frame, 1);
    const uint8_t *ptrB = vsapi->getReadPtr(frame, 2);
    int width = vsapi->getFrameWidth(frame, 0);
    int height = vsapi->getFrameHeight(frame, 0);
    ptrdiff_t stride = vsapi->getStride(frame, 0);
    uint8_t *frame_data = reinterpret_cast<uint8_t *>(malloc(width * height * 4));
    uint8_t *fd_ptr = frame_data;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            fd_ptr[0] = ptrB[x];
            fd_ptr[1] = ptrG[x];
            fd_ptr[2] = ptrR[x];
            fd_ptr[3] = 0;
            fd_ptr += 4;
        }
        ptrR += stride;
        ptrG += stride;
        ptrB += stride;
    }

    return frame_data;
}