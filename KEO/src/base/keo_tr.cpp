#include "base/keo_tr.h"
#include "base/klib_include.h"
#define KLOC_DOMAIN "keo"
#include "base/Localization.h"
#include "base/klib_include_end.h"

bool IsMainThread();
bool DetailedLogEnabled();
static bool translationReady = false;

void KeoSetTranslationReady(bool ready)
{
	translationReady = ready;
	if (IsMainThread()) KLocLog::SetEnabled(ready && DetailedLogEnabled());
}
void KeoSetLocalizationDetailed(bool enabled)
{
	if (IsMainThread()) KLocLog::SetEnabled(translationReady && enabled);
}

const char* KeoTr(const char* english)
{
	if (!english || !*english)
		return english ? english : "";
	if (!IsMainThread() || !translationReady) return english;
	return Tr(english);
}
