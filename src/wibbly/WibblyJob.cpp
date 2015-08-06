#include "WibblyJob.h"

WibblyJob::WibblyJob()
    : steps(StepTrim | StepCrop | StepFieldMatch | StepInterlacedFades | StepDecimation | StepSceneChanges)
    , crop{ 0, 0, 0, 0 }
    , vfm{ 1, 1, 9, 80, 1, 16, 16, 16, 16, 12, 0 }
    , fades_threshold(0.4f)
    , project(nullptr)
{

}

