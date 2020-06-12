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


#include "UndoAction.h"


QString UndoAction::description() const {
    switch (type) {
    case AddFreezeFrame:
        return QStringLiteral("add freezeframe [%1,%2,%3]")
                .arg(freezeframes[0].first)
                .arg(freezeframes[0].last)
                .arg(freezeframes[0].replacement);
        break;
    case DeleteFreezeFrames:
        if (freezeframes.size() == 1)
            return QStringLiteral("delete freezeframe [%1,%2,%3]")
                    .arg(freezeframes[0].first)
                    .arg(freezeframes[0].last)
                    .arg(freezeframes[0].replacement);
        else
            return QStringLiteral("delete %1 freezeframes")
                    .arg(freezeframes.size());
        break;
    case AddRangeToCustomList:
        return QStringLiteral("add [%1,%2] to custom list '%3'")
                .arg(first_frame)
                .arg(last_frame)
                .arg(QString::fromStdString(cl_name));
        break;
    case AssignPresetToSection:
        return QStringLiteral("assign preset '%1' to section %2")
                .arg(QString::fromStdString(preset_name))
                .arg(section_start);
        break;
    case CycleMatch:
        return QStringLiteral("cycle the match for frame %1")
                .arg(first_frame);
        break;
    case AddSection:
        return QStringLiteral("add section %1")
                .arg(sections[0].start);
        break;
    case DeleteSections:
        if (sections.size() == 1)
            return QStringLiteral("delete section %1")
                    .arg(sections[0].start);
        else
            return QStringLiteral("delete %1 sections")
                    .arg(sections.size());
        break;
    case GuessSectionPatternsFromMatches:
        return QStringLiteral("guess patterns from matches for section %1")
                .arg(section_start);
        break;
    case GuessSectionPatternsFromMics:
        return QStringLiteral("guess patterns from mics for section %1")
                .arg(section_start);
        break;
    case GuessProjectPatternsFromMatches:
        return QStringLiteral("guess project patterns from matches");
        break;
    case GuessProjectPatternsFromMics:
        return QStringLiteral("guess project patterns from mics");
        break;
    }

    return QStringLiteral("a bug, probably");
}


UndoAction::Evaluate UndoAction::evaluate() const {
    if (!evaluate_needed)
        return EvaluateNothing;

    switch (type) {
    case AddFreezeFrame:
    case DeleteFreezeFrames:
    case CycleMatch:
    case GuessSectionPatternsFromMatches:
    case GuessSectionPatternsFromMics:
    case GuessProjectPatternsFromMatches:
    case GuessProjectPatternsFromMics:
        return EvaluateBoth;
        break;
    case AddRangeToCustomList:
    case AssignPresetToSection:
    case AddSection:
    case DeleteSections:
        return EvaluateFinalScript;
        break;
    }

    return EvaluateNothing;
}
