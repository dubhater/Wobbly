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


#include "CombedFramesCollector.h"


CombedFramesCollector::CombedFramesCollector(const VSSCRIPTAPI *_vssapi, const VSAPI *_vsapi, VSCore *_vscore, VSScript *_vsscript)
    : vssapi(_vssapi)
    , vsapi(_vsapi)
    , vscore(_vscore)
    , vsscript(_vsscript)
{

}


void CombedFramesCollector::start(std::string script, const char *script_name) {
    script +=
            "src = vs.get_output(index=0)\n"

            // Since VapourSynth R41 get_output returns the alpha as well.
            "if isinstance(src, tuple):\n"
            "    src = src[0]\n"

            "src = c.tdm.IsCombed(src)\n"

            "src.set_output()\n";

    vssapi->evalSetWorkingDir(vsscript, 1);
    if (vssapi->evaluateBuffer(vsscript, script.c_str(), script_name)) {
        QString error = vssapi->getError(vsscript);
        // The traceback is mostly unnecessary noise.
        int traceback = error.indexOf(QStringLiteral("Traceback"));
        if (traceback != -1)
            error.insert(traceback, '\n');

        emit errorMessage(QStringLiteral("Failed to evaluate final script. Error message:\n%1").arg(error).toUtf8().constData());
        emit workFinished();
        return;
    }

    vsnode = vssapi->getOutputNode(vsscript, 0);
    if (!vsnode) {
        emit errorMessage("Final script evaluated successfully, but no node found at output index 0.");
        emit workFinished();
        return;
    }

    num_frames = vsapi->getVideoInfo(vsnode)->numFrames;

    VSCoreInfo core_info;
    vsapi->getCoreInfo(vscore, &core_info);

    int requests = std::min(core_info.numThreads, num_frames);

    aborted = false;
    frames_left = num_frames;
    next_frame = 0;
    request_count = 0;
    elapsed_timer.start();
    update_timer.start();

    for (int i = 0; i < requests; i++) {
        request_count++;
        vsapi->getFrameAsync(next_frame, vsnode, CombedFramesCollector::frameDoneCallback, (void *)this);
        next_frame++;
    }
}


void CombedFramesCollector::stop() {
    aborted = true;
}


void VS_CC CombedFramesCollector::frameDoneCallback(void *userData, const VSFrame *f, int n, VSNode *, const char *errorMsg) {
    CombedFramesCollector *collector = (CombedFramesCollector *)userData;

    // Qt::DirectConnection = frameDone runs in the worker threads
    // Qt::QueuedConnection = frameDone runs in the GUI thread
    QMetaObject::invokeMethod(collector,
                              "frameDone",
                              Qt::QueuedConnection,
                              Q_ARG(void *, (void *)f),
                              Q_ARG(int, n),
                              Q_ARG(QString, QString(errorMsg)));
}


void CombedFramesCollector::frameDone(void *frame_v, int n, const QString &error_msg) {
    const VSFrame *frame = (const VSFrame *)frame_v;

    if (aborted) {
        vsapi->freeFrame(frame);
    } else {
        if (frame) {
            // Extract the _Combed property
            const VSMap *props = vsapi->getFramePropertiesRO(frame);

            int err;

            if (vsapi->mapGetInt(props, "_Combed", 0, &err))
                combed_frames.insert(n);

            vsapi->freeFrame(frame);

            // Request another frame
            if (next_frame < num_frames) {
                request_count++;
                vsapi->getFrameAsync(next_frame, vsnode, CombedFramesCollector::frameDoneCallback, (void *)this);
                next_frame++;
            }

            frames_left--;

            // Send progress updates
            if (update_timer.elapsed() >= 5000) {
                update_timer.start();

                qint64 elapsed_milliseconds = elapsed_timer.elapsed();
                double frames_per_second = (double)(num_frames - frames_left) * 1000 / elapsed_milliseconds;
                int seconds_left = (int)(frames_left / frames_per_second);
                int minutes_left = seconds_left / 60;
                seconds_left = seconds_left % 60;
                int hours_left = minutes_left / 60;
                minutes_left = minutes_left % 60;

                emit speedUpdate(frames_per_second,
                                 QStringLiteral("%1:%2:%3")
                                 .arg(hours_left, 2, 10, QLatin1Char('0'))
                                 .arg(minutes_left, 2, 10, QLatin1Char('0'))
                                 .arg(seconds_left, 2, 10, QLatin1Char('0')));

                emit progressUpdate(num_frames - frames_left);
            }

            // Send the final results
            if (frames_left == 0)
                emit combedFramesCollected(combed_frames);
        } else {
            aborted = true;

            emit errorMessage(QStringLiteral("Combed frames collector: failed to retrieve frame number %1. Error message:\n\n%2").arg(n).arg(error_msg).toUtf8().constData());
        }
    }

    request_count--;

    // All frames processed, or there was an error.
    // Either way we're done. This function isn't getting called again.
    if (request_count == 0) {
        vsapi->freeNode(vsnode);

        emit workFinished();
    }
}
