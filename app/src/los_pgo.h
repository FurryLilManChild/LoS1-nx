// LoS1-nx: profile collection of the PGO-instrumented Switch build (LOS_PGO=generate), see los_pgo.cpp.
// In every other build these do nothing.

#pragma once

namespace los::pgo {

// Picks this session's file (pgo/los_pgo_<N>.stream next to the executable) and starts the 2-minute dumps.
void Start();
// Writes every count so far to this session's file (the whole session each time, replacing the previous dump).
void DumpNow();
// The last dump, then the dump thread ends.
void Stop();

}  // namespace los::pgo
