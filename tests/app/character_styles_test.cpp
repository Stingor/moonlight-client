// LES COULEURS DE CHAQUE PERSONNAGE, gardées pour la tablée.
//
// 🔴 UN CACHE : ce que la sélection montre quand le serveur de personnages ne
// répond pas. Un défaut ici ne casse rien de visible : le personnage paraît
// simplement dans ses couleurs natives, ou dans celles d'un autre.

#include "character/character_styles.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/style.h"
#include "ro/sim/style_code.h"
#include "ro/sim/style_outfit.h"

#include "support/temp_directory.h"

using moonclient::CharacterStylesPath;
using moonclient::LoadCharacterStyles;
using moonclient::NoteAccountStyles;
using moonclient::NoteCharacterStyles;
using moonclient::NoteCharacterStylesServed;
using moonclient::OutfitOf;
using moonclient::VariantsOfOutfit;
using ro::sim::StyleOutfit;

namespace {

constexpr std::uint32_t kKnight = 150'001;
constexpr std::uint32_t kSmith = 150'002;
constexpr std::uint32_t kFootBody = 0x1234ABCDU;
constexpr std::uint32_t kMountedBody = 0x0BADF00DU;
constexpr std::uint32_t kHatView = 17;

[[nodiscard]] ro::sim::StyleVariants Dyed(std::int16_t palette) {
  ro::sim::StyleRecipe recipe;
  recipe.body_palette = palette;
  recipe.ramps[0].hue = 120;
  recipe.ramps[0].absolute = true;
  ro::sim::StyleVariants variants;
  variants.by_body[kFootBody] = recipe;
  recipe.ramps[1].value = -20;
  variants.by_body[kMountedBody] = recipe;
  variants.fallback_body = kMountedBody;
  return variants;
}

[[nodiscard]] std::vector<ro::sim::StyleLayerRecipe> Hat(std::int16_t hue) {
  ro::sim::StyleLayerRecipe hat;
  hat.layer = ro::sim::StyleLayerKind::kHeadgear;
  hat.key = kHatView;
  hat.profile = 1;
  hat.adjusts[0].hue = hue;
  return {hat};
}

bool Note(const ro::test_support::TempDirectory& root, std::uint32_t character_id,
          std::int16_t palette, std::int16_t hat_hue = 0) {
  const ro::sim::StyleVariants variants = Dyed(palette);
  const std::vector<ro::sim::StyleLayerRecipe> layers = Hat(hat_hue);
  const StyleOutfit outfit = OutfitOf(&variants, hat_hue != 0 ? &layers : nullptr);
  return NoteCharacterStyles(root.path(), character_id, &outfit);
}

}  // namespace

TEST_CASE("Couleurs des personnages : chacun retrouve sa tenue, corps et calques", "[app][style]") {
  const ro::test_support::TempDirectory root;
  CHECK(LoadCharacterStyles(root.path()).characters.empty());

  REQUIRE(Note(root, kKnight, 12, 45));
  REQUIRE(Note(root, kSmith, 40));
  const auto kept = LoadCharacterStyles(root.path());
  REQUIRE(kept.characters.size() == 2);
  const ro::sim::StyleVariants knight = VariantsOfOutfit(kept.characters.at(kKnight));
  REQUIRE(knight.by_body.size() == 2);
  CHECK(knight.fallback_body == kMountedBody);
  CHECK(knight.by_body.at(kFootBody).body_palette == 12);
  CHECK(knight.by_body.at(kFootBody).ramps[0].hue == 120);
  CHECK(knight.by_body.at(kMountedBody).ramps[1].value == -20);
  REQUIRE(kept.characters.at(kKnight).layers.size() == 1);
  CHECK(kept.characters.at(kKnight).layers[0].key == kHatView);
  CHECK(kept.characters.at(kKnight).layers[0].adjusts[0].hue == 45);
  CHECK(kept.characters.at(kSmith).layers.empty());
  CHECK_FALSE(kept.served);
}

TEST_CASE("Couleurs des personnages : rien de neuf, rien d'écrit ; plus rien, on oublie",
          "[app][style]") {
  const ro::test_support::TempDirectory root;
  REQUIRE(Note(root, kKnight, 12));
  // La même chose redite par le serveur : le fichier ne bouge pas.
  CHECK_FALSE(Note(root, kKnight, 12));
  CHECK(Note(root, kKnight, 13));
  CHECK(Note(root, kKnight, 13, 30));
  // 🔴 UN EFFACEMENT OUBLIE LE PERSONNAGE, et lui seul.
  REQUIRE(Note(root, kSmith, 40));
  CHECK(NoteCharacterStyles(root.path(), kKnight, nullptr));
  const auto kept = LoadCharacterStyles(root.path());
  CHECK_FALSE(kept.characters.contains(kKnight));
  CHECK(kept.characters.contains(kSmith));
  CHECK_FALSE(NoteCharacterStyles(root.path(), kKnight, nullptr));
}

TEST_CASE("Couleurs des personnages : un fichier abîmé laisse jouer", "[app][style]") {
  const ro::test_support::TempDirectory root;
  std::filesystem::create_directories(CharacterStylesPath(root.path()).parent_path());
  std::ofstream(CharacterStylesPath(root.path())) << "characters: [pas, une, table\n";
  CHECK(LoadCharacterStyles(root.path()).characters.empty());
  // Et la prochaine écriture repart proprement.
  CHECK(Note(root, kKnight, 12));
  CHECK(LoadCharacterStyles(root.path()).characters.size() == 1);
}

TEST_CASE("Couleurs des personnages : l'ancien fichier de codes v7 se lit, puis se convertit",
          "[app][style]") {
  const ro::test_support::TempDirectory root;
  const ro::sim::StyleVariants old = Dyed(12);
  std::filesystem::create_directories(CharacterStylesPath(root.path()).parent_path());
  {
    std::ofstream file(CharacterStylesPath(root.path()));
    file << "characters:\n  " << kKnight << ":\n    fallback: " << kMountedBody
         << "\n    styles:\n";
    for (const auto& [body_key, recipe] : old.by_body) {
      file << "      - \"" << ro::sim::EncodeStyleCode(recipe, body_key) << "\"\n";
    }
  }
  const auto kept = LoadCharacterStyles(root.path());
  REQUIRE(kept.characters.contains(kKnight));
  const ro::sim::StyleVariants read = VariantsOfOutfit(kept.characters.at(kKnight));
  CHECK(read.fallback_body == kMountedBody);
  CHECK(read.by_body.at(kFootBody).body_palette == 12);
  // Une écriture d'un autre personnage réécrit tout au format des tenues : le
  // premier s'y retrouve à l'identique.
  REQUIRE(Note(root, kSmith, 40));
  const auto converted = LoadCharacterStyles(root.path());
  CHECK(VariantsOfOutfit(converted.characters.at(kKnight)).by_body.at(kFootBody).body_palette ==
        12);
}

TEST_CASE("Couleurs des personnages : la preuve que le serveur sert la tablée se garde",
          "[app][style]") {
  const ro::test_support::TempDirectory root;
  REQUIRE(Note(root, kKnight, 12));
  CHECK(NoteCharacterStylesServed(root.path()));
  CHECK_FALSE(NoteCharacterStylesServed(root.path()));
  const auto kept = LoadCharacterStyles(root.path());
  CHECK(kept.served);
  CHECK(kept.characters.contains(kKnight));
}

TEST_CASE("Couleurs des personnages : la réponse du serveur fait foi pour la liste seule",
          "[app][style]") {
  const ro::test_support::TempDirectory root;
  REQUIRE(Note(root, kKnight, 12));
  REQUIRE(Note(root, kSmith, 40));
  constexpr std::uint32_t kStranger = 999'999;
  const ro::sim::StyleVariants variants = Dyed(77);
  std::map<std::uint32_t, StyleOutfit> told;
  told[kSmith] = OutfitOf(&variants, nullptr);
  // 🔴 UN PERSONNAGE HORS DE LA LISTE NE SE RANGE PAS, même si on le lui dit.
  told[kStranger] = OutfitOf(&variants, nullptr);
  // Le chevalier est listé sans recette : il ne montre plus rien.
  const std::array listed{kKnight, kSmith};
  CHECK(NoteAccountStyles(root.path(), listed, told));
  const auto kept = LoadCharacterStyles(root.path());
  CHECK_FALSE(kept.characters.contains(kKnight));
  CHECK_FALSE(kept.characters.contains(kStranger));
  CHECK(VariantsOfOutfit(kept.characters.at(kSmith)).by_body.at(kFootBody).body_palette == 77);
  CHECK_FALSE(NoteAccountStyles(root.path(), listed, told));
}
