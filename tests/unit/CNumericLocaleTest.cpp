// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <clocale>
#include <cstring>
#include <string>

#include "core/CNumericLocale.h"

namespace {

// Locales whose numeric formatting uses a comma. A locale's name does not
// promise that, and which locales a machine has installed varies, so each
// candidate is accepted only once localeconv() confirms the comma.
constexpr const char* kCommaLocales[] = {"en_DK.UTF-8", "ru_RU.UTF-8", "de_DE.UTF-8",
                                         "fr_FR.UTF-8", "es_ES.UTF-8", "nb_NO.UTF-8"};

// Leaves the first installed comma-decimal locale active and returns its name,
// or returns nullptr with the process locale unchanged if none qualifies.
const char* SetFirstAvailableCommaLocale() {
    for (const char* candidate : kCommaLocales) {
        if (std::setlocale(LC_ALL, candidate) == nullptr) {
            continue;
        }
        if (std::strcmp(std::localeconv()->decimal_point, ",") == 0) {
            return candidate;
        }
    }
    return nullptr;
}

std::string CandidateList() {
    std::string list;
    for (const char* candidate : kCommaLocales) {
        if (!list.empty()) {
            list += ", ";
        }
        list += candidate;
    }
    return list;
}

}  // namespace

TEST(CNumericLocale, ForcesDecimalPointEvenUnderACommaLocale) {
    const std::string previous = std::setlocale(LC_ALL, nullptr);

    const char* commaLocale = SetFirstAvailableCommaLocale();
    if (commaLocale == nullptr) {
        std::setlocale(LC_ALL, previous.c_str());
        GTEST_SKIP() << "No comma-decimal locale is installed on this machine (tried "
                     << CandidateList() << ")";
    }

    wbc::UseCNumericLocale();
    EXPECT_STREQ(std::setlocale(LC_NUMERIC, nullptr), "C");
    EXPECT_STREQ(std::localeconv()->decimal_point, ".");

    std::setlocale(LC_ALL, previous.c_str());
}

TEST(CNumericLocale, ReturnsTheLocaleThatWasInEffectBefore) {
    const std::string previous = std::setlocale(LC_ALL, nullptr);

    const std::string returned = wbc::UseCNumericLocale();
    EXPECT_FALSE(returned.empty());

    std::setlocale(LC_ALL, previous.c_str());
}
