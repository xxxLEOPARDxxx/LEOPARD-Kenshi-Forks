#include "resources/resource_warm_hud.h"
#include "resources/resource_warm_hud_policy.h"
#include "base/core.h"
#include "base/keo_tr.h"
#include "base/klib_include.h"
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_WidgetManager.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_ResourceManager.h>
#include <mygui/MyGUI_SkinManager.h>
#include <mygui/MyGUI_FontManager.h>
#include "base/klib_include_end.h"

namespace {
// Like shared WidgetRef: native unlink notifications, never a GUI tree search.
// One observer tracks three independent text widgets, with no panel/frame.
struct HudState : MyGUI::IUnlinkWidget {
    MyGUI::Gui* gui;
    MyGUI::WidgetManager* manager;
    MyGUI::TextBox* rows[3];
    std::string captions[3];
    resource_warm_hud::Sampler sampler;
    unsigned at,failedAt;
    bool sampled,failed;
    HudState():gui(NULL),manager(NULL),at(0),failedAt(0),sampled(false),failed(false) { for(int i=0;i<3;++i)rows[i]=NULL; }
    virtual void _unlinkWidget(MyGUI::Widget* widget) {
        for(int i=0;i<3;++i)if(widget==rows[i])rows[i]=NULL;
    }
    bool Complete() const {return rows[0]&&rows[1]&&rows[2];}
    void Clear() {
        MyGUI::Gui* liveGui=MyGUI::Gui::getInstancePtr();
        MyGUI::WidgetManager* liveManager=MyGUI::WidgetManager::getInstancePtr();
        // A cached widget is dereferenceable only while its unlink observer's
        // original manager and GUI still exist. Otherwise forget stale values.
        if(gui==liveGui&&manager==liveManager)for(int i=0;i<3;++i)if(rows[i])liveGui->destroyWidget(rows[i]);
        if(manager&&manager==liveManager)manager->unregisterUnlinker(this);
        gui=NULL;manager=NULL;
        for(int i=0;i<3;++i){rows[i]=NULL;captions[i].clear();}
        sampler.Reset();sampled=false;
    }
    void Create(MyGUI::Gui* liveGui,MyGUI::WidgetManager* liveManager) {
        gui=liveGui;manager=liveManager;manager->registerUnlinker(this);
        for(int i=0;i<3;++i) {
            rows[i]=gui->createWidget<MyGUI::TextBox>("Kenshi_TextBoxEmptySkin",MyGUI::IntCoord(16,96+i*24,800,24),MyGUI::Align::Default,"ToolTip","");
            rows[i]->setVisible(false);
            rows[i]->setNeedMouseFocus(false);rows[i]->setNeedKeyFocus(false);
            // Kenshi_TextboxStandardText is a ResourceLayout, not a skin. Its
            // template assigns this font explicitly; direct creation must too.
            // A fontless text part later crashes native changeFontSize refresh.
            rows[i]->setFontName("Kenshi_StandardFont_Medium");
            rows[i]->setFontHeight(18);rows[i]->setTextAlign(MyGUI::Align::Left|MyGUI::Align::Top);
            rows[i]->setTextColour(MyGUI::Colour(1,1,1));rows[i]->setTextShadow(true);
            rows[i]->setTextShadowColour(MyGUI::Colour(0,0,0));
        }
    }
    void Paint(const resource_warm_hud::Snapshot& snapshot,unsigned now) {
        const resource_warm_hud::Lines lines=resource_warm_hud::Format(snapshot,sampler.Observe(now,snapshot.ramReadBytes),KeoTr);
        int textWidth=0,lineHeight=24;
        for(int i=0;i<3;++i) {
            if(captions[i]!=lines.text[i]){rows[i]->setCaption(MyGUI::UString(lines.text[i]));captions[i]=lines.text[i];}
            const MyGUI::IntSize size=rows[i]->getTextSize();
            const MyGUI::IntCoord region=rows[i]->getTextRegion();
            textWidth=resource_warm_hud::Maximum(textWidth,size.width+resource_warm_hud::Maximum(0,rows[i]->getWidth()-region.width));
            lineHeight=resource_warm_hud::Maximum(lineHeight,size.height+resource_warm_hud::Maximum(0,rows[i]->getHeight()-region.height));
        }
        const MyGUI::IntSize view=MyGUI::RenderManager::getInstance().getViewSize();
        const resource_warm_hud::Layout layout=resource_warm_hud::Place(view.width,view.height,textWidth,lineHeight);
        for(int i=0;i<3;++i) {
            const MyGUI::IntCoord row(layout.left,layout.top+i*lineHeight,layout.width,resource_warm_hud::Minimum(lineHeight,resource_warm_hud::Maximum(0,view.height-layout.top-i*lineHeight)));
            if(rows[i]->getCoord()!=row)rows[i]->setCoord(row);
            if(!rows[i]->getVisible())rows[i]->setVisible(true);
        }
    }
} state;
}

void ResourceWarmHudTick(bool enabled,const resource_warm_hud::Snapshot& snapshot)
{
    if(!IsMainThread())return;
    if(!enabled){if(state.manager)state.Clear();state.failed=false;return;}
    MyGUI::Gui* gui=MyGUI::Gui::getInstancePtr();
    MyGUI::WidgetManager* manager=MyGUI::WidgetManager::getInstancePtr();
    if(!gui||!manager||!MyGUI::RenderManager::getInstancePtr()){state.Clear();return;}
    if(state.gui!=gui||state.manager!=manager)state.Clear();
    const unsigned now=GetTickCount();
    if(state.failed&&unsigned(now-state.failedAt)<5000)return;
    if(!resource_warm_hud::Due(now,state.at,state.sampled))return;
    try {
        // Constructor-published singleton pointers alone do not admit native
        // widget/font calls. Resource identity must be present and typed as a
        // real skin/font, not merely a template with a similar name.
        MyGUI::ResourceManager* resources=MyGUI::ResourceManager::getInstancePtr();
        MyGUI::SkinManager* skins=MyGUI::SkinManager::getInstancePtr();
        MyGUI::FontManager* fonts=MyGUI::FontManager::getInstancePtr();
        if(!resources||!skins||!fonts||!skins->isExist("Kenshi_TextBoxEmptySkin")||
            !resources->isExist("Kenshi_StandardFont_Medium")||!fonts->getByName("Kenshi_StandardFont_Medium")){
            state.Clear();return;
        }
        if(!state.Complete()){state.Clear();state.Create(gui,manager);}
        state.Paint(snapshot,now);state.at=now;state.sampled=true;state.failed=false;
    } catch(...) {
        state.Clear();state.failed=true;state.failedAt=now;
        LogError("Resource warmup HUD: native widget creation/update failed; retrying in five seconds");
    }
}
void ResourceWarmHudCleanup(){if(IsMainThread()){state.Clear();state.failed=false;}}
void ResourceWarmHudGuiGone(){ResourceWarmHudCleanup();}
