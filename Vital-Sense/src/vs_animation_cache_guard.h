#pragma once

namespace vs_animation_cache_guard
{

// Attempts to verify OgreMain_x64.dll binary fingerprint and install the 64-way striped
// lock detour guard around Ogre::Animation cache rebuild paths.
// Returns true if all 4 Ogre hooks were successfully installed; false otherwise.
bool Install();

// Returns whether the cache guard is currently active.
bool Ready();

} // namespace vs_animation_cache_guard
