#pragma once

#include <functional>
#include <unordered_map>

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
template<typename Key, typename T, typename Hash, typename Pred, typename Alloc>
class unordered_map : public oti_boost_shim_std::unordered_map<Key, T, Hash, Pred, Alloc>
{
public:
    typedef oti_boost_shim_std::unordered_map<Key, T, Hash, Pred, Alloc> base_type;

    unordered_map()
        : base_type()
    {
    }
};

template<typename Key, typename T, typename Hash, typename Pred, typename Alloc>
class unordered_multimap : public oti_boost_shim_std::unordered_multimap<Key, T, Hash, Pred, Alloc>
{
public:
    typedef oti_boost_shim_std::unordered_multimap<Key, T, Hash, Pred, Alloc> base_type;

    unordered_multimap()
        : base_type()
    {
    }
};
}

using unordered::unordered_map;
using unordered::unordered_multimap;
}
