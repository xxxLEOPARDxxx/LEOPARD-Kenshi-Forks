// Included after KillStats/PendingKill globals. All receipt/stat mutations run
// on the existing world-update thread. Other callers publish bounded POD only.
#define KILLCOUNTER_BRIDGE_BUILD
#include "ExecutionBridge.h"
static_assert(sizeof(KillCounterExecutionV1)==48,"Execution bridge POD ABI");
static const size_t kDeathReceiptLimit=4096, kExecutionQueueLimit=64;
struct DeathReceipt
{
    Character* credited;
    Character* koCreditor;
    bool koUndone, execution;
    DeathReceipt():credited(NULL),koCreditor(NULL),koUndone(false),execution(false){}
};
static std::map<Character*,DeathReceipt> g_deathReceipts;
// At most two referenced actors per receipt. Unrelated NPC destruction does
// not scan receipts, and raw references are cleared before address reuse.
static std::map<Character*,unsigned> g_receiptReferences;
static bool g_receiptSaturated=false, g_bridgeLifecycleReady=false;
static volatile LONG g_bridgeWorldThread=0;
static SRWLOCK g_executionQueueLock=SRWLOCK_INIT;
struct QueuedExecution {KillCounterExecutionV1 packet;LONG generation;};
static QueuedExecution g_executionQueue[64];
static volatile LONG g_executionGeneration=0;
static unsigned g_executionQueueCount=0;

static void WriteExecutionIdentity(uint32_t* words,const hand& h)
{
    words[0]=h.type;words[1]=h.container;words[2]=h.containerSerial;
    words[3]=h.index;words[4]=h.serial;
}
static bool MatchesExecutionIdentity(const uint32_t* words,const hand& h)
{
    return words[0]==static_cast<uint32_t>(h.type) && words[1]==h.container &&
        words[2]==h.containerSerial && words[3]==h.index && words[4]==h.serial;
}
static Character* ResolveExecutionIdentity(const uint32_t* words)
{
    if(words[0]!=CHARACTER) return NULL;
    hand identity(words[3],words[4],CHARACTER,words[1],words[2]);
    Character* resolved=identity.getCharacter();
    return resolved && MatchesExecutionIdentity(words,resolved->getHandle())?resolved:NULL;
}
static void ReferenceReceiptActor(Character* actor)
{ if(actor) ++g_receiptReferences[actor]; }
static void UnreferenceReceiptActor(Character* actor)
{
    std::map<Character*,unsigned>::iterator found=g_receiptReferences.find(actor);
    if(found!=g_receiptReferences.end() && --found->second==0) g_receiptReferences.erase(found);
}
static void ForgetExecutionCharacter(Character* character)
{
    std::map<Character*,DeathReceipt>::iterator own=g_deathReceipts.find(character);
    if(own!=g_deathReceipts.end())
    {
        UnreferenceReceiptActor(own->second.credited);
        UnreferenceReceiptActor(own->second.koCreditor);
        g_deathReceipts.erase(own);
    }
    if(g_receiptReferences.erase(character)==0) return;
    for(std::map<Character*,DeathReceipt>::iterator it=g_deathReceipts.begin();it!=g_deathReceipts.end();++it)
    {
        if(it->second.credited==character) it->second.credited=NULL;
        if(it->second.koCreditor==character) it->second.koCreditor=NULL;
    }
}
static void ClearExecutionQueue()
{
    AcquireSRWLockExclusive(&g_executionQueueLock);
    g_executionQueueCount=0;
    ReleaseSRWLockExclusive(&g_executionQueueLock);
}
static void ResetExecutionBridge()
{
    InterlockedExchange(&g_bridgeWorldThread,0);
    AcquireSRWLockExclusive(&g_executionQueueLock);
    InterlockedIncrement(&g_executionGeneration);g_executionQueueCount=0;
    ReleaseSRWLockExclusive(&g_executionQueueLock);
    g_deathReceipts.clear();g_receiptReferences.clear();g_receiptSaturated=false;
}
static bool UndoDeathKo(Character* killer,Character* koCreditor,MedicalSystem* medical)
{
    if(killer && killer==koCreditor && medical && medical->isUnconcious())
    {
        KillStats& stats=g_stats[killer];
        if(stats.kos>0){--stats.kos;return true;}
    }
    return false;
}
static uint32_t CreditConfirmedDeath(Character* victim,const PendingKill& pending,bool execution)
{
    Character* killer=pending.attacker;
    MedicalSystem* medical=victim?victim->getMedical():NULL;
    if(!killer || !medical || !medical->isDead()) return 0;
    if(!g_bridgeLifecycleReady)
    {
        if(execution) return 0;
        UndoDeathKo(killer,pending.koCreditor,medical);++g_stats[killer].kills;
        return 1;
    }
    std::map<Character*,DeathReceipt>::iterator found=g_deathReceipts.find(victim);
    if(found!=g_deathReceipts.end())
    {
        DeathReceipt& receipt=found->second;
        if(!execution || receipt.execution) return 2;
        if(receipt.credited!=killer)
        {
            std::map<Character*,KillStats>::iterator old=g_stats.find(receipt.credited);
            if(old!=g_stats.end() && old->second.kills>0) --old->second.kills;
            if(receipt.koUndone && receipt.koCreditor) ++g_stats[receipt.koCreditor].kos;
            UnreferenceReceiptActor(receipt.credited);
            receipt.credited=killer;ReferenceReceiptActor(killer);
            ++g_stats[killer].kills;
            receipt.koUndone=UndoDeathKo(killer,receipt.koCreditor,medical);
        }
        receipt.execution=true;
        return 1;
    }
    // Do not evict a loaded victim: a delayed callback could otherwise count
    // it twice. Once saturated, unreceipted native deaths may exist, so new
    // bridge events fail closed until the next world reset even if slots free.
    const bool room=!g_receiptSaturated && g_deathReceipts.size()<kDeathReceiptLimit;
    if(!room && execution){g_receiptSaturated=true;return 0;}
    if(room)
    {
        DeathReceipt receipt;receipt.credited=killer;receipt.koCreditor=pending.koCreditor;
        receipt.execution=execution;
        receipt.koUndone=UndoDeathKo(killer,pending.koCreditor,medical);
        g_deathReceipts.insert(std::make_pair(victim,receipt));
        ReferenceReceiptActor(killer);ReferenceReceiptActor(receipt.koCreditor);
    }
    else
    {
        g_receiptSaturated=true;
        UndoDeathKo(killer,pending.koCreditor,medical);
    }
    ++g_stats[killer].kills;
    if(PluginDiagnostics::Enabled())
    {
        char note[160];sprintf_s(note,"KillCounter: kill credited attacker=%p total=%d",killer,g_stats[killer].kills);
        DebugLog(note);
    }
    return 1;
}
static void ReportExecutionValidationFault()
{
    static bool reported=false;
    if(!reported){reported=true;ErrorLog("KillCounter: native execution identity validation failed");}
}
static bool ValidateExecutionActors(const KillCounterExecutionV1& packet,Character*& actor,Character*& victim)
{
    __try
    {
        actor=ResolveExecutionIdentity(packet.actor);victim=ResolveExecutionIdentity(packet.victim);
        if(!actor || !victim || actor==victim || !actor->isPlayerCharacter()) return false;
        MedicalSystem* medical=victim->getMedical();
        return medical && medical->isDead();
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        actor=victim=NULL;
        ReportExecutionValidationFault();
        return false;
    }
}
static uint32_t ProcessExecutionPacket(const KillCounterExecutionV1& packet)
{
    if(!g_enabled || !g_bridgeLifecycleReady || packet.size!=sizeof(packet) || packet.version!=1) return 0;
    Character* actor=NULL;Character* victim=NULL;
    if(!ValidateExecutionActors(packet,actor,victim)) return 0;
    PendingKill pending;
    const hand victimHandle(victim);
    std::map<hand,PendingKill>::iterator old=g_pendingKills.find(victimHandle);
    if(old!=g_pendingKills.end()) pending=old->second;
    pending.attacker=actor;
    const uint32_t result=CreditConfirmedDeath(victim,pending,true);
    if(result && old!=g_pendingKills.end()) g_pendingKills.erase(old);
    return result;
}
static uint32_t SafeProcessExecutionPacket(const KillCounterExecutionV1& packet)
{
    try { return ProcessExecutionPacket(packet); }
    catch(...) { ErrorLog("KillCounter: execution attribution could not be recorded");return 0; }
}
static bool ReadExecutionPacket(const KillCounterExecutionV1* request,KillCounterExecutionV1& copy)
{
    __try
    {
        if(!request || request->size!=sizeof(copy) || request->version!=1) return false;
        std::memcpy(&copy,request,sizeof(copy));
        return copy.actor[0]==CHARACTER && copy.victim[0]==CHARACTER;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
extern "C" __declspec(dllexport) uint32_t __cdecl KillCounter_RecordExecutionV1(const KillCounterExecutionV1* request)
{
    const LONG generation=InterlockedCompareExchange(&g_executionGeneration,0,0);
    const DWORD worldThread=static_cast<DWORD>(InterlockedCompareExchange(&g_bridgeWorldThread,0,0));
    if(!worldThread) return 0;
    KillCounterExecutionV1 packet;
    if(!g_enabled || !g_bridgeLifecycleReady || !ReadExecutionPacket(request,packet)) return 0;
    // Reading a foreign packet can fault/re-enter loading. The same thread
    // identifier and saved identities may exist again in a later world.
    if(generation!=InterlockedCompareExchange(&g_executionGeneration,0,0) ||
       worldThread!=static_cast<DWORD>(InterlockedCompareExchange(&g_bridgeWorldThread,0,0))) return 0;
    if(worldThread==GetCurrentThreadId())
        return SafeProcessExecutionPacket(packet);
    uint32_t result=0;
    AcquireSRWLockExclusive(&g_executionQueueLock);
    if(generation==g_executionGeneration && g_bridgeWorldThread!=0)
    {
        for(unsigned i=0;i<g_executionQueueCount;++i)
            if(std::memcmp(&g_executionQueue[i].packet,&packet,sizeof(packet))==0){result=2;break;}
        if(!result && g_executionQueueCount<kExecutionQueueLimit)
        {
            QueuedExecution& queued=g_executionQueue[g_executionQueueCount++];
            queued.packet=packet;queued.generation=generation;result=1;
        }
    }
    ReleaseSRWLockExclusive(&g_executionQueueLock);
    return result;
}
static void DrainExecutionQueue()
{
    QueuedExecution packets[64];unsigned count;
    AcquireSRWLockExclusive(&g_executionQueueLock);
    count=g_executionQueueCount;
    for(unsigned i=0;i<count;++i) packets[i]=g_executionQueue[i];
    g_executionQueueCount=0;
    ReleaseSRWLockExclusive(&g_executionQueueLock);
    for(unsigned i=0;i<count;++i)
        if(packets[i].generation==InterlockedCompareExchange(&g_executionGeneration,0,0))
            SafeProcessExecutionPacket(packets[i].packet);
}
