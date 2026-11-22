// The languages of the builder. English is the source: a text is its own key, and a text with no translation stays
// English, so a new language is one file in lang/. {0} and the like are the values put into a text. The names are the
// builder's own, so that a page that hosts it beside another editor keeps both.
'use strict';

const DGX_UI_LANGUAGES = { en: { title: 'English', texts: {} } };

function dgxUiTranslate(language, text, ...values) {
    const found = DGX_UI_LANGUAGES[language]?.texts[text] ?? text;
    return found.replace(/\{(\d)\}/g, (_, i) => values[+i]);
}

if (typeof module !== 'undefined') module.exports = { DGX_UI_LANGUAGES, dgxUiTranslate };
