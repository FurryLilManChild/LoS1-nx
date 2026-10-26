// LoS1-nx: functions sa2-nx's enhancement switches (src/cpu/sa2_enhancements.cpp) call, which sa2-nx defines in
// files this port does not have: its post-codegen self-checks. There are none here. Built with or without Renut's
// renderer.

namespace sa2::enh {
void CodegenChecksInit() {}
void CodegenChecksReport(int) {}
void FormsSelfCheck() {}
}  // namespace sa2::enh
