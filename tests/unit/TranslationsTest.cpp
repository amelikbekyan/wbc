// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <QLocale>
#include <QSet>

#include "core/Translations.h"

TEST(AvailableLanguages, StartsWithTheLanguageTheInterfaceIsWrittenIn) {
    const QStringList languages = wbc::AvailableLanguages();
    ASSERT_FALSE(languages.isEmpty());
    EXPECT_EQ(languages.first(), wbc::kSourceLanguage);
    // Every shipped language needs a file to be shipped with it, so a
    // duplicate here would quietly mean one was forgotten.
    EXPECT_EQ(languages.size(), QSet<QString>(languages.begin(), languages.end()).size());
}

TEST(AvailableLanguages, OffersOnlyTheSourceLanguageWhenNoTranslationWasBuilt) {
    // lrelease is optional, and a build without it embeds no translation: a
    // language menu that offered one anyway would change nothing when chosen.
    EXPECT_EQ(wbc::AvailableLanguages(":/i18n/nothing_was_built_"),
              QStringList{wbc::kSourceLanguage});
}

TEST(LanguageDisplayName, NamesEachLanguageInItsOwnWords) {
    // A reader looking for their language does not know the word for it in
    // the language they are trying to leave.
    EXPECT_EQ(wbc::LanguageDisplayName("ru"), QString::fromUtf8("Русский"));
    EXPECT_EQ(wbc::LanguageDisplayName("de"), "Deutsch");
    EXPECT_EQ(wbc::LanguageDisplayName("fr"), QString::fromUtf8("Français"));
    EXPECT_EQ(wbc::LanguageDisplayName("es"), QString::fromUtf8("Español"));
    EXPECT_EQ(wbc::LanguageDisplayName("en"), "English");
}

TEST(LanguageDisplayName, HandsBackACodeItDoesNotKnow) {
    EXPECT_EQ(wbc::LanguageDisplayName("xx"), "xx");
}

TEST(ResolveLanguage, PrefersARememberedChoiceOverTheSystem) {
    EXPECT_EQ(wbc::ResolveLanguage("de", QLocale("ru_RU")), "de");
}

TEST(ResolveLanguage, FallsBackToTheSystemWhenNothingWasRemembered) {
    EXPECT_EQ(wbc::ResolveLanguage({}, QLocale("ru_RU")), "ru");
    EXPECT_EQ(wbc::ResolveLanguage({}, QLocale("fr_CA")), "fr");
}

TEST(ResolveLanguage, FallsBackToTheSourceLanguageWhenNeitherShips) {
    EXPECT_EQ(wbc::ResolveLanguage({}, QLocale("ja_JP")), wbc::kSourceLanguage);
    // A settings file is editable by hand and survives upgrades, so a code
    // this build no longer ships has to resolve to something.
    EXPECT_EQ(wbc::ResolveLanguage("kl", QLocale("ja_JP")), wbc::kSourceLanguage);
}
