// keo_tr.h - LEOPARD fork: the KEO tab's text in the game's language.
// Strings stay English in the code and are the keys; the translation is
// locale/<lang>/LC_MESSAGES/keo.mo beside KEO.dll, read by the shared
// Localization.h. Native locale access is admitted on the UI thread only.
#pragma once
// Game-free interface. The native implementation alone owns the localization
// include; host policy/layout tests link an explicit identity implementation.
// NULL and empty keys return "", rather than the gettext catalog header.
const char* KeoTr(const char* english);
// Main/UI thread: native localization is admitted only after the build gate.
void KeoSetTranslationReady(bool ready);
void KeoSetLocalizationDetailed(bool enabled);
