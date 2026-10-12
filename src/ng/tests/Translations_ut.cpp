/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Translations.h"

#include "base/tests/UtAssert.h"

static bool LocaleIs(Str locale, Str code) {
    int idx = trans::LangIdxForLocale(locale);
    return idx >= 0 && str::Eq(trans::GetLangCodeByIdxTemp(idx), code);
}

// macOS language ids and $LANG values to our codes, which are not all ISO
bool Translations_UnitTestLocales() {
    utassert(LocaleIs(StrL("es-PE"), StrL("es")));
    utassert(LocaleIs(StrL("es-419"), StrL("es")));
    utassert(LocaleIs(StrL("de_DE.UTF-8"), StrL("de")));
    utassert(LocaleIs(StrL("en-US"), StrL("en")));
    utassert(LocaleIs(StrL("pt-BR"), StrL("br")));
    utassert(LocaleIs(StrL("pt-PT"), StrL("pt")));
    utassert(LocaleIs(StrL("zh-Hans-CN"), StrL("cn")));
    utassert(LocaleIs(StrL("zh-Hant-TW"), StrL("tw")));
    utassert(LocaleIs(StrL("cs-CZ"), StrL("cz")));
    utassert(LocaleIs(StrL("sr-Latn-RS"), StrL("sp-rs")));
    utassert(LocaleIs(StrL("ca-ES-valencia"), StrL("ca-xv")));
    utassert(LocaleIs(StrL("my-MM"), StrL("mm")));
    utassert(LocaleIs(StrL("ms-MY"), StrL("my")));

    // Breton and Amharic, whose ISO codes our table has for other languages
    utassert(trans::LangIdxForLocale(StrL("br-FR")) < 0);
    utassert(trans::LangIdxForLocale(StrL("am-ET")) < 0);
    utassert(trans::LangIdxForLocale(StrL("C.UTF-8")) < 0);
    return true;
}
