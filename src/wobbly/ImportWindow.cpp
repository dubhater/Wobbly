/*

Copyright (c) 2015, John Smith

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted, provided that the
above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/


#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "ImportWindow.h"


ImportWindow::ImportWindow(const QString &_file_name, const ImportedThings &import_things, QWidget *parent)
    : QDialog(parent)
    , file_name(_file_name)
    , file_name_edit(nullptr)
{
    setWindowTitle(QStringLiteral("Import from project"));


    file_name_edit = new QLineEdit(file_name);

    QPushButton *browse_button = new QPushButton(QStringLiteral("&Browse"));
    QPushButton *previous_button = new QPushButton(QStringLiteral("Previous in &series"));
    
    QCheckBox *geometry_check = new QCheckBox(QStringLiteral("&Geometry"));
    QCheckBox *zoom_check = new QCheckBox(QStringLiteral("&Zoom"));
    QCheckBox *presets_check = new QCheckBox(QStringLiteral("&Presets"));
    QCheckBox *cl_check = new QCheckBox(QStringLiteral("Custom &lists"));
    QCheckBox *crop_check = new QCheckBox(QStringLiteral("&Crop"));
    QCheckBox *resize_check = new QCheckBox(QStringLiteral("&Resize"));
    QCheckBox *bits_check = new QCheckBox(QStringLiteral("Bit &depth"));
    QCheckBox *mic_check = new QCheckBox(QStringLiteral("&Mic search threshold"));
    
    geometry_check->setChecked(import_things.geometry);
    zoom_check->setChecked(import_things.zoom);
    presets_check->setChecked(import_things.presets);
    cl_check->setChecked(import_things.custom_lists);
    crop_check->setChecked(import_things.crop);
    resize_check->setChecked(import_things.resize);
    bits_check->setChecked(import_things.bit_depth);
    mic_check->setChecked(import_things.mic_search);

    QPushButton *cancel_button = new QPushButton(QStringLiteral("Cancel"));
    
    QPushButton *import_button = new QPushButton(QStringLiteral("&Import"));
    import_button->setDefault(true);


    connect(browse_button, &QPushButton::clicked, [this] () {
        QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Select Wobbly project"), file_name, QStringLiteral("Wobbly projects (*.json);;All files (*)"));

        if (!path.isEmpty()) {
            file_name_edit->setText(path);

            file_name = path;
        }
    });

    connect(previous_button, &QPushButton::clicked, [this] () {
        emit previousWanted();
    });
    
    connect(cancel_button, &QPushButton::clicked, this, &ImportWindow::reject);

    connect(import_button, &QPushButton::clicked, [this, geometry_check, presets_check, cl_check, crop_check, resize_check, bits_check, mic_check, zoom_check] () {
        ImportedThings imports;
        imports.geometry = geometry_check->isChecked();
        imports.zoom = zoom_check->isChecked();
        imports.presets = presets_check->isChecked();
        imports.custom_lists = cl_check->isChecked();
        imports.crop = crop_check->isChecked();
        imports.resize = resize_check->isChecked();
        imports.bit_depth = bits_check->isChecked();
        imports.mic_search = mic_check->isChecked();

        emit import(file_name, imports);
    });
    

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(new QLabel("Import from:"));

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(file_name_edit);

    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(browse_button);
    hbox->addWidget(previous_button);
    hbox->addStretch(1);

    vbox->addLayout(hbox);

    vbox->addWidget(geometry_check);
    vbox->addWidget(zoom_check);
    vbox->addWidget(presets_check);
    vbox->addWidget(cl_check);
    vbox->addWidget(crop_check);
    vbox->addWidget(resize_check);
    vbox->addWidget(bits_check);
    vbox->addWidget(mic_check);

    hbox = new QHBoxLayout;
    hbox->addWidget(cancel_button);
    hbox->addStretch(1);
    hbox->addWidget(import_button);
    
    vbox->addLayout(hbox);


    setLayout(vbox);
}


void ImportWindow::setFileName(const QString &new_name) {
    file_name_edit->setText(new_name);

    file_name = new_name;
}
