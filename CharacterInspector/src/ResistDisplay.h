#pragma once

// ProtectionModel stores resists as 0..1 fractions (stacks may exceed 1.0).
// Always convert with * 100 for display — do NOT treat values > 1.5 as "already percent"
// or stacked totals like 1.96 become ~2%.
inline float resistToPercent(float value)
{
    if (value < 0.f)
        return 0.f;
    return value * 100.0f;
}

// CharStats::getWeatherProtection returns an aggregated fraction. The native
// immunity branch returns 1.0, and equipment totals may legitimately exceed it.
// Do not guess units from magnitude: 1.55 means 155%, never 1.55%.
inline float normalizeResist(float value)
{
    if (value < 0.f)
        return 0.f;
    return value;
}

inline float coverageToPercent(float normalizedCoverage)
{
    return normalizedCoverage * 100.0f;
}
