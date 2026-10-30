#pragma once
namespace PairTrace
{
typedef unsigned __int64 Key;
struct Context {void* gui;const void* section;Context():gui(0),section(0){} };
struct ScopedContext
{
    Context& slot;Context old;
    ScopedContext(Context& target,void* gui,const void* section):slot(target),old(target){slot.gui=gui;slot.section=section;}
    ~ScopedContext(){slot=old;}
};
class Gate
{
    struct Pair {Key key,state;unsigned long tick;bool used;Pair():key(0),state(0),tick(0),used(false){} };
    Pair pairs[64];unsigned long times[4];unsigned next,count,replacement;
public:
    Gate():next(0),count(0),replacement(0){for(int i=0;i<4;++i)times[i]=0;}
    bool Admit(Key key,Key state,unsigned long now)
    {
        Pair* found=0;
        for(int i=0;i<64;++i)if(pairs[i].used&&pairs[i].key==key){found=&pairs[i];break;}
        if(found && ((unsigned long)(now-found->tick)<250 ||
            (found->state==state && (unsigned long)(now-found->tick)<2000)))return false;
        if(count==4 && (unsigned long)(now-times[next])<1000)return false;
        times[next]=now;next=(next+1)%4;if(count<4)++count;
        if(!found){found=&pairs[replacement];replacement=(replacement+1)%64;}
        found->used=true;found->key=key;found->state=state;found->tick=now;return true;
    }
};
inline const char* Reason(bool nativeResult,bool finalResult,bool full)
{return !nativeResult?"OriginalReject":!finalResult?"RotateReject":full?"TargetFull":"Accepted";}
}
