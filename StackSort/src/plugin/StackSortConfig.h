#pragma once

static const int MAX_GUIS            = 16;
static const int MAX_SECTIONS        = 4;
static const int MAX_CACHED_SECTIONS = 64;

// Upper bounds for the num_workers and top_targets settings; both size
// fixed C arrays, so they stay compile-time.
static const int MAX_WORKERS     = 8;
static const int TOP_TARGETS_MAX = 10;
