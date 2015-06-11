#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QStatusBar>

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

    frame_num_label = new QLabel;
    match_label = new QLabel;

    statusBar()->addPermanentWidget(frame_num_label);
    statusBar()->addPermanentWidget(match_label);
    statusBar()->setSizeGripEnabled(true);

    frame_label = new QLabel;
    frame_label->setTextFormat(Qt::PlainText);
    frame_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    setCentralWidget(frame_label);
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

    if (vsscript_evaluateScript(&vsscript, script.c_str(), QFileInfo(project->project_path.c_str()).dir().path().toUtf8().constData(), efSetWorkingDir))
        throw WobblyException(std::string("Failed to evaluate main display script. This should never happen. Error message:\n") + vsscript_getError(vsscript));

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
    if (n >= project->num_frames_after_trim)
        n = project->num_frames_after_trim - 1;

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
    frame_num_label->setText(QStringLiteral("Frame: ") + QString::number(current_frame));

    match_label->setText(QString(project->matches[current_frame]));
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
        if (current_frame == project->num_frames_after_trim - 1)
            match = 'c';
        else
            match = 'n';
    }

    evaluateMainDisplayScript();
}
