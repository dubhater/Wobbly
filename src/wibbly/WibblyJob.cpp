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


#include <sstream>
#include "RandomStuff.h"
#include "WibblyJob.h"


WibblyJob::WibblyJob()
    : steps(StepTrim | StepCrop | StepFieldMatch | StepInterlacedFades | StepDecimation | StepSceneChanges)
    , crop{ true, false, 0, 0, 0, 0 }
    , vfm{
            {
                { "order", 1 },
                { "cthresh", 9 },
                { "mi", 80 },
                { "blockx", 16 },
                { "blocky", 16 },
                { "y0", 16 },
                { "y1", 16 },
                { "micmatch", 0 }
            },
            {
                { "scthresh", 12 }
            },
            {
                { "mchroma", true },
                { "chroma", true }
            }
    }
    , vdecimate{
            {
                { "blockx", 32 },
                { "blocky", 32 }
            },
            {
                { "dupthresh", 1.1 },
                { "scthresh", 15 }
            },
            {
                { "chroma", true }
            }
    }
    , fades_threshold(0.4 / 255)
{

}


std::string WibblyJob::getInputFile() const {
    return input_file;
}


void WibblyJob::setInputFile(const std::string &path) {
    input_file = path;
}


std::string WibblyJob::getSourceFilter() const {
    return source_filter;
}


void WibblyJob::setSourceFilter(const std::string &filter) {
    source_filter = filter;
}


std::string WibblyJob::getOutputFile() const {
    return output_file;
}


void WibblyJob::setOutputFile(const std::string &path) {
    output_file = path;
}


int WibblyJob::getSteps() const {
    return steps;
}


void WibblyJob::setSteps(int new_steps) {
    steps = new_steps;
}


const Crop &WibblyJob::getCrop() const {
    return crop;
}


void WibblyJob::setCrop(int left, int top, int right, int bottom) {
    if (left < 0 || top < 0 || right < 0 || bottom < 0)
        throw WobblyException("Can't crop (" + std::to_string(left) + "," + std::to_string(top) + "," + std::to_string(right) + "," + std::to_string(bottom) + "): negative values.");

    crop.left = left;
    crop.top = top;
    crop.right = right;
    crop.bottom = bottom;
}


const std::map<int, FrameRange> &WibblyJob::getTrims() const {
    return trims;
}


void WibblyJob::addTrim(int trim_start, int trim_end) {
    for (auto it = trims.cbegin(); it != trims.cend(); it++) {
        if ((it->second.first <= trim_start && trim_start <= it->second.last) ||
            (it->second.first <= trim_end && trim_end <= it->second.last) ||
            (trim_start <= it->second.first && it->second.first <= trim_end) ||
            (trim_start <= it->second.last && it->second.last <= trim_end))

            throw WobblyException("Can't add trim (" + std::to_string(trim_start) + "," + std::to_string(trim_end) + "): overlaps trim (" + std::to_string(it->second.first) + "," + std::to_string(it->second.last) + ").");
    }

    trims.insert({ trim_start, { trim_start, trim_end } });
}


void WibblyJob::deleteTrim(int trim_start) {
    trims.erase(trim_start);
}


int WibblyJob::getVFMParameterInt(const std::string &name) const {
    return vfm.int_params.at(name);
}


double WibblyJob::getVFMParameterDouble(const std::string &name) const {
    return vfm.double_params.at(name);
}


bool WibblyJob::getVFMParameterBool(const std::string &name) const {
    return vfm.bool_params.at(name);
}


void WibblyJob::setVFMParameter(const std::string &name, int value) {
    vfm.int_params[name] = value;
}


void WibblyJob::setVFMParameter(const std::string &name, double value) {
    vfm.double_params[name] = value;
}


void WibblyJob::setVFMParameter(const std::string &name, bool value) {
    vfm.bool_params[name] = value;
}


int WibblyJob::getVDecimateParameterInt(const std::string &name) const {
    return vdecimate.int_params.at(name);
}


double WibblyJob::getVDecimateParameterDouble(const std::string &name) const {
    return vdecimate.double_params.at(name);
}


bool WibblyJob::getVDecimateParameterBool(const std::string &name) const {
    return vdecimate.bool_params.at(name);
}


void WibblyJob::setVDecimateParameter(const std::string &name, int value) {
    vdecimate.int_params[name] = value;
}


void WibblyJob::setVDecimateParameter(const std::string &name, double value) {
    vdecimate.double_params[name] = value;
}


void WibblyJob::setVDecimateParameter(const std::string &name, bool value) {
    vdecimate.bool_params[name] = value;
}


double WibblyJob::getFadesThreshold() const {
    return fades_threshold;
}


void WibblyJob::setFadesThreshold(double threshold) {
    fades_threshold = threshold;
}


void WibblyJob::headerToScript(std::string &script) const {
    script +=
            "import vapoursynth as vs\n"
            "\n"
            "c = vs.core\n"
            "\n";
}


void WibblyJob::sourceToScript(std::string &script) const {
    std::string fixed_input_file = handleSingleQuotes(input_file);

    script +=
            "if wibbly_last_input_file == r'" + fixed_input_file + "':\n"
            "    try:\n"
            "        src = vs.get_output(index=1)\n"
            // Since VapourSynth R41 get_output returns the alpha as well.
            "        if isinstance(src, tuple):\n"
            "            src = src[0]\n"
            "    except KeyError:\n"
            "        src = c." + source_filter + "(r'" + fixed_input_file + "')\n"
            "        src.set_output(index=1)\n"
            "else:\n"
            "    src = c." + source_filter + "(r'" + fixed_input_file + "')\n"
            "    src.set_output(index=1)\n"
            "    wibbly_last_input_file = r'" + fixed_input_file + "'\n"
            "\n";
}


void WibblyJob::trimToScript(std::string &script) const {
    if (!trims.size())
        return;

    script += "src = c.std.Splice(clips=[";
    for (auto it = trims.cbegin(); it != trims.cend(); it++)
        script += "src[" + std::to_string(it->second.first) + ":" + std::to_string(it->second.last + 1) + "],";
    script +=
            "])\n"
            "\n";
}


void WibblyJob::cropToScript(std::string &script) const {
    script += "src = c.std.CropRel(clip=src, left=";
    script += std::to_string(crop.left) + ", top=";
    script += std::to_string(crop.top) + ", right=";
    script += std::to_string(crop.right) + ", bottom=";
    script += std::to_string(crop.bottom) + ")\n\n";
}


void WibblyJob::fieldMatchToScript(std::string &script) const {
    script += "src = c.vivtc.VFM(clip=src";

    for (auto it = vfm.int_params.cbegin(); it != vfm.int_params.cend(); it++)
        script += ", " + it->first + "=" + std::to_string(it->second);
    for (auto it = vfm.double_params.cbegin(); it != vfm.double_params.cend(); it++) {
        std::stringstream ss;
        ss.imbue(std::locale::classic());
        ss << it->second;
        script += ", " + it->first + "=" + ss.str();
    }
    for (auto it = vfm.bool_params.cbegin(); it != vfm.bool_params.cend(); it++)
        script += ", " + it->first + "=" + std::to_string((int)it->second);

    script += ", field=" + std::to_string(!vfm.int_params.at("order"));
    script += ", mode=0";
    script += ", micout=1";
    script += ")\n\n";
}


void WibblyJob::interlacedFadesToScript(std::string &script) const {
    script +=
            "def copyProp(n, f):\n"
            "    fout = f[0].copy()\n"
            "    fout.props.WibblyFieldDifference = abs(f[0].props.WibblyEvenAverage - f[1].props.WibblyOddAverage)\n"
            "    return fout\n"
            "\n"
            "separated = c.std.SeparateFields(clip=src, tff=True)\n"

            "even = c.std.SelectEvery(clip=separated, cycle=2, offsets=0)\n"
            "even = c.std.PlaneStats(clipa=even, plane=0, prop='WibblyEven')\n"

            "odd = c.std.SelectEvery(clip=separated, cycle=2, offsets=1)\n"
            "odd = c.std.PlaneStats(clipa=odd, plane=0, prop='WibblyOdd')\n"

            "even = c.std.ModifyFrame(clip=even, clips=[even, odd], selector=copyProp)\n"
            "src = c.std.Interleave(clips=[even, odd])\n"
            "src = c.std.DoubleWeave(clip=src, tff=True)\n"
            "src = c.std.SelectEvery(clip=src, cycle=2, offsets=0)\n\n";
}


void WibblyJob::framePropsToScript(std::string &script) const {
    script += "src = c.text.FrameProps(clip=src, props=[";

    std::string props;
    if (steps & StepFieldMatch)
        props += "'VFMMatch', 'VFMMics', 'VFMSceneChange', '_Combed', ";
    if (steps & StepInterlacedFades)
        props += "'WibblyFieldDifference', ";

    script += props;
    script += "])\n\n";
}


void WibblyJob::decimationToScript(std::string &script) const {
    script += "src = c.vivtc.VDecimate(clip=src";

    for (auto it = vdecimate.int_params.cbegin(); it != vdecimate.int_params.cend(); it++)
        script += ", " + it->first + "=" + std::to_string(it->second);
    for (auto it = vdecimate.double_params.cbegin(); it != vdecimate.double_params.cend(); it++) {
        std::stringstream ss;
        ss.imbue(std::locale::classic());
        ss << it->second;
        script += ", " + it->first + "=" + ss.str();
    }
    for (auto it = vdecimate.bool_params.cbegin(); it != vdecimate.bool_params.cend(); it++)
        script += ", " + it->first + "=" + std::to_string((int)it->second);

    script += ", cycle=5";
    script += ", dryrun=True";
    script += ")\n\n";
}


void WibblyJob::sceneChangesToScript(std::string &script) const {
    script += "src = c.scxvid.Scxvid(clip=src, use_slices=True)\n\n";
}


void WibblyJob::setOutputToScript(std::string &script) const {
    script += "src.set_output()\n";
}


std::string WibblyJob::generateFinalScript() const {
    std::string script;

    headerToScript(script);

    sourceToScript(script);

    if (steps & StepTrim)
        trimToScript(script);

    if (steps & StepCrop)
        cropToScript(script);

    if (steps & StepFieldMatch)
        fieldMatchToScript(script);

    if (steps & StepInterlacedFades)
        interlacedFadesToScript(script);

    if (steps & StepDecimation)
        decimationToScript(script);

    if (steps & StepSceneChanges)
        sceneChangesToScript(script);

    setOutputToScript(script);

    return script;
}


std::string WibblyJob::generateDisplayScript() const {
    std::string script;

    headerToScript(script);

    sourceToScript(script);

    if (steps & StepCrop)
        cropToScript(script);

    if (steps & StepFieldMatch)
        fieldMatchToScript(script);

    if (steps & StepInterlacedFades)
        interlacedFadesToScript(script);

    if (steps & StepFieldMatch || steps & StepInterlacedFades)
        framePropsToScript(script);

    setOutputToScript(script);

    return script;
}

