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
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include "WobblyException.h"
#include "WobblyProject.h"


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
    , pattern_guessing{ PatternGuessingFromMics, 10, UseThirdNMatchNever, DropFirstDuplicate, PatternCCCNN | PatternCCNNN | PatternCCCCC, { } }
    , resize{ false, 0, 0, "spline16" }
    , crop{ false, false, 0, 0, 0, 0 }
    , depth{ false, 8, false, "random" }
{

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
}


int WobblyProject::getNumFrames(PositionInFilterChain position) const {
    if (position == PostSource)
        return num_frames[0];
    else if (position == PostDecimate)
        return num_frames[1];
    else
        throw WobblyException("Can't set the number of frames for position " + std::to_string(position) + ": invalid position.");
}


void WobblyProject::setNumFrames(PositionInFilterChain position, int frames) {
    if (position == PostSource)
        num_frames[0] = frames;
    else if (position == PostDecimate)
        num_frames[1] = frames;
    else
        throw WobblyException("Can't set the number of frames for position " + std::to_string(position) + ": invalid position.");
}


void WobblyProject::writeProject(const std::string &path, bool compact_project) const {
    QFile file(QString::fromStdString(path));

    if (!file.open(QIODevice::WriteOnly))
        throw WobblyException("Couldn't open project file '" + path + "'. Error message: " + file.errorString().toStdString());

    QJsonObject json_project;

    json_project.insert("wobbly version", PACKAGE_VERSION);


    json_project.insert("input file", QString::fromStdString(input_file));


    QJsonArray json_fps;
    json_fps.append((qint64)fps_num);
    json_fps.append((qint64)fps_den);
    json_project.insert("input frame rate", json_fps);


    QJsonArray json_resolution;
    json_resolution.append(width);
    json_resolution.append(height);
    json_project.insert("input resolution", json_resolution);


    if (is_wobbly) {
        QJsonObject json_ui;
        json_ui.insert("zoom", zoom);
        json_ui.insert("last visited frame", last_visited_frame);
        json_ui.insert("geometry", QString::fromStdString(ui_geometry));
        json_ui.insert("state", QString::fromStdString(ui_state));

        QJsonArray json_rates;
        int rates[] = { 30, 24, 18, 12, 6 };
        for (int i = 0; i < 5; i++)
            if (shown_frame_rates[i])
                json_rates.append(rates[i]);
        json_ui.insert("show frame rates", json_rates);

        json_ui.insert("mic search minimum", mic_search_minimum);
        json_ui.insert("c match sequences minimum", c_match_sequences_minimum);

        if (pattern_guessing.failures.size()) {
            QJsonObject json_pattern_guessing;

            const char *guessing_methods[] = {
                "from matches",
                "from mics"
            };
            json_pattern_guessing.insert("method", guessing_methods[pattern_guessing.method]);

            json_pattern_guessing.insert("minimum length", pattern_guessing.minimum_length);

            const char *third_n_match[] = {
                "always",
                "never",
                "if it has lower mic"
            };
            json_pattern_guessing.insert("use third n match", third_n_match[pattern_guessing.third_n_match]);

            const char *decimate[] = {
                "first duplicate",
                "second duplicate",
                "duplicate with higher mic per cycle",
                "duplicate with higher mic per section"
            };
            json_pattern_guessing.insert("decimate", decimate[pattern_guessing.decimation]);

            QJsonArray json_use_patterns;

            std::map<int, std::string> use_patterns = {
                { PatternCCCNN, "cccnn" },
                { PatternCCNNN, "ccnnn" },
                { PatternCCCCC, "ccccc" }
            };

            for (auto it = use_patterns.cbegin(); it != use_patterns.cend(); it++)
                if (pattern_guessing.use_patterns & it->first)
                    json_use_patterns.append(QString::fromStdString(it->second));
            json_pattern_guessing.insert("use patterns", json_use_patterns);

            QJsonArray json_failures;

            const char *reasons[] = {
                "section too short",
                "ambiguous pattern"
            };
            for (auto it = pattern_guessing.failures.cbegin(); it != pattern_guessing.failures.cend(); it++) {
                QJsonObject json_failure;
                json_failure.insert("start", it->second.start);
                json_failure.insert("reason", reasons[it->second.reason]);
                json_failures.append(json_failure);
            }
            json_pattern_guessing.insert("failures", json_failures);

            json_ui.insert("pattern guessing", json_pattern_guessing);
        }

        json_project.insert("user interface", json_ui);
    }


    QJsonArray json_trims;

    for (auto it = trims.cbegin(); it != trims.cend(); it++) {
        QJsonArray json_trim;
        json_trim.append(it->second.first);
        json_trim.append(it->second.last);
        json_trims.append(json_trim);
    }
    json_project.insert("trim", json_trims);


    QJsonObject json_vfm_parameters;

    for (auto it = vfm_parameters.cbegin(); it != vfm_parameters.cend(); it++)
        json_vfm_parameters.insert(QString::fromStdString(it->first), it->second);

    json_project.insert("vfm parameters", json_vfm_parameters);


    QJsonObject json_vdecimate_parameters;

    for (auto it = vdecimate_parameters.cbegin(); it != vdecimate_parameters.cend(); it++)
        json_vdecimate_parameters.insert(QString::fromStdString(it->first), it->second);

    json_project.insert("vdecimate parameters", json_vdecimate_parameters);


    QJsonArray json_mics, json_matches, json_original_matches, json_combed_frames, json_decimated_frames, json_decimate_metrics;

    for (size_t i = 0; i < mics.size(); i++) {
        QJsonArray json_mic;
        for (int j = 0; j < 5; j++)
            json_mic.append(mics[i][j]);
        json_mics.append(json_mic);
    }

    for (size_t i = 0; i < matches.size(); i++)
        json_matches.append(QString(matches[i]));

    for (size_t i = 0; i < original_matches.size(); i++)
        json_original_matches.append(QString(original_matches[i]));

    for (auto it = combed_frames.cbegin(); it != combed_frames.cend(); it++)
        json_combed_frames.append(*it);

    for (size_t i = 0; i < decimated_frames.size(); i++)
        for (auto it = decimated_frames[i].cbegin(); it != decimated_frames[i].cend(); it++)
            json_decimated_frames.append((int)i * 5 + *it);

    for (size_t i = 0; i < decimate_metrics.size(); i++)
        json_decimate_metrics.append(getDecimateMetric(i));

    json_project.insert("mics", json_mics);
    json_project.insert("matches", json_matches);
    json_project.insert("original matches", json_original_matches);
    json_project.insert("combed frames", json_combed_frames);
    json_project.insert("decimated frames", json_decimated_frames);
    json_project.insert("decimate metrics", json_decimate_metrics);


    QJsonArray json_sections;

    for (auto it = sections.cbegin(); it != sections.cend(); it++) {
        QJsonObject json_section;
        json_section.insert("start", it->second.start);
        QJsonArray json_presets;
        for (size_t i = 0; i < it->second.presets.size(); i++)
            json_presets.append(QString::fromStdString(it->second.presets[i]));
        json_section.insert("presets", json_presets);

        json_sections.append(json_section);
    }

    json_project.insert("sections", json_sections);


    json_project.insert("source filter", QString::fromStdString(source_filter));


    QJsonArray json_interlaced_fades;

    for (auto it = interlaced_fades.cbegin(); it != interlaced_fades.cend(); it++) {
        QJsonObject json_interlaced_fade;
        json_interlaced_fade.insert("frame", it->second.frame);
        json_interlaced_fade.insert("field difference", it->second.field_difference);

        json_interlaced_fades.append(json_interlaced_fade);
    }

    json_project.insert("interlaced fades", json_interlaced_fades);


    if (is_wobbly) {
        QJsonArray json_presets, json_frozen_frames;

        for (auto it = presets.cbegin(); it != presets.cend(); it++) {
            QJsonObject json_preset;
            json_preset.insert("name", QString::fromStdString(it->second.name));
            json_preset.insert("contents", QString::fromStdString(it->second.contents));

            json_presets.append(json_preset);
        }

        for (auto it = frozen_frames.cbegin(); it != frozen_frames.cend(); it++) {
            QJsonArray json_ff;
            json_ff.append(it->second.first);
            json_ff.append(it->second.last);
            json_ff.append(it->second.replacement);

            json_frozen_frames.append(json_ff);
        }

        json_project.insert("presets", json_presets);
        json_project.insert("frozen frames", json_frozen_frames);


        QJsonArray json_custom_lists;

        for (size_t i = 0; i < custom_lists.size(); i++) {
            QJsonObject json_custom_list;
            json_custom_list.insert("name", QString::fromStdString(custom_lists[i].name));
            json_custom_list.insert("preset", QString::fromStdString(custom_lists[i].preset));
            json_custom_list.insert("position", custom_lists[i].position);
            QJsonArray json_frames;
            for (auto it = custom_lists[i].ranges.cbegin(); it != custom_lists[i].ranges.cend(); it++) {
                QJsonArray json_pair;
                json_pair.append(it->second.first);
                json_pair.append(it->second.last);
                json_frames.append(json_pair);
            }
            json_custom_list.insert("frames", json_frames);

            json_custom_lists.append(json_custom_list);
        }

        json_project.insert("custom lists", json_custom_lists);


        if (resize.enabled) {
            QJsonObject json_resize;
            json_resize.insert("width", resize.width);
            json_resize.insert("height", resize.height);
            json_resize.insert("filter", QString::fromStdString(resize.filter));
            json_project.insert("resize", json_resize);
        }

        if (crop.enabled) {
            QJsonObject json_crop;
            json_crop.insert("early", crop.early);
            json_crop.insert("left", crop.left);
            json_crop.insert("top", crop.top);
            json_crop.insert("right", crop.right);
            json_crop.insert("bottom", crop.bottom);
            json_project.insert("crop", json_crop);
        }

        if (depth.enabled) {
            QJsonObject json_depth;
            json_depth.insert("bits", depth.bits);
            json_depth.insert("float samples", depth.float_samples);
            json_depth.insert("dither", QString::fromStdString(depth.dither));
            json_project.insert("depth", json_depth);
        }
    }

    QJsonDocument json_doc(json_project);

    QJsonDocument::JsonFormat json_format = QJsonDocument::Indented;
    if (compact_project)
        json_format = QJsonDocument::Compact;

    if (file.write(json_doc.toJson(json_format)) < 0)
        throw WobblyException("Couldn't write the project to file '" + path + "'. Error message: " + file.errorString().toStdString());
}

void WobblyProject::readProject(const std::string &path) {
    QFile file(QString::fromStdString(path));

    if (!file.open(QIODevice::ReadOnly))
        throw WobblyException("Couldn't open project file '" + path + "'. Error message: " + file.errorString().toStdString());

    QByteArray data = file.readAll();

    QJsonDocument json_doc(QJsonDocument::fromJson(data));
    if (json_doc.isNull())
        throw WobblyException("Couldn't open project file '" + path + "': file is not a valid JSON document.");
    if (!json_doc.isObject())
        throw WobblyException("Couldn't open project file '" + path + "': file is not a valid Wobbly project.");

    QJsonObject json_project = json_doc.object();


    const char *required_keys[] = {
        "input file",
        "input frame rate",
        "input resolution",
        "trim",
        "source filter",
        nullptr
    };

    for (int i = 0; required_keys[i]; i++)
        if (!json_project.contains(required_keys[i]))
            throw WobblyException("Couldn't open project file '" + path + "': project is missing JSON key '" + required_keys[i] + "'.");


    //int version = (int)json_project["wobbly version"].toDouble();


    input_file = json_project["input file"].toString().toStdString();


    fps_num = (int64_t)json_project["input frame rate"].toArray()[0].toDouble();
    fps_den = (int64_t)json_project["input frame rate"].toArray()[1].toDouble();


    width = json_project["input resolution"].toArray()[0].toInt();
    height = json_project["input resolution"].toArray()[1].toInt();


    QJsonObject json_ui = json_project["user interface"].toObject();
    zoom = json_ui["zoom"].toInt(1);
    last_visited_frame = json_ui["last visited frame"].toInt(0);
    ui_state = json_ui["state"].toString().toStdString();
    ui_geometry = json_ui["geometry"].toString().toStdString();

    if (json_ui.contains("show frame rates")) {
        QJsonArray json_rates = json_ui["show frame rates"].toArray();
        int rates[] = { 30, 24, 18, 12, 6 };
        for (int i = 0; i < 5; i++)
            shown_frame_rates[i] = json_rates.contains(rates[i]);
    } else {
        shown_frame_rates = { true, false, true, true, true };
    }

    mic_search_minimum = json_ui["mic search minimum"].toInt(mic_search_minimum);
    c_match_sequences_minimum = json_ui["c match sequences minimum"].toInt(c_match_sequences_minimum);

    QJsonObject json_pattern_guessing = json_ui["pattern guessing"].toObject();

    if (!json_pattern_guessing.isEmpty()) {
        std::unordered_map<std::string, int> guessing_methods = {
            { "from matches", PatternGuessingFromMatches },
            { "from mics", PatternGuessingFromMics }
        };
        try {
            pattern_guessing.method = guessing_methods.at(json_pattern_guessing["method"].toString("from mics").toStdString());
        } catch (std::out_of_range &) {

        }

        pattern_guessing.minimum_length = json_pattern_guessing["minimum length"].toInt();

        std::unordered_map<std::string, int> third_n_match = {
            { "always", UseThirdNMatchAlways },
            { "never", UseThirdNMatchNever },
            { "if it has lower mic", UseThirdNMatchIfPrettier }
        };
        try {
            pattern_guessing.third_n_match = third_n_match.at(json_pattern_guessing["use third n match"].toString("never").toStdString());
        } catch (std::out_of_range &) {

        }

        std::unordered_map<std::string, int> decimate = {
            { "first duplicate", DropFirstDuplicate },
            { "second duplicate", DropSecondDuplicate },
            { "duplicate with higher mic per cycle", DropUglierDuplicatePerCycle },
            { "duplicate with higher mic per section", DropUglierDuplicatePerSection }
        };
        try {
            pattern_guessing.decimation = decimate.at(json_pattern_guessing["decimate"].toString("first duplicate").toStdString());
        } catch (std::out_of_range &) {

        }

        std::unordered_map<std::string, int> use_patterns = {
            { "cccnn", PatternCCCNN },
            { "ccnnn", PatternCCNNN },
            { "ccccc", PatternCCCCC }
        };
        QJsonArray json_use_patterns = json_pattern_guessing["use patterns"].toArray();
        for (int i = 0; i < json_use_patterns.size(); i++) {
            try {
                pattern_guessing.use_patterns |= use_patterns.at(json_use_patterns[i].toString().toStdString());
            } catch (std::out_of_range &) {

            }
        }

        QJsonArray json_failures = json_pattern_guessing["failures"].toArray();

        std::unordered_map<std::string, int> reasons = {
            { "section too short", SectionTooShort },
            { "ambiguous pattern", AmbiguousMatchPattern }
        };
        for (int i = 0; i < json_failures.size(); i++) {
            QJsonObject json_failure = json_failures[i].toObject();
            FailedPatternGuessing fail;
            fail.start = json_failure["start"].toInt();
            try {
                fail.reason = reasons.at(json_failure["reason"].toString("ambiguous pattern").toStdString());
            } catch (std::out_of_range &) {
                fail.reason = AmbiguousMatchPattern;
            }

            pattern_guessing.failures.insert({ fail.start, fail });
        }
    }


    setNumFrames(PostSource, 0);

    QJsonArray json_trims = json_project["trim"].toArray();
    for (int i = 0; i < json_trims.size(); i++) {
        QJsonArray json_trim = json_trims[i].toArray();
        FrameRange range;
        range.first = json_trim[0].toInt();
        range.last = json_trim[1].toInt();
        trims.insert(std::make_pair(range.first, range));
        setNumFrames(PostSource, getNumFrames(PostSource) + (range.last - range.first + 1));
    }

    setNumFrames(PostDecimate, getNumFrames(PostSource));

    QJsonObject json_vfm_parameters = json_project["vfm parameters"].toObject();

    QStringList keys = json_vfm_parameters.keys();
    for (int i = 0; i < keys.size(); i++)
        vfm_parameters.insert(std::make_pair(keys.at(i).toStdString(), json_vfm_parameters[keys.at(i)].toDouble()));


    QJsonObject json_vdecimate_parameters = json_project["vdecimate parameters"].toObject();

    keys = json_vdecimate_parameters.keys();
    for (int i = 0; i < keys.size(); i++)
        vdecimate_parameters.insert(std::make_pair(keys.at(i).toStdString(), json_vdecimate_parameters[keys.at(i)].toDouble()));


    QJsonArray json_mics, json_matches, json_original_matches, json_combed_frames, json_decimated_frames, json_decimate_metrics;


    json_mics = json_project["mics"].toArray();
    if (json_mics.size()) {
        mics.resize(getNumFrames(PostSource), { 0 });
        for (int i = 0; i < json_mics.size(); i++) {
            QJsonArray json_mic = json_mics[i].toArray();
            for (int j = 0; j < 5; j++)
                mics[i][j] = (int16_t)json_mic[j].toDouble();
        }
    }


    json_matches = json_project["matches"].toArray();
    if (json_matches.size()) {
        matches.resize(getNumFrames(PostSource), 'c');
        for (int i = 0; i < std::min(json_matches.size(), (int)matches.size()); i++)
            matches[i] = json_matches[i].toString().toStdString()[0];
    }


    json_original_matches = json_project["original matches"].toArray();
    if (json_original_matches.size()) {
        original_matches.resize(getNumFrames(PostSource), 'c');
        for (int i = 0; i < std::min(json_original_matches.size(), (int)original_matches.size()); i++)
            original_matches[i] = json_original_matches[i].toString().toStdString()[0];
    }


    json_combed_frames = json_project["combed frames"].toArray();
    for (int i = 0; i < json_combed_frames.size(); i++)
        addCombedFrame(json_combed_frames[i].toInt());


    decimated_frames.resize((getNumFrames(PostSource) - 1) / 5 + 1);
    json_decimated_frames = json_project["decimated frames"].toArray();
    for (int i = 0; i < json_decimated_frames.size(); i++)
        addDecimatedFrame(json_decimated_frames[i].toInt());

    // num_frames[PostDecimate] is correct at this point.

    json_decimate_metrics = json_project["decimate metrics"].toArray();
    if (json_decimate_metrics.size()) {
        decimate_metrics.resize(getNumFrames(PostSource), 0);
        for (int i = 0; i < std::min(json_decimate_metrics.size(), (int)decimate_metrics.size()); i++)
            decimate_metrics[i] = json_decimate_metrics[i].toInt();
    }


    QJsonArray json_presets, json_frozen_frames;

    json_presets = json_project["presets"].toArray();
    for (int i = 0; i < json_presets.size(); i++) {
        QJsonObject json_preset = json_presets[i].toObject();
        addPreset(json_preset["name"].toString().toStdString(), json_preset["contents"].toString().toStdString());
    }


    json_frozen_frames = json_project["frozen frames"].toArray();
    for (int i = 0; i < json_frozen_frames.size(); i++) {
        QJsonArray json_ff = json_frozen_frames[i].toArray();
        addFreezeFrame(json_ff[0].toInt(), json_ff[1].toInt(), json_ff[2].toInt());
    }


    QJsonArray json_sections, json_custom_lists;

    json_sections = json_project["sections"].toArray();

    for (int j = 0; j < json_sections.size(); j++) {
        QJsonObject json_section = json_sections[j].toObject();
        int section_start = json_section["start"].toInt();
        Section section(section_start);
        json_presets = json_section["presets"].toArray();
        section.presets.resize(json_presets.size());
        for (int k = 0; k < json_presets.size(); k++)
            section.presets[k] = json_presets[k].toString().toStdString();

        addSection(section);
    }

    if (json_sections.size() == 0) {
        addSection(0);
    }

    json_custom_lists = json_project["custom lists"].toArray();

    custom_lists.reserve(json_custom_lists.size());

    for (int i = 0; i < json_custom_lists.size(); i++) {
        QJsonObject json_list = json_custom_lists[i].toObject();

        CustomList list(json_list["name"].toString().toStdString(),
                json_list["preset"].toString().toStdString(),
                json_list["position"].toInt());

        addCustomList(list);

        QJsonArray json_frames = json_list["frames"].toArray();
        for (int j = 0; j < json_frames.size(); j++) {
            QJsonArray json_range = json_frames[j].toArray();
            addCustomListRange(i, json_range[0].toInt(), json_range[1].toInt());
        }
    }


    QJsonObject json_resize, json_crop, json_depth;

    json_resize = json_project["resize"].toObject();
    resize.enabled = !json_resize.isEmpty();
    resize.width = json_resize["width"].toInt(width);
    resize.height = json_resize["height"].toInt(height);
    resize.filter = json_resize["filter"].toString().toStdString();

    json_crop = json_project["crop"].toObject();
    crop.enabled = !json_crop.isEmpty();
    crop.early = json_crop["early"].toBool();
    crop.left = json_crop["left"].toInt();
    crop.top = json_crop["top"].toInt();
    crop.right = json_crop["right"].toInt();
    crop.bottom = json_crop["bottom"].toInt();

    json_depth = json_project["depth"].toObject();
    depth.enabled = !json_depth.isEmpty();
    if (depth.enabled) {
        depth.bits = json_depth["bits"].toInt();
        depth.float_samples = json_depth["float samples"].toBool();
        depth.dither = json_depth["dither"].toString().toStdString();
    }

    source_filter = json_project["source filter"].toString().toStdString();


    QJsonArray json_interlaced_fades = json_project["interlaced fades"].toArray();

    for (int i = 0; i < json_interlaced_fades.size(); i++) {
        QJsonObject json_interlaced_fade = json_interlaced_fades[i].toObject();

        int frame = json_interlaced_fade["frame"].toInt();
        double field_difference = json_interlaced_fade["field difference"].toDouble();

        interlaced_fades.insert({ frame, { frame, field_difference } });
    }
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
        auto it = frozen_frames.upper_bound(first);
        if (it != frozen_frames.cend() && it->second.first < last)
            overlap = &it->second;
    }

    if (overlap)
        throw WobblyException("Can't add FreezeFrame (" + std::to_string(first) + "," + std::to_string(last) + "," + std::to_string(replacement) + "): overlaps (" + std::to_string(overlap->first) + "," + std::to_string(overlap->last) + "," + std::to_string(overlap->replacement) + ").");

    FreezeFrame ff = {
        .first = first,
        .last = last,
        .replacement = replacement
    };
    frozen_frames.insert(std::make_pair(first, ff));
}

void WobblyProject::deleteFreezeFrame(int frame) {
    frozen_frames.erase(frame);
}

const FreezeFrame *WobblyProject::findFreezeFrame(int frame) const {
    if (!frozen_frames.size())
        return nullptr;

    auto it = frozen_frames.upper_bound(frame);

    it--;

    if (it->second.first <= frame && frame <= it->second.last)
        return &it->second;

    return nullptr;
}


std::vector<FreezeFrame> WobblyProject::getFreezeFrames() const {

    std::vector<FreezeFrame> ff;

    ff.reserve(frozen_frames.size());

    for (auto it = frozen_frames.cbegin(); it != frozen_frames.cend(); it++)
        ff.push_back(it->second);

    return ff;
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

    Preset preset;
    preset.name = preset_name;
    preset.contents = preset_contents;
    auto ret = presets.insert(std::make_pair(preset_name, preset));
    if (!ret.second)
        throw WobblyException("Can't add preset '" + preset_name + "': preset name already in use.");
}

void WobblyProject::renamePreset(const std::string &old_name, const std::string &new_name) {
    if (old_name == new_name)
        return;

    if (!presets.count(old_name))
        throw WobblyException("Can't rename preset '" + old_name + "' to '" + new_name + "': no such preset.");

    if (!isNameSafeForPython(new_name))
        throw WobblyException("Can't rename preset '" + old_name + "' to '" + new_name + "': new name is invalid. Use only letters, numbers, and the underscore character. The first character cannot be a number.");

    Preset preset;
    preset.name = new_name;
    preset.contents = presets.at(old_name).contents;

    presets.erase(old_name);
    presets.insert(std::make_pair(new_name, preset));

    for (auto it = sections.begin(); it != sections.end(); it++)
        for (size_t j = 0; j < it->second.presets.size(); j++)
            if (it->second.presets[j] == old_name)
                it->second.presets[j] = new_name;

    for (auto it = custom_lists.begin(); it != custom_lists.end(); it++)
        if (it->preset == old_name)
            it->preset = new_name;
}

void WobblyProject::deletePreset(const std::string &preset_name) {
    if (presets.erase(preset_name) == 0)
        throw WobblyException("Can't delete preset '" + preset_name + "': no such preset.");

    for (auto it = sections.begin(); it != sections.end(); it++)
        for (size_t j = 0; j < it->second.presets.size(); j++)
            if (it->second.presets[j] == preset_name)
                it->second.presets.erase(it->second.presets.cbegin() + j);

    for (auto it = custom_lists.begin(); it != custom_lists.end(); it++)
        if (it->preset == preset_name)
            it->preset.clear();
}

std::vector<std::string> WobblyProject::getPresets() const {
    std::vector<std::string> preset_list;

    preset_list.reserve(presets.size());

    for (auto it = presets.cbegin(); it != presets.cend(); it++)
        preset_list.push_back(it->second.name);

    return preset_list;
}

const std::string &WobblyProject::getPresetContents(const std::string &preset_name) const {
    if (!presets.count(preset_name))
        throw WobblyException("Can't retrieve the contents of preset '" + preset_name + "': no such preset.");

    const Preset &preset = presets.at(preset_name);
    return preset.contents;
}

void WobblyProject::setPresetContents(const std::string &preset_name, const std::string &preset_contents) {
    if (!presets.count(preset_name))
        throw WobblyException("Can't modify the contents of preset '" + preset_name + "': no such preset.");

    Preset &preset = presets.at(preset_name);
    preset.contents = preset_contents;
}


bool WobblyProject::isPresetInUse(const std::string &preset_name) const {
    if (!presets.count(preset_name))
        throw WobblyException("Can't check if preset '" + preset_name + "' is in use: no such preset.");

    for (auto it = sections.begin(); it != sections.end(); it++)
        for (size_t j = 0; j < it->second.presets.size(); j++)
            if (it->second.presets[j] == preset_name)
                return true;

    for (auto it = custom_lists.begin(); it != custom_lists.end(); it++)
        if (it->preset == preset_name)
            return true;

    return false;
}


bool WobblyProject::presetExists(const std::string &preset_name) const {
    return (bool)presets.count(preset_name);
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
}


void WobblyProject::addSection(int section_start) {
    Section section(section_start);
    addSection(section);
}

void WobblyProject::addSection(const Section &section) {
    if (section.start < 0 || section.start >= getNumFrames(PostSource))
        throw WobblyException("Can't add section starting at " + std::to_string(section.start) + ": value out of range.");

    sections.insert(std::make_pair(section.start, section));
}

void WobblyProject::deleteSection(int section_start) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't delete section starting at " + std::to_string(section_start) + ": value out of range.");

    if (!sections.count(section_start))
        throw WobblyException("Can't delete section starting at " + std::to_string(section_start) + ": no such section.");

    // Never delete the very first section.
    if (section_start > 0)
        sections.erase(section_start);
}

Section *WobblyProject::findSection(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't find the section frame " + std::to_string(frame) + " belongs to: frame number out of range.");

    auto it = sections.upper_bound(frame);
    it--;
    return &it->second;
}

Section *WobblyProject::findNextSection(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't find the section after frame " + std::to_string(frame) + ": frame number out of range.");

    auto it = sections.upper_bound(frame);

    if (it != sections.cend())
        return &it->second;

    return nullptr;
}

int WobblyProject::getSectionEnd(int frame) {
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

    if (!sections.count(section_start))
        throw WobblyException("Can't add preset '" + preset_name + "' to section starting at " + std::to_string(section_start) + ": no such section.");

    if (!presets.count(preset_name))
        throw WobblyException("Can't add preset '" + preset_name + "' to section starting at " + std::to_string(section_start) + ": no such preset.");

    // The user may want to assign the same preset twice.
    sections.at(section_start).presets.push_back(preset_name);
}

void WobblyProject::setSectionMatchesFromPattern(int section_start, const std::string &pattern) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't apply match pattern to section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections.count(section_start))
        throw WobblyException("Can't apply match pattern to section starting at " + std::to_string(section_start) + ": no such section.");

    int section_end = getSectionEnd(section_start);

    setRangeMatchesFromPattern(section_start, section_end - 1, pattern);
}

void WobblyProject::setSectionDecimationFromPattern(int section_start, const std::string &pattern) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't apply decimation pattern to section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections.count(section_start))
        throw WobblyException("Can't apply decimation pattern to section starting at " + std::to_string(section_start) + ": no such section.");

    int section_end = getSectionEnd(section_start);

    setRangeDecimationFromPattern(section_start, section_end - 1, pattern);
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
}


void WobblyProject::resetSectionMatches(int section_start) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't reset the matches for section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections.count(section_start))
        throw WobblyException("Can't reset the matches for section starting at " + std::to_string(section_start) + ": no such section.");

    int section_end = getSectionEnd(section_start);

    resetRangeMatches(section_start, section_end - 1);
}


const std::vector<CustomList> &WobblyProject::getCustomLists() const {
    return custom_lists;
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

    if (list.preset.size() && presets.count(list.preset) == 0)
        throw WobblyException("Can't add custom list '" + list.name + "' with preset '" + list.preset + "': no such preset.");

    for (size_t i = 0; i < custom_lists.size(); i++)
        if (custom_lists[i].name == list.name)
            throw WobblyException("Can't add custom list '" + list.name + "': a list with this name already exists.");

    custom_lists.push_back(list);
}

void WobblyProject::renameCustomList(const std::string &old_name, const std::string &new_name) {
    if (old_name == new_name)
        return;

    size_t index = custom_lists.size();

    for (size_t i = 0; i < custom_lists.size(); i++)
        if (custom_lists[i].name == old_name) {
            index = i;
            break;
        }

    if (index == custom_lists.size())
        throw WobblyException("Can't rename custom list '" + old_name + "': no such list.");

    for (size_t i = 0; i < custom_lists.size(); i++)
        if (custom_lists[i].name == new_name)
            throw WobblyException("Can't rename custom list '" + old_name + "' to '" + new_name + "': new name is already in use.");

    if (!isNameSafeForPython(new_name))
        throw WobblyException("Can't rename custom list '" + old_name + "' to '" + new_name + "': new name is invalid. Use only letters, numbers, and the underscore character. The first character cannot be a number.");

    custom_lists[index].name = new_name;
}

void WobblyProject::deleteCustomList(const std::string &list_name) {
    for (size_t i = 0; i < custom_lists.size(); i++)
        if (custom_lists[i].name == list_name) {
            deleteCustomList(i);
            return;
        }

    throw WobblyException("Can't delete custom list with name '" + list_name + "': no such list.");
}

void WobblyProject::deleteCustomList(int list_index) {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't delete custom list with index " + std::to_string(list_index) + ": index out of range.");

    custom_lists.erase(custom_lists.cbegin() + list_index);
}


void WobblyProject::moveCustomListUp(int list_index) {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't move up custom list with index " + std::to_string(list_index) + ": index out of range.");

    if (list_index == 0)
        return;

    std::swap(custom_lists[list_index - 1], custom_lists[list_index]);
}


void WobblyProject::moveCustomListDown(int list_index) {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't move down custom list with index " + std::to_string(list_index) + ": index out of range.");

    if (list_index == (int)custom_lists.size() - 1)
        return;

    std::swap(custom_lists[list_index], custom_lists[list_index + 1]);
}


void WobblyProject::setCustomListPreset(int list_index, const std::string &preset_name) {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't assign preset '" + preset_name + "' to custom list with index " + std::to_string(list_index) + ": index out of range.");

    if (!presets.count(preset_name))
        throw WobblyException("Can't assign preset '" + preset_name + "' to custom list '" + custom_lists[list_index].name + "': no such preset.");

    custom_lists[list_index].preset = preset_name;
}


void WobblyProject::setCustomListPosition(int list_index, PositionInFilterChain position) {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't set the position of the custom list with index " + std::to_string(list_index) + ": index out of range.");

    if (position < 0 || position > 2)
        throw WobblyException("Can't put custom list '" + custom_lists[list_index].name + "' in position " + std::to_string(position) + ": position out of range.");

    custom_lists[list_index].position = position;
}


void WobblyProject::addCustomListRange(int list_index, int first, int last) {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't add a new range to custom list with index " + std::to_string(list_index) + ": index out of range.");

    if (first < 0 || first >= getNumFrames(PostSource) ||
        last < 0 || last >= getNumFrames(PostSource))
        throw WobblyException("Can't add range (" + std::to_string(first) + "," + std::to_string(last) + ") to custom list '" + custom_lists[list_index].name + "': values out of range.");

    auto &ranges = custom_lists[list_index].ranges;

    if (first > last)
        std::swap(first, last);

    const FrameRange *overlap = findCustomListRange(list_index, first);
    if (!overlap)
        overlap = findCustomListRange(list_index, last);
    if (!overlap) {
        auto it = ranges.upper_bound(first);
        if (it != ranges.cend() && it->second.first < last)
            overlap = &it->second;
    }

    if (overlap)
        throw WobblyException("Can't add range (" + std::to_string(first) + "," + std::to_string(last) + ") to custom list '" + custom_lists[list_index].name + "': overlaps range (" + std::to_string(overlap->first) + "," + std::to_string(overlap->last) + ").");

    ranges.insert({ first, { first, last } });
}


void WobblyProject::deleteCustomListRange(int list_index, int first) {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't delete a range from custom list with index " + std::to_string(list_index) + ": index out of range.");

    auto &ranges = custom_lists[list_index].ranges;

    if (!ranges.count(first))
        throw WobblyException("Can't delete range starting at frame " + std::to_string(first) + " from custom list '" + custom_lists[list_index].name + "': no such range.");

    custom_lists[list_index].ranges.erase(first);
}


const FrameRange *WobblyProject::findCustomListRange(int list_index, int frame) const {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't find a range in custom list with index " + std::to_string(list_index) + ": index out of range.");

    const auto &ranges = custom_lists[list_index].ranges;

    if (!ranges.size())
        return nullptr;

    auto it = ranges.upper_bound(frame);

    if (it == ranges.cbegin())
        return nullptr;

    it--;

    if (it->second.first <= frame && frame <= it->second.last)
        return &it->second;

    return nullptr;
}


bool WobblyProject::customListExists(const std::string &list_name) const {
    for (size_t i = 0; i < custom_lists.size(); i++)
        if (custom_lists[i].name == list_name)
            return true;

    return false;
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

    auto result = decimated_frames[frame / 5].insert(frame % 5);

    if (result.second)
        setNumFrames(PostDecimate, getNumFrames(PostDecimate) - 1);
}


void WobblyProject::deleteDecimatedFrame(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't delete decimated frame " + std::to_string(frame) + ": value out of range.");

    size_t result = decimated_frames[frame / 5].erase(frame % 5);

    if (result)
        setNumFrames(PostDecimate, getNumFrames(PostDecimate) + 1);
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


std::vector<DecimationRange> WobblyProject::getDecimationRanges() const {
    std::vector<DecimationRange> ranges;

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

std::vector<DecimationPatternRange> WobblyProject::getDecimationPatternRanges() const {
    std::vector<DecimationPatternRange> ranges;

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


void WobblyProject::addCombedFrame(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't mark frame " + std::to_string(frame) + " as combed: value out of range.");

    combed_frames.insert(frame);
}


void WobblyProject::deleteCombedFrame(int frame) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't mark frame " + std::to_string(frame) + " as not combed: value out of range.");

    combed_frames.erase(frame);
}


bool WobblyProject::isCombedFrame(int frame) const {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't check if frame " + std::to_string(frame) + " is combed: value out of range.");

    return (bool)combed_frames.count(frame);
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
}


void WobblyProject::setResizeEnabled(bool enabled) {
    resize.enabled = enabled;
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
}


void WobblyProject::setCropEnabled(bool enabled) {
    crop.enabled = enabled;
}


bool WobblyProject::isCropEnabled() const {
    return crop.enabled;
}


void WobblyProject::setCropEarly(bool early) {
    crop.early = early;
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
}


void WobblyProject::setBitDepthEnabled(bool enabled) {
    depth.enabled = enabled;
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
}


bool WobblyProject::guessSectionPatternsFromMics(int section_start, int minimum_length, int use_patterns, int drop_duplicate) {
    if (!mics.size())
        throw WobblyException("Can't guess patterns from mics because there are no mics in the project.");

    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't guess patterns from mics for section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections.count(section_start))
        throw WobblyException("Can't reset patterns from mics for section starting at " + std::to_string(section_start) + ": no such section.");


    int section_end = getSectionEnd(section_start);

    if (section_end - section_start < minimum_length) {
        FailedPatternGuessing failure;
        failure.start = section_start;
        failure.reason = SectionTooShort;
        pattern_guessing.failures.erase(failure.start);
        pattern_guessing.failures.insert({ failure.start, failure });

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
        int first_duplicate = (4 + patterns[best_pattern].pattern_offset) % 5;

        applyPatternGuessingDecimation(section_start, section_end, first_duplicate, drop_duplicate);
    }

    return true;
}


void WobblyProject::guessProjectPatternsFromMics(int minimum_length, int use_patterns, int drop_duplicate) {
    pattern_guessing.failures.clear();

    for (auto it = sections.cbegin(); it != sections.cend(); it++)
        guessSectionPatternsFromMics(it->second.start, minimum_length, use_patterns, drop_duplicate);

    pattern_guessing.method = PatternGuessingFromMics;
    pattern_guessing.minimum_length = minimum_length;
    pattern_guessing.use_patterns = use_patterns;
    pattern_guessing.decimation = drop_duplicate;
}


bool WobblyProject::guessSectionPatternsFromMatches(int section_start, int minimum_length, int use_third_n_match, int drop_duplicate) {
    if (section_start < 0 || section_start >= getNumFrames(PostSource))
        throw WobblyException("Can't guess patterns from matches for section starting at " + std::to_string(section_start) + ": frame number out of range.");

    if (!sections.count(section_start))
        throw WobblyException("Can't reset patterns from matches for section starting at " + std::to_string(section_start) + ": no such section.");

    int section_end = getSectionEnd(section_start);

    if (section_end - section_start < minimum_length) {
        FailedPatternGuessing failure;
        failure.start = section_start;
        failure.reason = SectionTooShort;
        pattern_guessing.failures.erase(failure.start);
        pattern_guessing.failures.insert({ failure.start, failure });

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
        return true;
    } else {
        // A pattern was not found.
        FailedPatternGuessing failure;
        failure.start = section_start;
        failure.reason = AmbiguousMatchPattern;
        pattern_guessing.failures.erase(failure.start);
        pattern_guessing.failures.insert({ failure.start, failure });
        return false;
    }
}


void WobblyProject::guessProjectPatternsFromMatches(int minimum_length, int use_third_n_match, int drop_duplicate) {
    pattern_guessing.failures.clear();

    for (auto it = sections.cbegin(); it != sections.cend(); it++)
        guessSectionPatternsFromMatches(it->second.start, minimum_length, use_third_n_match, drop_duplicate);

    pattern_guessing.method = PatternGuessingFromMatches;
    pattern_guessing.minimum_length = minimum_length;
    pattern_guessing.third_n_match = use_third_n_match;
    pattern_guessing.decimation = drop_duplicate;
}


const PatternGuessing &WobblyProject::getPatternGuessing() {
    return pattern_guessing;
}


void WobblyProject::addInterlacedFade(int frame, double field_difference) {
    if (frame < 0 || frame >= getNumFrames(PostSource))
        throw WobblyException("Can't add interlaced fade at frame " + std::to_string(frame) + ": frame number out of range.");

    interlaced_fades.insert({ frame, { frame, field_difference } });
}


const std::map<int, InterlacedFade> &WobblyProject::getInterlacedFades() const {
    return interlaced_fades;
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

    std::map<int, Section> merged_sections;
    merged_sections.insert({ 0, sections.cbegin()->second });

    for (auto it = ++(sections.cbegin()); it != sections.cend(); it++)
        if (!samePresets(it->second.presets, merged_sections.crbegin()->second.presets))
            merged_sections.insert({ it->first, it->second });


    std::string splice = "src = c.std.Splice(mismatch=True, clips=[";
    for (auto it = merged_sections.cbegin(); it != merged_sections.cend(); it++) {
        std::string section_name = "section";
        section_name += std::to_string(it->second.start);
        script += section_name + " = src";

        for (size_t i = 0; i < it->second.presets.size(); i++) {
            script += "\n";
            script += section_name + " = ";
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
    for (size_t i = 0; i < custom_lists.size(); i++) {
        // Ignore lists that are in a different position in the filter chain.
        if (custom_lists[i].position != position)
            continue;

        // Ignore lists with no frame ranges.
        if (!custom_lists[i].ranges.size())
            continue;

        // Complain if the custom list doesn't have a preset assigned.
        if (!custom_lists[i].preset.size())
            throw WobblyException("Custom list '" + custom_lists[i].name + "' has no preset assigned.");


        std::string list_name = "cl_";
        list_name += custom_lists[i].name;

        script += list_name + " = " + custom_lists[i].preset + "(src)\n";

        std::string splice = "src = c.std.Splice(mismatch=True, clips=[";

        auto it = custom_lists[i].ranges.cbegin();
        auto it_prev = it;

        if (it->second.first > 0) {
            splice += "src[0:";
            splice += std::to_string(maybeTranslate(it->second.first, false, position)) + "],";
        }

        splice += list_name + "[" + std::to_string(maybeTranslate(it->second.first, false, position)) + ":" + std::to_string(maybeTranslate(it->second.last, true, position) + 1) + "],";

        it++;
        for ( ; it != custom_lists[i].ranges.cend(); it++, it_prev++) {
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
            "import vapoursynth as vs\n"
            "\n"
            "c = vs.get_core()\n"
            "\n";
}

void WobblyProject::presetsToScript(std::string &script) const {
    for (auto it = presets.cbegin(); it != presets.cend(); it++) {
        if (!isPresetInUse(it->second.name))
            continue;

        script += "def " + it->second.name + "(clip):\n";
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

void WobblyProject::sourceToScript(std::string &script) const {
    script +=
            "try:\n"
            "    src = vs.get_output(index=1)\n"
            "except KeyError:\n"
            "    src = c." + source_filter + "(r'" + input_file + "')\n"
            "    src.set_output(index=1)\n"
            "\n";
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

    for (auto it = frozen_frames.cbegin(); it != frozen_frames.cend(); it++) {
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

    const std::vector<DecimationRange> &decimation_ranges = getDecimationRanges();

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

    const std::vector<DecimationPatternRange> &decimation_pattern_ranges = getDecimationPatternRanges();

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

void WobblyProject::showCropToScript(std::string &script) const {
    script += "src = c.std.AddBorders(clip=src, left=";
    script += std::to_string(crop.left) + ", top=";
    script += std::to_string(crop.top) + ", right=";
    script += std::to_string(crop.right) + ", bottom=";
    script += std::to_string(crop.bottom) + ", color=[128, 230, 180])\n\n";
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

std::string WobblyProject::generateFinalScript() const {
    // XXX Insert comments before and after each part.
    std::string script;

    headerToScript(script);

    presetsToScript(script);

    sourceToScript(script);

    if (crop.early && crop.enabled)
        cropToScript(script);

    trimToScript(script);

    customListsToScript(script, PostSource);

    fieldHintToScript(script);

    customListsToScript(script, PostFieldMatch);

    sectionsToScript(script);

    if (frozen_frames.size())
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

std::string WobblyProject::generateMainDisplayScript(bool show_crop) const {
    std::string script;

    headerToScript(script);

    sourceToScript(script);

    trimToScript(script);

    fieldHintToScript(script);

    if (frozen_frames.size())
        freezeFramesToScript(script);

    if (show_crop && crop.enabled) {
        cropToScript(script);
        showCropToScript(script);
    }

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

    const std::vector<DecimationRange> &ranges = getDecimationRanges();

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
            const auto &p = other->getPresets();
            for (size_t i = 0; i < p.size(); i++) {
                std::string preset_name = p[i];

                bool rename_needed = presetExists(preset_name);
                while (presetExists(preset_name))
                    preset_name += "_imported";

                if (rename_needed) {
                    while (presetExists(preset_name) || other->presetExists(preset_name))
                        preset_name += "_imported";
                }

                other->renamePreset(p[i], preset_name); // changes to other aren't saved, so it's okay.
                if (imports.presets)
                    addPreset(preset_name, other->getPresetContents(preset_name));
            }
        }

        if (imports.custom_lists) {
            const auto &lists = other->getCustomLists();
            for (size_t i = 0; i < lists.size(); i++) {
                if (lists[i].preset.size() && !presetExists(lists[i].preset))
                    addPreset(lists[i].preset, other->getPresetContents(lists[i].preset));

                CustomList list = lists[i];
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
    } catch (WobblyException &e) {
        delete other;

        throw e;
    }
}
