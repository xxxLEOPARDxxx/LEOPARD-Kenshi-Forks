#pragma once
#include <string>
#include <vector>
#include <memory>
namespace resource_cache {
struct CachedFile {
    std::vector<unsigned char> data;
    unsigned long sizeHigh, sizeLow, timeHigh, timeLow;
    unsigned long volume, indexHigh, indexLow;
    unsigned __int64 charged;
    CachedFile();
    ~CachedFile();
private:
    CachedFile(const CachedFile&);
    CachedFile& operator=(const CachedFile&);
};
typedef std::shared_ptr<const CachedFile> FilePtr;
// VC10 vectorizes aggregate copies with aligned stores; keep callers aligned.
struct __declspec(align(16)) Stats {
    unsigned __int64 allocatedBytes, hits, misses, queued, rejected, readBytes;
    unsigned entries, pending;
    bool active;
    unsigned startupRoots, startupFiles, startupProcessed, startupLoaded, startupSkipped;
    unsigned __int64 startupTotalBytes, startupReadBytes;
    bool startupScanning, startupDone, startupCapped;
    unsigned __int64 startupFastReadBytes;
    bool startupMenuFast;
};
bool Start(); // before installing archive hook; worker does no engine calls
void Configure(bool enabled, unsigned __int64 bytes, unsigned megabytesPerSecond);
void RegisterTextureRoot(const std::string& absolutePath, bool recursive); // legacy API name; copied model/texture filesystem location, retained while off
void ConfigureStartup(bool enabled); // live opt-in; unchanged values do not restart inventory
bool StartupEnabled();
void SetStartupMenu(bool); // main owner heartbeat; short lease expires if rendering stops
FilePtr Find(const std::string& absolutePath); // non-waiting cache lock; validates file identity
void Observe(const std::string& absolutePath); // native read-only open: paced demand, not paused by speculative gate
void Prefetch(const std::string& absolutePath); // explicit verified metadata request, no zone activation
bool StandbyAllowed(); // GPU source pins must release when RAM cap/reserve is exceeded
void SetBackgroundAllowed(bool); // pause speculative prefetch/startup only; demand stays rate-limited, cache preserved
bool BackgroundAllowed();
Stats Snapshot();
void SignalStop(); // loader-lock safe, no waits/handle close
void Stop(); // outside DllMain; cancel IO, join <=250ms; pinned worker may drain later
}
