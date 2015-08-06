#ifndef WIBBLYWINDOW_H
#define WIBBLYWINDOW_H

#include <QDoubleSpinBox>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QSlider>
#include <QSpinBox>

#include "DockWidget.h"
#include "ListWidget.h"

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

    DockWidget *vfm_dock;

    DockWidget *trim_dock;
    ListWidget *trim_ranges_list;
    QLabel *trim_label;

    DockWidget *fades_dock;
    QDoubleSpinBox *fades_threshold_spin;


    // VapourSynth stuff.


    // Other stuff.


    // Functions.
    void createUI();
    void createMenus();
    void createMainWindow();
    void createVideoOutputWindow();
    void createCropWindow();
    void createVFMWindow();
    void createTrimWindow();
    void createInterlacedFadesWindow();

public:
    WibblyWindow();

public slots:
};

#endif // WIBBLYWINDOW_H
