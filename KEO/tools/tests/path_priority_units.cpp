#include "pathfind/path_priority_publication.h"
#include "check.h"
#include <cstring>
int main()
{
    for(int mask=0;mask<64;++mask)
        Check(PathPriorityCapability((mask&1)!=0,(mask&2)!=0,(mask&4)!=0,(mask&8)!=0,(mask&16)!=0,(mask&32)!=0)==(mask==63),"capability requires every hook/profile/original and wanted state");
    unsigned char request[160],before[160];std::memset(request,0xA5,sizeof(request));int kind=0,priority=20;std::memcpy(request+40,&kind,4);std::memcpy(request+44,&priority,4);std::memcpy(before,request,sizeof(request));
    unsigned char queue;void* item=request;PathPriorityContext* slot=NULL;PathPriorityFrame f;PathPriorityDecision d;
    PathPriorityEnter(&f,slot,request,&queue,(void*)0x3AAF60,45,false,true,true);
    Check(slot==&f.current&&!slot->consumed,"fresh stack context admitted once");
    Check(!PathPriorityApply(slot,&queue,&item,(void*)0x3AAFD5,&d),"cancel return cannot borrow submit");
    Check(PathPriorityApply(slot,&queue,&item,(void*)0x3AAF60,&d)&&!d.repath,"exact input publication applies order tier");
    int result=0;std::memcpy(&result,request+44,4);Check(result==45,"field carries45 before caller publishes it");std::memcpy(before+44,&result,4);Check(!std::memcmp(before,request,sizeof(request)),"all request/water/handle/result bytes except priority unchanged");
    Check(!PathPriorityApply(slot,&queue,&item,(void*)0x3AAF60,&d),"same invocation cannot publish boost twice");PathPriorityLeave(&f,slot);Check(slot==NULL,"scope returns to no owner");
    PathPriorityContext parent={};slot=&parent;PathPriorityEnter(&f,slot,request,&queue,(void*)0x3AAF60,0,false,false,true);slot->request=(void*)1;void*bad=(void*)1;
    Check(!PathPriorityApply(slot,&queue,&bad,(void*)0x3AAF60,&d),"NPC tier refuses before reading native payload");PathPriorityLeave(&f,slot);Check(slot==&parent,"nested leave restores exact parent");
    Check(!PathPriorityProfile(NULL,NULL),"missing internal-profile reader fails closed");
    return CheckExit("path_priority_units");
}
