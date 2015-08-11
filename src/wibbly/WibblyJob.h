#ifndef WIBBLYJOB_H
#define WIBBLYJOB_H

#include <map>
#include <unordered_map>
#include <string>

#include "WobblyProject.h"


enum MetricsGatheringSteps {
    StepNone = 0,
    StepTrim = 1 << 0,
    StepCrop = 1 << 1,
    StepFieldMatch = 1 << 2,
    StepInterlacedFades = 1 << 3,
    StepDecimation = 1 << 4,
    StepSceneChanges = 1 << 5,
};


struct VIVTCParameters {
    std::unordered_map<std::string, int> int_params;
    std::unordered_map<std::string, double> double_params;
    std::unordered_map<std::string, bool> bool_params;
};


class WibblyJob {
    std::string input_file;

    std::string source_filter;

    std::string output_file;

    int steps;

    Crop crop;

    std::map<int, FrameRange> trims;

    VIVTCParameters vfm;

    VIVTCParameters vdecimate;

    double fades_threshold;


    void headerToScript(std::string &script) const;
    void sourceToScript(std::string &script) const;
    void trimToScript(std::string &script) const;
    void cropToScript(std::string &script) const;
    void fieldMatchToScript(std::string &script) const;
    void interlacedFadesToScript(std::string &script) const;
    void decimationToScript(std::string &script) const;
    void sceneChangesToScript(std::string &script) const;
    void setOutputToScript(std::string &script) const;

public:
    WibblyJob();


    std::string getInputFile() const;
    void setInputFile(const std::string &path);


    std::string getSourceFilter() const;
    void setSourceFilter(const std::string &filter);


    std::string getOutputFile() const;
    void setOutputFile(const std::string &path);


    int getSteps() const;
    void setSteps(int new_steps);


    const Crop &getCrop() const;
    void setCrop(int left, int top, int right, int bottom);


    const std::map<int, FrameRange> &getTrims() const;
    void addTrim(int trim_start, int trim_end);
    void deleteTrim(int trim_start);


    int getVFMParameterInt(const std::string &name) const;
    double getVFMParameterDouble(const std::string &name) const;
    bool getVFMParameterBool(const std::string &name) const;
    void setVFMParameter(const std::string &name, int value);
    void setVFMParameter(const std::string &name, double value);
    void setVFMParameter(const std::string &name, bool value);


    int getVDecimateParameterInt(const std::string &name) const;
    double getVDecimateParameterDouble(const std::string &name) const;
    bool getVDecimateParameterBool(const std::string &name) const;
    void setVDecimateParameter(const std::string &name, int value);
    void setVDecimateParameter(const std::string &name, double value);
    void setVDecimateParameter(const std::string &name, bool value);


    double getFadesThreshold() const;
    void setFadesThreshold(double threshold);


    std::string generateFinalScript() const;
    std::string generateDisplayScript() const;
};

#endif // WIBBLYJOB_H
