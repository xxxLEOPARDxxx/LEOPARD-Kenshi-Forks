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
    Check(TryGeometryReservation(GiB/2,GiB),"geometry half of three-way cap");
    Check(TryTextureRetentionReservation(GiB/2-100,GiB),"native textures share remaining cap");
    Check(TextureRetentionReservation()==GiB/2-100,"texture accounting independent of geometry");
    Check(Reserve(100),"terrain uses exact remaining three-way100 bytes");
    Check(!Reserve(1)&&!TryTextureRetentionReservation(GiB/2-99,GiB)&&!TryGeometryReservation(GiB/2+1,GiB),"each cache refuses growth over common geometry+textures+terrain ceiling");
    Check(!TextureRetentionStandbyAllowed(GiB/2)&&!GeometryStandbyAllowed(GiB/2),"both owner caches see lowered common cap");
    {Reservation release(100);}
    Check(TryTextureRetentionReservation(0,GiB)&&TryGeometryReservation(0,GiB),"both pin reservations release independently");
    SetBudgetProbe(&HeadroomProbe,NULL);
    Check(TryTextureRetentionReservation(GiB/2,GiB),"existing texture residency not double charged in DXGI");
    InterlockedExchange(&fixtureRamAllowed,0);
    Check(!TextureRetentionStandbyAllowed(GiB)&&!TryTextureRetentionReservation(GiB/2+1,GiB),"CPU pressure refuses texture growth");
    Check(TryTextureRetentionReservation(GiB/4,GiB)&&TryTextureRetentionReservation(0,GiB),"CPU pressure still allows reducing and releasing textures");
    InterlockedExchange(&fixtureRamAllowed,1);SetBudgetProbe(&Probe,NULL);
    Check(TryGeometryReservation(GiB/4,GiB)&&TryTextureRetentionReservation(GiB/4,GiB),"two nonempty stores begin at1GiB");
    Check(TryGeometryReservation(GiB/4,8*GiB)&&TryTextureRetentionReservation(GiB/4,8*GiB),"both stores observe cap increase without dropping references");
    Check(TryGeometryReservation(2*GiB,8*GiB)&&TryTextureRetentionReservation(2*GiB,8*GiB),"raising cap1to8GiB permits both nonempty stores to grow");
    Check(TryGeometryReservation(0,GiB)&&TryTextureRetentionReservation(0,GiB),"release grown stores");
    Check(TryGeometryReservation(GiB/4,GiB)&&TryTextureRetentionReservation(GiB/4,GiB),"refill before terrain-only growth");
    Configure(true,8*GiB,1);
    Check(Reserve(2*GiB),"shared configuration propagates raised cap even when native pins stay unchanged");
    {Reservation release(2*GiB);}
    Check(TryGeometryReservation(0,GiB)&&TryTextureRetentionReservation(0,GiB),"release after shared setting test");
    Configure(false,GiB,1);
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
