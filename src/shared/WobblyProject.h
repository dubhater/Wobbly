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
    std::map<int, FrameRange> frames; // Key is FrameRange::first

    CustomList(const std::string &_name, const std::string &_preset = "", int _position = 0)
        : name(_name)
        , preset(_preset)
        , position(_position)
    { }

    void addFrameRange(int first, int last) {
        if (first > last)
            std::swap(first, last);

        const FrameRange *overlap = findFrameRange(first);
        if (!overlap)
            overlap = findFrameRange(last);
        if (!overlap) {
            auto it = frames.upper_bound(first);
            if (it != frames.cend() && it->second.first < last)
                overlap = &it->second;
        }

        if (overlap)
            throw WobblyException("Can't add range (" + std::to_string(first) + "," + std::to_string(last) + ") to custom list '" + name + "': overlaps range (" + std::to_string(overlap->first) + "," + std::to_string(overlap->last) + ").");

        FrameRange range = { first, last };
        frames.insert(std::make_pair(range.first, range));
    }

    void deleteFrameRange(int first) {
        frames.erase(first);
    }

    const FrameRange *findFrameRange(int frame) const {
        if (!frames.size())
            return nullptr;

        auto it = frames.upper_bound(frame);

        if (it == frames.cbegin())
            return nullptr;

        it--;

        if (it->second.first <= frame && frame <= it->second.last)
            return &it->second;

        return nullptr;
    }
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


struct FailedPatternGuessing {
    int start;
    int reason;
};


enum PatternGuessingFailureReason {
    SectionTooShort = 0,
    AmbiguousMatchPattern
};


struct PatternGuessing {
    int minimum_length;
    int third_n_match;
    int decimation;
    std::map<int, FailedPatternGuessing> failures; // Key is FailedPatternGuessing::start
};


class WobblyProject {
    private:
        int num_frames[3];

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

        Resize resize;
        Crop crop;
        Depth depth;

        std::string source_filter;

        // Only functions below.

        bool isNameSafeForPython(const std::string &name);
        int maybeTranslate(int frame, bool is_end, PositionInFilterChain position);

    public:
        WobblyProject(bool _is_wobbly);
        WobblyProject(bool _is_wobbly, const std::string &_input_file, const std::string &_source_filter, int64_t _fps_num, int64_t _fps_den, int _width, int _height, int _num_frames);

        int getNumFrames(PositionInFilterChain position);

        void writeProject(const std::string &path);
        void readProject(const std::string &path);


        void addFreezeFrame(int first, int last, int replacement);
        void deleteFreezeFrame(int frame);
        const FreezeFrame *findFreezeFrame(int frame);
        std::vector<FreezeFrame> getFreezeFrames();


        void addPreset(const std::string &preset_name);
        void addPreset(const std::string &preset_name, const std::string &preset_contents);
        void renamePreset(const std::string &old_name, const std::string &new_name);
        void deletePreset(const std::string &preset_name);
        std::vector<std::string> getPresets();
        const std::string &getPresetContents(const std::string &preset_name);
        void setPresetContents(const std::string &preset_name, const std::string &preset_contents);
        bool isPresetInUse(const std::string &preset_name);


        const std::array<int16_t, 5> &getMics(int frame);
        int getPreviousFrameWithMic(int minimum, int start_frame);
        int getNextFrameWithMic(int minimum, int start_frame);


        char getMatch(int frame);
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


        const std::vector<CustomList> &getCustomLists();
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
        const FrameRange *findCustomListRange(int list_index, int frame);


        int getDecimateMetric(int frame);


        void addDecimatedFrame(int frame);
        void deleteDecimatedFrame(int frame);
        bool isDecimatedFrame(int frame);
        void clearDecimatedFramesFromCycle(int frame);


        std::vector<DecimationRange> getDecimationRanges();
        std::vector<DecimationPatternRange> getDecimationPatternRanges();


        std::map<size_t, size_t> getCMatchSequences(int minimum);


        void addCombedFrame(int frame);
        void deleteCombedFrame(int frame);
        bool isCombedFrame(int frame);


        const Resize &getResize();
        void setResize(int new_width, int new_height, const std::string &filter);
        void setResizeEnabled(bool enabled);
        bool isResizeEnabled();

        const Crop &getCrop();
        void setCrop(int left, int top, int right, int bottom);
        void setCropEnabled(bool enabled);
        bool isCropEnabled();
        void setCropEarly(bool early);
        bool isCropEarly();

        const Depth &getBitDepth();
        void setBitDepth(int bits, bool float_samples, const std::string &dither);
        void setBitDepthEnabled(bool enabled);
        bool isBitDepthEnabled();


        const std::string &getSourceFilter();
        void setSourceFilter(const std::string &filter);


        int getZoom();
        void setZoom(int ratio);


        int getLastVisitedFrame();
        void setLastVisitedFrame(int frame);


        std::string getUIState();
        void setUIState(const std::string &state);


        std::string getUIGeometry();
        void setUIGeometry(const std::string &geometry);


        std::array<bool, 5> getShownFrameRates();
        void setShownFrameRates(const std::array<bool, 5> &rates);


        int getMicSearchMinimum();
        void setMicSearchMinimum(int minimum);


        int getCMatchSequencesMinimum();
        void setCMatchSequencesMinimum(int minimum);


        std::string frameToTime(int frame);


        int frameNumberAfterDecimation(int frame);


        bool guessSectionPatternsFromMatches(int section_start, int minimum_length, int use_third_n_match, int drop_duplicate);
        void guessProjectPatternsFromMatches(int minimum_length, int use_third_n_match, int drop_duplicate);
        const PatternGuessing &getPatternGuessing();


        void sectionsToScript(std::string &script);
        void customListsToScript(std::string &script, PositionInFilterChain position);
        void headerToScript(std::string &script);
        void presetsToScript(std::string &script);
        void sourceToScript(std::string &script);
        void trimToScript(std::string &script);
        void fieldHintToScript(std::string &script);
        void freezeFramesToScript(std::string &script);
        void decimatedFramesToScript(std::string &script);
        void cropToScript(std::string &script);
        void showCropToScript(std::string &script);
        void resizeToScript(std::string &script);
        void bitDepthToScript(std::string &script);
        void setOutputToScript(std::string &script);

        std::string generateFinalScript();
        std::string generateMainDisplayScript(bool show_crop);

        std::string generateTimecodesV1();
};

#endif // WOBBLYPROJECT_H
