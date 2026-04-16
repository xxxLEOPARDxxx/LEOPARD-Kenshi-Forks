#pragma once

#include <functional>
#include <unordered_set>

// Local shim to avoid an external Boost header dependency for KenshiLib's
// unordered container declarations. This project only needs the container names.
#if defined(_MSC_VER) && _MSC_VER < 1700
namespace oti_boost_shim_std = std::tr1;
#else
namespace oti_boost_shim_std = std;
#endif

namespace boost
{
#ifndef OTI_BOOST_HASH_DEFINED
#define OTI_BOOST_HASH_DEFINED
template<typename T>
struct hash : public oti_boost_shim_std::hash<T>
{
};
#endif

namespace unordered
{
template<typename Key, typename Hash, typename Pred, typename Alloc>
class unordered_set : public oti_boost_shim_std::unordered_set<Key, Hash, Pred, Alloc>
{
public:
    typedef oti_boost_shim_std::unordered_set<Key, Hash, Pred, Alloc> base_type;

    unordered_set()
        : base_type()
    {
    }
};

template<typename Key, typename Hash, typename Pred, typename Alloc>
class unordered_multiset : public oti_boost_shim_std::unordered_multiset<Key, Hash, Pred, Alloc>
{
public:
    typedef oti_boost_shim_std::unordered_multiset<Key, Hash, Pred, Alloc> base_type;

    unordered_multiset()
        : base_type()
    {
    }
};
}

using unordered::unordered_set;
using unordered::unordered_multiset;
}
