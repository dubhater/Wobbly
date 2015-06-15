#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QStatusBar>

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <VSScript.h>

#include "WobblyException.h"
#include "WobblyWindow.h"

#include "WobblyWindow.moc"


WobblyWindow::WobblyWindow()
    : project(nullptr)
    , current_frame(0)
    , vsapi(nullptr)
    , vsscript(nullptr)
    , vscore(nullptr)
    , vsnode(nullptr)
    , vsframe(nullptr)
    , current_section_set(PostFieldMatch)
    , current_custom_list_set(PostFieldMatch)
{
    createUI();

    try {
        initialiseVapourSynth();

        checkRequiredFilters();
    } catch (WobblyException &e) {
        frame_label->setText(e.what());
        // XXX Disable everything but "Quit".
    }
}


void WobblyWindow::closeEvent(QCloseEvent *event) {
    cleanUpVapourSynth();

    if (project) {
        delete project;
        project = nullptr;
    }

    // XXX Message box/automatic saving of the project.
    event->accept();
}


void WobblyWindow::createMenu() {
    QMenuBar *bar = menuBar();

    QMenu *p = bar->addMenu("&Project");

    QAction *projectOpen = new QAction("&Open project", this);
    QAction *projectSave = new QAction("&Save project", this);
    QAction *projectQuit = new QAction("&Quit", this);

    projectOpen->setShortcut(QKeySequence::Open);
    projectSave->setShortcut(QKeySequence::Save);
    projectQuit->setShortcut(QKeySequence("Ctrl+Q"));

    connect(projectOpen, &QAction::triggered, this, &WobblyWindow::openProject);
    connect(projectSave, &QAction::triggered, this, &WobblyWindow::saveProject);
    connect(projectQuit, &QAction::triggered, this, &QWidget::close);

    p->addAction(projectOpen);
    p->addAction(projectSave);
    p->addSeparator();
    p->addAction(projectQuit);
}


struct Shortcut {
    int keys;
    void (WobblyWindow::* func)();
};


void WobblyWindow::createShortcuts() {
    Shortcut shortcuts[] = {
        { Qt::Key_Left, &WobblyWindow::jump1Backward },
        { Qt::Key_Right, &WobblyWindow::jump1Forward },
        { Qt::ControlModifier + Qt::Key_Left, &WobblyWindow::jump5Backward },
        { Qt::ControlModifier + Qt::Key_Right, &WobblyWindow::jump5Forward },
        { Qt::AltModifier + Qt::Key_Left, &WobblyWindow::jump50Backward },
        { Qt::AltModifier + Qt::Key_Right, &WobblyWindow::jump50Forward },
        { Qt::Key_Home, &WobblyWindow::jumpToStart },
        { Qt::Key_End, &WobblyWindow::jumpToEnd },
        { Qt::Key_PageDown, &WobblyWindow::jumpALotBackward },
        { Qt::Key_PageUp, &WobblyWindow::jumpALotForward },
        { Qt::Key_S, &WobblyWindow::cycleMatchPCN },
        { 0, nullptr }
    };

    for (int i = 0; shortcuts[i].func; i++) {
        QShortcut *s = new QShortcut(QKeySequence(shortcuts[i].keys), this);
        connect(s, &QShortcut::activated, this, shortcuts[i].func);
    }
}


void WobblyWindow::createUI() {
    createMenu();
    createShortcuts();

    setWindowTitle(QStringLiteral("Wobbly"));

    statusBar()->setSizeGripEnabled(true);

    frame_num_label = new QLabel;
    time_label = new QLabel;
    matches_label = new QLabel;
    matches_label->setTextFormat(Qt::RichText);
    section_set_label = new QLabel;
    section_label = new QLabel;
    custom_list_set_label = new QLabel;
    custom_list_label = new QLabel;
    freeze_label = new QLabel;
    decimate_metric_label = new QLabel;
    mic_label = new QLabel;
    mic_label->setTextFormat(Qt::RichText);
    combed_label = new QLabel;

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(frame_num_label);
    vbox->addWidget(time_label);
    vbox->addWidget(matches_label);
    vbox->addWidget(section_set_label);
    vbox->addWidget(section_label);
    vbox->addWidget(custom_list_set_label);
    vbox->addWidget(custom_list_label);
    vbox->addWidget(freeze_label);
    vbox->addWidget(decimate_metric_label);
    vbox->addWidget(mic_label);
    vbox->addWidget(combed_label);
    vbox->addStretch(1);

    frame_label = new QLabel;
    frame_label->setTextFormat(Qt::PlainText);
    frame_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addWidget(frame_label);

    QWidget *central_widget = new QWidget;
    central_widget->setLayout(hbox);

    setCentralWidget(central_widget);
}


void WobblyWindow::initialiseVapourSynth() {
    if (!vsscript_init())
        throw WobblyException("Fatal error: failed to initialise VSScript. Your VapourSynth installation is probably broken.");
    
    
    vsapi = vsscript_getVSApi();
    if (!vsapi)
        throw WobblyException("Fatal error: failed to acquire VapourSynth API struct. Did you update the VapourSynth library but not the Python module (or the other way around)?");
    
    if (vsscript_createScript(&vsscript))
        throw WobblyException(std::string("Fatal error: failed to create VSScript object. Error message: ") + vsscript_getError(vsscript));

    vscore = vsscript_getCore(vsscript);
    if (!vscore)
        throw WobblyException("Fatal error: failed to retrieve VapourSynth core object.");
}


void WobblyWindow::cleanUpVapourSynth() {
    frame_label->setPixmap(QPixmap()); // Does it belong here?
    vsapi->freeFrame(vsframe);

    vsapi->freeNode(vsnode);

    vsscript_freeScript(vsscript);
}


void WobblyWindow::checkRequiredFilters() {
    const char *filters[][4] = {
        {
            "com.sources.d2vsource",
            "Source",
            "d2vsource plugin not found.",
            "How would I know?"
        },
        {
            "com.nodame.fieldhint",
            "FieldHint",
            "FieldHint plugin not found.",
            "FieldHint plugin is too old."
        },
        {
            "com.vapoursynth.std",
            "FreezeFrames",
            "VapourSynth standard filter library not found. This should never happen.",
            "VapourSynth version is too old."
        },
        {
            "com.vapoursynth.std",
            "DeleteFrames",
            "VapourSynth standard filter library not found. This should never happen.",
            "VapourSynth version is too old."
        },
        {
            "the.weather.channel",
            "Colorspace",
            "zimg plugin not found.",
            "Arwen broke it."
        },
        {
            "the.weather.channel",
            "Depth",
            "zimg plugin not found.",
            "Arwen broke it."
        },
        {
            "the.weather.channel",
            "Resize",
            "zimg plugin not found.",
            "Arwen broke it."
        },
        {
            nullptr
        }
    };

    for (int i = 0; filters[i][0]; i++) {
        VSPlugin *plugin = vsapi->getPluginById(filters[i][0], vscore);
        if (!plugin)
            throw WobblyException(std::string("Fatal error: ") + filters[i][2]);

        VSMap *map = vsapi->getFunctions(plugin);
        if (vsapi->propGetType(map, filters[i][1]) == ptUnset)
            throw WobblyException(std::string("Fatal error: plugin found but it lacks filter '") + filters[i][1] + "'" + (filters[i][3] ? std::string(". Likely reason: ") + filters[i][3] : ""));
    }
}


void WobblyWindow::openProject() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Wobbly project"), QString(), QString(), nullptr, QFileDialog::DontUseNativeDialog);

    if (!path.isNull()) {
        WobblyProject *tmp = new WobblyProject;

        try {
            tmp->readProject(path.toStdString());

            if (project)
                delete project;
            project = tmp;

            evaluateMainDisplayScript();
        } catch (WobblyException &e) {
            QMessageBox::information(this, QStringLiteral("Error"), e.what());

            if (project == tmp)
                project = nullptr;
            delete tmp;
        }
    }
}


void WobblyWindow::saveProject() {
    try {
        if (!project)
            throw WobblyException("Can't save the project because none has been loaded.");

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Wobbly project"), QString(), QString(), nullptr, QFileDialog::DontUseNativeDialog);

        if (!path.isNull())
            project->writeProject(path.toStdString());
    } catch (WobblyException &e) {
        QMessageBox::information(this, QStringLiteral("Error"), e.what());
    }
}


void WobblyWindow::evaluateMainDisplayScript() {
    std::string script = project->generateMainDisplayScript();

    if (vsscript_evaluateScript(&vsscript, script.c_str(), QFileInfo(project->project_path.c_str()).dir().path().toUtf8().constData(), efSetWorkingDir)) {
        std::string error = vsscript_getError(vsscript);
        // The traceback is mostly unnecessary noise.
        size_t traceback = error.find("Traceback");
        if (traceback != std::string::npos)
            error.erase(traceback);

        throw WobblyException("Failed to evaluate main display script. Error message:\n" + error);
    }

    vsapi->freeNode(vsnode);

    vsnode = vsscript_getOutput(vsscript, 0);
    if (!vsnode)
        throw WobblyException("Main display script evaluated successfully, but no node found at output index 0.");

    displayFrame(current_frame);
}


void WobblyWindow::displayFrame(int n) {
    if (!vsnode)
        return;

    if (n < 0)
        n = 0;
    if (n >= project->num_frames[PostSource])
        n = project->num_frames[PostSource] - 1;

    std::vector<char> error(1024);
    const VSFrameRef *frame = vsapi->getFrame(n, vsnode, error.data(), 1024);

    if (!frame)
        throw WobblyException(std::string("Failed to retrieve frame. Error message: ") + error.data());

    const uint8_t *ptr = vsapi->getReadPtr(frame, 0);
    int width = vsapi->getFrameWidth(frame, 0);
    int height = vsapi->getFrameHeight(frame, 0);
    int stride = vsapi->getStride(frame, 0);
    frame_label->setPixmap(QPixmap::fromImage(QImage(ptr, width, height, stride, QImage::Format_RGB32)));
    // Must free the frame only after replacing the pixmap.
    vsapi->freeFrame(vsframe);
    vsframe = frame;

    current_frame = n;

    updateFrameDetails();
}


void WobblyWindow::updateFrameDetails() {
    frame_num_label->setText(QStringLiteral("Frame: %1").arg(current_frame));

    // frame number after decimation is a bit complicated to calculate

    time_label->setText(QString::fromStdString("Time: " + project->frameToTime(current_frame)));


    int matches_start = std::max(0, current_frame - 10);
    int matches_end = std::min(current_frame + 10, project->num_frames[PostSource] - 1);

    QString matches("Matches: ");
    for (int i = matches_start; i <= matches_end; i++) {
        char match = project->matches[i];

        bool is_decimated = project->isDecimatedFrame(i);

        if (i % 5 == 0)
            matches += "<u>";

        if (is_decimated)
            matches += "<s>";

        if (i == current_frame)
            match += 'C' - 'c';
        matches += match;

        if (is_decimated)
            matches += "</s>";

        if (i % 5 == 0)
            matches += "</u>";
    }
    matches_label->setText(matches);


    if (project->isCombedFrame(current_frame))
        combed_label->setText(QStringLiteral("Combed"));
    else
        combed_label->clear();


    decimate_metric_label->setText(QStringLiteral("DMetric: ") + QString::number(project->decimate_metrics[current_frame]));


    int match_index = matchCharToIndex(project->matches[current_frame]);
    QString mics("Mics: ");
    for (int i = 0; i < 5; i++) {
        if (i == match_index)
            mics += "<b>";

        mics += QStringLiteral("%1 ").arg((int)project->mics[current_frame][i]);

        if (i == match_index)
            mics += "</b>";
    }
    mic_label->setText(mics);


    const char *positions[3] = {
        "post source",
        "post field match",
        "post decimate"
    };
    section_set_label->setText(QStringLiteral("Section set: ") + positions[current_section_set]);


    const Section *current_section = project->findSection(current_frame, current_section_set);
    const Section *next_section = project->findNextSection(current_frame, current_section_set);
    int section_start = current_section->start;
    int section_end;
    if (next_section)
        section_end = next_section->start - 1;
    else
        section_end = project->num_frames[current_section_set] - 1;

    QString presets;
    for (auto it = current_section->presets.cbegin(); it != current_section->presets.cend(); it++)
        presets.append(QString::fromStdString(*it));

    if (presets.isNull())
        presets = "<none>";

    section_label->setText(QStringLiteral("Section: [%1,%2]\nPresets:\n%3").arg(section_start).arg(section_end).arg(presets));


    custom_list_set_label->setText(QStringLiteral("Custom list set: ") + positions[current_custom_list_set]);


    QString custom_lists;
    for (auto it = project->custom_lists[current_custom_list_set].cbegin(); it != project->custom_lists[current_custom_list_set].cend(); it++) {
        const FrameRange *range = it->findFrameRange(current_frame);
        if (range)
            custom_lists += QStringLiteral("%1: [%2,%3]\n").arg(QString::fromStdString(it->name)).arg(range->first).arg(range->last);
    }

    if (custom_lists.isNull())
        custom_lists = "<none>";

    custom_list_label->setText(QStringLiteral("Custom lists:\n%1").arg(custom_lists));


    const FreezeFrame *freeze = project->findFreezeFrame(current_frame);
    if (freeze)
        freeze_label->setText(QStringLiteral("Frozen: [%1,%2,%3]").arg(freeze->first).arg(freeze->last).arg(freeze->replacement));
    else
        freeze_label->clear();
}


void WobblyWindow::jump1Backward() {
    displayFrame(current_frame - 1);
}


void WobblyWindow::jump1Forward() {
    displayFrame(current_frame + 1);
}


void WobblyWindow::jump5Backward() {
    displayFrame(current_frame - 5);
}


void WobblyWindow::jump5Forward() {
    displayFrame(current_frame + 5);
}


void WobblyWindow::jump50Backward() {
    displayFrame(current_frame - 50);
}


void WobblyWindow::jump50Forward() {
    displayFrame(current_frame + 50);
}


void WobblyWindow::jumpALotBackward() {
    displayFrame(current_frame - 200); // XXX Something like 20% of the total number of frames?
}


void WobblyWindow::jumpALotForward() {
    displayFrame(current_frame + 200);
}


void WobblyWindow::jumpToStart() {
    displayFrame(0);
}


void WobblyWindow::jumpToEnd() {
    displayFrame(INT_MAX - 16); // XXX Use the real end.
}


void WobblyWindow::cycleMatchPCN() {
    // N -> C -> P. This is the order Yatta uses, so we use it.

    char &match = project->matches[current_frame];

    if (match == 'n')
        match = 'c';
    else if (match == 'c') {
        if (current_frame == 0)
            match = 'n';
        else
            match = 'p';
    } else if (match == 'p') {
        if (current_frame == project->num_frames[PostSource] - 1)
            match = 'c';
        else
            match = 'n';
    }

    evaluateMainDisplayScript();
}
