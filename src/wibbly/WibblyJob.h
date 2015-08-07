#ifndef WIBBLYJOB_H
#define WIBBLYJOB_H

#include <map>
#include <string>

#include "WobblyProject.h"


enum MetricsGatheringSteps {
    StepTrim = 1 << 0,
    StepCrop = 1 << 1,
    StepFieldMatch = 1 << 2,
    StepInterlacedFades = 1 << 3,
    StepDecimation = 1 << 4,
    StepSceneChanges = 1 << 5,
};


struct VFMParameters {
    int order;
    int mchroma;
    int cthresh;
    int mi;
    int chroma;
    int blockx;
    int blocky;
    int y0;
    int y1;
    double scthresh;
    int micmatch;
};


class WibblyJob {
    std::string input_file;

    std::string source_filter;

    std::string output_file;

    int steps;

    Crop crop;

    std::map<int, FrameRange> trims;

    VFMParameters vfm;

    float fades_threshold;


    void headerToScript(std::string &script);
    void sourceToScript(std::string &script);
    void trimToScript(std::string &script);
    void cropToScript(std::string &script);
    void fieldMatchToScript(std::string &script);
    void interlacedFadesToScript(std::string &script);
    void decimationToScript(std::string &script);
    void sceneChangesToScript(std::string &script);
    void setOutputToScript(std::string &script);

public:
    WibblyJob();


    std::string getInputFile();
    void setInputFile(const std::string &path);


    std::string getSourceFilter();
    void setSourceFilter(const std::string &filter);


    std::string getOutputFile();
    void setOutputFile(const std::string &path);


    std::string generateFinalScript();
    std::string generateDisplayScript();
};

#endif // WIBBLYJOB_H
