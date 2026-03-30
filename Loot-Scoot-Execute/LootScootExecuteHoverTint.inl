#pragma once

struct HoverTintApplyDiagnostics
{
    uintptr_t candidatePtr;
    uintptr_t appearancePtr;
    uintptr_t entityPtr;
    bool hasAppearance;
    bool hasEntity;
    bool isAnimalCharacter;
    bool wantsBodyHighlight;
    bool hadTargetHandle;
    bool restoredAnimalClone;
    bool appliedAnimalClone;
    bool appliedAppearanceMaterials;
    bool appliedEntityMaterials;
    const char* reason;
};

static HoverTintApplyDiagnostics MakeHoverTintApplyDiagnostics()
{
    HoverTintApplyDiagnostics diagnostics;
    std::memset(&diagnostics, 0, sizeof(diagnostics));
    diagnostics.reason = "uninitialized";
    return diagnostics;
}
