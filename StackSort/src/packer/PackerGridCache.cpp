#include "Packer.h"

namespace Packer
{

namespace Cache
{

bool GridCacheLookup(PackContext& ctx, int gridW, int gridH, int& outLerArea, int& outLerWidth, int& outLerHeight,
                     int& outLerX, int& outLerY, double& outConcentration, int& outStrandedCells)
{
    unsigned long long hA       = ctx.cache.curHashA;
    const unsigned char* curPtr = &ctx.grid[0];

    // Pure memo of a pure function of ctx.grid, so a false hit can only come
    // from a 64-bit Zobrist collision -- the same exposure the old linear
    // table had, just spread over 512 entries instead of 64.
    const int base = (int)(hA & (unsigned long long)(GRID_CACHE_SETS - 1)) * GRID_CACHE_WAYS;

    for (int w = 0; w < GRID_CACHE_WAYS; ++w)
    {
        const GridCacheEntry& e = ctx.cache.entries[base + w];
        if (e.gen != ctx.cache.curGen || e.hashA != hA) continue;
        outLerArea       = e.lerArea;
        outLerWidth      = e.lerWidth;
        outLerHeight     = e.lerHeight;
        outLerX          = e.lerX;
        outLerY          = e.lerY;
        outConcentration = e.concentration;
        outStrandedCells = e.strandedCells;
        return true;
    }

    Ler::ComputeLERCtx(ctx, curPtr, gridW, gridH, outLerArea, outLerWidth, outLerHeight, outLerX, outLerY);
    outStrandedCells = 0;
    outConcentration = Ler::ComputeConcentrationAndStrandedCtx(ctx, gridW, gridH, outLerX, outLerY, outLerWidth,
                                                               outLerHeight, outStrandedCells);

    // Fill a stale way first, else evict round-robin within the set.
    int victim = -1;
    for (int w = 0; w < GRID_CACHE_WAYS; ++w)
    {
        if (ctx.cache.entries[base + w].gen != ctx.cache.curGen)
        {
            victim = w;
            break;
        }
    }
    int set = base / GRID_CACHE_WAYS;
    if (victim < 0)
    {
        victim                 = ctx.cache.wayHead[set];
        ctx.cache.wayHead[set] = (unsigned char)((victim + 1) & (GRID_CACHE_WAYS - 1));
    }

    GridCacheEntry& dst = ctx.cache.entries[base + victim];
    dst.gen             = ctx.cache.curGen;
    dst.hashA           = hA;
    dst.lerArea         = outLerArea;
    dst.lerWidth        = outLerWidth;
    dst.lerHeight       = outLerHeight;
    dst.lerX            = outLerX;
    dst.lerY            = outLerY;
    dst.concentration   = outConcentration;
    dst.strandedCells   = outStrandedCells;

    return false;
}

} // namespace Cache

} // namespace Packer
