// LoS1-nx: the PC build's checks of the post-codegen steps (2026-11-01), from sa2-nx's src/cpu/sa2_codegen_checks.cpp
// (GPL-3.0), the parts tools/post_codegen.py's steps need:
//   - tools/ro_constants.py: a load from the image's read-only data folded to a constant still loads on the PC, compares
//     (g_rex_ro_checked counts) and calls RexRoMismatch when guest memory does not hold the constant. On the Switch the
//     constant is used directly and none of this runs.
// RexFlushFlowMismatch (tools/flush_mode_flow.py) is in sa2_enhancements.cpp.
#include <atomic>
#include <cstdint>

#include <rex/logging.h>

#include "sa2_enhancements.h"

extern "C" {
volatile unsigned long long g_rex_ro_checked = 0;  // plain lossy count: "at least this many"
}

namespace {
std::atomic<uint64_t> g_ro_mismatches{0};
}

extern "C" void RexRoMismatch(unsigned address) noexcept {
  sa2::enh::VerifyDifference();
  if (g_ro_mismatches.fetch_add(1, std::memory_order_relaxed) < 16) {
    REXLOG_ERROR("[enh] DIFFERENCE: read-only constants: guest memory at {:08X} does not hold the constant the "
                 "generated code uses",
                 address);
  }
}
