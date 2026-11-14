// Exercise the ACTUAL store reservation/terrain eviction under a shared cap,
// including real WARP COM resources. Reuse the established independent decoder
// and worker fixtures; no gigabyte-sized allocations are needed for cap math.
#define RESOURCE_GPU_INCLUDE_STORE
#define main ExistingGpuFixtureMain
#include "test_resource_gpu.cpp"
#undef main
static bool HeadroomProbe(ID3D11Device*,resource_gpu::U64*b,resource_gpu::U64*u,void*){
    *b=4ULL<<30;*u=*b-*b/10-16;return true;
}
int main(){
    using namespace resource_gpu;
    ID3D11Device*d=NULL;ID3D11DeviceContext*c=NULL;D3D_FEATURE_LEVEL level;
    Check(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&d,&level,&c)),"shared cap WARP");
    Check(Start()&&SetDevice(d),"shared cap worker/device");SetBudgetProbe(&Probe,NULL);
    Configure(false,GiB,1); // native geometry MUST work with terrain warming off
    Check(TryGeometryReservation(GiB-100,GiB),"geometry independent of terrain toggle");
    Check(GeometryReservation()==GiB-100&&Read64(allocated)==0,"geometry keeps separate true accounting");
    InterlockedExchange(&enabled,1); // extract actual reservation arithmetic
    Check(Reserve(100),"terrain exact remaining100 shared bytes");
    Check(!Reserve(1),"terrain cannot allocate over geometry+terrain cap");
    Check(!TryGeometryReservation(GiB-99,GiB),"geometry cannot allocate over terrain+geometry cap");
    {Reservation release(100);}
    Check(TryGeometryReservation(0,GiB)&&GeometryReservation()==0,"geometry release balances");
    Check(TryGeometryReservation(2*GiB,2*GiB),"larger common cap");
    Check(!GeometryStandbyAllowed(GiB),"lower common cap signals existing pressure");
    Check(TryGeometryReservation(0,GiB),"release after lowering cap");
    // Existing geometry is ALREADY included in DXGI CurrentUsage. Requiring
    // an extra equal allocation here would double-charge and refuse residency.
    SetBudgetProbe(&HeadroomProbe,NULL);
    Check(TryGeometryReservation(GiB/2,GiB),"existing native buffers not double charged in DXGI");
    InterlockedExchange(&fixtureRamAllowed,0);
    Check(!GeometryStandbyAllowed(GiB),"CPU pressure rejects geometry standby");
    Check(!TryGeometryReservation(GiB/2+1,GiB),"CPU pressure refuses growth");
    Check(TryGeometryReservation(0,GiB),"CPU pressure allows full release");
    InterlockedExchange(&fixtureRamAllowed,1);SetBudgetProbe(&Probe,NULL);
    InterlockedExchange(&fixturePressure,1);Check(!GeometryStandbyAllowed(GiB),"DXGI pressure rejects geometry standby");
    InterlockedExchange(&fixturePressure,0);
    unsigned generation=DeviceGeneration();Check(!SetDevice(NULL),"device detach");
    Check(DeviceGeneration()!=generation&&!GeometryStandbyAllowed(GiB),"device generation/removal visible to owner");
    Check(Stop(250),"shared reservation worker shutdown");
    Check(Read64(allocated)==0&&GeometryReservation()==0,"all shared accounting released");
    c->Release();d->Release();std::puts("geometry shared-cap actual WARP reservation checks passed");return 0;
}
