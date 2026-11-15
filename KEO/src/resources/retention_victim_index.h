#pragma once

namespace resource_retention {
// Nonowning map iterators only: no additional resource pin or allocation.
// Entry::victimSlot is valid until Remove/Reset; map nodes must outlive it.
// Numeric unsigned touch ordering deliberately matches the old scan even at
// clock wrap. Equal touches use the SAME map comparator as that scan's begin.
template<class Map,unsigned Capacity> class VictimIndex {
    typedef typename Map::iterator Iterator;
    Map*map_;
    Iterator nodes_[Capacity];
    unsigned size_;
    bool Less(Iterator a,Iterator b)const{
        if(a->second.touch<b->second.touch)return true;
        if(b->second.touch<a->second.touch)return false;
        return map_->key_comp()(a->first,b->first);
    }
    void Swap(unsigned a,unsigned b){
        Iterator t=nodes_[a];nodes_[a]=nodes_[b];nodes_[b]=t;
        nodes_[a]->second.victimSlot=a;nodes_[b]->second.victimSlot=b;
    }
    void Repair(unsigned slot){
        if(slot&&Less(nodes_[slot],nodes_[(slot-1)/2])){
            do{unsigned parent=(slot-1)/2;Swap(slot,parent);slot=parent;}
            while(slot&&Less(nodes_[slot],nodes_[(slot-1)/2]));
        }else{
            while(slot*2+1<size_){
                unsigned child=slot*2+1;
                if(child+1<size_&&Less(nodes_[child+1],nodes_[child]))++child;
                if(!Less(nodes_[child],nodes_[slot]))break;
                Swap(slot,child);slot=child;
            }
        }
    }
    VictimIndex(const VictimIndex&);
    VictimIndex&operator=(const VictimIndex&);
public:
    explicit VictimIndex(Map&map):map_(&map),size_(0){}
    unsigned Size()const{return size_;}
    Iterator Oldest()const{return size_?nodes_[0]:map_->end();}
    bool Insert(Iterator i){
        if(size_>=Capacity||i->second.victimSlot!=~0u)return false;
        unsigned slot=size_++;nodes_[slot]=i;i->second.victimSlot=slot;Repair(slot);return true;
    }
    // Call after ANY touch change: refresh can restore an old touch, while a
    // native hit normally increases it and an unsigned clock wrap decreases it.
    void Changed(Iterator i){Repair(i->second.victimSlot);}
    void Remove(Iterator i){
        unsigned slot=i->second.victimSlot;i->second.victimSlot=~0u;
        --size_;
        if(slot!=size_){nodes_[slot]=nodes_[size_];nodes_[slot]->second.victimSlot=slot;}
        nodes_[size_]=Iterator();
        if(slot<size_)Repair(slot);
    }
    // Must run BEFORE map::clear destroys Entries and their native references.
    void Reset(){
        while(size_){--size_;nodes_[size_]->second.victimSlot=~0u;nodes_[size_]=Iterator();}
    }
};
}
