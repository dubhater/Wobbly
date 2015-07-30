#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <VSScript.h>

#include "WobblyException.h"
#include "WibblyWindow.h"

WibblyWindow::WibblyWindow()
    : QMainWindow()
{
    createUI();
}

void WibblyWindow::createUI() {
    setWindowTitle(QStringLiteral("Wibbly Metrics Collector v%1").arg(PACKAGE_VERSION));

    statusBar()->setSizeGripEnabled(true);

    createMainWindow();
    createVideoOutputWindow();
    createCropWindow();
    createVFMWindow();
    createVDecimateWindow();
}

void WibblyWindow::createMainWindow() {
    main_jobs_list = new ListWidget;
    main_jobs_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    main_jobs_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QPushButton *main_add_jobs_button = new QPushButton("Add jobs");
    QPushButton *main_remove_jobs_button = new QPushButton("Remove jobs");
    QPushButton *main_copy_jobs_button = new QPushButton("Copy jobs");
    QPushButton *main_move_jobs_up_button = new QPushButton("Move up");
    QPushButton *main_move_jobs_down_button = new QPushButton("Move down");

    QLineEdit *main_destination_edit = new QLineEdit;

    QPushButton *main_choose_button = new QPushButton("Choose");

    ListWidget *main_steps_list = new ListWidget;

    QPushButton *main_configure_button = new QPushButton("Configure step");

    main_progress_bar = new QProgressBar;

    QPushButton *main_engage_button = new QPushButton("Engage");


    // connect


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(main_jobs_list, 1);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(new QLabel("Destination:"));
    hbox->addWidget(main_destination_edit);
    hbox->addWidget(main_choose_button);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;

    QVBoxLayout *vbox2 = new QVBoxLayout;
    vbox2->addWidget(main_add_jobs_button);
    vbox2->addWidget(main_remove_jobs_button);
    vbox2->addWidget(main_copy_jobs_button);
    vbox2->addWidget(main_move_jobs_up_button);
    vbox2->addWidget(main_move_jobs_down_button);
    vbox2->addStretch(1);
    vbox2->addWidget(main_configure_button);
    hbox->addLayout(vbox2);

    hbox->addWidget(main_steps_list);

    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(main_progress_bar);
    hbox->addWidget(main_engage_button);

    vbox->addLayout(hbox);


    QWidget *main_widget = new QWidget;
    main_widget->setLayout(vbox);

    setCentralWidget(main_widget);
}

void WibblyWindow::createVideoOutputWindow() {

}

void WibblyWindow::createCropWindow() {

}

void WibblyWindow::createVFMWindow() {

}

void WibblyWindow::createVDecimateWindow() {

}

