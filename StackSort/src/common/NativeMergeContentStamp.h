#pragma once
#include "MergeContentStamp.h"
#include <cstring>
#include <float.h>
// Main-thread, explicit-click inspection of live borrowed section contents.
// No cached Item/GUI is dereferenced; only copied words survive this call.
inline bool ReadMergeContentStamp(Inventory* inventory,MergeContent::Value& out)
{
    if(!inventory)return false;
    lektor<InventorySection*>& sections=inventory->getAllSections();
    if(sections.size()>MergeContent::Builder::MaxSections)return false;
    size_t count=0;
    for(size_t s=0;s<sections.size();++s)
    {
        InventorySection* section=sections[s];if(!StackSort_EligibleSection(section)||!section->enabled)continue;
        const size_t n=section->getItems().size();if(n>MergeContent::Builder::MaxItems-count)return false;count+=n;
    }
    const bool rotate=StackSort_RotateAvailable();
    MergeContent::Builder stamp(count,(MergeContent::Word)(size_t)inventory,rotate);
    for(size_t s=0;s<sections.size();++s)
    {
        InventorySection* section=sections[s];if(!StackSort_EligibleSection(section)||!section->enabled)continue;
        const Ogre::vector<InventorySection::SectionItem>::type& items=section->getItems();
        for(size_t i=0;i<items.size();++i)
        {
            Item* item=items[i].item;if(!item)continue;
            if(!_finite(item->originalFullChargeAmount)||item->quantity<0
                ||(item->originalFullChargeAmount>0.0f&&!_finite(item->chargesLeft)))return false;
            MergeContent::Key key;key.part[0]=(MergeContent::Word)(size_t)section;
            key.part[1]=(MergeContent::Word)(size_t)item->getGameData();
            key.part[2]=(MergeContent::Word)(size_t)item->manufacturerData;
            key.part[3]=(MergeContent::Word)(size_t)item->materialData;
            // Ownership compatibility is not an equivalence relation: two clean
            // items ignore owners; if EITHER is stolen Native compares BOTH.
            // Keep all such possible pairs in the same rotation group, and put
            // stolen state / full owner identity in EACH item's semantic words.
            const bool stolen=item->isStolen(true);
            const hand& owner=item->getProperOwner();
            const hand& token=item->handle;MergeContent::Word value=(MergeContent::Word)(size_t)item;
            MergeContent::AddWord(value,stolen?1:0);
            MergeContent::AddWord(value,(unsigned int)owner.type);MergeContent::AddWord(value,owner.container);
            MergeContent::AddWord(value,owner.containerSerial);MergeContent::AddWord(value,owner.index);MergeContent::AddWord(value,owner.serial);
            MergeContent::AddWord(value,(unsigned int)token.type);MergeContent::AddWord(value,token.container);
            MergeContent::AddWord(value,token.containerSerial);MergeContent::AddWord(value,token.index);MergeContent::AddWord(value,token.serial);
            unsigned int charge=0,full=0;if(item->originalFullChargeAmount>0.0f)std::memcpy(&charge,&item->chargesLeft,4);std::memcpy(&full,&item->originalFullChargeAmount,4);
            MergeContent::AddWord(value,(unsigned int)item->quantity);MergeContent::AddWord(value,charge);MergeContent::AddWord(value,full);
            MergeContent::AddWord(value,item->isEquipped?1:0);MergeContent::AddWord(value,(unsigned int)item->isStackable(section));
            int width=item->itemWidth,height=item->itemHeight;if(rotate&&width>height){int swap=width;width=height;height=swap;}
            MergeContent::AddWord(value,(unsigned int)width);MergeContent::AddWord(value,(unsigned int)height);
            if(!stamp.Add(key,value,rotate&&StackSort_IsRotated(item)))return false;
        }
    }
    out=stamp.Finish();return true;
}
