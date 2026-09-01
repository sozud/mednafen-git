#include <mednafen/mednafen.h>
#include <mednafen/mednafen-driver.h>
#include <mednafen/state-driver.h>
#include <mednafen/debug.h>
#include <mednafen/psx/psx.h>
#include <mednafen/psx/gpu.h>
#include <mednafen/psx/spu.h>
#include <mednafen/video/png.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#define MIPS_J_OPCODE 0x08000000U
#define MIPS_J_TARGET_MASK 0x03FFFFFFU
#define PSX_KSEG0_BASE 0x80000000U
#define PSX_RAM_START 0x80010000U
#define PSX_RAM_END 0x80200000U
#define PSX_GPU_TAG_ADDRESS_MASK 0x00FFFFFFU
#define PSX_GPU_TAG_END 0x00FFFFFFU
#define MMX4_TITLE_GAME_STATE 0x00000D01U
#define MMX4_SP_DRAW_INFO_POS 0x1F800000U
#define MMX4_SP_LAYOUT 0x1F800004U
#define MMX4_SP_TILES 0x1F800008U
#define MMX4_SP_DESCRIPTORS 0x1F80000CU
#define MMX4_SP_PALETTE_SLOT 0x1F800014U
#define MMX4_SP_GRAPHICS_SLOT 0x1F80001CU
#define MMX4_SP_ARCHIVE_SLOT 0x1F800020U
#define MMX4_SP_ARCHIVE_OFFSET 0x1F800024U
#define MMX4_SP_PALETTE 0x1F800028U
#define MMX4_GAME_INFO 0x80173C70U
#define MMX4_ARCHIVE_DESTINATION 0x80173C80U
#define MMX4_ARCHIVE_BUFFER 0x80178000U
#define MMX4_ENGINE_OBJ 0x801721C0U
#define MMX4_ENGINE_STAGE 0x801721CCU
#define MMX4_ENGINE_SUBSTAGE 0x801721CDU
#define MMX4_ENGINE_CHECKPOINT 0x801721DDU
#define MMX4_ENGINE_CHARACTER 0x80172203U
#define MMX4_ENGINE_UPDATE_FLAG(index) (MMX4_ENGINE_OBJ + 0x10U + (index))
#define MMX4_PLAYER 0x801418C8U
#define MMX4_ENTITY 0x80175D58U
#define MMX4_MAIN_OBJECTS 0x8013BED0U
#define MMX4_VISUAL_OBJECTS 0x8013E510U
#define MMX4_SHOT_OBJECTS 0x8013F328U
#define MMX4_WEAPON_OBJECTS 0x801406F8U
#define MMX4_UNK_OBJECTS 0x801410C0U
#define MMX4_ITEM_OBJECTS 0x80165A30U
#define MMX4_MISC_OBJECTS 0x80173CA0U
#define MMX4_QUAD_OBJECTS 0x801435B0U
#define MMX4_EFFECT_OBJECTS 0x80142F98U
#define MMX4_BACKGROUND_OBJECTS 0x801419B0U
#define MMX4_CURRENT_DRAW_INFO 0x80142F80U
#define MMX4_LOADING_FRAME_COUNTER 0x80141BD8U
#define MMX4_FADE_AMOUNT 0x8016DEA4U
#define MMX4_LAYOUT_WIDTH 0x80172224U
#define MMX4_LAYOUT_HEIGHT 0x80172225U
#define MMX4_LAYOUT_SIZE 0x80172226U
#define MMX4_LAYOUT_MAP 0x8010FFDCU
#define MMX4_GRAPHICS_POINTER 0x801406A8U
#define MMX4_UPLOAD_SLOT 0x801374B4U
#define MMX4_UPLOAD_TABLE 0x80137D04U
#define MMX4_UPLOAD_SOURCE 0x8012F4B4U
#define MMX4_UPLOAD_STATE 0x80137DD8U
#define MMX4_UPLOAD_DESTINATION 0x80137CD4U
#define MMX4_SOUND_DUMP 0x80166BE8U
#define MMX4_SOUND_SAMPLE_ADDRESS 0x80166D96U
#define MMX4_SOUND_SAMPLE_ADDRESS_HIGH 0x80166D97U
#define MMX4_SOUND_SAMPLE_LOOP 0x80166DA6U
#define MMX4_SOUND_SAMPLE_LOOP_HIGH 0x80166DA7U
#define PSX_SPU_REG_1D84 0x1F801D84U
#define PSX_SPU_REG_1D85 0x1F801D85U
#define PSX_SPU_REG_1D86 0x1F801D86U
#define PSX_SPU_REG_1D87 0x1F801D87U
#define PSX_SPU_REG_1D98 0x1F801D98U
#define PSX_SPU_REG_1D99 0x1F801D99U
#define PSX_SPU_REG_1D9A 0x1F801D9AU
#define PSX_SPU_REG_1D9B 0x1F801D9BU
#define PSX_SPU_REG_1DAA 0x1F801DAAU
#define PSX_SPU_REG_1DAB 0x1F801DABU
#define MMX4_FUNC_FRAME_BOUNDARY 0x8001211CU
#define MMX4_FUNC_LOAD_COMMON_ARCHIVES 0x80012E38U
#define MMX4_FUNC_LOAD_PLAYER_ARCHIVES 0x80012EB8U
#define MMX4_FUNC_LOAD_SCENE_ARCHIVE 0x80013014U
#define MMX4_FUNC_LOADING_FRAME_BEGIN 0x8001326CU
#define MMX4_FUNC_LOADING_FRAME_END 0x800133FCU
#define MMX4_FUNC_800148EC 0x800148ECU
#define MMX4_FUNC_START_SOUND 0x8001512CU
#define MMX4_FUNC_PLAY_SOUND 0x8001540CU
#define MMX4_FUNC_COPY_STAGE_DATA 0x800160ACU
#define MMX4_FUNC_80018000 0x80018000U
#define MMX4_FUNC_800182E8 0x800182E8U
#define MMX4_FUNC_8001D064 0x8001D064U
#define MMX4_FUNC_8001DAF8 0x8001DAF8U
#define MMX4_FUNC_ENGINE_DISPATCH 0x8001FB50U
#define MMX4_FUNC_ENGINE_STATE_0 0x8001FBB8U
#define MMX4_FUNC_8001FC20 0x8001FC20U
#define MMX4_FUNC_RESET_GAME_ENGINE 0x8002A6FCU
#define MMX4_FUNC_800B2C8C 0x800B2C8CU
#define MMX4_FUNC_800CB048 0x800CB048U
#define MMX4_FUNC_800CCA34 0x800CCA34U
#define MMX4_FUNC_800CCB74 0x800CCB74U
#define MMX4_FUNC_SS_VM_KEY_ON_NOW 0x800E2C74U
#define MMX4_FUNC_800E2DB4 0x800E2DB4U
#define MMX4_SFX_FIXTURE_IDLE 0x801FFE00U
#define MMX4_SFX_FIXTURE_RETURN 0x801FFEF0U
#define MMX4_DIRECT_BOOT_RETURN 0x801FFF00U

namespace Mednafen {

void MDFND_OutputNotice(MDFN_NoticeType type, const char* text) noexcept
{
 std::fprintf(type == MDFN_NOTICE_ERROR ? stderr : stdout, "%s\n", text);
}

void MDFND_OutputInfo(const char* text) noexcept
{
 std::fputs(text, stdout);
 std::fflush(stdout);
}

void MDFND_MidSync(EmulateSpecStruct*, const unsigned) {}
bool MDFND_CheckNeedExit(void) { return false; }
void MDFND_MediaSetNotification(uint32, uint32, uint32, uint32) {}
void MDFND_SetStateStatus(StateStatusStruct*) noexcept {}
void MDFND_SetMovieStatus(StateStatusStruct*) noexcept {}
void MDFND_NetplayText(const char*, bool) {}
void MDFND_NetplaySetHints(bool, bool, uint32) {}

}

using namespace Mednafen;

static const RegGroupType* cpu_regs;
static unsigned frame_number;
static bool direct_boot_applied;
static bool sfx_fixture_enabled;
static unsigned sfx_fixture_group;
static unsigned sfx_fixture_index;
static unsigned sfx_fixture_phase;
static const char* audio_wav_path;
static bool audio_recording;
static unsigned sfx_fixture_capture_frames = 360;
static unsigned sfx_fixture_capture_start;

static void write_ppm(const char* path, const MDFN_Surface& surface,
                      const MDFN_Rect& rect, const int32* line_widths);
static uint32 peek32(uint32 address);
static uint8 peek8(uint32 address);
static uint16 peek16(uint32 address);

static bool has_visible_pixels(const MDFN_Surface& surface,
                               const MDFN_Rect& rect, const int32* line_widths)
{
 const unsigned h = rect.h ? rect.h : surface.h;
 uint64 visible = 0, count = 0;
 for(unsigned y = 0; y < h; y++)
 {
  const unsigned w = line_widths && line_widths[0] != ~0
      ? line_widths[rect.y + y] : (rect.w ? rect.w : surface.w);
  for(unsigned x = 0; x < w; x++)
  {
   const uint32 p = surface.pix<uint32>()[((rect.y + y) * surface.pitchinpix) + rect.x + x];
   visible += (uint8(p) + uint8(p >> 8) + uint8(p >> 16)) > 24;
   count++;
  }
 }
 return count && visible >= count / 100;
}

static void write_transition(const MDFN_Surface& surface, const MDFN_Rect& rect,
                             const int32* line_widths, unsigned transition,
                             uint32 game_state, uint32 engine_state)
{
 const char* directory = std::getenv("MMX4_ORACLE_SCREENSHOT_DIR");
 if(!directory || !*directory) return;
 mkdir(directory, 0755);
 char filename[128], path[4096], manifest_path[4096];
 std::snprintf(filename, sizeof(filename),
               "transition_%04u_f%06u_g%08x_e%08x.ppm", transition,
               frame_number, game_state, engine_state);
 std::snprintf(path, sizeof(path), "%s/%s", directory, filename);
 write_ppm(path, surface, rect, line_widths);
 char vram_filename[128], vram_path[4096];
 std::snprintf(vram_filename, sizeof(vram_filename),
               "transition_%04u_f%06u_g%08x_e%08x.vram", transition,
               frame_number, game_state, engine_state);
 std::snprintf(vram_path, sizeof(vram_path), "%s/%s", directory, vram_filename);
 FILE* vram = std::fopen(vram_path, "wb");
 if(vram)
 {
  std::fwrite(MDFN_IEN_PSX::GPU.GPURAM, sizeof(uint16), 1024 * 512, vram);
  std::fclose(vram);
 }
 std::snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.tsv", directory);
 FILE* manifest = std::fopen(manifest_path, transition ? "a" : "w");
 if(manifest)
 {
  std::fprintf(manifest, "%u\t%u\t%08x\t%08x\t%s\n", transition,
               frame_number, game_state, engine_state, filename);
  std::fclose(manifest);
 }
}

static void write_frame_capture(const MDFN_Surface& surface,
                                const MDFN_Rect& rect,
                                const int32* line_widths)
{
 const char* directory = std::getenv("MMX4_ORACLE_FRAME_DIR");
 if(!directory || !*directory) return;

 const char* first_text = std::getenv("MMX4_ORACLE_FRAME_FIRST");
 const unsigned first = first_text ? std::strtoul(first_text, nullptr, 0) : 0;
 const char* last_text = std::getenv("MMX4_ORACLE_FRAME_LAST");
 const unsigned last = last_text ? std::strtoul(last_text, nullptr, 0) : ~0U;
 const char* interval_text = std::getenv("MMX4_ORACLE_FRAME_INTERVAL");
 const unsigned interval = interval_text ? std::strtoul(interval_text, nullptr, 0) : 1;
 if(frame_number < first || frame_number > last || !interval ||
    ((frame_number - first) % interval))
  return;

 mkdir(directory, 0755);
 char path[4096];
 std::snprintf(path, sizeof(path), "%s/frame_%06u.ppm", directory,
               frame_number);
 write_ppm(path, surface, rect, line_widths);
}

static void write_ppm(const char* path, const MDFN_Surface& surface,
                      const MDFN_Rect& rect, const int32* line_widths)
{
 FILE* fp = std::fopen(path, "wb");
 if(!fp) return;
 const unsigned w = 320;
 const unsigned h = rect.h ? rect.h : surface.h;
 std::fprintf(fp, "P6\n%u %u\n255\n", w, h);
 for(unsigned y = 0; y < h; y++)
 {
  const unsigned source_w = line_widths && line_widths[0] != ~0
      ? line_widths[rect.y + y] : (rect.w ? rect.w : surface.w);
  for(unsigned x = 0; x < w; x++)
  {
   const unsigned source_x = (uint64(x) * source_w) / w;
   const uint32 p = surface.pix<uint32>()[((rect.y + y) * surface.pitchinpix) + rect.x + source_x];
   const uint8 rgb[3] = { uint8(p), uint8(p >> 8), uint8(p >> 16) };
   std::fwrite(rgb, 1, 3, fp);
  }
 }
 std::fclose(fp);
}

static std::vector<uint8> capture_completed_vram_rgb()
{
 const unsigned source_y = ((peek32(MMX4_SP_DRAW_INFO_POS) & 1) ^ 1) * 240;
 std::vector<uint8> pixels(320 * 240 * 3);
 for(unsigned y = 0; y < 240; y++)
  for(unsigned x = 0; x < 320; x++)
  {
   const uint16 pixel = MDFN_IEN_PSX::GPU.GPURAM[source_y + y][x];
   const size_t output = (y * 320 + x) * 3;
   pixels[output + 0] = uint8((pixel & 0x1F) << 3);
   pixels[output + 1] = uint8((pixel >> 5 & 0x1F) << 3);
   pixels[output + 2] = uint8((pixel >> 10 & 0x1F) << 3);
  }
 return pixels;
}

struct ObjectWatch
{
 const char* name;
 uint32 address;
 uint32 count;
 uint32 stride;
};

static const ObjectWatch object_watches[] = {
 { "player", MMX4_PLAYER, 1, 0xE4 },
 { "entity", MMX4_ENTITY, 1, 0xE4 },
 { "main", MMX4_MAIN_OBJECTS, 0x30, 0x9C },
 { "visual", MMX4_VISUAL_OBJECTS, 0x20, 0x70 },
 { "shot", MMX4_SHOT_OBJECTS, 0x20, 0x9C },
 { "weapon", MMX4_WEAPON_OBJECTS, 0x10, 0x9C },
 { "unk", MMX4_UNK_OBJECTS, 0x14, 0x60 },
 { "item", MMX4_ITEM_OBJECTS, 0x20, 0x8C },
 { "misc", MMX4_MISC_OBJECTS, 0x40, 0x60 },
 { "quad", MMX4_QUAD_OBJECTS, 0x20, 0x60 },
 { "effect", MMX4_EFFECT_OBJECTS, 0x20, 0x30 },
 { "background", MMX4_BACKGROUND_OBJECTS, 3, 0x54 },
};

struct ObjectStep
{
 uint8 active, id, state, step, substep;
};

static void capture_object_changes(const char* directory,
                                   bool capture_enabled)
{
 static bool initialized;
 static bool manifest_initialized;
 static bool logged_character_archive;
  static unsigned event;
  static std::vector<ObjectStep> previous;
 static uint32 previous_game_state;
 static uint32 previous_engine_state;
 static uint16 previous_engine_scene;
  std::vector<ObjectStep> current;
 struct Change { const char* kind; const ObjectWatch* table; unsigned slot;
                 ObjectStep old_value; ObjectStep new_value;
                 uint8 old_phase, new_phase; };
 static std::vector<Change> pending_changes;
 static unsigned pending_event_frame;
  std::vector<Change> changes;
 for(const auto& table : object_watches)
  for(uint32 slot = 0; slot < table.count; slot++)
  {
   const uint32 p = table.address + slot * table.stride;
   ObjectStep value = { peek8(p), peek8(p + 1), peek8(p + 4),
                        peek8(p + 5), peek8(p + 6) };
   const size_t index = current.size();
   current.push_back(value);
   if(initialized)
   {
    const ObjectStep old_value = previous[index];
    if(!old_value.active && value.active)
     changes.push_back({ "created", &table, slot, old_value, value, 0, 0 });
    else if(old_value.active && !value.active)
     changes.push_back({ "destroyed", &table, slot, old_value, value, 0, 0 });
    else if(old_value.active && value.active && old_value.id != value.id)
     changes.push_back({ "replaced", &table, slot, old_value, value, 0, 0 });
    else if(value.active &&
            (value.state != old_value.state || value.step != old_value.step ||
             value.substep != old_value.substep))
     changes.push_back({ "control", &table, slot, old_value, value, 0, 0 });
   }
  }
 const uint32 game_state = peek32(MMX4_GAME_INFO);
 if(initialized && game_state != previous_game_state)
 {
  ObjectStep old_value = { 1, 0, uint8(previous_game_state),
      uint8(previous_game_state >> 8), uint8(previous_game_state >> 16) };
  ObjectStep new_value = { 1, 0, uint8(game_state), uint8(game_state >> 8),
      uint8(game_state >> 16) };
  changes.push_back({ "game_info", nullptr, 0, old_value, new_value,
      uint8(previous_game_state >> 24), uint8(game_state >> 24) });
 }
 previous_game_state = game_state;
 const uint32 engine_state = peek32(MMX4_ENGINE_OBJ);
 if(initialized && engine_state != previous_engine_state)
 {
  ObjectStep old_value = { 1, 0, uint8(previous_engine_state),
      uint8(previous_engine_state >> 8), uint8(previous_engine_state >> 16) };
  ObjectStep new_value = { 1, 0, uint8(engine_state),
      uint8(engine_state >> 8), uint8(engine_state >> 16) };
  changes.push_back({ "engine_info", nullptr, 0, old_value, new_value,
      uint8(previous_engine_state >> 24), uint8(engine_state >> 24) });
 }
 previous_engine_state = engine_state;
 const uint16 engine_scene = peek8(MMX4_ENGINE_STAGE) |
                             (uint16(peek8(MMX4_ENGINE_SUBSTAGE)) << 8);
 if(!logged_character_archive && std::getenv("MMX4_ORACLE_LOG_ARCHIVES") &&
    (engine_state & 0xFF) == 1 && (engine_state >> 8 & 0xFF) >= 3)
 {
  const uint32 archive = peek32(MMX4_SP_ARCHIVE_SLOT);
  const uint32 palette = peek32(MMX4_SP_PALETTE);
  const uint32 graphics = peek32(MMX4_GRAPHICS_POINTER);
  uint32 hash = 2166136261U;
  uint32 vram_hash = 2166136261U;
  uint32 texture_hash = 2166136261U;
  for(unsigned i = 0; i < 4096; i++) hash = (hash ^ peek8(palette + i)) * 16777619U;
  for(unsigned y = 480; y < 488; y++)
   for(unsigned x = 0; x < 256; x++)
   {
    const uint16 pixel = MDFN_IEN_PSX::GPU.GPURAM[y][x];
    vram_hash = (vram_hash ^ uint8(pixel)) * 16777619U;
   vram_hash = (vram_hash ^ uint8(pixel >> 8)) * 16777619U;
   }
  for(unsigned y = 256; y < 512; y++)
   for(unsigned x = 384; x < 448; x++)
   {
    const uint16 pixel = MDFN_IEN_PSX::GPU.GPURAM[y][x];
    texture_hash = (texture_hash ^ uint8(pixel)) * 16777619U;
    texture_hash = (texture_hash ^ uint8(pixel >> 8)) * 16777619U;
   }
  std::printf("character archive=%08x offsets=%08x,%08x,%08x palette=%08x hash=%08x vram=%08x texture=%08x graphics=%08x values=%08x,%08x,%08x\n",
              archive, peek32(archive), peek32(archive + 4), peek32(archive + 8),
              palette, hash, vram_hash, texture_hash, graphics, peek32(graphics), peek32(graphics + 4),
              peek32(graphics + 8));
  for(unsigned i = 0; i < 16; i++)
  {
   const uint32 object = MMX4_MISC_OBJECTS + i * 0x60;
   if(peek8(object))
    std::printf("character misc[%u]=%u/%u layer=%u flip=%u frame=%u texture=%04x clut=%04x\n",
                i, peek8(object + 1), peek8(object + 2), peek8(object + 0x16),
                peek8(object + 0x15), peek8(object + 0x47),
                peek16(object + 0x40), peek16(object + 0x42));
  }
  logged_character_archive = true;
 }
 if(initialized && engine_scene != previous_engine_scene)
 {
  ObjectStep old_value = { 1, 0, uint8(previous_engine_scene),
      uint8(previous_engine_scene >> 8), 0 };
  ObjectStep new_value = { 1, 0, uint8(engine_scene),
      uint8(engine_scene >> 8), 0 };
  changes.push_back({ "engine_scene", nullptr, 0, old_value, new_value,
                      0, 0 });
 }
 previous_engine_scene = engine_scene;
 if(!capture_enabled)
  changes.clear();
 previous.swap(current);
 if(!initialized)
 {
  initialized = true;
  return;
 }
 std::vector<Change> output_changes;
 output_changes.swap(pending_changes);
 const unsigned output_event_frame = pending_event_frame;
 if(!changes.empty())
 {
  pending_changes = std::move(changes);
  pending_event_frame = frame_number;
 }
 if(output_changes.empty()) return;

 const std::vector<uint8> pixels = capture_completed_vram_rgb();
 char filename[128], path[4096], manifest_path[4096];
 mkdir(directory, 0755);
 std::snprintf(filename, sizeof(filename),
               "change_%04u_f%06u.ppm", event, frame_number);
 std::snprintf(path, sizeof(path), "%s/%s", directory, filename);
 FILE* image = std::fopen(path, "wb");
 if(image)
 {
  std::fprintf(image, "P6\n320 240\n255\n");
  std::fwrite(pixels.data(), 1, pixels.size(), image);
  std::fclose(image);
 }
 std::snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.tsv",
               directory);
 FILE* manifest = std::fopen(manifest_path,
                             manifest_initialized ? "a" : "w");
 if(manifest)
 {
  if(!manifest_initialized)
   std::fprintf(manifest,
                "event\tframe\ttrigger\tobject\tslot\told_active\tactive\t"
                "old_id\tid\told_state\told_step\told_substep\tstate\t"
                "step\tsubstep\told_phase\tphase\tfilename\n");
  for(const auto& change : output_changes)
   std::fprintf(manifest,
                "%u\t%u\t%s\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t"
                "%u\t%u\t%u\t%u\t%u\t%s\n",
                event, output_event_frame, change.kind,
                change.table ? change.table->name : change.kind, change.slot,
                change.old_value.active, change.new_value.active,
                change.old_value.id, change.new_value.id,
                change.old_value.state,
                change.old_value.step, change.old_value.substep,
                change.new_value.state, change.new_value.step,
                change.new_value.substep, change.old_phase, change.new_phase,
                filename);
  std::fclose(manifest);
 }
 manifest_initialized = true;
 event++;
}

static uint32 peek32(uint32 address)
{
 return MDFN_IEN_PSX::PSX_DBGInfo.MemPeek(address, 4, true, true);
}

static uint8 peek8(uint32 address)
{
 return MDFN_IEN_PSX::PSX_DBGInfo.MemPeek(address, 1, true, true);
}

static uint16 peek16(uint32 address)
{
 return MDFN_IEN_PSX::PSX_DBGInfo.MemPeek(address, 2, true, true);
}

static void poke_bytes(uint32 address, const uint8* data, uint32 size)
{
 for(auto& space : *MDFN_IEN_PSX::PSX_DBGInfo.AddressSpaces)
 {
  if(space.name == "cpu")
  {
   space.PutAddressSpaceBytes(space.name.c_str(), address, size, 1, true, data);
   return;
  }
 }
 throw std::runtime_error("PSX CPU address space is unavailable");
}

static void poke32(uint32 address, uint32 value)
{
 const uint8 data[4] = {
  uint8(value), uint8(value >> 8), uint8(value >> 16), uint8(value >> 24)
 };
 poke_bytes(address, data, sizeof(data));
}

static void poke8(uint32 address, uint8 value)
{
 poke_bytes(address, &value, 1);
}

static unsigned env_u8(const char* name, unsigned fallback)
{
 const char* text = std::getenv(name);
 if(!text || !*text) return fallback;
 const unsigned long value = std::strtoul(text, nullptr, 0);
 if(value > 0xFF) throw std::runtime_error(std::string(name) + " is out of range");
 return unsigned(value);
}

static void configure_sfx_fixture(const char* text)
{
 if(!text || !*text) return;
 char* end;
 unsigned long first = std::strtoul(text, &end, 0);
 unsigned long group = 0;
 unsigned long index = first;
 if(end == text)
  throw std::runtime_error("MMX4_SFX must be INDEX or GROUP:INDEX");
 if(*end == ':')
 {
  char* index_end;
  group = first;
  index = std::strtoul(end + 1, &index_end, 0);
  if(index_end == end + 1 || *index_end)
   throw std::runtime_error("MMX4_SFX must be INDEX or GROUP:INDEX");
 }
 else if(*end)
  throw std::runtime_error("MMX4_SFX must be INDEX or GROUP:INDEX");
 if(group >= 8 || index > 0xFF)
  throw std::runtime_error("MMX4_SFX is out of range");
 sfx_fixture_group = unsigned(group);
 sfx_fixture_index = unsigned(index);
 const char* frames_text = std::getenv("MMX4_SFX_FRAMES");
 if(frames_text && *frames_text)
 {
  char* frames_end;
  unsigned long frames = std::strtoul(frames_text, &frames_end, 0);
  if(*frames_end || !frames || frames > 36000)
   throw std::runtime_error("MMX4_SFX_FRAMES is out of range");
  sfx_fixture_capture_frames = unsigned(frames);
 }
 sfx_fixture_enabled = true;
}

static void sfx_fixture_call(uint32 function)
{
 cpu_regs->SetRegister(31, MMX4_SFX_FIXTURE_RETURN);
 cpu_regs->SetRegister(32, function);
 cpu_regs->SetRegister(33, function + 4);
}

static void apply_sfx_fixture()
{
 sfx_fixture_phase = 1;
 sfx_fixture_call(MMX4_FUNC_LOAD_COMMON_ARCHIVES);
 std::printf("frame %u: SFX fixture loading group=%u index=%u\n",
             frame_number, sfx_fixture_group, sfx_fixture_index);
}

static void trigger_sfx_fixture()
{
 if(audio_wav_path && *audio_wav_path && !audio_recording)
 {
  if(!MDFNI_StartWAVRecord(audio_wav_path, 44100))
   throw std::runtime_error("unable to start fixture WAV recording");
  audio_recording = true;
 }
 sfx_fixture_capture_start = frame_number;
 cpu_regs->SetRegister(4, sfx_fixture_group);
 cpu_regs->SetRegister(5, sfx_fixture_index);
 cpu_regs->SetRegister(6, 0);
 sfx_fixture_call(MMX4_FUNC_PLAY_SOUND);
}

static void advance_sfx_fixture()
{
 switch(sfx_fixture_phase++)
 {
  case 1:
   sfx_fixture_call(MMX4_FUNC_START_SOUND);
   break;
  case 2:
   sfx_fixture_call(MMX4_FUNC_RESET_GAME_ENGINE);
   break;
  case 3:
   poke32(MMX4_ARCHIVE_DESTINATION, MMX4_ARCHIVE_BUFFER);
   poke8(MMX4_ENGINE_STAGE, 0x0E);
   poke8(MMX4_ENGINE_SUBSTAGE, 0);
   poke8(MMX4_ENGINE_CHARACTER, 0);
   sfx_fixture_call(MMX4_FUNC_LOAD_SCENE_ARCHIVE);
   break;
  case 4:
   sfx_fixture_call(MMX4_FUNC_COPY_STAGE_DATA);
   break;
  case 5:
   poke8(MMX4_ENGINE_SUBSTAGE, 1);
   sfx_fixture_call(MMX4_FUNC_LOAD_SCENE_ARCHIVE);
   break;
  case 6:
   sfx_fixture_call(MMX4_FUNC_COPY_STAGE_DATA);
   break;
  case 7:
   if(sfx_fixture_group == 5)
   {
    sfx_fixture_phase = 9;
    trigger_sfx_fixture();
   }
   else
    sfx_fixture_call(MMX4_FUNC_LOAD_PLAYER_ARCHIVES);
   break;
  case 8:
   trigger_sfx_fixture();
   break;
  default:
   std::printf("frame %u: SFX fixture trigger result=%d\n",
               frame_number, int32(cpu_regs->GetRegister(2, nullptr, 0)));
   poke32(MMX4_SFX_FIXTURE_IDLE,
          MIPS_J_OPCODE | ((MMX4_SFX_FIXTURE_IDLE >> 2) & MIPS_J_TARGET_MASK));
   poke32(MMX4_SFX_FIXTURE_IDLE + 4, 0);
   cpu_regs->SetRegister(32, MMX4_SFX_FIXTURE_IDLE);
   cpu_regs->SetRegister(33, MMX4_SFX_FIXTURE_IDLE + 4);
   break;
 }
}

static unsigned direct_boot_phase;
static uint8 direct_boot_stage;
static uint8 direct_boot_substage;
static uint8 direct_boot_checkpoint;
static uint8 direct_boot_character;
static bool direct_boot_mission_briefing;
static void direct_boot_call(uint32 function)
{
 cpu_regs->SetRegister(31, MMX4_DIRECT_BOOT_RETURN);
 cpu_regs->SetRegister(32, function);
 cpu_regs->SetRegister(33, function + 4);
}

static void apply_direct_boot()
{
 uint8 engine[0x64] = {};
 const char* scene = std::getenv("MMX4_ORACLE_SCENE");
 const bool character_select = scene && !std::strcmp(scene, "character-select");
 direct_boot_mission_briefing =
     scene && !std::strcmp(scene, "mission-briefing");
 if(character_select)
 {
  engine[0] = 1;
  poke_bytes(MMX4_ENGINE_OBJ, engine, sizeof(engine));
  poke32(MMX4_ARCHIVE_DESTINATION, MMX4_ARCHIVE_BUFFER);
  cpu_regs->SetRegister(32, MMX4_FUNC_ENGINE_DISPATCH);
  cpu_regs->SetRegister(33, (MMX4_FUNC_ENGINE_DISPATCH + 4));
  direct_boot_applied = true;
  std::printf("frame %u: direct boot character-select\n", frame_number);
  return;
 }
 direct_boot_stage = env_u8("MMX4_DIRECT_STAGE", 0);
 direct_boot_substage = env_u8("MMX4_DIRECT_SUBSTAGE", 0);
 direct_boot_checkpoint = env_u8("MMX4_DIRECT_CHECKPOINT", 0);
 direct_boot_character = env_u8("MMX4_DIRECT_CHARACTER", 0);
 engine[0x0C] = 0xE;
 engine[0x0D] = 0;
 engine[0x43] = direct_boot_character;
 poke_bytes(MMX4_ENGINE_OBJ, engine, sizeof(engine));
 poke32(MMX4_ARCHIVE_DESTINATION, MMX4_ARCHIVE_BUFFER);

 direct_boot_phase = 1;
 direct_boot_call(MMX4_FUNC_LOAD_SCENE_ARCHIVE);
 direct_boot_applied = true;
 std::printf("frame %u: direct boot preloading target=%u/%u checkpoint=%u character=%u base_slots=%08x,%08x,%08x,%08x\n",
             frame_number, direct_boot_stage, direct_boot_substage,
             direct_boot_checkpoint, direct_boot_character,
             peek32(MMX4_SP_TILES), peek32(MMX4_SP_DESCRIPTORS),
             peek32(MMX4_SP_PALETTE_SLOT), peek32(MMX4_SP_GRAPHICS_SLOT));
}

static void advance_direct_boot()
{
 switch(direct_boot_phase++)
 {
  case 1:
   direct_boot_call(MMX4_FUNC_COPY_STAGE_DATA);
   break;
  case 2:
   poke8(MMX4_ENGINE_SUBSTAGE, 1);
   direct_boot_call(MMX4_FUNC_LOAD_SCENE_ARCHIVE);
   break;
  case 3:
   direct_boot_call(MMX4_FUNC_COPY_STAGE_DATA);
   break;
  case 4:
   direct_boot_call(MMX4_FUNC_LOAD_PLAYER_ARCHIVES);
   break;
  case 5:
   direct_boot_call(MMX4_FUNC_RESET_GAME_ENGINE);
   break;
 case 6:
   if(direct_boot_mission_briefing)
   {
    poke8(MMX4_ENGINE_OBJ, 3);
    poke8(MMX4_ENGINE_STAGE, 0);
    poke8(MMX4_ENGINE_SUBSTAGE, 0);
    cpu_regs->SetRegister(32, MMX4_FUNC_ENGINE_DISPATCH);
    cpu_regs->SetRegister(33, (MMX4_FUNC_ENGINE_DISPATCH + 4));
    std::printf("frame %u: direct boot entering mission briefing\n",
                frame_number);
   }
   else
   {
    cpu_regs->SetRegister(4, MMX4_ENGINE_OBJ);
    direct_boot_call(MMX4_FUNC_ENGINE_STATE_0);
   }
   break;
  default:
  {
   poke8(MMX4_ENGINE_OBJ, direct_boot_mission_briefing ? 3 : 4);
   if(!direct_boot_mission_briefing)
   {
    poke8(MMX4_ENGINE_STAGE, direct_boot_stage);
    poke8(MMX4_ENGINE_SUBSTAGE, direct_boot_substage);
    poke8(MMX4_ENGINE_CHECKPOINT, direct_boot_checkpoint);
    poke8(MMX4_ENGINE_CHARACTER, direct_boot_character);
   }
   cpu_regs->SetRegister(32, MMX4_FUNC_ENGINE_DISPATCH);
   cpu_regs->SetRegister(33, (MMX4_FUNC_ENGINE_DISPATCH + 4));
   std::printf("frame %u: direct boot archives ready slots=%08x,%08x,%08x,%08x\n",
               frame_number, peek32(MMX4_SP_TILES), peek32(MMX4_SP_DESCRIPTORS),
               peek32(MMX4_SP_PALETTE_SLOT), peek32(MMX4_SP_GRAPHICS_SLOT));
   break;
  }
 }
}

static FILE* render_dump;
static FILE* render_source_dump;
static FILE* object_dump;
static FILE* upload_dump;
static unsigned upload_index;

static void dump_psx_upload()
{
 const char* directory = std::getenv("MMX4_ORACLE_DUMP_DIR");
 if(!directory || !*directory) return;
 if(!upload_dump)
 {
  mkdir(directory, 0755);
  char path[4096];
  std::snprintf(path, sizeof(path), "%s/uploads.tsv", directory);
  upload_dump = std::fopen(path, "w");
  if(!upload_dump) throw std::runtime_error("unable to open upload dump");
 }
 const unsigned slot = peek8(MMX4_UPLOAD_SLOT) & 0xF;
 const uint32 packed = peek32(MMX4_UPLOAD_TABLE + slot * 12 + 4);
 const uint32 source = MMX4_UPLOAD_SOURCE + slot * 0x800;
 uint32 hash = 2166136261U;
 for(unsigned i = 0; i < 0x800; i++)
  hash = (hash ^ peek8(source + i)) * 16777619U;
 std::fprintf(upload_dump, "%u\t%u\t%08x\t%u\t%u\t64\t16\t%08x\n",
              upload_index++, peek8(MMX4_UPLOAD_STATE), peek32(MMX4_UPLOAD_DESTINATION),
              packed >> 16, packed & 0xFFFF, hash);
 std::fflush(upload_dump);
}

static void open_oracle_dumps()
{
 const char* directory = std::getenv("MMX4_ORACLE_DUMP_DIR");
 if(render_dump || !directory || !*directory) return;
 mkdir(directory, 0755);
 char path[4096];
 std::snprintf(path, sizeof(path), "%s/render.tsv", directory);
 render_dump = std::fopen(path, "w");
 std::snprintf(path, sizeof(path), "%s/render_sources.tsv", directory);
 render_source_dump = std::fopen(path, "w");
 std::snprintf(path, sizeof(path), "%s/objects.tsv", directory);
 object_dump = std::fopen(path, "w");
 if(!render_dump || !render_source_dump || !object_dump)
  throw std::runtime_error("unable to open oracle dump files");
}

static void dump_psx_objects(uint32 game_state, uint32 engine_state)
{
 static bool dumped_title_background_sources;
 if(!dumped_title_background_sources && game_state == MMX4_TITLE_GAME_STATE)
 {
  const uint32 tiles = peek32(MMX4_SP_TILES);
  const uint32 descriptors = peek32(MMX4_SP_DESCRIPTORS);
  const uint32 layout = peek32(MMX4_SP_LAYOUT);
  uint32 tile_hash = 2166136261U, descriptor_hash = 2166136261U;
  for(unsigned i = 0; i < 0xE00; i++)
   tile_hash = (tile_hash ^ peek8(tiles + i)) * 16777619U;
  for(unsigned i = 0; i < 0x10000; i++)
   descriptor_hash = (descriptor_hash ^ peek8(descriptors + i)) * 16777619U;
  std::fprintf(stderr,
   "title background sources tiles=%08x descriptors=%08x hashes=%08x,%08x\n",
   tiles, descriptors, tile_hash, descriptor_hash);
  std::fprintf(stderr, "title layout=%08x", layout);
  for(unsigned i = 0; i < 18; i++)
   std::fprintf(stderr, "%c%02x", i ? ' ' : ' ', peek8(layout + i));
  std::fprintf(stderr, "\n");
  for(unsigned i = 0; i < 16; i++)
   std::fprintf(stderr, "title descriptor[%u]=%08x\n", i,
                peek32(descriptors + i * 4));
  dumped_title_background_sources = true;
 }
 struct Table { const char* name; uint32 address, count, stride; };
 static const Table tables[] = {
  { "player", MMX4_PLAYER, 1, 0xE4 },
  { "entity", MMX4_ENTITY, 1, 0xE4 },
  { "main", MMX4_MAIN_OBJECTS, 0x30, 0x9C },
  { "visual", MMX4_VISUAL_OBJECTS, 0x20, 0x70 },
  { "shot", MMX4_SHOT_OBJECTS, 0x20, 0x9C },
  { "weapon", MMX4_WEAPON_OBJECTS, 0x10, 0x9C },
  { "unk", MMX4_UNK_OBJECTS, 0x14, 0x60 },
  { "item", MMX4_ITEM_OBJECTS, 0x20, 0x8C },
  { "misc", MMX4_MISC_OBJECTS, 0x40, 0x60 },
  { "quad", MMX4_QUAD_OBJECTS, 0x20, 0x60 },
  { "effect", MMX4_EFFECT_OBJECTS, 0x20, 0x30 },
  { "background", MMX4_BACKGROUND_OBJECTS, 3, 0x54 },
 };
 for(const auto& table : tables)
  for(uint32 slot = 0; slot < table.count; slot++)
  {
   const uint32 p = table.address + slot * table.stride;
   const int active = int8(peek8(p));
   const int on_screen = int8(peek8(p + 3));
   if(!active && !on_screen) continue;
   std::fprintf(object_dump,
    "%u\t%08x\t%08x\t%s\t%u\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
    frame_number, game_state, engine_state, table.name, slot, active,
    int8(peek8(p + 1)), int8(peek8(p + 2)), on_screen,
    int8(peek8(p + 4)), int8(peek8(p + 5)), int8(peek8(p + 6)),
    int8(peek8(p + 7)), int32(peek32(p + 8)), int32(peek32(p + 12)));
  }
}

static void dump_psx_render(uint32 game_state, uint32 engine_state)
{
 const char* engine_filter = std::getenv("MMX4_ORACLE_DUMP_ENGINE");
 if(engine_filter && *engine_filter &&
    (engine_state & 0xFF) != std::strtoul(engine_filter, nullptr, 0)) return;
 open_oracle_dumps();
 if(!render_dump) return;
 const uint32 draw_info = peek32(MMX4_CURRENT_DRAW_INFO);
 if(draw_info < PSX_RAM_START || draw_info >= PSX_RAM_END)
 {
  dump_psx_objects(game_state, engine_state);
  return;
 }
 const uint32 ot_start = draw_info + 0x70;
 const uint32 ot_end = draw_info + 0xA0;
 for(int bucket = 11; bucket >= 0; bucket--)
 {
  uint32 primitive = peek32(ot_start + bucket * 4) & PSX_GPU_TAG_ADDRESS_MASK;
  for(unsigned order = 0; primitive != PSX_GPU_TAG_END && order < 4096; order++)
  {
   const uint32 address = PSX_KSEG0_BASE | primitive;
   if(address >= ot_start && address < ot_end) break;
   if(address < PSX_RAM_START || address >= PSX_RAM_END) break;
   const uint32 tag = peek32(address);
   const unsigned len = tag >> 24;
   if(len > 16) break;
   std::fprintf(render_dump, "%u\t%08x\t%08x\t%d\t%u\t%u\t%02x\t",
                frame_number, game_state, engine_state, bucket, order, len,
                len ? peek8(address + 7) : 0);
   std::fprintf(render_source_dump, "%u\t%d\t%u\t%08x\t%02x\n",
                frame_number, bucket, order, address,
                len ? peek8(address + 7) : 0);
   const uint8 code = len ? peek8(address + 7) : 0;
   for(unsigned i = 0; i < len * 4; i++)
   {
    uint8 byte = peek8(address + 4 + i);
    if((code & 0xFC) == 0x38 && (i == 11 || i == 19 || i == 27)) byte = 0;
    if((code & 1) && code < 0x80 && i < 3) byte = 0;
    std::fprintf(render_dump, "%02x", byte);
   }
   std::fputc('\n', render_dump);
   primitive = tag & PSX_GPU_TAG_ADDRESS_MASK;
  }
 }
 dump_psx_objects(game_state, engine_state);
 std::fflush(render_dump);
 std::fflush(render_source_dump);
 std::fflush(object_dump);
}

static void cpu_hook(uint32 pc, bool)
{
 static bool logged_ready_source;
 static bool logged_character_archive;
 static uint32 loading_frame_counter;
 static bool loading_frame_sync;
 const char* scene = std::getenv("MMX4_ORACLE_SCENE");
 if(pc == MMX4_FUNC_LOADING_FRAME_BEGIN &&
    std::getenv("MMX4_ORACLE_OBJECT_CHANGE_DIR"))
 {
  loading_frame_sync = (cpu_regs->GetRegister(4, nullptr, 0) & 0xFF) == 0;
  if(loading_frame_sync)
  {
   loading_frame_counter = peek32(MMX4_LOADING_FRAME_COUNTER);
   poke32(MMX4_LOADING_FRAME_COUNTER, 0);
  }
 }
 else if(pc == MMX4_FUNC_LOADING_FRAME_END && loading_frame_sync)
 {
  poke32(MMX4_LOADING_FRAME_COUNTER, loading_frame_counter);
  loading_frame_sync = false;
 }
 if(pc == MMX4_FUNC_FRAME_BOUNDARY)
 {
  const char* directory = std::getenv("MMX4_ORACLE_OBJECT_CHANGE_DIR");
  if(directory && *directory)
  {
   const uint32 game = peek32(MMX4_GAME_INFO);
   const uint32 engine = peek32(MMX4_ENGINE_OBJ);
   bool capture_enabled = game != 0 || direct_boot_applied;
   if(scene && !std::strcmp(scene, "title"))
    capture_enabled = (game & 0xFF) == 1;
   else if(scene && !std::strcmp(scene, "character-select"))
    capture_enabled = (engine & 0xFF) == 1;
   else if(scene && !std::strcmp(scene, "mission-briefing"))
    capture_enabled = (engine & 0xFF) == 3;
   else if(scene && !std::strcmp(scene, "initial-stage"))
    capture_enabled = (engine & 0xFF) == 6 &&
                      peek8(MMX4_ENGINE_STAGE) == 0 && peek8(MMX4_ENGINE_SUBSTAGE) == 0;
   capture_object_changes(directory, capture_enabled);
  }
 }
 else if(pc == MMX4_FUNC_PLAY_SOUND && std::getenv("MMX4_ORACLE_LOG_SFX"))
 {
  const uint32 game = peek32(MMX4_GAME_INFO);
  const uint32 engine = peek32(MMX4_ENGINE_OBJ);
  std::printf("frame %u: SFX group=%u index=%u source=%08x game=%u/%u/%u engine=%u/%u/%u stage=%u/%u checkpoint=%u character=%u ra=%08x\n",
              frame_number,
              cpu_regs->GetRegister(4, nullptr, 0),
              cpu_regs->GetRegister(5, nullptr, 0),
              cpu_regs->GetRegister(6, nullptr, 0),
              game & 0xFF, (game >> 8) & 0xFF, (game >> 16) & 0xFF,
              engine & 0xFF, (engine >> 8) & 0xFF, (engine >> 16) & 0xFF,
              peek8(MMX4_ENGINE_STAGE), peek8(MMX4_ENGINE_SUBSTAGE),
              peek8(MMX4_ENGINE_CHECKPOINT), peek8(MMX4_ENGINE_CHARACTER),
              cpu_regs->GetRegister(31, nullptr, 0));
 }
 else if((pc == MMX4_FUNC_SS_VM_KEY_ON_NOW || pc == MMX4_FUNC_800E2DB4) &&
         std::getenv("MMX4_ORACLE_LOG_SPU_VOICES"))
 {
  std::printf("frame %u: PSX keyon pc=%08x count=%u pitch=%04x addr0=%02x%02x addr1=%02x%02x spucnt=%02x%02x revvoices=%02x%02x/%02x%02x revdepth=%02x%02x/%02x%02x cur=",
              frame_number, pc, cpu_regs->GetRegister(4, nullptr, 0),
              cpu_regs->GetRegister(5, nullptr, 0) & 0xffff,
              peek8(MMX4_SOUND_SAMPLE_ADDRESS_HIGH), peek8(MMX4_SOUND_SAMPLE_ADDRESS),
              peek8(MMX4_SOUND_SAMPLE_LOOP_HIGH), peek8(MMX4_SOUND_SAMPLE_LOOP),
              peek8(PSX_SPU_REG_1DAB), peek8(PSX_SPU_REG_1DAA),
              peek8(PSX_SPU_REG_1D99), peek8(PSX_SPU_REG_1D98),
              peek8(PSX_SPU_REG_1D9B), peek8(PSX_SPU_REG_1D9A),
              peek8(PSX_SPU_REG_1D85), peek8(PSX_SPU_REG_1D84),
              peek8(PSX_SPU_REG_1D87), peek8(PSX_SPU_REG_1D86));
  for(unsigned i = 0; i < 0x20; i++)
   std::printf("%02x", peek8(MMX4_SOUND_DUMP + i));
  std::printf(" sample=");
  const uint32 sample_address = (peek16(MMX4_SOUND_SAMPLE_ADDRESS) << 3);
  for(unsigned i = 0; i < 32; i++)
  {
   const uint16 word = MDFN_IEN_PSX::SPU->PeekSPURAM((sample_address + i) >> 1);
   std::printf("%02x", (word >> ((i & 1) * 8)) & 0xff);
  }
  std::printf("\n");
 }
 else if(pc == MMX4_FUNC_8001D064 && sfx_fixture_enabled)
  apply_sfx_fixture();
 else if(pc == MMX4_SFX_FIXTURE_RETURN && sfx_fixture_enabled)
  advance_sfx_fixture();
 else if(pc == MMX4_FUNC_8001DAF8 && !direct_boot_applied &&
    ((scene && (!std::strcmp(scene, "character-select") ||
                !std::strcmp(scene, "mission-briefing") ||
                !std::strcmp(scene, "initial-stage"))) ||
     std::getenv("MMX4_ORACLE_DIRECT_BOOT")))
  apply_direct_boot();
 else if(pc == MMX4_DIRECT_BOOT_RETURN && direct_boot_applied)
  advance_direct_boot();
 else if(pc == MMX4_FUNC_800B2C8C)
 {
  const uint32 slot3 = peek32(MMX4_SP_GRAPHICS_SLOT);
  std::printf("frame %u: misc36 slots=%08x,%08x,%08x,%08x,%08x,%08x low30=%08x resolved=%08x object=%08x\n",
              frame_number, peek32(MMX4_SP_TILES), peek32(MMX4_SP_DESCRIPTORS),
              peek32(MMX4_SP_PALETTE_SLOT), slot3,
              peek32(MMX4_SP_PALETTE), peek32(MMX4_SP_ARCHIVE_OFFSET),
              peek32(slot3 + 0x30), slot3 + peek32(slot3 + 0x30),
              cpu_regs->GetRegister(4, nullptr, 0));
 }
 else if(pc == MMX4_FUNC_800CB048 && !logged_ready_source)
 {
  const uint32 archive = peek32(MMX4_SP_GRAPHICS_SLOT);
  const uint32 offset = peek32(archive + 0x24);
  std::printf("frame %u: READY source archive=%08x offsets=%08x/%08x frame=%08x bytes=",
              frame_number, archive, peek32(archive + 0x10), offset,
              archive + offset);
  for(unsigned i = 0; i < 32; i++) std::printf("%02x", peek8(archive + offset + i));
  std::printf("\n");
  logged_ready_source = true;
 }
 else if((pc == MMX4_FUNC_800CCA34 || pc == MMX4_FUNC_800CCB74) && !logged_character_archive &&
         std::getenv("MMX4_ORACLE_LOG_ARCHIVES"))
 {
  const uint32 archive = peek32(MMX4_SP_ARCHIVE_SLOT);
  const uint32 palette = peek32(MMX4_SP_PALETTE);
  const uint32 graphics = peek32(MMX4_GRAPHICS_POINTER);
  uint32 hash = 2166136261U;
  for(unsigned i = 0; i < 4096; i++) hash = (hash ^ peek8(palette + i)) * 16777619U;
  std::printf("character archive=%08x offsets=%08x,%08x,%08x palette=%08x hash=%08x graphics=%08x values=%08x,%08x,%08x\n",
              archive, peek32(archive), peek32(archive + 4), peek32(archive + 8),
              palette, hash, graphics, peek32(graphics), peek32(graphics + 4),
              peek32(graphics + 8));
  logged_character_archive = true;
 }
 else if(pc == MMX4_FUNC_800148EC)
  dump_psx_upload();
 else if(pc == MMX4_FUNC_80018000 || pc == MMX4_FUNC_800182E8)
 {
  const uint32 ra = cpu_regs->GetRegister(31, nullptr, 0);
  cpu_regs->SetRegister(32, ra);
  cpu_regs->SetRegister(33, ra + 4);
  std::printf("frame %u: skipped movie call at %08x -> %08x\n", frame_number, pc, ra);
 }
 else
  std::printf("frame %u: reached anchor %08x\n", frame_number, pc);
 MDFN_IEN_PSX::PSX_DBGInfo.SetCPUCallback(cpu_hook, false);
}

int main(int argc, char** argv)
{
 if(argc < 3)
 {
  std::fprintf(stderr, "usage: %s GAME.cue SCPH1001.BIN [frames]\n", argv[0]);
  return 2;
 }
 const unsigned max_frames = argc >= 4 ? std::strtoul(argv[3], nullptr, 0) : 3600;
 char cue_path[4096], bios_path[4096], cwd[4096];
 if(!realpath(argv[1], cue_path) || !realpath(argv[2], bios_path) || !getcwd(cwd, sizeof(cwd)))
 {
  std::perror("oracle path");
  return 2;
 }
 const std::string cue = cue_path;
 const std::string bios = bios_path;
 const std::string base = std::string(cwd) + "/tools/oracle/mednafen-state";
 mkdir(base.c_str(), 0755);

 try
 {
  configure_sfx_fixture(std::getenv("MMX4_SFX"));
  if(!MDFNI_Init() || !MDFNI_InitFinalize(base.c_str()))
   throw std::runtime_error("Mednafen initialization failed");
  if(!MDFNI_SetSetting("psx.bios_na", bios) || !MDFNI_SetSettingB("psx.bios_sanity", false))
   throw std::runtime_error("unable to configure PSX BIOS");
  MDFNGI* game = MDFNI_LoadGame("psx", &NVFS, cue.c_str(), true);
  if(!game)
   throw std::runtime_error("unable to load disc image");
  if(!MDFNI_SetMedia(0, 2, 0, 0))
   throw std::runtime_error("unable to insert disc into virtual drive");

  cpu_regs = MDFN_IEN_PSX::PSX_DBGInfo.RegGroups->at(0);
  for(uint32 pc : { MMX4_FUNC_FRAME_BOUNDARY, MMX4_FUNC_LOADING_FRAME_BEGIN, MMX4_FUNC_LOADING_FRAME_END, MMX4_FUNC_800148EC,
                    MMX4_FUNC_80018000, MMX4_FUNC_800182E8,
                    MMX4_FUNC_8001D064, MMX4_SFX_FIXTURE_RETURN,
                    MMX4_FUNC_8001DAF8, MMX4_FUNC_8001FC20, MMX4_FUNC_800B2C8C,
                    MMX4_FUNC_800CB048,
                    MMX4_DIRECT_BOOT_RETURN })
  MDFN_IEN_PSX::PSX_DBGInfo.AddBreakPoint(BPOINT_PC, pc, pc, true);
 if(std::getenv("MMX4_ORACLE_LOG_SFX"))
 {
  MDFN_IEN_PSX::PSX_DBGInfo.AddBreakPoint(BPOINT_PC, MMX4_FUNC_PLAY_SOUND,
                                           MMX4_FUNC_PLAY_SOUND, true);
 }
 if(std::getenv("MMX4_ORACLE_LOG_SPU_VOICES"))
 {
  for(uint32 pc : { MMX4_FUNC_SS_VM_KEY_ON_NOW, MMX4_FUNC_800E2DB4 })
   MDFN_IEN_PSX::PSX_DBGInfo.AddBreakPoint(BPOINT_PC, pc, pc, true);
 }
 MDFN_IEN_PSX::PSX_DBGInfo.SetCPUCallback(cpu_hook, false);

  uint8* pad = MDFNI_SetInput(0, 1);
  MDFN_Surface surface(nullptr, game->fb_width, game->fb_height, game->fb_width,
                       MDFN_PixelFormat::ABGR32_8888);
  std::vector<int32> line_widths(game->fb_height);
  EmulateSpecStruct espec;
  espec.surface = &surface;
  espec.LineWidths = line_widths.data();
  espec.skip = false;
  espec.soundmultiplier = 1.0;
  espec.SoundVolume = 1.0;
  std::vector<int16> sound_buffer;
  audio_wav_path = std::getenv("MMX4_AUDIO_WAV");
  if(audio_wav_path && *audio_wav_path)
  {
   sound_buffer.resize(65536 * game->soundchan);
   espec.SoundRate = 44100;
   espec.SoundBuf = sound_buffer.data();
   espec.SoundBufMaxSize = 65536;
   if(!sfx_fixture_enabled)
   {
    if(!MDFNI_StartWAVRecord(audio_wav_path, espec.SoundRate))
     throw std::runtime_error("unable to start WAV recording");
    audio_recording = true;
   }
  }

  uint32 previous_log_state = ~0U;
  const char* log_interval_text = std::getenv("MMX4_ORACLE_LOG_INTERVAL");
  const unsigned log_interval = log_interval_text
      ? std::strtoul(log_interval_text, nullptr, 0) : 120;
  bool logged_background_sources = false;
  uint32 previous_state = ~0U;
  uint32 previous_engine = ~0U;
  unsigned stable_state_frames = 0;
  uint64 oracle_input_state = ~uint64(0);
  unsigned oracle_input_state_frames = 0;
  unsigned transition = 0;
  const char* autoplay_text = std::getenv("MMX4_ORACLE_AUTOPLAY");
  const bool autoplay = !autoplay_text || std::strcmp(autoplay_text, "0");
  const char* burst_engine_text = std::getenv("MMX4_ORACLE_BURST_ENGINE");
  const unsigned burst_engine = burst_engine_text
      ? std::strtoul(burst_engine_text, nullptr, 0) : ~0U;
  const char* burst_count_text = std::getenv("MMX4_ORACLE_BURST_COUNT");
  unsigned burst_remaining = burst_engine_text
      ? (burst_count_text ? std::strtoul(burst_count_text, nullptr, 0) : 30)
      : 0;
  struct PendingCapture { unsigned index; uint32 game; uint32 engine; };
  std::vector<PendingCapture> pending;
  for(frame_number = 0; frame_number < max_frames; frame_number++)
  {
   std::memset(pad, 0, 2);
   const uint32 engine = peek32(MMX4_ENGINE_OBJ);
   const uint32 game_state = peek32(MMX4_GAME_INFO);
   const uint64 packed_input_state = (uint64(game_state) << 32) | engine;
   if(packed_input_state != oracle_input_state)
   {
    oracle_input_state = packed_input_state;
    oracle_input_state_frames = 0;
   }
   else
    oracle_input_state_frames++;
   if(game_state != previous_log_state ||
      (log_interval && !(frame_number % log_interval)))
   {
    std::printf("frame %u: pc=%08x game=%08x engine=%u/%u health=%u "
                "player=%08x pstate=%u/%u/%u pos=%d,%d anim=%u/%u phealth=%u flags=%02x\n", frame_number,
                cpu_regs->GetRegister(32, nullptr, 0), game_state,
                engine & 0xFF, (engine >> 8) & 0xFF, peek8(MMX4_ENGINE_OBJ + 0x46),
                peek32(MMX4_PLAYER), peek8(MMX4_PLAYER + 4),
                peek8(MMX4_PLAYER + 5), peek8(MMX4_PLAYER + 6),
                int16(peek16(MMX4_PLAYER + 0x0a)),
                int16(peek16(MMX4_PLAYER + 0x0e)),
                peek8(MMX4_PLAYER + 0x47), peek8(MMX4_PLAYER + 0x46),
                peek8(MMX4_PLAYER + 0x5c), peek8(MMX4_PLAYER + 0x89));
    previous_log_state = game_state;
   }
   if(autoplay && ((game_state & 0xFFFF) == 0x0D01 ||
       (game_state & 0xFFFF) == 0x0501) && oracle_input_state_frames % 30 < 2)
    pad[0] |= 1U << 3;
   if(autoplay && (game_state & 0xFFFF) == 0x0106 &&
      oracle_input_state_frames % 30 < 2)
    pad[1] |= 1U << 6;
   if(autoplay && (engine & 0xFFFF) == 0x0301 &&
      oracle_input_state_frames % 30 < 2)
    pad[1] |= 1U << 6;
   if(autoplay && (engine & 0xFFFF) == 0x0903 &&
      oracle_input_state_frames % 20 < 4)
    pad[1] |= 1U << 6;
   std::fill(line_widths.begin(), line_widths.end(), ~0);
   MDFNI_Emulate(&espec);
   const uint32 captured_engine = peek32(MMX4_ENGINE_OBJ);
   const uint32 captured_state = peek32(MMX4_GAME_INFO);
   if(captured_state == MMX4_TITLE_GAME_STATE && previous_state != captured_state)
    std::fprintf(stderr, "title display rect=%d,%d %dx%d line=%d\n",
                 espec.DisplayRect.x, espec.DisplayRect.y,
                 espec.DisplayRect.w, espec.DisplayRect.h,
                 line_widths[espec.DisplayRect.y]);
   if(!logged_background_sources && (captured_engine & 0xFF) == 6)
   {
    const uint32 tiles = peek32(MMX4_SP_TILES);
    uint32 hash = 2166136261U;
    for(unsigned i = 0; i < 0x1000; i++)
     hash = (hash ^ peek8(tiles + i)) * 16777619U;
    std::printf("oracle: background sources tiles=%08x descriptors=%08x "
                "graphics=%08x main0_graphics=%08x "
                "layout=%u,%u/%u map=%08x tile_hash=%08x "
                "update_flags=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                tiles, peek32(MMX4_SP_DESCRIPTORS),
                peek32(MMX4_SP_GRAPHICS_SLOT), peek32(MMX4_MAIN_OBJECTS + 0x3C),
                peek8(MMX4_LAYOUT_WIDTH), peek8(MMX4_LAYOUT_HEIGHT),
                peek32(MMX4_LAYOUT_SIZE) & 0xFFFF,
                peek32(MMX4_LAYOUT_MAP), hash,
                peek8(MMX4_ENGINE_UPDATE_FLAG(0)), peek8(MMX4_ENGINE_UPDATE_FLAG(1)),
                peek8(MMX4_ENGINE_UPDATE_FLAG(2)), peek8(MMX4_ENGINE_UPDATE_FLAG(3)),
                peek8(MMX4_ENGINE_UPDATE_FLAG(4)), peek8(MMX4_ENGINE_UPDATE_FLAG(5)),
                peek8(MMX4_ENGINE_UPDATE_FLAG(6)), peek8(MMX4_ENGINE_UPDATE_FLAG(7)),
                peek8(MMX4_ENGINE_UPDATE_FLAG(8)), peek8(MMX4_ENGINE_UPDATE_FLAG(9)));
    logged_background_sources = true;
   }
   dump_psx_render(captured_state, captured_engine);
   write_frame_capture(surface, espec.DisplayRect, line_widths.data());
   if(captured_state != previous_state || captured_engine != previous_engine)
   {
    pending.clear();
    pending.push_back({ transition++, captured_state, captured_engine });
   previous_state = captured_state;
   previous_engine = captured_engine;
    stable_state_frames = 0;
   }
   else
   {
    stable_state_frames++;
    if((stable_state_frames == 1 || stable_state_frames == 31) &&
       (peek32(MMX4_FADE_AMOUNT) & 0xFFFF) == 0 &&
       has_visible_pixels(surface, espec.DisplayRect, line_widths.data()))
     write_transition(surface, espec.DisplayRect, line_widths.data(),
                      transition++, captured_state, captured_engine);
   }
   if(!pending.empty() && (peek32(MMX4_FADE_AMOUNT) & 0xFFFF) == 0 &&
      has_visible_pixels(surface, espec.DisplayRect, line_widths.data()))
   {
    for(const auto& capture : pending)
     write_transition(surface, espec.DisplayRect, line_widths.data(),
                      capture.index, capture.game, capture.engine);
    pending.clear();
   }
   if(burst_remaining && (captured_engine & 0xFF) == burst_engine &&
      (peek32(MMX4_FADE_AMOUNT) & 0xFFFF) == 0 &&
      has_visible_pixels(surface, espec.DisplayRect, line_widths.data()))
   {
    write_transition(surface, espec.DisplayRect, line_widths.data(),
                     transition++, captured_state, captured_engine);
    burst_remaining--;
   }
   if(sfx_fixture_enabled && audio_recording &&
      frame_number + 1 >= sfx_fixture_capture_start + sfx_fixture_capture_frames)
    break;
  }
  write_ppm("tools/oracle/mednafen-screen.ppm", surface, espec.DisplayRect,
            line_widths.data());
  unlink("tools/oracle/mednafen-screen.png");
  PNGWrite("tools/oracle/mednafen-screen.png", &surface, espec.DisplayRect, line_widths.data());
  if(audio_recording)
   MDFNI_StopWAVRecord();
  MDFNI_CloseGame();
  MDFNI_Kill();
 }
 catch(const std::exception& e)
 {
  std::fprintf(stderr, "oracle: %s\n", e.what());
  return 1;
 }
 return 0;
}
