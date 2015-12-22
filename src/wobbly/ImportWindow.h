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


#ifndef IMPORTWINDOW_H
#define IMPORTWINDOW_H

#include <QDialog>
#include <QLineEdit>

#include "WobblyProject.h"


class ImportWindow : public QDialog {
    Q_OBJECT

public:
    ImportWindow(const QString &_file_name, const ImportedThings &imports, QWidget *parent=0);

    void setFileName(const QString &new_name);

signals:
    void import(const QString &file_name, const ImportedThings &imports);
    void previousWanted();

private:
    QString file_name;

    QLineEdit *file_name_edit;
};

#endif // IMPORTWINDOW_H
