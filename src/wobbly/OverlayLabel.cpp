/*

Copyright (c) 2018, John Smith

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


#include <QPainter>
#include <QPaintEvent>
#include <QTextDocument>

#include "OverlayLabel.h"


void OverlayLabel::setFramePixmapSize(QSize new_size) {
    m_frame_pixmap_size = new_size;

    update();
}


void OverlayLabel::paintEvent(QPaintEvent *) {
    QTextDocument doc;
    doc.setHtml("<font color=black>" + text() + "</font>");


    QPoint top_left(std::max(0, width() - m_frame_pixmap_size.width()) / 2,
                    std::max(0, height() - m_frame_pixmap_size.height()) / 2);


    QImage img(size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(0, 0, 0, 0));

    int passes = 2;

    {
        QPainter paint(&img);
        paint.setViewport(QRect(top_left, img.size()));
        doc.drawContents(&paint);
    }

    QImage img2 = img.copy();

    for (int pass = 0; pass < passes; pass++) {
        for (int y = 1; y < img.height() - 1; y++) {
            const uint32_t *above = (const uint32_t *)img.constScanLine(y - 1);
            const uint32_t *middle = (const uint32_t *)img.constScanLine(y);
            const uint32_t *below = (const uint32_t *)img.constScanLine(y + 1);
            uint32_t *dest = (uint32_t *)img2.scanLine(y);

            for (int x = 1; x < img.width() - 1; x++) {
                uint32_t maximum = middle[x];

                maximum = std::max(maximum, above[x - 1]);
                maximum = std::max(maximum, above[x]);
                maximum = std::max(maximum, above[x + 1]);

                maximum = std::max(maximum, middle[x - 1]);
                maximum = std::max(maximum, middle[x + 1]);

                maximum = std::max(maximum, below[x - 1]);
                maximum = std::max(maximum, below[x]);
                maximum = std::max(maximum, below[x + 1]);

                dest[x] = maximum;
            }
        }

        img.swap(img2);
    }

    {
        QPainter paint(&img);
        paint.setViewport(QRect(top_left, img.size()));
        doc.setHtml("<font color=white>" + text() + "</font>");
        doc.drawContents(&paint);
    }

    QPainter paint(this);
    paint.drawImage(rect(), img);
}
