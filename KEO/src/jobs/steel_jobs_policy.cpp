#include "jobs/steel_jobs_policy.h"
#include <float.h>
#include <string.h>
namespace keo_jobs {
namespace {
bool Finite(float f){return f>=-FLT_MAX&&f<=FLT_MAX;}
bool ValidRoute(const RouteView&r){
 if(r.count>MAX_ROUTE_POINTS||(r.count&&!r.points))return false;
 for(unsigned i=0;i<r.count;++i){const RoutePoint&p=r.points[i];
  if(!p.serial||!Finite(p.x)||!Finite(p.y)||!Finite(p.z))return false;
  for(unsigned j=0;j<i;++j)if(r.points[j].serial==p.serial)return false;
 }return true;
}
int Index(const RouteView&r,uint32_t serial){for(unsigned i=0;i<r.count;++i)if(r.points[i].serial==serial)return (int)i;return -1;}
bool Matches(const RouteState&s,const JobToken&t){return s.epoch==t.epoch&&t.epoch&&SameActor(s.actor,t.actor)&&t.serial&&s.currentSerial==t.serial&&s.revision==t.revision;}
bool Contains(const StockTypes&t,uint64_t id){for(unsigned i=0;i<t.count;++i)if(t.types[i]==id)return true;return false;}
bool Bounded(const RoutePoint&p,float limit){return p.serial&&limit>0&&Finite(limit)&&Finite(p.x)&&Finite(p.y)&&Finite(p.z)&&p.x>=-limit&&p.x<=limit&&p.y>=-limit&&p.y<=limit&&p.z>=-limit&&p.z<=limit;}
}
bool SameActor(const ActorId&a,const ActorId&b){for(unsigned i=0;i<5;++i)if(a.word[i]!=b.word[i])return false;return true;}
bool TaskEnabled(int type,const Switches&s){return type==GATHER_OUTPUT?s.gather:type==STOCK_COUNTER?s.stock:type==WAYPOINT_ROUTE?s.waypoints:false;}
bool CanAssign(int type,const Switches&s,bool player,bool valid,bool counter){return TaskEnabled(type,s)&&player&&valid&&(type!=STOCK_COUNTER||counter);}
bool SyncRoute(RouteState&s,uint64_t epoch,const ActorId&actor,const RouteView&r){
 if(!epoch||!ValidRoute(r))return false;
 uint32_t selected=s.epoch==epoch&&SameActor(s.actor,actor)?s.currentSerial:0;
 if(Index(r,selected)<0)selected=r.count?r.points[0].serial:0;
 s.epoch=epoch;s.actor=actor;s.currentSerial=selected;s.revision=r.revision;return true;
}
float FilterScore(int type,float native,const Switches&s,const JobToken&t,const RouteState*state,bool counter){
 if(type!=GATHER_OUTPUT&&type!=STOCK_COUNTER&&type!=WAYPOINT_ROUTE)return native;
 if(!TaskEnabled(type,s)||t.type!=type||!t.epoch)return 0;
 if(type==STOCK_COUNTER)return counter?native:0;
 if(type==WAYPOINT_ROUTE)return state&&Matches(*state,t)?native:0;
 return native;
}
BodyResult RunBody(const Switches&s,const JobToken&t,const RouteState*state,const RouteView&r,const BodyOps&o){
 BodyResult result={BODY_SKIPPED,0};
 if((t.type!=GATHER_OUTPUT&&t.type!=WAYPOINT_ROUTE)||!TaskEnabled(t.type,s)||!t.epoch||!o.valid||!o.nativeBody)return result;
 if(!o.valid(o.ctx,t)){result.status=BODY_STALE;return result;}
 bool before=false;
 if(t.type==WAYPOINT_ROUTE){if(!state||!Matches(*state,t)||!ValidRoute(r)||r.revision!=t.revision||Index(r,t.serial)<0||!o.completed)return result;before=o.completed(o.ctx);}
 o.nativeBody(o.ctx);result.status=BODY_RAN;
 if(!o.valid(o.ctx,t)){result.status=BODY_STALE;return result;}
 if(t.type==WAYPOINT_ROUTE&&!before&&o.completed(o.ctx)){
  int i=Index(r,t.serial);result.status=BODY_ADVANCE;result.nextSerial=r.points[((unsigned)i+1)%r.count].serial;
 }return result;
}
bool CommitWaypoint(RouteState&s,const JobToken&t,const RouteView&r,uint32_t next){
 if(t.type!=WAYPOINT_ROUTE||!Matches(s,t)||r.revision!=t.revision||!ValidRoute(r))return false;
 int i=Index(r,t.serial);if(i<0||r.points[((unsigned)i+1)%r.count].serial!=next)return false;
 s.currentSerial=next;return true;
}
bool SelectStockTypes(bool gathers,const TypeView&products,const TypeView&native,StockTypes&out){
 out.count=0;const TypeView&v=gathers?products:native;
 if(v.count>MAX_STOCK_TYPES||(v.count&&!v.types))return false;
 for(unsigned i=0;i<v.count;++i){if(!v.types[i]){out.count=0;return false;}if(!Contains(out,v.types[i]))out.types[out.count++]=v.types[i];}return true;
}
StockResult RunStock(const Switches&s,const JobToken&t,const StockTypes&types,const StockOps&o){
 StockResult r={0,0,false};
 if(t.type!=STOCK_COUNTER||!s.stock||!t.epoch||!types.count||types.count>MAX_STOCK_TYPES||!o.valid||!o.ownedPlayerCounter||!o.candidate||!o.canFit||!o.transferExact)return r;
 for(unsigned slot=0;slot<MAX_STOCK_ATTEMPTS;++slot){
  if(!o.valid(o.ctx,t)||!o.ownedPlayerCounter(o.ctx)){r.stale=true;break;}
  StockItem item={};bool candidate=o.candidate(o.ctx,types,slot,&item);
  if(!o.valid(o.ctx,t)||!o.ownedPlayerCounter(o.ctx)){r.stale=true;break;}
  if(!candidate)break;++r.attempted;
  if(!item.item||!item.source||!item.type||!Contains(types,item.type))continue;
  bool fits=o.canFit(o.ctx,item);
  if(!o.valid(o.ctx,t)||!o.ownedPlayerCounter(o.ctx)){r.stale=true;break;}
  if(!fits)continue;
  if(o.transferExact(o.ctx,item))++r.moved;
  if(!o.valid(o.ctx,t)||!o.ownedPlayerCounter(o.ctx)){r.stale=true;break;}
 }return r;
}
bool EncodeWaypoint(uint32_t sentinel,const RoutePoint&p,float limit,WaypointSubject&out){
 if(!sentinel||!Bounded(p,limit))return false;WaypointSubject s={sentinel,0,0,0,p.serial};
 memcpy(&s.container,&p.x,4);memcpy(&s.containerSerial,&p.y,4);memcpy(&s.index,&p.z,4);out=s;return true;
}
bool DecodeWaypoint(const WaypointSubject&s,uint32_t sentinel,float limit,RoutePoint&out){
 if(!sentinel||s.type!=sentinel)return false;RoutePoint p={s.serial,0,0,0};
 memcpy(&p.x,&s.container,4);memcpy(&p.y,&s.containerSerial,4);memcpy(&p.z,&s.index,4);
 if(!Bounded(p,limit))return false;out=p;return true;
}
}
