// LoS1-nx: stable copies of the resource headers a draw uses, for Renut's renderer.
//
// Mirror of Fate builds its Direct3D resource headers (textures, vertex buffers, index buffers, like its render
// target surfaces: los_render_targets.cpp) in memory it reuses all the time. Counted on the Linux build at the title
// screen (LOS_FRAME_DIAG, 2026-10-23): a header address bound to a draw held other words than at its previous draw in
// 42 % of the index buffer checks and about 10 % of the texture and vertex buffer ones. Renut's render thread reads
// a draw's headers through the device's pointers a frame or more later, so a draw could find another resource's
// header there: whole layers missing in single frames on Mark's PC (title screen and in game, D3D12 and Vulkan
// alike), while the game issued the same 44 draws with the same signature in every frame.
//
// So, around each draw (game thread), the device's texture, vertex buffer and index buffer pointers point at copies
// that do not change, and Renut takes its snapshot of the device with those:
// - a texture's copy is permanent and stands for the texture's memory (base address, format, size, pitch, mip
//   address), as the GPU sees it: Renut keys its host textures by the header address, and a render-to-texture result
//   (a resolve into one header) must be found by a draw that samples the same memory through another header. The
//   resolve hook (renut_engine/gpu/d3d/hooks/draw.cpp, marked LoS1-nx) uses the same copy;
// - vertex and index buffer copies come from a ring by content (identical headers share a copy): only the words
//   matter there, and the ring holds many frames' worth.

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include <rex/logging.h>

namespace renut::gpu::records {
uint32_t AllocPhysicalBlock(uint32_t size);  // renut_engine/gpu/guest/guest_records.h
}

namespace los {
namespace {

constexpr uint32_t kTextureHeaderSize = 52;  // D3DBaseTexture
constexpr uint32_t kBufferHeaderSize = 32;   // D3DVertexBuffer, D3DIndexBuffer
constexpr uint32_t kSlot = 64;
constexpr uint32_t kTextureSlots = 65536;  // 4 MB, permanent
constexpr uint32_t kBufferSlots = 65536;   // 4 MB, a ring

uint32_t Load32(const uint8_t* base, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, base + a, 4);
  return __builtin_bswap32(v);
}

bool InHeap(uint32_t va) { return va >= 0x40000000u && va < 0xC0000000u; }

// LOS_HEADER_COPIES: 1 buffers, 2 textures, 4 the resolve destination, 8 render target surfaces (default 15, all).
unsigned Parts() {
  static const unsigned v = [] {
    const char* e = std::getenv("LOS_HEADER_COPIES");
    return e ? unsigned(std::atoi(e)) : 15u;
  }();
  return v;
}

struct Key {
  uint32_t w[5];
  bool operator==(const Key&) const = default;
};
struct KeyHash {
  size_t operator()(const Key& k) const {
    uint64_t h = 0x9E3779B97F4A7C15ull;
    for (uint32_t v : k.w) h = (h ^ v) * 0xC2B2AE3D27D4EB4Full;
    return size_t(h ^ (h >> 31));
  }
};

struct Content {
  std::array<uint8_t, kBufferHeaderSize> b;
  bool operator==(const Content&) const = default;
};
struct ContentHash {
  size_t operator()(const Content& c) const {
    uint64_t h = 0x9E3779B97F4A7C15ull;
    for (size_t i = 0; i < c.b.size(); i += 8) {
      uint64_t v;
      std::memcpy(&v, c.b.data() + i, 8);
      h = (h ^ v) * 0xC2B2AE3D27D4EB4Full;
    }
    return size_t(h ^ (h >> 31));
  }
};

std::mutex g_mutex;
uint32_t g_texture_block = 0, g_texture_used = 0;
std::unordered_map<Key, uint32_t, KeyHash> g_textures;  // the texture's memory -> its copy
uint32_t g_buffer_block = 0, g_buffer_next = 0;
std::unordered_map<Content, uint32_t, ContentHash> g_buffers;  // header words -> slot
std::array<Content, kBufferSlots>* g_buffer_owner = nullptr;   // what each ring slot holds
bool g_failed = false;
constexpr uint32_t kSurfaceHeaderSize = 48;  // D3DSurface
constexpr uint32_t kSurfaceSlots = 4096;     // 256 KB, permanent
uint32_t g_surface_block = 0, g_surface_used = 0;
struct SurfaceContent {
  std::array<uint8_t, kSurfaceHeaderSize> b;
  bool operator==(const SurfaceContent&) const = default;
};
struct SurfaceContentHash {
  size_t operator()(const SurfaceContent& c) const {
    uint64_t h = 0x9E3779B97F4A7C15ull;
    for (size_t i = 0; i < c.b.size(); i += 8) {
      uint64_t v;
      std::memcpy(&v, c.b.data() + i, 8);
      h = (h ^ v) * 0xC2B2AE3D27D4EB4Full;
    }
    return size_t(h ^ (h >> 31));
  }
};
std::unordered_map<SurfaceContent, uint32_t, SurfaceContentHash> g_surfaces;  // header words -> copy
uint8_t* g_base = nullptr;  // the guest memory base, from the first draw

bool Init() {
  if (g_texture_block || g_failed) return !g_failed;
  g_texture_block = renut::gpu::records::AllocPhysicalBlock(kTextureSlots * kSlot);
  g_buffer_block = renut::gpu::records::AllocPhysicalBlock(kBufferSlots * kSlot);
  g_buffer_owner = new std::array<Content, kBufferSlots>();
  g_surface_block = renut::gpu::records::AllocPhysicalBlock(kSurfaceSlots * kSlot);
  if (!g_texture_block || !g_buffer_block || !g_surface_block) {
    g_failed = true;
    REXLOG_ERROR("[los] resource header copies: no guest memory; the renderer reads the game's headers");
    return false;
  }
  REXLOG_INFO("[los] resource header copies at {:08X} (textures) and {:08X} (buffers)", g_texture_block,
              g_buffer_block);
  return true;
}

// The header's words with the reference count and the two fences (+0x04..+0x0F) cleared: Renut does not read
// them, and they change while the resource stays the same.
void CopyHeader(uint8_t* base, uint32_t dst, uint32_t src, uint32_t size) {
  std::memcpy(base + dst, base + src, size);
  std::memset(base + dst + 4, 0, 12);
}

}  // namespace

uint32_t TextureCopyLocked(uint8_t* base, uint32_t va) {
  if (!(Parts() & 2) || !InHeap(va) || !Init()) return va;
  const uint32_t f = va + 0x1C;  // the fetch constant
  const uint32_t d0 = Load32(base, f), d1 = Load32(base, f + 4), d2 = Load32(base, f + 8),
                 d5 = Load32(base, f + 20);
  // pitch and tiling; format, endian, base address; size; mip address and dimension (not the sampler fields)
  // (Not the header's Common word: Direct3D's own headers carry a "made by Direct3D" bit the game's do not.)
  const Key key{{d0 & 0xFFC00000u, d1 & ~0x800u, d2, d5 & 0xFFFFFE00u, 0}};
  // A header Direct3D made itself (Common bit 31: the back and front buffers' textures) lives as long as the device
  // and is what the Swap presents by address: it stands for its memory itself.
  if (Load32(base, va) & 0x80000000u) {
    g_textures.insert_or_assign(key, va);
    return va;
  }
  auto it = g_textures.find(key);
  uint32_t copy;
  if (it != g_textures.end()) {
    if (it->second < g_texture_block || it->second >= g_texture_block + kTextureSlots * kSlot)
      return it->second;  // Direct3D's own header
    copy = it->second;
  } else {
    if (g_texture_used == kTextureSlots) {
      static bool logged = false;
      if (!logged) REXLOG_WARN("[los] texture header copies full ({}); later textures read the game's", kTextureSlots);
      logged = true;
      return va;
    }
    copy = g_texture_block + kSlot * g_texture_used++;
    g_textures.emplace(key, copy);
  }
  // The newest words (the sampler fields may differ between uses): Renut reads its sampler state from the device.
  CopyHeader(base, copy, va, kTextureHeaderSize);
  return copy;
}

uint32_t BufferCopyLocked(uint8_t* base, uint32_t va) {
  if (!(Parts() & 1) || !InHeap(va) || !Init()) return va;
  Content c;
  std::memcpy(c.b.data(), base + va, kBufferHeaderSize);
  std::memset(c.b.data() + 4, 0, 12);
  if (auto it = g_buffers.find(c); it != g_buffers.end()) return g_buffer_block + kSlot * it->second;
  const uint32_t slot = g_buffer_next;
  g_buffer_next = (g_buffer_next + 1) % kBufferSlots;
  Content& old = (*g_buffer_owner)[slot];
  if (auto it = g_buffers.find(old); it != g_buffers.end() && it->second == slot) g_buffers.erase(it);
  old = c;
  g_buffers.emplace(c, slot);
  const uint32_t copy = g_buffer_block + kSlot * slot;
  std::memcpy(base + copy, c.b.data(), kBufferHeaderSize);
  return copy;
}

// A render target surface: one permanent copy per set of header words. The game builds a surface object for each
// SetRenderTarget in memory it reuses, for targets of other sizes too: Mark's PC trace (2026-10-24, 150 title frames)
// had the surface at 50139FF0 as 1280x720 in some frames and 640x360 in others, and the renderer, which kept each
// address's newest words (RememberBoundSurface), took a later bind's size for an earlier pass when the game ran ahead
// (the whole scene black in single frames). Direct3D's own surfaces (Common bit 31) stand for themselves.
uint32_t SurfaceCopyLocked(uint8_t* base, uint32_t va) {
  if (!(Parts() & 8) || !InHeap(va) || !Init()) return va;
  if (Load32(base, va) & 0x80000000u) return va;
  SurfaceContent c;
  std::memcpy(c.b.data(), base + va, kSurfaceHeaderSize);
  std::memset(c.b.data() + 4, 0, 12);
  if (auto it = g_surfaces.find(c); it != g_surfaces.end()) return it->second;
  if (g_surface_used == kSurfaceSlots) {
    static bool logged = false;
    if (!logged) REXLOG_WARN("[los] surface header copies full ({}); later surfaces read the game's", kSurfaceSlots);
    logged = true;
    return va;
  }
  const uint32_t copy = g_surface_block + kSlot * g_surface_used++;
  std::memcpy(base + copy, c.b.data(), kSurfaceHeaderSize);
  g_surfaces.emplace(c, copy);
  return copy;
}

// The device's resource pointers (big-endian words at the offsets in renut_engine/gpu/d3d/d3d.h).
// los1-nx: LoS's (renut_engine/gpu/d3d/d3d.h; SA2/MoF 12812, 12836, 12920, 12816).
constexpr uint32_t kIndexBuffer = 12612, kStreams = 12636, kTextures = 12720, kRenderTargets = 12616;
constexpr uint32_t kStreamCount = 16, kTextureCount = 26, kSurfaceCount = 5;  // 4 colour targets, then depth (12632)
constexpr uint32_t kSaved = 1 + kStreamCount + kTextureCount + kSurfaceCount;
static_assert(kSaved == 48, "los_renut_glue.cpp declares SavedPointers with 48 words");

struct SavedPointers {
  uint32_t device = 0;
  uint32_t words[kSaved];
};

static void Swap(uint8_t* base, uint32_t at, uint32_t copy, uint32_t* saved) {
  std::memcpy(saved, base + at, 4);
  const uint32_t v = __builtin_bswap32(copy);
  std::memcpy(base + at, &v, 4);
}

// Points the device at the copies; RestorePointers puts the game's back.
void UseCopies(uint8_t* base, uint32_t device, SavedPointers& s) {
  s.device = device;
  g_base = base;
  std::lock_guard lock(g_mutex);
  uint32_t n = 0;
  Swap(base, device + kIndexBuffer, BufferCopyLocked(base, Load32(base, device + kIndexBuffer)), &s.words[n++]);
  for (uint32_t i = 0; i < kStreamCount; ++i) {
    const uint32_t at = device + kStreams + 4 * i;
    Swap(base, at, BufferCopyLocked(base, Load32(base, at)), &s.words[n++]);
  }
  for (uint32_t i = 0; i < kTextureCount; ++i) {
    const uint32_t at = device + kTextures + 4 * i;
    Swap(base, at, TextureCopyLocked(base, Load32(base, at)), &s.words[n++]);
  }
  for (uint32_t i = 0; i < kSurfaceCount; ++i) {
    const uint32_t at = device + kRenderTargets + 4 * i;
    Swap(base, at, SurfaceCopyLocked(base, Load32(base, at)), &s.words[n++]);
  }
}

void RestorePointers(uint8_t* base, const SavedPointers& s) {
  uint32_t n = 0;
  std::memcpy(base + s.device + kIndexBuffer, &s.words[n++], 4);
  for (uint32_t i = 0; i < kStreamCount; ++i) std::memcpy(base + s.device + kStreams + 4 * i, &s.words[n++], 4);
  for (uint32_t i = 0; i < kTextureCount; ++i) std::memcpy(base + s.device + kTextures + 4 * i, &s.words[n++], 4);
  for (uint32_t i = 0; i < kSurfaceCount; ++i)
    std::memcpy(base + s.device + kRenderTargets + 4 * i, &s.words[n++], 4);
}

// For Renut's clear and resolve hooks (game thread): the same copies around their queueing.
thread_local SavedPointers t_hook_saved;
thread_local bool t_hook_active = false;

// For the resolve hook: the copy that stands for a texture's memory.
uint32_t ResolveTextureCopy(uint8_t* base, uint32_t va) {
  if (!(Parts() & 4)) return va;
  std::lock_guard lock(g_mutex);
  return TextureCopyLocked(base, va);
}

}  // namespace los

// Called from Renut's resolve and Swap hooks (renut_engine/gpu/d3d/hooks/draw.cpp, device.cpp): the resolve's
// destination and the front buffer the Swap presents (the game makes its own front buffers) as the copy the draws
// sample through. Before the first draw: the address itself.
extern "C" uint32_t los_resolve_texture_copy(uint8_t* base, uint32_t va) { return los::ResolveTextureCopy(base, va); }
extern "C" void los_copies_begin(uint8_t* base, uint32_t device) {
  if (los::t_hook_active) return;
  los::t_hook_active = true;
  los::UseCopies(base, device, los::t_hook_saved);
}
extern "C" void los_copies_end(uint8_t* base) {
  if (!los::t_hook_active) return;
  los::RestorePointers(base, los::t_hook_saved);
  los::t_hook_active = false;
}
extern "C" uint32_t los_texture_copy(uint32_t va) {
  return los::g_base && va ? los::ResolveTextureCopy(los::g_base, va) : va;
}
