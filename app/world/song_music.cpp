#include "world/song_music.h"

#include <algorithm>
#include <array>
#include <string>

#include "ro/render/skill_suite_table.h"

namespace moonclient {
namespace {

// UNE UNITÉ DE CHANSON ET SA COMPÉTENCE, telles que le serveur les pose : les
// types 158 à 175, des ensembles aux danses.
struct SongUnit {
  std::uint32_t unit_id = 0;
  std::uint16_t skill_id = 0;
};

constexpr std::array<SongUnit, 18> kSongUnits = {{
    {158, 306},  // Lullaby
    {159, 307},  // Mr. Kim A Rich Man
    {160, 308},  // Eternal Chaos
    {161, 309},  // A Drum on the Battlefield
    {162, 310},  // The Ring of Nibelungen
    {163, 311},  // Loki's Veil
    {164, 312},  // Into the Abyss
    {165, 313},  // Invulnerable Siegfried
    {166, 317},  // Dissonance
    {167, 319},  // A Whistle
    {168, 320},  // Assassin Cross of Sunset
    {169, 321},  // A Poem of Bragi
    {170, 322},  // The Apple of Idun
    {171, 325},  // Ugly Dance
    {172, 327},  // Humming
    {173, 328},  // Please Don't Forget Me
    {174, 329},  // Fortune's Kiss
    {175, 330},  // Service for You
}};

}  // namespace

std::optional<std::uint16_t> SongSkillOfUnit(std::uint32_t unit_id) noexcept {
  const auto found = std::ranges::find(kSongUnits, unit_id, &SongUnit::unit_id);
  if (found == kSongUnits.end()) {
    return std::nullopt;
  }
  return found->skill_id;
}

void StopSongMusic(ro::audio::AudioEngine& audio, std::uint16_t skill_id) {
  for (const ro::render::SkillSuiteStep& step :
       ro::render::SkillSuiteOf(skill_id, ro::render::SkillSuiteMoment::kAnnounced)) {
    if (step.kind == ro::render::SkillSuiteKind::kSound && !step.sound.empty()) {
      audio.StopSound(std::string(step.sound));
    }
  }
}

}  // namespace moonclient
