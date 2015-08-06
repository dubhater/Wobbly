#ifndef WIBBLYWINDOW_H
#define WIBBLYWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QSlider>

#include "DockWidget.h"
#include "ListWidget.h"

class WibblyWindow : public QMainWindow
{
    Q_OBJECT

    // Widgets.
    ListWidget *main_jobs_list;
    QProgressBar *main_progress_bar;

    DockWidget *video_dock;
    QLabel *video_frame_label;
    QSlider *video_frame_slider;


    // VapourSynth stuff.


    // Other stuff.


    // Functions.
    void createUI();
    void createMainWindow();
    void createVideoOutputWindow();
    void createCropWindow();
    void createVFMWindow();
    void createVDecimateWindow();

public:
    WibblyWindow();

public slots:
};

#endif // WIBBLYWINDOW_H
