"""Extract production placement selection; never load a game DLL/process.

Native boundary model follows Steam 1.0.65: getValidInventoryPosition clamps
coordinates only; findNearestPlaceForItem checks actual Item dimensions and
occupied footprints. hasRoomForItem(GameData*,1) retains stack admission.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Source/BetterLooting.cpp'
FIXTURE = r'''
#include <stdint.h>
#include <cstdio>
#include <vector>
struct GameData { int width,height; GameData(int w=1,int h=1):width(w),height(h){} };
struct InventorySection;
struct Item {
    int width,height,quantity;
    Item(int w=1,int h=1,int q=1):width(w),height(h),quantity(q){}
    int isStackable(InventorySection*) const;
};
template<class T> struct lektor {
    std::vector<T> data;
    uint32_t size() const {return static_cast<uint32_t>(data.size());}
    T operator[](uint32_t n) const {return data[n];}
};
struct InventorySection {
    bool enabled,containerSlot,isAnEquippedItemSection,compatible,itemLimit;
    bool genericStackRoom;
    int width,height,maxStack;
    unsigned hintCalls,footprintCalls,clampCalls;
    std::vector<bool> occupied;
    InventorySection(int w,int h):enabled(true),containerSlot(false),isAnEquippedItemSection(false),compatible(true),itemLimit(false),genericStackRoom(false),width(w),height(h),maxStack(5),hintCalls(0),footprintCalls(0),clampCalls(0),occupied(w*h,false){}
    bool getEnabled(){return enabled;}
    bool isLimitedSlotCompatible(Item*){return compatible;}
    bool getItemsLimitReached(){return itemLimit;}
    bool Empty(int w,int h,int& x,int& y) {
        for(x=0;x+w<=width;++x)for(y=0;y+h<=height;++y) {
            bool free=true;
            for(int a=x;a<x+w;++a)for(int b=y;b<y+h;++b)if(occupied[b*width+a])free=false;
            if(free)return true;
        }
        x=y=-1;return false;
    }
    bool hasRoomForItem(GameData* data,int quantity) {
        ++hintCalls;
        if(genericStackRoom)return true;
        int x=0,y=0;return !itemLimit && quantity==1 && Empty(data->width,data->height,x,y);
    }
    bool getValidInventoryPosition(Item* item,int& x,int& y) {
        ++clampCalls;x=y=0;return item->width<=width && item->height<=height;
    }
    bool findNearestPlaceForItem(Item* item,int& x,int& y) {
        ++footprintCalls;return Empty(item->width,item->height,x,y);
    }
    void Fill(){for(unsigned i=0;i<occupied.size();++i)occupied[i]=true;}
};
int Item::isStackable(InventorySection* s)const{return s->maxStack;}
struct Inventory {lektor<InventorySection*> sections;lektor<InventorySection*>& getAllSections(){return sections;}};
struct BetterLootingDestinations {Inventory* first;Inventory* second;};
void BlTrace(const char*){}
// @PRODUCTION@
int checks=0,failures=0,detached=0;
void Check(bool ok,const char* text){++checks;if(!ok){++failures;printf("FAIL: %s\n",text);}}
BetterLootingDirectPlacement Preflight(const BetterLootingDestinations& dest,Item* item,GameData* data) {
    const BetterLootingDirectPlacement p=FindBetterLootingDirectPlacement(dest,item,data,1);
    if(p.found)++detached; // Represents the caller's later removeItem... boundary.
    return p;
}
int main(){
    GameData square(1,1);Item item(1,1,4);
    Inventory first,second;BetterLootingDestinations dest={&first,&second};
    InventorySection full(2,3);full.Fill();first.sections.data.push_back(&full);
    int x=0,y=0;
    Check(full.getValidInventoryPosition(&item,x,y),"native coordinate clamp can be true for a completely full section");
    full.clampCalls=0;
    Check(!Preflight(dest,&item,&square).found && detached==0,"full inventory is rejected before a single source unit is detached");
    Check(full.footprintCalls==1,"negative generic hint is verified using actual native occupancy once");
    Check(item.quantity==4&&item.width==1&&item.height==1,"preflight never mutates source quantity or rotation");

    InventorySection empty(2,3);second.sections.data.push_back(&empty);
    BetterLootingDirectPlacement p=Preflight(dest,&item,&square);
    Check(p.found&&p.inventory==&second&&p.section==&empty,"a full preferred inventory cannot mask a free secondary inventory");
    Check(detached==1,"only the admitted secondary destination reaches the detach boundary");

    InventorySection later(2,3);first.sections.data.push_back(&later);
    p=Preflight(dest,&item,&square);
    Check(p.found&&p.inventory==&first&&p.section==&later,"a rejected first section does not mask a later section");
    Check(full.clampCalls==0,"bounds-only clamp is never mistaken for free space");

    Inventory stackInventory;InventorySection stack(1,1);stack.Fill();stack.genericStackRoom=true;
    stackInventory.sections.data.push_back(&stack);BetterLootingDestinations stackDest={&stackInventory,0};
    p=Preflight(stackDest,&item,&square);
    Check(p.found&&p.section==&stack,"native room hint still admits a compatible partial stack on a full grid");
    Check(stack.footprintCalls==0,"positive generic stack hint does not add a redundant grid scan");
    Check(item.quantity==4,"one-unit preflight does not require room for the full four-unit source stack");

    GameData unrotated(3,2);Item rotated(2,3,4);
    Inventory rotatedInventory;InventorySection rotatedSection(2,3);rotatedInventory.sections.data.push_back(&rotatedSection);
    BetterLootingDestinations rotatedDest={&rotatedInventory,0};
    Check(!rotatedSection.hasRoomForItem(&unrotated,1),"GameData dimensions reject a valid rotated Item footprint");
    p=Preflight(rotatedDest,&rotated,&unrotated);
    Check(p.found&&p.section==&rotatedSection,"actual free 2x3 rotated footprint survives a negative 3x2 generic hint");
    Check(rotated.width==2&&rotated.height==3&&rotated.quantity==4,"rotation and split quantity remain unchanged");
    rotatedSection.Fill();int prior=detached;
    Check(!Preflight(rotatedDest,&rotated,&unrotated).found&&detached==prior,"a rotated shape fitting the bounds still cannot use occupied cells");

    Inventory cappedInventory;InventorySection capped(2,3);capped.itemLimit=true;cappedInventory.sections.data.push_back(&capped);
    BetterLootingDestinations cappedDest={&cappedInventory,0};prior=detached;
    Check(!Preflight(cappedDest,&rotated,&unrotated).found&&detached==prior,"native item-count limit rejects an otherwise empty rotated footprint before detach");
    Check(capped.footprintCalls==0,"reached item-count cap avoids an unnecessary footprint search");
    capped.genericStackRoom=true;p=Preflight(cappedDest,&rotated,&unrotated);
    Check(p.found,"an existing partial stack remains legal when no new row may be added");

    Inventory filterInventory;InventorySection disabled(2,3),container(2,3),equipment(2,3),incompatible(2,3);
    disabled.enabled=false;container.containerSlot=true;equipment.isAnEquippedItemSection=true;incompatible.compatible=false;
    filterInventory.sections.data.push_back(&disabled);filterInventory.sections.data.push_back(&container);filterInventory.sections.data.push_back(&equipment);filterInventory.sections.data.push_back(&incompatible);
    BetterLootingDestinations filtered={&filterInventory,0};prior=detached;
    Check(!Preflight(filtered,&item,&square).found&&detached==prior,"existing section eligibility filters remain mandatory");
    Check(!disabled.hintCalls&&!container.hintCalls&&!equipment.hintCalls&&!incompatible.hintCalls,"ineligible sections receive no native placement query");
    Check(!FindBetterLootingDirectPlacementInInventory(&first,&item,&square,0,false).found,"invalid quantity remains rejected");
    printf("BetterLooting extracted placement preflight: %s (%d checks)\n",failures?"FAIL":"PASS",checks);
    return failures?1:0;
}
'''

def main():
    source=SOURCE.read_text(encoding='utf-8-sig')
    start=source.index('struct BetterLootingDirectPlacement')
    end=source.index('// BL_FIX_PLAYER_DESTINATION_ORDER_AND_EMERGENCY_STORAGE_FALLBACK',start)
    code=FIXTURE.replace('// @PRODUCTION@',source[start:end])
    vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
    sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
    env=os.environ.copy()
    env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
    env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include')
    env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64')
    with tempfile.TemporaryDirectory(prefix='betterlooting-placement-') as temporary:
        out=Path(temporary)
        cpp=out/'placement.cpp';exe=out/'placement.exe'
        cpp.write_text(code,encoding='utf-8')
        subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/W3',str(cpp),'/Fe'+str(exe),'/Fo'+str(out/'placement.obj')],cwd=out,env=env,check=True)
        subprocess.run([str(exe)],check=True)

if __name__=='__main__':
    main()
