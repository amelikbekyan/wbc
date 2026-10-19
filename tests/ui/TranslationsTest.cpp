// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include "core/Translations.h"

namespace {

// Any string from the window, asked for the way the window asks for it.
QString BlurInCurrentLanguage() {
    return QCoreApplication::translate("wbc::MainWindow", "Blur");
}

// Leaves the application back in the source language whatever a test did, so
// that one test's language cannot decide the next test's answers.
struct BackToEnglish {
    wbc::LanguageSwitcher& switcher;
    ~BackToEnglish() { switcher.Use(wbc::kSourceLanguage); }
};

}  // namespace

TEST(LanguageSwitcher, StartsInTheSourceLanguage) {
    wbc::LanguageSwitcher switcher{*QCoreApplication::instance()};
    EXPECT_EQ(switcher.Current(), wbc::kSourceLanguage);
    EXPECT_EQ(BlurInCurrentLanguage(), "Blur");
}

TEST(LanguageSwitcher, InstallsAShippedLanguage) {
    wbc::LanguageSwitcher switcher{*QCoreApplication::instance()};
    const BackToEnglish cleanup{switcher};

    ASSERT_TRUE(switcher.Use("ru"));
    EXPECT_EQ(switcher.Current(), "ru");
    EXPECT_NE(BlurInCurrentLanguage(), "Blur") << "the window is still speaking English";
}

TEST(LanguageSwitcher, SwitchingAgainReplacesTheLanguageRatherThanAddingToIt) {
    wbc::LanguageSwitcher switcher{*QCoreApplication::instance()};
    const BackToEnglish cleanup{switcher};

    ASSERT_TRUE(switcher.Use("ru"));
    const QString inRussian = BlurInCurrentLanguage();
    ASSERT_TRUE(switcher.Use("de"));
    const QString inGerman = BlurInCurrentLanguage();

    // Qt consults installed translators in turn, so a switcher that installed
    // without removing would still be answering in the first language.
    EXPECT_NE(inGerman, inRussian);
    EXPECT_EQ(switcher.Current(), "de");
}

TEST(LanguageSwitcher, ReturningToTheSourceLanguageRemovesTheTranslation) {
    wbc::LanguageSwitcher switcher{*QCoreApplication::instance()};
    ASSERT_TRUE(switcher.Use("fr"));
    ASSERT_TRUE(switcher.Use(wbc::kSourceLanguage));

    EXPECT_EQ(switcher.Current(), wbc::kSourceLanguage);
    EXPECT_EQ(BlurInCurrentLanguage(), "Blur");
}

TEST(LanguageSwitcher, RefusesALanguageItDoesNotShipAndLeavesEnglishInPlace) {
    wbc::LanguageSwitcher switcher{*QCoreApplication::instance()};
    const BackToEnglish cleanup{switcher};

    EXPECT_FALSE(switcher.Use("ja"));
    EXPECT_EQ(switcher.Current(), wbc::kSourceLanguage);
    EXPECT_EQ(BlurInCurrentLanguage(), "Blur");
}

TEST(LanguageSwitcher, FallsBackToEnglishWhenTheTranslationsWereNotBuilt) {
    // lrelease is optional: a build without it embeds nothing, and asking for
    // a shipped language then has to fail rather than half-succeed.
    wbc::LanguageSwitcher switcher{*QCoreApplication::instance(), ":/i18n/nothing_was_built_"};
    const BackToEnglish cleanup{switcher};

    EXPECT_FALSE(switcher.Use("ru"));
    EXPECT_EQ(switcher.Current(), wbc::kSourceLanguage);
    EXPECT_EQ(BlurInCurrentLanguage(), "Blur");
}

TEST(LanguageSwitcher, InstallsQtsOwnTranslationsWhenTheyAreThere) {
    // Any compiled translation will do as a stand-in for Qt's: what is being
    // tested is that the switcher finds and installs one, not what it says.
    QTemporaryDir qtDir;
    ASSERT_TRUE(QFile::copy(":/i18n/wbc_ru.qm", qtDir.filePath("qtbase_ru.qm")));

    wbc::LanguageSwitcher switcher{*QCoreApplication::instance(), ":/i18n/wbc_",
                                  qtDir.path()};
    const BackToEnglish cleanup{switcher};

    EXPECT_TRUE(switcher.Use("ru"));
    EXPECT_EQ(switcher.Current(), "ru");
}

TEST(LanguageSwitcher, DoesWithoutQtsOwnTranslationsWhenTheyAreMissing) {
    // A machine that installed Qt without its translations: the window still
    // switches, and only the standard dialogs stay in English.
    QTemporaryDir emptyDir;
    wbc::LanguageSwitcher switcher{*QCoreApplication::instance(), ":/i18n/wbc_",
                                  emptyDir.path()};
    const BackToEnglish cleanup{switcher};

    EXPECT_TRUE(switcher.Use("ru"));
    EXPECT_NE(BlurInCurrentLanguage(), "Blur");
}
