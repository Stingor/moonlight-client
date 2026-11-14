// LA TAILLE ET LES EFFETS D'OFFICE DE CERTAINS MONSTRES, lus dans leur YAML.

#include "ro/vfs/monster_size_effects.h"

#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::vfs::MonsterSizeEffects;

namespace {

constexpr std::string_view kMiniature = R"(
version: 1
monsters:
  1200: {size: 1, effects: [1301]}
  1002: {size: 1.2, effects: [890, 892]}
  1039: {size: 2, effects: []}
  1113: {size: 0.5, effects: []}
  1150: {size: 0, effects: [612]}
)";

[[nodiscard]] MonsterSizeEffects Read() {
  auto table = MonsterSizeEffects::Parse(kMiniature);
  REQUIRE(table.has_value());
  return *table;
}

}  // namespace

TEST_CASE("Taille d'office : la table grandit et pare un monstre", "[vfs][monster]") {
  const MonsterSizeEffects table = Read();
  const MonsterSizeEffects::Entry* poring = table.For(1002);
  REQUIRE(poring != nullptr);
  CHECK(poring->size == 1.2F);
  // Dans l'ordre de la liste.
  CHECK(poring->effect_ids == std::vector<std::uint32_t>{890, 892});
  CHECK(table.ScaleOf(1002) == 1.2F);
  CHECK(table.ScaleOf(1113) == 0.5F);
  CHECK(table.For(1039)->effect_ids.empty());
}

TEST_CASE("Taille d'office : une taille nulle ne change rien, une classe absente non plus",
          "[vfs][monster]") {
  const MonsterSizeEffects table = Read();
  REQUIRE(table.For(1150) != nullptr);
  CHECK(table.ScaleOf(1150) == 1.0F);
  CHECK(table.For(1150)->effect_ids == std::vector<std::uint32_t>{612});
  CHECK(table.For(9999) == nullptr);
  CHECK(table.ScaleOf(9999) == 1.0F);
  // Triée par classe, quel que soit l'ordre du fichier.
  REQUIRE(table.entries().size() == 5);
  CHECK(table.entries().front().monster_id == 1002);
  CHECK(table.entries().back().monster_id == 1200);
}

TEST_CASE("Taille d'office : une table mal formée est refusée entière", "[vfs][monster]") {
  CHECK_FALSE(MonsterSizeEffects::Parse("version: 2\nmonsters: {}\n").has_value());
  CHECK_FALSE(MonsterSizeEffects::Parse("version: 1\n").has_value());
  CHECK_FALSE(
      MonsterSizeEffects::Parse("version: 1\nmonsters: {1002: {effects: []}}\n").has_value());
  CHECK_FALSE(MonsterSizeEffects::Parse("version: 1\nmonsters: {1002: {size: 1, effects: 3}}\n")
                  .has_value());
  CHECK_FALSE(MonsterSizeEffects::Parse("version: 1\nmonsters: {1002: {size: 1, effects: [-1]}}\n")
                  .has_value());
  CHECK_FALSE(MonsterSizeEffects::Parse("version: 1\nmonsters: {1002: 3}\n").has_value());
  CHECK_FALSE(MonsterSizeEffects::Parse("[").has_value());
  CHECK(MonsterSizeEffects::Parse("version: 1\nmonsters: {}\n").has_value());
}
