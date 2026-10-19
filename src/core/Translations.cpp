// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Translations.h"

#include <QCoreApplication>
#include <QFile>
#include <QLibraryInfo>

#include <utility>

namespace wbc {

QStringList AvailableLanguages(const QString& resourcePrefix) {
    QStringList languages{QString::fromLatin1(kSourceLanguage)};
    for (const char* code : {"ru", "de", "fr", "es"}) {
        if (QFile::exists(resourcePrefix + code + ".qm")) {
            languages.append(QString::fromLatin1(code));
        }
    }
    return languages;
}

QString LanguageDisplayName(const QString& code) {
    if (code == "en") {
        return QStringLiteral("English");
    }
    if (code == "ru") {
        return QStringLiteral("Русский");
    }
    if (code == "de") {
        return QStringLiteral("Deutsch");
    }
    if (code == "fr") {
        return QStringLiteral("Français");
    }
    if (code == "es") {
        return QStringLiteral("Español");
    }
    return code;
}

QString ResolveLanguage(const QString& stored, const QLocale& system) {
    const QStringList available = AvailableLanguages();
    if (available.contains(stored)) {
        return stored;
    }
    // QLocale::name() is a full name such as "ru_RU"; the part before the
    // underscore is the language, which is as fine a distinction as this
    // application draws.
    const QString systemLanguage = system.name().section('_', 0, 0);
    if (available.contains(systemLanguage)) {
        return systemLanguage;
    }
    return QString::fromLatin1(kSourceLanguage);
}

LanguageSwitcher::LanguageSwitcher(QCoreApplication& app,
                                   QString resourcePrefix,
                                   QString qtTranslationsDir)
    : app_(app),
      resourcePrefix_(std::move(resourcePrefix)),
      qtTranslationsDir_(qtTranslationsDir.isEmpty()
                             ? QLibraryInfo::path(QLibraryInfo::TranslationsPath)
                             : std::move(qtTranslationsDir)),
      current_(QString::fromLatin1(kSourceLanguage)) {}

bool LanguageSwitcher::Use(const QString& language) {
    app_.removeTranslator(&appTranslator_);
    app_.removeTranslator(&qtTranslator_);
    current_ = QString::fromLatin1(kSourceLanguage);

    if (!AvailableLanguages().contains(language) || language == kSourceLanguage) {
        // Removing the translators above is the whole of the work: with none
        // installed, tr() answers with the source text, which is English.
        return language == kSourceLanguage;
    }

    if (!appTranslator_.load(resourcePrefix_ + language)) {
        return false;
    }
    app_.installTranslator(&appTranslator_);
    current_ = language;

    if (qtTranslator_.load("qtbase_" + language, qtTranslationsDir_)) {
        app_.installTranslator(&qtTranslator_);
    }
    return true;
}

QString LanguageSwitcher::Current() const {
    return current_;
}

}  // namespace wbc
