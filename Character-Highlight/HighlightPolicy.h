#pragma once
namespace CharacterHighlightPolicy {
inline float Opacity(int percent) { return (percent<0?0:percent>100?100:percent)/100.f; }
}
