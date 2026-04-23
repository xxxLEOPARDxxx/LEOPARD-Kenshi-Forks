#pragma once

#include <cstddef>

class InventoryPerfTimer
{
public:
    InventoryPerfTimer();

    unsigned long ElapsedMicros() const;
    unsigned long ElapsedMs() const;

private:
    long long m_startedMicros;
};

struct InventorySearchTickPerfSample
{
    InventorySearchTickPerfSample();

    bool controlsEnabled;
    bool visibleTarget;
    bool hoverTarget;
    bool filterAttempted;
    bool filterApplied;
    bool filterSkipped;
    bool targetCacheHit;
    bool visibleScanAttempted;
    bool visibleScanSkipped;
    bool hoverScanAttempted;
    unsigned long visibleTargetMicros;
    unsigned long hoverTargetMicros;
    unsigned long totalMicros;
};

class InventorySearchTickPerfScope
{
public:
    InventorySearchTickPerfScope();
    ~InventorySearchTickPerfScope();

    InventorySearchTickPerfSample& Sample();

private:
    InventoryPerfTimer m_timer;
    InventorySearchTickPerfSample m_sample;
};

struct InventorySearchFilterPerfSample
{
    InventorySearchFilterPerfSample();

    bool success;
    bool forceShowAll;
    bool hasActiveFilter;
    bool blueprintOnly;
    bool entryCacheHit;
    std::size_t previousTrackedEntries;
    std::size_t rawWidgetEntries;
    std::size_t boundEntries;
    std::size_t mergedEntries;
    std::size_t totalEntryCount;
    std::size_t visibleEntryCount;
    std::size_t visibleQuantity;
    std::size_t searchTextCacheHits;
    std::size_t searchTextCacheMisses;
    unsigned long elapsedMicros;
};

class InventorySearchFilterPerfScope
{
public:
    InventorySearchFilterPerfScope();
    ~InventorySearchFilterPerfScope();

    InventorySearchFilterPerfSample& Sample();

private:
    InventoryPerfTimer m_timer;
    InventorySearchFilterPerfSample m_sample;
};

struct InventoryDebugProbePerfSample
{
    InventoryDebugProbePerfSample();

    bool signatureChanged;
    std::size_t localCandidates;
    std::size_t globalBackpackContents;
    unsigned long elapsedMicros;
};

class InventoryDebugProbePerfScope
{
public:
    InventoryDebugProbePerfScope();
    ~InventoryDebugProbePerfScope();

    InventoryDebugProbePerfSample& Sample();

private:
    InventoryPerfTimer m_timer;
    InventoryDebugProbePerfSample m_sample;
};
