#ifndef WIBBLYWINDOW_H
#define WIBBLYWINDOW_H

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QSlider>
#include <QSpinBox>

#include "DockWidget.h"
#include "ListWidget.h"

#include "WibblyJob.h"


enum VFMParameterTypes {
    VFMParamInt,
    VFMParamDouble,
    VFMParamBool
};


struct VFMParameter {
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
    QProgressBar *main_progress_bar;

    DockWidget *video_dock;
    QLabel *video_frame_label;
    QSlider *video_frame_slider;

    DockWidget *crop_dock;
    QSpinBox *crop_spin[4];

    DockWidget *vfm_dock;
    std::vector<VFMParameter> vfm_params;

    DockWidget *trim_dock;
    ListWidget *trim_ranges_list;

    DockWidget *fades_dock;
    QDoubleSpinBox *fades_threshold_spin;


    // VapourSynth stuff.


    // Other stuff.
    std::vector<WibblyJob> jobs;

    int current_frame;

    int trim_start;
    int trim_end;


    // Functions.
    void createUI();
    void createMenus();
    void createMainWindow();
    void createVideoOutputWindow();
    void createCropWindow();
    void createVFMWindow();
    void createTrimWindow();
    void createInterlacedFadesWindow();

    void realOpenVideo(const QString &path);

    void errorPopup(const char *msg);

public:
    WibblyWindow();

public slots:
};

#endif // WIBBLYWINDOW_H
