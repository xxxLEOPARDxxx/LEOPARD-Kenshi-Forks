#include "Packer.h"

#include <vector>

namespace Packer
{

// Scoring tiers in ComputeScore, as measured on the Kenshi-sized corpus:
//
//   numPlaced * 1e6 dominates absolutely; nothing below can outvote it.
//   lerArea² and -strandedCells² are the space-efficiency tier.
//   TARGET_BONUS (10000) is nominally a tier bonus, but on grids this size a
//     single LER cell near the top of the range is worth more than 10000
//     (e.g. 140² - 139² = 279), so in practice it acts as a hard
//     preference for meeting the target rather than as a nudge.
//   fragWeight (default 50) really is a tiebreaker: it can never reach the
//     minimum LER delta of 3 (2² - 1²).
//   ROTATION_PENALTY = 1 is a pure tiebreaker, ~42pts at corpus item counts.
//   groupingWeight (default 1) reaches several hundred points on the
//     production config, so grouping does trade against one or two LER cells.
//     That is intended: grouping is a primary quality axis, not a tiebreak.
//
// ctx.scoring.tierScale (default 1) multiplies the whole space-efficiency tier
// -- lerArea^2, the stranded penalty and TARGET_BONUS -- so an ablation can
// widen the gap to the tiers below without touching them. At the corpus maximum
// lerArea of 180, a scale of 8 puts that term at 259,200, still well under the
// 1,000,000 per placement, so the placement-count tier stays dominant.
//
// Weights live in ctx.scoring (seeded with DEFAULT_* by InitPackContext,
// overridden per-pack by PackAnnealedH from SearchParams).

namespace Scoring
{

static const int ROTATION_PENALTY = 1;

long long ComputeScore(const PackContext& ctx, size_t numPlaced, int lerArea, int lerHeight, double concentration,
                       int target, int numRotated, long long groupingBonus, int strandedCells)
{
    const long long tierScale = (long long)ctx.scoring.tierScale;

    long long score = (long long)numPlaced * 1000000LL;
    score += tierScale * (long long)lerArea * (long long)lerArea;
    // Stranded cell penalty: interior empty cells outside the LER are
    // geometrically unreachable waste. Quadratic penalty (same tier as LER)
    // provides steep gradient away from scattered configurations.
    score -= tierScale * (long long)strandedCells * (long long)strandedCells;
    if (lerHeight >= target) score += tierScale * (long long)TARGET_BONUS;
    score += (long long)(concentration * (double)ctx.scoring.fragWeight);
    score -= (long long)numRotated * ROTATION_PENALTY;
    // Grouping bonus discounted by concentration: clustering that fragments
    // the free space gets reduced credit. concentration=1.0 (one blob) = full
    // bonus, concentration=0.3 (scattered gaps) = 30% bonus. groupingBonus
    // is already power-applied (b^(quarters/4)) by ComputeGroupingBonus.
    score += (long long)((double)groupingBonus * concentration * ctx.scoring.groupingWeight);
    return score;
}

} // namespace Scoring

namespace Grid
{

bool ValidatePlacements(int gridW, int gridH, const std::vector<Placement>& placements)
{
    std::vector<unsigned char> grid((size_t)gridW * (size_t)gridH, 0);

    for (size_t i = 0; i < placements.size(); ++i)
    {
        const Placement& p = placements[i];
        for (int dy = 0; dy < p.h; ++dy)
        {
            for (int dx = 0; dx < p.w; ++dx)
            {
                int cx = p.x + dx;
                int cy = p.y + dy;
                if (cx < 0 || cx >= gridW || cy < 0 || cy >= gridH) return false;
                int idx = cy * gridW + cx;
                if (grid[idx]) return false;
                grid[idx] = 1;
            }
        }
    }

    return true;
}

bool ValidateLayout(int gridW, int gridH, const std::vector<Item>& items, const std::vector<Placement>& placements)
{
    if (placements.size() != items.size()) return false;

    // Placement ids index the caller's items vector (not the permuted packing
    // order), so one placement per index proves every item was placed once.
    std::vector<unsigned char> seen(items.size(), 0);
    for (size_t i = 0; i < placements.size(); ++i)
    {
        const Placement& p = placements[i];
        if (p.id < 0 || (size_t)p.id >= items.size()) return false;
        if (seen[p.id]) return false;
        seen[p.id] = 1;

        const Item& it = items[p.id];
        bool sameDims  = (p.w == it.w && p.h == it.h);
        bool swapped   = (p.w == it.h && p.h == it.w);
        if (!sameDims && !(swapped && it.canRotate)) return false;
    }

    return ValidatePlacements(gridW, gridH, placements);
}

} // namespace Grid

} // namespace Packer
