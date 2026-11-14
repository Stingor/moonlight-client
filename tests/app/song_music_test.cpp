#include "world/song_music.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skill_suite_table.h"

using moonclient::SongSkillOfUnit;

TEST_CASE("Chansons : chaque unité de chanson nomme sa compétence", "[app][chanson]") {
  // Les deux bouts de la série du serveur, et un milieu.
  CHECK(SongSkillOfUnit(158) == std::uint16_t{306});  // Lullaby
  CHECK(SongSkillOfUnit(170) == std::uint16_t{322});  // The Apple of Idun
  CHECK(SongSkillOfUnit(175) == std::uint16_t{330});  // Service for You
  // Hors de la série : ni le Land Protector qui la précède, ni le Graffiti qui
  // la suit.
  CHECK_FALSE(SongSkillOfUnit(157).has_value());
  CHECK_FALSE(SongSkillOfUnit(176).has_value());
}

TEST_CASE("Chansons : l'annonce de chaque chanson porte la musique à couper", "[app][chanson]") {
  // 🔴 SANS SON À L'ANNONCE, la chanson interrompue n'aurait rien à taire, et
  // le défaut passerait inaperçu jusqu'au jeu.
  //
  // ⚠ TROIS N'EN JOUENT AUCUNE à l'annonce : Dissonance, A Whistle et Ugly
  // Dance. Elles n'ont rien à taire.
  constexpr std::uint32_t kFirstSongUnit = 158;
  constexpr std::uint32_t kLastSongUnit = 175;
  constexpr std::array<std::uint16_t, 3> kSilentSongs = {317, 319, 325};
  for (std::uint32_t unit = kFirstSongUnit; unit <= kLastSongUnit; ++unit) {
    const std::optional<std::uint16_t> skill = SongSkillOfUnit(unit);
    REQUIRE(skill.has_value());
    if (std::ranges::find(kSilentSongs, *skill) != kSilentSongs.end()) {
      continue;
    }
    bool sounds = false;
    for (const ro::render::SkillSuiteStep& step :
         ro::render::SkillSuiteOf(*skill, ro::render::SkillSuiteMoment::kAnnounced)) {
      sounds = sounds || (step.kind == ro::render::SkillSuiteKind::kSound && !step.sound.empty());
    }
    INFO("unité " << unit << ", compétence " << *skill);
    CHECK(sounds);
  }
}
