#include "WibblyJob.h"


WibblyJob::WibblyJob()
    : steps(StepTrim | StepCrop | StepFieldMatch | StepInterlacedFades | StepDecimation | StepSceneChanges)
    , crop{ 0, 0, 0, 0 }
    , vfm{ 1, 1, 9, 80, 1, 16, 16, 16, 16, 12, 0 }
    , fades_threshold(0.4f)
{

}


void WibblyJob::headerToScript(std::string &script) {
    script +=
            "import vapoursynth as vs\n"
            "\n"
            "c = vs.get_core()\n"
            "\n";
}


void WibblyJob::sourceToScript(std::string &script) {
    script +=
            "src = c." + source_filter + "(r'" + input_file + "')\n"
            "\n";
}


void WibblyJob::trimToScript(std::string &script) {
    if (!trims.size())
        return;

    script += "src = c.std.Splice(clips=[";
    for (auto it = trims.cbegin(); it != trims.cend(); it++)
        script += "src[" + std::to_string(it->second.first) + ":" + std::to_string(it->second.last + 1) + "],";
    script +=
            "])\n"
            "\n";
}


void WibblyJob::cropToScript(std::string &script) {
    script += "src = c.std.CropRel(clip=src, left=";
    script += std::to_string(crop.left) + ", top=";
    script += std::to_string(crop.top) + ", right=";
    script += std::to_string(crop.right) + ", bottom=";
    script += std::to_string(crop.bottom) + ")\n\n";
}


void WibblyJob::fieldMatchToScript(std::string &script) {
    script += "src = c.vivtc.VFM(clip=src";
    script += ", order=" + std::to_string(vfm.order);
    script += ", field=" + std::to_string(!vfm.order);
    script += ", mode=" + std::to_string(0);
    script += ", mchroma=" + std::to_string(vfm.mchroma);
    script += ", cthresh=" + std::to_string(vfm.cthresh);
    script += ", mi=" + std::to_string(vfm.mi);
    script += ", chroma=" + std::to_string(vfm.chroma);
    script += ", blockx=" + std::to_string(vfm.blockx);
    script += ", blocky=" + std::to_string(vfm.blocky);
    script += ", y0=" + std::to_string(vfm.y0);
    script += ", y1=" + std::to_string(vfm.y1);
    script += ", scthresh=" + std::to_string(vfm.scthresh);
    script += ", micmatch=" + std::to_string(vfm.micmatch);
    script += ")\n\n";
}


void WibblyJob::interlacedFadesToScript(std::string &script) {
    script += "separated = c.std.SeparateFields(clip=src, tff=True)\n";
    script += "even = c.std.SelectEvery(clip=separated, cycle=2, offsets=0)\n";
    script += "odd = c.std.SelectEvery(clip=separated, cycle=2, offsets=1)\n";
    script += "diff = c.std.PlaneDifference(clips=[even, odd], plane=0, prop='WibblyFieldDifference')\n";
    script += "src = c.std.Interleave(clips=[diff, odd])\n";
    script += "src = c.std.DoubleWeave(clip=src, tff=True)\n";
    script += "src = c.std.SelectEvery(clip=src, cycle=2, offsets=0)\n\n";
}


void WibblyJob::decimationToScript(std::string &script) {
    script += "src = c.vivtc.VDecimate(clip=src, cycle=5, chroma=1, dupthresh=1.1, scthresh=15, blockx=32, blocky=32, dryrun=True)\n\n";
}


void WibblyJob::sceneChangesToScript(std::string &script) {
    script += "src = c.scxvid.Scxvid(clip=src, use_slices=True)\n\n";
}


void WibblyJob::setOutputToScript(std::string &script) {
    script += "src.set_output()\n";
}


std::string WibblyJob::generateFinalScript() {
    std::string script;

    headerToScript(script);

    sourceToScript(script);

    trimToScript(script);

    cropToScript(script);

    fieldMatchToScript(script);

    interlacedFadesToScript(script);

    decimationToScript(script);

    sceneChangesToScript(script);

    setOutputToScript(script);

    return script;
}


std::string WibblyJob::generateDisplayScript() {
    std::string script;

    headerToScript(script);

    sourceToScript(script);

    cropToScript(script);

    setOutputToScript(script);

    return script;
}

