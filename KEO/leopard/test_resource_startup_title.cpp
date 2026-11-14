#include <windows.h>
#include <cstdint>
#include <cstdio>
struct Widget{bool visible;bool getVisible(){return visible;}};
class TitleScreen{public:Widget*win;Widget*mMainWidget;static TitleScreen*instance;__declspec(noinline) static TitleScreen*getSingleton(){return instance;}};
TitleScreen*TitleScreen::instance=NULL;
static bool mainThread=true;LONG g_navMeshStopSeen=0;bool IsMainThread(){return mainThread;}
template<typename T> intptr_t KlibRealAddress(T p){return intptr_t(p);}
#include "title-current.inc"
int main(){Widget w={true};TitleScreen title={NULL,&w};TitleScreen::instance=&title;
 if(!ResourceStartupTitleVisible()){puts("FAIL: actual BaseLayout title visible, GUIWindow win NULL");return 1;}
 w.visible=false;if(ResourceStartupTitleVisible())return 2;
 w.visible=true;mainThread=false;if(ResourceStartupTitleVisible())return 3;mainThread=true;
 g_navMeshStopSeen=1;if(ResourceStartupTitleVisible())return 4;g_navMeshStopSeen=0;
 TitleScreen::instance=NULL;if(ResourceStartupTitleVisible())return 5;
 puts("actual title helper: visible BaseLayout, hide, main thread, stop and missing singleton PASS");return 0;}
