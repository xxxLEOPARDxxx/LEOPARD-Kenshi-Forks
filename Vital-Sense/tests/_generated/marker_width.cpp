
#include <cstdio>
#include <algorithm>
const int kKoMarkerWidthPx=64,kKoMarkerHeightPx=18,kKoBountySymbolMinWidthPx=12,kKoBountySymbolGapPx=1;
const int kKoTextOutlineLayerCount=8,kKoBountyGlowOffsetXPx=-3,kKoBountyGlowOffsetYPx=-3,kKoBountyGlowPaddingPx=3;
namespace MyGUI {
struct IntSize {int width,height;IntSize(int w=0,int h=0):width(w),height(h){}};
struct Widget {
 int left,top,width,height,applies,measures;IntSize measured;
 Widget():left(0),top(0),width(0),height(0),applies(0),measures(0),measured(30,18){}
 void setCoord(int l,int t,int w,int h){left=l;top=t;width=w;height=h;++applies;}
 IntSize getTextSize(){++measures;return measured;}
};
typedef Widget TextBox;typedef Widget ImageBox;
}
struct Visual {
 bool valid;unsigned configRevision;int markerState,markerRelation,totalBounty;
 bool showStateIcon,showStateText,showBountyGlow,showBountySymbol,visible;
 bool positionValid;int positionLeft,positionTop,positionTotalBounty;
 bool positionShowStateIcon,positionShowStateText,positionShowBountySymbol,positionIsBountyOnlyState;
};
typedef Visual KoMarkerVisualState;
struct KoMarkerWidget {
 MyGUI::ImageBox* bountyGlow;MyGUI::ImageBox* icon;MyGUI::TextBox* bountySymbolOutline[8];MyGUI::TextBox* bountySymbol;
 MyGUI::TextBox* fallbackTextOutline[8];MyGUI::TextBox* fallbackText;Visual visualState;
 int textWidthPx,symbolWidthPx;
};
struct RuntimeStateView {struct Config {bool placeBountySymbolBeforeStateIcon;}config;unsigned configRevision;};
int ResolveBountySymbolWidthPx(RuntimeStateView&,int){return 20;}
void GetKoMarkerTextOutlineOffset(size_t n,int* x,int* y){*x=int(n%3)-1;*y=int(n/3)-1;}
bool NeedsKoMarkerVisualRefresh(
    const KoMarkerWidget& marker,
    RuntimeStateView& state,
    int markerState,
    int markerRelation,
    int totalBounty,
    bool showStateIcon,
    bool showStateText,
    bool showBountyGlow,
    bool showBountySymbol)
{
    const KoMarkerVisualState& visualState = marker.visualState;
    return !visualState.valid
        || visualState.configRevision != state.configRevision
        || visualState.markerState != markerState
        || visualState.markerRelation != markerRelation
        || visualState.totalBounty != totalBounty
        || visualState.showStateIcon != showStateIcon
        || visualState.showStateText != showStateText
        || visualState.showBountyGlow != showBountyGlow
        || visualState.showBountySymbol != showBountySymbol;
}
void UpdateKoMarkerVisualStateCache(
    KoMarkerWidget& marker,
    RuntimeStateView& state,
    int markerState,
    int markerRelation,
    int totalBounty,
    bool showStateIcon,
    bool showStateText,
    bool showBountyGlow,
    bool showBountySymbol)
{
    marker.visualState.valid = true;
    marker.visualState.configRevision = state.configRevision;
    marker.visualState.markerState = markerState;
    marker.visualState.markerRelation = markerRelation;
    marker.visualState.totalBounty = totalBounty;
    marker.visualState.showStateIcon = showStateIcon;
    marker.visualState.showStateText = showStateText;
    marker.visualState.showBountyGlow = showBountyGlow;
    marker.visualState.showBountySymbol = showBountySymbol;
    marker.visualState.visible = false;
    marker.visualState.positionValid = false;
}
int MeasureKoMarkerTextWidth(MyGUI::TextBox* text)
{
    if (!text) return 0;
    try
    {
        const int width = text->getTextSize().width;
        // The font engine measures actual glyphs (including UTF-8 captions).
        // Leave room for the shadow/outline and bound malformed font metrics.
        if (width <= 0) return 0;
        return (std::min)(4096, width) + 4;
    }
    catch (...) { return 0; }
}
void UpdateKoMarkerTextWidths(KoMarkerWidget& marker, bool showText, bool showSymbol)
{
    // Only called from the existing visual/config refresh, never on movement.
    marker.textWidthPx = showText ? MeasureKoMarkerTextWidth(marker.fallbackText) : 0;
    marker.symbolWidthPx = showSymbol ? MeasureKoMarkerTextWidth(marker.bountySymbol) : 0;
}
int ResolveKoMarkerTextWidth(const KoMarkerWidget& marker)
{
    // A bounty symbol must not consume the caption's previous content area.
    return (std::max)(kKoMarkerWidthPx - (kKoMarkerHeightPx + 1), marker.textWidthPx);
}
int ResolveKoMarkerSymbolWidth(RuntimeStateView& state, const KoMarkerWidget& marker, int totalBounty)
{
    return (std::max)(ResolveBountySymbolWidthPx(state, totalBounty), marker.symbolWidthPx);
}
int ResolveKoMarkerWidth(RuntimeStateView& state, const KoMarkerWidget& marker, int totalBounty,
                         bool showIcon, bool showText, bool showSymbol)
{
    int width = showIcon ? kKoMarkerHeightPx + 1 : 0;
    if (showText) width += ResolveKoMarkerTextWidth(marker);
    if (showSymbol) width += ResolveKoMarkerSymbolWidth(state, marker, totalBounty) + kKoBountySymbolGapPx;
    return (std::max)(kKoMarkerWidthPx, width);
}
void SetKoMarkerPosition(
    RuntimeStateView& state,
    KoMarkerWidget& marker,
    int left,
    int top,
    int totalBounty,
    bool showStateIcon,
    bool showStateText,
    bool showBountySymbol,
    bool isBountyOnlyState)
{
    if (marker.visualState.positionValid
        && marker.visualState.positionLeft == left
        && marker.visualState.positionTop == top
        && marker.visualState.positionTotalBounty == totalBounty
        && marker.visualState.positionShowStateIcon == showStateIcon
        && marker.visualState.positionShowStateText == showStateText
        && marker.visualState.positionShowBountySymbol == showBountySymbol
        && marker.visualState.positionIsBountyOnlyState == isBountyOnlyState)
    {
        return;
    }

    const bool placeSymbolBeforeIcon = showBountySymbol && showStateIcon && state.config.placeBountySymbolBeforeStateIcon;
    const int symbolWidth = showBountySymbol ? ResolveKoMarkerSymbolWidth(state, marker, totalBounty) : kKoBountySymbolMinWidthPx;
    const int markerWidth = ResolveKoMarkerWidth(state, marker, totalBounty, showStateIcon, showStateText, showBountySymbol);
    int layoutLeft = left;
    int iconLeft = left;
    int symbolLeft = left;
    bool hasSymbolPlacement = false;

    if (showBountySymbol && isBountyOnlyState && !showStateIcon && !showStateText)
    {
        symbolLeft = left + ((markerWidth - symbolWidth) / 2);
        hasSymbolPlacement = true;
    }

    if (!isBountyOnlyState && placeSymbolBeforeIcon)
    {
        symbolLeft = layoutLeft;
        hasSymbolPlacement = true;
        layoutLeft += symbolWidth + kKoBountySymbolGapPx;
    }

    if (showStateIcon)
    {
        iconLeft = layoutLeft;
        layoutLeft += kKoMarkerHeightPx + 1;
    }

    if (!isBountyOnlyState && showBountySymbol && !placeSymbolBeforeIcon)
    {
        symbolLeft = layoutLeft;
        hasSymbolPlacement = true;
        layoutLeft += symbolWidth + kKoBountySymbolGapPx;
    }

    if (marker.icon)
    {
        try
        {
            marker.icon->setCoord(iconLeft, top, kKoMarkerHeightPx, kKoMarkerHeightPx);
        }
        catch (...)
        {
        }
    }

    for (int i = 0; i < kKoTextOutlineLayerCount; ++i)
    {
        if (!marker.bountySymbolOutline[i])
        {
            continue;
        }
        int offsetX = 0;
        int offsetY = 0;
        GetKoMarkerTextOutlineOffset(static_cast<size_t>(i), &offsetX, &offsetY);
        marker.bountySymbolOutline[i]->setCoord(
            symbolLeft + offsetX,
            top + offsetY,
            symbolWidth,
            kKoMarkerHeightPx);
    }

    if (marker.bountySymbol)
    {
        try
        {
            marker.bountySymbol->setCoord(symbolLeft, top, symbolWidth, kKoMarkerHeightPx);
        }
        catch (...)
        {
        }
    }

    if (marker.bountyGlow)
    {
        try
        {
            int glowAnchorLeft = left;
            if (showStateIcon)
            {
                glowAnchorLeft = iconLeft;
            }
            else if (hasSymbolPlacement)
            {
                glowAnchorLeft = symbolLeft;
            }
            marker.bountyGlow->setCoord(
                glowAnchorLeft + kKoBountyGlowOffsetXPx,
                top + kKoBountyGlowOffsetYPx,
                kKoMarkerHeightPx + (kKoBountyGlowPaddingPx * 2),
                kKoMarkerHeightPx + (kKoBountyGlowPaddingPx * 2));
        }
        catch (...)
        {
        }
    }

    if (marker.fallbackText)
    {
        try
        {
            const int textWidth = ResolveKoMarkerTextWidth(marker);
            if (showStateText)
            {
                marker.fallbackText->setCoord(layoutLeft, top, textWidth, kKoMarkerHeightPx);
                for (int i = 0; i < kKoTextOutlineLayerCount; ++i)
                {
                    if (!marker.fallbackTextOutline[i])
                    {
                        continue;
                    }
                    int offsetX = 0;
                    int offsetY = 0;
                    GetKoMarkerTextOutlineOffset(static_cast<size_t>(i), &offsetX, &offsetY);
                    marker.fallbackTextOutline[i]->setCoord(
                        layoutLeft + offsetX,
                        top + offsetY,
                        textWidth,
                        kKoMarkerHeightPx);
                }
            }
            else
            {
                marker.fallbackText->setCoord(layoutLeft, top, 0, kKoMarkerHeightPx);
                for (int i = 0; i < kKoTextOutlineLayerCount; ++i)
                {
                    if (!marker.fallbackTextOutline[i])
                    {
                        continue;
                    }
                    marker.fallbackTextOutline[i]->setCoord(layoutLeft, top, 0, kKoMarkerHeightPx);
                }
            }
        }
        catch (...)
        {
        }
    }

    marker.visualState.positionValid = true;
    marker.visualState.positionLeft = left;
    marker.visualState.positionTop = top;
    marker.visualState.positionTotalBounty = totalBounty;
    marker.visualState.positionShowStateIcon = showStateIcon;
    marker.visualState.positionShowStateText = showStateText;
    marker.visualState.positionShowBountySymbol = showBountySymbol;
    marker.visualState.positionIsBountyOnlyState = isBountyOnlyState;
}
int checks=0,failures=0;
void Check(bool value,const char* message){++checks;if(!value){++failures;printf("FAIL %s\n",message);}}
int main(){
 RuntimeStateView state={};state.config.placeBountySymbolBeforeStateIcon=true;
 MyGUI::Widget icon,text,symbol,glow,outline[8],symbolOutline[8];KoMarkerWidget marker={};
 marker.icon=&icon;marker.fallbackText=&text;marker.bountySymbol=&symbol;marker.bountyGlow=&glow;
 for(int n=0;n<8;++n){marker.fallbackTextOutline[n]=&outline[n];marker.bountySymbolOutline[n]=&symbolOutline[n];}
 marker.textWidthPx=34;marker.symbolWidthPx=14;
 SetKoMarkerPosition(state,marker,100,50,100,true,true,true,false);
 Check(text.width>=34,"three-character status must not be shortened by bounty symbol");
 Check(text.left>=icon.left+icon.width,"text does not overlap icon");
 Check(icon.left>=symbol.left+symbol.width,"icon does not overlap bounty");
 for(int n=0;n<8;++n)Check(outline[n].width==text.width,"outline and caption get identical content width");
 int writes=text.applies;SetKoMarkerPosition(state,marker,100,50,100,true,true,true,false);
 Check(text.applies==writes,"unchanged marker does not dirty GUI geometry");
 marker.visualState.positionValid=false;marker.textWidthPx=160;
 SetKoMarkerPosition(state,marker,100,50,100,true,true,true,false);
 Check(text.width>=160,"large or Chinese measured glyphs retain their complete width");
 Check(text.measures==0,"placement does not measure text each frame");
 marker.visualState.positionValid=false;
 state.config.placeBountySymbolBeforeStateIcon=false;
 SetKoMarkerPosition(state,marker,100,50,100,true,true,true,false);
 Check(symbol.left>=icon.left+icon.width,"alternate symbol order keeps the icon first");
 Check(text.left>=symbol.left+symbol.width&&text.width>=160,"alternate order keeps full caption after symbol");
 for(int flags=0;flags<8;++flags){
  marker.visualState.positionValid=false;
  bool hasIcon=(flags&1)!=0,hasText=(flags&2)!=0,hasSymbol=(flags&4)!=0;
  int width=ResolveKoMarkerWidth(state,marker,100,hasIcon,hasText,hasSymbol);
  SetKoMarkerPosition(state,marker,100,50,100,hasIcon,hasText,hasSymbol,false);
  if(hasText)Check(text.left+text.width<=100+width,"combined width contains caption for every visible component combination");
  if(hasSymbol)Check(symbol.left+symbol.width<=100+width,"combined width contains bounty for every visible component combination");
 }
 marker.visualState.positionValid=false;marker.symbolWidthPx=100;
 SetKoMarkerPosition(state,marker,100,50,100,false,false,true,true);
 Check(symbol.width>=100,"custom multi-character bounty label is not forced into32px");
 UpdateKoMarkerTextWidths(marker,true,true);
 Check(marker.textWidthPx==34&&marker.symbolWidthPx==34,"actual font glyph width plus outline margin cached");
 int measures=text.measures;UpdateKoMarkerTextWidths(marker,false,false);
 Check(marker.textWidthPx==0&&marker.symbolWidthPx==0&&text.measures==measures,"hidden labels do not request measurement");
 text.measured.width=100000;UpdateKoMarkerTextWidths(marker,true,false);
 Check(marker.textWidthPx==4100,"malformed excessive font widths have bounded geometry");
 UpdateKoMarkerVisualStateCache(marker,state,1,2,100,true,true,false,true);
 marker.visualState.positionValid=true;
 Check(!NeedsKoMarkerVisualRefresh(marker,state,1,2,100,true,true,false,true),"stable visuals retain their measurement cache");
 ++state.configRevision;
 Check(NeedsKoMarkerVisualRefresh(marker,state,1,2,100,true,true,false,true),"config revision requests caption/font measurement refresh");
 UpdateKoMarkerVisualStateCache(marker,state,1,2,100,true,true,false,true);
 Check(!marker.visualState.positionValid,"visual refresh invalidates position cache even without camera motion");
 printf("Vital Sense marker placement: %s (%d checks)\n",failures?"FAIL":"PASS",checks);
 return failures?1:0;
}
