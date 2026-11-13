// Optional KillCounter ABI v1. This fixed POD mirror deliberately introduces
// no build/load dependency on another plugin and passes no STL or raw actors.
struct LseKillCounterExecutionV1
{
    uint32_t size, version;
    uint32_t actor[5], victim[5];
};
static_assert(sizeof(LseKillCounterExecutionV1) == 48, "KillCounter execution event ABI");
typedef uint32_t (__cdecl *KillCounterExecutionFn)(const LseKillCounterExecutionV1*);

static uint32_t CallExecutionCounterSafe(KillCounterExecutionFn callback,
                                         const LseKillCounterExecutionV1* event)
{
    __try { return callback(event); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Loot-Scoot-Execute: KillCounter execution callback failed");
        return 0;
    }
}

static void NotifyExecutionKillCounter(Character* actor, Character* target)
{
    bool dead = false;
    if (!actor || !target || !TryIsCharacterDead(target, &dead) || !dead)
        return;
    // Resolve only at a confirmed death, never per frame. No DLL/function
    // pointer is retained across another module's lifetime.
    HMODULE module = GetModuleHandleW(L"KillCounter.dll");
    KillCounterExecutionFn callback = module ? reinterpret_cast<KillCounterExecutionFn>(
        GetProcAddress(module, "KillCounter_RecordExecutionV1")) : 0;
    if (!callback)
        return;
    const hand& attacker = actor->getHandle();
    const hand& victim = target->getHandle();
    LseKillCounterExecutionV1 event = {
        sizeof(LseKillCounterExecutionV1), 1,
        {static_cast<uint32_t>(attacker.type), attacker.container, attacker.containerSerial,
         attacker.index, attacker.serial},
        {static_cast<uint32_t>(victim.type), victim.container, victim.containerSerial,
         victim.index, victim.serial}
    };
    const uint32_t result = CallExecutionCounterSafe(callback, &event);
    if (ShouldLogExecuteDebug())
    {
        std::stringstream message;
        message << "Loot-Scoot-Execute: execution counter result=" << result
                << " actor=" << attacker.index << "/" << attacker.serial
                << " target=" << victim.index << "/" << victim.serial;
        PluginLog(message.str().c_str());
    }
}

static void RegisterExecutionCrime(Character* actor, Character* target, bool enemy)
{
    if (!actor || !target || enemy)
        return;
    // Register murder without inventing witnesses. The native duration setter
    // refreshes perception; it does not force a bounty/witnessed-crime event.
    const bool accepted = actor->crimes.setCrime(CRIME_MURDER, target->getFaction(), target->getHandle());
    if (accepted && (!_finite(actor->crimes.crimeExpiry) || actor->crimes.crimeExpiry < 20.0f))
        actor->crimes.notifyPossibleCrimeWitnessed(20.0f);
    if (ShouldLogExecuteDebug())
        PluginLog(accepted ? "Loot-Scoot-Execute: execution murder registered, minimum window 20 seconds"
                           : "Loot-Scoot-Execute: native murder registration declined");
}
