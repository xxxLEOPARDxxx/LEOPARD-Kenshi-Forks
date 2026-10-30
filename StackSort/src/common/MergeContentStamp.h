#pragma once
#include <vector>
#include <cstddef>
#include <stdexcept>
// Change detector only; Native canStackWith remains the transfer authority.
// XY and section iteration order are deliberately absent. Hash collisions can
// at worst defer a retry; they never authorize a native merge.
namespace MergeContent
{
typedef unsigned __int64 Word;
inline Word Mix(Word x){x^=x>>33;x*=0xff51afd7ed558ccdui64;x^=x>>33;x*=0xc4ceb9fe1a85ec53ui64;return x^(x>>33);}
inline void AddWord(Word& hash,Word value){hash=Mix(hash^Mix(value+0x9e3779b97f4a7c15ui64));}
struct Value
{
    Word a,b;size_t items;
    Value():a(0),b(0),items(0){}
    bool operator==(const Value& o)const{return a==o.a&&b==o.b&&items==o.items;}
    bool operator!=(const Value& o)const{return !(*this==o);}
};
struct Key
{
    Word part[4];
    Key(){for(int i=0;i<4;++i)part[i]=0;}
    Word Hash()const{Word h=0;for(int i=0;i<4;++i)AddWord(h,part[i]);return h;}
    bool operator==(const Key& o)const{for(int i=0;i<4;++i)if(part[i]!=o.part[i])return false;return true;}
};
struct Side
{
    Word sum,xorValue,count;
    Side():sum(0),xorValue(0),count(0){}
    void Add(Word value){sum+=value;xorValue^=Mix(value);++count;}
    bool operator<(const Side& o)const{return count!=o.count?count<o.count:sum!=o.sum?sum<o.sum:xorValue<o.xorValue;}
    Word Hash()const{Word h=count;AddWord(h,sum);AddWord(h,xorValue);return h;}
};
class Builder
{
    struct Group{bool used;Key key;Side sides[2];Group():used(false){}};
    std::vector<Group> groups_;size_t count_;Word seed_;
public:
    enum{MaxItems=4096,MaxSections=128};
    Builder(size_t expected,Word inventory,bool rotate):count_(0),seed_(inventory)
    {
        size_t size=8;while(size<expected*2&&size<MaxItems*2)size*=2;
        if(expected>MaxItems)throw std::length_error("StackSort merge item bound");
        groups_.resize(size);AddWord(seed_,rotate?1:0);
    }
    bool Add(const Key& key,Word semantic,bool rotated)
    {
        if(count_>=MaxItems)return false;
        size_t at=(size_t)key.Hash()&(groups_.size()-1);
        for(size_t probe=0;probe<groups_.size();++probe,at=(at+1)&(groups_.size()-1))
        {
            Group& g=groups_[at];
            if(!g.used){g.used=true;g.key=key;}
            if(g.key==key){g.sides[rotated?1:0].Add(semantic);++count_;return true;}
        }
        return false;
    }
    Value Finish()const
    {
        Value out;out.a=Mix(seed_);out.b=Mix(seed_^0xd6e8feb86659fd93ui64);out.items=count_;
        for(size_t i=0;i<groups_.size();++i)if(groups_[i].used)
        {
            const Group& g=groups_[i];const bool reverse=g.sides[1]<g.sides[0];
            Word h=g.key.Hash();AddWord(h,g.sides[reverse?1:0].Hash());AddWord(h,g.sides[reverse?0:1].Hash());
            // Base-isSameAs group is a superset of ALL potentially compatible
            // ownership pairs. Side swap preserves their relative rotation.
            // Moving one member between sides changes the semantic stamp.
            out.a+=Mix(h);out.b^=Mix(h^0xa0761d6478bd642fui64);
        }
        return out;
    }
};
}
