// Native saved jobs. The native GOAP chooser owns job priority; only a selected
// Task_Blank action can request the existing main-thread execution queue.
namespace lse_jobs_runtime
{
using namespace lse_jobs;
static SRWLOCK lock = SRWLOCK_INIT;
static Ledger ledger;
static volatile LONG ready = 0, live = 0;
static volatile LONG requestCount = 0;
static volatile LONG ledgerCount = 0;
static bool registrationAttempted = false;
static TaskData* metadata[2] = {0, 0};
typedef const TaskData* (*GetDataFn)(TaskType);
typedef TaskData* (*RegisterDataFn)(
    TaskType, float (AI::*)(const hand&, const Ogre::Vector3&), bool, bool, const std::string&, unsigned);
static GetDataFn getData = 0;
static RegisterDataFn registerData = 0;
struct ActorSlot
{
    bool used;
    Identity id;
    Character* pointer;
    Identity offer[2];
    bool offered[2];
    DWORD expires[2];
    Tasker* request;
    int requestMode;
    Identity requestTarget;
    ActorSlot() : used(false), pointer(0), request(0), requestMode(0)
    {
        offered[0] = offered[1] = false;
        expires[0] = expires[1] = 0;
    }
};
static ActorSlot actors[ACTOR_CAP];
static unsigned scanCursor = 0;
static DWORD lastScan = 0;
// Kenshi's lektor does not free its buffer on destruction. Retain one bounded
// scratch buffer for the plugin lifetime, rather than allocating on each scan.
static lektor<RootObject*> scanScratch;
static bool ticking = false;
// Fixed-size counters only in hooks. Formatting and output stay on Tick's
// thread, outside the actor lock, and stop completely with detailed logging.
enum DebugCounter
{
    DBG_TICK, DBG_LIVE, DBG_OBSERVED, DBG_SLOT_FULL, DBG_PRUNED, DBG_READY,
    DBG_NO_STATE, DBG_NO_ACTOR, DBG_NOT_PLAYER, DBG_CANNOT_ACT, DBG_RAGDOLL,
    DBG_COMBAT, DBG_NO_ORDERS, DBG_JOBS_OFF, DBG_MANUAL, DBG_NO_ROW, DBG_MODE_OFF,
    DBG_POSITION_FAILED, DBG_QUERY, DBG_QUERY_FAILED, DBG_CANDIDATES,
    DBG_PREDICATE_REJECTED, DBG_POLICY_REJECTED, DBG_ALLOWED, DBG_OFFERS, DBG_SCORE,
    DBG_SCORE_AVAILABLE, DBG_SCORE_BLOCKED, DBG_BLANK, DBG_BLANK_ACCEPTED,
    DBG_BLANK_BLOCKED, DBG_BLANK_NO_OFFER, DBG_REQUEST, DBG_DISPATCH,
    DBG_REJECT_STALE, DBG_REJECT_BUSY, DBG_REJECT_NOT_READY, DBG_REJECT_TARGET,
    DBG_REJECT_CLAIMED, DBG_REJECT_QUEUE, DBG_COUNT
};
static volatile LONG debugCounters[DBG_COUNT] = {0};
static DWORD debugSummaryStamp = 0;
static bool debugSummaryActive = false;
static void DebugCount(DebugCounter counter, LONG amount = 1)
{
    if (ShouldLogExecuteDebug())
    {
        InterlockedExchangeAdd(&debugCounters[counter], amount);
    }
}
static void DebugSummary(DWORD now, bool paused)
{
    if (!ShouldLogExecuteDebug())
    {
        debugSummaryActive = false;
        return;
    }
    if (!debugSummaryActive)
    {
        debugSummaryActive = true;
        debugSummaryStamp = now;
        for (unsigned index = 0; index < DBG_COUNT; ++index)
        {
            InterlockedExchange(&debugCounters[index], 0);
        }
        return;
    }
    if (!DebounceWindowElapsed(now, debugSummaryStamp, 2000))
    {
        return;
    }
    const DWORD elapsed = now - debugSummaryStamp;
    debugSummaryStamp = now;
    LONG counts[DBG_COUNT];
    for (unsigned index = 0; index < DBG_COUNT; ++index)
    {
        counts[index] = InterlockedExchange(&debugCounters[index], 0);
    }
    unsigned slots = 0, ownOffers = 0, enemyOffers = 0;
    AcquireSRWLockShared(&lock);
    for (unsigned index = 0; index < ACTOR_CAP; ++index)
    {
        slots += actors[index].used ? 1 : 0;
        ownOffers += actors[index].used && actors[index].offered[0] ? 1 : 0;
        enemyOffers += actors[index].used && actors[index].offered[1] ? 1 : 0;
    }
    ReleaseSRWLockShared(&lock);
    static const char* names[DBG_COUNT] = {
        "tick", "live_ticks", "observed", "slot_full", "pruned", "actor_ready",
        "no_state", "no_actor", "not_player", "cannot_act", "ragdoll",
        "combat", "no_orders", "jobs_off", "manual", "no_row", "mode_off",
        "position_failed", "query", "query_failed", "candidates",
        "predicate_rejected", "policy_rejected", "allowed", "offers", "score",
        "score_available", "score_blocked", "blank", "blank_accepted",
        "blank_blocked", "blank_no_offer", "request", "dispatch",
        "reject_stale", "reject_busy", "reject_not_ready", "reject_target",
        "reject_claimed", "reject_queue"};
    std::stringstream log;
    log << "Loot-Scoot-Execute DEBUG: permanent_job_scheduler elapsed_ms=" << elapsed
        << " ready=" << ready << " live=" << live << " paused=" << paused
        << " enabled=" << g_effectiveEnableExecuteAction << " slots=" << slots
        << " own_offers=" << ownOffers << " enemy_offers=" << enemyOffers;
    for (unsigned index = 0; index < DBG_COUNT; ++index)
    {
        log << " " << names[index] << "=" << counts[index];
    }
    PluginLog(log.str().c_str());
}
static const std::string attributionKey("LSE_confirmed_defeat_by");
static Identity Id(const hand& handle)
{
    Identity identity = {{static_cast<uint32_t>(handle.type),
                          handle.container,
                          handle.containerSerial,
                          handle.index,
                          handle.serial}};
    return identity;
}
static hand Handle(const Identity& identity)
{
    return hand(identity.word[3],
                identity.word[4],
                static_cast<itemType>(identity.word[0]),
                identity.word[1],
                identity.word[2]);
}
static int ModeIndex(int mode)
{
    return mode == OWN_DEFEATS ? 0 : 1;
}
static bool Ours(const Tasker* task)
{
    return task && task->taskData &&
           ((metadata[0] && task->taskData == metadata[0] && task->taskData->key == OWN_DEFEATS) ||
            (metadata[1] && task->taskData == metadata[1] && task->taskData->key == ALL_ENEMIES));
}
static int Type(const Tasker* task)
{
    return Ours(task) ? static_cast<int>(task->taskData->key) : 0;
}
static int Find(const Identity& identity)
{
    for (unsigned slotIndex = 0; slotIndex < ACTOR_CAP; ++slotIndex)
    {
        if (actors[slotIndex].used && Same(actors[slotIndex].id, identity))
        {
            return slotIndex;
        }
    }
    return -1;
}
static bool HasRow(Character* character, int mode)
{
    OrdersReceiver* orders = character ? character->getOrdersReciever() : 0;
    if (!orders || orders->permajobs.count > 256 || orders->permajobs.count > orders->permajobs.maxSize ||
        (orders->permajobs.count && !orders->permajobs.stuff))
    {
        return false;
    }
    for (unsigned jobIndex = 0; jobIndex < orders->permajobs.count; ++jobIndex)
    {
        if (Type(orders->permajobs.stuff[jobIndex]) == mode)
        {
            return true;
        }
    }
    return false;
}
static bool ObserveActor(Character* character)
{
    if (!character || !character->isPlayerCharacter())
    {
        return false;
    }
    const Identity identity = Id(character->getHandle());
    AcquireSRWLockExclusive(&lock);
    int actorIndex = Find(identity);
    if (actorIndex < 0)
    {
        for (unsigned slotIndex = 0; slotIndex < ACTOR_CAP; ++slotIndex)
        {
            if (!actors[slotIndex].used)
            {
                actorIndex = slotIndex;
                break;
            }
        }
    }
    if (actorIndex >= 0)
    {
        actors[actorIndex].used = true;
        actors[actorIndex].id = identity;
        actors[actorIndex].pointer = character;
    }
    ReleaseSRWLockExclusive(&lock);
    DebugCount(actorIndex >= 0 ? DBG_OBSERVED : DBG_SLOT_FULL);
    return actorIndex >= 0;
}
static bool Combat(Character* character)
{
    return character->isInCombatMode(true, true) || character->isLiterallyUnderMeleeAttackRightNowForSure() ||
           character->getAllAttackersCount() > 0;
}
static bool ActorReady(Character* character, int mode)
{
    OrdersReceiver* orders = character ? character->getOrdersReciever() : 0;
    if (ShouldLogExecuteDebug())
    {
        // Detailed mode reports all admission flags, not just the first false
        // operand. These native queries never run under our publication lock.
        const bool state = ready && live;
        const bool player = character && character->isPlayerCharacter();
        const bool canAct = character && CanActOnExecuteOrder(character);
        const bool ragdoll = character && character->isRagdoll();
        const bool combat = character && Combat(character);
        const bool jobs = orders && orders->isJobsEnabled();
        const bool manual = orders && orders->hasPlayerOrders();
        const bool row = character && HasRow(character, mode);
        const bool enabled = mode != ALL_ENEMIES || g_config.enableExecuteAll;
        if (!state) DebugCount(DBG_NO_STATE);
        if (!character) DebugCount(DBG_NO_ACTOR);
        if (character && !player) DebugCount(DBG_NOT_PLAYER);
        if (character && !canAct) DebugCount(DBG_CANNOT_ACT);
        if (ragdoll) DebugCount(DBG_RAGDOLL);
        if (combat) DebugCount(DBG_COMBAT);
        if (!orders) DebugCount(DBG_NO_ORDERS);
        if (orders && !jobs) DebugCount(DBG_JOBS_OFF);
        if (manual) DebugCount(DBG_MANUAL);
        if (character && !row) DebugCount(DBG_NO_ROW);
        if (!enabled) DebugCount(DBG_MODE_OFF);
        const bool accepted = state && player && canAct && !ragdoll && !combat &&
                              orders && jobs && !manual && row && enabled;
        if (accepted) DebugCount(DBG_READY);
        return accepted;
    }
    return ready && live && character && character->isPlayerCharacter() && CanActOnExecuteOrder(character) &&
           !character->isRagdoll() && !Combat(character) && orders && orders->isJobsEnabled() &&
           !orders->hasPlayerOrders() && HasRow(character, mode) &&
           (mode != ALL_ENEMIES || g_config.enableExecuteAll);
}
static bool Owns(Character* character, Character* target)
{
    const Identity victimIdentity = Id(target->getHandle()), actorIdentity = Id(character->getHandle());
    const bool down = target->isUnconcious(), dead = target->isDead();
    AcquireSRWLockExclusive(&lock);
    ledger.Medical(victimIdentity, down, dead, GetTickCount());
    InterlockedExchange(&ledgerCount, ledger.Count());
    const bool result = ledger.Owns(victimIdentity, actorIdentity);
    ReleaseSRWLockExclusive(&lock);
    return result;
}
static bool Allowed(Character* character, Character* target, int mode)
{
    CanExecuteDiagnostics diagnostics = MakeCanExecuteDiagnostics();
    if (!target)
    {
        return false;
    }
    const bool credited = mode == OWN_DEFEATS && Owns(character, target);
    if (!CanExecuteFromNativeMenuSelection(character, target, &diagnostics, false))
    {
        DebugCount(DBG_PREDICATE_REJECTED);
        return false;
    }
    const bool result = TargetAllowed(mode, true, diagnostics.targetIsEnemy, credited);
    if (!result) DebugCount(DBG_POLICY_REJECTED);
    return result;
}
static bool Claimed(Character* character, Character* target)
{
    const Identity targetIdentity = Id(target->getHandle());
    for (unsigned jobIndex = 0; jobIndex < kExecuteJobMax; ++jobIndex)
    {
        const ExecuteJob& job = g_executeJobs[jobIndex];
        if (job.queuedActive && job.actorPtr != reinterpret_cast<uintptr_t>(character) &&
            Same(Id(job.targetHandle), targetIdentity))
        {
            return true;
        }
    }
    return false;
}
static void Reset()
{
    InterlockedExchange(&live, 0);
    AcquireSRWLockExclusive(&lock);
    ledger.Clear();
    for (unsigned slotIndex = 0; slotIndex < ACTOR_CAP; ++slotIndex)
    {
        actors[slotIndex] = ActorSlot();
    }
    InterlockedExchange(&requestCount, 0);
    InterlockedExchange(&ledgerCount, 0);
    lastScan = 0;
    scanCursor = 0;
    ReleaseSRWLockExclusive(&lock);
}
static bool AutomaticQueueValid(Character* character, Character* target, int mode)
{
    // A queue owns a temporary MOVE order, so hasPlayerOrders() is deliberately
    // checked only at native job admission, not while that owned move runs.
    OrdersReceiver* orders = character ? character->getOrdersReciever() : 0;
    return ready && live && character && target && CanActOnExecuteOrder(character) && !Combat(character) &&
           orders && orders->isJobsEnabled() && HasRow(character, mode) &&
           (mode != ALL_ENEMIES || g_config.enableExecuteAll) && Allowed(character, target, mode);
}
static void (*blankActionOriginal)(Tasker*, CharBody*) = 0;
static void BlankAction(Tasker* task, CharBody* body)
{
    if (!Ours(task))
    {
        blankActionOriginal(task, body);
        return;
    }
    DebugCount(DBG_BLANK);
    Character* character = body ? body->character : 0;
    if (!ready || !live || !character || body->currentAction != task || Combat(character))
    {
        DebugCount(DBG_BLANK_BLOCKED);
        if (body && body->currentAction == task)
        {
            body->notifyBodyTaskComplete();
        }
        return;
    }
    const Identity identity = Id(character->getHandle());
    const int mode = Type(task), modeIndex = ModeIndex(mode);
    const DWORD now = GetTickCount();
    bool accepted = false;
    AcquireSRWLockExclusive(&lock);
    const int actorIndex = Find(identity);
    if (actorIndex >= 0 && actors[actorIndex].pointer == character && actors[actorIndex].offered[modeIndex] &&
        static_cast<LONG>(actors[actorIndex].expires[modeIndex] - now) > 0)
    {
        if (!actors[actorIndex].request)
        {
            InterlockedIncrement(&requestCount);
        }
        actors[actorIndex].request = task;
        actors[actorIndex].requestMode = mode;
        actors[actorIndex].requestTarget = actors[actorIndex].offer[modeIndex];
        accepted = true;
    }
    ReleaseSRWLockExclusive(&lock);
    DebugCount(accepted ? DBG_BLANK_ACCEPTED : DBG_BLANK_NO_OFFER);
    if (!accepted)
    {
        body->notifyBodyTaskComplete();
    }
}
static float (*scoreOriginal)(Tasker*, AI*) = 0;
static float Score(Tasker* task, AI* ai)
{
    if (!Ours(task))
    {
        return scoreOriginal(task, ai);
    }
    DebugCount(DBG_SCORE);
    Character* character = ai ? ai->getCharacter() : 0;
    if (!ready || !live || !character || Combat(character))
    {
        DebugCount(DBG_SCORE_BLOCKED);
        return 0.0f;
    }
    const Identity identity = Id(character->getHandle());
    const int modeIndex = ModeIndex(Type(task));
    const DWORD now = GetTickCount();
    bool available = false;
    AcquireSRWLockShared(&lock);
    const int actorIndex = Find(identity);
    available = actorIndex >= 0 && actors[actorIndex].pointer == character &&
                actors[actorIndex].offered[modeIndex] &&
                static_cast<LONG>(actors[actorIndex].expires[modeIndex] - now) > 0;
    ReleaseSRWLockShared(&lock);
    if (available) DebugCount(DBG_SCORE_AVAILABLE);
    // Keep the native score/weight and permanent row ordering intact.
    return available ? scoreOriginal(task, ai) : 0.0f;
}
static void Tick()
{
    if (ticking)
    {
        return;
    }
    struct TickGuard
    {
        TickGuard()
        {
            ticking = true;
        }
        ~TickGuard()
        {
            ticking = false;
        }
    } guard;
    const DWORD now = GetTickCount();
    const bool paused = !ou || ou->isPaused();
    InterlockedExchange(&live, ready && g_effectiveEnableExecuteAction && !paused ? 1 : 0);
    DebugCount(DBG_TICK);
    if (live) DebugCount(DBG_LIVE);
    DebugSummary(now, paused);
    if (!ready)
    {
        return;
    }
    // Consume only native selected-action requests. No world query here.
    struct Request
    {
        Tasker* task;
        int mode;
        Identity actor, target;
    };
    Request requests[ACTOR_CAP];
    unsigned requestTotal = 0;
    if (requestCount)
    {
        AcquireSRWLockExclusive(&lock);
        for (unsigned slotIndex = 0; slotIndex < ACTOR_CAP; ++slotIndex)
        {
            ActorSlot& slot = actors[slotIndex];
            if (slot.used && slot.request)
            {
                Request& pendingRequest = requests[requestTotal++];
                pendingRequest.task = slot.request;
                pendingRequest.mode = slot.requestMode;
                pendingRequest.actor = slot.id;
                pendingRequest.target = slot.requestTarget;
                slot.request = 0;
                slot.offered[0] = slot.offered[1] = false;
            }
        }
        InterlockedExchange(&requestCount, 0);
        ReleaseSRWLockExclusive(&lock);
    }
    for (unsigned slotIndex = 0; slotIndex < requestTotal; ++slotIndex)
    {
        const Request& request = requests[slotIndex];
        DebugCount(DBG_REQUEST);
        const int mode = request.mode;
        Character* character = Handle(request.actor).getCharacter();
        Character* victim = Handle(request.target).getCharacter();
        CharBody* body = character ? character->getBody() : 0;
        if (!character || !body || body->currentAction != request.task || !Ours(body->currentAction) ||
            Type(body->currentAction) != mode)
        {
            DebugCount(DBG_REJECT_STALE);
            continue;
        }
        bool busy = false;
        for (unsigned jobIndex = 0; jobIndex < kExecuteJobMax; ++jobIndex)
        {
            if (g_executeJobs[jobIndex].queuedActive &&
                g_executeJobs[jobIndex].actorPtr == reinterpret_cast<uintptr_t>(character))
            {
                busy = true;
            }
        }
        DebugCounter rejection = DBG_COUNT;
        if (busy) rejection = DBG_REJECT_BUSY;
        else if (!ActorReady(character, mode)) rejection = DBG_REJECT_NOT_READY;
        else if (!Allowed(character, victim, mode)) rejection = DBG_REJECT_TARGET;
        else if (Claimed(character, victim)) rejection = DBG_REJECT_CLAIMED;
        else if (!QueueExecuteFromNativeMenuSelection(character, victim, ShouldLogExecuteDebug()))
            rejection = DBG_REJECT_QUEUE;
        if (rejection == DBG_COUNT)
        {
            DebugCount(DBG_DISPATCH);
            g_executeJobs[g_executeJobIndex].permanentMode = mode;
            if (ShouldLogExecuteDebug())
            {
                std::stringstream log;
                log << "Loot-Scoot-Execute: permanent_job_dispatch mode=" << mode
                    << " actor=" << request.actor.word[3] << "/" << request.actor.word[4]
                    << " target=" << request.target.word[3] << "/" << request.target.word[4];
                PluginLog(log.str().c_str());
            }
        }
        else
        {
            DebugCount(rejection);
            if (body->currentAction == request.task)
            {
                body->notifyBodyTaskComplete();
            }
        }
    }
    if (!live || !DebounceWindowElapsed(now, lastScan, 250))
    {
        return;
    }
    lastScan = now;
    int selectedSlotIndex = -1;
    Identity actor;
    AcquireSRWLockShared(&lock);
    for (unsigned scanOffset = 0; scanOffset < ACTOR_CAP; ++scanOffset)
    {
        const unsigned slotIndex = (scanCursor + scanOffset) % ACTOR_CAP;
        if (actors[slotIndex].used)
        {
            selectedSlotIndex = slotIndex;
            actor = actors[slotIndex].id;
            scanCursor = (slotIndex + 1) % ACTOR_CAP;
            break;
        }
    }
    ReleaseSRWLockShared(&lock);
    if (selectedSlotIndex < 0)
    {
        return;
    }
    Character* character = Handle(actor).getCharacter();
    if (!character || (!HasRow(character, OWN_DEFEATS) && !HasRow(character, ALL_ENEMIES)))
    {
        DebugCount(DBG_PRUNED);
        AcquireSRWLockExclusive(&lock);
        if (actors[selectedSlotIndex].request)
        {
            InterlockedDecrement(&requestCount);
        }
        actors[selectedSlotIndex] = ActorSlot();
        ReleaseSRWLockExclusive(&lock);
        return;
    }
    bool modes[2] = {ActorReady(character, OWN_DEFEATS), ActorReady(character, ALL_ENEMIES)};
    Identity best[2];
    bool found[2] = {false, false};
    float distances[2] = {FLT_MAX, FLT_MAX};
    if (modes[0] || modes[1])
    {
        Ogre::Vector3 actorPosition;
        if (TryReadRootObjectPosition(character, &actorPosition))
        {
            DebugCount(DBG_QUERY);
            const bool querySucceeded = TryGetCharactersWithinSphere(&scanScratch,
                                                                     actorPosition,
                                                                     ComputeExecuteAllRadiusUnits(),
                                                                     character,
                                                                     kExecuteAllWorldQueryMaxTargets);
            if (!querySucceeded) DebugCount(DBG_QUERY_FAILED);
            for (unsigned scanOffset = 0;
                 scanOffset < scanScratch.size() && scanOffset < kExecuteAllWorldQueryMaxTargets;
                 ++scanOffset)
            {
                DebugCount(DBG_CANDIDATES);
                RootObject* root = scanScratch[scanOffset];
                itemType type;
                if (!root || !TryGetRootObjectTypeForExecutePredicate(root, &type) ||
                    !IsCharacterDataType(type))
                {
                    continue;
                }
                Character* candidate = static_cast<Character*>(root);
                Ogre::Vector3 candidatePosition;
                if (Claimed(character, candidate) ||
                    !TryReadRootObjectPosition(candidate, &candidatePosition))
                {
                    continue;
                }
                const float distanceSquared = (candidatePosition - actorPosition).squaredLength();
                if (!_finite(distanceSquared))
                {
                    continue;
                }
                for (int modeIndex = 0; modeIndex < 2; ++modeIndex)
                {
                    if (modes[modeIndex] && distanceSquared < distances[modeIndex] &&
                        Allowed(character, candidate, modeIndex ? ALL_ENEMIES : OWN_DEFEATS))
                    {
                        DebugCount(DBG_ALLOWED);
                        best[modeIndex] = Id(candidate->getHandle());
                        found[modeIndex] = true;
                        distances[modeIndex] = distanceSquared;
                    }
                }
            }
            scanScratch.clear();
        }
        else
        {
            DebugCount(DBG_POSITION_FAILED);
        }
    }
    bool changed = false;
    bool modeChanged[2] = {false, false};
    bool previouslyOffered[2] = {false, false};
    AcquireSRWLockExclusive(&lock);
    ActorSlot& slot = actors[selectedSlotIndex];
    for (int modeIndex = 0; modeIndex < 2; ++modeIndex)
    {
        previouslyOffered[modeIndex] = slot.offered[modeIndex];
        modeChanged[modeIndex] = slot.offered[modeIndex] != found[modeIndex] ||
                                (found[modeIndex] && !Same(slot.offer[modeIndex], best[modeIndex]));
        changed = changed || modeChanged[modeIndex];
        slot.offered[modeIndex] = found[modeIndex];
        if (found[modeIndex])
        {
            slot.offer[modeIndex] = best[modeIndex];
        }
        slot.expires[modeIndex] = now + 20000;
    }
    ReleaseSRWLockExclusive(&lock);
    if (ShouldLogExecuteDebug())
    {
        for (int modeIndex = 0; modeIndex < 2; ++modeIndex)
        {
            if (found[modeIndex]) DebugCount(DBG_OFFERS);
            if (modeChanged[modeIndex])
            {
                std::stringstream log;
                log << "Loot-Scoot-Execute DEBUG: permanent_job_offer mode="
                    << (modeIndex ? ALL_ENEMIES : OWN_DEFEATS)
                    << " actor=" << actor.word[3] << "/" << actor.word[4]
                    << " previous=" << previouslyOffered[modeIndex] << " available=" << found[modeIndex];
                if (found[modeIndex])
                {
                    log << " target=" << best[modeIndex].word[3] << "/" << best[modeIndex].word[4]
                        << " distance_sq=" << distances[modeIndex];
                }
                PluginLog(log.str().c_str());
            }
        }
    }
    if (changed)
    {
        OrdersReceiver* orders = character->getOrdersReciever();
        orders->setNeedGOAP();
        character->getAI()->getTaskSystem()->callForUpdate(false);
    }
}

// Native damage and medical transitions are synchronous on the caller's
// thread. This context never attributes a UI chance query or an already KO'd
// target. A successful stealth knockout may apply collapse on the next update.
__declspec(thread) static Character* damageActor = 0;
__declspec(thread) static Character* damageVictim = 0;
struct DamageContext
{
    Character* oldActor;
    Character* oldVictim;
    DamageContext(Character* actor, Character* victim) : oldActor(damageActor), oldVictim(damageVictim)
    {
        damageActor = actor;
        damageVictim = victim;
    }
    ~DamageContext()
    {
        damageActor = oldActor;
        damageVictim = oldVictim;
    }
};
static bool PlayerAttacker(Character* character)
{
    return ready && g_config.enabled && character && character->isPlayerCharacter();
}
static void HitTransition(Character* victim, Character* attacker, bool wasDown)
{
    if (!PlayerAttacker(attacker) || !victim || victim == attacker)
    {
        return;
    }
    const Identity victimIdentity = Id(victim->getHandle()), attackerIdentity = Id(attacker->getHandle());
    const bool down = victim->isUnconcious(), dead = victim->isDead();
    AcquireSRWLockExclusive(&lock);
    ledger.Hit(victimIdentity, attackerIdentity, wasDown, down, dead, GetTickCount());
    InterlockedExchange(&ledgerCount, ledger.Count());
    ReleaseSRWLockExclusive(&lock);
}
static HitMaterialType (*meleeOriginal)(
    Character*, CutDirection, Damages&, Character*, CombatTechniqueData*, int) = 0;
static HitMaterialType Melee(Character* victim,
                             CutDirection direction,
                             Damages& damage,
                             Character* attacker,
                             CombatTechniqueData* attack,
                             int combo)
{
    if (!PlayerAttacker(attacker))
    {
        return meleeOriginal(victim, direction, damage, attacker, attack, combo);
    }
    const bool down = victim ? victim->isUnconcious() : true;
    DamageContext context(attacker, victim);
    const HitMaterialType result = meleeOriginal(victim, direction, damage, attacker, attack, combo);
    HitTransition(victim, attacker, down);
    return result;
}
static bool (*rangedOriginal)(Character*, Character*, Harpoon*, bool) = 0;
static bool Ranged(Character* victim, Character* attacker, Harpoon* harpoon, bool purpose)
{
    if (!PlayerAttacker(attacker))
    {
        return rangedOriginal(victim, attacker, harpoon, purpose);
    }
    const bool down = victim ? victim->isUnconcious() : true;
    DamageContext context(attacker, victim);
    const bool result = rangedOriginal(victim, attacker, harpoon, purpose);
    HitTransition(victim, attacker, down);
    return result;
}
static void (*stealthOriginal)(Tasker*, CharBody*) = 0;
static void Stealth(Tasker* task, CharBody* body)
{
    Character* attacker = body ? body->character : 0;
    if (!PlayerAttacker(attacker))
    {
        stealthOriginal(task, body);
        return;
    }
    Character* victim = task ? task->subject.getCharacter() : 0;
    DamageContext context(attacker, victim);
    stealthOriginal(task, body);
}
static void (*knockoutOriginal)(MedicalSystem*, float) = 0;
static void Knockout(MedicalSystem* medical, float skill)
{
    if (!damageActor || !PlayerAttacker(damageActor))
    {
        knockoutOriginal(medical, skill);
        return;
    }
    Character* victim = medical ? medical->me : 0;
    const bool wasUnconscious = medical ? medical->isUnconcious() : true;
    knockoutOriginal(medical, skill);
    if (victim && !wasUnconscious && damageVictim == victim && PlayerAttacker(damageActor) &&
        !medical->isDead())
    {
        const Identity victimIdentity = Id(victim->getHandle());
        const Identity actorIdentity = Id(damageActor->getHandle());
        const bool pending = !medical->isUnconcious();
        AcquireSRWLockExclusive(&lock);
        ledger.Record(victimIdentity, actorIdentity, pending, GetTickCount());
        InterlockedExchange(&ledgerCount, ledger.Count());
        ReleaseSRWLockExclusive(&lock);
    }
}
static void (*medicalOriginal)(MedicalSystem*, float) = 0;
static void Medical(MedicalSystem* medical, float elapsed)
{
    medicalOriginal(medical, elapsed);
    if (!ready || !ledgerCount || !medical || !medical->me)
    {
        return;
    }
    const Identity identity = Id(medical->me->getHandle());
    AcquireSRWLockShared(&lock);
    const bool tracked = ledger.Find(identity) != 0;
    ReleaseSRWLockShared(&lock);
    if (!tracked)
    {
        return;
    }
    const bool down = medical->isUnconcious(), dead = medical->isDead();
    AcquireSRWLockExclusive(&lock);
    ledger.Medical(identity, down, dead, GetTickCount());
    InterlockedExchange(&ledgerCount, ledger.Count());
    ReleaseSRWLockExclusive(&lock);
}
typedef GameSaveState* (*SaveFn)(void*, void*, GameDataContainer*, GameData*, PosRotPair*);
static SaveFn saveOriginal = 0;
static GameSaveState* Save(
    void* first, void* second, GameDataContainer* database, GameData* references, PosRotPair* offset)
{
    GameSaveState* result = saveOriginal(first, second, database, references, offset);
    Character* character = static_cast<Character*>(result == first ? second : first);
    if (!ready || !result || !character || !character->isUnconcious() || character->isDead())
    {
        return result;
    }
    const Identity identity = Id(character->getHandle());
    Identity owner;
    bool confirmed = false;
    AcquireSRWLockShared(&lock);
    const Defeat* record = ledger.Find(identity);
    if (record && record->confirmed)
    {
        owner = record->actor;
        confirmed = true;
    }
    ReleaseSRWLockShared(&lock);
    GameData* data = confirmed ? result->getState(STATS) : 0;
    if (data)
    {
        data->storeHandle(Handle(owner), attributionKey, false);
    }
    return result;
}
static void (*loadOriginal)(Character*, GameSaveState*) = 0;
static void Load(Character* character, GameSaveState* save)
{
    loadOriginal(character, save);
    if (!ready || !character || !save)
    {
        return;
    }
    if (character->isPlayerCharacter() && (HasRow(character, OWN_DEFEATS) || HasRow(character, ALL_ENEMIES)))
    {
        ObserveActor(character);
    }
    const Identity identity = Id(character->getHandle());
    hand owner;
    GameData* data = save->getState(STATS);
    const bool confirmed = character->isUnconcious() && !character->isDead() && data &&
                           data->getHandle(owner, attributionKey) && !owner.isNull();
    AcquireSRWLockExclusive(&lock);
    ledger.Forget(identity);
    if (confirmed)
    {
        ledger.Record(identity, Id(owner), false, GetTickCount());
    }
    InterlockedExchange(&ledgerCount, ledger.Count());
    ReleaseSRWLockExclusive(&lock);
}
static void (*titleOriginal)(TitleScreen*, bool) = 0;
static void Title(TitleScreen* title, bool show)
{
    titleOriginal(title, show);
    if (!show || ready || registrationAttempted)
    {
        return;
    }
    registrationAttempted = true;
    // Both IDs are checked before any registration. Never replace another
    // plugin's metadata; the fixed IDs are the native save-file contract.
    if (getData(static_cast<TaskType>(OWN_DEFEATS)) || getData(static_cast<TaskType>(ALL_ENEMIES)))
    {
        ErrorLog("Loot-Scoot-Execute: task IDs 294/295 already registered; permanent jobs disabled");
        return;
    }
    metadata[0] = registerData(
        static_cast<TaskType>(OWN_DEFEATS), &AI::scoreNone, false, false, Tr("Execute own defeats"), 1);
    metadata[1] = registerData(
        static_cast<TaskType>(ALL_ENEMIES), &AI::scoreNone, false, false, Tr("Execute enemies"), 1);
    for (int modeIndex = 0; modeIndex < 2; ++modeIndex)
    {
        if (!metadata[modeIndex])
        {
            ErrorLog("Loot-Scoot-Execute: native permanent job registration failed");
            return;
        }
        metadata[modeIndex]->setPermaJob(
            PERMAJOB_NORMAL, false, static_cast<TaskType>(0), static_cast<TaskType>(0));
        metadata[modeIndex]->_resultsAlwaysFalse = true;
        metadata[modeIndex]->hasAction = true;
    }
    InterlockedExchange(&ready, 1);
}
// Discover an exact native class from PE RTTI once, without platform-specific
// physical addresses or writes to shared vtables. The entry must be a native
// jump thunk, and the body must match the audited class prefix.
static uintptr_t NativeActionAt(uintptr_t moduleBase,
                                const char* className,
                                const unsigned char* expectedPrefix,
                                size_t prefixLength)
{
    __try
    {
        const IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleBase);
        if (!moduleBase || dosHeader->e_magic != IMAGE_DOS_SIGNATURE || dosHeader->e_lfanew < 0 ||
            dosHeader->e_lfanew > 0x100000)
        {
            return 0;
        }
        const IMAGE_NT_HEADERS64* peHeader =
            reinterpret_cast<const IMAGE_NT_HEADERS64*>(moduleBase + dosHeader->e_lfanew);
        const DWORD imageSize = peHeader->OptionalHeader.SizeOfImage;
        if (peHeader->Signature != IMAGE_NT_SIGNATURE || imageSize < 0x100000 || imageSize > 0x3000000 ||
            peHeader->FileHeader.NumberOfSections > 32)
        {
            return 0;
        }
        const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(peHeader);
        uintptr_t typeRva = 0, locatorRva = 0, actionThunk = 0;
        const size_t nameLength = std::strlen(className) + 1;
        for (unsigned sectionIndex = 0; sectionIndex < peHeader->FileHeader.NumberOfSections; ++sectionIndex)
        {
            const IMAGE_SECTION_HEADER& section = sections[sectionIndex];
            if (!(section.Characteristics & IMAGE_SCN_MEM_READ) || section.VirtualAddress > imageSize ||
                section.Misc.VirtualSize > imageSize - section.VirtualAddress)
            {
                continue;
            }
            for (size_t sectionOffset = 16; sectionOffset + nameLength <= section.Misc.VirtualSize;
                 ++sectionOffset)
            {
                if (std::memcmp(reinterpret_cast<void*>(moduleBase + section.VirtualAddress + sectionOffset),
                                className,
                                nameLength) == 0)
                {
                    if (typeRva)
                    {
                        return 0;
                    }
                    typeRva = section.VirtualAddress + sectionOffset - 16;
                }
            }
        }
        if (!typeRva)
        {
            return 0;
        }
        for (unsigned sectionIndex = 0; sectionIndex < peHeader->FileHeader.NumberOfSections; ++sectionIndex)
        {
            const IMAGE_SECTION_HEADER& section = sections[sectionIndex];
            if (!(section.Characteristics & IMAGE_SCN_MEM_READ) || section.VirtualAddress > imageSize ||
                section.Misc.VirtualSize > imageSize - section.VirtualAddress)
            {
                continue;
            }
            for (size_t sectionOffset = 0; sectionOffset + 24 <= section.Misc.VirtualSize; sectionOffset += 4)
            {
                const DWORD* locator =
                    reinterpret_cast<const DWORD*>(moduleBase + section.VirtualAddress + sectionOffset);
                if (locator[0] == 1 && locator[1] == 0 && locator[3] == typeRva &&
                    locator[5] == section.VirtualAddress + sectionOffset)
                {
                    if (locatorRva)
                    {
                        return 0;
                    }
                    locatorRva = section.VirtualAddress + sectionOffset;
                }
            }
        }
        if (!locatorRva)
        {
            return 0;
        }
        for (unsigned sectionIndex = 0; sectionIndex < peHeader->FileHeader.NumberOfSections; ++sectionIndex)
        {
            const IMAGE_SECTION_HEADER& section = sections[sectionIndex];
            if (!(section.Characteristics & IMAGE_SCN_MEM_READ) || section.VirtualAddress > imageSize ||
                section.Misc.VirtualSize > imageSize - section.VirtualAddress)
            {
                continue;
            }
            for (size_t sectionOffset = 0; sectionOffset + 32 <= section.Misc.VirtualSize; sectionOffset += 8)
            {
                const uintptr_t* vtable =
                    reinterpret_cast<const uintptr_t*>(moduleBase + section.VirtualAddress + sectionOffset);
                if (vtable[0] != moduleBase + locatorRva)
                {
                    continue;
                }
                const uintptr_t actionEntry = vtable[3];
                if (actionEntry < moduleBase || actionEntry - moduleBase > imageSize - 5 ||
                    *reinterpret_cast<const unsigned char*>(actionEntry) != 0xE9)
                {
                    return 0;
                }
                const uintptr_t actionBody =
                    actionEntry + 5 + *reinterpret_cast<const LONG*>(actionEntry + 1);
                if (actionBody < moduleBase || actionBody - moduleBase > imageSize - prefixLength ||
                    std::memcmp(reinterpret_cast<void*>(actionBody), expectedPrefix, prefixLength))
                {
                    return 0;
                }
                if (actionThunk)
                {
                    return 0;
                }
                actionThunk = actionEntry;
            }
        }
        return actionThunk;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}
static uintptr_t NativeAction(const char* className, const unsigned char* expectedPrefix, size_t prefixLength)
{
    return NativeActionAt(
        reinterpret_cast<uintptr_t>(GetModuleHandleW(0)), className, expectedPrefix, prefixLength);
}
template <class Fn, class Hook> static bool HookNative(Fn nativeFunction, Hook hook, Hook* original)
{
    return KenshiLib::SUCCESS ==
           KenshiLib::AddHook(KenshiLib::GetRealAddress(nativeFunction), hook, original);
}
static bool Install(bool worldReady, unsigned platform)
{
    if (!worldReady)
    {
        return false;
    }
    // These two static SDK declarations are not exported by the installed
    // official DLL. Use the audited KEO native registration contract directly,
    // checking both platform-specific byte windows before resolving either.
    static const unsigned char registration[] = {
        0x48, 0x8b, 0xc4, 0x89, 0x48, 0x08, 0x55, 0x57, 0x41, 0x54, 0x48, 0x8b, 0xec, 0x48, 0x83, 0xec,
        0x60, 0x48, 0xc7, 0x45, 0xd0, 0xfe, 0xff, 0xff, 0xff, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70};
    static const unsigned char lookupSteam[] = {
        0x89, 0x4c, 0x24, 0x08, 0x48, 0x83, 0xec, 0x28, 0x48, 0x8d, 0x54, 0x24, 0x30, 0x48, 0x8d, 0x0d,
        0x9c, 0x41, 0xa6, 0x01, 0xe8, 0xaa, 0x20, 0xda, 0xff, 0x48, 0x8b, 0x40, 0x08, 0x48, 0x83, 0xc4};
    static const unsigned char lookupGog[] = {
        0x89, 0x4c, 0x24, 0x08, 0x48, 0x83, 0xec, 0x28, 0x48, 0x8d, 0x54, 0x24, 0x30, 0x48, 0x8d, 0x0d,
        0x0c, 0x26, 0xa6, 0x01, 0xe8, 0x83, 0x25, 0xda, 0xff, 0x48, 0x8b, 0x40, 0x08, 0x48, 0x83, 0xc4};
    const bool steam = platform == KenshiLib::BinaryVersion::STEAM;
    const uintptr_t moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(0));
    const uintptr_t registrationAddress = moduleBase + (steam ? 0x60DF70 : 0x60E280),
                    lookupAddress = moduleBase + (steam ? 0x283F40 : 0x283AD0);
    __try
    {
        if (std::memcmp(reinterpret_cast<void*>(registrationAddress), registration, sizeof(registration)) ||
            std::memcmp(
                reinterpret_cast<void*>(lookupAddress), steam ? lookupSteam : lookupGog, sizeof(lookupSteam)))
        {
            return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    getData = reinterpret_cast<GetDataFn>(lookupAddress);
    registerData = reinterpret_cast<RegisterDataFn>(registrationAddress);
    static const unsigned char blank[] = {0xC2, 0, 0};
    static const unsigned char stealth[] = {0x48, 0x8B, 0xC4, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41,
                                            0x56, 0x41, 0x57, 0x48, 0x8D, 0xA8, 0x08, 0xFE, 0xFF, 0xFF};
    const uintptr_t action = NativeAction(".?AVTask_Blank@@", blank, sizeof(blank));
    const uintptr_t knockoutAction = NativeAction(".?AVTask_StealthKO@@", stealth, sizeof(stealth));
    if (!action || !knockoutAction)
    {
        return false;
    }
    // Register only after every required hook is available. Partial installs
    // remain inert for our jobs; unrelated native actions always forward.
    bool ok = KenshiLib::SUCCESS ==
                  KenshiLib::AddHook(static_cast<intptr_t>(action), BlankAction, &blankActionOriginal) &&
              KenshiLib::SUCCESS ==
                  KenshiLib::AddHook(static_cast<intptr_t>(knockoutAction), Stealth, &stealthOriginal) &&
              HookNative(&Tasker::score, Score, &scoreOriginal) &&
              HookNative(&Character::_NV_hitByMeleeAttack, Melee, &meleeOriginal) &&
              HookNative(&Character::iShotYou, Ranged, &rangedOriginal) &&
              HookNative(&MedicalSystem::knockout, Knockout, &knockoutOriginal) &&
              HookNative(&MedicalSystem::medicalUpdate, Medical, &medicalOriginal) &&
              HookNative(&Character::_NV_serialise, Save, &saveOriginal) &&
              HookNative(&Character::_NV_loadFromSerialisePostCreationStage, Load, &loadOriginal);
    if (ok)
    {
        ok = HookNative(&TitleScreen::_NV_show, Title, &titleOriginal);
    }
    return ok;
}
static bool Shift()
{
    OIS::Keyboard* keyboard = key ? key->keyboard : 0;
    return keyboard && (keyboard->isKeyDown(OIS::KC_LSHIFT) || keyboard->isKeyDown(OIS::KC_RSHIFT));
}
static bool Add(Character* actor, int mode)
{
    if (!ready || !g_effectiveEnableExecuteAction || !actor || !actor->isPlayerCharacter() ||
        (mode == ALL_ENEMIES && !g_config.enableExecuteAll) || !ObserveActor(actor))
    {
        return false;
    }
    OrdersReceiver* orders = actor->getOrdersReciever();
    if (!orders)
    {
        return false;
    }
    if (!HasRow(actor, mode))
    {
        orders->addJob(static_cast<TaskType>(mode), hand(), Ogre::Vector3::ZERO, true);
    }
    const bool added = HasRow(actor, mode);
    if (ShouldLogExecuteDebug())
    {
        std::stringstream log;
        log << "Loot-Scoot-Execute: permanent_job_add mode=" << mode << " actor=" << actor->getHandle().index
            << "/" << actor->getHandle().serial << " result=" << added;
        PluginLog(log.str().c_str());
    }
    return added;
}
}

static bool PermanentExecuteStillValid(Character* actor, Character* target, int mode)
{
    return lse_jobs_runtime::AutomaticQueueValid(actor, target, mode);
}
static bool DispatchExecuteMenuAction(Character* actor, RootObject* target, bool all, bool verboseLog)
{
    if (!lse_jobs_runtime::Shift())
    {
        return all ? QueueExecuteAllFromNativeMenuSelection(actor, target, verboseLog)
                   : QueueExecuteFromNativeMenuSelection(actor, target, verboseLog);
    }
    bool added = false;
    const int mode = all ? lse_jobs::ALL_ENEMIES : lse_jobs::OWN_DEFEATS;
    if (!all)
    {
        added = lse_jobs_runtime::Add(actor, mode);
    }
    else
    {
        // Match existing Execute All selection scope, with no order cancellation
        // and no immediate execution while a character is still fighting.
        added = lse_jobs_runtime::Add(actor, mode);
        if (ou && ou->player)
        {
            unsigned actorCount = added ? 1 : 0;
            for (ogre_unordered_set<hand>::type::const_iterator selectedIt =
                     ou->player->selectedCharacters.begin();
                 selectedIt != ou->player->selectedCharacters.end() &&
                 actorCount < static_cast<unsigned>(g_config.executeAllMaxActors);
                 ++selectedIt)
            {
                Character* selected = TryResolveCharacterFromHandleSafe(*selectedIt);
                if (selected == actor)
                {
                    continue;
                }
                if (lse_jobs_runtime::Add(selected, mode))
                {
                    added = true;
                    ++actorCount;
                }
            }
        }
    }
    if (added && gui)
    {
        gui->updateToOrdersPanel();
    }
    return added;
}
static void RegisterExecutionCrime(Character* actor, Character* target, bool enemy)
{
    if (!actor || !target || enemy)
    {
        return;
    }
    // Publish the real crime on the murderer. Native perception decides who
    // witnesses it, raises bounties and betrayal, and selects retaliation.
    const bool accepted = actor->crimes.setCrime(CRIME_MURDER, target->getFaction(), target->getHandle());
    if (ShouldLogExecuteDebug())
    {
        PluginLog(accepted ? "Loot-Scoot-Execute: execution murder registered"
                           : "Loot-Scoot-Execute: native murder registration declined");
    }
}
