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
    : num_frames{ 0, 0, 0 }
    , fps_num(0)
    , fps_den(0)
    , width(0)
    , height(0)
    , zoom(1)
    , last_visited_frame(0)
    , is_wobbly(_is_wobbly)
    , pattern_guessing{ 0, UseThirdNMatchNever, DropFirstDuplicate, { } }
    , resize{ false, 0, 0, "spline16" }
    , crop{ false, false, 0, 0, 0, 0 }
    , depth{ false, 8, false, "random" }
{

}


const std::string &WobblyProject::getProjectPath() {
    return project_path;
}


int WobblyProject::getNumFrames(PositionInFilterChain position) {
    return num_frames[position];
}


void WobblyProject::writeProject(const std::string &path) {
    QFile file(QString::fromStdString(path));

    if (!file.open(QIODevice::WriteOnly))
        throw WobblyException("Couldn't open project file. Error message: " + file.errorString());

    project_path = path;

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


    QJsonObject json_ui;
    json_ui.insert("zoom", zoom);
    json_ui.insert("last visited frame", last_visited_frame);
    json_ui.insert("geometry", QString::fromStdString(ui_geometry));
    json_ui.insert("state", QString::fromStdString(ui_state));
    json_project.insert("user interface", json_ui);


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
        json_decimate_metrics.append(decimate_metrics[i]);

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
            for (auto it = custom_lists[i].frames.cbegin(); it != custom_lists[i].frames.cend(); it++) {
                QJsonArray json_pair;
                json_pair.append(it->second.first);
                json_pair.append(it->second.last);
                json_frames.append(json_pair);
            }
            json_custom_list.insert("frames", json_frames);

            json_custom_lists.append(json_custom_list);
        }

        json_project.insert("custom lists", json_custom_lists);


    if (pattern_guessing.failures.size()) {
        QJsonObject json_pattern_guessing;

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

        json_project.insert("pattern guessing", json_pattern_guessing);
    }


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

    file.write(json_doc.toJson(QJsonDocument::Indented));
}

void WobblyProject::readProject(const std::string &path) {
    QFile file(QString::fromStdString(path));

    if (!file.open(QIODevice::ReadOnly))
        throw WobblyException("Couldn't open project file '" + path + "'. Error message: " + file.errorString().toStdString());

    project_path = path;

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
        nullptr
    };

    for (int i = 0; required_keys[i]; i++)
        if (!json_project.contains(required_keys[i]))
            throw WobblyException("Couldn't open project file '" + path + "': project is missing JSON key '" + required_keys[i] + "'.");


    //int version = (int)json_project["wobbly version"].toDouble();


    input_file = json_project["input file"].toString().toStdString();


    fps_num = (int64_t)json_project["input frame rate"].toArray()[0].toDouble();
    fps_den = (int64_t)json_project["input frame rate"].toArray()[1].toDouble();


    width = (int)json_project["input resolution"].toArray()[0].toDouble();
    height = (int)json_project["input resolution"].toArray()[1].toDouble();


    QJsonObject json_ui = json_project["user interface"].toObject();
    zoom = (int)json_ui["zoom"].toDouble(1);
    last_visited_frame = (int)json_ui["last visited frame"].toDouble(0);
    ui_state = json_ui["state"].toString().toStdString();
    ui_geometry = json_ui["geometry"].toString().toStdString();


    num_frames[PostSource] = 0;

    QJsonArray json_trims = json_project["trim"].toArray();
    for (int i = 0; i < json_trims.size(); i++) {
        QJsonArray json_trim = json_trims[i].toArray();
        FrameRange range;
        range.first = (int)json_trim[0].toDouble();
        range.last = (int)json_trim[1].toDouble();
        trims.insert(std::make_pair(range.first, range));
        num_frames[PostSource] += range.last - range.first + 1;
    }

    num_frames[PostFieldMatch] = num_frames[PostSource];

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
    mics.resize(num_frames[PostSource], { 0 });
    for (int i = 0; i < json_mics.size(); i++) {
        QJsonArray json_mic = json_mics[i].toArray();
        for (int j = 0; j < 5; j++)
            mics[i][j] = (int16_t)json_mic[j].toDouble();
    }


    json_matches = json_project["matches"].toArray();
    matches.resize(num_frames[PostSource], 'c');
    for (int i = 0; i < std::min(json_matches.size(), (int)matches.size()); i++)
        matches[i] = json_matches[i].toString().toStdString()[0];


    json_original_matches = json_project["original matches"].toArray();
    original_matches.resize(num_frames[PostSource], 'c');
    for (int i = 0; i < std::min(json_original_matches.size(), (int)original_matches.size()); i++)
        original_matches[i] = json_original_matches[i].toString().toStdString()[0];

    if (json_matches.size() == 0 && json_original_matches.size() != 0) {
        memcpy(matches.data(), original_matches.data(), matches.size());
    }


    json_combed_frames = json_project["combed frames"].toArray();
    for (int i = 0; i < json_combed_frames.size(); i++)
        addCombedFrame((int)json_combed_frames[i].toDouble());


    decimated_frames.resize((num_frames[PostSource] - 1) / 5 + 1);
    json_decimated_frames = json_project["decimated frames"].toArray();
    for (int i = 0; i < json_decimated_frames.size(); i++)
        addDecimatedFrame((int)json_decimated_frames[i].toDouble());

    // num_frames[PostDecimate] is correct at this point.

    json_decimate_metrics = json_project["decimate metrics"].toArray();
    decimate_metrics.resize(num_frames[PostSource], 0);
    for (int i = 0; i < std::min(json_decimate_metrics.size(), (int)decimate_metrics.size()); i++)
        decimate_metrics[i] = (int)json_decimate_metrics[i].toDouble();


    QJsonArray json_presets, json_frozen_frames;

    json_presets = json_project["presets"].toArray();
    for (int i = 0; i < json_presets.size(); i++) {
        QJsonObject json_preset = json_presets[i].toObject();
        addPreset(json_preset["name"].toString().toStdString(), json_preset["contents"].toString().toStdString());
    }


    json_frozen_frames = json_project["frozen frames"].toArray();
    for (int i = 0; i < json_frozen_frames.size(); i++) {
        QJsonArray json_ff = json_frozen_frames[i].toArray();
        addFreezeFrame((int)json_ff[0].toDouble(), (int)json_ff[1].toDouble(), (int)json_ff[2].toDouble());
    }


    QJsonArray json_sections, json_custom_lists;

    json_sections = json_project["sections"].toArray();

    for (int j = 0; j < json_sections.size(); j++) {
        QJsonObject json_section = json_sections[j].toObject();
        int section_start = (int)json_section["start"].toDouble();
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
                (int)json_list["position"].toDouble());

        QJsonArray json_frames = json_list["frames"].toArray();
        for (int j = 0; j < json_frames.size(); j++) {
            QJsonArray json_range = json_frames[j].toArray();
            list.addFrameRange((int)json_range[0].toDouble(), (int)json_range[1].toDouble());
        }

        addCustomList(list);
    }


    QJsonObject json_pattern_guessing = json_project["pattern guessing"].toObject();

    if (!json_pattern_guessing.isEmpty()) {
        pattern_guessing.minimum_length = (int)json_pattern_guessing["minimum length"].toDouble();

        std::unordered_map<std::string, int> third_n_match = {
            { "always", 0 },
            { "never", 1 },
            { "if it has lower mic", 2 }
        };
        pattern_guessing.third_n_match = third_n_match[json_pattern_guessing["use third n match"].toString("never").toStdString()];

        std::unordered_map<std::string, int> decimate = {
            { "first duplicate", 0 },
            { "second duplicate", 1 },
            { "duplicate with higher mic per cycle", 2 },
            { "duplicate with higher mic per section", 3 }
        };
        pattern_guessing.decimation = decimate[json_pattern_guessing["decimate"].toString("first duplicate").toStdString()];

        QJsonArray json_failures = json_pattern_guessing["failures"].toArray();

        std::unordered_map<std::string, int> reasons = {
            { "section too short", 0 },
            { "ambiguous pattern", 1 }
        };
        for (int i = 0; i < json_failures.size(); i++) {
            QJsonObject json_failure = json_failures[i].toObject();
            FailedPatternGuessing fail;
            fail.start = (int)json_failure["start"].toDouble();
            fail.reason = reasons[json_failure["reason"].toString().toStdString()];
            pattern_guessing.failures.insert({ fail.start, fail });
        }
    }


    QJsonObject json_resize, json_crop, json_depth;

    json_resize = json_project["resize"].toObject();
    resize.enabled = !json_resize.isEmpty();
    resize.width = (int)json_resize["width"].toDouble(width);
    resize.height = (int)json_resize["height"].toDouble(height);
    resize.filter = json_resize["filter"].toString().toStdString();

    json_crop = json_project["crop"].toObject();
    crop.enabled = !json_crop.isEmpty();
    crop.early = json_crop["early"].toBool();
    crop.left = (int)json_crop["left"].toDouble();
    crop.top = (int)json_crop["top"].toDouble();
    crop.right = (int)json_crop["right"].toDouble();
    crop.bottom = (int)json_crop["bottom"].toDouble();

    json_depth = json_project["depth"].toObject();
    depth.enabled = !json_depth.isEmpty();
    if (depth.enabled) {
        depth.bits = (int)json_depth["bits"].toDouble();
        depth.float_samples = json_depth["float samples"].toBool();
        depth.dither = json_depth["dither"].toString().toStdString();
    }
}

void WobblyProject::addFreezeFrame(int first, int last, int replacement) {
    if (first > last)
        std::swap(first, last);

    if (first < 0 || first >= num_frames[PostSource] ||
        last < 0 || last >= num_frames[PostSource] ||
        replacement < 0 || replacement >= num_frames[PostSource])
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

const FreezeFrame *WobblyProject::findFreezeFrame(int frame) {
    if (!frozen_frames.size())
        return nullptr;

    auto it = frozen_frames.upper_bound(frame);

    it--;

    if (it->second.first <= frame && frame <= it->second.last)
        return &it->second;

    return nullptr;
}


std::vector<FreezeFrame> WobblyProject::getFreezeFrames() {

    std::vector<FreezeFrame> ff;

    ff.reserve(frozen_frames.size());

    for (auto it = frozen_frames.cbegin(); it != frozen_frames.cend(); it++)
        ff.push_back(it->second);

    return ff;
}


void WobblyProject::addPreset(const std::string &preset_name) {
    std::string contents =
            "# The preset is a Python function. It takes a single parameter, called 'clip'.\n"
            "# Filter that and assign the result to the same variable.\n"
            "# The VapourSynth core object is called 'c'.\n";
    addPreset(preset_name, contents);
}


bool WobblyProject::isNameSafeForPython(const std::string &name) {
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
    presets.insert(std::make_pair(preset_name, preset));
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

std::vector<std::string> WobblyProject::getPresets() {
    std::vector<std::string> preset_list;

    preset_list.reserve(presets.size());

    for (auto it = presets.cbegin(); it != presets.cend(); it++)
        preset_list.push_back(it->second.name);

    return preset_list;
}

const std::string &WobblyProject::getPresetContents(const std::string &preset_name) {
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


bool WobblyProject::isPresetInUse(const std::string &preset_name) {
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


const std::array<int16_t, 5> &WobblyProject::getMics(int frame) {
    return mics[frame];
}


char WobblyProject::getMatch(int frame) {
    return matches[frame];
}


void WobblyProject::setMatch(int frame, char match) {
    matches[frame] = match;
}


void WobblyProject::cycleMatchBCN(int frame) {
    // N -> C -> B. This is the order Yatta uses, so we use it.

    char match = matches[frame];

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

    matches[frame] = match;
}


void WobblyProject::cycleMatch(int frame) {
    // U -> B -> N -> C -> P

    char match = matches[frame];

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

    matches[frame] = match;
}


void WobblyProject::addSection(int section_start) {
    Section section(section_start);
    addSection(section);
}

void WobblyProject::addSection(const Section &section) {
    if (section.start < 0 || section.start >= num_frames[PostSource])
        throw WobblyException("Can't add section starting at " + std::to_string(section.start) + ": value out of range.");

    sections.insert(std::make_pair(section.start, section));
}

void WobblyProject::deleteSection(int section_start) {
    // Never delete the very first section.
    if (section_start > 0)
        sections.erase(section_start);
}

Section *WobblyProject::findSection(int frame) {
    auto it = sections.upper_bound(frame);
    it--;
    return &it->second;
}

Section *WobblyProject::findNextSection(int frame) {
    auto it = sections.upper_bound(frame);

    if (it != sections.cend())
        return &it->second;

    return nullptr;
}

int WobblyProject::getSectionEnd(int frame) {
    const Section *next_section = findNextSection(frame);
    if (next_section)
        return next_section->start;
    else
        return num_frames[PostSource];
}

void WobblyProject::setSectionPreset(int section_start, const std::string &preset_name) {
    // The user may want to assign the same preset twice.
    sections.at(section_start).presets.push_back(preset_name);
}

void WobblyProject::setSectionMatchesFromPattern(int section_start, const std::string &pattern) {
    int section_end = getSectionEnd(section_start);

    for (int i = 0; i < section_end - section_start; i++) {
        if ((section_start + i == 0 && (pattern[i % 5] == 'p' || pattern[i % 5] == 'b')) ||
            (section_start + i == num_frames[PostSource] - 1 && (pattern[i % 5] == 'n' || pattern[i % 5] == 'u')))
            // Skip the first and last frame if their new matches are incompatible.
            continue;

        // Yatta does it like this.
        matches[section_start + i] = pattern[i % 5];
    }
}

void WobblyProject::setSectionDecimationFromPattern(int section_start, const std::string &pattern) {
    int section_end = getSectionEnd(section_start);

    for (int i = 0; i < section_end - section_start; i++) {
        // Yatta does it like this.
        if (pattern[i % 5] == 'd')
            addDecimatedFrame(section_start + i);
        else
            deleteDecimatedFrame(section_start + i);
    }
}


void WobblyProject::resetRangeMatches(int start, int end) {
    if (start > end)
        std::swap(start, end);

    if (start < 0 || end >= num_frames[PostSource])
        throw WobblyException("Can't reset the matches for range [" + std::to_string(start) + "," + std::to_string(end) + "]: values out of range.");

    memcpy(matches.data() + start, original_matches.data() + start, end - start + 1);
}


void WobblyProject::resetSectionMatches(int section_start) {
    int section_end = getSectionEnd(section_start);

    resetRangeMatches(section_start, section_end - 1);
}


const std::vector<CustomList> &WobblyProject::getCustomLists() {
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

    custom_lists[list_index].addFrameRange(first, last);
}


void WobblyProject::deleteCustomListRange(int list_index, int first) {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't delete a range from custom list with index " + std::to_string(list_index) + ": index out of range.");

    custom_lists[list_index].deleteFrameRange(first);
}


const FrameRange *WobblyProject::findCustomListRange(int list_index, int frame) {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't find a range in custom list with index " + std::to_string(list_index) + ": index out of range.");

    return custom_lists[list_index].findFrameRange(frame);
}


int WobblyProject::getDecimateMetric(int frame) {
    return decimate_metrics[frame];
}


void WobblyProject::addDecimatedFrame(int frame) {
    if (frame < 0 || frame >= num_frames[PostSource])
        throw WobblyException("Can't mark frame " + std::to_string(frame) + " for decimation: value out of range.");

    auto result = decimated_frames[frame / 5].insert(frame % 5);

    if (result.second)
        num_frames[PostDecimate]--;
}


void WobblyProject::deleteDecimatedFrame(int frame) {
    if (frame < 0 || frame >= num_frames[PostSource])
        throw WobblyException("Can't delete decimated frame " + std::to_string(frame) + ": value out of range.");

    size_t result = decimated_frames[frame / 5].erase(frame % 5);

    if (result)
        num_frames[PostDecimate]++;
}


bool WobblyProject::isDecimatedFrame(int frame) {
    if (frame < 0 || frame >= num_frames[PostSource])
        throw WobblyException("Can't check if frame " + std::to_string(frame) + " is decimated: value out of range.");

    return (bool)decimated_frames[frame / 5].count(frame % 5);
}


void WobblyProject::clearDecimatedFramesFromCycle(int frame) {
    if (frame < 0 || frame >= num_frames[PostSource])
        throw WobblyException("Can't clear decimated frames from cycle containing frame " + std::to_string(frame) + ": value out of range.");

    int cycle = frame / 5;

    size_t new_frames = decimated_frames[cycle].size();

    decimated_frames[cycle].clear();

    num_frames[PostDecimate] += new_frames;
}


std::vector<DecimationRange> WobblyProject::getDecimationRanges() {
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

std::vector<DecimationPatternRange> WobblyProject::getDecimationPatternRanges() {
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


void WobblyProject::addCombedFrame(int frame) {
    if (frame < 0 || frame >= num_frames[PostSource])
        throw WobblyException("Can't mark frame " + std::to_string(frame) + " as combed: value out of range.");

    combed_frames.insert(frame);
}


void WobblyProject::deleteCombedFrame(int frame) {
    combed_frames.erase(frame);
}


bool WobblyProject::isCombedFrame(int frame) {
    return (bool)combed_frames.count(frame);
}


const Resize &WobblyProject::getResize() {
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


bool WobblyProject::isResizeEnabled() {
    return resize.enabled;
}


const Crop &WobblyProject::getCrop() {
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


bool WobblyProject::isCropEnabled() {
    return crop.enabled;
}


void WobblyProject::setCropEarly(bool early) {
    crop.early = early;
}


bool WobblyProject::isCropEarly() {
    return crop.early;
}


const Depth &WobblyProject::getBitDepth() {
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


bool WobblyProject::isBitDepthEnabled() {
    return depth.enabled;
}


int WobblyProject::getZoom() {
    return zoom;
}


void WobblyProject::setZoom(int ratio) {
    if (ratio < 1)
        throw WobblyException("Can't set zoom to ratio " + std::to_string(ratio) + ": invalid value.");

    zoom = ratio;
}


int WobblyProject::getLastVisitedFrame() {
    return last_visited_frame;
}


void WobblyProject::setLastVisitedFrame(int frame) {
    last_visited_frame = frame;
}


std::string WobblyProject::getUIState() {
    return ui_state;
}


void WobblyProject::setUIState(const std::string &state) {
    ui_state = state;
}


std::string WobblyProject::getUIGeometry() {
    return ui_geometry;
}


void WobblyProject::setUIGeometry(const std::string &geometry) {
    ui_geometry = geometry;
}


std::string WobblyProject::frameToTime(int frame) {
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


int WobblyProject::frameNumberAfterDecimation(int frame) {
    if (frame < 0)
        return 0;

    if (frame >= num_frames[PostSource])
        return num_frames[PostDecimate];

    int cycle_number = frame / 5;

    int position_in_cycle = frame % 5;

    int out_frame = cycle_number * 5;

    for (int i = 0; i < cycle_number; i++)
        out_frame -= decimated_frames[i].size();

    for (int8_t i = 0; i < position_in_cycle; i++)
        if (!decimated_frames[cycle_number].count(i))
            out_frame++;

    if (frame == num_frames[PostSource] - 1 && isDecimatedFrame(frame))
        out_frame--;

    return out_frame;
}


bool WobblyProject::guessSectionPatternsFromMatches(int section_start, int minimum_length, int use_third_n_match, int drop_duplicate) {
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

    for (int i = section_start; i < std::min(section_end, num_frames[PostSource] - 1); i++) {
        if (original_matches[i] == 'n' && original_matches[i + 1] == 'c') {
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

        // If the first duplicate is the last frame in the cycle, we have to drop the same duplicate in the entire section.
        if (drop_duplicate == DropUglierDuplicatePerCycle && best == 4)
            drop_duplicate = DropUglierDuplicatePerSection;

        int drop = -1;

        if (drop_duplicate == DropUglierDuplicatePerSection) {
            // Find the uglier duplicate.
            int drop_n = 0;
            int drop_c = 0;

            for (int i = section_start; i < std::min(section_end, num_frames[PostSource] - 1); i++) {
                if (i % 5 == best) {
                    int16_t mic_n = mics[i][matchCharToIndex('n')];
                    int16_t mic_c = mics[i + 1][matchCharToIndex('c')];
                    if (mic_n > mic_c)
                        drop_n++;
                    else
                        drop_c++;
                }
            }

            if (drop_n > drop_c)
                drop = best;
            else
                drop = (best + 1) % 5;
        } else if (drop_duplicate == DropFirstDuplicate) {
            drop = best;
        } else if (drop_duplicate == DropSecondDuplicate) {
            drop = (best + 1) % 5;
        }

        int first_cycle = section_start / 5;
        int last_cycle = (section_end - 1) / 5;
        for (int i = first_cycle; i < last_cycle + 1; i++) {
            if (drop_duplicate == DropUglierDuplicatePerCycle) {
                if (i == first_cycle) {
                    if (section_start % 5 > best + 1)
                        continue;
                    else if (section_start % 5 > best)
                        drop = best + 1;
                } else if (i == last_cycle) {
                    if ((section_end - 1) % 5 < best)
                        continue;
                    else if ((section_end - 1) % 5 < best + 1)
                        drop = best;
                }

                if (drop == -1) {
                    int16_t mic_n = mics[i * 5 + best][matchCharToIndex('n')];
                    int16_t mic_c = mics[i * 5 + best + 1][matchCharToIndex('c')];
                    if (mic_n > mic_c)
                        drop = best;
                    else
                        drop = (best + 1) % 5;
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


        // Now the matches.
        std::string patterns[5] = { "ncccn", "nnccc", "cnncc", "ccnnc", "cccnn" };
        if (use_third_n_match == UseThirdNMatchAlways)
            for (int i = 0; i < 5; i++)
                patterns[i][(i + 3) % 5] = 'n';

        const std::string &pattern = patterns[best];

        for (int i = section_start; i < section_end; i++) {
            if (use_third_n_match == UseThirdNMatchIfPrettier && pattern[i % 5] == 'c' && pattern[(i + 1) % 5] == 'n') {
                int16_t mic_n = mics[i][matchCharToIndex('n')];
                int16_t mic_c = mics[i][matchCharToIndex('c')];
                if (mic_n < mic_c)
                    matches[i] = 'n';
                else
                    matches[i] = 'c';
            } else {
                matches[i] = pattern[i % 5];
            }
        }

        // If the last frame of the section has much higher mic with c/n matches than with b match, use the b match.
        char match_index = matchCharToIndex(matches[section_end - 1]);
        int16_t mic_cn = mics[section_end - 1][match_index];
        int16_t mic_b = mics[section_end - 1][matchCharToIndex('b')];
        if (mic_cn > mic_b * 2)
            matches[section_end - 1] = 'b';

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

    pattern_guessing.minimum_length = minimum_length;
    pattern_guessing.third_n_match = use_third_n_match;
    pattern_guessing.decimation = drop_duplicate;
}


const PatternGuessing &WobblyProject::getPatternGuessing() {
    return pattern_guessing;
}


void WobblyProject::sectionsToScript(std::string &script) {
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

int WobblyProject::maybeTranslate(int frame, bool is_end, PositionInFilterChain position) {
    if (position == PostDecimate) {
        if (is_end)
            while (isDecimatedFrame(frame))
                frame--;
        return frameNumberAfterDecimation(frame);
    } else
        return frame;
}

void WobblyProject::customListsToScript(std::string &script, PositionInFilterChain position) {
    for (size_t i = 0; i < custom_lists.size(); i++) {
        // Ignore lists that are in a different position in the filter chain.
        if (custom_lists[i].position != position)
            continue;

        // Ignore lists with no frame ranges.
        if (!custom_lists[i].frames.size())
            continue;

        // Complain if the custom list doesn't have a preset assigned.
        if (!custom_lists[i].preset.size())
            throw WobblyException("Custom list '" + custom_lists[i].name + "' has no preset assigned.");


        std::string list_name = "cl_";
        list_name += custom_lists[i].name;

        script += list_name + " = " + custom_lists[i].preset + "(src)\n";

        std::string splice = "src = c.std.Splice(mismatch=True, clips=[";

        auto it = custom_lists[i].frames.cbegin();
        auto it_prev = it;

        if (it->second.first > 0) {
            splice += "src[0:";
            splice += std::to_string(maybeTranslate(it->second.first, true, position)) + "],";
        }

        splice += list_name + "[" + std::to_string(maybeTranslate(it->second.first, false, position)) + ":" + std::to_string(maybeTranslate(it->second.last, true, position) + 1) + "],";

        it++;
        for ( ; it != custom_lists[i].frames.cend(); it++, it_prev++) {
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

        if (last_last < maybeTranslate(num_frames[PostSource] - 1, true, position)) {
            splice += "src[";
            splice += std::to_string(last_last + 1) + ":]";
        }

        splice += "])\n\n";

        script += splice;
    }
}

void WobblyProject::headerToScript(std::string &script) {
    script +=
            "import vapoursynth as vs\n"
            "\n"
            "c = vs.get_core()\n"
            "\n";
}

void WobblyProject::presetsToScript(std::string &script) {
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

void WobblyProject::sourceToScript(std::string &script) {
    script +=
            "try:\n"
            "    src = vs.get_output(index=1)\n"
            "except KeyError:\n"
            "    src = c.d2v.Source(input=r'" + input_file + "')\n"
            "    src.set_output(index=1)\n"
            "\n";
}

void WobblyProject::trimToScript(std::string &script) {
    script += "src = c.std.Splice(clips=[";
    for (auto it = trims.cbegin(); it != trims.cend(); it++)
        script += "src[" + std::to_string(it->second.first) + ":" + std::to_string(it->second.last + 1) + "],";
    script +=
            "])\n"
            "\n";
}

void WobblyProject::fieldHintToScript(std::string &script) {
    script += "src = c.fh.FieldHint(clip=src, tff=";
    script += std::to_string((int)vfm_parameters["order"]);
    script += ", matches='";
    script.append(matches.data(), matches.size());
    script +=
            "')\n"
            "\n";
}

void WobblyProject::freezeFramesToScript(std::string &script) {
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

void WobblyProject::decimatedFramesToScript(std::string &script) {
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
            range_end = num_frames[PostSource];
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
            range_end = num_frames[PostSource];
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

void WobblyProject::cropToScript(std::string &script) {
    script += "src = c.std.CropRel(clip=src, left=";
    script += std::to_string(crop.left) + ", top=";
    script += std::to_string(crop.top) + ", right=";
    script += std::to_string(crop.right) + ", bottom=";
    script += std::to_string(crop.bottom) + ")\n\n";
}

void WobblyProject::showCropToScript(std::string &script) {
    script += "src = c.std.AddBorders(clip=src, left=";
    script += std::to_string(crop.left) + ", top=";
    script += std::to_string(crop.top) + ", right=";
    script += std::to_string(crop.right) + ", bottom=";
    script += std::to_string(crop.bottom) + ", color=[128, 230, 180])\n\n";
}

void WobblyProject::resizeToScript(std::string &script) {
    script += "src = c.z.Depth(clip=src, depth=32, sample=vs.FLOAT)\n";
    script += "src = c.z.Resize(clip=src";
    script += ", width=" + std::to_string(resize.width);
    script += ", height=" + std::to_string(resize.height);
    script += ", filter='" + resize.filter + "'";
    script += ")\n\n";
}

void WobblyProject::bitDepthToScript(std::string &script) {
    script += "src = c.z.Depth(clip=src";
    script += ", depth=" + std::to_string(depth.bits);
    script += ", sample=" + std::string(depth.float_samples ? "vs.FLOAT" : "vs.INTEGER");
    script += ", dither='" + depth.dither + "'";
    script += ")\n\n";
}

void WobblyProject::setOutputToScript(std::string &script) {
    script += "src.set_output()\n";
}

std::string WobblyProject::generateFinalScript() {
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

    if (resize.enabled)
        resizeToScript(script);

    if (depth.enabled)
        bitDepthToScript(script);

    setOutputToScript(script);

    return script;
}

std::string WobblyProject::generateMainDisplayScript(bool show_crop) {
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


std::string WobblyProject::generateTimecodesV1() {
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
                end = num_frames[PostSource];
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
