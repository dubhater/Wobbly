#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
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
    // XXX Only ask if the project was modified.

    if (project) {
        QMessageBox::StandardButton answer = QMessageBox::question(this, QStringLiteral("Save?"), QStringLiteral("Save project?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);

        if (answer == QMessageBox::Yes) {
            saveProject();
        } else if (answer == QMessageBox::No) {
            ;
        } else {
            event->ignore();
            return;
        }
    }

    cleanUpVapourSynth();

    if (project) {
        delete project;
        project = nullptr;
    }

    event->accept();
}


void WobblyWindow::createMenu() {
    QMenuBar *bar = menuBar();

    QMenu *p = bar->addMenu("&Project");

    QAction *projectOpen = new QAction("&Open project", this);
    QAction *projectSave = new QAction("&Save project", this);
    QAction *projectSaveAs = new QAction("&Save project as", this);
    QAction *projectQuit = new QAction("&Quit", this);

    projectOpen->setShortcut(QKeySequence::Open);
    projectSave->setShortcut(QKeySequence::Save);
    projectSaveAs->setShortcut(QKeySequence::SaveAs);
    projectQuit->setShortcut(QKeySequence("Ctrl+Q"));

    connect(projectOpen, &QAction::triggered, this, &WobblyWindow::openProject);
    connect(projectSave, &QAction::triggered, this, &WobblyWindow::saveProject);
    connect(projectSaveAs, &QAction::triggered, this, &WobblyWindow::saveProjectAs);
    connect(projectQuit, &QAction::triggered, this, &QWidget::close);

    p->addAction(projectOpen);
    p->addAction(projectSave);
    p->addAction(projectSaveAs);
    p->addSeparator();
    p->addAction(projectQuit);


    tools_menu = bar->addMenu("&Tools");
}


struct Shortcut {
    const char *keys;
    void (WobblyWindow::* func)();
};


void WobblyWindow::createShortcuts() {
    Shortcut shortcuts[] = {
        { "Left", &WobblyWindow::jump1Backward },
        { "Right", &WobblyWindow::jump1Forward },
        { "Ctrl+Left", &WobblyWindow::jump5Backward },
        { "Ctrl+Right", &WobblyWindow::jump5Forward },
        { "Alt+Left", &WobblyWindow::jump50Backward },
        { "Alt+Right", &WobblyWindow::jump50Forward },
        { "Home", &WobblyWindow::jumpToStart },
        { "End", &WobblyWindow::jumpToEnd },
        { "PgDown", &WobblyWindow::jumpALotBackward },
        { "PgUp", &WobblyWindow::jumpALotForward },
        { "Ctrl+Up", &WobblyWindow::jumpToNextSectionStart },
        { "Ctrl+Down", &WobblyWindow::jumpToPreviousSectionStart },
        { "S", &WobblyWindow::cycleMatchPCN },
        { "Ctrl+F", &WobblyWindow::freezeForward },
        { "Shift+F", &WobblyWindow::freezeBackward },
        { "F", &WobblyWindow::freezeRange },
        { "Delete,F", &WobblyWindow::deleteFreezeFrame },
        { "D", &WobblyWindow::toggleDecimation },
        { "I", &WobblyWindow::addSection },
        { "Delete,I", &WobblyWindow::deleteSection },
        { "P", &WobblyWindow::toggleCombed },
        { nullptr, nullptr }
    };

    for (int i = 0; shortcuts[i].func; i++) {
        QShortcut *s = new QShortcut(QKeySequence(shortcuts[i].keys), this);
        connect(s, &QShortcut::activated, this, shortcuts[i].func);
    }
}


void WobblyWindow::createCropAssistant() {
    const char *crop_prefixes[4] = {
        "Left: ",
        "Top: ",
        "Right: ",
        "Bottom: "
    };

    const char *resize_prefixes[2] = {
        "Width: ",
        "Height: "
    };

    QVBoxLayout *vbox = new QVBoxLayout;

    for (int i = 0; i < 4; i++) {
        crop_spin[i] = new QSpinBox;
        crop_spin[i]->setRange(0, 99999);
        crop_spin[i]->setPrefix(crop_prefixes[i]);
        crop_spin[i]->setSuffix(QStringLiteral(" px"));
        connect(crop_spin[i], static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &WobblyWindow::cropChanged);

        vbox->addWidget(crop_spin[i]);
    }

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);

    vbox = new QVBoxLayout;

    vbox->addLayout(hbox);

    QVBoxLayout *vbox2 = new QVBoxLayout;

    for (int i = 0; i < 2; i++) {
        resize_spin[i] = new QSpinBox;
        resize_spin[i]->setRange(1, 999999);
        resize_spin[i]->setPrefix(resize_prefixes[i]);
        resize_spin[i]->setSuffix(QStringLiteral(" px"));

        vbox2->addWidget(resize_spin[i]);
    }

    hbox = new QHBoxLayout;
    hbox->addLayout(vbox2);
    hbox->addStretch(1);

    vbox->addLayout(hbox);

    vbox->addStretch(1);

    QWidget *crop_widget = new QWidget;
    crop_widget->setLayout(vbox);

    crop_dock = new QDockWidget("Cropping/Resizing", this);
    crop_dock->setVisible(false);
    crop_dock->setFloating(true);
    crop_dock->setWidget(crop_widget);
    addDockWidget(Qt::RightDockWidgetArea, crop_dock);
    tools_menu->addAction(crop_dock->toggleViewAction());
    connect(crop_dock, &QDockWidget::visibilityChanged, this, &WobblyWindow::cropAssistantVisibilityChanged);
}


void WobblyWindow::createPresetEditor() {
    preset_combo = new QComboBox;
    connect(preset_combo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this, &WobblyWindow::presetChanged);

    preset_edit = new PresetTextEdit;
    preset_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    preset_edit->setTabChangesFocus(true);
    connect(preset_edit, &PresetTextEdit::focusLost, this, &WobblyWindow::presetEdited);

    QPushButton *new_button = new QPushButton(QStringLiteral("New"));
    QPushButton *rename_button = new QPushButton(QStringLiteral("Rename"));
    QPushButton *delete_button = new QPushButton(QStringLiteral("Delete"));

    connect(new_button, &QPushButton::clicked, this, &WobblyWindow::presetNew);
    connect(rename_button, &QPushButton::clicked, this, &WobblyWindow::presetRename);
    connect(delete_button, &QPushButton::clicked, this, &WobblyWindow::presetDelete);


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(new_button);
    hbox->addWidget(rename_button);
    hbox->addWidget(delete_button);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(preset_combo);
    vbox->addWidget(preset_edit);
    vbox->addLayout(hbox);

    QWidget *preset_widget = new QWidget;
    preset_widget->setLayout(vbox);


    QDockWidget *preset_dock = new QDockWidget("Preset editor", this);
    preset_dock->setVisible(false);
    preset_dock->setFloating(true);
    preset_dock->setWidget(preset_widget);
    addDockWidget(Qt::RightDockWidgetArea, preset_dock);
    tools_menu->addAction(preset_dock->toggleViewAction());
    //connect(preset_dock, &QDockWidget::visibilityChanged, this, &WobblyWindow::presetEditorVisibilityChanged);
}


void WobblyWindow::createUI() {
    createMenu();
    createShortcuts();

    setWindowTitle(QStringLiteral("Wobbly IVTC Assistant"));

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
    hbox->addStretch(1);

    QWidget *central_widget = new QWidget;
    central_widget->setLayout(hbox);

    setCentralWidget(central_widget);


    createCropAssistant();
    createPresetEditor();
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


void WobblyWindow::initialiseUIFromProject() {
    // Crop.
    for (int i = 0; i < 4; i++)
        crop_spin[i]->blockSignals(true);

    crop_spin[0]->setValue(project->crop.left);
    crop_spin[1]->setValue(project->crop.top);
    crop_spin[2]->setValue(project->crop.right);
    crop_spin[3]->setValue(project->crop.bottom);

    for (int i = 0; i < 4; i++)
        crop_spin[i]->blockSignals(false);


    // Resize.
    for (int i = 0; i < 2; i++)
        resize_spin[i]->blockSignals(true);

    resize_spin[0]->setValue(project->resize.width);
    resize_spin[1]->setValue(project->resize.height);

    for (int i = 0; i < 2; i++)
        resize_spin[i]->blockSignals(false);


    // Presets.
    for (auto it = project->presets.cbegin(); it != project->presets.cend(); it++)
        preset_combo->addItem(QString::fromStdString(it->second.name));

    if (preset_combo->count()) {
        preset_combo->setCurrentIndex(0);
        presetChanged(preset_combo->currentText());
    }
}


void WobblyWindow::openProject() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Wobbly project"), QString(), QString(), nullptr, QFileDialog::DontUseNativeDialog);

    if (!path.isNull()) {
        WobblyProject *tmp = new WobblyProject(true);

        try {
            tmp->readProject(path.toStdString());

            project_path = path;

            if (project)
                delete project;
            project = tmp;

            initialiseUIFromProject();

            vsscript_clearOutput(vsscript, 1);

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

        if (project_path.isEmpty())
            saveProjectAs();
        else
            project->writeProject(project_path.toStdString());
    } catch (WobblyException &e) {
        QMessageBox::information(this, QStringLiteral("Error"), e.what());
    }
}


void WobblyWindow::saveProjectAs() {
    try {
        if (!project)
            throw WobblyException("Can't save the project because none has been loaded.");

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Wobbly project"), QString(), QString(), nullptr, QFileDialog::DontUseNativeDialog);

        if (!path.isNull()) {
            project->writeProject(path.toStdString());

            project_path = path;
        }
    } catch (WobblyException &e) {
        QMessageBox::information(this, QStringLiteral("Error"), e.what());
    }
}


void WobblyWindow::evaluateMainDisplayScript() {
    std::string script = project->generateMainDisplayScript(crop_dock->isVisible());

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
    if (!project)
        return;

    int twenty_percent = project->num_frames[current_section_set] * 20 / 100;

    displayFrame(current_frame - twenty_percent);
}


void WobblyWindow::jumpALotForward() {
    if (!project)
        return;

    int twenty_percent = project->num_frames[current_section_set] * 20 / 100;

    displayFrame(current_frame + twenty_percent);
}


void WobblyWindow::jumpToStart() {
    displayFrame(0);
}


void WobblyWindow::jumpToEnd() {
    displayFrame(INT_MAX - 16);
}


void WobblyWindow::jumpToNextSectionStart() {
    // XXX current_frame always refers to PostFieldMatch. Fix this.
    const Section *next_section = project->findNextSection(current_frame, current_section_set);

    if (next_section)
        displayFrame(next_section->start);
}


void WobblyWindow::jumpToPreviousSectionStart() {
    if (current_frame == 0)
        return;

    const Section *section = project->findSection(current_frame, current_section_set);
    if (section->start == current_frame)
        section = project->findSection(current_frame - 1, current_section_set);

    displayFrame(section->start);
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


void WobblyWindow::freezeForward() {
    if (current_frame == project->num_frames[PostSource] - 1)
        return;

    try {
        project->addFreezeFrame(current_frame, current_frame, current_frame + 1);

        evaluateMainDisplayScript();
    } catch (WobblyException &) {
        // XXX Maybe don't be silent.
    }
}


void WobblyWindow::freezeBackward() {
    if (current_frame == 0)
        return;

    try {
        project->addFreezeFrame(current_frame, current_frame, current_frame - 1);

        evaluateMainDisplayScript();
    } catch (WobblyException &) {

    }
}


void WobblyWindow::freezeRange() {
    static FreezeFrame ff = { -1, -1, -1 };

    // XXX Don't bother if first or last are part of a freezeframe.
    if (ff.first == -1)
        ff.first = current_frame;
    else if (ff.last == -1)
        ff.last = current_frame;
    else if (ff.replacement == -1) {
        ff.replacement = current_frame;
        try {
            project->addFreezeFrame(ff.first, ff.last, ff.replacement);

            evaluateMainDisplayScript();
        } catch (WobblyException &) {

        }

        ff = { -1, -1, -1 };
    }
}


void WobblyWindow::deleteFreezeFrame() {
    const FreezeFrame *ff = project->findFreezeFrame(current_frame);
    if (ff) {
        project->deleteFreezeFrame(ff->first);

        evaluateMainDisplayScript();
    }
}


void WobblyWindow::toggleDecimation() {
    if (project->isDecimatedFrame(current_frame))
        project->deleteDecimatedFrame(current_frame);
    else
        project->addDecimatedFrame(current_frame);

    updateFrameDetails();
}


void WobblyWindow::toggleCombed() {
    if (project->isCombedFrame(current_frame))
        project->deleteCombedFrame(current_frame);
    else
        project->addCombedFrame(current_frame);

    updateFrameDetails();
}


void WobblyWindow::addSection() {
    const Section *section = project->findSection(current_frame, current_section_set);
    if (section->start != current_frame) {
        project->addSection(current_frame, current_section_set);

        updateFrameDetails();
    }
}


void WobblyWindow::deleteSection() {
    const Section *section = project->findSection(current_frame, current_section_set);
    project->deleteSection(section->start, current_section_set);

    updateFrameDetails();
}


void WobblyWindow::cropChanged(int value) {
    (void)value;

    if (!project)
        return;

    project->setCrop(crop_spin[0]->value(), crop_spin[1]->value(), crop_spin[2]->value(), crop_spin[3]->value());

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &) {

    }
}


void WobblyWindow::resizeChanged(int value) {
    (void)value;

    if (!project)
        return;

    project->setResize(resize_spin[0]->value(), resize_spin[1]->value());
}


void WobblyWindow::cropAssistantVisibilityChanged(bool visible) {
    (void)visible;

    for (int i = 0; i < 4; i++) {
        crop_spin[i]->setFocusPolicy(visible ? Qt::StrongFocus : Qt::NoFocus);
        crop_spin[i]->clearFocus();
    }
    for (int i = 0; i < 2; i++) {
        resize_spin[i]->setFocusPolicy(visible ? Qt::StrongFocus : Qt::NoFocus);
        resize_spin[i]->clearFocus();
    }

    if (!project)
        return;

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &) {

    }
}


void WobblyWindow::presetChanged(const QString &text) {
    if (!project)
        return;

    if (text.isEmpty())
        preset_edit->setPlainText(QString());
    else
        preset_edit->setPlainText(QString::fromStdString(project->getPresetContents(text.toStdString())));
}


void WobblyWindow::presetEdited() {
    if (!project)
        return;

    if (preset_combo->currentIndex() == -1)
        return;

    project->setPresetContents(preset_combo->currentText().toStdString(), preset_edit->toPlainText().toStdString());
}


void WobblyWindow::presetNew() {
    if (!project)
        return;

    QString preset_name = QInputDialog::getText(this, QStringLiteral("New preset"), QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."));

    if (!preset_name.isEmpty()) {
        try {
            project->addPreset(preset_name.toStdString());

            preset_combo->addItem(preset_name);
            preset_combo->setCurrentText(preset_name);

            presetChanged(preset_name);
        } catch (WobblyException &e) {
            QMessageBox::information(this, QStringLiteral("Error"), e.what());
        }
    }
}


void WobblyWindow::presetRename() {
    if (!project)
        return;

    if (preset_combo->currentIndex() == -1)
        return;

    QString preset_name = QInputDialog::getText(this, QStringLiteral("Rename preset"), QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."), QLineEdit::Normal, preset_combo->currentText());

    if (!preset_name.isEmpty() && preset_name != preset_combo->currentText()) {
        try {
            project->renamePreset(preset_combo->currentText().toStdString(), preset_name.toStdString());

            preset_combo->setItemText(preset_combo->currentIndex(), preset_name);

            //presetChanged(preset_name);

            // If the preset's name was displayed in other places, update them.
        } catch (WobblyException &e) {
            QMessageBox::information(this, QStringLiteral("Error"), e.what());
        }
    }

}


void WobblyWindow::presetDelete() {
    if (!project)
        return;

    if (preset_combo->currentIndex() == -1)
        return;

    project->deletePreset(preset_combo->currentText().toStdString());

    preset_combo->removeItem(preset_combo->currentIndex());

    presetChanged(preset_combo->currentText());
}
