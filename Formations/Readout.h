#pragma once
#include "ReadoutLayout.h"

// Popup style keeps a native parent for lifetime/visibility but uses
// screen coordinates. Place follows the animated parent without clipping.
namespace formation_hud
{
    using readout::Rect;
    using readout::Size;
    struct Point {int x,y;};
    using MainBarUpdate=void(*)(void*);
    using GetWidget=void*(*)(void*,const EngineString*);
    using CreatePopup=void*(*)(void*,int,const EngineString*,const EngineString*,const Rect*,int,const EngineString*,const EngineString*);
    using CreateChild=void*(*)(void*,const EngineString*,const EngineString*,int,int,int,int,int,const EngineString*);
    using DestroyWidget=void(*)(void*,void*);
    using SetProperty=void(*)(void*,const EngineString*,const EngineString*);
    using SetCoord=void(*)(void*,int,int,int,int);
    using IsVisible=bool(*)(const void*);
    using Position=const Point*(*)(const void*);
    using MeasureText=Size*(*)(void*,Size*);
    using RegisterUnlinker=void(*)(void*,void*);
    using SetCaption=void(*)(void*,const EngineString*);
    using SetFontHeight=void(*)(void*,int);

    // This is the two-slot MyGUI IUnlinkWidget interface. The manager borrows
    // this process-lifetime object and calls the second slot on destruction.
    struct Widgets
    {
        virtual ~Widgets() {}
        virtual void unlink(void* widget)
        {
            if(widget==name)name=nullptr;
            if(widget==bounty)bounty=nullptr;
            if(widget==panel){panel=nullptr;backing=nullptr;text=nullptr;shown=false;}
            if(widget==backing){backing=nullptr;text=nullptr;}
            if(widget==text)text=nullptr;
        }
        void* name=nullptr;
        void* bounty=nullptr;
        void* panel=nullptr;
        void* backing=nullptr;
        void* text=nullptr;
        bool shown=false;
    };

    static Widgets widgets;
    static MainBarUpdate originalUpdate=nullptr;
    static GetWidget getWidget=nullptr;
    static CreatePopup createPopup=nullptr;
    static CreateChild createChild=nullptr;
    static DestroyWidget destroyWidget=nullptr;
    static SetProperty setProperty=nullptr;
    static SetCoord setCoord=nullptr;
    static IsVisible inheritedVisible=nullptr;
    static Position absolutePosition=nullptr;
    static MeasureText measureText=nullptr;
    static SetCaption setCaption=nullptr;
    static SetFontHeight setFontHeight=nullptr;
    static GuiGetPtr_t getManager=nullptr;
    static RegisterUnlinker registerUnlinker=nullptr;
    static void* owner=nullptr;
    static DWORD nextLookup=0;
    static bool disabled=false;
    static Rect lastCoord{};
    static constexpr int PreferredFont=17;
    static constexpr int SmallestFont=12;
    static constexpr int FrameInset=6;
    static int font=PreferredFont;
    static Size preferredSize{},textSize{};
    static int shownShape=-1,shownLoose=-1;
    static char shownKeys[KEY_COUNT][12]{};

    static Point WidgetPosition(const void* widget)
    {
        // The ICroppedRectangle subobject follows IObject in the verified
        // MyGUI Widget layout. This exported getter returns a borrowed point.
        return *absolutePosition(static_cast<const char*>(widget)+8);
    }

    static void Hide()
    {
        if(widgets.panel&&widgets.shown)MG_setVisible(widgets.panel,false);
        widgets.shown=false;
    }

    static void Discard()
    {
        void* panel=widgets.panel;
        widgets.panel=widgets.backing=widgets.text=nullptr;
        widgets.shown=false;
        if(panel)if(void* gui=MG_getGui())destroyWidget(gui,panel);
    }

    static void NonInteractive(void* widget)
    {
        EngineString no,mouse,key;
        StrMakeInline(no,"false");StrMakeInline(mouse,"NeedMouse");StrMakeInline(key,"NeedKey");
        setProperty(widget,&mouse,&no);
        setProperty(widget,&key,&no);
    }

    static void FindAnchor(void* mainBar)
    {
        if(owner!=mainBar)
        {
            Discard();
            owner=mainBar;
            widgets.name=widgets.bounty=nullptr;
            nextLookup=0;
        }
        if(widgets.name)return;
        const DWORD now=GetTickCount();
        if(nextLookup&&static_cast<LONG>(now-nextLookup)<0)return;
        nextLookup=now+500;
        void* manager=getManager();
        if(!manager)return;
        EngineString name,bounty;
        StrMakeInline(name,"NamePanel");
        StrMakeInline(bounty,"BountyPanel");
        void* namePanel=getWidget(mainBar,&name);
        if(!namePanel)return;
        // MyGUI clears observers when it restarts. Registration is idempotent.
        registerUnlinker(manager,&widgets);
        widgets.name=namePanel;
        widgets.bounty=getWidget(mainBar,&bounty);
    }

    static bool Make()
    {
        if(widgets.panel&&widgets.backing&&widgets.text)return true;
        if(widgets.panel)Discard();
        EngineString widgetType,frame,layer,panelName,textType,style,textName,backdrop,backdropName;
        char frameBacking[64],styleBacking[64],backdropBacking[64];
        StrMakeInline(widgetType,"Widget");StrMakeInline(layer,"Dialog");
        StrMakeInline(panelName,"OK_Shapes");StrMakeInline(textType,"TextBox");
        StrMakeInline(textName,"OK_ShapesText");
        StrMakeInline(backdropName,"OK_ShapesBg");
        StrMakeHeap(frame,frameBacking,sizeof(frameBacking),"Kenshi_FloatingPanelThinSkin");
        StrMakeHeap(style,styleBacking,sizeof(styleBacking),"Kenshi_TextboxStandardText");
        StrMakeHeap(backdrop,backdropBacking,sizeof(backdropBacking),"Kenshi_GenericTextBoxFlat");
        const Rect initial={0,0,readout::MinWidth,32};
        constexpr int Popup=1; // MyGUI::WidgetStyle::Popup
        widgets.panel=createPopup(widgets.name,Popup,&widgetType,&frame,&initial,ALIGN_DEFAULT,&layer,&panelName);
        if(!widgets.panel)return false;
        MG_setVisible(widgets.panel,false);
        NonInteractive(widgets.panel);
        // The native bounty panel uses this pairing for readable vanilla text.
        widgets.backing=createChild(widgets.panel,&widgetType,&backdrop,FrameInset,FrameInset,
                                    initial.width-2*FrameInset,initial.height-2*FrameInset,ALIGN_DEFAULT,&backdropName);
        if(!widgets.backing){Discard();return false;}
        NonInteractive(widgets.backing);
        const int inset=readout::Padding-FrameInset;
        widgets.text=createChild(widgets.backing,&textType,&style,inset,inset,512,512,ALIGN_DEFAULT,&textName);
        if(!widgets.text){Discard();return false;}
        NonInteractive(widgets.text);
        shownShape=-1;
        lastCoord={};
        return true;
    }

    static const char* KeyShown(const char* key)
    {
        static const char* const map[][2]={{"minus","-"},{"plus","="},{"comma",","},{"period","."}};
        for(const auto& pair:map)if(_stricmp(key,pair[0])==0)return pair[1];
        return key;
    }

    static void UpdateCaption()
    {
        if(shownShape==g_set.shape&&shownLoose==g_set.loose&&!memcmp(shownKeys,g_set.keys,sizeof(shownKeys)))return;
        static const char* const TITLE[3]={"FORMATIONS","ПОСТРОЕНИЯ","阵型"};
        static const char* const LOOSE[3]={"loose","свободно","松散"};
        static const char* const TIGHT[3]={"tight","плотно","紧密"};
        static const char* const RETREAT[3]={"retreat","отход","撤退"};
        static const char* const RALLY[3]={"rally point","точка сбора","集结点"};
        const int lang=g_uiLang>=0&&g_uiLang<3?g_uiLang:0;
        char text[1000];
        int p=sprintf_s(text,"%s\n",TITLE[lang]);
        for(int i=0;i<SHAPES;++i)
            p+=sprintf_s(text+p,sizeof(text)-p,"%s %s  %s\n",i==g_set.shape?">":"  ",KeyShown(g_set.keys[SHAPE_KEY[i]]),SHAPE_NAMES_UI[i][lang]);
        sprintf_s(text+p,sizeof(text)-p,"  %s  %s\n\n  %s  %s\n  %s  %s",KeyShown(g_set.keys[KEY_LOOSE]),
                  g_set.loose?LOOSE[lang]:TIGHT[lang],KeyShown(g_set.keys[KEY_RETREAT]),RETREAT[lang],
                  KeyShown(g_set.keys[KEY_RALLY]),RALLY[lang]);
        EngineString caption;
        char backing[sizeof(text)];
        if(!StrMakeHeap(caption,backing,sizeof(backing),text))return;
        setCaption(widgets.text,&caption);
        font=PreferredFont;
        setFontHeight(widgets.text,font);
        measureText(widgets.text,&preferredSize);
        textSize=preferredSize;
        shownShape=g_set.shape;
        shownLoose=g_set.loose;
        memcpy(shownKeys,g_set.keys,sizeof(shownKeys));
    }

    static void Update(void* mainBar,bool modifier)
    {
        if(!modifier){Hide();return;}
        FindAnchor(mainBar);
        if(!widgets.name||!inheritedVisible(widgets.name)){Hide();return;}
        if(!Make()){disabled=true;ErrorLog("readout: could not create the HUD popup");return;}
        UpdateCaption();
        const Point anchor=WidgetPosition(widgets.name);
        const int bountyTop=widgets.bounty&&inheritedVisible(widgets.bounty)?WidgetPosition(widgets.bounty).y:-1;
        Rect coord{};
        if(font<PreferredFont&&readout::Place(anchor.x,anchor.y,bountyTop,preferredSize.width,preferredSize.height,coord))
        {
            font=PreferredFont;
            setFontHeight(widgets.text,font);
            textSize=preferredSize;
        }
        while(!readout::Place(anchor.x,anchor.y,bountyTop,textSize.width,textSize.height,coord))
        {
            if(font==SmallestFont||textSize.height<=0||anchor.y<=0){Hide();return;}
            setFontHeight(widgets.text,--font);
            measureText(widgets.text,&textSize);
        }
        if(memcmp(&lastCoord,&coord,sizeof(coord))!=0)
        {
            setCoord(widgets.panel,coord.x,coord.y,coord.width,coord.height);
            setCoord(widgets.backing,FrameInset,FrameInset,coord.width-2*FrameInset,coord.height-2*FrameInset);
            const int inset=readout::Padding-FrameInset;
            setCoord(widgets.text,inset,inset,
                     coord.width-2*readout::Padding,coord.height-2*readout::Padding);
            lastCoord=coord;
        }
        if(!widgets.shown){MG_setVisible(widgets.panel,true);widgets.shown=true;}
    }

    static void HookUpdate(void* mainBar)
    {
        originalUpdate(mainBar);
        if(disabled)return;
        __try {Update(mainBar,ModifierHeld()&&!GuiHasKeyboard());}
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            disabled=true;
            __try {Discard();} __except(EXCEPTION_EXECUTE_HANDLER) {}
            ErrorLog("readout: HUD popup failed");
        }
    }

    static void Init(HMODULE kl,HMODULE mygui)
    {
        if(!mygui||!MG_getGui||!MG_setVisible)
        {
            ErrorLog("readout: HUD popup unavailable on this UI build");
            return;
        }
        getWidget=reinterpret_cast<GetWidget>(Real(GetProcAddress(kl,"?_getWidget@MainBarGUI@@QEAAPEAVWidget@MyGUI@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z")));
        createPopup=reinterpret_cast<CreatePopup>(GetProcAddress(mygui,"?createWidgetT@Widget@MyGUI@@QEAAPEAV12@UWidgetStyle@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@1AEBU?$TCoord@H@types@2@UAlign@2@11@Z"));
        createChild=reinterpret_cast<CreateChild>(GetProcAddress(mygui,"?createWidgetT@Widget@MyGUI@@QEAAPEAV12@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0HHHHUAlign@2@0@Z"));
        destroyWidget=reinterpret_cast<DestroyWidget>(GetProcAddress(mygui,"?destroyWidget@Gui@MyGUI@@QEAAXPEAVWidget@2@@Z"));
        setProperty=reinterpret_cast<SetProperty>(GetProcAddress(mygui,"?setProperty@Widget@MyGUI@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z"));
        setCoord=reinterpret_cast<SetCoord>(GetProcAddress(mygui,"?setCoord@Widget@MyGUI@@QEAAXHHHH@Z"));
        inheritedVisible=reinterpret_cast<IsVisible>(GetProcAddress(mygui,"?getInheritedVisible@Widget@MyGUI@@QEBA_NXZ"));
        absolutePosition=reinterpret_cast<Position>(GetProcAddress(mygui,"?getAbsolutePosition@ICroppedRectangle@MyGUI@@QEBAAEBU?$TPoint@H@types@2@XZ"));
        measureText=reinterpret_cast<MeasureText>(GetProcAddress(mygui,"?getTextSize@TextBox@MyGUI@@UEAA?AU?$TSize@H@types@2@XZ"));
        getManager=reinterpret_cast<GuiGetPtr_t>(GetProcAddress(mygui,"?getInstancePtr@?$Singleton@VWidgetManager@MyGUI@@@MyGUI@@SAPEAVWidgetManager@2@XZ"));
        registerUnlinker=reinterpret_cast<RegisterUnlinker>(GetProcAddress(mygui,"?registerUnlinker@WidgetManager@MyGUI@@QEAAXPEAVIUnlinkWidget@2@@Z"));
        setCaption=reinterpret_cast<SetCaption>(GetProcAddress(mygui,"?setCaptionWithReplacing@TextBox@MyGUI@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z"));
        setFontHeight=reinterpret_cast<SetFontHeight>(GetProcAddress(mygui,"?setFontHeight@TextBox@MyGUI@@UEAAXH@Z"));
        if(!getWidget||!createPopup||!createChild||!destroyWidget||!setProperty||!setCoord||
           !inheritedVisible||!absolutePosition||!measureText||!getManager||!registerUnlinker||!setCaption||!setFontHeight)
        {
            ErrorLog("readout: HUD popup unavailable on this UI build");
            return;
        }
        Hook("MainBarGUI::update (formations HUD)",GetProcAddress(kl,"?_NV_update@MainBarGUI@@QEAAXXZ"),
             reinterpret_cast<void*>(&HookUpdate),reinterpret_cast<void**>(&originalUpdate));
    }
}
