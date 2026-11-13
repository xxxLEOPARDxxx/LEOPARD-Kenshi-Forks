#include "production_jobs_sdk.inc"
#include "production_jobs_abi.inc"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static unsigned calls = 0;
static float FakeScore(AI*, const hand&, const Ogre::Vector3&) { ++calls; return 1.0f; }

int main()
{
    typedef float (AI::*ScorePmf)(const hand&, const Ogre::Vector3&);
    ScorePmf score = &AI::scoreNone;
    unsigned char bytes[24] = {0};
    memcpy(bytes, &score, sizeof(score));
    printf("AI score PMF bytes=%u TaskData size=%u score=%u subtask=%u hasAction=%u\n",
           unsigned(sizeof(score)), unsigned(sizeof(TaskData)), unsigned(offsetof(TaskData,scoreFunction)),
           unsigned(offsetof(TaskData,subTask)), unsigned(offsetof(TaskData,hasAction)));
    if (sizeof(score)!=24 || offsetof(TaskData,scoreFunction)!=0x108 ||
        offsetof(TaskData,subTask)!=0x128 || offsetof(TaskData,hasAction)!=0x134) return 1;
    void* table[20] = {0}; table[19] = reinterpret_cast<void*>(&FakeScore);
    struct FakeAi {void** vptr;} object = {table};
    AI* ai = reinterpret_cast<AI*>(&object);
    // FakeScore ignores the reference; no native hand constructor is called.
    __declspec(align(8)) unsigned char targetStorage[sizeof(hand)] = {0};
    const hand& target = *reinterpret_cast<const hand*>(targetStorage);
    Ogre::Vector3 location(0,0,0);
    if ((ai->*score)(target,location)!=1.0f || calls!=1) return 2;
    typedef float (*NativeScore)(AI*, const hand&, const Ogre::Vector3&);
    NativeScore native; int adjustment;
    memcpy(&native,bytes,sizeof(native));memcpy(&adjustment,bytes+8,sizeof(adjustment));
    if (native(reinterpret_cast<AI*>(reinterpret_cast<char*>(ai)+adjustment),target,location)!=1.0f || calls!=2) return 3;
    printf("Actual SDK PMF passes native score invocation\n");
    return 0;
}
