"""Keep path search, mesh construction and zone registration in the game."""
from pathlib import Path
import os
import re
import subprocess
import unittest

root = Path(__file__).resolve().parents[2]

def read(path):
    return (root / path).read_text(encoding="utf-8")

class StockNavigationTests(unittest.TestCase):
    def test_runtime_scope(self):
        rows = set(re.findall(r"\bHOOK_ROW\(HOOK_(\w+),", read("src/plugin/hook_manifest_rows.inc")))
        forbidden_rows = set("""
        SPEED_GROUP_GET_SPEED ISINISLAND_IMPL GETISLAND_IMPL SET_DESTINATION_VEC3 GET_ZONE_EDGE
        CHARMOVEMENT_UPDATE STOP_CHARACTERS_MOVEMENT ADD_JOB_SELECTED ADD_TASK_NEAREST CS_FIND_PATH
        CS_CHECK_FACE_CONN FIND_PATH_FULL REQUEST_PATH PATH_REQ_SUBMIT CS_FIND_PATH_FALLBACK
        CONTENT_STREAM_CALLEE_0X8869 CONTENT_STREAM DEQUEUE_WORK ENQUEUE_THREAD_SAFE GATES_UPDATE_CODES
        GATES_FIND_PATH GRAPH_INSTANCE_CONNECT
        """.split())
        assert not rows & forbidden_rows, sorted(rows & forbidden_rows)
        assert {"ADD_ORDER_SELECTED", "NAVMESH_STOP", "NAVMESH_GENERATE_ZONEMAP", "DISPATCH_JOB",
                "UPDATE_CAMERA_ZONE", "SHOW_LOADING_MESSAGE", "DESTROYLIST_INSERT", "SFS_LOAD_GAME"} <= rows
        build_units = set(read("tools/build/coresrc.txt").replace("\\", "/").splitlines())
        assert not any(line.startswith(("src/pathfind/", "src/planner/")) for line in build_units)
        assert {line for line in build_units if line.startswith("src/movement/")} == {
            "src/movement/tracking.cpp", "src/movement/mover_policy.cpp"
        }
        assert {"src/navmesh/nm_lazy_hooks.cpp", "src/zone/preload/preload_order.cpp",
                "src/resources/resource_cache.cpp", "src/jobs/steel_jobs.cpp"} <= build_units
        for path in ("src/plugin/hook_manifest.cpp", "src/plugin/plugin_entry.cpp", "src/zone/camera_zone_hook.cpp"):
            text = read(path)
            for name in ("PathPriorityPrepare", "PathPriorityFinalize", "InstallPathfindHooks", "InstallPathPoolHooks",
                         "InstallScatterPatch", "PlannerTick", "IslandTick", "PollFormationGroups"):
                assert name not in text, (path, name)
        layout = read("src/gui/settings_layout.cpp")
        assert '"Squad movement"' not in layout
        for key in ("navmeshWorkerCount", "navmeshDiskCacheMaxMB", "autoNavmeshRepair", "resourceStartupTextures",
                    "saveReadBuffer", "waypointRouteJob"):
            assert f'"{key}"' in layout,key
        print("Native route manifest, build inputs, settings and preserved features PASS")

    def test_order_forwarding(self):
        # Exercise the real ordinary-order boundary without loading the game DLL.
        # Zone observation is mocked independently here so it can throw consistently.
        source = read("src/zone/preload/preload_order.cpp")
        entry = source[source.index("void hook_addOrderSelected("):]
        fixture = r"""
        #include <cassert>
        #include <stdexcept>
        #include <cstdio>
        const int MoveTo=29;
        static int observed=0, warned=0, forwarded=0, notified=0;
        static bool consume=false, fail=false;
        static void* player=(void*)1;
        static void* building=(void*)2;
        static void* subject=(void*)3;
        static const float target[]={11.f,22.f,33.f};
        static int nativeTask=0;
        static bool nativeShift=false, nativeDontClear=false;
        static const float* nativeTarget=0;
        namespace keo_jobs {
            bool SteelJobsConsumeMove(void* p,void* b,void* s,const float* v,bool shift) {
                assert(p==player && b==building && s==subject && v==target);
                return consume && shift;
            }
        }
        void BenchNotifyPlayerOrder(){ ++notified; }
        void ObserveStreamingTarget(void* p,const float* v){
            assert(p==player && v==target); ++observed;
            if(fail) throw std::runtime_error("observer refused");
        }
        void LogErrorDeferrable(const char*) { ++warned; }
        namespace game {
            void Native(void* p,void* b,int task,void* s,bool shift,bool dontClear,const float* v) {
                assert(p==player && b==building && s==subject);
                ++forwarded;nativeTask=task;nativeShift=shift;nativeDontClear=dontClear;nativeTarget=v;
            }
            struct Originals { decltype(&Native) orig_addOrderSelected; } g_hookOrig={Native};
        }
        """
        checks = r"""
        int main(){
            hook_addOrderSelected(player,building,MoveTo,subject,false,true,target);
            assert(forwarded==1 && nativeTask==MoveTo && !nativeShift && nativeDontClear && nativeTarget==target);
            fail=true;
            hook_addOrderSelected(player,building,MoveTo,subject,true,false,target);
            assert(forwarded==2 && nativeShift && !nativeDontClear && nativeTarget==target && observed==2 && warned==1);
            hook_addOrderSelected(player,building,6,subject,false,false,target);
            assert(forwarded==3 && nativeTask==6 && observed==2 && notified==3);
            consume=true;
            hook_addOrderSelected(player,building,MoveTo,subject,true,true,target);
            assert(forwarded==3 && notified==3 && observed==2);
            puts("Ordinary orders, observer failure, and Steel consumption PASS");
        }
        """
        out = root / "build/stock-navigation"
        out.mkdir(parents=True, exist_ok=True)
        unit = out / "order_boundary.cpp"
        unit.write_text(fixture + entry + checks, encoding="utf-8")
        vc = Path("C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC")
        sdk = Path("C:/Program Files/Microsoft SDKs/Windows/v7.1")
        env = dict(os.environ, INCLUDE=f"{vc}/include;{sdk}/Include", LIB=f"{vc}/lib/amd64;{sdk}/Lib/x64")
        for optimization in ("Od", "O2"):
            output = out / f"order-{optimization}.exe"
            subprocess.run([str(vc / "bin/amd64/cl.exe"), "/nologo", "/EHsc", f"/{optimization}",
                            f"/Fo{out}/order-{optimization}.obj", f"/Fe{output}", str(unit)], env=env, check=True)
            subprocess.run([str(output)], check=True)


if __name__ == "__main__":
    unittest.main()
