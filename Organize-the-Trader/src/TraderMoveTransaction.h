#pragma once
#include <cstddef>
namespace TraderMoveTransaction
{
// The caller validates target bounds and pairwise non-overlap before entering.
// No target is inserted until every source is removed and every target accepted.
template <typename Moves, typename Operations>
bool Apply(const Moves& moves, Operations& ops)
{
    std::size_t removed = 0;
    for (; removed < moves.size(); ++removed)
    {
        if (!ops.remove(moves[removed]))
        {
            for (std::size_t i = 0; i < removed; ++i) ops.restore(moves[i]);
            return false;
        }
    }
    for (std::size_t i = 0; i < moves.size(); ++i)
    {
        if (!ops.canPlace(moves[i]))
        {
            for (std::size_t j = 0; j < moves.size(); ++j) ops.restore(moves[j]);
            return false;
        }
    }
    for (std::size_t i = 0; i < moves.size(); ++i) ops.addTarget(moves[i]);
    return true;
}
}
