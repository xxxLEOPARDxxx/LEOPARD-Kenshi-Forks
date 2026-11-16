// Exact production Snapshot() extraction; deterministic non-waiting lock boundary.
#include "resources/resource_cache_store.h"
#include <vector>
#include <cstdio>
namespace resource_cache {
static bool initialized=false,allowLock=false;static int lock=0,attempts=0,releases=0;
static __int64 allocated=11,hits=12,misses=13,queued=14,rejected=15,readBytes=16;
static std::vector<int> files(2),requests(3);static std::string active("request");
static Stats progress={};
static __int64 Load64(const __int64& v){return v;}
static unsigned GetTickCount(){return 123;}
static bool StartupUnpaced(bool,unsigned){return true;}
static bool TryEnterCriticalSection(int*){++attempts;return allowLock;}
static void LeaveCriticalSection(int*){++releases;}
#include "resource_cache_snapshot_actual.inc"
}
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("FAIL %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){using namespace resource_cache;
 progress.startupDone=progress.startupCapped=true;progress.startupFiles=200;progress.startupProcessed=200;
 Stats s=Snapshot();C(!s.lockedSnapshotValid&&attempts==0&&releases==0);
 initialized=true;s=Snapshot();C(!s.lockedSnapshotValid&&attempts==1&&releases==0);
 C(!s.startupDone&&!s.pending&&!s.active);C(s.allocatedBytes==11&&s.readBytes==16);
 allowLock=true;s=Snapshot();C(s.lockedSnapshotValid&&attempts==2&&releases==1);
 C(s.startupDone&&s.startupCapped&&s.startupFiles==200&&s.startupProcessed==200);
 C(s.entries==2&&s.pending==3&&s.active);
 allowLock=false;s=Snapshot();C(!s.lockedSnapshotValid&&attempts==3&&releases==1);
 C(!s.startupDone&&!s.pending&&!s.active); // callers must preserve prior display, not trust these zeros
 initialized=false;s=Snapshot();C(!s.lockedSnapshotValid&&attempts==3&&releases==1);
 printf("Actual resource cache Snapshot lock boundary %u checks PASS\n",checks);return 0;
}
