// Compiles the actual production header update/teardown against a small MyGUI
// model. Theme colours represent XML properties, never an inferred palette.
#include "../src/TraderAdaptiveLayout.h"
#include <string>
#include <cstring>
#include <map>
#include <vector>
#include <cstdio>
#include <stdexcept>
namespace MyGUI {
struct Colour {float red,green,blue,alpha;Colour(float r=0,float g=0,float b=0,float a=1):red(r),green(g),blue(b),alpha(a){}bool operator==(const Colour& c)const{return red==c.red&&green==c.green&&blue==c.blue&&alpha==c.alpha;}bool operator!=(const Colour& c)const{return !(*this==c);}};
struct IntCoord {int left,top,width,height;IntCoord(int l=0,int t=0,int w=0,int h=0):left(l),top(t),width(w),height(h){}bool operator==(const IntCoord& c)const{return left==c.left&&top==c.top&&width==c.width&&height==c.height;}bool operator!=(const IntCoord& c)const{return !(*this==c);}};
struct IntSize {int width,height;IntSize(int w=0,int h=0):width(w),height(h){}};
struct Align {int bits;Align(int b=0):bits(b){}enum{Default=0,Left=1,Right=2,HCenter=4,VCenter=8,Top=16,Bottom=32,HStretch=64,VStretch=128};bool isHStretch()const{return (bits&HStretch)!=0;}bool isVStretch()const{return (bits&VStretch)!=0;}bool isRight()const{return bits&Right;}bool isHCenter()const{return bits&HCenter;}bool isBottom()const{return bits&Bottom;}bool isVCenter()const{return bits&VCenter;}bool operator!=(Align a)const{return bits!=a.bits;}};
struct UString:std::string {UString(){}UString(const char* s):std::string(s){}UString(const std::string& s):std::string(s){}std::string asUTF8()const{return *this;}};
struct ISubWidgetText {int height;std::string font;Colour colour;ISubWidgetText():height(21),font("NativeMedium"){}int getFontHeight(){return height;}const std::string& getFontName(){return font;}Colour getTextColour(){return colour;}};
class Widget;static Widget* destroyOnCoord=0;static void AfterCoordinate(Widget*);
class TextBox;static Colour xmlColour;static int creates=0,coordWrites=0,captionWrites=0,failAt=0,treeReads=0;
class Widget {public:Widget* parent;IntCoord coord;float alpha;bool visible,dead,focus;std::string name,skin;std::map<std::string,std::string> tags;Align align;std::vector<Widget*> children;
 Widget(Widget* p=0,const char* n=""):parent(p),coord(0,0,500,550),alpha(1),visible(true),dead(false),focus(false),name(n){if(p)p->children.push_back(this);}virtual ~Widget(){}
 template<class T>T* castType(bool){return dynamic_cast<T*>(this);}template<class T>T* createWidget(const std::string& s,IntCoord c,Align a,const char* n){++creates;if(creates==failAt)throw std::runtime_error("missing skin");T* t=new T(this,n);t->skin=s;t->coord=c;t->align=a;return t;}
 size_t getChildCount(){++treeReads;return children.size();}Widget* getChildAt(size_t i){++treeReads;return children[i];}
 Widget* getParent(){return parent;}int getWidth(){return coord.width;}int getHeight(){return coord.height;}IntCoord getCoord(){return coord;}void setCoord(IntCoord c){int dx=c.width-coord.width,dy=c.height-coord.height;coord=c;++coordWrites;for(size_t i=0;i<children.size();++i){Widget* w=children[i];if(w->dead)continue;Align a=w->align;TraderAdaptiveLayout::Rect r=TraderAdaptiveLayout::ProjectAlign(TraderAdaptiveLayout::Rect(w->coord.left,w->coord.top,w->coord.width,w->coord.height),dx,dy,a.isHStretch()?3:a.isRight()?2:a.isHCenter()?1:0,a.isVStretch()?3:a.isBottom()?2:a.isVCenter()?1:0);IntCoord next(r.left,r.top,r.width,r.height);if(next!=w->coord)w->setCoord(next);}AfterCoordinate(this);}IntCoord getAbsoluteCoord(){IntCoord c=coord;if(parent){IntCoord p=parent->getAbsoluteCoord();c.left+=p.left;c.top+=p.top;}return c;}
 const std::string& getName(){return name;}float getAlpha(){return alpha;}void setAlpha(float a){alpha=a;}float realAlpha(){return alpha*(parent?parent->realAlpha():1);}bool getVisible(){if(dead)throw std::runtime_error("Native read of destroyed widget");return visible;}void setVisible(bool v){visible=v;}Align getAlign(){return align;}
 bool getNeedMouseFocus(){return focus;}void setNeedMouseFocus(bool v){focus=v;}std::string getUserString(const std::string& k){return tags[k];}void setUserString(const std::string& k,const std::string& v){tags[k]=v;}void clearUserString(const std::string& k){tags.erase(k);}
};
class Window:public Widget {public:Window(Widget* p=0,const char* n=""):Widget(p,n){}};
static size_t VisibleCaptionSize(const std::string& s){size_t n=0;for(size_t i=0;i<s.size();++i){if(s[i]=='#'&&i+6<s.size()&&std::strspn(s.c_str()+i+1,"0123456789abcdefABCDEF")>=6){i+=6;continue;}++n;}return n;}
class TextBox:public Widget {public:ISubWidgetText rendered;UString caption;Align textAlign;TextBox(Widget* p=0,const char* n=""):Widget(p,n){rendered.colour=xmlColour;}const UString& getCaption(){return caption;}void setCaption(UString c){caption=c;++captionWrites;}void setFontName(const std::string& f){rendered.font=f;}void setFontHeight(int h){rendered.height=h;}void setTextColour(Colour c){rendered.colour=c;}IntSize getTextSize(){return IntSize((int)VisibleCaptionSize(caption)*rendered.height/2,rendered.height);}Align getTextAlign(){return textAlign;}void setTextAlign(Align a){textAlign=a;}};
class Gui {public:static Gui* getInstancePtr(){static Gui g;return &g;}void destroyWidget(Widget* w){while(!w->children.empty())destroyWidget(w->children.back());w->dead=true;if(w->parent){std::vector<Widget*>& v=w->parent->children;for(size_t i=0;i<v.size();++i)if(v[i]==w){v.erase(v.begin()+i);break;}}}};
static void AfterCoordinate(Widget* w){if(destroyOnCoord==w){destroyOnCoord=0;Gui::getInstancePtr()->destroyWidget(w);}}
}
struct WidgetRef {MyGUI::Widget* value;WidgetRef():value(0){}MyGUI::Widget* get(){return value&&!value->dead?value:0;}void set(MyGUI::Widget* w){value=w;}void reset(){value=0;}};
struct Metric {void Reset(){}};
struct Snapshot {bool Observe(MyGUI::Widget*){return false;}};
struct Cache {WidgetRef controls[7],viewports[2],nativeMoney,plate,trader,player,measure,popup;Metric metrics[2];Snapshot headerText[3];MyGUI::UString nativeCaption;MyGUI::IntCoord headerCoord;
 int playerMoney,moneyFontHeight,balance,baseFont[7],fittedFont[7],headerExtra,footerExtra;std::string moneyFontName;MyGUI::Colour moneyColour;bool headerValid,showDeal,popupValid,viewportsBound,uiStateValid,footerValid,viewportHidden[2],viewportVisible[2];float plateAlpha;int headerCaptionSets;TraderAdaptiveLayout::Viewport viewportLayout[2];
 float dataAlpha;WidgetRef nativePanel,nativePlateParent,failedNativePanel,viewportParents[2],viewportHost;int viewportRetry;TraderAdaptiveLayout::Viewport plateLayout;MyGUI::IntCoord headerContent;
 Cache():playerMoney(-2),moneyFontHeight(-1),balance(0),headerExtra(0),footerExtra(0),headerValid(false),showDeal(false),popupValid(false),viewportsBound(false),uiStateValid(false),footerValid(false),plateAlpha(1),headerCaptionSets(0),dataAlpha(1),viewportRetry(0){for(int i=0;i<2;++i)viewportHidden[i]=viewportVisible[i]=false;for(int i=0;i<7;++i)baseFont[i]=fittedFont[i]=-1;}
};
typedef Cache AdaptiveUiCache;
static Cache cache;static WidgetRef dataRef,balanceRef;static MyGUI::Widget* root=0;static MyGUI::Widget* liveData=0;static MyGUI::TextBox* value=0;
static Cache& AdaptiveUi(){return cache;}static WidgetRef& DataPanelRef(){return dataRef;}static WidgetRef& TradeBalanceRef(){return balanceRef;}static WidgetRef& ArrangeRef(){static WidgetRef r;return r;}
static MyGUI::Widget* g_balanceOwner=0;static int g_balanceStartMoney=-1,g_balanceMoney=-1;static std::string g_balanceMoneyCaption,g_moneyPrefix="c.";
struct State {struct Core{bool g_showDealBalance;Core():g_showDealBalance(true){}}core;};static State state;static State& TraderState(){return state;}
static MyGUI::Widget* CachedInParent(WidgetRef& r,MyGUI::Widget*,const char*){if(!r.get())r.set(liveData);return r.get();}static MyGUI::TextBox* AsText(MyGUI::Widget* w){return dynamic_cast<MyGUI::TextBox*>(w);}static MyGUI::ISubWidgetText* RenderedText(MyGUI::Widget* w){MyGUI::TextBox* t=AsText(w);return t?&t->rendered:0;}
static void FindMoneyTexts(MyGUI::Widget*,int,MyGUI::TextBox**,MyGUI::TextBox** v){*v=value;}static int FontHeight(MyGUI::Widget* w){return RenderedText(w)->height;}static const std::string& FontName(MyGUI::Widget* w){return RenderedText(w)->font;}
namespace GameTheme {static std::map<std::string,std::string> palette;static int lookups=0;const char* Hex(const char* key,const char* fallback){++lookups;std::map<std::string,std::string>::iterator i=palette.find(key);return i==palette.end()?fallback:i->second.c_str();}}
static int wallet=1234567;static bool hasPlayer=true;static int walletReads=0;
struct Character{int getMoney(){++walletReads;return wallet;}};
struct PlayerInterface{Character member;Character* getAnyPlayerCharacter(){return hasPlayer?&member:0;}};
struct GameWorld{PlayerInterface* player;};static PlayerInterface playerInterface;static GameWorld world={&playerInterface};static GameWorld* ou=&world;
static const char* Tr(const char* s){return s;}static int ParseMoneyCaption(const std::string& s){return TraderAdaptiveLayout::ParseMoneyCaption(s);}static void LearnMoneyFormat(const std::string&){}static std::string FormatMoney(int n){char b[32];sprintf(b,"%d",n);return b;}
static void HeaderCaptionIfChanged(MyGUI::TextBox* t,MyGUI::UString c){if(t->caption!=c){t->setCaption(c);++cache.headerCaptionSets;}}static void CoordIfChanged(MyGUI::Widget* w,MyGUI::IntCoord c){if(w&&w->getCoord()!=c)w->setCoord(c);}static void AlignIfChanged(MyGUI::TextBox* t,MyGUI::Align a){if(t->getTextAlign()!=a)t->setTextAlign(a);}static void FullCaptionTip(MyGUI::Widget*,const MyGUI::UString&,bool){}
#include "ViewportProductionUnderTest.h"
static MyGUI::Widget* FindLiveDockOwner(bool){return root&&!root->dead?root:0;}static MyGUI::Widget* ResolveInjectionParent(MyGUI::Widget* w){return w;}static const int kHeaderCount=3;
static int errors=0;static void LogErrorLine(const std::string&){++errors;}
#include "HeaderProductionUnderTest.h"
static void Reparent(MyGUI::Widget& w,MyGUI::Widget* parent){if(w.parent){std::vector<MyGUI::Widget*>& v=w.parent->children;for(size_t i=0;i<v.size();++i)if(v[i]==&w){v.erase(v.begin()+i);break;}}w.parent=parent;if(parent)parent->children.push_back(&w);}
static int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
int main(int argc,char** argv){
 ou=0;CHECK(ReadPlayerMoney()==-1);ou=&world;
 world.player=0;CHECK(ReadPlayerMoney()==-1);world.player=&playerInterface;
 hasPlayer=false;int moneyReads=walletReads;CHECK(ReadPlayerMoney()==-1);CHECK(walletReads==moneyReads);hasPlayer=true;
 FILE* fixtures=argc>1?fopen(argv[1],"r"):NULL;if(!fixtures)return 2;
 for(int theme=0;theme<3;++theme){
  cache=Cache();dataRef.reset();balanceRef.reset();g_balanceOwner=0;g_balanceStartMoney=g_balanceMoney=-1;g_balanceMoneyCaption.clear();
  char skin[128],good[8],bad[8];MyGUI::IntCoord base,dc;int pa,da;MyGUI::Colour source;
  if(fscanf(fixtures,"%127s %d%d%d%d %d%d%d%d %d%d %f%f%f%f %f%f%f%f %7s %7s",skin,&base.left,&base.top,&base.width,&base.height,&dc.left,&dc.top,&dc.width,&dc.height,&pa,&da,&MyGUI::xmlColour.red,&MyGUI::xmlColour.green,&MyGUI::xmlColour.blue,&MyGUI::xmlColour.alpha,&source.red,&source.green,&source.blue,&source.alpha,good,bad)!=21)return 3;
  GameTheme::palette["Good"]=good;GameTheme::palette["Bad"]=bad;
  MyGUI::Widget owner(0,"Content"),plate(&owner,"NativeCashPlate"),data(&plate,"datapanel");root=&owner;liveData=&data;plate.coord=base;plate.skin=skin;plate.align=MyGUI::Align(pa?MyGUI::Align::HStretch:0);data.alpha=.8f;data.coord=dc;data.align=MyGUI::Align(da?MyGUI::Align::HStretch:0);
  MyGUI::TextBox cash(&data,"value");value=&cash;cash.caption="c.6000";cash.rendered.colour=source;dataRef.set(&data);
  UpdateTradeBalance(&owner,&owner);
  CHECK(cache.player.get()!=0);
  if(cache.player.get()){
   CHECK(AsText(cache.player.get())->caption=="Player: c.1234567");
   CHECK(cache.player.get()->getParent()==&plate);
   CHECK(AsText(cache.player.get())->getTextAlign().isRight());
   CHECK(RenderedText(cache.player.get())->getTextColour()==MyGUI::xmlColour);
   CHECK(cache.player.get()->coord.left+cache.player.get()->coord.width==plate.coord.width-(base.width-dc.left-dc.width));
   int beforeWrites=MyGUI::captionWrites;wallet=7654321;UpdateTradeBalance(&owner,&owner);
   CHECK(AsText(cache.player.get())->caption=="Player: c.7654321");CHECK(MyGUI::captionWrites==beforeWrites+1);
   hasPlayer=false;UpdateTradeBalance(&owner,&owner);CHECK(AsText(cache.player.get())->caption=="Player: -");
   hasPlayer=true;wallet=1234567;UpdateTradeBalance(&owner,&owner);
  }
  CHECK(plate.getAlpha()==1);CHECK(data.getAlpha()==0);CHECK(cache.trader.get()&&balanceRef.get());
  if(cache.trader.get()&&balanceRef.get()){
   CHECK(cache.trader.get()->getParent()==&plate&&balanceRef.get()->getParent()==&plate);
   CHECK(cache.trader.get()->coord.left==dc.left&&cache.trader.get()->coord.width==468-base.width+dc.width);
   CHECK(2*balanceRef.get()->coord.left+balanceRef.get()->coord.width==plate.coord.width);
   CHECK(RenderedText(cache.trader.get())->getTextColour()==MyGUI::xmlColour);
   CHECK(RenderedText(balanceRef.get())->getTextColour()==MyGUI::xmlColour);
   CHECK(cache.trader.get()->realAlpha()==.8f&&balanceRef.get()->realAlpha()==.8f);
   int writes=MyGUI::captionWrites,created=MyGUI::creates,coords=MyGUI::coordWrites;
   for(int i=0;i<1000;++i)UpdateTradeBalance(&owner,&owner);
   CHECK(writes==MyGUI::captionWrites&&created==MyGUI::creates&&coords==MyGUI::coordWrites);
   cash.caption="c.5000";UpdateTradeBalance(&owner,&owner);
   CHECK(AsText(balanceRef.get())->caption.find(good)!=std::string::npos);
   CHECK(AsText(cache.trader.get())->caption.find(good)==std::string::npos);
   int paletteReads=GameTheme::lookups;writes=MyGUI::captionWrites;created=MyGUI::creates;coords=MyGUI::coordWrites;
   for(int n=0;n<1000;++n)UpdateTradeBalance(&owner,&owner);
   CHECK(paletteReads==GameTheme::lookups&&writes==MyGUI::captionWrites&&created==MyGUI::creates&&coords==MyGUI::coordWrites);
   cash.caption="c.7000";UpdateTradeBalance(&owner,&owner);
   CHECK(AsText(balanceRef.get())->caption.find(bad)!=std::string::npos);
   cash.caption="c.6000";UpdateTradeBalance(&owner,&owner);
   CHECK(AsText(balanceRef.get())->caption.find('#')==std::string::npos);
   GameTheme::palette.erase("Good");cash.caption="c.5000";UpdateTradeBalance(&owner,&owner);
   CHECK(AsText(balanceRef.get())->caption.find('#')==std::string::npos);
   GameTheme::palette["Good"]="#bad";cash.caption="c.4000";UpdateTradeBalance(&owner,&owner);
   CHECK(AsText(balanceRef.get())->caption.find('#')==std::string::npos);
   GameTheme::palette["Good"]=good;cash.caption="c.6000";UpdateTradeBalance(&owner,&owner);
   writes=MyGUI::captionWrites;created=MyGUI::creates;coords=MyGUI::coordWrites;
   const MyGUI::IntCoord expanded=plate.getCoord();
   plate.setCoord(base);UpdateTradeBalance(&owner,&owner);
   CHECK(plate.getCoord()==expanded);
   CHECK(cache.trader.get()->coord.left+cache.trader.get()->coord.width<=plate.getWidth()
       &&balanceRef.get()->coord.left+balanceRef.get()->coord.width<=plate.getWidth());
   CHECK(writes==MyGUI::captionWrites&&created==MyGUI::creates);
   coords=MyGUI::coordWrites;
   for(int i=0;i<100;++i)UpdateTradeBalance(&owner,&owner);
   CHECK(writes==MyGUI::captionWrites&&created==MyGUI::creates&&coords==MyGUI::coordWrites);
  }
  owner.setCoord(MyGUI::IntCoord(0,0,600,550));UpdateTradeBalance(&owner,&owner);CHECK(plate.coord.width==568);
  data.alpha=.6f;UpdateTradeBalance(&owner,&owner);CHECK(data.alpha==0&&cache.trader.get()->getAlpha()==.6f&&balanceRef.get()->getAlpha()==.6f);
  DestroyTradeBalance();CHECK(!cache.player.get());CHECK(data.alpha==.6f);if(pa)base.width+=100;CHECK(plate.getCoord()==base);CHECK(plate.skin==skin);
  dataRef.set(&data);UpdateTradeBalance(&owner,&owner);plate.coord=MyGUI::IntCoord(30,5,211,43);DestroyTradeBalance();CHECK(plate.getCoord()==MyGUI::IntCoord(30,5,211,43));
  dataRef.set(&data);UpdateTradeBalance(&owner,&owner);MyGUI::Widget other;plate.parent=&other;plate.coord=MyGUI::IntCoord(7,9,100,40);DestroyTradeBalance();CHECK(plate.getCoord()==MyGUI::IntCoord(7,9,100,40));plate.parent=&owner;
  for(int fail=1;fail<=3;++fail){dataRef.set(&data);MyGUI::failAt=MyGUI::creates+fail;int oldErrors=errors;UpdateTradeBalance(&owner,&owner);CHECK(errors==oldErrors+1&&data.alpha==.6f&&!cache.trader.get()&&!balanceRef.get());CHECK(data.getUserString("LEOPARD_OTT_MoneyHeader")!="1");int allocated=MyGUI::creates;for(int n=0;n<100;++n)UpdateTradeBalance(&owner,&owner);CHECK(allocated==MyGUI::creates);MyGUI::failAt=0;DestroyTradeBalance();}
  MyGUI::xmlColour.alpha=0;cash.rendered.colour.alpha=1;int oldErrors=errors;UpdateTradeBalance(&owner,&owner);CHECK(errors==oldErrors+1&&data.alpha==.6f&&!cache.trader.get()&&!balanceRef.get());int allocated=MyGUI::creates;for(int n=0;n<100;++n)UpdateTradeBalance(&owner,&owner);CHECK(allocated==MyGUI::creates);MyGUI::xmlColour.alpha=1;DestroyTradeBalance();
  plate.alpha=.5f;UpdateTradeBalance(&owner,&owner);CHECK(plate.alpha==.5f&&cache.trader.get()->getAlpha()==.6f);DestroyTradeBalance();CHECK(plate.alpha==.5f);
 }
 // Native layouts place the plate under Content, while Window returns its
 // skin Client as the injection parent. Keep the real Content offset/size.
 cache=Cache();dataRef.reset();balanceRef.reset();g_balanceOwner=0;g_balanceStartMoney=g_balanceMoney=-1;
 MyGUI::Widget client(0,"Client"),content(&client,"generated_Content"),plate(&content,"NativeCashPlate"),data(&plate,"datapanel");
 root=&client;liveData=&data;content.coord=MyGUI::IntCoord(11,7,450,500);plate.coord=MyGUI::IntCoord(14,3,203,41);data.coord=MyGUI::IntCoord(21,11,161,19);
 MyGUI::TextBox cash(&data,"value");value=&cash;cash.caption="c.64760";dataRef.set(&data);MyGUI::xmlColour.alpha=1;
 UpdateTradeBalance(&client,&client);
 CHECK(cache.trader.get()&&balanceRef.get());CHECK(data.alpha==0);CHECK(cache.nativePlateParent.get()==&content);
 CHECK(plate.coord.left==16&&plate.coord.top==3&&plate.coord.width==418);
 int allocated=MyGUI::creates,written=MyGUI::coordWrites;
 for(int i=0;i<1000;++i)UpdateTradeBalance(&client,&client);
 CHECK(allocated==MyGUI::creates&&written==MyGUI::coordWrites);
 content.setCoord(MyGUI::IntCoord(19,12,470,510));UpdateTradeBalance(&client,&client);CHECK(plate.coord.width==438);
 DestroyTradeBalance();CHECK(data.alpha==1);CHECK(plate.coord==MyGUI::IntCoord(14,3,203,41));
 MyGUI::Widget foreign;plate.parent=&foreign;dataRef.set(&data);allocated=MyGUI::creates;
 UpdateTradeBalance(&client,&client);CHECK(!cache.trader.get()&&!balanceRef.get()&&data.alpha==1&&allocated==MyGUI::creates);
 plate.parent=&content;
 MyGUI::Widget sibling(&client,"other_scope"),a(&client),b(&a),c(&b),d(&c),e(&d),f(&e);
 CHECK(TraderAdaptiveLayout::WithinParent(&e,&client));CHECK(!TraderAdaptiveLayout::WithinParent(&f,&client));
 CHECK(!TraderAdaptiveLayout::WithinParent(&content,&sibling));
 MyGUI::Widget cyclic;cyclic.parent=&cyclic;CHECK(!TraderAdaptiveLayout::WithinParent(&cyclic,&client));
 // A sibling target must not gain control over another Content's plate.
 root=&sibling;dataRef.set(&data);allocated=MyGUI::creates;
 UpdateTradeBalance(&sibling,&sibling);CHECK(!cache.trader.get()&&!balanceRef.get()&&data.alpha==1&&allocated==MyGUI::creates);
 {
 // Combined production header + viewport reservation, including Native prefixes.
 cache=Cache();dataRef.reset();balanceRef.reset();g_balanceOwner=0;g_balanceStartMoney=g_balanceMoney=-1;
 MyGUI::Widget client(0,"Client"),content(&client,"prefix_Content"),plate(&content,"CashPlate"),data(&plate,"datapanel"),viewport(&content,"prefix_backpack_content"),scroll(&content,"prefix_scrollview_backpack_content");
 root=&client;liveData=&data;content.coord=MyGUI::IntCoord(19,12,450,500);plate.coord=MyGUI::IntCoord(14,3,203,41);data.coord=MyGUI::IntCoord(21,11,161,19);viewport.coord=MyGUI::IntCoord(8,50,400,400);scroll.coord=MyGUI::IntCoord(9,60,390,390);
 viewport.align=scroll.align=MyGUI::Align(MyGUI::Align::HStretch|MyGUI::Align::VStretch);
 MyGUI::TextBox cash(&data,"value");value=&cash;cash.caption="c.64760";dataRef.set(&data);MyGUI::xmlColour.alpha=1;
 UpdateTradeBalance(&client,&client);const int extra=cache.headerExtra;CHECK(extra>0);
 ReserveAdaptiveRows(&client,extra,12);CHECK(viewport.coord.top==50+extra&&viewport.coord.height==400-extra-12);CHECK(scroll.coord.top==60+extra&&scroll.coord.height==390-extra-12);
 CHECK(cache.viewports[0].get()==&viewport&&cache.viewports[1].get()==&scroll);
 int created=MyGUI::creates,writes=MyGUI::coordWrites,reads=MyGUI::treeReads;
 for(int n=0;n<1000;++n){UpdateTradeBalance(&client,&client);ReserveAdaptiveRows(&client,cache.headerExtra,12);}
 CHECK(created==MyGUI::creates&&writes==MyGUI::coordWrites&&reads==MyGUI::treeReads);
 content.setCoord(MyGUI::IntCoord(31,18,470,510));UpdateTradeBalance(&client,&client);ReserveAdaptiveRows(&client,cache.headerExtra,12);
 CHECK(viewport.coord.top==50+extra&&viewport.coord.height==410-extra-12);CHECK(scroll.coord.top==60+extra&&scroll.coord.height==400-extra-12);
 DestroyTradeBalance();CHECK(viewport.coord==MyGUI::IntCoord(8,50,420,410));CHECK(scroll.coord==MyGUI::IntCoord(9,60,410,400));
 CHECK(!cache.viewportParents[0].get()&&!cache.viewportParents[1].get());
 // Reopen after disable and prove parent identity changes cannot reuse an old
 // projection. Undo an exact owned local rectangle before a safe rebind.
 dataRef.set(&data);UpdateTradeBalance(&client,&client);ReserveAdaptiveRows(&client,cache.headerExtra,12);
 MyGUI::Widget alternate(&client,"other_Content");alternate.coord=MyGUI::IntCoord(7,9,430,490);
 Reparent(viewport,&alternate);ReserveAdaptiveRows(&client,extra,12);
 CHECK(cache.viewportParents[0].get()==&alternate);CHECK(viewport.coord.top==50+extra&&viewport.coord.height==410-extra-12);
 viewport.coord=MyGUI::IntCoord(9,70,300,250);ReserveAdaptiveRows(&client,extra,12);
 CHECK(viewport.coord.top==70+extra&&viewport.coord.height==250-extra-12);
 DestroyTradeBalance();CHECK(viewport.coord==MyGUI::IntCoord(9,70,300,250));CHECK(scroll.coord==MyGUI::IntCoord(9,60,410,400));
 // An alive old viewport moved to a foreign root must be left alone. A new
 // same-role viewport in this host replaces its stale refs/baseline immediately.
 dataRef.set(&data);UpdateTradeBalance(&client,&client);ReserveAdaptiveRows(&client,cache.headerExtra,12);
 MyGUI::Widget foreign;Reparent(viewport,&foreign);viewport.coord=MyGUI::IntCoord(17,21,33,44);
 MyGUI::Widget replacement(&content,"next_backpack_content");replacement.coord=MyGUI::IntCoord(6,80,310,240);
 ReserveAdaptiveRows(&client,extra,12);CHECK(viewport.coord==MyGUI::IntCoord(17,21,33,44));
 CHECK(cache.viewports[0].get()==&replacement&&cache.viewportParents[0].get()==&content);CHECK(replacement.coord.top==80+extra&&replacement.coord.height==240-extra-12);
 MyGUI::Gui::getInstancePtr()->destroyWidget(&replacement);
 MyGUI::Widget replacement2(&content,"last_backpack_content");replacement2.coord=MyGUI::IntCoord(4,90,300,230);replacement2.visible=false;
 ReserveAdaptiveRows(&client,extra,12);CHECK(cache.viewports[0].get()==&replacement2&&replacement2.coord.top==90+extra);
 // Clipping and disable preserve each native visibility state independently.
 ReserveAdaptiveRows(&client,extra,1000);CHECK(!scroll.visible&&!replacement2.visible);
 DestroyTradeBalance();CHECK(scroll.visible&&!replacement2.visible);CHECK(replacement2.coord==MyGUI::IntCoord(4,90,300,230));
 dataRef.set(&data);UpdateTradeBalance(&client,&client);ReserveAdaptiveRows(&client,extra,12);
 MyGUI::Gui::getInstancePtr()->destroyWidget(&replacement2);ReserveAdaptiveRows(&client,extra,12);
 CHECK(!cache.viewports[0].get()&&cache.viewports[1].get()==&scroll);
 // A nested foreign Window cannot supply this host's missing viewport.
 MyGUI::Window nested(&client,"ForeignWindow");MyGUI::Widget nestedViewport(&nested,"backpack_content");nestedViewport.coord=MyGUI::IntCoord(3,4,200,210);
 for(int n=0;n<61;++n)ReserveAdaptiveRows(&client,extra,12);
 CHECK(!cache.viewports[0].get()&&nestedViewport.coord==MyGUI::IntCoord(3,4,200,210));
 MyGUI::Widget late(&content,"late_backpack_content");late.coord=MyGUI::IntCoord(2,100,300,200);
 for(int n=0;n<61;++n)ReserveAdaptiveRows(&client,extra,12);
 CHECK(cache.viewports[0].get()==&late&&late.coord.top==100+extra);
 // Item icon descendants do not enter the bounded viewport binder.
 MyGUI::Widget icons[300];for(int n=0;n<300;++n)Reparent(icons[n],&late);
 MyGUI::Widget* found[2];reads=MyGUI::treeReads;FindAdaptiveViewports(&client,found);
 CHECK(found[0]==&late&&found[1]==&scroll&&MyGUI::treeReads-reads<128);
 DestroyTradeBalance();CHECK(late.coord==MyGUI::IntCoord(2,100,300,200)&&scroll.coord==MyGUI::IntCoord(9,60,410,400));
 dataRef.set(&data);UpdateTradeBalance(&client,&client);ReserveAdaptiveRows(&client,extra,12);
 MyGUI::Gui::getInstancePtr()->destroyWidget(&late);
 MyGUI::Widget callbackViewport(&content,"callback_backpack_content");callbackViewport.coord=MyGUI::IntCoord(3,110,300,220);
 MyGUI::destroyOnCoord=&callbackViewport;ReserveAdaptiveRows(&client,extra,12);
 CHECK(callbackViewport.dead&&!cache.viewports[0].get()&&!cache.viewportParents[0].get());
 MyGUI::Gui::getInstancePtr()->destroyWidget(&client);writes=MyGUI::coordWrites;DestroyTradeBalance();
 CHECK(writes==MyGUI::coordWrites&&!cache.viewports[0].get()&&!cache.viewportParents[0].get()&&!cache.viewportHost.get());
 root=0;liveData=0;value=0;
 }
 fclose(fixtures);printf("%d actual header skin/lifecycle checks, %d failures\n",checks,failures);return failures?1:0;
}
