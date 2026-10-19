// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <QApplication>

#include <cstdlib>

int main(int argc, char** argv) {
    // Set before QApplication is constructed; afterwards it has no effect.
    ::setenv("QT_QPA_PLATFORM", "offscreen", 0);
    QApplication application(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
