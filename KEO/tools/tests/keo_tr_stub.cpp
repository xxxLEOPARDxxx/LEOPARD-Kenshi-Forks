// Host tests verify layout/config policy in English, not native catalog loading.
#include "base/keo_tr.h"
void KeoSetTranslationReady(bool) {}
void KeoSetLocalizationDetailed(bool) {}
const char* KeoTr(const char* english)
{
	return english ? english : "";
}
