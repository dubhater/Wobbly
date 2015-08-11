#ifndef WIBBLYWINDOW_H
#define WIBBLYWINDOW_H

#include <QCheckBox>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMainWindow>
#include <QProgressDialog>
#include <QSlider>
#include <QSpinBox>
#include <QTimeEdit>

#include <VSScript.h>

#include "DockWidget.h"
#include "ListWidget.h"

#include "WibblyJob.h"


enum VIVTCParameterTypes {
    VIVTCParamInt,
    VIVTCParamDouble,
    VIVTCParamBool
};


struct VIVTCParameter {
    QWidget *widget;
    QString name;
    int minimum;
    int maximum;
    int type;
};


class WibblyWindow : public QMainWindow
{
    Q_OBJECT

    // Menus.
    QMenu *menu_menu;


    // Widgets.
    ListWidget *main_jobs_list;
    QProgressDialog *main_progress_dialog;

    DockWidget *video_dock;
    QLabel *video_frame_label;
    QSpinBox *video_frame_spin;
    QTimeEdit *video_time_edit;
    QSlider *video_frame_slider;

    DockWidget *crop_dock;
    QSpinBox *crop_spin[4];

    DockWidget *vfm_dock;
    std::vector<VIVTCParameter> vfm_params;

    std::vector<VIVTCParameter> vdecimate_params;

    DockWidget *trim_dock;
    ListWidget *trim_ranges_list;

    DockWidget *fades_dock;
    QDoubleSpinBox *fades_threshold_spin;


    // VapourSynth stuff.
    const VSAPI *vsapi;
    VSScript *vsscript;
    VSCore *vscore;
    VSNodeRef *vsnode;
    const VSVideoInfo *vsvi;
    const VSFrameRef *vsframe;


    // Other stuff.
    std::vector<WibblyJob> jobs;

    int current_frame;

    int trim_start;
    int trim_end;

    WobblyProject *current_project;
    int current_job;
    int next_frame;
    int frames_left;
    bool aborted;


    // Functions.
    void initialiseVapourSynth();
    void cleanUpVapourSynth();
    void checkRequiredFilters();

    void closeEvent(QCloseEvent *event);

    void createUI();
    void createMenus();
    void createMainWindow();
    void createVideoOutputWindow();
    void createCropWindow();
    void createVFMWindow();
    void createVDecimateWindow();
    void createTrimWindow();
    void createInterlacedFadesWindow();

    void realOpenVideo(const QString &path);

    void errorPopup(const char *msg);
    void errorPopup(const std::string &msg);

    void evaluateFinalScript(int job_index);
    void evaluateDisplayScript();
    void displayFrame(int n);

    void startNextJob();

public:
    WibblyWindow();

public slots:
    void frameDone(void *frame_v, int n, void *error_msg_v);
};

#endif // WIBBLYWINDOW_H
