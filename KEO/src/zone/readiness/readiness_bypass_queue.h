#pragma once
// Fixed diagnostic records only. One CAS try-gate; never wait or call native code.
#include <windows.h>
#include <stdint.h>

enum { READINESS_BYPASS_CAPACITY = 128, READINESS_BYPASS_DRAIN_MAX = 16 };
struct ReadinessBypassEvent
{
 LONGLONG firstQpc, lastQpc;
 uintptr_t returnRva;
 DWORD tid, repeats;
 int gx, gy, caller, readyClass, contractClass, sections, mainState;
 unsigned char onMain, justLoaded;
};
struct ReadinessBypassQueue
{
 volatile LONG gate, dropped, coalesced, discarded;
 unsigned read, write, count;
 ReadinessBypassEvent rows[READINESS_BYPASS_CAPACITY];
};
inline bool ReadinessBypassSame(const ReadinessBypassEvent& a, const ReadinessBypassEvent& b)
{
 return a.returnRva==b.returnRva && a.tid==b.tid && a.gx==b.gx && a.gy==b.gy &&
  a.caller==b.caller && a.readyClass==b.readyClass && a.contractClass==b.contractClass &&
  a.sections==b.sections && a.mainState==b.mainState && a.onMain==b.onMain && a.justLoaded==b.justLoaded;
}
inline bool ReadinessBypassPush(ReadinessBypassQueue* q, const ReadinessBypassEvent& event, bool enabled)
{
 if (!enabled) return false;
 if (InterlockedCompareExchange(&q->gate,1,0)!=0) { InterlockedIncrement(&q->dropped); return false; }
 if (q->count)
 {
  unsigned last=q->write ? q->write-1 : READINESS_BYPASS_CAPACITY-1;
  if (ReadinessBypassSame(q->rows[last],event))
  {
   if (q->rows[last].repeats!=0xFFFFFFFFu) ++q->rows[last].repeats;
   q->rows[last].lastQpc=event.lastQpc;
   InterlockedIncrement(&q->coalesced);
   InterlockedExchange(&q->gate,0);
   return true;
  }
 }
 if (q->count==READINESS_BYPASS_CAPACITY)
 {
  InterlockedIncrement(&q->dropped); InterlockedExchange(&q->gate,0); return false;
 }
 q->rows[q->write]=event;
 q->rows[q->write].repeats=1;
 q->write=(q->write+1)%READINESS_BYPASS_CAPACITY; ++q->count;
 InterlockedExchange(&q->gate,0);
 return true;
}
inline unsigned ReadinessBypassDrain(ReadinessBypassQueue* q, ReadinessBypassEvent* out, unsigned capacity)
{
 if (!out || !capacity || InterlockedCompareExchange(&q->gate,1,0)!=0) return 0;
 if (capacity>READINESS_BYPASS_DRAIN_MAX) capacity=READINESS_BYPASS_DRAIN_MAX;
 unsigned n=q->count<capacity?q->count:capacity;
 for (unsigned i=0;i<n;++i)
 {
  out[i]=q->rows[q->read]; q->read=(q->read+1)%READINESS_BYPASS_CAPACITY;
 }
 q->count-=n;
 InterlockedExchange(&q->gate,0);
 return n;
}
inline void ReadinessBypassDiscard(ReadinessBypassQueue* q)
{
 if (InterlockedCompareExchange(&q->gate,1,0)!=0) return;
 InterlockedExchangeAdd(&q->discarded,(LONG)q->count);
 q->read=q->write=q->count=0;
 InterlockedExchange(&q->gate,0);
}
