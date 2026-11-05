#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "base/log_admission.h"
#include "game/game.h"
#include "zone/handoff/zone_handoff.h"
#include "diag/squad_watch.h"
#include "diag/squad_watch_policy.h"
#include "base/klib_include.h"
#include <kenshi/GameWorld.h>
#include <kenshi/Faction.h>
#include <kenshi/Platoon.h>
#include <kenshi/GameData.h>
#include <kenshi/OptionsHolder.h>
#include "base/klib_include_end.h"
#include <sstream>
#include <math.h>

namespace {
static SquadWatchLedger<8192> ledger;
static unsigned factionIndex, listKind, platoonIndex, visits, faults, firstSeen, activated, returned, omitted;
static unsigned emitted, identityFull, pendingCount, pendingRead;
static double startedAt;
static float camX,camZ;
static bool camValid, truncated;
struct Row {
    SquadWatchKey key;
    int current, original, type;
    float x,z;
    bool neverActive, persistent, resident, dead;
    char faction[80], id[80], squad[80], town[80];
};
struct Pending { Row row; SquadWatchEvent event; double at; float cameraX,cameraZ; bool cameraValid; };
static Pending pending[24];
// Bounded copies; no game allocator, virtual call, retained string or pointer.
static void CopyID(char* out, const std::string& s) {
    size_t n=s.size(); if(n>79)n=79;
    memcpy(out,s.data(),n); out[n]=0;
    for(size_t i=0;i<n;++i) if((unsigned char)out[i]<32 || out[i]=='"') out[i]='_';
}
// Only POD locals in SEH readers (VC10); never log or recover by writing to game data.
static int ReadRow(unsigned f, unsigned kind, unsigned p, Row* out) {
    __try {
        GameWorld* w=(GameWorld*)GameAddr(RVA_GLOBAL_GAMEWORLD);
        FactionManager* mgr=w->factionMgr;
        if(!mgr)return 0;
        if(mgr->participants.size()>4096) return -2;
        if(f>=mgr->participants.size()) return 0;
        Faction* fac=mgr->participants.stuff[f]; if(!fac)return 2;
        const lektor<Platoon*>& list=kind?fac->unloadedPlatoons:fac->activePlatoons;
        if(list.size()>8192) return -1;
        if(p>=list.size()) return 2;
        Platoon* sq=list.stuff[p]; if(!sq)return 3;
        memcpy(out->key.v,(const char*)&sq->handle+8,sizeof(out->key.v));
        out->current=sq->_characterCountCurrent; out->original=sq->_characterCountOriginal;
        out->type=(int)sq->squadType; out->x=sq->pos.x; out->z=sq->pos.z;
        out->neverActive=sq->hasNeverBeenActivated; out->persistent=sq->_persistentSquad;
        out->resident=sq->isResidentSquad; out->dead=sq->isDead;
        out->faction[0]=out->squad[0]=out->town[0]=0;
        if(fac->data)CopyID(out->faction,fac->data->stringID);
        CopyID(out->id,sq->stringID);
        if(sq->squadTemplate)CopyID(out->squad,sq->squadTemplate->stringID);
        if(sq->myBaseHomeTownData)CopyID(out->town,sq->myBaseHomeTownData->stringID);
        return 1;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
static bool Camera(const void* p) {
    __try { if(!p)return false; camX=((const float*)p)[0]; camZ=((const float*)p)[2];
        return _finite(camX) && _finite(camZ); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool Settings(float* v) {
    __try { const OptionsHolder* o=(const OptionsHolder*)GameAddr(RVA_GLOBAL_OPTIONS);
        v[0]=o->populationMult; v[1]=o->squadSizeMult; v[2]=o->raidSizeMult;
        v[3]=o->raidFrequencyMult; v[4]=o->NPCRange;
        v[5]=((GameWorld*)GameAddr(RVA_GLOBAL_GAMEWORLD))->frameSpeedMult; return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static void Drain() {
    for(unsigned n=0;n<2 && pendingRead<pendingCount;++n) {
        const Pending& p=pending[pendingRead++]; const Row& r=p.row;
        std::ostringstream s; s << "SquadWatch observed t=" << p.at << " epoch=" << ledger.epoch
          << " event=" << (p.event==SQUAD_WATCH_FIRST?"firstObserved":p.event==SQUAD_WATCH_ACTIVATED?"becameActive":"returned")
          << " faction=\"" << r.faction << "\" squad=\"" << r.squad << "\" id=\"" << r.id
          << "\" town=\"" << r.town << "\" key=";
        for(unsigned j=0;j<5;++j)s << (j?":":"") << r.key.v[j];
        s << " members=" << r.current << " original=" << r.original << " type=" << r.type
          << " lastKnownXZ=" << r.x << "," << r.z
          << " neverActive=" << r.neverActive << " persistent=" << r.persistent
          << " resident=" << r.resident << " dead=" << r.dead;
        if(p.cameraValid && _finite(r.x) && _finite(r.z)) {
            double dx=(double)r.x-p.cameraX,dz=(double)r.z-p.cameraZ;
            s << " cameraXZ=" << p.cameraX << "," << p.cameraZ << " cameraDistanceM=" << sqrt(dx*dx+dz*dz);
        }
        LogMsg(s.str());
    }
}
static void Finish(double now) {
    std::ostringstream s;
    s << "SquadWatch census epoch=" << ledger.epoch << " pass=" << ledger.serial
      << " baseline=" << ledger.baseline << " unique=" << ledger.unique
      << " active=" << ledger.active << " unloaded=" << ledger.unloaded
      << " firstObserved=" << firstSeen << " becameActive=" << activated
      << " returned=" << returned << " visits=" << visits << " faults=" << faults
      << " factionsVisited=" << factionIndex
      << " truncated=" << truncated << " identityTableFull=" << identityFull << " detailOmitted=" << omitted
      << " spanSec=" << now-startedAt;
    float v[6]; if(Settings(v))s << " populationMult=" << v[0] << " squadSizeMult=" << v[1]
      << " raidSizeMult=" << v[2] << " raidFrequencyMult=" << v[3] << " npcRange=" << v[4] << " gameSpeed=" << v[5];
    LogMsg(s.str()); ledger.End(now);
}
}

void SquadWatchTick(double now, const void* cameraPosition) {
    // Debug-off fast path: no time query, world access, allocation or enumeration.
    if(!DetailedLogEnabled()) { ledger.Begin(false,0,0); pendingCount=pendingRead=0; return; }
    unsigned epoch=ZoneHandoffWorldEpoch();
    if(ledger.epoch!=epoch)pendingCount=pendingRead=0;
    Drain();
    if(!ledger.running && pendingRead<pendingCount)return;
    if(!ledger.running || ledger.epoch!=epoch) {
        if(!ledger.Begin(true,epoch,now))return;
        factionIndex=listKind=platoonIndex=visits=faults=firstSeen=activated=returned=omitted=emitted=identityFull=0;
        pendingCount=pendingRead=0;
        truncated=false; startedAt=now;
        KEO_DETAIL("SquadWatch: bounded census; firstObserved is NOT a proven creation; active/unloaded membership is not a birth count. No spawn rules changed.");
    }
    camValid=Camera(cameraPosition);
    // At most 32 native entries and two detail lines per frame. All cursors are
    // indices: live containers are re-read, no native pointer survives the call.
    for(unsigned work=0;work<32;++work) {
        if(factionIndex>=256 || visits>=8192) { truncated=true; Finish(now); return; }
        Row r; int status=ReadRow(factionIndex,listKind,platoonIndex,&r);
        if(status==0) { Finish(now); return; }
        if(status==-2) { ++faults; truncated=true; Finish(now); return; }
        if(status==2 || status<0) {
            if(status<0) { ++faults; truncated=true; }
            platoonIndex=0;
            if(++listKind>=2) {listKind=0; ++factionIndex;}
            continue;
        }
        ++visits; ++platoonIndex;
        if(status!=1)continue;
        unsigned valid=0; for(unsigned j=0;j<5;++j)valid|=r.key.v[j];
        if(!valid) {++faults; truncated=true; continue;}
        SquadWatchEvent e=ledger.Observe(r.key,listKind?2:1);
        if(e==SQUAD_WATCH_FULL) {truncated=true; ++omitted; ++identityFull; continue;}
        if(e==SQUAD_WATCH_FIRST)++firstSeen;
        if(e==SQUAD_WATCH_ACTIVATED)++activated;
        if(e==SQUAD_WATCH_RETURNED)++returned;
        if(ledger.baseline || listKind || e==SQUAD_WATCH_SEEN)continue;
        if(emitted>=24) {++omitted;continue;}
        ++emitted; Pending& p=pending[pendingCount++];
        p.row=r; p.event=e; p.at=now; p.cameraX=camX; p.cameraZ=camZ; p.cameraValid=camValid;
    }
}
