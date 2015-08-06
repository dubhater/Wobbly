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

    int crop[4];

    std::map<int, FrameRange> trim;

    VFMParameters vfm;

    float fades_threshold;

    WobblyProject *project;

public:
    WibblyJob();
};

#endif // WIBBLYJOB_H
