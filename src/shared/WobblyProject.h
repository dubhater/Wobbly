/*

Copyright (c) 2015, John Smith

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


#ifndef WOBBLYPROJECT_H
#define WOBBLYPROJECT_H


#include <cstdint>

#include <unordered_map>
#include <map>

#include <set>

#include <array>
#include <vector>
#include <string>

#include "WobblyException.h"


/*
static const char[] match_chars = { 'p', 'c', 'n', 'b', 'u' };


enum Matches {
    P = 0,
    C,
    N,
    B,
    U
};
*/


static inline uint8_t matchCharToIndex(char match) {
    if (match == 'p')
        return 0;
    if (match == 'c')
        return 1;
    if (match == 'n')
        return 2;
    if (match == 'b')
        return 3;
    if (match == 'u')
        return 4;

    return 255;
}


struct FreezeFrame {
    int first;
    int last;
    int replacement;
};


struct Preset {
    std::string name; // Must be suitable for use as Python function name.
    std::string contents;
};


struct Section {
    int start;
    std::vector<std::string> presets; // Preset names, in user-defined order.

    Section(int _start)
        : start(_start)
    { }
};


struct FrameRange {
    int first;
    int last;
};


struct CustomList {
    std::string name;
    std::string preset; // Preset name.
    int position;
    std::map<int, FrameRange> ranges; // Key is FrameRange::first

    CustomList(const std::string &_name, const std::string &_preset = "", int _position = 0)
        : name(_name)
        , preset(_preset)
        , position(_position)
    { }
};


struct Resize {
    bool enabled;
    int width;
    int height;
    std::string filter;
};


struct Crop {
    bool enabled;
    bool early;
    int left;
    int top;
    int right;
    int bottom;
};


struct Depth {
    bool enabled;
    int bits;
    bool float_samples;
    std::string dither;
};


struct DecimationRange {
    int start;
    int num_dropped;
};


struct DecimationPatternRange {
    int start;
    std::set<int8_t> dropped_offsets;
};


enum PositionInFilterChain {
    PostSource = 0,
    PostFieldMatch,
    PostDecimate
};


enum UseThirdNMatch {
    UseThirdNMatchAlways = 0,
    UseThirdNMatchNever,
    UseThirdNMatchIfPrettier
};


enum DropDuplicate {
    DropFirstDuplicate = 0,
    DropSecondDuplicate,
    DropUglierDuplicatePerCycle,
    DropUglierDuplicatePerSection
};


enum Patterns {
    PatternCCCNN = 1 << 0,
    PatternCCNNN = 1 << 1,
    PatternCCCCC = 1 << 2
};


struct FailedPatternGuessing {
    int start;
    int reason;
};


enum PatternGuessingFailureReason {
    SectionTooShort = 0,
    AmbiguousMatchPattern
};


enum PatternGuessingMethods {
    PatternGuessingFromMatches = 0,
    PatternGuessingFromMics
};


struct PatternGuessing {
    int method;
    int minimum_length;
    int third_n_match;
    int decimation;
    int use_patterns;
    std::map<int, FailedPatternGuessing> failures; // Key is FailedPatternGuessing::start
};


struct InterlacedFade {
    int frame;
    double field_difference;
};


struct ImportedThings {
    bool geometry;
    bool presets;
    bool custom_lists;
    bool crop;
    bool resize;
    bool bit_depth;
    bool mic_search;
    bool zoom;
};


class WobblyProject {
    private:
        int num_frames[2];

        int64_t fps_num;
        int64_t fps_den;

        int width;
        int height;

        int zoom;
        int last_visited_frame;
        std::string ui_state;
        std::string ui_geometry;
        std::array<bool, 5> shown_frame_rates;
        int mic_search_minimum;
        int c_match_sequences_minimum;

        std::string input_file;
        std::map<int, FrameRange> trims; // Key is FrameRange::first
        std::unordered_map<std::string, double> vfm_parameters;
        std::unordered_map<std::string, double> vdecimate_parameters;

        std::vector<std::array<int16_t, 5> > mics;
        std::vector<char> matches;
        std::vector<char> original_matches;
        std::set<int> combed_frames;
        std::vector<std::set<int8_t> > decimated_frames; // unordered_set may be sufficient.
        std::vector<int> decimate_metrics;

        bool is_wobbly; // XXX Maybe only the json writing function needs to know.

        std::map<std::string, Preset> presets; // Key is Preset::name

        std::map<int, FreezeFrame> frozen_frames; // Key is FreezeFrame::first

        std::map<int, Section> sections; // Key is Section::start

        std::vector<CustomList> custom_lists;

        PatternGuessing pattern_guessing;

        std::map<int, InterlacedFade> interlaced_fades; // Key is InterlacedFade::frame

        Resize resize;
        Crop crop;
        Depth depth;

        std::string source_filter;

        // Only functions below.

        void setNumFrames(PositionInFilterChain position, int frames);

        bool isNameSafeForPython(const std::string &name) const;
        int maybeTranslate(int frame, bool is_end, PositionInFilterChain position) const;

        void applyPatternGuessingDecimation(const int section_start, const int section_end, const int first_duplicate, int drop_duplicate);

    public:
        WobblyProject(bool _is_wobbly);
        WobblyProject(bool _is_wobbly, const std::string &_input_file, const std::string &_source_filter, int64_t _fps_num, int64_t _fps_den, int _width, int _height, int _num_frames);

        int getNumFrames(PositionInFilterChain position) const;

        void writeProject(const std::string &path, bool compact_project) const;
        void readProject(const std::string &path);


        void addFreezeFrame(int first, int last, int replacement);
        void deleteFreezeFrame(int frame);
        const FreezeFrame *findFreezeFrame(int frame) const;
        std::vector<FreezeFrame> getFreezeFrames() const;


        void addPreset(const std::string &preset_name);
        void addPreset(const std::string &preset_name, const std::string &preset_contents);
        void renamePreset(const std::string &old_name, const std::string &new_name);
        void deletePreset(const std::string &preset_name);
        std::vector<std::string> getPresets() const;
        const std::string &getPresetContents(const std::string &preset_name) const;
        void setPresetContents(const std::string &preset_name, const std::string &preset_contents);
        bool isPresetInUse(const std::string &preset_name) const;
        bool presetExists(const std::string &preset_name) const;


        void addTrim(int trim_start, int trim_end);


        void setVFMParameter(const std::string &name, double value);
        void setVDecimateParameter(const std::string &name, double value);


        std::array<int16_t, 5> getMics(int frame) const;
        void setMics(int frame, int16_t mic_p, int16_t mic_c, int16_t mic_n, int16_t mic_b, int16_t mic_u);
        int getPreviousFrameWithMic(int minimum, int start_frame) const;
        int getNextFrameWithMic(int minimum, int start_frame) const;


        char getOriginalMatch(int frame) const;
        void setOriginalMatch(int frame, char match);


        char getMatch(int frame) const;
        void setMatch(int frame, char match);
        void cycleMatchBCN(int frame);
        void cycleMatch(int frame);


        void addSection(int section_start);
        void addSection(const Section &section);
        void deleteSection(int section_start);
        Section *findSection(int frame);
        Section *findNextSection(int frame);
        int getSectionEnd(int frame);
        void setSectionPreset(int section_start, const std::string &preset_name);
        void setSectionMatchesFromPattern(int section_start, const std::string &pattern);
        void setSectionDecimationFromPattern(int section_start, const std::string &pattern);


        void setRangeMatchesFromPattern(int range_start, int range_end, const std::string &pattern);
        void setRangeDecimationFromPattern(int range_start, int range_end, const std::string &pattern);


        void resetSectionMatches(int section_start);
        void resetRangeMatches(int start, int end);


        const std::vector<CustomList> &getCustomLists() const;
        void addCustomList(const std::string &list_name);
        void addCustomList(const CustomList &list);
        void renameCustomList(const std::string &old_name, const std::string &new_name);
        void deleteCustomList(const std::string &list_name);
        void deleteCustomList(int list_index);
        void moveCustomListUp(int list_index);
        void moveCustomListDown(int list_index);
        void setCustomListPreset(int list_index, const std::string &preset_name);
        void setCustomListPosition(int list_index, PositionInFilterChain position);
        void addCustomListRange(int list_index, int first, int last);
        void deleteCustomListRange(int list_index, int first);
        const FrameRange *findCustomListRange(int list_index, int frame) const;
        bool customListExists(const std::string &list_name) const;


        int getDecimateMetric(int frame) const;
        void setDecimateMetric(int frame, int decimate_metric);


        void addDecimatedFrame(int frame);
        void deleteDecimatedFrame(int frame);
        bool isDecimatedFrame(int frame) const;
        void clearDecimatedFramesFromCycle(int frame);


        std::vector<DecimationRange> getDecimationRanges() const;
        std::vector<DecimationPatternRange> getDecimationPatternRanges() const;


        std::map<size_t, size_t> getCMatchSequences(int minimum) const;


        void addCombedFrame(int frame);
        void deleteCombedFrame(int frame);
        bool isCombedFrame(int frame) const;


        const Resize &getResize() const;
        void setResize(int new_width, int new_height, const std::string &filter);
        void setResizeEnabled(bool enabled);
        bool isResizeEnabled() const;

        const Crop &getCrop() const;
        void setCrop(int left, int top, int right, int bottom);
        void setCropEnabled(bool enabled);
        bool isCropEnabled() const;
        void setCropEarly(bool early);
        bool isCropEarly() const;

        const Depth &getBitDepth() const;
        void setBitDepth(int bits, bool float_samples, const std::string &dither);
        void setBitDepthEnabled(bool enabled);
        bool isBitDepthEnabled() const;


        const std::string &getSourceFilter() const;
        void setSourceFilter(const std::string &filter);


        int getZoom() const;
        void setZoom(int ratio);


        int getLastVisitedFrame() const;
        void setLastVisitedFrame(int frame);


        std::string getUIState() const;
        void setUIState(const std::string &state);


        std::string getUIGeometry() const;
        void setUIGeometry(const std::string &geometry);


        std::array<bool, 5> getShownFrameRates() const;
        void setShownFrameRates(const std::array<bool, 5> &rates);


        int getMicSearchMinimum() const;
        void setMicSearchMinimum(int minimum);


        int getCMatchSequencesMinimum() const;
        void setCMatchSequencesMinimum(int minimum);


        std::string frameToTime(int frame) const;


        int frameNumberAfterDecimation(int frame) const;


        bool guessSectionPatternsFromMics(int section_start, int minimum_length, int use_patterns, int drop_duplicate);
        void guessProjectPatternsFromMics(int minimum_length, int use_patterns, int drop_duplicate);

        bool guessSectionPatternsFromMatches(int section_start, int minimum_length, int use_third_n_match, int drop_duplicate);
        void guessProjectPatternsFromMatches(int minimum_length, int use_third_n_match, int drop_duplicate);
        const PatternGuessing &getPatternGuessing();


        void addInterlacedFade(int frame, double field_difference);
        const std::map<int, InterlacedFade> &getInterlacedFades() const;


        void sectionsToScript(std::string &script) const;
        void customListsToScript(std::string &script, PositionInFilterChain position) const;
        void headerToScript(std::string &script) const;
        void presetsToScript(std::string &script) const;
        void sourceToScript(std::string &script) const;
        void trimToScript(std::string &script) const;
        void fieldHintToScript(std::string &script) const;
        void freezeFramesToScript(std::string &script) const;
        void decimatedFramesToScript(std::string &script) const;
        void cropToScript(std::string &script) const;
        void resizeAndBitDepthToScript(std::string &script, bool resize_enabled, bool depth_enabled) const;
        void setOutputToScript(std::string &script) const;

        std::string generateFinalScript() const;
        std::string generateMainDisplayScript() const;

        std::string generateTimecodesV1() const;


        void importFromOtherProject(const std::string &path, const ImportedThings &imports);
};

#endif // WOBBLYPROJECT_H
