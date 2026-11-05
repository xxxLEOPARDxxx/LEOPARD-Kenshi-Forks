#pragma once
namespace CharacterHighlightPolicy {
inline float Opacity(int percent) { return (percent<0?0:percent>100?100:percent)/100.f; }
struct Decision { bool tint, outline; };
inline Decision Choose(bool enabled, bool held, bool hoverEnabled, bool byRelation, bool pointed,bool contour=false) {
    Decision d;
    d.tint=enabled && !contour && (held || (hoverEnabled && pointed && byRelation));
    d.outline=enabled && ((held && contour) || (hoverEnabled && pointed && (contour || !byRelation)));
    return d;
}
}
