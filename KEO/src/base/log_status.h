#pragma once
#include <string>
// Mixed config/benchmark callback protocol. Normal retired/DEV-only notices
// and progress remain detailed; invalid-input/refusal/failure messages are errors.
inline bool DiagnosticStatusIsError(const std::string& line)
{
    // The game-free benchmark modules have an established text callback.
    // Only their explicit rejected-input forms are errors. The normal sweep
    // precedence notice starts "Bench sweep:" and is intentionally excluded.
    const bool badBenchInput = line.compare(0, 7, "Bench: ") == 0
        && (line.find(" ignored") != std::string::npos
            || line.find(", dropped") != std::string::npos
            || line.find("not resolved (past ") != std::string::npos);
    if (line.compare(0, 7, "Bench: ") == 0)
        return badBenchInput
            || line.compare(0, 21, "Bench: unknown lever ") == 0
            || line.find(" refused (") != std::string::npos
            || line.find(" named no known lever") != std::string::npos;
    if (line.compare(0, 13, "Bench sweep: ") == 0)
        return line.compare(0, 20, "Bench sweep: refused") == 0;
    // Config messages use explicit validation words. Successful benchmark
    // values (e.g. a particle template named "unknown") must stay detailed.
    return line.find("unknown") != std::string::npos
        || line.find("invalid") != std::string::npos
        || line.find("refused") != std::string::npos
        || line.find("failed") != std::string::npos
        || line.find("clamped") != std::string::npos;
}
