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


#ifndef COMBEDFRAMESCOLLECTOR_H
#define COMBEDFRAMESCOLLECTOR_H

#include <set>

#include <VapourSynth.h>
#include <VSScript.h>

#include <QElapsedTimer>
#include <QObject>

class CombedFramesCollector : public QObject {
    Q_OBJECT

    const VSAPI *vsapi;
    VSCore *vscore;
    VSScript *vsscript;
    VSNodeRef *vsnode;

    bool aborted;
    int request_count;
    int next_frame;
    int num_frames;
    int frames_left;

    QElapsedTimer update_timer;
    QElapsedTimer elapsed_timer;

    std::set<int> combed_frames;

    static void VS_CC frameDoneCallback(void *userData, const VSFrameRef *f, int n, VSNodeRef *, const char *errorMsg);

private slots:
    void frameDone(void *frame_v, int n, const QString &error_msg);

public:
    CombedFramesCollector(const VSAPI *_vsapi, VSCore *_vscore, VSScript *_vsscript);

    void start(std::string script, const char *script_name);

signals:
    void workFinished();
    void progressUpdate(int frame);
    void speedUpdate(double fps, QString time_left);
    void errorMessage(const char *text);
    void combedFramesCollected(const std::set<int> &frames);

public slots:
    void stop();
};

#endif // COMBEDFRAMESCOLLECTOR_H
