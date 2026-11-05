#include "../HighlightPolicy.h"
#include <assert.h>
#include <stdio.h>
int main() {
    using namespace CharacterHighlightPolicy;
    assert(Opacity(-10)==0.f && Opacity(0)==0.f && Opacity(100)==1.f && Opacity(110)==1.f);
    assert(Opacity(35)>.349f && Opacity(35)<.351f);
    struct Scenario {bool enabled,key,hover,relation,pointed,contour,tint,outline;};
    // Expected player-visible outcomes, independent of the policy expression.
    const Scenario cases[]={
        {false,true,true,true,true,false,false,false}, // Master switch off.
        {false,true,true,true,true,true,false,false},
        {true,false,false,false,true,false,false,false}, // Existing idle default.
        {true,true,false,false,false,false,true,false}, // Existing ALT tint.
        {true,false,true,false,true,false,false,true}, // Plain beige hover.
        {true,false,true,false,false,false,false,false}, // Cursor leaves model.
        {true,false,true,true,true,false,true,false}, // Relation tint hover.
        {true,true,true,false,true,false,true,true}, // ALT tint + beige hover.
        {true,true,false,false,false,true,false,true}, // ALT contour.
        {true,false,true,true,true,true,false,true}, // Relation contour hover.
        {true,false,true,true,false,true,false,false}, // Contour leaves model.
        {true,false,false,true,true,true,false,false} // Disabled hover stays off.
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        const Scenario& s=cases[i];
        const Decision d=Choose(s.enabled,s.key,s.hover,s.relation,s.pointed,s.contour);
        assert(d.tint==s.tint && d.outline==s.outline);
    }
    puts("PASS: intensity boundaries and 12 hover/hold-key/style scenarios");
}
