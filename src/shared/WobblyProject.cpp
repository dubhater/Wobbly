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


#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QFile>

#define RAPIDJSON_NAMESPACE rj
#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/error/en.h"

#include "RandomStuff.h"
#include "WobblyException.h"
#include "WobblyProject.h"


#define PROJECT_FORMAT_VERSION 2


// Isn't it wonderful?
#define K(...) KEY(__VA_ARGS__, 4, 3, 2, 1)
#define KEY(a, b, c, d, count, ...) KEY##count(a, b, c, d)

#define KEY1(a, b, c, d) const char a[] = #a;
#define KEY2(a, b, c, d) const char a##_##b[] = #a " " #b;
#define KEY3(a, b, c, d) const char a##_##b##_##c[] = #a " " #b " " #c;
#define KEY4(a, b, c, d) const char a##_##b##_##c##_##d[] = #a " " #b " " #c " " #d;

namespace Keys {
    K(wobbly, version);
    K(project, format, version);
    K(input, file);
    K(input, frame, rate);
    K(input, resolution);
    K(trim);
    K(source, filter);
    K(user, interface);
    namespace UserInterface {
        K(zoom);
        K(last, visited, frame);
        K(geometry);
        K(state);
        K(show, frame, rates);
        K(mic, search, minimum);
        K(c, match, sequences, minimum);
        K(pattern, guessing);
        namespace PatternGuessing {
            K(method);
            K(minimum, length);
            K(use, third, n, match);
            K(decimate);
            K(use, patterns);
            K(failures);
            namespace Failures {
                K(start);
                K(reason);
            }
        }
        K(bookmarks);
        namespace Bookmarks {
            K(frame);
            K(description);
        }
    }
    K(vfm, parameters);
    namespace VFMParameters {
        K(blockx);
        K(blocky);
        K(chroma);
        K(cthresh);
        K(mchroma);
        K(mi);
        K(micmatch);
        K(order);
        K(scthresh);
        K(y0);
        K(y1);
    }
    K(vdecimate, parameters);
    namespace VDecimateParameters {
        K(blockx);
        K(blocky);
        K(chroma);
        K(dupthresh);
        K(scthresh);
    }
    K(mics);
    K(matches);
    K(original, matches);
    K(combed, frames);
    K(decimated, frames);
    K(decimate, metrics);
    K(sections);
    namespace Sections {
        K(start);
        K(presets);
    }
    K(interlaced, fades);
    namespace InterlacedFades {
        K(frame);
        K(field, difference);
    }
    K(presets);
    namespace Presets {
        K(name);
        K(contents);
    }
    K(frozen, frames);
    K(custom, lists);
    namespace CustomLists {
        K(name);
        K(preset);
        K(position);
        K(frames);
    }
    K(resize);
    namespace Resize {
        K(width);
        K(height);
        K(filter);
    }
    K(crop);
    namespace Crop {
        K(early);
        K(left);
        K(top);
        K(right);
        K(bottom);
    }
    K(depth);
    namespace Depth {
        K(bits);
        K(float, samples);
        K(dither);
    }
}
#undef K
#undef KEY
#undef KEY1
#undef KEY2
#undef KEY3
#undef KEY4


WobblyProject::WobblyProject(bool _is_wobbly)
    : num_frames{ 0, 0 }
    , fps_num(0)
    , fps_den(0)
    , width(0)
    , height(0)
    , zoom(1)
    , last_visited_frame(0)
    , shown_frame_rates{ false, false, false, false, false }
    , mic_search_minimum(20)
    , c_match_sequences_minimum(20)
    , is_wobbly(_is_wobbly)
    , pattern_guessing{ PatternGuessingFromMics, 10, UseThirdNMatchNever, DropFirstDuplicate, PatternCCCNN | PatternCCNNN | PatternCCCCC, FailedPatternGuessingMap() }
    , combed_frames(new CombedFramesModel(this))
    , frozen_frames(new FrozenFramesModel(this))
    , presets(new PresetsModel(this))
    , custom_lists(new CustomListsModel(this))
    , sections(new SectionsModel(this))
    , bookmarks(new BookmarksModel(this))
    , resize{ false, 0, 0, "spline16" }
    , crop{ false, false, 0, 0, 0, 0 }
    , depth{ false, 8, false, "random" }
    , freeze_frames_wanted(true)
    , is_modified(false)
{
    connect(bookmarks, &BookmarksModel::dataChanged, [this] () {
        setModified(true);
    });
}


WobblyProject::WobblyProject(bool _is_wobbly, const std::string &_input_file, const std::string &_source_filter, int64_t _fps_num, int64_t _fps_den, int _width, int _height, int _num_frames)
    : WobblyProject(_is_wobbly)
{
    input_file = _input_file;
    source_filter = _source_filter;
    fps_num = _fps_num;
    fps_den = _fps_den;
    width = _width;
    height = _height;
    setNumFrames(PostSource, _num_frames);
    setNumFrames(PostDecimate, _num_frames);

    // XXX What happens when the video happens to be bottom field first?
    vfm_parameters.insert({ "order", 1 });
    decimated_frames.resize((_num_frames - 1) / 5 + 1);
    addSection(0);
    resize.width = _width;
    resize.height = _height;

    setModified(false);
}


int WobblyProject::getNumFrames(PositionInFilterChain position) const {
    if (position == PostSource)
        return num_frames[0];
    else if (position == PostDecimate)
        return num_frames[1];
    else
        throw WobblyException("Can't get the number of frames for position " + std::to_string(position) + ": invalid position.");
}


void WobblyProject::setNumFrames(PositionInFilterChain position, int frames) {
    if (position == PostSource)
        num_frames[0] = frames;
    else if (position == PostDecimate)
        num_frames[1] = frames;
    else
        throw WobblyException("Can't set the number of frames for position " + std::to_string(position) + ": invalid position.");
}


void WobblyProject::writeProject(const std::string &path, bool compact_project) {
    QFile file(QString::fromStdString(path));

    if (!file.open(QIODevice::WriteOnly))
        throw WobblyException("Couldn't open project file '" + path + "'. Error message: " + file.errorString().toStdString());

    rj::Document json_project(rj::kObjectType);

    rj::Document::AllocatorType &a = json_project.GetAllocator();

    json_project.AddMember(Keys::wobbly_version, std::atoi(PACKAGE_VERSION), a);


    json_project.AddMember(Keys::project_format_version, PROJECT_FORMAT_VERSION, a);


    json_project.AddMember(Keys::input_file, input_file, a);


    rj::Value json_fps(rj::kArrayType);
    json_fps.PushBack(fps_num, a);
    json_fps.PushBack(fps_den, a);
    json_project.AddMember(Keys::input_frame_rate, json_fps, a);


    rj::Value json_resolution(rj::kArrayType);
    json_resolution.PushBack(width, a);
    json_resolution.PushBack(height, a);
    json_project.AddMember(Keys::input_resolution, json_resolution, a);


    if (is_wobbly) {
        rj::Value json_ui(rj::kObjectType);
        json_ui.AddMember(Keys::UserInterface::zoom, zoom, a);
        json_ui.AddMember(Keys::UserInterface::last_visited_frame, last_visited_frame, a);
        json_ui.AddMember(Keys::UserInterface::geometry, ui_geometry, a);
        json_ui.AddMember(Keys::UserInterface::state, ui_state, a);

        rj::Value json_rates(rj::kArrayType);
        int rates[] = { 30, 24, 18, 12, 6 };
        for (int i = 0; i < 5; i++)
            if (shown_frame_rates[i])
                json_rates.PushBack(rates[i], a);
        json_ui.AddMember(Keys::UserInterface::show_frame_rates, json_rates, a);

        json_ui.AddMember(Keys::UserInterface::mic_search_minimum, mic_search_minimum, a);
        json_ui.AddMember(Keys::UserInterface::c_match_sequences_minimum, c_match_sequences_minimum, a);

        if (pattern_guessing.failures.size()) {
            rj::Value json_pattern_guessing(rj::kObjectType);

            const char *guessing_methods[] = {
                "from matches",
                "from mics"
            };
            json_pattern_guessing.AddMember(Keys::UserInterface::PatternGuessing::method, rj::Value(guessing_methods[pattern_guessing.method], a), a);

            json_pattern_guessing.AddMember(Keys::UserInterface::PatternGuessing::minimum_length, pattern_guessing.minimum_length, a);

            const char *third_n_match[] = {
                "always",
                "never",
                "if it has lower mic"
            };
            json_pattern_guessing.AddMember(Keys::UserInterface::PatternGuessing::use_third_n_match, rj::Value(third_n_match[pattern_guessing.third_n_match], a), a);

            const char *decimate[] = {
                "first duplicate",
                "second duplicate",
                "duplicate with higher mic per cycle",
                "duplicate with higher mic per section"
            };
            json_pattern_guessing.AddMember(Keys::UserInterface::PatternGuessing::decimate, rj::Value(decimate[pattern_guessing.decimation], a), a);

            rj::Value json_use_patterns(rj::kArrayType);

            std::map<int, std::string> use_patterns = {
                { PatternCCCNN, "cccnn" },
                { PatternCCNNN, "ccnnn" },
                { PatternCCCCC, "ccccc" }
            };

            for (auto it = use_patterns.cbegin(); it != use_patterns.cend(); it++)
                if (pattern_guessing.use_patterns & it->first)
                    json_use_patterns.PushBack(rj::Value(it->second, a), a);
            json_pattern_guessing.AddMember(Keys::UserInterface::PatternGuessing::use_patterns, json_use_patterns, a);

            rj::Value json_failures(rj::kArrayType);

            const char *reasons[] = {
                "section too short",
                "ambiguous pattern"
            };
            for (auto it = pattern_guessing.failures.cbegin(); it != pattern_guessing.failures.cend(); it++) {
                rj::Value json_failure(rj::kObjectType);
                json_failure.AddMember(Keys::UserInterface::PatternGuessing::Failures::start, it->second.start, a);
                json_failure.AddMember(Keys::UserInterface::PatternGuessing::Failures::reason, rj::Value(reasons[it->second.reason], a), a);
                json_failures.PushBack(json_failure, a);
            }
            json_pattern_guessing.AddMember(Keys::UserInterface::PatternGuessing::failures, json_failures, a);

            json_ui.AddMember(Keys::UserInterface::pattern_guessing, json_pattern_guessing, a);
        }

        if (bookmarks->size()) {
            rj::Value json_bookmarks(rj::kArrayType);

            for (auto it = bookmarks->cbegin(); it != bookmarks->cend(); it++) {
                rj::Value json_bookmark(rj::kObjectType);
                json_bookmark.AddMember(Keys::UserInterface::Bookmarks::frame, it->second.frame, a);
                json_bookmark.AddMember(Keys::UserInterface::Bookmarks::description, rj::Value(it->second.description, a), a);
                json_bookmarks.PushBack(json_bookmark, a);
            }

            json_ui.AddMember(Keys::UserInterface::bookmarks, json_bookmarks, a);
        }

        json_project.AddMember(Keys::user_interface, json_ui, a);
    }


    rj::Value json_trims(rj::kArrayType);

    for (auto it = trims.cbegin(); it != trims.cend(); it++) {
        rj::Value json_trim(rj::kArrayType);
        json_trim.PushBack(it->second.first, a);
        json_trim.PushBack(it->second.last, a);
        json_trims.PushBack(json_trim, a);
    }
    json_project.AddMember(Keys::trim, json_trims, a);


    rj::Value json_vfm_parameters(rj::kObjectType);

    for (auto it = vfm_parameters.cbegin(); it != vfm_parameters.cend(); it++)
        json_vfm_parameters.AddMember(rj::Value(it->first, a), rj::Value(it->second), a);

    json_project.AddMember(Keys::vfm_parameters, json_vfm_parameters, a);


    rj::Value json_vdecimate_parameters(rj::kObjectType);

    for (auto it = vdecimate_parameters.cbegin(); it != vdecimate_parameters.cend(); it++)
        json_vdecimate_parameters.AddMember(rj::Value(it->first, a), rj::Value(it->second), a);

    json_project.AddMember(Keys::vdecimate_parameters, json_vdecimate_parameters, a);

    if (mics.size()) {
        rj::Value json_mics(rj::kArrayType);

        for (size_t i = 0; i < mics.size(); i++) {
            rj::Value json_mic(rj::kArrayType);
            for (int j = 0; j < 5; j++)
                json_mic.PushBack(mics[i][j], a);
            json_mics.PushBack(json_mic, a);
        }

        json_project.AddMember(Keys::mics, json_mics, a);
    }

    if (matches.size()) {
        rj::Value json_matches(rj::kArrayType);

        for (size_t i = 0; i < matches.size(); i++)
            json_matches.PushBack(rj::Value(&matches[i], 1), a);

        json_project.AddMember(Keys::matches, json_matches, a);
    }

    if (original_matches.size()) {
        rj::Value json_original_matches(rj::kArrayType);

        for (size_t i = 0; i < original_matches.size(); i++)
            json_original_matches.PushBack(rj::Value(&original_matches[i], 1), a);

        json_project.AddMember(Keys::original_matches, json_original_matches, a);
    }

    if (combed_frames->cbegin() != combed_frames->cend()) {
        rj::Value json_combed_frames(rj::kArrayType);

        for (auto it = combed_frames->cbegin(); it != combed_frames->cend(); it++)
            json_combed_frames.PushBack(*it, a);

        json_project.AddMember(Keys::combed_frames, json_combed_frames, a);
    }

    if (decimated_frames.size()) {
        rj::Value json_decimated_frames(rj::kArrayType);

        for (size_t i = 0; i < decimated_frames.size(); i++)
            for (auto it = decimated_frames[i].cbegin(); it != decimated_frames[i].cend(); it++)
                json_decimated_frames.PushBack((int)i * 5 + *it, a);

        json_project.AddMember(Keys::decimated_frames, json_decimated_frames, a);
    }

    if (decimate_metrics.size()) {
        rj::Value json_decimate_metrics(rj::kArrayType);

        for (size_t i = 0; i < decimate_metrics.size(); i++)
            json_decimate_metrics.PushBack(getDecimateMetric(i), a);

        json_project.AddMember(Keys::decimate_metrics, json_decimate_metrics, a);
    }


    rj::Value json_sections(rj::kArrayType);

    for (auto it = sections->cbegin(); it != sections->cend(); it++) {
        rj::Value json_section(rj::kObjectType);
        json_section.AddMember(Keys::Sections::start, it->second.start, a);
        rj::Value json_presets(rj::kArrayType);
        for (size_t i = 0; i < it->second.presets.size(); i++)
            json_presets.PushBack(rj::Value(it->second.presets[i], a), a);
        json_section.AddMember(Keys::Sections::presets, json_presets, a);

        json_sections.PushBack(json_section, a);
    }

    json_project.AddMember(Keys::sections, json_sections, a);


    json_project.AddMember(Keys::source_filter, source_filter, a);


    rj::Value json_interlaced_fades(rj::kArrayType);

    for (auto it = interlaced_fades.cbegin(); it != interlaced_fades.cend(); it++) {
        rj::Value json_interlaced_fade(rj::kObjectType);
        json_interlaced_fade.AddMember(Keys::InterlacedFades::frame, it->second.frame, a);
        json_interlaced_fade.AddMember(Keys::InterlacedFades::field_difference, it->second.field_difference, a);

        json_interlaced_fades.PushBack(json_interlaced_fade, a);
    }

    json_project.AddMember(Keys::interlaced_fades, json_interlaced_fades, a);


    if (is_wobbly) {
        rj::Value json_presets(rj::kArrayType);
        rj::Value json_frozen_frames(rj::kArrayType);

        for (auto it = presets->cbegin(); it != presets->cend(); it++) {
            rj::Value json_preset(rj::kObjectType);
            json_preset.AddMember(Keys::Presets::name, it->second.name, a);
            json_preset.AddMember(Keys::Presets::contents, it->second.contents, a);

            json_presets.PushBack(json_preset, a);
        }

        for (auto it = frozen_frames->cbegin(); it != frozen_frames->cend(); it++) {
            rj::Value json_ff(rj::kArrayType);
            json_ff.PushBack(it->second.first, a);
            json_ff.PushBack(it->second.last, a);
            json_ff.PushBack(it->second.replacement, a);

            json_frozen_frames.PushBack(json_ff, a);
        }

        json_project.AddMember(Keys::presets, json_presets, a);
        json_project.AddMember(Keys::frozen_frames, json_frozen_frames, a);


        rj::Value json_custom_lists(rj::kArrayType);

        const char *list_positions[] = {
            "post source",
            "post field match",
            "post decimate"
        };

        for (size_t i = 0; i < custom_lists->size(); i++) {
            const CustomList &cl = custom_lists->at(i);

            rj::Value json_custom_list(rj::kObjectType);
            json_custom_list.AddMember(Keys::CustomLists::name, cl.name, a);
            json_custom_list.AddMember(Keys::CustomLists::preset, cl.preset, a);
            json_custom_list.AddMember(Keys::CustomLists::position, rj::Value(list_positions[cl.position], a), a);
            rj::Value json_frames(rj::kArrayType);
            for (auto it = cl.ranges->cbegin(); it != cl.ranges->cend(); it++) {
                rj::Value json_pair(rj::kArrayType);
                json_pair.PushBack(it->second.first, a);
                json_pair.PushBack(it->second.last, a);
                json_frames.PushBack(json_pair, a);
            }
            json_custom_list.AddMember(Keys::CustomLists::frames, json_frames, a);

            json_custom_lists.PushBack(json_custom_list, a);
        }

        json_project.AddMember(Keys::custom_lists, json_custom_lists, a);


        if (resize.enabled) {
            rj::Value json_resize(rj::kObjectType);
            json_resize.AddMember(Keys::Resize::width, resize.width, a);
            json_resize.AddMember(Keys::Resize::height, resize.height, a);
            json_resize.AddMember(Keys::Resize::filter, resize.filter, a);
            json_project.AddMember(Keys::resize, json_resize, a);
        }

        if (crop.enabled) {
            rj::Value json_crop(rj::kObjectType);
            json_crop.AddMember(Keys::Crop::early, crop.early, a);
            json_crop.AddMember(Keys::Crop::left, crop.left, a);
            json_crop.AddMember(Keys::Crop::top, crop.top, a);
            json_crop.AddMember(Keys::Crop::right, crop.right, a);
            json_crop.AddMember(Keys::Crop::bottom, crop.bottom, a);
            json_project.AddMember(Keys::crop, json_crop, a);
        }

        if (depth.enabled) {
            rj::Value json_depth(rj::kObjectType);
            json_depth.AddMember(Keys::Depth::bits, depth.bits, a);
            json_depth.AddMember(Keys::Depth::float_samples, depth.float_samples, a);
            json_depth.AddMember(Keys::Depth::dither, depth.dither, a);
            json_project.AddMember(Keys::depth, json_depth, a);
        }
    }

    rj::StringBuffer buffer;

    if (compact_project) {
        rj::Writer<rj::StringBuffer> writer(buffer);
        json_project.Accept(writer);
    } else {
        rj::PrettyWriter<rj::StringBuffer> writer(buffer);
        json_project.Accept(writer);
    }

    if (file.write(buffer.GetString(), buffer.GetSize()) < 0)
        throw WobblyException("Couldn't write the project to file '" + path + "'. Error message: " + file.errorString().toStdString());

    setModified(false);
}


void WobblyProject::readProject(const std::string &path) {
    QFile file(QString::fromStdString(path));

    if (!file.open(QIODevice::ReadOnly))
        throw WobblyException("Couldn't open project file '" + path + "'. Error message: " + file.errorString().toStdString());

    QByteArray file_contents = file.readAll();

    rj::Document json_project;

    rj::ParseResult result = json_project.ParseInsitu(file_contents.data());
    if (result.IsError())
        throw WobblyException("Failed to parse project file '" + path + "' at byte " + std::to_string(result.Offset()) + ": " + rj::GetParseError_En(result.Code()));

    if (!json_project.IsObject())
        throw WobblyException("File '" + path + "' is not a valid Wobbly project: JSON document root is not an object.");

#define CHECK_INT \
    if (!it->value.IsInt()) \
        throw WobblyException(path + ": JSON key '" + it->name.GetString() + "' must be an integer.");

#define CHECK_STRING \
    if (!it->value.IsString()) \
        throw WobblyException(path + ": JSON key '" + it->name.GetString() + "' must be a string.");

#define CHECK_OBJECT \
    if (!it->value.IsObject()) \
        throw WobblyException(path + ": JSON key '" + it->name.GetString() + "' must be an object.");

#define CHECK_ARRAY \
    if (!it->value.IsArray()) \
        throw WobblyException(path + ": JSON key '" + it->name.GetString() + "' must be an array.");

    int project_format_version = 1; // If the key doesn't exist, assume it's version 1 (Wobbly v1).
    rj::Value::ConstMemberIterator it = json_project.FindMember(Keys::project_format_version);
    if (it != json_project.MemberEnd()) {
        CHECK_INT;

        project_format_version = it->value.GetInt();
    }

    if (project_format_version > PROJECT_FORMAT_VERSION)
        throw WobblyException(path + ": the project's format version is " + std::to_string(project_format_version) + ", but this software only understands format version " + std::to_string(PROJECT_FORMAT_VERSION) + " and older. Upgrade the software and try again.");


//    int wobbly_version = 0;
//    it = json_project.FindMember(Keys::wobbly_version);
//    if (it != json_project.MemberEnd()) {
//        if (project_format_version == 1)
//            wobbly_version = std::atoi(it->value.GetString());
//        else
//            wobbly_version = it->value.GetInt();
//    }


    it = json_project.FindMember(Keys::input_file);
    if (it == json_project.MemberEnd())
        throw WobblyException(path + ": JSON key '" + Keys::input_file + "' is missing.");
    CHECK_STRING;
    input_file = it->value.GetString();


    it = json_project.FindMember(Keys::input_frame_rate);
    if (it == json_project.MemberEnd())
        throw WobblyException(path + ": JSON key '" + Keys::input_frame_rate + "' is missing.");
    if (!it->value.IsArray() || it->value.Size() != 2 || !it->value[0].IsInt64() || !it->value[1].IsInt64())
        throw WobblyException(path + ": JSON key '" + Keys::input_frame_rate + "' must be an array of two integers.");
    fps_num = it->value[0].GetInt64();
    fps_den = it->value[1].GetInt64();


    it = json_project.FindMember(Keys::input_resolution);
    if (it == json_project.MemberEnd())
        throw WobblyException(path + ": JSON key '" + Keys::input_resolution + "' is missing.");
    if (!it->value.IsArray() || it->value.Size() != 2 || !it->value[0].IsInt() || !it->value[1].IsInt())
        throw WobblyException(path + ": JSON key '" + Keys::input_resolution + "' must be an array of two integers.");
    width = it->value[0].GetInt();
    height = it->value[1].GetInt();


    setNumFrames(PostSource, 0);

    it = json_project.FindMember(Keys::trim);
    if (it == json_project.MemberEnd())
        throw WobblyException(path + ": JSON key '" + Keys::trim + "' is missing.");
    if (!it->value.IsArray() || it->value.Size() < 1)
        throw WobblyException(path + ": JSON key '" + Keys::trim + "' must be an array with at least one element.");

    const rj::Value &json_trims = it->value;

    for (rj::SizeType i = 0; i < json_trims.Size(); i++) {
        const rj::Value &json_trim = json_trims[i];

        if (!json_trim.IsArray() || json_trim.Size() != 2 || !json_trim[0].IsInt() || !json_trim[1].IsInt())
            throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::trim + "' must be an array of two integers.");

        FrameRange range;
        range.first = json_trim[0].GetInt();
        range.last = json_trim[1].GetInt();
        trims.insert({ range.first, range });
        setNumFrames(PostSource, getNumFrames(PostSource) + (range.last - range.first + 1));
    }

    setNumFrames(PostDecimate, getNumFrames(PostSource));


    it = json_project.FindMember(Keys::source_filter);
    if (it == json_project.MemberEnd())
        throw WobblyException(path + ": JSON key '" + Keys::source_filter + "' is missing.");
    CHECK_STRING;
    source_filter = it->value.GetString();


    it = json_project.FindMember(Keys::user_interface);
    if (it != json_project.MemberEnd()) {
        CHECK_OBJECT;

        const rj::Value &json_ui = it->value;

        zoom = 1;
        it = json_ui.FindMember(Keys::UserInterface::zoom);
        if (it != json_ui.MemberEnd()) {
            CHECK_INT;
            zoom = it->value.GetInt();
        }

        last_visited_frame = 0;
        it = json_ui.FindMember(Keys::UserInterface::last_visited_frame);
        if (it != json_ui.MemberEnd()) {
            CHECK_INT;
            last_visited_frame = it->value.GetInt();
        }

        it = json_ui.FindMember(Keys::UserInterface::state);
        if (it != json_ui.MemberEnd()) {
            CHECK_STRING;
            ui_state = it->value.GetString();
        }

        it = json_ui.FindMember(Keys::UserInterface::geometry);
        if (it != json_ui.MemberEnd()) {
            CHECK_STRING;
            ui_geometry = it->value.GetString();
        }

        shown_frame_rates = { true, false, true, true, true };
        it = json_ui.FindMember(Keys::UserInterface::show_frame_rates);
        if (it != json_ui.MemberEnd()) {
            CHECK_ARRAY;

            const rj::Value &json_rates = it->value;

            int rates[] = { 30, 24, 18, 12, 6 };

            std::unordered_set<int> project_rates;
            for (rj::SizeType i = 0; i < json_rates.Size(); i++) {
                if (!json_rates[i].IsInt())
                    throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::UserInterface::show_frame_rates + "' must be an integer.");

                project_rates.insert(json_rates[i].GetInt());
            }

            for (int i = 0; i < 5; i++)
                shown_frame_rates[i] = (bool)project_rates.count(rates[i]);
        }

        it = json_ui.FindMember(Keys::UserInterface::mic_search_minimum);
        if (it != json_ui.MemberEnd()) {
            CHECK_INT;
            mic_search_minimum = it->value.GetInt();
        }

        it = json_ui.FindMember(Keys::UserInterface::c_match_sequences_minimum);
        if (it != json_ui.MemberEnd()) {
            CHECK_INT;
            c_match_sequences_minimum = it->value.GetInt();
        }

        it = json_ui.FindMember(Keys::UserInterface::pattern_guessing);
        if (it != json_ui.MemberEnd()) {
            CHECK_OBJECT;

            const rj::Value &json_pattern_guessing = it->value;

            pattern_guessing.method = PatternGuessingFromMics;
            it = json_pattern_guessing.FindMember(Keys::UserInterface::PatternGuessing::method);
            if (it != json_pattern_guessing.MemberEnd()) {
                CHECK_STRING;

                std::unordered_map<std::string, int> guessing_methods = {
                    { "from matches", PatternGuessingFromMatches },
                    { "from mics", PatternGuessingFromMics }
                };

                try {
                    pattern_guessing.method = guessing_methods.at(it->value.GetString());
                } catch (std::out_of_range &) {

                }
            }

            it = json_pattern_guessing.FindMember(Keys::UserInterface::PatternGuessing::minimum_length);
            if (it != json_pattern_guessing.MemberEnd()) {
                CHECK_INT;
                pattern_guessing.minimum_length = it->value.GetInt();
            }

            pattern_guessing.third_n_match = UseThirdNMatchNever;
            it = json_pattern_guessing.FindMember(Keys::UserInterface::PatternGuessing::use_third_n_match);
            if (it != json_pattern_guessing.MemberEnd()) {
                CHECK_STRING;

                std::unordered_map<std::string, int> third_n_match = {
                    { "always", UseThirdNMatchAlways },
                    { "never", UseThirdNMatchNever },
                    { "if it has lower mic", UseThirdNMatchIfPrettier }
                };

                try {
                    pattern_guessing.third_n_match = third_n_match.at(it->value.GetString());
                } catch (std::out_of_range &) {

                }
            }

            pattern_guessing.decimation = DropFirstDuplicate;
            it = json_pattern_guessing.FindMember(Keys::UserInterface::PatternGuessing::decimate);
            if (it != json_pattern_guessing.MemberEnd()) {
                CHECK_STRING;

                std::unordered_map<std::string, int> decimate = {
                    { "first duplicate", DropFirstDuplicate },
                    { "second duplicate", DropSecondDuplicate },
                    { "duplicate with higher mic per cycle", DropUglierDuplicatePerCycle },
                    { "duplicate with higher mic per section", DropUglierDuplicatePerSection }
                };

                try {
                    pattern_guessing.decimation = decimate.at(it->value.GetString());
                } catch (std::out_of_range &) {

                }
            }

            it = json_pattern_guessing.FindMember(Keys::UserInterface::PatternGuessing::use_patterns);
            if (it != json_pattern_guessing.MemberEnd()) {
                CHECK_ARRAY;

                std::unordered_map<std::string, int> use_patterns = {
                    { "cccnn", PatternCCCNN },
                    { "ccnnn", PatternCCNNN },
                    { "ccccc", PatternCCCCC }
                };

                pattern_guessing.use_patterns = 0;

                const rj::Value &json_use_patterns = it->value;
                for (rj::SizeType i = 0; i < json_use_patterns.Size(); i++) {
                    if (!json_use_patterns[i].IsString())
                        throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::UserInterface::PatternGuessing::use_patterns + "' must be a string.");

                    pattern_guessing.use_patterns |= use_patterns[json_use_patterns[i].GetString()];
                }
            }

            it = json_pattern_guessing.FindMember(Keys::UserInterface::PatternGuessing::failures);
            if (it != json_pattern_guessing.MemberEnd()) {
                CHECK_ARRAY;

                std::unordered_map<std::string, int> reasons = {
                    { "section too short", SectionTooShort },
                    { "ambiguous pattern", AmbiguousMatchPattern }
                };

                const rj::Value &json_failures = it->value;

                for (rj::SizeType i = 0; i < json_failures.Size(); i++) {
                    const rj::Value &json_failure = json_failures[i];

                    if (!json_failure.IsObject())
                        throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::UserInterface::PatternGuessing::failures + "' must be an object.");

                    it = json_failure.FindMember(Keys::UserInterface::PatternGuessing::Failures::start);
                    if (it == json_failure.MemberEnd() || !it->value.IsInt())
                        throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::UserInterface::PatternGuessing::failures + "' must contain the key '" + Keys::UserInterface::PatternGuessing::Failures::start + "', which must be an integer.");

                    FailedPatternGuessing fail;
                    fail.start = it->value.GetInt();

                    it = json_failure.FindMember(Keys::UserInterface::PatternGuessing::Failures::reason);
                    if (it == json_failure.MemberEnd() || !it->value.IsString())
                        throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::UserInterface::PatternGuessing::failures + "' must contain the key '" + Keys::UserInterface::PatternGuessing::Failures::reason + "', which must be a string.");

                    try {
                        fail.reason = reasons.at(it->value.GetString());
                    } catch (std::out_of_range &) {
                        fail.reason = AmbiguousMatchPattern;
                    }

                    pattern_guessing.failures.insert({ fail.start, fail });
                }
            }
        }

        it = json_ui.FindMember(Keys::UserInterface::bookmarks);
        if (it != json_ui.MemberEnd()) {
            CHECK_ARRAY;

            const rj::Value &json_bookmarks = it->value;

            for (rj::SizeType i = 0; i < json_bookmarks.Size(); i++) {
                const rj::Value &json_bookmark = json_bookmarks[i];

                if (!json_bookmark.IsObject())
                    throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::UserInterface::bookmarks + "' must be an object.");

                it = json_bookmark.FindMember(Keys::UserInterface::Bookmarks::frame);
                if (it == json_bookmark.MemberEnd() || !it->value.IsInt())
                    throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::UserInterface::bookmarks + "' must contain the key '" + Keys::UserInterface::Bookmarks::frame + "', which must be an integer.");

                int bookmark_frame = it->value.GetInt();

                it = json_bookmark.FindMember(Keys::UserInterface::Bookmarks::description);
                if (it == json_bookmark.MemberEnd() || !it->value.IsString())
                    throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::UserInterface::bookmarks + "' must contain the key '" + Keys::UserInterface::Bookmarks::description + "', which must be a string.");

                std::string bookmark_description(it->value.GetString(), it->value.GetStringLength());

                addBookmark(bookmark_frame, bookmark_description);
            }
        }
    }


    it = json_project.FindMember(Keys::vfm_parameters);
    if (it != json_project.MemberEnd()) {
        CHECK_OBJECT;

        std::vector<std::string> valid_parameters = {
            Keys::VFMParameters::blockx,
            Keys::VFMParameters::blocky,
            Keys::VFMParameters::chroma,
            Keys::VFMParameters::cthresh,
            Keys::VFMParameters::mchroma,
            Keys::VFMParameters::mi,
            Keys::VFMParameters::micmatch,
            Keys::VFMParameters::order,
            Keys::VFMParameters::scthresh,
            Keys::VFMParameters::y0,
            Keys::VFMParameters::y1
        };

        const rj::Value &json_vfm_parameters = it->value;

        for (size_t i = 0; i < valid_parameters.size(); i++) {
            it = json_vfm_parameters.FindMember(valid_parameters[i]);

            if (it != json_vfm_parameters.MemberEnd()) {
                if (!it->value.IsNumber())
                    throw WobblyException(path + ": JSON key '" + valid_parameters[i] + "', member of '" + Keys::vfm_parameters + "', must be a number.");

                vfm_parameters.insert({ valid_parameters[i], it->value.GetDouble() });
            }
        }
    }

    it = json_project.FindMember(Keys::vdecimate_parameters);
    if (it != json_project.MemberEnd()) {
        CHECK_OBJECT;

        std::vector<std::string> valid_parameters = {
            Keys::VDecimateParameters::blockx,
            Keys::VDecimateParameters::blocky,
            Keys::VDecimateParameters::chroma,
            Keys::VDecimateParameters::dupthresh,
            Keys::VDecimateParameters::scthresh
        };

        const rj::Value &json_vdecimate_parameters = it->value;

        for (size_t i = 0; i < valid_parameters.size(); i++) {
            it = json_vdecimate_parameters.FindMember(valid_parameters[i]);

            if (it != json_vdecimate_parameters.MemberEnd()) {
                if (!it->value.IsNumber())
                    throw WobblyException(path + ": JSON key '" + valid_parameters[i] + "', member of '" + Keys::vdecimate_parameters + "', must be a number.");

                vdecimate_parameters.insert({ valid_parameters[i], it->value.GetDouble() });
            }
        }
    }


    it = json_project.FindMember(Keys::mics);
    if (it != json_project.MemberEnd()) {
        const rj::Value &json_mics = it->value;

        if (!json_mics.IsArray() || json_mics.Size() != (rj::SizeType)getNumFrames(PostSource))
            throw WobblyException(path + ": JSON key '" + Keys::mics + "' must be an array with exactly " + std::to_string(getNumFrames(PostSource)) + " elements.");

        mics.resize(getNumFrames(PostSource), { 0 });
        for (size_t i = 0; i < mics.size(); i++) {
            const rj::Value &json_mic = json_mics[i];

            if (!json_mic.IsArray() ||
                    json_mic.Size() != 5 ||
                    !json_mic[0].IsInt() ||
                    !json_mic[1].IsInt() ||
                    !json_mic[2].IsInt() ||
                    !json_mic[3].IsInt() ||
                    !json_mic[4].IsInt())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::mics + "' must be an array of exactly 5 integers.");

            for (rj::SizeType j = 0; j < json_mic.Size(); j++)
                mics[i][j] = json_mic[j].GetInt();
        }
    }


    it = json_project.FindMember(Keys::matches);
    if (it != json_project.MemberEnd()) {
        const rj::Value &json_matches = it->value;

        if (!json_matches.IsArray() || json_matches.Size() != (rj::SizeType)getNumFrames(PostSource))
            throw WobblyException(path + ": JSON key '" + Keys::matches + "' must be an array with exactly " + std::to_string(getNumFrames(PostSource)) + " elements.");

        matches.resize(getNumFrames(PostSource), 'c');
        for (size_t i = 0; i < matches.size(); i++) {
            if (!json_matches[i].IsString() || json_matches[i].GetStringLength() != 1)
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::matches + "' must be a string with the length of 1.");

            matches[i] = json_matches[i].GetString()[0];

            if (matches[i] != 'p' &&
                matches[i] != 'c' &&
                matches[i] != 'n' &&
                matches[i] != 'b' &&
                matches[i] != 'u')
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::matches + "' must be one of 'p', 'c', 'n', 'b', or 'u'.");
        }
    }


    it = json_project.FindMember(Keys::original_matches);
    if (it != json_project.MemberEnd()) {
        const rj::Value &json_original_matches = it->value;

        if (!json_original_matches.IsArray() || json_original_matches.Size() != (rj::SizeType)getNumFrames(PostSource))
            throw WobblyException(path + ": JSON key '" + Keys::original_matches + "' must be an array with exactly " + std::to_string(getNumFrames(PostSource)) + " elements.");

        original_matches.resize(getNumFrames(PostSource), 'c');
        for (size_t i = 0; i < original_matches.size(); i++) {
            if (!json_original_matches[i].IsString() || json_original_matches[i].GetStringLength() != 1)
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::original_matches + "' must be a string with the length of 1.");

            original_matches[i] = json_original_matches[i].GetString()[0];

            if (original_matches[i] != 'p' &&
                original_matches[i] != 'c' &&
                original_matches[i] != 'n' &&
                original_matches[i] != 'b' &&
                original_matches[i] != 'u')
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::original_matches + "' must be one of 'p', 'c', 'n', 'b', or 'u'.");
        }
    }


    it = json_project.FindMember(Keys::combed_frames);
    if (it != json_project.MemberEnd()) {
        const rj::Value &json_combed_frames = it->value;

        if (!json_combed_frames.IsArray() || json_combed_frames.Size() > (rj::SizeType)getNumFrames(PostSource))
            throw WobblyException(path + ": JSON key '" + Keys::combed_frames + "' must be an array with at most " + std::to_string(getNumFrames(PostSource)) + " elements.");

        for (rj::SizeType i = 0; i < json_combed_frames.Size(); i++) {
            if (!json_combed_frames[i].IsInt())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::combed_frames + "' must be an integer.");
            addCombedFrame(json_combed_frames[i].GetInt());
        }
    }


    decimated_frames.resize((getNumFrames(PostSource) - 1) / 5 + 1);
    it = json_project.FindMember(Keys::decimated_frames);
    if (it != json_project.MemberEnd()) {
        const rj::Value &json_decimated_frames = it->value;

        if (!json_decimated_frames.IsArray() || json_decimated_frames.Size() > (rj::SizeType)getNumFrames(PostSource))
            throw WobblyException(path + ": JSON key '" + Keys::decimated_frames + "' must be an array with at most " + std::to_string(getNumFrames(PostSource)) + " elements.");

        for (rj::SizeType i = 0; i < json_decimated_frames.Size(); i++) {
            if (!json_decimated_frames[i].IsInt())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::decimated_frames + "' must be an integer.");
            addDecimatedFrame(json_decimated_frames[i].GetInt());
        }
    }

    // getNumFrames(PostDecimate) is correct at this point.

    it = json_project.FindMember(Keys::decimate_metrics);
    if (it != json_project.MemberEnd()) {
        const rj::Value &json_decimate_metrics = it->value;

        if (!json_decimate_metrics.IsArray() || json_decimate_metrics.Size() != (rj::SizeType)getNumFrames(PostSource))
            throw WobblyException(path + ": JSON key '" + Keys::decimate_metrics + "' must be an array with exactly " + std::to_string(getNumFrames(PostSource)) + " elements.");

        decimate_metrics.resize(getNumFrames(PostSource), 0);
        for (size_t i = 0; i < decimate_metrics.size(); i++) {
            if (!json_decimate_metrics[i].IsInt())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::decimate_metrics + "' must be an integer.");
            decimate_metrics[i] = json_decimate_metrics[i].GetInt();
        }
    }


    it = json_project.FindMember(Keys::presets);
    if (it != json_project.MemberEnd()) {
        CHECK_ARRAY;

        const rj::Value &json_presets = it->value;

        for (rj::SizeType i = 0; i < json_presets.Size(); i++) {
            const rj::Value &json_preset = json_presets[i];

            if (!json_preset.IsObject())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::presets + "' must be an object.");

            it = json_preset.FindMember(Keys::Presets::name);
            if (it == json_preset.MemberEnd() || !it->value.IsString())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::presets + "' must contain the key '" + Keys::Presets::name + "', which must be a string.");

            const char *preset_name = it->value.GetString();

            it = json_preset.FindMember(Keys::Presets::contents);
            if (it == json_preset.MemberEnd() || !it->value.IsString())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::presets + "' must contain the key '" + Keys::Presets::contents + "', which must be a string.");

            const char *preset_contents = it->value.GetString();

            addPreset(preset_name, preset_contents);
        }
    }


    it = json_project.FindMember(Keys::frozen_frames);
    if (it != json_project.MemberEnd()) {
        CHECK_ARRAY;

        const rj::Value &json_frozen_frames = it->value;

        for (rj::SizeType i = 0; i < json_frozen_frames.Size(); i++) {
            const rj::Value &json_ff = json_frozen_frames[i];

            if (!json_ff.IsArray() || json_ff.Size() != 3 || !json_ff[0].IsInt() || !json_ff[1].IsInt() || !json_ff[2].IsInt())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::frozen_frames + "' must be an array of three integers.");

            addFreezeFrame(json_ff[0].GetInt(), json_ff[1].GetInt(), json_ff[2].GetInt());
        }
    }


    it = json_project.FindMember(Keys::sections);
    if (it != json_project.MemberEnd()) {
        CHECK_ARRAY;

        const rj::Value &json_sections = it->value;

        for (rj::SizeType i = 0; i < json_sections.Size(); i++) {
            const rj::Value &json_section = json_sections[i];

            if (!json_section.IsObject())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::sections + "' must be an object.");

            it = json_section.FindMember(Keys::Sections::start);
            if (it == json_section.MemberEnd() || !it->value.IsInt())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::sections + "' must contain the key '" + Keys::Sections::start + "', which must be an integer.");

            int section_start = it->value.GetInt();
            Section section(section_start);

            it = json_section.FindMember(Keys::Sections::presets);
            if (it != json_section.MemberEnd()) {
                if (!it->value.IsArray())
                    throw WobblyException(path + ": JSON key '" + Keys::Sections::presets + "', member of element number " + std::to_string(i) + " of JSON key '" + Keys::sections + "', must be an array.");

                const rj::Value &json_presets = it->value;
                section.presets.resize(json_presets.Size());
                for (rj::SizeType k = 0; k < json_presets.Size(); k++) {
                    if (!json_presets[k].IsString())
                        throw WobblyException(path + ": element number " + std::to_string(k) + " of JSON key '" + Keys::Sections::presets + "', part of element number " + std::to_string(i) + " of key '" + Keys::sections + "', must be a string.");
                    section.presets[k] = json_presets[k].GetString();
                }
            }

            addSection(section);
        }

        if (json_sections.Size() == 0)
            addSection(0);
    }

    it = json_project.FindMember(Keys::custom_lists);
    if (it != json_project.MemberEnd()) {
        CHECK_ARRAY;

        const rj::Value &json_custom_lists = it->value;

        custom_lists->reserve(json_custom_lists.Size());

        std::unordered_map<std::string, int> list_positions = {
            { "post source", PostSource },
            { "post field match", PostFieldMatch },
            { "post decimate", PostDecimate }
        };

        for (rj::SizeType i = 0; i < json_custom_lists.Size(); i++) {
            const rj::Value &json_list = json_custom_lists[i];

            if (!json_list.IsObject())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::custom_lists + "' must be an object.");

            it = json_list.FindMember(Keys::CustomLists::name);
            if (it == json_list.MemberEnd() || !it->value.IsString())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::custom_lists + "' must contain the key '" + Keys::CustomLists::name + "', which must be a string.");

            const char *list_name = it->value.GetString();

            std::string list_preset;

            it = json_list.FindMember(Keys::CustomLists::preset);
            if (it != json_list.MemberEnd()) {
                if (!it->value.IsString())
                    throw WobblyException(path + ": JSON key '" + Keys::CustomLists::preset + "', member of element number " + std::to_string(i) + " of JSON key '" + Keys::custom_lists + "', must be a string.");

                list_preset = it->value.GetString();
            }

            it = json_list.FindMember(Keys::CustomLists::position);

            int list_position = PostSource;

            if (project_format_version == 1) {
                if (it == json_list.MemberEnd() || !it->value.IsInt())
                    throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::custom_lists + "' must contain the key '" + Keys::CustomLists::position + "', which must be an integer.");

                list_position = it->value.GetInt();
            } else {
                if (it == json_list.MemberEnd() || !it->value.IsString())
                    throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::custom_lists + "' must contain the key '" + Keys::CustomLists::position + "', which must be a string.");

                try {
                    list_position = list_positions.at(it->value.GetString());
                } catch (std::out_of_range &) {

                }
            }

            addCustomList(CustomList(list_name, list_preset, list_position));

            it = json_list.FindMember(Keys::CustomLists::frames);
            if (it != json_list.MemberEnd()) {
                const rj::Value &json_frames = it->value;
                if (!json_frames.IsArray())
                    throw WobblyException(path + ": JSON key '" + Keys::CustomLists::frames + "', member of element number " + std::to_string(i) + " of JSON key '" + Keys::custom_lists + "', must be an array.");

                for (rj::SizeType j = 0; j < json_frames.Size(); j++) {
                    const rj::Value &json_range = json_frames[j];

                    if (!json_range.IsArray() || json_range.Size() != 2 || !json_range[0].IsInt() || !json_range[1].IsInt())
                        throw WobblyException(path + ": element number " + std::to_string(j) + " of JSON key '" + Keys::CustomLists::frames + "', member of element number " + std::to_string(i) + " of JSON key '" + Keys::custom_lists + "', must be an array of two integers.");

                    addCustomListRange(i, json_range[0].GetInt(), json_range[1].GetInt());
                }
            }
        }
    }


    it = json_project.FindMember(Keys::resize);
    if (it != json_project.MemberEnd()) {
        CHECK_OBJECT;

        const rj::Value &json_resize = it->value;

        resize.enabled = true;

        it = json_resize.FindMember(Keys::Resize::width);
        if (it == json_resize.MemberEnd() || !it->value.IsInt())
            throw WobblyException(path + ": JSON key '" + Keys::resize + "' must contain the key '" + Keys::Resize::width + "', which must be an integer.");

        resize.width = it->value.GetInt();

        it = json_resize.FindMember(Keys::Resize::height);
        if (it == json_resize.MemberEnd() || !it->value.IsInt())
            throw WobblyException(path + ": JSON key '" + Keys::resize + "' must contain the key '" + Keys::Resize::height + "', which must be an integer.");

        resize.height = it->value.GetInt();

        it = json_resize.FindMember(Keys::Resize::filter);
        if (it == json_resize.MemberEnd() || !it->value.IsString())
            throw WobblyException(path + ": JSON key '" + Keys::resize + "' must contain the key '" + Keys::Resize::filter + "', which must be a string.");

        resize.filter = it->value.GetString();
    } else {
        resize.enabled = false;
        resize.width = width;
        resize.height = height;
    }

    it = json_project.FindMember(Keys::crop);
    if (it != json_project.MemberEnd()) {
        CHECK_OBJECT;

        const rj::Value &json_crop = it->value;

        crop.enabled = true;

        it = json_crop.FindMember(Keys::Crop::early);
        if (it == json_crop.MemberEnd() || !it->value.IsBool())
            throw WobblyException(path + ": JSON key '" + Keys::crop + "' must contain the key '" + Keys::Crop::early + "', which must be a boolean.");

        crop.early = it->value.GetBool();

        it = json_crop.FindMember(Keys::Crop::left);
        if (it == json_crop.MemberEnd() || !it->value.IsInt())
            throw WobblyException(path + ": JSON key '" + Keys::crop + "' must contain the key '" + Keys::Crop::left + "', which must be an integer.");

        crop.left = it->value.GetInt();

        it = json_crop.FindMember(Keys::Crop::top);
        if (it == json_crop.MemberEnd() || !it->value.IsInt())
            throw WobblyException(path + ": JSON key '" + Keys::crop + "' must contain the key '" + Keys::Crop::top + "', which must be an integer.");

        crop.top = it->value.GetInt();

        it = json_crop.FindMember(Keys::Crop::right);
        if (it == json_crop.MemberEnd() || !it->value.IsInt())
            throw WobblyException(path + ": JSON key '" + Keys::crop + "' must contain the key '" + Keys::Crop::right + "', which must be an integer.");

        crop.right = it->value.GetInt();

        it = json_crop.FindMember(Keys::Crop::bottom);
        if (it == json_crop.MemberEnd() || !it->value.IsInt())
            throw WobblyException(path + ": JSON key '" + Keys::crop + "' must contain the key '" + Keys::Crop::bottom + "', which must be an integer.");

        crop.bottom = it->value.GetInt();
    } else {
        crop.enabled = false;
    }

    it = json_project.FindMember(Keys::depth);
    if (it != json_project.MemberEnd()) {
        CHECK_OBJECT;

        const rj::Value &json_depth = it->value;

        depth.enabled = true;

        it = json_depth.FindMember(Keys::Depth::bits);
        if (it == json_depth.MemberEnd() || !it->value.IsInt())
            throw WobblyException(path + ": JSON key '" + Keys::depth + "' must contain the key '" + Keys::Depth::bits + "', which must be an integer.");

        depth.bits = it->value.GetInt();

        it = json_depth.FindMember(Keys::Depth::float_samples);
        if (it == json_depth.MemberEnd() || !it->value.IsBool())
            throw WobblyException(path + ": JSON key '" + Keys::depth + "' must contain the key '" + Keys::Depth::float_samples + "', which must be a boolean.");

        depth.float_samples = it->value.GetBool();

        it = json_depth.FindMember(Keys::Depth::dither);
        if (it == json_depth.MemberEnd() || !it->value.IsString())
            throw WobblyException(path + ": JSON key '" + Keys::depth + "' must contain the key '" + Keys::Depth::dither + "', which must be a string.");

        depth.dither = it->value.GetString();
    } else {
        depth.enabled = false;
    }


    it = json_project.FindMember(Keys::interlaced_fades);
    if (it != json_project.MemberEnd()) {
        CHECK_ARRAY;

        const rj::Value &json_interlaced_fades = it->value;

        for (rj::SizeType i = 0; i < json_interlaced_fades.Size(); i++) {
            const rj::Value &json_interlaced_fade = json_interlaced_fades[i];

            if (!json_interlaced_fade.IsObject())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::interlaced_fades + "' must be an object.");

            it = json_interlaced_fade.FindMember(Keys::InterlacedFades::frame);
            if (it == json_interlaced_fade.MemberEnd() || !it->value.IsInt())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::interlaced_fades + "' must contain the key '" + Keys::InterlacedFades::frame + "', which must be an integer.");

            int frame = it->value.GetInt();

            it = json_interlaced_fade.FindMember(Keys::InterlacedFades::field_difference);
            if (it == json_interlaced_fade.MemberEnd() || !it->value.IsNumber())
                throw WobblyException(path + ": element number " + std::to_string(i) + " of JSON key '" + Keys::interlaced_fades + "' must contain the key '" + Keys::InterlacedFades::field_difference + "', which must be a number.");

            double field_difference = it->value.GetDouble();

            interlaced_fades.insert({ frame, { frame, field_difference } });
        }
    }

    setModified(false);
}


void WobblyProject::addFreezeFrame(int first, int last, int replacement) {
    if (first > last)
        std::swap(first, last);

    if (first < 0 || first >= getNumFrames(PostSource) ||
        last < 0 || last >= getNumFrames(PostSource) ||
        replacement < 0 || replacement >= getNumFrames(PostSource))
        throw WobblyException("Can't add FreezeFrame (" + std::to_string(first) + "," + std::to_string(last) + "," + std::to_string(replacement) + "): values out of range.");

    const FreezeFrame *overlap = findFreezeFrame(first);
    if (!overlap)
        overlap = findFreezeFrame(last);
    if (!overlap) {
        auto it = frozen_frames->upper_bound(first);
        if (it != frozen_frames->cend() && it->second.first < last)
            overlap = &it->second;
    }

    if (overlap)
        throw WobblyException("Can't add FreezeFrame (" + std::to_string(first) + "," + std::to_string(last) + "," + std::to_string(replacement) + "): overlaps (" + std::to_string(overlap->first) + "," + std::to_string(overlap->last) + "," + std::to_string(overlap->replacement) + ").");

    FreezeFrame ff = {
        .first = first,
        .last = last,
        .replacement = replacement
    };
    frozen_frames->insert(std::make_pair(first, ff));

    setModified(true);
}


void WobblyProject::deleteFreezeFrame(int frame) {
    frozen_frames->erase(frame);

    setModified(true);
}


const FreezeFrame *WobblyProject::findFreezeFrame(int frame) const {
    if (!frozen_frames->size())
        return nullptr;

    auto it = frozen_frames->upper_bound(frame);

    it--;

    if (it->second.first <= frame && frame <= it->second.last)
        return &it->second;

    return nullptr;
}


FrozenFramesModel *WobblyProject::getFrozenFramesModel() {
    return frozen_frames;
}


void WobblyProject::addPreset(const std::string &preset_name) {
    addPreset(preset_name, "");
}


bool WobblyProject::isNameSafeForPython(const std::string &name) const {
    for (size_t i = 0; i < name.size(); i++) {
        const char &c = name[i];

        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (i && c >= '0' && c <= '9') || c == '_'))
            return false;
    }

    return true;
}


void WobblyProject::addPreset(const std::string &preset_name, const std::string &preset_contents) {
    if (!isNameSafeForPython(preset_name))
        throw WobblyException("Can't add preset '" + preset_name + "': name is invalid. Use only letters, numbers, and the underscore character. The first character cannot be a number.");

    if (presetExists(preset_name))
        throw WobblyException("Can't add preset '" + preset_name + "': preset name already in use.");

    Preset preset;
    preset.name = preset_name;
    preset.contents = preset_contents;
    presets->insert(std::make_pair(preset_name, preset));

    setModified(true);
}


void WobblyProject::renamePreset(const std::string &old_name, const std::string &new_name) {
    if (old_name == new_name)
        return;

    if (!presets->count(old_name))
        throw WobblyException("Can't rename preset '" + old_name + "' to '" + new_name + "': no such preset.");

    if (!isNameSafeForPython(new_name))
        throw WobblyException("Can't rename preset '" + old_name + "' to '" + new_name + "': new name is invalid. Use only letters, numbers, and the underscore character. The first character cannot be a number.");

    if (presetExists(new_name))
        throw WobblyException("Can't rename preset '" + old_name + "' to '" + new_name + "': preset '" + new_name + "' already exists.");

    Preset preset;
    preset.name = new_name;
    preset.contents = getPresetContents(old_name);

    presets->erase(old_name);
    presets->insert(std::make_pair(new_name, preset));

    for (auto it = sections->cbegin(); it != sections->cend(); it++)
        for (size_t j = 0; j < it->second.presets.size(); j++)
            if (it->second.presets[j] == old_name)
                sections->setSectionPresetName(it->second.start, j, new_name);

    for (size_t i = 0; i < custom_lists->size(); i++)
        if (custom_lists->at(i).preset == old_name)
            custom_lists->setCustomListPreset(i, new_name);

    setModified(true);
}


void WobblyProject::deletePreset(const std::string &preset_name) {
    if (!presetExists(preset_name))
        throw WobblyException("Can't delete preset '" + preset_name + "': no such preset.");

    presets->erase(preset_name);

    for (auto it = sections->cbegin(); it != sections->cend(); it++)
        for (size_t j = 0; j < it->second.presets.size(); j++)
            if (it->second.presets[j] == preset_name)
                sections->deleteSectionPreset(it->second.start, j);

    for (size_t i = 0; i < custom_lists->size(); i++)
        if (custom_lists->at(i).preset == preset_name)
            custom_lists->setCustomListPreset(i, "");

    setModified(true);
}


const std::string &WobblyProject::getPresetContents(const std::string &preset_name) const {
    if (!presets->count(preset_name))
        throw WobblyException("Can't retrieve the contents of preset '" + preset_name + "': no such preset.");

    const Preset &preset = presets->at(preset_name);
    return preset.contents;
}


void WobblyProject::setPresetContents(const std::string &preset_name, const std::string &preset_contents) {
    if (!presets->count(preset_name))
        throw WobblyException("Can't modify the contents of preset '" + preset_name + "': no such preset.");

    Preset &preset = presets->at(preset_name);
    if (preset.contents != preset_contents) {
        preset.contents = preset_contents;

        setModified(true);
    }
}


bool WobblyProject::isPresetInUse(const std::string &preset_name) const {
    if (!presets->count(preset_name))
        throw WobblyException("Can't check if preset '" + preset_name + "' is in use: no such preset.");

    for (auto it = sections->cbegin(); it != sections->cend(); it++)
        for (size_t j = 0; j < it->second.presets.size(); j++)
            if (it->second.presets[j] == preset_name)
                return true;

    for (auto it = custom_lists->cbegin(); it != custom_lists->cend(); it++)
        if (it->preset == preset_name)
            return true;

    return false;
}


bool WobblyProject::presetExists(const std::string &preset_name) const {
    return (bool)presets->count(preset_name);
}


PresetsModel *WobblyProject::getPresetsModel() {
    return presets;
}


void WobblyProject::addTrim(int trim_start, int trim_end) {
    if (trim_start > trim_end)
        std::swap(trim_start, trim_end);

    trims.insert({ trim_start, { trim_start, trim_end } });
}


void WobblyProject::setVFMParameter(const std::string &name, double value) {
    vfm_parameters[name] = value;
}


void WobblyProject::setVDecimateParameter(const std::string &name, double value) {
    vdecimate_parameters[name] = value;
}


std::array<int16_t, 5> WobblyProject::getMics(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't get the mics for frame " + std::to_string(frame) + ": frame number out of range.");

    if (mics.size())
        return mics[frame];
    else
        return { 0, 0, 0, 0, 0 };
}


void WobblyProject::setMics(int frame, int16_t mic_p, int16_t mic_c, int16_t mic_n, int16_t mic_b, int16_t mic_u) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't set the mics for frame " + std::to_string(frame) + ": frame number out of range.");

    if (!mics.size())
        mics.resize(getNumFrames(PostSource), { 0 });

    auto &mic = mics[frame];
    mic[0] = mic_p;
    mic[1] = mic_c;
    mic[2] = mic_n;
    mic[3] = mic_b;
    mic[4] = mic_u;
}


int WobblyProject::getPreviousFrameWithMic(int minimum, int start_frame) const {
    if (start_frame < 0 || start_frame >= getNumFrames(PostSource))
        throw WobblyException("Can't get the previous frame with mic " + std::to_string(minimum) + " or greater: frame " + std::to_string(start_frame) + " is out of range.");

    for (int i = start_frame - 1; i >= 0; i--) {
        int index = matchCharToIndex(getMatch(i));
        int16_t mic = getMics(i)[index];

        if (mic >= minimum)
            return i;
    }

    return -1;
}


int WobblyProject::getNextFrameWithMic(int minimum, int start_frame) const {
    if (start_frame < 0 || start_frame >= getNumFrames(PostSource))
        throw WobblyException("Can't get the next frame with mic " + std::to_string(minimum) + " or greater: frame " + std::to_string(start_frame) + " is out of range.");

    for (int i = start_frame + 1; i < getNumFrames(PostSource); i++) {
        int index = matchCharToIndex(getMatch(i));
        int16_t mic = getMics(i)[index];

        if (mic >= minimum)
            return i;
    }

    return -1;
}


char WobblyProject::getOriginalMatch(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't get the original match for frame " + std::to_string(frame) + ": frame number out of range.");

    if (original_matches.size())
        return original_matches[frame];
    else
        return 'c';
}


void WobblyProject::setOriginalMatch(int frame, char match) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't set the original match for frame " + std::to_string(frame) + ": frame number out of range.");

    if (match != 'p' && match != 'c' && match != 'n' && match != 'b' && match != 'u')
        throw WobblyException("Can't set the original match for frame " + std::to_string(frame) + ": '" + match + "' is not a valid match character.");

    if (!original_matches.size())
        original_matches.resize(getNumFrames(PostSource), 'c');

    original_matches[frame] = match;
}


char WobblyProject::getMatch(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't get the match for frame " + std::to_string(frame) + ": frame number out of range.");

    if (matches.size())
        return matches[frame];
    else if (original_matches.size())
        return original_matches[frame];
    else
        return 'c';
}


void WobblyProject::setMatch(int frame, char match) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't set the match for frame " + std::to_string(frame) + ": frame number out of range.");

    if (match != 'p' && match != 'c' && match != 'n' && match != 'b' && match != 'u')
        throw WobblyException("Can't set the match for frame " + std::to_string(frame) + ": '" + match + "' is not a valid match character.");

    if (!matches.size())
        matches.resize(getNumFrames(PostSource), 'c');

    matches[frame] = match;
}


void WobblyProject::cycleMatchBCN(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't cycle the match for frame " + std::to_string(frame) + ": frame number out of range.");

    // N -> C -> B. This is the order Yatta uses, so we use it.

    char match = getMatch(frame);

    if (match == 'n')
        match = 'c';
    else if (match == 'c') {
        if (frame == 0)
            match = 'n';
        else
            match = 'b';
    } else if (match == 'b') {
        if (frame == getNumFrames(PostSource) - 1)
            match = 'c';
        else
            match = 'n';
    }

    setMatch(frame, match);

    setModified(true);
}


void WobblyProject::cycleMatch(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't cycle the match for frame " + std::to_string(frame) + ": frame number out of range.");

    // U -> B -> N -> C -> P

    char match = getMatch(frame);

    if (match == 'u') {
        if (frame == 0)
            match = 'n';
        else
            match = 'b';
    } else if (match == 'b') {
        if (frame == getNumFrames(PostSource) - 1)
            match = 'c';
        else
            match = 'n';
    } else if (match == 'n') {
        match = 'c';
    } else if (match == 'c') {
        if (frame == 0)
            match = 'u';
        else
            match = 'p';
    } else if (match == 'p') {
        if (frame == getNumFrames(PostSource) - 1)
            match = 'b';
        else
            match = 'u';
    }

    setMatch(frame, match);

    setModified(true);
}


void WobblyProject::addSection(int section_start) {
    Section section(section_start);
    addSection(section);
}


void WobblyProject::addSection(const Section &section) {
    if (section.start < 0 || section.start >= getNumFrames(PostSource))
        throw WobblyException("Can't add section starting at " + std::to_string(section.start) + ": value out of range.");

    sections->insert(std::make_pair(section.start, section));

    setModified(true);
}


void WobblyProject::deleteSection(int section_start) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't delete section starting at " + std::to_string(section_start) + ": value out of range.");

    if (!sections->count(section_start))
        throw WobblyException("Can't delete section starting at " + std::to_string(section_start) + ": no such section.");

    // Never delete the very first section.
    if (section_start > 0)
        sections->erase(section_start);

    setModified(true);
}


const Section *WobblyProject::findSection(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't find the section frame " + std::to_string(frame) + " belongs to: frame number out of range.");

    auto it = sections->upper_bound(frame);
    it--;
    return &it->second;
}


const Section *WobblyProject::findNextSection(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't find the section after frame " + std::to_string(frame) + ": frame number out of range.");

    auto it = sections->upper_bound(frame);

    if (it != sections->cend())
        return &it->second;

    return nullptr;
}


int WobblyProject::getSectionEnd(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't find the end of the section frame " + std::to_string(frame) + " belongs to: frame number out of range.");

    const Section *next_section = findNextSection(frame);
    if (next_section)
        return next_section->start;
    else
        return getNumFrames(PostSource);
}


void WobblyProject::setSectionPreset(int section_start, const std::string &preset_name) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't add preset '" + preset_name + "' to section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections->count(section_start))
        throw WobblyException("Can't add preset '" + preset_name + "' to section starting at " + std::to_string(section_start) + ": no such section.");

    if (!presets->count(preset_name))
        throw WobblyException("Can't add preset '" + preset_name + "' to section starting at " + std::to_string(section_start) + ": no such preset.");

    // The user may want to assign the same preset twice.
    sections->appendSectionPreset(section_start, preset_name);

    setModified(true);
}


void WobblyProject::deleteSectionPreset(int section_start, size_t preset_index) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't delete preset number " + std::to_string(preset_index) + " from section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections->count(section_start))
        throw WobblyException("Can't delete preset number " + std::to_string(preset_index) + " from section starting at " + std::to_string(section_start) + ": no such section.");

    sections->deleteSectionPreset(section_start, preset_index);

    setModified(true);
}


void WobblyProject::moveSectionPresetUp(int section_start, size_t preset_index) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't move up preset number " + std::to_string(preset_index) + " from section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections->count(section_start))
        throw WobblyException("Can't move up preset number " + std::to_string(preset_index) + " from section starting at " + std::to_string(section_start) + ": no such section.");

    sections->moveSectionPresetUp(section_start, preset_index);

    setModified(true);
}


void WobblyProject::moveSectionPresetDown(int section_start, size_t preset_index) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't move down preset number " + std::to_string(preset_index) + " from section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections->count(section_start))
        throw WobblyException("Can't move down preset number " + std::to_string(preset_index) + " from section starting at " + std::to_string(section_start) + ": no such section.");

    sections->moveSectionPresetDown(section_start, preset_index);

    setModified(true);
}


void WobblyProject::setSectionMatchesFromPattern(int section_start, const std::string &pattern) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't apply match pattern to section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections->count(section_start))
        throw WobblyException("Can't apply match pattern to section starting at " + std::to_string(section_start) + ": no such section.");

    int section_end = getSectionEnd(section_start);

    setRangeMatchesFromPattern(section_start, section_end - 1, pattern);

    setModified(true);
}


void WobblyProject::setSectionDecimationFromPattern(int section_start, const std::string &pattern) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't apply decimation pattern to section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections->count(section_start))
        throw WobblyException("Can't apply decimation pattern to section starting at " + std::to_string(section_start) + ": no such section.");

    int section_end = getSectionEnd(section_start);

    setRangeDecimationFromPattern(section_start, section_end - 1, pattern);

    setModified(true);
}


SectionsModel *WobblyProject::getSectionsModel() {
    return sections;
}


void WobblyProject::setRangeMatchesFromPattern(int range_start, int range_end, const std::string &pattern) {
    if (range_start > range_end)
        std::swap(range_start, range_end);

    if (range_start < 0 || range_end >= getNumFrames(PostSource))
        throw WobblyException("Can't apply match pattern to frames [" + std::to_string(range_start) + "," + std::to_string(range_end) + "]: frame numbers out of range.");

    for (int i = range_start; i <= range_end; i++) {
        if ((i == 0 && (pattern[i % 5] == 'p' || pattern[i % 5] == 'b')) ||
            (i == getNumFrames(PostSource) - 1 && (pattern[i % 5] == 'n' || pattern[i % 5] == 'u')))
            // Skip the first and last frame if their new matches are incompatible.
            continue;

        setMatch(i, pattern[i % 5]);
    }

    setModified(true);
}


void WobblyProject::setRangeDecimationFromPattern(int range_start, int range_end, const std::string &pattern) {
    if (range_start > range_end)
        std::swap(range_start, range_end);

    if (range_start < 0 || range_end >= getNumFrames(PostSource))
        throw WobblyException("Can't apply decimation pattern to frames [" + std::to_string(range_start) + "," + std::to_string(range_end) + "]: frame numbers out of range.");

    for (int i = range_start; i <= range_end; i++) {
        if (pattern[i % 5] == 'd')
            addDecimatedFrame(i);
        else
            deleteDecimatedFrame(i);
    }

    setModified(true);
}


void WobblyProject::resetRangeMatches(int start, int end) {
    if (start > end)
        std::swap(start, end);

    if (start < 0 || end >= getNumFrames(PostSource))
        throw WobblyException("Can't reset the matches for frames [" + std::to_string(start) + "," + std::to_string(end) + "]: values out of range.");

    if (!matches.size())
        matches.resize(getNumFrames(PostSource), 'c');

    if (original_matches.size())
        memcpy(matches.data() + start, original_matches.data() + start, end - start + 1);
    else
        memset(matches.data() + start, 'c', end - start + 1);

    setModified(true);
}


void WobblyProject::resetSectionMatches(int section_start) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't reset the matches for section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections->count(section_start))
        throw WobblyException("Can't reset the matches for section starting at " + std::to_string(section_start) + ": no such section.");

    int section_end = getSectionEnd(section_start);

    resetRangeMatches(section_start, section_end - 1);

    setModified(true);
}


void WobblyProject::addCustomList(const std::string &list_name) {
    CustomList list(list_name);
    addCustomList(list);
}


void WobblyProject::addCustomList(const CustomList &list) {
    if (list.position < 0 || list.position >= 3)
        throw WobblyException("Can't add custom list '" + list.name + "' with position " + std::to_string(list.position) + ": position out of range.");

    if (!isNameSafeForPython(list.name))
        throw WobblyException("Can't add custom list '" + list.name + "': name is invalid. Use only letters, numbers, and the underscore character. The first character cannot be a number.");

    if (list.preset.size() && presets->count(list.preset) == 0)
        throw WobblyException("Can't add custom list '" + list.name + "' with preset '" + list.preset + "': no such preset.");

    for (size_t i = 0; i < custom_lists->size(); i++)
        if (custom_lists->at(i).name == list.name)
            throw WobblyException("Can't add custom list '" + list.name + "': a list with this name already exists.");

    custom_lists->push_back(list);

    setModified(true);
}


void WobblyProject::renameCustomList(const std::string &old_name, const std::string &new_name) {
    if (old_name == new_name)
        return;

    size_t index = custom_lists->size();

    for (size_t i = 0; i < custom_lists->size(); i++)
        if (custom_lists->at(i).name == old_name) {
            index = i;
            break;
        }

    if (index == custom_lists->size())
        throw WobblyException("Can't rename custom list '" + old_name + "': no such list.");

    for (size_t i = 0; i < custom_lists->size(); i++)
        if (custom_lists->at(i).name == new_name)
            throw WobblyException("Can't rename custom list '" + old_name + "' to '" + new_name + "': new name is already in use.");

    if (!isNameSafeForPython(new_name))
        throw WobblyException("Can't rename custom list '" + old_name + "' to '" + new_name + "': new name is invalid. Use only letters, numbers, and the underscore character. The first character cannot be a number.");

    custom_lists->setCustomListName(index, new_name);

    setModified(true);
}


void WobblyProject::deleteCustomList(const std::string &list_name) {
    for (size_t i = 0; i < custom_lists->size(); i++)
        if (custom_lists->at(i).name == list_name) {
            deleteCustomList(i);
            return;
        }

    throw WobblyException("Can't delete custom list with name '" + list_name + "': no such list.");
}


void WobblyProject::deleteCustomList(int list_index) {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't delete custom list with index " + std::to_string(list_index) + ": index out of range.");

    custom_lists->erase(list_index);

    setModified(true);
}


void WobblyProject::moveCustomListUp(int list_index) {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't move up custom list with index " + std::to_string(list_index) + ": index out of range.");

    if (list_index == 0)
        return;

    custom_lists->moveCustomListUp(list_index);

    setModified(true);
}


void WobblyProject::moveCustomListDown(int list_index) {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't move down custom list with index " + std::to_string(list_index) + ": index out of range.");

    if (list_index == (int)custom_lists->size() - 1)
        return;

    custom_lists->moveCustomListDown(list_index);

    setModified(true);
}


const std::string &WobblyProject::getCustomListPreset(int list_index) const {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't get the preset for the custom list with index " + std::to_string(list_index) + ": index out of range.");

    return custom_lists->at(list_index).preset;
}


void WobblyProject::setCustomListPreset(int list_index, const std::string &preset_name) {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't assign preset '" + preset_name + "' to custom list with index " + std::to_string(list_index) + ": index out of range.");

    const CustomList &cl = custom_lists->at(list_index);

    if (!presets->count(preset_name))
        throw WobblyException("Can't assign preset '" + preset_name + "' to custom list '" + cl.name + "': no such preset.");

    custom_lists->setCustomListPreset(list_index, preset_name);

    setModified(true);
}


PositionInFilterChain WobblyProject::getCustomListPosition(int list_index) const {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't get the position for the custom list with index " + std::to_string(list_index) + ": index out of range.");

    return (PositionInFilterChain)custom_lists->at(list_index).position;
}


void WobblyProject::setCustomListPosition(int list_index, PositionInFilterChain position) {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't set the position of the custom list with index " + std::to_string(list_index) + ": index out of range.");

    const CustomList &cl = custom_lists->at(list_index);

    if (position < 0 || position > 2)
        throw WobblyException("Can't put custom list '" + cl.name + "' in position " + std::to_string(position) + ": position out of range.");

    custom_lists->setCustomListPosition(list_index, position);

    setModified(true);
}


void WobblyProject::addCustomListRange(int list_index, int first, int last) {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't add a new range to custom list with index " + std::to_string(list_index) + ": index out of range.");

    CustomList &cl = custom_lists->at(list_index);

    if (first < 0 || first >= getNumFrames(PostSource) ||
        last < 0 || last >= getNumFrames(PostSource))
        throw WobblyException("Can't add range (" + std::to_string(first) + "," + std::to_string(last) + ") to custom list '" + cl.name + "': values out of range.");

    auto &ranges = cl.ranges;

    if (first > last)
        std::swap(first, last);

    const FrameRange *overlap = findCustomListRange(list_index, first);
    if (!overlap)
        overlap = findCustomListRange(list_index, last);
    if (!overlap) {
        auto it = ranges->upper_bound(first);
        if (it != ranges->cend() && it->second.first < last)
            overlap = &it->second;
    }

    if (overlap)
        throw WobblyException("Can't add range (" + std::to_string(first) + "," + std::to_string(last) + ") to custom list '" + cl.name + "': overlaps range (" + std::to_string(overlap->first) + "," + std::to_string(overlap->last) + ").");

    ranges->insert({ first, { first, last } });

    setModified(true);
}


void WobblyProject::deleteCustomListRange(int list_index, int first) {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't delete a range from custom list with index " + std::to_string(list_index) + ": index out of range.");

    CustomList &cl = custom_lists->at(list_index);

    auto &ranges = cl.ranges;

    if (!ranges->count(first))
        throw WobblyException("Can't delete range starting at frame " + std::to_string(first) + " from custom list '" + cl.name + "': no such range.");

    ranges->erase(first);

    setModified(true);
}


const FrameRange *WobblyProject::findCustomListRange(int list_index, int frame) const {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't find a range in custom list with index " + std::to_string(list_index) + ": index out of range.");

    const auto &ranges = custom_lists->at(list_index).ranges;

    if (!ranges->size())
        return nullptr;

    auto it = ranges->upper_bound(frame);

    if (it == ranges->cbegin())
        return nullptr;

    it--;

    if (it->second.first <= frame && frame <= it->second.last)
        return &it->second;

    return nullptr;
}


bool WobblyProject::customListExists(const std::string &list_name) const {
    for (size_t i = 0; i < custom_lists->size(); i++)
        if (custom_lists->at(i).name == list_name)
            return true;

    return false;
}


bool WobblyProject::isCustomListInUse(int list_index) {
    if (list_index < 0 || list_index >= (int)custom_lists->size())
        throw WobblyException("Can't determine if custom list with index " + std::to_string(list_index) + "is in use: index out of range.");

    const CustomList &list = custom_lists->at(list_index);

    return list.preset.size() && list.ranges->size();
}


CustomListsModel *WobblyProject::getCustomListsModel() {
    return custom_lists;
}


int WobblyProject::getDecimateMetric(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't get the decimation metric for frame " + std::to_string(frame) + ": frame number out of range.");

    if (decimate_metrics.size())
        return decimate_metrics[frame];
    else
        return 0;
}


void WobblyProject::setDecimateMetric(int frame, int decimate_metric) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't set the decimation metric for frame " + std::to_string(frame) + ": frame number out of range.");

    if (!decimate_metrics.size())
        decimate_metrics.resize(getNumFrames(PostSource), 0);

    decimate_metrics[frame] = decimate_metric;
}


void WobblyProject::addDecimatedFrame(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't mark frame " + std::to_string(frame) + " for decimation: value out of range.");

    // Don't allow decimating all the frames in a cycle.
    if (decimated_frames[frame / 5].size() == 5 - 1)
        return;

    auto result = decimated_frames[frame / 5].insert(frame % 5);

    if (result.second) {
        setNumFrames(PostDecimate, getNumFrames(PostDecimate) - 1);

        setModified(true);
    }
}


void WobblyProject::deleteDecimatedFrame(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't delete decimated frame " + std::to_string(frame) + ": value out of range.");

    size_t result = decimated_frames[frame / 5].erase(frame % 5);

    if (result) {
        setNumFrames(PostDecimate, getNumFrames(PostDecimate) + 1);

        setModified(true);
    }
}


bool WobblyProject::isDecimatedFrame(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't check if frame " + std::to_string(frame) + " is decimated: value out of range.");

    return (bool)decimated_frames[frame / 5].count(frame % 5);
}


void WobblyProject::clearDecimatedFramesFromCycle(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't clear decimated frames from cycle containing frame " + std::to_string(frame) + ": value out of range.");

    int cycle = frame / 5;

    size_t new_frames = decimated_frames[cycle].size();

    decimated_frames[cycle].clear();

    setNumFrames(PostDecimate, getNumFrames(PostDecimate) + new_frames);
}


DecimationRangeVector WobblyProject::getDecimationRanges() const {
    DecimationRangeVector ranges;

    DecimationRange current_range;
    current_range.num_dropped = -1;

    for (size_t i = 0; i < decimated_frames.size(); i++) {
        if ((int)decimated_frames[i].size() != current_range.num_dropped) {
            current_range.start = i * 5;
            current_range.num_dropped = decimated_frames[i].size();
            ranges.push_back(current_range);
        }
    }

    return ranges;
}


static bool areDecimationPatternsEqual(const std::set<int8_t> &a, const std::set<int8_t> &b) {
    if (a.size() != b.size())
        return false;

    for (auto it1 = a.cbegin(), it2 = b.cbegin(); it1 != a.cend(); it1++, it2++)
        if (*it1 != *it2)
            return false;

    return true;
}


DecimationPatternRangeVector WobblyProject::getDecimationPatternRanges() const {
    DecimationPatternRangeVector ranges;

    DecimationPatternRange current_range;
    current_range.dropped_offsets.insert(-1);

    for (size_t i = 0; i < decimated_frames.size(); i++) {
        if (!areDecimationPatternsEqual(decimated_frames[i], current_range.dropped_offsets)) {
            current_range.start = i * 5;
            current_range.dropped_offsets = decimated_frames[i];
            ranges.push_back(current_range);
        }
    }

    return ranges;
}


std::map<size_t, size_t> WobblyProject::getCMatchSequences(int minimum) const {
    std::map<size_t, size_t> sequences;

    size_t start = 0;
    size_t length = 0;

    auto cbegin = matches.cbegin();
    auto cend = matches.cend();
    if (!matches.size()) {
        cbegin = original_matches.cbegin();
        cend = original_matches.cend();
    }

    for (auto match = cbegin; match != cend; match++) {
        if (*match == 'c') {
            if (length == 0)
                start = std::distance(cbegin, match);
            length++;
        } else {
            if (length >= (size_t)minimum)
                sequences.insert({ start, length });
            length = 0;
        }
    }

    if (!matches.size() && !original_matches.size())
        length = getNumFrames(PostSource);

    // The very last sequence.
    if (length > 0)
        sequences.insert({ start, length });

    return sequences;
}


CombedFramesModel *WobblyProject::getCombedFramesModel() {
    return combed_frames;
}


void WobblyProject::addCombedFrame(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't mark frame " + std::to_string(frame) + " as combed: value out of range.");

    combed_frames->insert(frame);

    setModified(true);
}


void WobblyProject::deleteCombedFrame(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't mark frame " + std::to_string(frame) + " as not combed: value out of range.");

    combed_frames->erase(frame);

    setModified(true);
}


bool WobblyProject::isCombedFrame(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't check if frame " + std::to_string(frame) + " is combed: value out of range.");

    return (bool)combed_frames->count(frame);
}


void WobblyProject::clearCombedFrames() {
    combed_frames->clear();
}


const Resize &WobblyProject::getResize() const {
    return resize;
}


void WobblyProject::setResize(int new_width, int new_height, const std::string &filter) {
    if (new_width <= 0 || new_height <= 0)
        throw WobblyException("Can't resize to " + std::to_string(new_width) + "x" + std::to_string(new_height) + ": dimensions must be positive.");

    resize.width = new_width;
    resize.height = new_height;
    resize.filter = filter;

    setModified(true);
}


void WobblyProject::setResizeEnabled(bool enabled) {
    resize.enabled = enabled;

    setModified(true);
}


bool WobblyProject::isResizeEnabled() const {
    return resize.enabled;
}


const Crop &WobblyProject::getCrop() const {
    return crop;
}


void WobblyProject::setCrop(int left, int top, int right, int bottom) {
    if (left < 0 || top < 0 || right < 0 || bottom < 0)
        throw WobblyException("Can't crop (" + std::to_string(left) + "," + std::to_string(top) + "," + std::to_string(right) + "," + std::to_string(bottom) + "): negative values.");

    crop.left = left;
    crop.top = top;
    crop.right = right;
    crop.bottom = bottom;

    setModified(true);
}


void WobblyProject::setCropEnabled(bool enabled) {
    crop.enabled = enabled;

    setModified(true);
}


bool WobblyProject::isCropEnabled() const {
    return crop.enabled;
}


void WobblyProject::setCropEarly(bool early) {
    crop.early = early;

    setModified(true);
}


bool WobblyProject::isCropEarly() const {
    return crop.early;
}


const Depth &WobblyProject::getBitDepth() const {
    return depth;
}


void WobblyProject::setBitDepth(int bits, bool float_samples, const std::string &dither) {
    depth.bits = bits;
    depth.float_samples = float_samples;
    depth.dither = dither;

    setModified(true);
}


void WobblyProject::setBitDepthEnabled(bool enabled) {
    depth.enabled = enabled;

    setModified(true);
}


bool WobblyProject::isBitDepthEnabled() const {
    return depth.enabled;
}


const std::string &WobblyProject::getSourceFilter() const {
    return source_filter;
}


void WobblyProject::setSourceFilter(const std::string &filter) {
    source_filter = filter;
}


bool WobblyProject::getFreezeFramesWanted() const {
    return freeze_frames_wanted;
}


void WobblyProject::setFreezeFramesWanted(bool wanted) {
    freeze_frames_wanted = wanted;
}


bool WobblyProject::isModified() const {
    return is_modified;
}


void WobblyProject::setModified(bool modified) {
    if (modified != is_modified) {
        is_modified = modified;

        emit modifiedChanged(modified);
    }
}


int WobblyProject::getZoom() const {
    return zoom;
}


void WobblyProject::setZoom(int ratio) {
    if (ratio < 1)
        throw WobblyException("Can't set zoom to ratio " + std::to_string(ratio) + ": ratio must be at least 1.");

    zoom = ratio;
}


int WobblyProject::getLastVisitedFrame() const {
    return last_visited_frame;
}


void WobblyProject::setLastVisitedFrame(int frame) {
    last_visited_frame = frame;
}


std::string WobblyProject::getUIState() const {
    return ui_state;
}


void WobblyProject::setUIState(const std::string &state) {
    ui_state = state;
}


std::string WobblyProject::getUIGeometry() const {
    return ui_geometry;
}


void WobblyProject::setUIGeometry(const std::string &geometry) {
    ui_geometry = geometry;
}


std::array<bool, 5> WobblyProject::getShownFrameRates() const {
    return shown_frame_rates;
}


void WobblyProject::setShownFrameRates(const std::array<bool, 5> &rates) {
    shown_frame_rates = rates;
}


int WobblyProject::getMicSearchMinimum() const {
    return mic_search_minimum;
}


void WobblyProject::setMicSearchMinimum(int minimum) {
    mic_search_minimum = minimum;
}


int WobblyProject::getCMatchSequencesMinimum() const {
    return c_match_sequences_minimum;
}


void WobblyProject::setCMatchSequencesMinimum(int minimum) {
    c_match_sequences_minimum = minimum;
}


std::string WobblyProject::frameToTime(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't convert frame " + std::to_string(frame) + " to a time: frame number out of range.");

    int milliseconds = (int)((frame * fps_den * 1000 / fps_num) % 1000);
    int seconds_total = (int)(frame * fps_den / fps_num);
    int seconds = seconds_total % 60;
    int minutes = (seconds_total / 60) % 60;
    int hours = seconds_total / 3600;

    char time[16];
#ifdef _MSC_VER
    _snprintf
#else
    snprintf
#endif
            (time, sizeof(time), "%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
    time[15] = '\0';

    return std::string(time);
}


int WobblyProject::frameNumberAfterDecimation(int frame) const {
    if (frame < 0)
        return 0;

    if (frame >= getNumFrames(PostSource))
        return getNumFrames(PostDecimate);

    int cycle_number = frame / 5;

    int position_in_cycle = frame % 5;

    int out_frame = cycle_number * 5;

    for (int i = 0; i < cycle_number; i++)
        out_frame -= decimated_frames[i].size();

    for (int8_t i = 0; i < position_in_cycle; i++)
        if (!decimated_frames[cycle_number].count(i))
            out_frame++;

    if (frame == getNumFrames(PostSource) - 1 && isDecimatedFrame(frame))
        out_frame--;

    return out_frame;
}


int WobblyProject::frameNumberBeforeDecimation(int frame) const {
    int original_frame = frame;

    if (frame < 0)
        frame = 0;

    if (frame >= getNumFrames(PostDecimate))
        frame = getNumFrames(PostDecimate) - 1;

    for (size_t i = 0; i < decimated_frames.size(); i++) {
        for (int j = 0; j < 5; j++) {
            if (!decimated_frames[i].count(j))
                frame--;

            if (frame == -1)
                return i * 5 + j;
        }
    }

    throw WobblyException("Failed to convert frame number " + std::to_string(original_frame) + " after decimation into the frame number before decimation.");
}


void WobblyProject::applyPatternGuessingDecimation(const int section_start, const int section_end, const int first_duplicate, int drop_duplicate) {
    // If the first duplicate is the last frame in the cycle, we have to drop the same duplicate in the entire section.
    if (drop_duplicate == DropUglierDuplicatePerCycle && first_duplicate == 4)
        drop_duplicate = DropUglierDuplicatePerSection;

    int drop = -1;

    if (drop_duplicate == DropUglierDuplicatePerSection) {
        // Find the uglier duplicate.
        int drop_n = 0;
        int drop_c = 0;

        for (int i = section_start; i < std::min(section_end, getNumFrames(PostSource) - 1); i++) {
            if (i % 5 == first_duplicate) {
                int16_t mic_n = getMics(i)[matchCharToIndex('n')];
                int16_t mic_c = getMics(i + 1)[matchCharToIndex('c')];
                if (mic_n > mic_c)
                    drop_n++;
                else
                    drop_c++;
            }
        }

        if (drop_n > drop_c)
            drop = first_duplicate;
        else
            drop = (first_duplicate + 1) % 5;
    } else if (drop_duplicate == DropFirstDuplicate) {
        drop = first_duplicate;
    } else if (drop_duplicate == DropSecondDuplicate) {
        drop = (first_duplicate + 1) % 5;
    }

    int first_cycle = section_start / 5;
    int last_cycle = (section_end - 1) / 5;
    for (int i = first_cycle; i < last_cycle + 1; i++) {
        if (drop_duplicate == DropUglierDuplicatePerCycle) {
            if (i == first_cycle) {
                if (section_start % 5 > first_duplicate + 1)
                    continue;
                else if (section_start % 5 > first_duplicate)
                    drop = first_duplicate + 1;
            } else if (i == last_cycle) {
                if ((section_end - 1) % 5 < first_duplicate)
                    continue;
                else if ((section_end - 1) % 5 < first_duplicate + 1)
                    drop = first_duplicate;
            }

            if (drop == -1) {
                int16_t mic_n = getMics(i * 5 + first_duplicate)[matchCharToIndex('n')];
                int16_t mic_c = getMics(i * 5 + first_duplicate + 1)[matchCharToIndex('c')];
                if (mic_n > mic_c)
                    drop = first_duplicate;
                else
                    drop = (first_duplicate + 1) % 5;
            }
        }

        // At this point we know what frame to drop in this cycle.

        if (i == first_cycle) {
            // See if the cycle has a decimated frame from the previous section.

            /*
                bool conflicting_patterns = false;

                for (int j = i * 5; j < section_start; j++)
                    if (isDecimatedFrame(j)) {
                        conflicting_patterns = true;
                        break;
                    }

                if (conflicting_patterns) {
                    // If 18 fps cycles are not wanted, try to decimate from the side with more motion.
                }
                */

            // Clear decimated frames in the cycle, but only from this section.
            for (int j = section_start; j < (i + 1) * 5; j++)
                if (isDecimatedFrame(j))
                    deleteDecimatedFrame(j);
        } else if (i == last_cycle) {
            // See if the cycle has a decimated frame from the next section.

            // Clear decimated frames in the cycle, but only from this section.
            for (int j = i * 5; j < section_end; j++)
                if (isDecimatedFrame(j))
                    deleteDecimatedFrame(j);
        } else {
            clearDecimatedFramesFromCycle(i * 5);
        }

        int drop_frame = i * 5 + drop;
        if (drop_frame >= section_start && drop_frame < section_end)
            addDecimatedFrame(drop_frame);
    }

    setModified(true);
}


bool WobblyProject::guessSectionPatternsFromMics(int section_start, int minimum_length, int use_patterns, int drop_duplicate) {
    if (!mics.size())
        throw WobblyException("Can't guess patterns from mics because there are no mics in the project.");

    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't guess patterns from mics for section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections->count(section_start))
        throw WobblyException("Can't guess patterns from mics for section starting at " + std::to_string(section_start) + ": no such section.");


    int section_end = getSectionEnd(section_start);

    if (section_end - section_start < minimum_length) {
        FailedPatternGuessing failure;
        failure.start = section_start;
        failure.reason = SectionTooShort;
        pattern_guessing.failures.erase(failure.start);
        pattern_guessing.failures.insert({ failure.start, failure });

        setModified(true);

        return false;
    }

    struct Pattern {
        const std::string pattern;
        int pattern_offset;
        int mic_dev; // "dev" ? Name inherited from Yatta.
    };

    std::vector<Pattern> patterns = {
        { "cccnn", -1, INT_MAX },
        { "ccnnn", -1, INT_MAX },
        { "c",     -1, INT_MAX }
    };

    int best_mic_dev = INT_MAX;
    int best_pattern = -1;

    for (size_t p = 0; p < patterns.size(); p++) {
        if (patterns[p].pattern == "cccnn" && !(use_patterns & PatternCCCNN))
            continue;
        if (patterns[p].pattern == "ccnnn" && !(use_patterns & PatternCCNNN))
            continue;
        if (patterns[p].pattern == "c" && !(use_patterns & PatternCCCCC))
            continue;

        for (int pattern_offset = 0; pattern_offset < (int)patterns[p].pattern.size(); pattern_offset++) {
            int mic_dev = 0;

            for (int frame = section_start; frame < section_end; frame++) {
                char pattern_match = patterns[p].pattern[(frame + pattern_offset) % patterns[p].pattern.size()];
                char other_match = pattern_match == 'c' ? 'n' : 'c';

                auto frame_mics = getMics(frame);

                int16_t mic_pattern_match = frame_mics[matchCharToIndex(pattern_match)];
                int16_t mic_other_match = frame_mics[matchCharToIndex(other_match)];

                mic_dev += std::max(0, mic_pattern_match - mic_other_match);
            }

            if (mic_dev < patterns[p].mic_dev) {
                patterns[p].pattern_offset = pattern_offset;
                patterns[p].mic_dev = mic_dev;
            }
        }

        if (patterns[p].mic_dev < best_mic_dev) {
            best_mic_dev = patterns[p].mic_dev;
            best_pattern = p;
        }
    }

    if (patterns[best_pattern].mic_dev > section_end - section_start) {
        FailedPatternGuessing failure;
        failure.start = section_start;
        failure.reason = AmbiguousMatchPattern;
        pattern_guessing.failures.erase(failure.start);
        pattern_guessing.failures.insert({ failure.start, failure });

        setModified(true);

        return false;
    }


    for (int i = section_start; i < section_end; i++)
        setMatch(i, patterns[best_pattern].pattern[(i + patterns[best_pattern].pattern_offset) % patterns[best_pattern].pattern.size()]);

    if (section_end == getNumFrames(PostSource) && getMatch(section_end - 1) == 'n')
        setMatch(section_end - 1, 'b');

    // If the last frame of the section has much higher mic with c/n matches than with b match, use the b match.
    char match_index = matchCharToIndex(getMatch(section_end - 1));
    int16_t mic_cn = getMics(section_end - 1)[match_index];
    int16_t mic_b = getMics(section_end - 1)[matchCharToIndex('b')];
    if (mic_cn > mic_b * 2)
        setMatch(section_end - 1, 'b');

    if (patterns[best_pattern].pattern == "c") {
        for (int i = section_start; i < section_end; i++)
            deleteDecimatedFrame(i);
    } else {
        int first_duplicate = 4 - patterns[best_pattern].pattern_offset;

        applyPatternGuessingDecimation(section_start, section_end, first_duplicate, drop_duplicate);
    }

    pattern_guessing.failures.erase(section_start);

    setModified(true);

    return true;
}


void WobblyProject::guessProjectPatternsFromMics(int minimum_length, int use_patterns, int drop_duplicate) {
    pattern_guessing.failures.clear();

    for (auto it = sections->cbegin(); it != sections->cend(); it++)
        guessSectionPatternsFromMics(it->second.start, minimum_length, use_patterns, drop_duplicate);

    pattern_guessing.method = PatternGuessingFromMics;
    pattern_guessing.minimum_length = minimum_length;
    pattern_guessing.use_patterns = use_patterns;
    pattern_guessing.decimation = drop_duplicate;

    setModified(true);
}


bool WobblyProject::guessSectionPatternsFromMatches(int section_start, int minimum_length, int use_third_n_match, int drop_duplicate) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't guess patterns from matches for section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections->count(section_start))
        throw WobblyException("Can't reset patterns from matches for section starting at " + std::to_string(section_start) + ": no such section.");

    int section_end = getSectionEnd(section_start);

    if (section_end - section_start < minimum_length) {
        FailedPatternGuessing failure;
        failure.start = section_start;
        failure.reason = SectionTooShort;
        pattern_guessing.failures.erase(failure.start);
        pattern_guessing.failures.insert({ failure.start, failure });

        setModified(true);

        return false;
    }

    // Count the "nc" pairs in each position.
    int positions[5] = { 0 };
    int total = 0;

    for (int i = section_start; i < std::min(section_end, getNumFrames(PostSource) - 1); i++) {
        if (getOriginalMatch(i) == 'n' && getOriginalMatch(i + 1) == 'c') {
            positions[i % 5]++;
            total++;
        }
    }

    // Find the two positions with the most "nc" pairs.
    int best = 0;
    int next_best = 0;
    int tmp = -1;

    for (int i = 0; i < 5; i++)
        if (positions[i] > tmp) {
            tmp = positions[i];
            best = i;
        }

    tmp = -1;

    for (int i = 0; i < 5; i++) {
        if (i == best)
            continue;

        if (positions[i] > tmp) {
            tmp = positions[i];
            next_best = i;
        }
    }

    float best_percent = 0.0f;
    float next_best_percent = 0.0f;

    if (total > 0) {
        best_percent = positions[best] * 100 / (float)total;
        next_best_percent = positions[next_best] * 100 / (float)total;
    }

    // Totally arbitrary thresholds.
    if (best_percent > 40.0f && best_percent - next_best_percent > 10.0f) {
        // Take care of decimation first.
        applyPatternGuessingDecimation(section_start, section_end, best, drop_duplicate);


        // Now the matches.
        std::string patterns[5] = { "ncccn", "nnccc", "cnncc", "ccnnc", "cccnn" };
        if (use_third_n_match == UseThirdNMatchAlways)
            for (int i = 0; i < 5; i++)
                patterns[i][(i + 3) % 5] = 'n';

        const std::string &pattern = patterns[best];

        for (int i = section_start; i < section_end; i++) {
            if (use_third_n_match == UseThirdNMatchIfPrettier && pattern[i % 5] == 'c' && pattern[(i + 1) % 5] == 'n') {
                int16_t mic_n = getMics(i)[matchCharToIndex('n')];
                int16_t mic_c = getMics(i)[matchCharToIndex('c')];
                if (mic_n < mic_c)
                    setMatch(i, 'n');
                else
                    setMatch(i, 'c');
            } else {
                setMatch(i, pattern[i % 5]);
            }
        }

        // If the last frame of the section has much higher mic with c/n matches than with b match, use the b match.
        char match_index = matchCharToIndex(getMatch(section_end - 1));
        int16_t mic_cn = getMics(section_end - 1)[match_index];
        int16_t mic_b = getMics(section_end - 1)[matchCharToIndex('b')];
        if (mic_cn > mic_b * 2)
            setMatch(section_end - 1, 'b');

        // A pattern was found.
        pattern_guessing.failures.erase(section_start);

        setModified(true);

        return true;
    } else {
        // A pattern was not found.
        FailedPatternGuessing failure;
        failure.start = section_start;
        failure.reason = AmbiguousMatchPattern;
        pattern_guessing.failures.erase(failure.start);
        pattern_guessing.failures.insert({ failure.start, failure });

        setModified(true);

        return false;
    }
}


void WobblyProject::guessProjectPatternsFromMatches(int minimum_length, int use_third_n_match, int drop_duplicate) {
    pattern_guessing.failures.clear();

    for (auto it = sections->cbegin(); it != sections->cend(); it++)
        guessSectionPatternsFromMatches(it->second.start, minimum_length, use_third_n_match, drop_duplicate);

    pattern_guessing.method = PatternGuessingFromMatches;
    pattern_guessing.minimum_length = minimum_length;
    pattern_guessing.third_n_match = use_third_n_match;
    pattern_guessing.decimation = drop_duplicate;

    setModified(true);
}


const PatternGuessing &WobblyProject::getPatternGuessing() {
    return pattern_guessing;
}


void WobblyProject::addInterlacedFade(int frame, double field_difference) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't add interlaced fade at frame " + std::to_string(frame) + ": frame number out of range.");

    interlaced_fades.insert({ frame, { frame, field_difference } });
}


const InterlacedFadeMap &WobblyProject::getInterlacedFades() const {
    return interlaced_fades;
}


void WobblyProject::addBookmark(int frame, const std::string &description) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't add bookmark at frame " + std::to_string(frame) + ": frame number out of range.");

    bookmarks->insert({ frame, { frame, description } });

    setModified(true);
}


void WobblyProject::deleteBookmark(int frame) {
    if (!bookmarks->count(frame))
        throw WobblyException("Can't delete bookmark at frame " + std::to_string(frame) + ": no such bookmark.");

    bookmarks->erase(frame);
}


bool WobblyProject::isBookmark(int frame) const {
    return (bool)bookmarks->count(frame);
}


int WobblyProject::findPreviousBookmark(int frame) const {
    BookmarksModel::const_iterator it = bookmarks->lower_bound(frame);

    if (it != bookmarks->cbegin()) {
        it--;

        return it->second.frame;
    }

    return frame;
}


int WobblyProject::findNextBookmark(int frame) const {
    BookmarksModel::const_iterator it = bookmarks->upper_bound(frame);

    if (it != bookmarks->cend())
        return it->second.frame;

    return frame;
}


const Bookmark *WobblyProject::getBookmark(int frame) const {
    try {
        return &bookmarks->at(frame);
    } catch (std::out_of_range &) {
        return nullptr;
    }
}


BookmarksModel *WobblyProject::getBookmarksModel() {
    return bookmarks;
}


void WobblyProject::sectionsToScript(std::string &script) const {
    auto samePresets = [] (const std::vector<std::string> &a, const std::vector<std::string> &b) -> bool {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); i++)
            if (a[i] != b[i])
                return false;

        return true;
    };

    SectionMap merged_sections;
    merged_sections.insert({ 0, sections->cbegin()->second });

    for (auto it = ++(sections->cbegin()); it != sections->cend(); it++)
        if (!samePresets(it->second.presets, merged_sections.crbegin()->second.presets))
            merged_sections.insert({ it->first, it->second });


    std::string splice = "src = c.std.Splice(mismatch=True, clips=[";
    for (auto it = merged_sections.cbegin(); it != merged_sections.cend(); it++) {
        std::string section_name = "section";
        section_name += std::to_string(it->second.start);
        script += section_name + " = src";

        for (size_t i = 0; i < it->second.presets.size(); i++) {
            script += "\n";
            script += section_name + " = preset_";
            script += it->second.presets[i] + "(";
            script += section_name + ")";
        }

        script += "[";
        script += std::to_string(it->second.start);
        script += ":";

        auto it_next = it;
        it_next++;
        if (it_next != merged_sections.cend())
            script += std::to_string(it_next->second.start);
        script += "]\n";

        splice += section_name + ",";
    }
    splice +=
            "])\n"
            "\n";

    script += splice;
}


int WobblyProject::maybeTranslate(int frame, bool is_end, PositionInFilterChain position) const {
    if (position == PostDecimate) {
        if (is_end)
            while (isDecimatedFrame(frame))
                frame--;
        return frameNumberAfterDecimation(frame);
    } else
        return frame;
}


void WobblyProject::customListsToScript(std::string &script, PositionInFilterChain position) const {
    for (size_t i = 0; i < custom_lists->size(); i++) {
        const CustomList &cl = custom_lists->at(i);

        // Ignore lists that are in a different position in the filter chain.
        if (cl.position != position)
            continue;

        // Ignore lists with no frame ranges.
        if (!cl.ranges->size())
            continue;

        // Complain if the custom list doesn't have a preset assigned.
        if (!cl.preset.size())
            throw WobblyException("Custom list '" + cl.name + "' has no preset assigned.");


        std::string list_name = "cl_";
        list_name += cl.name;

        script += list_name + " = preset_" + cl.preset + "(src)\n";

        std::string splice = "src = c.std.Splice(mismatch=True, clips=[";

        auto it = cl.ranges->cbegin();
        auto it_prev = it;

        if (it->second.first > 0) {
            splice += "src[0:";
            splice += std::to_string(maybeTranslate(it->second.first, false, position)) + "],";
        }

        splice += list_name + "[" + std::to_string(maybeTranslate(it->second.first, false, position)) + ":" + std::to_string(maybeTranslate(it->second.last, true, position) + 1) + "],";

        it++;
        for ( ; it != cl.ranges->cend(); it++, it_prev++) {
            int previous_last = maybeTranslate(it_prev->second.last, true, position);
            int current_first = maybeTranslate(it->second.first, false, position);
            int current_last = maybeTranslate(it->second.last, true, position);
            if (current_first - previous_last > 1) {
                splice += "src[";
                splice += std::to_string(previous_last + 1) + ":" + std::to_string(current_first) + "],";
            }

            splice += list_name + "[" + std::to_string(current_first) + ":" + std::to_string(current_last + 1) + "],";
        }

        // it_prev is cend()-1 at the end of the loop.

        int last_last = maybeTranslate(it_prev->second.last, true, position);

        if (last_last < maybeTranslate(getNumFrames(PostSource) - 1, true, position)) {
            splice += "src[";
            splice += std::to_string(last_last + 1) + ":]";
        }

        splice += "])\n\n";

        script += splice;
    }
}


void WobblyProject::headerToScript(std::string &script) const {
    script +=
            "# Generated by Wobbly v" PACKAGE_VERSION "\n"
            "# " PACKAGE_URL "\n"
            "\n"
            "import vapoursynth as vs\n"
            "\n"
            "c = vs.core\n"
            "\n";
}


void WobblyProject::presetsToScript(std::string &script) const {
    for (auto it = presets->cbegin(); it != presets->cend(); it++) {
        if (!isPresetInUse(it->second.name))
            continue;

        script += "def preset_" + it->second.name + "(clip):\n";
        size_t start = 0, end;
        do {
            end = it->second.contents.find('\n', start);
            script += "    " + it->second.contents.substr(start, end - start) + "\n";
            start = end + 1;
        } while (end != std::string::npos);
        script += "    return clip\n";
        script += "\n\n";
    }
}


void WobblyProject::sourceToScript(std::string &script, bool save_node) const {
    std::string src = "src = c." + source_filter + "(r'" + handleSingleQuotes(input_file) + "')\n";

    if (save_node) {
    script +=
            "try:\n"
            "    src = vs.get_output(index=1)\n"
            // Since VapourSynth R41 get_output returns the alpha as well.
            "    if isinstance(src, tuple):\n"
            "        src = src[0]\n"
            "except KeyError:\n"
            "    " + src +
            "    src.set_output(index=1)\n"
            "\n";
    } else {
        script += src;
        script += "\n";
    }
}


void WobblyProject::trimToScript(std::string &script) const {
    script += "src = c.std.Splice(clips=[";
    for (auto it = trims.cbegin(); it != trims.cend(); it++)
        script += "src[" + std::to_string(it->second.first) + ":" + std::to_string(it->second.last + 1) + "],";
    script +=
            "])\n"
            "\n";
}


void WobblyProject::fieldHintToScript(std::string &script) const {
    if (!matches.size() && !original_matches.size())
        return;

    script += "src = c.fh.FieldHint(clip=src, tff=";
    script += std::to_string((int)vfm_parameters.at("order"));
    script += ", matches='";
    if (matches.size())
        script.append(matches.data(), matches.size());
    else
        script.append(original_matches.data(), original_matches.size());
    script +=
            "')\n"
            "\n";
}


void WobblyProject::freezeFramesToScript(std::string &script) const {
    std::string ff_first = ", first=[";
    std::string ff_last = ", last=[";
    std::string ff_replacement = ", replacement=[";

    for (auto it = frozen_frames->cbegin(); it != frozen_frames->cend(); it++) {
        ff_first += std::to_string(it->second.first) + ",";
        ff_last += std::to_string(it->second.last) + ",";
        ff_replacement += std::to_string(it->second.replacement) + ",";
    }
    ff_first += "]";
    ff_last += "]";
    ff_replacement += "]";

    script += "src = c.std.FreezeFrames(clip=src";
    script += ff_first;
    script += ff_last;
    script += ff_replacement;
    script +=
            ")\n"
            "\n";
}


void WobblyProject::decimatedFramesToScript(std::string &script) const {
    std::string delete_frames;

    const DecimationRangeVector &decimation_ranges = getDecimationRanges();

    std::array<int, 5> frame_rate_counts = { 0, 0, 0, 0, 0 };

    for (size_t i = 0; i < decimation_ranges.size(); i++)
        frame_rate_counts[decimation_ranges[i].num_dropped]++;

    std::string frame_rates[5] = { "30", "24", "18", "12", "6" };

    for (size_t i = 0; i < 5; i++)
        if (frame_rate_counts[i])
            delete_frames += "r" + frame_rates[i] + " = c.std.AssumeFPS(clip=src, fpsnum=" + frame_rates[i] + "000, fpsden=1001)\n";

    delete_frames += "src = c.std.Splice(mismatch=True, clips=[";

    for (size_t i = 0; i < decimation_ranges.size(); i++) {
        int range_end;
        if (i == decimation_ranges.size() - 1)
            range_end = getNumFrames(PostSource);
        else
            range_end = decimation_ranges[i + 1].start;

        delete_frames += "r" + frame_rates[decimation_ranges[i].num_dropped] + "[" + std::to_string(decimation_ranges[i].start) + ":" + std::to_string(range_end) + "],";
    }

    delete_frames += "])\n";

    delete_frames += "src = c.std.DeleteFrames(clip=src, frames=[";

    for (size_t i = 0; i < decimated_frames.size(); i++)
        for (auto it = decimated_frames[i].cbegin(); it != decimated_frames[i].cend(); it++)
            delete_frames += std::to_string(i * 5 + *it) + ",";

    delete_frames +=
            "])\n"
            "\n";


    std::string select_every;

    const DecimationPatternRangeVector &decimation_pattern_ranges = getDecimationPatternRanges();

    std::string splice = "src = c.std.Splice(mismatch=True, clips=[";

    for (size_t i = 0; i < decimation_pattern_ranges.size(); i++) {
        int range_end;
        if (i == decimation_pattern_ranges.size() - 1)
            range_end = getNumFrames(PostSource);
        else
            range_end = decimation_pattern_ranges[i + 1].start;

        if (decimation_pattern_ranges[i].dropped_offsets.size()) {
            // The last range could contain fewer than five frames.
            // If they're all decimated, don't generate a SelectEvery
            // because clips with no frames are not allowed.
            if (range_end - decimation_pattern_ranges[i].start <= (int)decimation_pattern_ranges[i].dropped_offsets.size())
                break;

            std::set<int8_t> offsets = { 0, 1, 2, 3, 4 };

            for (auto it = decimation_pattern_ranges[i].dropped_offsets.cbegin(); it != decimation_pattern_ranges[i].dropped_offsets.cend(); it++)
                offsets.erase(*it);

            std::string range_name = "dec" + std::to_string(decimation_pattern_ranges[i].start);

            select_every += range_name + " = c.std.SelectEvery(clip=src[" + std::to_string(decimation_pattern_ranges[i].start) + ":" + std::to_string(range_end) + "], cycle=5, offsets=[";

            for (auto it = offsets.cbegin(); it != offsets.cend(); it++)
                select_every += std::to_string(*it) + ",";

            select_every += "])\n";

            splice += range_name + ",";
        } else {
            // 30 fps range.
            splice += "src[" + std::to_string(decimation_pattern_ranges[i].start) + ":" + std::to_string(range_end) + "],";
        }
    }

    select_every += "\n" + splice + "])\n\n";

    if (delete_frames.size() < select_every.size())
        script += delete_frames;
    else
        script += select_every;
}


void WobblyProject::cropToScript(std::string &script) const {
    script += "src = c.std.CropRel(clip=src, left=";
    script += std::to_string(crop.left) + ", top=";
    script += std::to_string(crop.top) + ", right=";
    script += std::to_string(crop.right) + ", bottom=";
    script += std::to_string(crop.bottom) + ")\n\n";
}


void WobblyProject::resizeAndBitDepthToScript(std::string &script, bool resize_enabled, bool depth_enabled) const {
    script += "src = c.resize.";

    if (resize_enabled) {
        script += (char)(resize.filter[0] - ('a' - 'A'));
        script += resize.filter.c_str() + 1;
    } else {
        script += "Bicubic";
    }

    script += "(clip=src";

    if (resize_enabled) {
        script += ", width=" + std::to_string(resize.width);
        script += ", height=" + std::to_string(resize.height);
    }

    if (depth_enabled)
        script += ", format=c.register_format(src.format.color_family, " + std::string(depth.float_samples ? "vs.FLOAT" : "vs.INTEGER") + ", " + std::to_string(depth.bits) + ", src.format.subsampling_w, src.format.subsampling_h).id";

    script += ")\n\n";
}


void WobblyProject::setOutputToScript(std::string &script) const {
    script += "src.set_output()\n";
}


std::string WobblyProject::generateFinalScript(bool save_source_node) const {
    // XXX Insert comments before and after each part.
    std::string script;

    headerToScript(script);

    presetsToScript(script);

    sourceToScript(script, save_source_node);

    if (crop.early && crop.enabled)
        cropToScript(script);

    trimToScript(script);

    customListsToScript(script, PostSource);

    fieldHintToScript(script);

    customListsToScript(script, PostFieldMatch);

    sectionsToScript(script);

    if (frozen_frames->size())
        freezeFramesToScript(script);

    bool decimation_needed = false;
    for (size_t i = 0; i < decimated_frames.size(); i++)
        if (decimated_frames[i].size()) {
            decimation_needed = true;
            break;
        }
    if (decimation_needed)
        decimatedFramesToScript(script);

    customListsToScript(script, PostDecimate);

    if (!crop.early && crop.enabled)
        cropToScript(script);

    if (resize.enabled || depth.enabled)
        resizeAndBitDepthToScript(script, resize.enabled, depth.enabled);

    setOutputToScript(script);

    return script;
}


std::string WobblyProject::generateMainDisplayScript() const {
    std::string script;

    headerToScript(script);

    sourceToScript(script, true);

    trimToScript(script);

    fieldHintToScript(script);

    if (frozen_frames->size() && freeze_frames_wanted)
        freezeFramesToScript(script);

    setOutputToScript(script);

    return script;
}


std::string WobblyProject::generateTimecodesV1() const {
    std::string tc =
            "# timecode format v1\n"
            "Assume ";

    char buf[20] = { 0 };
    snprintf(buf, sizeof(buf), "%.12f\n", 24000 / (double)1001);

    tc += buf;

    const DecimationRangeVector &ranges = getDecimationRanges();

    int numerators[] = { 30000, 24000, 18000, 12000, 6000 };

    for (size_t i = 0; i < ranges.size(); i++) {
        if (numerators[ranges[i].num_dropped] != 24000) {
            int end;
            if (i == ranges.size() - 1)
                end = getNumFrames(PostSource);
            else
                end = ranges[i + 1].start;

            tc += std::to_string(frameNumberAfterDecimation(ranges[i].start)) + ",";
            tc += std::to_string(frameNumberAfterDecimation(end) - 1) + ",";
            snprintf(buf, sizeof(buf), "%.12f\n", numerators[ranges[i].num_dropped] / (double)1001);
            char *comma = std::strchr(buf, ',');
            if (comma)
                *comma = '.';
            tc += buf;
        }
    }

    return tc;
}


void WobblyProject::importFromOtherProject(const std::string &path, const ImportedThings &imports) {
    WobblyProject *other = new WobblyProject(true);

    try {
        other->readProject(path);

        if (imports.geometry) {
            setUIState(other->getUIState());
            setUIGeometry(other->getUIGeometry());
        }

        if (imports.presets || imports.custom_lists) {
            const PresetsModel *p = other->getPresetsModel();
            for (auto it = p->cbegin(); it != p->cend(); it++) {
                std::string preset_name = it->second.name;

                bool rename_needed = presetExists(preset_name);
                while (presetExists(preset_name))
                    preset_name += "_imported";

                if (rename_needed) {
                    while (presetExists(preset_name) || other->presetExists(preset_name))
                        preset_name += "_imported";
                }

                other->renamePreset(it->second.name, preset_name); // changes to other aren't saved, so it's okay.
                if (imports.presets)
                    addPreset(preset_name, other->getPresetContents(preset_name));
            }
        }

        if (imports.custom_lists) {
            const CustomListsModel *lists = other->getCustomListsModel();
            for (size_t i = 0; i < lists->size(); i++) {
                if (lists->at(i).preset.size() && !presetExists(lists->at(i).preset))
                    addPreset(lists->at(i).preset, other->getPresetContents(lists->at(i).preset));

                CustomList list = lists->at(i);
                while (customListExists(list.name))
                    list.name += "_imported";
                addCustomList(list);
            }
        }

        if (imports.crop) {
            setCropEnabled(other->isCropEnabled());
            setCropEarly(other->isCropEarly());
            const Crop &c = other->getCrop();
            setCrop(c.left, c.top, c.right, c.bottom);
        }

        if (imports.resize) {
           setResizeEnabled(other->isResizeEnabled());
           const Resize &r = other->getResize();
           setResize(r.width, r.height, r.filter);
        }

        if (imports.bit_depth) {
            setBitDepthEnabled(other->isBitDepthEnabled());
            const Depth &d = other->getBitDepth();
            setBitDepth(d.bits, d.float_samples, d.dither);
        }

        if (imports.mic_search)
            setMicSearchMinimum(other->getMicSearchMinimum());

        if (imports.zoom)
            setZoom(other->getZoom());

        delete other;

        setModified(true);
    } catch (WobblyException &e) {
        delete other;

        throw e;
    }
}
