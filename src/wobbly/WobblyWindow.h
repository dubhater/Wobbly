#ifndef WOBBLYWINDOW_H
#define WOBBLYWINDOW_H


#include <QCloseEvent>
#include <QLabel>
#include <QMainWindow>

#include <VapourSynth.h>
#include <VSScript.h>

#include "WobblyProject.h"


class WobblyWindow : public QMainWindow {
    Q_OBJECT

public:
    WobblyWindow();

private:
    // Widgets.

    QLabel *frame_label;

    QLabel *frame_num_label;
    QLabel *match_label;



    // Other stuff.

    WobblyProject *project;

    int current_frame;

    const VSAPI *vsapi;
    VSScript *vsscript;
    VSCore *vscore;
    VSNodeRef *vsnode;
    const VSFrameRef *vsframe;

    PositionInFilterChain current_section_set;
    PositionInFilterChain current_custom_list_set;


    // Functions

    void createMenu();
    void createShortcuts();
    void createUI();

    void initialiseVapourSynth();
    void cleanUpVapourSynth();
    void checkRequiredFilters();


    void toggleDecimate();

    void toggleCombed();

    void addSection();
    void deleteSection();

    void deleteFreezeFrame();

    void jump(int frame);

    void evaluateMainDisplayScript();
    void displayFrame(int n);


    void closeEvent(QCloseEvent *event);

public slots:
    void jump1Forward();
    void jump1Backward();
    void jump5Forward();
    void jump5Backward();
    void jump50Forward();
    void jump50Backward();
    void jumpALotForward();
    void jumpALotBackward();

    void jumpToStart();
    void jumpToEnd();

    //void jumpToNextSection();
    //void jumpToPreviousSection();

    void cycleMatchPCN();


    void openProject();
    void saveProject();
};

#endif // WOBBLYWINDOW_H
