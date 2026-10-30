#include "Packer.h"
#include "PackerScoringInline.h"

#include <algorithm>
#include <cstring>
#include <vector>

#ifdef STACKSORT_VALIDATE_GROUPING
#include <cstdio>
#include <cstdlib>
#endif

namespace Packer
{

namespace Scoring
{

void BuildPairWeightMatrix(PackContext& ctx, const std::vector<Item>& items)
{
    int n = (int)items.size();
    // assign() overwrites in place when capacity >= n*n; InitPackContext
    // reserved exactly that much, so this is a memset, not an allocation.
    ctx.grouping.pairWeightMatrix.assign((size_t)n * (size_t)n, (unsigned char)0);
    ctx.grouping.pairWeightMatrixN = n;
    for (int i = 0; i < n; ++i)
    {
        for (int j = i + 1; j < n; ++j)
        {
            unsigned char w = (unsigned char)PairWeight(items[i], items[j], ctx);
            ctx.grouping.pairWeightMatrix[(size_t)i * (size_t)n + (size_t)j] = w;
            ctx.grouping.pairWeightMatrix[(size_t)j * (size_t)n + (size_t)i] = w;
        }
    }
}

void BuildAdjGraph(AdjGraph& g, const std::vector<Placement>& placements)
{
    int n = (int)placements.size();
    if (n > 256) return;
    memset(g.count, 0, n * sizeof(int));

    for (int i = 0; i < n; ++i)
    {
        for (int j = i + 1; j < n; ++j)
        {
            int b = Geometry::SharedBorder(placements[i], placements[j]);
            if (b > 0)
            {
                // Drop symmetrically if either side is full --
                // ComputeGroupingBonusAdj walks j > i only, so a one-sided
                // entry would be silently missed.
                if (g.count[i] < 24 && g.count[j] < 24)
                {
                    g.adj[i][g.count[i]].neighbor = j;
                    g.adj[i][g.count[i]].border   = b;
                    ++g.count[i];
                    g.adj[j][g.count[j]].neighbor = i;
                    g.adj[j][g.count[j]].border   = b;
                    ++g.count[j];
                }
            }
        }
    }
}

namespace
{

// Enumerate contact pairs by walking each placement's four sides in the
// placement-id grid instead of testing all n(n-1)/2 rect pairs. The pair set
// is identical: SharedBorder is non-zero only when the rects overlap on one
// axis and abut on the other, which always leaves a shared edge cell, and a
// rectangle pair cannot abut on both axes, so each neighbour appears on
// exactly one side as one contiguous run. Dropping consecutive repeats along
// a side therefore visits every contact pair exactly once.
//
// Requires ctx.placementIdGrid to describe `placements` -- true immediately
// after SkylinePack or RestoreSkylineState, and nowhere else.
void AccumulateFromPidGrid(const std::vector<Placement>& placements, const std::vector<Item>& items,
                           const PackContext& ctx, int n, int gridW, int gridH, bool softActive,
                           const unsigned char* matBase, int matN, int parent[], int compBorders[], int softParent[],
                           int softCompBorders[])
{
    const unsigned char* pid = &ctx.placementIdGrid[0];

    for (int i = 0; i < n; ++i)
    {
        const Placement& pi = placements[i];
        const int idA       = pi.id;
        const int exA       = items[idA].exactId;

        for (int side = 0; side < 4; ++side)
        {
            int cell, step, count;
            if (side == 0)
            {
                if (pi.y == 0) continue;
                cell  = (pi.y - 1) * gridW + pi.x;
                step  = 1;
                count = pi.w;
            }
            else if (side == 1)
            {
                if (pi.y + pi.h >= gridH) continue;
                cell  = (pi.y + pi.h) * gridW + pi.x;
                step  = 1;
                count = pi.w;
            }
            else if (side == 2)
            {
                if (pi.x == 0) continue;
                cell  = pi.y * gridW + (pi.x - 1);
                step  = gridW;
                count = pi.h;
            }
            else
            {
                if (pi.x + pi.w >= gridW) continue;
                cell  = pi.y * gridW + (pi.x + pi.w);
                step  = gridW;
                count = pi.h;
            }

            int prev = -1;
            for (int k = 0; k < count; ++k, cell += step)
            {
                const int j = pid[cell];
                if (j == PLACEMENT_ID_EMPTY || j == prev) continue;
                prev = j;
                if (j <= i) continue;

                int shared = Geometry::SharedBorder(pi, placements[j]);
                if (shared <= 0) continue;

                const int idB = placements[j].id;
                if (exA >= 0 && items[idB].exactId == exA)
                {
                    uf_unite(parent, i, j);
                    compBorders[uf_find(parent, i)] += shared;
                }
                else if (softActive)
                {
                    int w = matBase ? matBase[(size_t)idA * (size_t)matN + (size_t)idB]
                                    : PairWeight(items[idA], items[idB], ctx);
                    if (w <= 0) continue;
                    uf_unite(softParent, i, j);
                    softCompBorders[uf_find(softParent, i)] += shared * w;
                }
            }
        }
    }
}

// Shared accumulator for the three grouping scorers. Fills caller-owned
// union-find arrays with per-component border totals. Three enumeration
// strategies dispatched on (g, softActive):
//   - g != NULL              : walk prebuilt AdjGraph edges (ComputeGroupingBonusAdj)
//   - g == NULL, !softActive : sort placements by exactId and enumerate only
//                              within-run pairs (fast path, ComputeGroupingBonus)
//   - g == NULL,  softActive : full O(n²) pair scan with soft-track uniting
//                              (ComputeGroupingBonus + ComputeGroupingBordersRaw)
// Always runs the union-find fixup pass so callers can index compBorders[]
// by root index.

void AccumulateGroupingComponents(const std::vector<Placement>& placements, const std::vector<Item>& items,
                                  const PackContext& ctx, int n, const AdjGraph* g, int parent[], int compBorders[],
                                  int softParent[], int softCompBorders[])
{
    // The soft arrays are read only under softActive, here and in
    // FinalizeGroupingBonus, so leave them untouched when the track is off.
    bool softActive = ctx.grouping.softGroupingPct > 0;
    for (int i = 0; i < n; ++i)
    {
        parent[i]      = i;
        compBorders[i] = 0;
    }
    if (softActive)
    {
        for (int i = 0; i < n; ++i)
        {
            softParent[i]      = i;
            softCompBorders[i] = 0;
        }
    }

    const int matN = ctx.grouping.pairWeightMatrixN;
    const unsigned char* matBase =
        softActive && (matN > 0) ? &ctx.grouping.pairWeightMatrix[0] : (const unsigned char*)0;

    if (g != NULL)
    {
        for (int i = 0; i < n; ++i)
        {
            int idA = placements[i].id;
            int exA = items[idA].exactId;
            for (int k = 0; k < g->count[i]; ++k)
            {
                int j = g->adj[i][k].neighbor;
                if (j <= i) continue;
                int idB    = placements[j].id;
                int border = g->adj[i][k].border;
                if (exA >= 0 && items[idB].exactId == exA)
                {
                    uf_unite(parent, i, j);
                    compBorders[uf_find(parent, i)] += border;
                }
                else if (softActive)
                {
                    int w = matBase ? matBase[(size_t)idA * (size_t)matN + (size_t)idB]
                                    : PairWeight(items[idA], items[idB], ctx);
                    if (w <= 0) continue;
                    uf_unite(softParent, i, j);
                    softCompBorders[uf_find(softParent, i)] += border * w;
                }
            }
        }
    }
    else if (!softActive)
    {
        // Fast path: same-exactId pairs only can unite. Sort by exactId and
        // enumerate within-run pairs; union order is irrelevant since the
        // fixup pass consolidates per-component totals at the root.
        int exBuf[256];
        int ixBuf[256];
        int m = 0;
        for (int i = 0; i < n; ++i)
        {
            int ex = items[placements[i].id].exactId;
            if (ex < 0) continue;
            exBuf[m] = ex;
            ixBuf[m] = i;
            ++m;
        }
        for (int p = 1; p < m; ++p)
        {
            int keyEx = exBuf[p];
            int keyIx = ixBuf[p];
            int q     = p - 1;
            while (q >= 0 && exBuf[q] > keyEx)
            {
                exBuf[q + 1] = exBuf[q];
                ixBuf[q + 1] = ixBuf[q];
                --q;
            }
            exBuf[q + 1] = keyEx;
            ixBuf[q + 1] = keyIx;
        }
        int runStart = 0;
        while (runStart < m)
        {
            int runEnd = runStart + 1;
            while (runEnd < m && exBuf[runEnd] == exBuf[runStart])
                ++runEnd;
            for (int pi = runStart; pi < runEnd; ++pi)
            {
                int i = ixBuf[pi];
                for (int pj = pi + 1; pj < runEnd; ++pj)
                {
                    int j      = ixBuf[pj];
                    int shared = Geometry::SharedBorder(placements[i], placements[j]);
                    if (shared <= 0) continue;
                    uf_unite(parent, i, j);
                    compBorders[uf_find(parent, i)] += shared;
                }
            }
            runStart = runEnd;
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            int idA = placements[i].id;
            int exA = items[idA].exactId;
            for (int j = i + 1; j < n; ++j)
            {
                int shared = Geometry::SharedBorder(placements[i], placements[j]);
                if (shared <= 0) continue;
                int idB = placements[j].id;
                if (exA >= 0 && items[idB].exactId == exA)
                {
                    uf_unite(parent, i, j);
                    compBorders[uf_find(parent, i)] += shared;
                }
                else
                {
                    int w = matBase ? matBase[(size_t)idA * (size_t)matN + (size_t)idB]
                                    : PairWeight(items[idA], items[idB], ctx);
                    if (w <= 0) continue;
                    uf_unite(softParent, i, j);
                    softCompBorders[uf_find(softParent, i)] += shared * w;
                }
            }
        }
    }

    // Fixup: collect borders scattered at intermediate roots to final roots
    // so callers can index compBorders[] by the current root.
    for (int i = 0; i < n; ++i)
    {
        int root = uf_find(parent, i);
        if (root != i && compBorders[i] > 0)
        {
            compBorders[root] += compBorders[i];
            compBorders[i] = 0;
        }
    }
    if (softActive)
    {
        for (int i = 0; i < n; ++i)
        {
            int root = uf_find(softParent, i);
            if (root != i && softCompBorders[i] > 0)
            {
                softCompBorders[root] += softCompBorders[i];
                softCompBorders[i] = 0;
            }
        }
    }
}

// Sum per-component totals through applyGroupingPower and combine exact +
// soft tracks. exactQuarters / softQuarters select the power exponent (4 =
// linear pass-through used by BordersRaw). outExactOnly receives the
// exact-track contribution pre-soft — NULL when the caller doesn't need it.

long long FinalizeGroupingBonus(int n, int parent[], const int compBorders[], int softParent[],
                                const int softCompBorders[], int exactQuarters, int softQuarters, int softPct,
                                long long* outExactOnly)
{
    long long exactBonus = 0;
    for (int i = 0; i < n; ++i)
    {
        if (uf_find(parent, i) == i && compBorders[i] > 0)
            exactBonus += applyGroupingPower(compBorders[i], exactQuarters);
    }
    if (outExactOnly) *outExactOnly = exactBonus;

    if (softPct <= 0) return exactBonus;

    long long softBonus = 0;
    for (int i = 0; i < n; ++i)
    {
        if (uf_find(softParent, i) == i && softCompBorders[i] > 0)
            softBonus += applyGroupingPower(softCompBorders[i] / 100, softQuarters);
    }
    return exactBonus + softBonus * softPct / 100;
}

} // namespace

// O(E) scorer over a prebuilt AdjGraph. Used by OptimizeGrouping / StripShift
// / TileSwap where the same graph is re-scored many times per pack.
long long ComputeGroupingBonusAdj(const std::vector<Placement>& placements, const std::vector<Item>& items,
                                  const AdjGraph& g, int n, const PackContext& ctx)
{
    if (n <= 1 || n > 256) return 0;
    int parent[256], compBorders[256], softParent[256], softCompBorders[256];
    AccumulateGroupingComponents(placements, items, ctx, n, &g, parent, compBorders, softParent, softCompBorders);
    return FinalizeGroupingBonus(n, parent, compBorders, softParent, softCompBorders, ctx.scoring.groupingPowerQuarters,
                                 5, ctx.grouping.softGroupingPct, NULL);
}

// O(n²) scorer without a prebuilt graph. Used by the LAHC inner loop where
// a graph rebuild per iter would just duplicate the same SharedBorder scans.
long long ComputeGroupingBonus(const std::vector<Placement>& placements, const std::vector<Item>& items,
                               const PackContext& ctx, long long* outExactOnly)
{
    int n = (int)placements.size();
    if (n <= 1 || n > 256)
    {
        if (outExactOnly) *outExactOnly = 0;
        return 0;
    }
    int parent[256], compBorders[256], softParent[256], softCompBorders[256];
    AccumulateGroupingComponents(placements, items, ctx, n, NULL, parent, compBorders, softParent, softCompBorders);
    return FinalizeGroupingBonus(n, parent, compBorders, softParent, softCompBorders, ctx.scoring.groupingPowerQuarters,
                                 5, ctx.grouping.softGroupingPct, outExactOnly);
}

// Same result as ComputeGroupingBonus, enumerating contact pairs through
// ctx.placementIdGrid rather than the O(n^2) rect scan. Only valid where that
// grid still describes `placements`.
long long ComputeGroupingBonusPidGrid(const std::vector<Placement>& placements, const std::vector<Item>& items,
                                      const PackContext& ctx, int gridW, int gridH)
{
    int n = (int)placements.size();
    if (n <= 1 || n > 256) return 0;

    int parent[256], compBorders[256], softParent[256], softCompBorders[256];
    bool softActive = ctx.grouping.softGroupingPct > 0;
    for (int i = 0; i < n; ++i)
    {
        parent[i]      = i;
        compBorders[i] = 0;
    }
    if (softActive)
    {
        for (int i = 0; i < n; ++i)
        {
            softParent[i]      = i;
            softCompBorders[i] = 0;
        }
    }

    const int matN = ctx.grouping.pairWeightMatrixN;
    const unsigned char* matBase =
        softActive && (matN > 0) ? &ctx.grouping.pairWeightMatrix[0] : (const unsigned char*)0;

    AccumulateFromPidGrid(placements, items, ctx, n, gridW, gridH, softActive, matBase, matN, parent, compBorders,
                          softParent, softCompBorders);

    for (int i = 0; i < n; ++i)
    {
        int root = uf_find(parent, i);
        if (root != i && compBorders[i] > 0)
        {
            compBorders[root] += compBorders[i];
            compBorders[i] = 0;
        }
    }
    if (softActive)
    {
        for (int i = 0; i < n; ++i)
        {
            int root = uf_find(softParent, i);
            if (root != i && softCompBorders[i] > 0)
            {
                softCompBorders[root] += softCompBorders[i];
                softCompBorders[i] = 0;
            }
        }
    }

    long long bonus = FinalizeGroupingBonus(n, parent, compBorders, softParent, softCompBorders,
                                            ctx.scoring.groupingPowerQuarters, 5, ctx.grouping.softGroupingPct, NULL);

#ifdef STACKSORT_VALIDATE_GROUPING
    // Scratch-build cross-check: the O(n^2) scorer is the reference.
    long long ref = ComputeGroupingBonus(placements, items, ctx, NULL);
    if (ref != bonus)
    {
        (void)fprintf(stderr, "GROUPING MISMATCH pidGrid=%lld nsq=%lld n=%d\n", bonus, ref, n);
        abort();
    }
#endif

    return bonus;
}

long long GroupingUpperBound(const std::vector<Item>& items, const PackContext& ctx)
{
    const int q = ctx.scoring.groupingPowerQuarters;
    if (q != 4 && q != 5 && q != 6 && q != 8) return -1;

    long long perimeter = 0;
    for (size_t i = 0; i < items.size(); ++i)
        perimeter += 2LL * ((long long)items[i].w + (long long)items[i].h);
    if (perimeter <= 0) return 0;
    if (perimeter > 0x7FFFFFFFLL) return -1;

    int b           = (int)perimeter;
    long long bound = applyGroupingPower(b, q);
    if (ctx.grouping.softGroupingPct > 0) bound += applyGroupingPower(b, 5) * ctx.grouping.softGroupingPct / 100;
    return bound;
}

// Power-independent border total. Sums weighted Σ b per component with no
// exponent applied (exactQuarters = softQuarters = 4 → applyGroupingPower
// returns b as-is). Used once per final result for cross-power diagnostics.
long long ComputeGroupingBordersRaw(const std::vector<Placement>& placements, const std::vector<Item>& items,
                                    const PackContext& ctx)
{
    int n = (int)placements.size();
    if (n <= 1 || n > 256) return 0;
    int parent[256], compBorders[256], softParent[256], softCompBorders[256];
    AccumulateGroupingComponents(placements, items, ctx, n, NULL, parent, compBorders, softParent, softCompBorders);
    return FinalizeGroupingBonus(n, parent, compBorders, softParent, softCompBorders, 4, 4,
                                 ctx.grouping.softGroupingPct, NULL);
}

} // namespace Scoring

namespace PostPack
{

// Swap same-footprint items to improve clustering.
// Physical layout unchanged since occupied cells are identical.

void OptimizeGrouping(std::vector<Placement>& placements, const std::vector<Item>& items, const PackContext& ctx)
{
    int n = (int)placements.size();
    if (n <= 1 || n > 256) return;

    // Precompute adjacency graph (O(n²) once)
    AdjGraph g;
    Scoring::BuildAdjGraph(g, placements);

    // Precompute candidate swap pairs by footprint.
    // Group placements by (w,h). Only cross-type pairs within the same
    // footprint group are valid swap candidates.
    // Cap at n * 16 pairs -- realistic footprint groups are small
    // (same w/h, different type), so this is plenty for 20x20 grids.
    struct CandPair
    {
        int i;
        int j;
    };
    static const int MAX_CANDIDATES = 256 * 16;
    CandPair candidates[MAX_CANDIDATES];
    int numCandidates = 0;

    // Sort indices by footprint key for grouping
    int sortedIdx[256];
    int footprintKey[256];
    for (int i = 0; i < n; ++i)
    {
        sortedIdx[i]    = i;
        footprintKey[i] = (placements[i].w << 16) | placements[i].h;
    }
    // Insertion sort (n <= 256, simpler than std::sort with functor)
    for (int i = 1; i < n; ++i)
    {
        int key = footprintKey[sortedIdx[i]];
        int tmp = sortedIdx[i];
        int j   = i - 1;
        while (j >= 0 && footprintKey[sortedIdx[j]] > key)
        {
            sortedIdx[j + 1] = sortedIdx[j];
            --j;
        }
        sortedIdx[j + 1] = tmp;
    }

    // Find runs of same footprint and generate cross-type candidate pairs
    int runStart = 0;
    while (runStart < n)
    {
        int runEnd = runStart + 1;
        while (runEnd < n && footprintKey[sortedIdx[runEnd]] == footprintKey[sortedIdx[runStart]])
            ++runEnd;

        // All pairs within this footprint group. Include same-type pairs
        // because swaps mutate the type at each position — a pair that starts
        // same-type may become cross-type after earlier swaps. The inner loop's
        // same-type skip handles the common case cheaply.
        for (int a = runStart; a < runEnd; ++a)
        {
            for (int b = a + 1; b < runEnd; ++b)
            {
                if (numCandidates < MAX_CANDIDATES)
                {
                    candidates[numCandidates].i = sortedIdx[a];
                    candidates[numCandidates].j = sortedIdx[b];
                    ++numCandidates;
                }
            }
        }

        runStart = runEnd;
    }

    if (numCandidates == 0) return;

    // Greedy swap loop using adjacency-based scoring
    bool improved = true;
    while (improved)
    {
        improved              = false;
        long long curGrouping = Scoring::ComputeGroupingBonusAdj(placements, items, g, n, ctx);

        for (int ci = 0; ci < numCandidates; ++ci)
        {
            int pi = candidates[ci].i;
            int pj = candidates[ci].j;

            // Skip items of the same template — swap is a no-op for scoring.
            if (items[placements[pi].id].exactId == items[placements[pj].id].exactId) continue;

            std::swap(placements[pi].id, placements[pj].id);
            long long newGrouping = Scoring::ComputeGroupingBonusAdj(placements, items, g, n, ctx);

            if (newGrouping > curGrouping)
            {
                curGrouping = newGrouping;
                improved    = true;
            }
            else
            {
                std::swap(placements[pi].id, placements[pj].id);
            }
        }
    }
}

} // namespace PostPack

} // namespace Packer
