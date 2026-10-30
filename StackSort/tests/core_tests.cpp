#include <cstdio>
#include "SortPolicy.h"
#include "Packer.h"
#include "TargetMemory.h"
#include "ResultCache.h"
static int failures = 0, checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, #x); } } while(0)
int main()
{
    CHECK(!SortPolicy::SortableGrid(1, 1, 0, false, true));
    CHECK(!SortPolicy::SortableGrid(6, 6, 1, false, true));
    CHECK(!SortPolicy::SortableGrid(0, 6, 0, false, true));
    CHECK(!SortPolicy::SortableGrid(6, 6, 0, true, true));
    CHECK(!SortPolicy::SortableGrid(6, 6, 0, false, false));
    CHECK(SortPolicy::SortableGrid(6, 6, 0, false, true));
    CHECK(SortPolicy::SortableGrid(1, 8, 0, false, true));
    CHECK(SortPolicy::CanPublish(0, 5, 5, 1, 4));
    CHECK(!SortPolicy::CanPublish(1, 5, 5, 1, 4));
    CHECK(!SortPolicy::CanPublish(0, 6, 5, 1, 4));
    CHECK(!SortPolicy::CanPublish(0, 5, 5, 0, 4));
    CHECK(!SortPolicy::CanPublish(0, 5, 5, 5, 4));
    TargetKey remembered;
    TargetMemory::Remember(remembered, 4);
    int target = 0;
    CHECK(TargetMemory::Recall(remembered, target) && target == 4);
    TargetMemory::Clear();
    CHECK(!TargetMemory::Recall(remembered, target));
    // Run the actual packer with a rectangle that only fits when rotated.
    Packer::Item item = {};
    item.id=0;item.w=3;item.h=2;item.canRotate=false;
    item.exactId=1;item.customGroupId=-1;item.gameDataType=-1;item.itemFunction=-1;
    std::vector<Packer::Item> items(1,item);
    Packer::GridSpec grid={2,3,1};
    Packer::Result result = Packer::Search::Pack(grid,items,Packer::TARGET_H);
    CHECK(!result.allPlaced);
    items[0].canRotate=true;
    result=Packer::Search::Pack(grid,items,Packer::TARGET_H);
    CHECK(result.allPlaced && Packer::Grid::ValidateLayout(2,3,items,result.placements));
    CHECK(result.placements.size()==1 && result.placements[0].w==2 && result.placements[0].h==3);
    // Existing rotated dimensions work even without the optional Rotate API.
    items[0].w=2;items[0].h=3;items[0].canRotate=false;
    result=Packer::Search::Pack(grid,items,Packer::TARGET_W);
    CHECK(result.allPlaced && result.placements[0].w==2 && result.placements[0].h==3);
    volatile long cancelled=1;
    result=Packer::Search::Pack(grid,items,Packer::TARGET_H,&cancelled);
    CHECK(!result.allPlaced);
    // Opaque section keys are never dereferenced in the real cache.
    ResultCache::Init();
    InventorySection* section=reinterpret_cast<InventorySection*>(0x1000);
    std::vector<ItemIdentity> identities(1);
    identities[0].item=reinterpret_cast<Item*>(0x2000);
    identities[0].index=1;identities[0].serial=2;identities[0].container=0;
    identities[0].containerSerial=0;identities[0].type=1;identities[0].w=2;identities[0].h=3;
    LONG oldGeneration=ResultCache::Insert(section,2,3,Packer::TARGET_H,items,identities);
    CachedSection* cached=ResultCache::Find(section);
    CHECK(cached && cached->generation==oldGeneration);
    ResultCache::InvalidateAll();
    CHECK(cached->valid==0 && cached->generation!=oldGeneration);
    CHECK(!SortPolicy::CanPublish(0,cached->generation,oldGeneration,1,2));
    LONG currentGeneration=ResultCache::Insert(section,2,3,Packer::TARGET_H,items,identities);
    CHECK(currentGeneration!=oldGeneration);
    CHECK(!SortPolicy::CanPublish(1,currentGeneration,currentGeneration,1,2));
    InterlockedExchange(&cached->generation,LONG_MAX);
    ResultCache::InvalidateAll();
    CHECK(cached->generation==1);
    ResultCache::Shutdown();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
