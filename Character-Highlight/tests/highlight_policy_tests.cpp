#include "../HighlightPolicy.h"
#include <assert.h>
#include <stdio.h>
int main() {
    using namespace CharacterHighlightPolicy;
    assert(Opacity(-10)==0.f && Opacity(0)==0.f && Opacity(100)==1.f && Opacity(110)==1.f);
    assert(Opacity(35)>.349f && Opacity(35)<.351f);
    puts("PASS: tint intensity boundaries");
}
