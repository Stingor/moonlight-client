// L'ARÈNE D'UNE PARTIE : le sol que la simulation lit — rien à lire pour la
// prairie générée, la carte pour prt_fild08 —, le nom affiché, le bandeau
// d'entrée et la musique des champs, du catalogue du sol généré ou de la table
// des cartes.

#include "survivors/arena_choice.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/map_weather.h"
#include "ro/sim/survivors_rules.h"
#include "ro/vfs/map_display_names.h"

#include "survivors/arena_music.h"
#include "survivors/survivors_book.h"

namespace {

[[nodiscard]] const ro::sim::SurvivorsRules& Rules() { return ro::sim::ShippedSurvivorsRules(); }

// UNE ARÈNE QUI LIT UNE CARTE : le catalogue livré n'en a plus, mais une
// règle d'arène peut encore en nommer une.
[[nodiscard]] const ro::sim::ArenaRule& FieldArena() {
  static const ro::sim::ArenaRule kField{
      .arena_id = 1, .key = "prt_fild08", .map_name = "prt_fild08", .terrain_id = 0};
  return kField;
}

constexpr std::string_view kMapNamesYaml =
    "version: 1\n"
    "maps:\n"
    "  prt_fild08:\n"
    "    name: Prontera Field\n"
    "    title: Prontera Field\n"
    "    background: field_s2\n";

}  // namespace

TEST_CASE("Arène choisie : la prairie générée naît sans installation", "[app][survivors]") {
  const ro::sim::ArenaRule* plains = moonclient::DefaultArenaOf(Rules());
  REQUIRE(plains != nullptr);
  REQUIRE(plains->terrain_id != 0);
  const auto walk = moonclient::ArenaWalkOf(nullptr, *plains);
  REQUIRE(walk.has_value());
  CHECK_FALSE(walk->has_value());
  // Une carte, elle, se lit dans l'installation.
  CHECK_FALSE(moonclient::ArenaWalkOf(nullptr, FieldArena()).has_value());
}

TEST_CASE("Arène choisie : sans installation, aucun sol à annoncer", "[app][survivors]") {
  CHECK(moonclient::ArenaGroundsOf(nullptr, Rules()).empty());
}

TEST_CASE("Arène choisie : nom, bandeau et musique viennent du biome ou de la table des cartes",
          "[app][survivors]") {
  const ro::sim::ArenaRule& plains = *moonclient::DefaultArenaOf(Rules());
  const ro::sim::ArenaRule& field = FieldArena();
  const auto names = ro::vfs::MapDisplayNames::Parse(kMapNamesYaml);
  REQUIRE(names.has_value());

  // LA PRAIRIE : le catalogue du sol généré, table des cartes ou non.
  REQUIRE(moonclient::ArenaLookOf(Rules(), plains) != nullptr);
  CHECK(moonclient::ArenaNameOf(Rules(), plains, nullptr) == "Prairie de Midgard");
  CHECK(moonclient::ArenaNameOf(Rules(), plains, &*names) == "Prairie de Midgard");
  const std::optional<ro::vfs::MapSign> plains_sign =
      moonclient::ArenaSignOf(Rules(), plains, nullptr);
  REQUIRE(plains_sign.has_value());
  CHECK(plains_sign->title == "Prairie de Midgard");
  CHECK(plains_sign->background == "field_s2");
  CHECK(plains_sign->announced);
  CHECK(moonclient::ArenaFieldTrackOf(Rules(), plains) ==
        moonclient::ArenaLookOf(Rules(), plains)->field_track);
  CHECK_FALSE(moonclient::ArenaFieldTrackOf(Rules(), plains).empty());

  // PRT_FILD08 : la table des cartes, la clé sans elle, et la musique des
  // champs de Prontera.
  CHECK(moonclient::ArenaLookOf(Rules(), field) == nullptr);
  CHECK(moonclient::ArenaNameOf(Rules(), field, &*names) == "Prontera Field");
  CHECK(moonclient::ArenaNameOf(Rules(), field, nullptr) == std::string(field.key));
  const std::optional<ro::vfs::MapSign> field_sign =
      moonclient::ArenaSignOf(Rules(), field, &*names);
  REQUIRE(field_sign.has_value());
  CHECK(field_sign->title == "Prontera Field");
  CHECK_FALSE(moonclient::ArenaSignOf(Rules(), field, nullptr).has_value());
  CHECK(moonclient::ArenaFieldTrackOf(Rules(), field) == moonclient::kArenaFieldTrack);
}

TEST_CASE("Arène choisie : il neige sur les neiges de Lutie, et nulle part ailleurs",
          "[app][survivors]") {
  constexpr std::uint16_t kSnowArenaId = 4;
  const ro::sim::ArenaRule* snow = ro::sim::FindArena(Rules(), kSnowArenaId);
  REQUIRE(snow != nullptr);
  CHECK(moonclient::ArenaNameOf(Rules(), *snow, nullptr) == "Neiges de Lutie");
  CHECK(moonclient::ArenaFieldTrackOf(Rules(), *snow) == "bgm\\55.mp3");
  CHECK(moonclient::ArenaWeatherOf(Rules(), *snow) ==
        std::vector<std::uint32_t>{ro::render::kSnowEffect});
  for (const ro::sim::ArenaRule& arena : Rules().arenas) {
    if (arena.arena_id == kSnowArenaId) continue;
    CHECK(moonclient::ArenaWeatherOf(Rules(), arena).empty());
  }
}
