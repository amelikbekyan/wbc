// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>

#include <cstdlib>

#include "core/ModelPath.h"

TEST(ModelSearchPath, AsksTheOverrideFirst) {
    const QStringList candidates = wbc::ModelSearchPath("/somewhere/mine.onnx", "/opt/app/bin");
    ASSERT_FALSE(candidates.isEmpty());
    EXPECT_EQ(candidates.first(), "/somewhere/mine.onnx");
}

TEST(ModelSearchPath, LooksBesideTheExecutableBeforeTheConfiguredPrefix) {
    const QStringList candidates = wbc::ModelSearchPath({}, "/opt/app/bin");
    ASSERT_FALSE(candidates.isEmpty());
    // <prefix>/bin/wbc has to find <prefix>/share/..., so that
    // a tree moved after installation still works.
    EXPECT_EQ(candidates.first(),
              QString("/opt/app/share/wbc/") + wbc::kModelFileName);
}

TEST(ModelSearchPath, LeavesOutWhatItWasNotGiven) {
    const QStringList candidates = wbc::ModelSearchPath({}, {});
    for (const QString& candidate : candidates) {
        EXPECT_FALSE(candidate.isEmpty());
    }
    // The configured prefix needs nothing supplied, so the list is never
    // empty.
    EXPECT_EQ(candidates, QStringList{WBC_INSTALLED_MODEL_PATH});
}

TEST(ModelSearchPath, LooksBesideTheExecutableLast) {
    // Where the build tree keeps its copy. Last, so that an installed copy
    // wins for an installed binary.
    const QStringList candidates = wbc::ModelSearchPath({}, "/build/release");
    ASSERT_FALSE(candidates.isEmpty());
    EXPECT_EQ(candidates.last(), QString("/build/release/") + wbc::kModelFileName);
}

TEST(ModelSearchPath, NeverNamesTheSourceTree) {
    // The program must carry no path from the machine it was built on; the
    // tests are the only ones that know where the model was downloaded.
    const QString source = QFileInfo(WBC_MODEL_PATH).absolutePath();
    for (const QString& candidate : wbc::ModelSearchPath({}, "/opt/app/bin")) {
        EXPECT_FALSE(candidate.startsWith(source)) << qPrintable(candidate);
    }
}

TEST(ModelSearchPath, NamesEachPlaceOnlyOnce) {
    // A build configured with its prefix inside the source tree would
    // otherwise produce the same path twice, and a failure message listing it
    // twice reads like a bug in the search.
    const QStringList candidates = wbc::ModelSearchPath(WBC_MODEL_PATH, "/opt/app/bin");
    EXPECT_EQ(candidates.size(), QSet<QString>(candidates.begin(), candidates.end()).size());
}

TEST(FindModelFile, TakesTheFirstCandidateThatIsThere) {
    const QStringList candidates{"/first", "/second", "/third"};
    const QString found = wbc::FindModelFile(
        candidates, [](const QString& path) { return path == "/second" || path == "/third"; });
    EXPECT_EQ(found, "/second");
}

TEST(FindModelFile, ReturnsNothingWhenNoCandidateIsThere) {
    const QStringList candidates{"/first", "/second"};
    EXPECT_TRUE(wbc::FindModelFile(candidates, [](const QString&) { return false; }).isEmpty());
}

TEST(FindModelFile, HonoursTheEnvironmentOverride) {
    QTemporaryDir directory;
    const QString model = directory.filePath("elsewhere.onnx");
    {
        QFile file(model);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    }

    ASSERT_EQ(::setenv("WBC_MODEL", model.toUtf8().constData(), 1), 0);
    const QString found = wbc::FindModelFile();
    ::unsetenv("WBC_MODEL");

    EXPECT_EQ(found, model);
}
