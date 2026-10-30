// Compiles the actual production header update/teardown against a small MyGUI
// model. Theme colours represent XML properties, never an inferred palette.
#include "../src/TraderAdaptiveLayout.h"
#include <string>
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
class TextBox;static Colour xmlColour;static int creates=0,coordWrites=0,captionWrites=0,failAt=0;
class Widget {public:Widget* parent;IntCoord coord;float alpha;bool visible,dead,focus;std::string name,skin;std::map<std::string,std::string> tags;Align align;std::vector<Widget*> children;
 Widget(Widget* p=0,const char* n=""):parent(p),coord(0,0,500,550),alpha(1),visible(true),dead(false),focus(false),name(n){if(p)p->children.push_back(this);}virtual ~Widget(){}
 template<class T>T* castType(bool){return dynamic_cast<T*>(this);}template<class T>T* createWidget(const std::string& s,IntCoord c,Align a,const char* n){++creates;if(creates==failAt)throw std::runtime_error("missing skin");T* t=new T(this,n);t->skin=s;t->coord=c;t->align=a;return t;}
 Widget* getParent(){return parent;}int getWidth(){return coord.width;}int getHeight(){return coord.height;}IntCoord getCoord(){return coord;}void setCoord(IntCoord c){int dx=c.width-coord.width,dy=c.height-coord.height;coord=c;++coordWrites;for(size_t i=0;i<children.size();++i){Widget* w=children[i];if(w->dead)continue;Align a=w->align;TraderAdaptiveLayout::Rect r=TraderAdaptiveLayout::ProjectAlign(TraderAdaptiveLayout::Rect(w->coord.left,w->coord.top,w->coord.width,w->coord.height),dx,dy,a.isHStretch()?3:a.isRight()?2:a.isHCenter()?1:0,a.isVStretch()?3:a.isBottom()?2:a.isVCenter()?1:0);IntCoord next(r.left,r.top,r.width,r.height);if(next!=w->coord)w->setCoord(next);}}IntCoord getAbsoluteCoord(){IntCoord c=coord;if(parent){IntCoord p=parent->getAbsoluteCoord();c.left+=p.left;c.top+=p.top;}return c;}
 const std::string& getName(){return name;}float getAlpha(){return alpha;}void setAlpha(float a){alpha=a;}float realAlpha(){return alpha*(parent?parent->realAlpha():1);}bool getVisible(){return visible;}void setVisible(bool v){visible=v;}Align getAlign(){return align;}
 bool getNeedMouseFocus(){return focus;}void setNeedMouseFocus(bool v){focus=v;}std::string getUserString(const std::string& k){return tags[k];}void setUserString(const std::string& k,const std::string& v){tags[k]=v;}void clearUserString(const std::string& k){tags.erase(k);}
};
class TextBox:public Widget {public:ISubWidgetText rendered;UString caption;Align textAlign;TextBox(Widget* p=0,const char* n=""):Widget(p,n){rendered.colour=xmlColour;}const UString& getCaption(){return caption;}void setCaption(UString c){caption=c;++captionWrites;}void setFontName(const std::string& f){rendered.font=f;}void setFontHeight(int h){rendered.height=h;}void setTextColour(Colour c){rendered.colour=c;}IntSize getTextSize(){return IntSize((int)caption.size()*rendered.height/2,rendered.height);}Align getTextAlign(){return textAlign;}void setTextAlign(Align a){textAlign=a;}};
class Gui {public:static Gui* getInstancePtr(){static Gui g;return &g;}void destroyWidget(Widget* w){w->dead=true;}};
}
struct WidgetRef {MyGUI::Widget* value;WidgetRef():value(0){}MyGUI::Widget* get(){return value&&!value->dead?value:0;}void set(MyGUI::Widget* w){value=w;}void reset(){value=0;}};
struct Metric {void Reset(){}};
struct Snapshot {bool Observe(MyGUI::Widget*){return false;}};
struct Cache {WidgetRef controls[7],viewports[2],nativeMoney,plate,trader,measure,popup;Metric metrics[2];Snapshot headerText[2];MyGUI::UString nativeCaption;MyGUI::IntCoord headerCoord;
 int moneyFontHeight,balance,baseFont[7],fittedFont[7],headerExtra,footerExtra;std::string moneyFontName;MyGUI::Colour moneyColour;bool headerValid,showDeal,popupValid,viewportsBound,uiStateValid,footerValid,viewportHidden[2],viewportVisible[2];float plateAlpha;int headerCaptionSets;TraderAdaptiveLayout::Viewport viewportLayout[2];
 float dataAlpha;WidgetRef nativePanel,nativePlateParent,failedNativePanel;TraderAdaptiveLayout::Viewport plateLayout;MyGUI::IntCoord headerContent;
 Cache():moneyFontHeight(-1),balance(0),headerExtra(0),footerExtra(0),headerValid(false),showDeal(false),plateAlpha(1),headerCaptionSets(0),dataAlpha(1){}
};
typedef Cache AdaptiveUiCache;
static Cache cache;static WidgetRef dataRef,balanceRef;static MyGUI::Widget* root=0;static MyGUI::Widget* liveData=0;static MyGUI::TextBox* value=0;
static Cache& AdaptiveUi(){return cache;}static WidgetRef& DataPanelRef(){return dataRef;}static WidgetRef& TradeBalanceRef(){return balanceRef;}static WidgetRef& ArrangeRef(){static WidgetRef r;return r;}
static MyGUI::Widget* g_balanceOwner=0;static int g_balanceStartMoney=-1,g_balanceMoney=-1;static std::string g_balanceMoneyCaption,g_moneyPrefix="c.";
struct State {struct Core{bool g_showDealBalance;Core():g_showDealBalance(true){}}core;};static State state;static State& TraderState(){return state;}
static MyGUI::Widget* CachedInParent(WidgetRef& r,MyGUI::Widget*,const char*){if(!r.get())r.set(liveData);return r.get();}static MyGUI::TextBox* AsText(MyGUI::Widget* w){return dynamic_cast<MyGUI::TextBox*>(w);}static MyGUI::ISubWidgetText* RenderedText(MyGUI::Widget* w){MyGUI::TextBox* t=AsText(w);return t?&t->rendered:0;}
static void FindMoneyTexts(MyGUI::Widget*,int,MyGUI::TextBox**,MyGUI::TextBox** v){*v=value;}static int FontHeight(MyGUI::Widget* w){return RenderedText(w)->height;}static const std::string& FontName(MyGUI::Widget* w){return RenderedText(w)->font;}
static const char* Tr(const char* s){return s;}static int ParseMoneyCaption(const std::string& s){return TraderAdaptiveLayout::ParseMoneyCaption(s);}static void LearnMoneyFormat(const std::string&){}static std::string FormatMoney(int){return "0";}
static void HeaderCaptionIfChanged(MyGUI::TextBox* t,MyGUI::UString c){if(t->caption!=c){t->setCaption(c);++cache.headerCaptionSets;}}static void CoordIfChanged(MyGUI::Widget* w,MyGUI::IntCoord c){if(w&&w->getCoord()!=c)w->setCoord(c);}static void AlignIfChanged(MyGUI::TextBox* t,MyGUI::Align a){if(t->getTextAlign()!=a)t->setTextAlign(a);}static void FullCaptionTip(MyGUI::Widget*,const MyGUI::UString&,bool){}
static void ReserveAdaptiveRows(MyGUI::Widget*,int,int){}static MyGUI::Widget* FindLiveDockOwner(bool){return root;}static MyGUI::Widget* ResolveInjectionParent(MyGUI::Widget* w){return w;}static const int kHeaderCount=2;
static int errors=0;static void LogErrorLine(const std::string&){++errors;}
#include "HeaderProductionUnderTest.h"
static int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
int main(int argc,char** argv){
 FILE* fixtures=argc>1?fopen(argv[1],"r"):NULL;if(!fixtures)return 2;
 for(int theme=0;theme<3;++theme){
  cache=Cache();dataRef.reset();balanceRef.reset();g_balanceOwner=0;g_balanceStartMoney=g_balanceMoney=-1;g_balanceMoneyCaption.clear();
  char skin[128];MyGUI::IntCoord base,dc;int pa,da;MyGUI::Colour source;
  if(fscanf(fixtures,"%127s %d%d%d%d %d%d%d%d %d%d %f%f%f%f %f%f%f%f",skin,&base.left,&base.top,&base.width,&base.height,&dc.left,&dc.top,&dc.width,&dc.height,&pa,&da,&MyGUI::xmlColour.red,&MyGUI::xmlColour.green,&MyGUI::xmlColour.blue,&MyGUI::xmlColour.alpha,&source.red,&source.green,&source.blue,&source.alpha)!=19)return 3;
  MyGUI::Widget owner(0,"Content"),plate(&owner,"NativeCashPlate"),data(&plate,"datapanel");root=&owner;liveData=&data;plate.coord=base;plate.skin=skin;plate.align=MyGUI::Align(pa?MyGUI::Align::HStretch:0);data.alpha=.8f;data.coord=dc;data.align=MyGUI::Align(da?MyGUI::Align::HStretch:0);
  MyGUI::TextBox cash(&data,"value");value=&cash;cash.caption="c.6000";cash.rendered.colour=source;dataRef.set(&data);
  UpdateTradeBalance(&owner,&owner);
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
  DestroyTradeBalance();CHECK(data.alpha==.6f);if(pa)base.width+=100;CHECK(plate.getCoord()==base);CHECK(plate.skin==skin);
  dataRef.set(&data);UpdateTradeBalance(&owner,&owner);plate.coord=MyGUI::IntCoord(30,5,211,43);DestroyTradeBalance();CHECK(plate.getCoord()==MyGUI::IntCoord(30,5,211,43));
  dataRef.set(&data);UpdateTradeBalance(&owner,&owner);MyGUI::Widget other;plate.parent=&other;plate.coord=MyGUI::IntCoord(7,9,100,40);DestroyTradeBalance();CHECK(plate.getCoord()==MyGUI::IntCoord(7,9,100,40));plate.parent=&owner;
  for(int fail=1;fail<=2;++fail){dataRef.set(&data);MyGUI::failAt=MyGUI::creates+fail;int oldErrors=errors;UpdateTradeBalance(&owner,&owner);CHECK(errors==oldErrors+1&&data.alpha==.6f&&!cache.trader.get()&&!balanceRef.get());CHECK(data.getUserString("LEOPARD_OTT_MoneyHeader")!="1");int allocated=MyGUI::creates;for(int n=0;n<100;++n)UpdateTradeBalance(&owner,&owner);CHECK(allocated==MyGUI::creates);MyGUI::failAt=0;DestroyTradeBalance();}
  MyGUI::xmlColour.alpha=0;cash.rendered.colour.alpha=1;int oldErrors=errors;UpdateTradeBalance(&owner,&owner);CHECK(errors==oldErrors+1&&data.alpha==.6f&&!cache.trader.get()&&!balanceRef.get());int allocated=MyGUI::creates;for(int n=0;n<100;++n)UpdateTradeBalance(&owner,&owner);CHECK(allocated==MyGUI::creates);MyGUI::xmlColour.alpha=1;DestroyTradeBalance();
  plate.alpha=.5f;UpdateTradeBalance(&owner,&owner);CHECK(plate.alpha==.5f&&cache.trader.get()->getAlpha()==.6f);DestroyTradeBalance();CHECK(plate.alpha==.5f);
 }
 fclose(fixtures);printf("%d actual header skin/lifecycle checks, %d failures\n",checks,failures);return failures?1:0;
}
