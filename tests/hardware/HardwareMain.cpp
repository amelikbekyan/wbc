// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

// CTest sees only this process's exit code for the whole hardware entry, so
// a run where every case skipped for want of a device would otherwise be
// reported as a pass — the precise outcome the spec calls worse than no
// test at all. Exit 77, which the CTest entry is configured to read as
// "skipped", when nothing actually ran.
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();

    const ::testing::UnitTest& unitTest = *::testing::UnitTest::GetInstance();
    const int planned = unitTest.test_to_run_count();
    if (result == 0 && planned > 0 && unitTest.skipped_test_count() == planned) {
        return 77;
    }
    return result;
}
