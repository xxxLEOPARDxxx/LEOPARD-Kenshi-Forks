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
struct Stats {
    unsigned __int64 allocatedBytes, hits, misses, queued, rejected, readBytes;
    unsigned entries, pending;
};
bool Start(); // before installing archive hook; worker does no engine calls
void Configure(bool enabled, unsigned __int64 bytes, unsigned megabytesPerSecond);
FilePtr Find(const std::string& absolutePath); // non-waiting cache lock; validates file identity
void Observe(const std::string& absolutePath); // only after successful native read-only open
Stats Snapshot();
void SignalStop(); // loader-lock safe, no waits/handle close
void Stop(); // outside DllMain; cancel IO, join <=250ms; pinned worker may drain later
}
