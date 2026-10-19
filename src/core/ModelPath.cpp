// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/ModelPath.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace wbc {

QStringList ModelSearchPath(const QString& environmentOverride,
                            const QString& applicationDirectory) {
    QStringList candidates;

    // An explicit override answers first, which is what makes it an override.
    if (!environmentOverride.isEmpty()) {
        candidates.append(environmentOverride);
    }

    // Beside an installed binary: <prefix>/bin/wbc finds
    // <prefix>/share/wbc/. Relative to the executable rather
    // than to a compiled-in prefix, so that a relocated tree still works.
    if (!applicationDirectory.isEmpty()) {
        candidates.append(QDir::cleanPath(applicationDirectory + "/../share/wbc/" +
                                          kModelFileName));
    }

    // The prefix this build was configured with, for an installation the
    // executable cannot reach by a relative path.
    candidates.append(QString::fromUtf8(WBC_INSTALLED_MODEL_PATH));

    // The build tree, for anyone running what they just compiled: the build
    // copies the model beside the executable, so that the program never
    // names a path in the source tree.
    if (!applicationDirectory.isEmpty()) {
        candidates.append(QDir::cleanPath(applicationDirectory + "/" + kModelFileName));
    }

    candidates.removeDuplicates();
    return candidates;
}  // GCOVR_EXCL_LINE — gcc's exception-unwinding epilogue for the local
   // QStringList, reached only if an append() throws. Qt's containers
   // throw only on allocation failure, which the tests cannot provoke.

QString FindModelFile(const QStringList& candidates, const FileExistsFn& exists) {
    for (const QString& candidate : candidates) {
        if (exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

QString FindModelFile() {
    const QString override =
        QProcessEnvironment::systemEnvironment().value("WBC_MODEL");
    const QString directory =
        QCoreApplication::instance() != nullptr ? QCoreApplication::applicationDirPath() : QString{};
    return FindModelFile(ModelSearchPath(override, directory),
                         [](const QString& path) { return QFileInfo::exists(path); });
}

}  // namespace wbc
