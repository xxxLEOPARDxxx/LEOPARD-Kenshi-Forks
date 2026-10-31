#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include "../TauntPolicy.h"
#include "../BonusTransaction.h"
#include "../PatchSignature.h"

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
using namespace TauntExpansion;
static BonusTransaction transaction;
static float active = 2.0f;
static int calls = 0;
static bool Seen(void*) { CHECK(active == 10.0f); ++calls; return true; }
static bool Inner(void*) { CHECK(active == 20.0f); ++calls; return true; }
static bool Nested(void*) {
    CHECK(active == 10.0f);
    CHECK(transaction.Invoke(20.0f, Inner, NULL));
    CHECK(active == 10.0f);
    return true;
}
static bool NestedRetarget(void*) {
    CHECK(active == 20.0f);
    CHECK(transaction.Invoke(10.0f, Seen, NULL));
    CHECK(active == 20.0f);
    return true;
}
static bool ThrowSEH(void*) { RaiseException(0xE0424242, 0, 0, NULL); return false; }
static bool ThrowCpp(void*) { throw 7; }
static void CheckSEH() {
    __try { transaction.Invoke(90.0f, ThrowSEH, NULL); CHECK(false); }
    __except (GetExceptionCode() == 0xE0424242 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {}
}
static bool Concurrent(void* p) {
    float expected = (float)(size_t)p;
    CHECK(active == expected);
    SwitchToThread();
    CHECK(active == expected);
    return true;
}
static DWORD WINAPI ConcurrentWorker(void* p) {
    for (int i=0;i<5000;++i) transaction.Invoke((float)(size_t)p, Concurrent, p);
    return 0;
}
int main() {
    unsigned char pattern[]={0x80,0xB8,0x2A,0x01,0,0,0,0x74,0x10,0xF3,0x0F,0x58,0x3D,0,0,0,0};
    CHECK(FindTauntInstruction(pattern, sizeof(pattern)) == pattern+9);
    CHECK(FindTauntInstruction(pattern, 16) == NULL);
    unsigned char duplicate[34];memcpy(duplicate,pattern,17);memcpy(duplicate+17,pattern,17);
    CHECK(FindTauntInstruction(duplicate,sizeof(duplicate)) == NULL);
    pattern[7]=0x75;CHECK(FindTauntInstruction(pattern,sizeof(pattern)) == NULL);
    Config c;
    CHECK(c.enabled && !c.debug && !c.logCandidates);
    CHECK(c.tauntMultiplier == 5.0f && c.retargetMultiplier == 2.0f);
    CHECK(EffectiveBonus(2.0f, c, false) == 10.0f);
    CHECK(EffectiveBonus(2.0f, c, true) == 20.0f);
    c.enabled = false;
    CHECK(EffectiveBonus(2.0f, c, true) == 2.0f);
    c.enabled = true; c.tauntMultiplier = 0;
    CHECK(EffectiveBonus(2.0f, c, true) == 0);
    CHECK(ParseNumber("nan", 5, 0, 100) == 5);
    CHECK(ParseNumber("inf", 5, 0, 100) == 5);
    CHECK(ParseNumber("1e309", 5, 0, 100) == 5);
    CHECK(ParseNumber("9junk", 5, 0, 100) == 5);
    CHECK(ParseNumber(" 2.5 ", 5, 0, 100) == 2.5f);
    CHECK(ParseNumber("-9", 5, 0, 100) == 0);
    CHECK(ParseNumber("999", 5, 0, 100) == 100);
    ObservationCache cache;
    CHECK(cache.ShouldLog(1, 2, 1000, 500, true));
    CHECK(!cache.ShouldLog(1, 2, 1600, 500, true));
    CHECK(!cache.ShouldLog(1, 3, 1200, 500, true));
    CHECK(cache.ShouldLog(1, 3, 1600, 500, true));
    // No stale pointer dereferences; fixed capacity across arbitrary world churn.
    for (size_t i=1;i<100000;++i) cache.ShouldLog(i, i+1, i+30000, 0, false);
    CHECK(cache.Capacity() == 256);
    cache.Clear();
    CHECK(cache.ShouldLog(1, 0, 0, 500, true));
    transaction.Initialise(&active);
    CHECK(transaction.Invoke(10.0f, Seen, NULL)); CHECK(active == 2.0f);
    CHECK(transaction.Invoke(10.0f, Nested, NULL)); CHECK(active == 2.0f && calls == 2);
    CHECK(transaction.Invoke(20.0f, NestedRetarget, NULL));CHECK(active==2.0f && calls==3);
    CheckSEH(); CHECK(active == 2.0f);
    try { transaction.Invoke(90.0f,ThrowCpp,NULL);CHECK(false); }
    catch (int value) { CHECK(value==7); }
    CHECK(active==2.0f);
    HANDLE threads[2] = {CreateThread(NULL,0,ConcurrentWorker,(void*)10,0,NULL), CreateThread(NULL,0,ConcurrentWorker,(void*)20,0,NULL)};
    CHECK(threads[0] && threads[1]);
    CHECK(WaitForMultipleObjects(2,threads,TRUE,20000) == WAIT_OBJECT_0);
    CloseHandle(threads[0]); CloseHandle(threads[1]);
    CHECK(active == 2.0f);
    transaction.Dispose();
    puts("PASS: defaults, bonus modes, finite INI parsing, bounded diagnostics, nested/concurrent/SEH restoration");
    return 0;
}
