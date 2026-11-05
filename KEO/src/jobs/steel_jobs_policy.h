#pragma once
#include <stdint.h>
namespace keo_jobs {
enum { GATHER_OUTPUT=291, WAYPOINT_ROUTE=292, STOCK_COUNTER=293,
 MAX_ROUTE_POINTS=64, MAX_STOCK_TYPES=64, MAX_STOCK_ATTEMPTS=8 };
struct Switches { bool gather,stock,waypoints; };
// Exact native handle identity words; never a hash or an AI/Widget pointer.
struct ActorId { uint32_t word[5]; };
struct JobToken { uint64_t epoch; ActorId actor; int type; uint32_t serial; uint64_t revision; };
struct RoutePoint { uint32_t serial; float x,y,z; };
struct RouteView { const RoutePoint*points; unsigned count; uint64_t revision; };
struct RouteState { uint64_t epoch; ActorId actor; uint32_t currentSerial; uint64_t revision; };
// Caller owns bounded actor slots. Copy/commit under its short state lock; never
// hold that lock across native callbacks. All views are immutable for this call.
bool SameActor(const ActorId&,const ActorId&);
bool TaskEnabled(int,const Switches&);
bool CanAssign(int,const Switches&,bool playerActor,bool validSubject,bool playerCounter);
bool SyncRoute(RouteState&,uint64_t,const ActorId&,const RouteView&);
float FilterScore(int,float nativeScore,const Switches&,const JobToken&,const RouteState*,bool eligibleCounter);
// valid must check current toggle, actor/row identity, epoch, cancellation and
// route revision. Context/native pointers are synchronous only, never retained.
struct BodyOps {void*ctx;bool(*valid)(void*,const JobToken&);bool(*completed)(void*);void(*nativeBody)(void*);};
enum BodyStatus { BODY_SKIPPED,BODY_RAN,BODY_ADVANCE,BODY_STALE };
struct BodyResult { BodyStatus status; uint32_t nextSerial; };
// Only291/292; native body is called once when admitted, never while OFF.
// Exceptions propagate without policy state writes. Native SEH remains adapter-owned.
BodyResult RunBody(const Switches&,const JobToken&,const RouteState*,const RouteView&,const BodyOps&);
// Caller rechecks live token and commits under its state lock. Rejects stale or
// duplicate completion, including same epoch/actor but changed route/selection.
bool CommitWaypoint(RouteState&,const JobToken&,const RouteView&,uint32_t nextSerial);
struct TypeView {const uint64_t*types;unsigned count;};
struct StockTypes {uint64_t types[MAX_STOCK_TYPES];unsigned count;};
// No native-resource fallback if any291 row exists, even with no product output.
bool SelectStockTypes(bool hasGatherRows,const TypeView& products,const TypeView& nativeResources,StockTypes&);
struct StockItem {uint64_t item,source,type;};
struct StockOps {
 void*ctx;bool(*valid)(void*,const JobToken&);bool(*ownedPlayerCounter)(void*);
 bool(*candidate)(void*,const StockTypes&,unsigned slot,StockItem*);
 bool(*canFit)(void*,const StockItem&);
 // Strong guarantee: false/exception/SEH leaves both inventories unchanged;
 // true transfers the original full item state exactly once. Native adapter
 // must prove this contract or refuse unchanged. No unproven detach+rollback.
 bool(*transferExact)(void*,const StockItem&);
};
struct StockResult {unsigned attempted,moved;bool stale;};
StockResult RunStock(const Switches&,const JobToken&,const StockTypes&,const StockOps&);
// Native hand payload (excluding vptr), with explicit sentinel supplied by caller.
struct WaypointSubject {uint32_t type,container,containerSerial,index,serial;};
bool EncodeWaypoint(uint32_t sentinel,const RoutePoint&,float maxAbsCoordinate,WaypointSubject&);
bool DecodeWaypoint(const WaypointSubject&,uint32_t sentinel,float maxAbsCoordinate,RoutePoint&);
}
