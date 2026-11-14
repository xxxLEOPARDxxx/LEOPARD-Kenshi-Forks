// Native HUD implementation is included verbatim after replacing only SDK
// includes with this bounded ownership/unlink/renderer boundary model.
#include "resources/resource_warm_hud_policy.h"
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdio>
static unsigned clockMs=0,checks=0;static bool mainThread=true;
static int live=0,created=0,captions=0,coords=0,errors=0,makeFail=0,measurements=0;
static int skinX=0,skinY=0,glyphWidth=9,nativeHeight=18;
static bool resourcesReady=false,fontsReady=false;
unsigned GetTickCount(){return clockMs;}
bool IsMainThread(){return mainThread;}
void LogError(const std::string&){++errors;}
const char* KeoTr(const char* s){return s;}
namespace MyGUI {
struct IntCoord {
    int left,top,width,height;
    IntCoord(int a=0,int b=0,int c=0,int d=0):left(a),top(b),width(c),height(d){}
    bool operator!=(const IntCoord& b)const{return left!=b.left||top!=b.top||width!=b.width||height!=b.height;}
};
struct IntSize {int width,height;IntSize(int a=0,int b=0):width(a),height(b){}};
struct Align {enum {Default=0,Left=1,Top=2};};
struct Colour {Colour(float,float,float){}};
struct IFont {};
struct ResourceManager {static ResourceManager* getInstancePtr(){static ResourceManager r;return &r;}bool isExist(const std::string& s){return resourcesReady&&s=="Kenshi_StandardFont_Medium";}};
struct SkinManager {static SkinManager* getInstancePtr(){static SkinManager r;return &r;}bool isExist(const std::string& s){return resourcesReady&&(s=="Kenshi_FrameSkin"||s=="Kenshi_TextBoxEmptySkin");}};
struct FontManager {static FontManager* getInstancePtr(){static FontManager r;return &r;}IFont* getByName(const std::string& s){static IFont font;return fontsReady&&s=="Kenshi_StandardFont_Medium"?&font:NULL;}};
struct UString {std::string text;UString(const std::string& s):text(s){}};
struct Widget;
struct IUnlinkWidget {virtual void _unlinkWidget(Widget*)=0;};
struct WidgetManager {
    static WidgetManager* instance;std::vector<IUnlinkWidget*> refs;
    static WidgetManager* getInstancePtr(){return instance;}
    void registerUnlinker(IUnlinkWidget* p){refs.push_back(p);}
    void unregisterUnlinker(IUnlinkWidget* p){for(size_t i=0;i<refs.size();++i)if(refs[i]==p){refs.erase(refs.begin()+i);return;}}
    void Unlink(Widget* p){for(size_t i=0;i<refs.size();++i)refs[i]->_unlinkWidget(p);}
};WidgetManager* WidgetManager::instance=NULL;
struct Widget {
    std::vector<Widget*> children;IntCoord coord;bool visible,textPart;WidgetManager* owner;
    Widget():visible(true),textPart(false),owner(WidgetManager::instance){++live;++created;}
    virtual ~Widget(){while(!children.empty()){delete children.back();children.pop_back();}owner->Unlink(this);--live;}
    template<class T>T* createWidget(const char* skin,const IntCoord& c,int,const char*){if(makeFail&&--makeFail==0)throw std::runtime_error("creation");T* w=new T;w->textPart=!std::strcmp(skin,"Kenshi_TextBoxEmptySkin");w->setCoord(c);children.push_back(w);return w;}
    void setNeedMouseFocus(bool){}void setNeedKeyFocus(bool){}
    void setCoord(const IntCoord& c){coord=c;++coords;}const IntCoord& getCoord(){return coord;}
    bool getVisible(){return visible;}void setVisible(bool v){visible=v;}
    int getWidth(){return coord.width;}int getHeight(){return coord.height;}
};
struct TextBox:Widget {
    std::string caption,font;
    void setFontName(const std::string& s){if(textPart&&FontManager::getInstancePtr()->getByName(s))font=s;}
    void setFontHeight(int){if(font.empty())throw std::runtime_error("font missing before height");}void setTextAlign(int){}void setTextColour(Colour){}void setTextShadow(bool){}void setTextShadowColour(Colour){}
    void setCaption(UString text){caption=text.text;++captions;}
    IntSize getTextSize(){if(font.empty())throw std::runtime_error("font missing before measurement");++measurements;return IntSize(int(caption.size())*glyphWidth,nativeHeight);}
    IntCoord getTextRegion(){return IntCoord(skinX/2,skinY/2,getWidth()-skinX,getHeight()-skinY);}
};
struct Gui {
    static Gui* instance;std::vector<Widget*> roots;
    static Gui* getInstancePtr(){return instance;}
    template<class T>T* createWidget(const char* skin,const IntCoord& c,int,const char*,const char*){if(makeFail&&--makeFail==0)throw std::runtime_error("creation");T* w=new T;w->textPart=!std::strcmp(skin,"Kenshi_TextBoxEmptySkin");w->setCoord(c);roots.push_back(w);return w;}
    void destroyWidget(Widget* p){for(size_t i=0;i<roots.size();++i)if(roots[i]==p){roots.erase(roots.begin()+i);break;}delete p;}
};Gui* Gui::instance=NULL;
struct RenderManager {
    static RenderManager* instance;IntSize view;
    RenderManager():view(1920,1080){}
    static RenderManager* getInstancePtr(){return instance;}
    static RenderManager& getInstance(){return *instance;}
    IntSize getViewSize(){return view;}
};RenderManager* RenderManager::instance=NULL;
}
// Native changeFontSize forwards getFontName() back to setFontName().
// Fontless EditText returns the invalid string address 0x8 in the real DLL.
static bool NativeFontRefreshSafe(MyGUI::Gui& gui){
    for(size_t i=0;i<gui.roots.size();++i){
        MyGUI::TextBox* row=dynamic_cast<MyGUI::TextBox*>(gui.roots[i]);
        if(!row||row->font.empty())return false;
        const std::string name=row->font;row->setFontName(name);
        if(row->font.empty())return false;
    }
    return true;
}
#include "resource_warm_hud_actual_runtime.inc"
#define C(x) do{++checks;if(!(x)){printf("FAIL %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    resource_warm_hud::Snapshot s={};s.ramOn=s.gpuOn=true;s.ramCap=8ULL<<30;s.gpuCap=1ULL<<30;
    MyGUI::Gui gui;MyGUI::WidgetManager manager;MyGUI::RenderManager renderer;
    ResourceWarmHudTick(true,s);C(live==0); // no GUI, no calls into renderer
    MyGUI::Gui::instance=&gui;MyGUI::WidgetManager::instance=&manager;MyGUI::RenderManager::instance=&renderer;
    ResourceWarmHudTick(false,s);C(live==0&&manager.refs.empty());
    ResourceWarmHudTick(true,s);C(live==0&&errors==0&&manager.refs.empty()); // singleton exists, resources absent
    resourcesReady=true;clockMs+=250;ResourceWarmHudTick(true,s);C(live==0&&errors==0&&manager.refs.empty()); // font resource present, no native font yet
    fontsReady=true;clockMs=0;
    ResourceWarmHudTick(true,s);C(live==3&&created==3&&captions==3&&manager.refs.size()==1);C(state.rows[0]->getCoord().top==96);C(gui.roots.size()==3);
    C(state.rows[0]->font=="Kenshi_StandardFont_Medium"&&state.rows[1]->textPart&&state.rows[2]->textPart);
    C(NativeFontRefreshSafe(gui));
    clockMs=249;ResourceWarmHudTick(true,s);C(captions==3&&created==3&&measurements==3);
    const int oldCoords=coords;clockMs=250;ResourceWarmHudTick(true,s);C(captions==3&&coords==oldCoords);
    s.ramPending=12;clockMs=500;ResourceWarmHudTick(true,s);C(captions==5&&created==3);
    renderer.view=MyGUI::IntSize(400,160);clockMs=750;ResourceWarmHudTick(true,s);
    C(state.rows[0]->getCoord().left+state.rows[0]->getWidth()<=400);C(state.rows[2]->getCoord().top+state.rows[2]->getHeight()<=160);
    // Different native/modded text skin insets and line heights must fit.
    skinX=12;skinY=10;nativeHeight=30;glyphWidth=11;clockMs=1000;ResourceWarmHudTick(true,s);
    C(state.rows[0]->getHeight()>=40);C(state.rows[2]->getCoord().top+state.rows[2]->getHeight()<=160);
    renderer.view=MyGUI::IntSize(1920,1080);clockMs=1250;ResourceWarmHudTick(true,s);
    C(state.rows[2]->getCoord().top>=state.rows[1]->getCoord().top+state.rows[1]->getHeight());C(state.rows[2]->getCoord().top+state.rows[2]->getHeight()<=1080);
    MyGUI::Widget* old=state.rows[0];gui.destroyWidget(old);C(live==2&&state.rows[0]==NULL);
    clockMs=1500;ResourceWarmHudTick(true,s);C(live==3&&manager.refs.size()==1);
    ResourceWarmHudCleanup();C(live==0&&manager.refs.empty());C(!state.sampled);
    ResourceWarmHudCleanup();ResourceWarmHudGuiGone();C(live==0&&manager.refs.empty());
    ResourceWarmHudTick(true,s);C(live==3);ResourceWarmHudTick(false,s);C(live==0&&manager.refs.empty());
    makeFail=3;ResourceWarmHudTick(true,s);C(live==0&&manager.refs.empty()&&errors==1);C(gui.roots.empty()&&NativeFontRefreshSafe(gui));
    clockMs+=4999;ResourceWarmHudTick(true,s);C(live==0&&errors==1);clockMs+=1;ResourceWarmHudTick(true,s);C(live==3&&errors==1);
    mainThread=false;ResourceWarmHudTick(false,s);C(live==3);mainThread=true;ResourceWarmHudGuiGone();C(live==0&&manager.refs.empty());
    // A new manager/GUI after shutdown must register exactly once, not retain
    // the old manager's registration bit (even if addresses get reused).
    ResourceWarmHudTick(true,s);for(int i=0;i<3;++i)gui.destroyWidget(state.rows[i]);MyGUI::Gui::instance=NULL;MyGUI::WidgetManager::instance=NULL;
    ResourceWarmHudGuiGone();C(live==0&&state.manager==NULL);
    manager.refs.clear();MyGUI::Gui::instance=&gui;MyGUI::WidgetManager::instance=&manager;ResourceWarmHudTick(true,s);
    C(live==3&&manager.refs.size()==1);C(NativeFontRefreshSafe(gui));
    resourcesReady=false;clockMs+=250;ResourceWarmHudTick(true,s);C(live==0&&manager.refs.empty());
    resourcesReady=true;fontsReady=false;clockMs+=250;ResourceWarmHudTick(true,s);C(live==0&&errors==1);
    fontsReady=true;ResourceWarmHudTick(true,s);C(live==3&&NativeFontRefreshSafe(gui));
    ResourceWarmHudCleanup();C(live==0&&manager.refs.empty());
    printf("Resource warm HUD actual runtime boundary %u checks PASS\n",checks);return 0;
}
