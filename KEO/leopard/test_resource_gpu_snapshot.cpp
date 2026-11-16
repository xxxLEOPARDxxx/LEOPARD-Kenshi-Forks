// Exact production GPU Snapshot() extraction; no worker/device/driver calls.
#include "resources/resource_gpu_store.h"
#include <vector>
#include <cstdio>
namespace resource_gpu {
static bool initialized=false,allowLock=false,pressure=true;
static int lock=0,attempts=0,releases=0;
static __int64 allocated=11,reserved=12,created=13,rejected=14,retries=15;
static std::vector<int> entries(2),requests(3);static std::string activeKey("request");
static ID3D11Device* device=reinterpret_cast<ID3D11Device*>(1);
static __int64 Read64(const __int64& v){return v;}
static bool TryEnterCriticalSection(int*){++attempts;return allowLock;}
static void LeaveCriticalSection(int*){++releases;}
#include "resource_gpu_snapshot_actual.inc"
}
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("FAIL %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){using namespace resource_gpu;
 Stats s=Snapshot();C(!s.lockedSnapshotValid&&attempts==0&&releases==0);
 initialized=true;s=Snapshot();C(!s.lockedSnapshotValid&&attempts==1&&releases==0);
 C(!s.pending&&!s.active);C(s.allocatedBytes==11&&s.retries==15);
 allowLock=true;s=Snapshot();C(s.lockedSnapshotValid&&attempts==2&&releases==1);
 C(s.entries==2&&s.pending==3&&s.active&&s.deviceReady&&s.pressure);
 allowLock=false;s=Snapshot();C(!s.lockedSnapshotValid&&attempts==3&&releases==1);
 C(!s.pending&&!s.active);
 initialized=false;s=Snapshot();C(!s.lockedSnapshotValid&&attempts==3&&releases==1);
 printf("Actual GPU cache Snapshot lock boundary %u checks PASS\n",checks);return 0;
}
