// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QLocale>
#include <QString>
#include <QStringList>
#include <QTranslator>

class QCoreApplication;

namespace wbc {

// The language the interface is written in. It needs no translation file:
// the source text is the English text.
inline constexpr const char* kSourceLanguage = "en";

// Where the build embeds the compiled translations, as wbc_<code>.qm.
inline constexpr const char* kEmbeddedTranslations = ":/i18n/wbc_";

// Language codes this build can show, source language first: the source
// language, and each translation whose compiled file is embedded under
// `resourcePrefix`. A build without lrelease embeds none, and then offers the
// source language alone rather than choices that would change nothing.
QStringList AvailableLanguages(const QString& resourcePrefix = kEmbeddedTranslations);

// The language's own name for itself, which is what a language menu should
// show: a reader looking for their language does not know the word for it in
// the language they are trying to leave. Returns the code itself for a code
// the application does not ship.
QString LanguageDisplayName(const QString& code);

// Decides which language to start in: a remembered choice if it is one that
// ships, otherwise the system's if that ships, otherwise the source language.
QString ResolveLanguage(const QString& stored, const QLocale& system);

// Owns the installed translators. Switching removes the previous ones, which
// a bare QTranslator per switch would not: Qt keeps every translator that was
// installed and consults them in turn, so translations would accumulate and
// the earliest-installed language would keep answering first.
class LanguageSwitcher {
public:
    // Where the compiled translations were embedded. A parameter because a
    // build without lrelease has none embedded at all, and that configuration
    // has to behave -- falling back to the source language -- rather than
    // being a case nothing ever exercises.
    //
    // Qt's own translations are looked for in `qtTranslationsDir`, which is
    // Qt's installed location unless a test says otherwise: whether they are
    // installed differs from machine to machine, and both answers have to be
    // exercised wherever the tests run.
    explicit LanguageSwitcher(QCoreApplication& app,
                              QString resourcePrefix = kEmbeddedTranslations,
                              QString qtTranslationsDir = {});

    // Installs `language`, or leaves the interface in the source language if
    // it does not ship. Returns true when the requested language is in
    // effect, which the source language always is.
    bool Use(const QString& language);
    QString Current() const;

private:
    QCoreApplication& app_;
    // Qt's own translations, for the text inside standard dialogs. Missing on
    // a machine that installed Qt without them, which is not an error: the
    // dialogs then speak English while the rest of the window does not.
    QTranslator qtTranslator_;
    QTranslator appTranslator_;
    QString resourcePrefix_;
    QString qtTranslationsDir_;
    QString current_;
};

}  // namespace wbc
