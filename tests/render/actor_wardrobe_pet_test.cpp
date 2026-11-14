// LE PET PARÉ : son accessoire remplace le .act du corps, le .spr de l'espèce
// reste. Le banc bâtit sa propre archive, en mémoire : aucun asset du jeu.

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"
#include "ro/vfs/pet_tables.h"
#include "ro/vfs/vfs.h"

#include "support/actor_client.h"
#include "support/grf_builder.h"
#include "support/sprite_tables.h"
#include "support/temp_directory.h"

using ro::render::ActorFiles;
using ro::render::ActorLook;
using ro::render::ActorOutfit;
using ro::render::ActorPart;
using ro::render::ActorPieceFiles;
using ro::render::ActorWardrobe;
using ro::render::LoadSpriteCatalog;
using ro::render::PetAccessoryAnimationPath;
using ro::render::SpriteCatalog;
using ro::test_support::AnimationBytes;
using ro::test_support::BuildGrf;
using ro::test_support::Entry;
using ro::test_support::GrfFileSpec;
using ro::test_support::Poring;
using ro::test_support::SpriteBytes;
using ro::test_support::SpriteTablesFrom;
using ro::test_support::TempDirectory;
using ro::vfs::Vfs;

namespace {

// L'accessoire dont l'archive porte le .act, et celui dont elle ne le porte pas.
constexpr std::uint16_t kBag = 10013;
constexpr std::uint16_t kMissingAct = 10014;
// Un accessoire que la table ignore.
constexpr std::uint16_t kUnlisted = 10099;

// Le .act de l'espèce porte UNE animation, celui de l'accessoire DEUX : c'est
// ce qui dit lequel le corps a pris.
constexpr std::size_t kSpeciesActions = 8;
constexpr std::size_t kAccessoryActions = 16;

// La table nomme le fichier AVEC son extension, dans la casse qu'elle veut.
constexpr const char* kPetInfo =
    "version: 1\n"
    "accessories:\n"
    "  10013: \"Poring_고글.ACT\"\n"
    "  10014: \"Poring_absent.act\"\n";
constexpr std::string_view kPoringSprite = "version: 1\nclasses:\n  1002: {sprite: \"Poring\"}\n";

void MakePetClient(const TempDirectory& client) {
  client.WriteText("data/yaml_files/class_sprites.yml", kPoringSprite);
  client.WriteText("data/yaml_files/pet_info.yml", kPetInfo);
  client.WriteText("data/yaml_files/item_info.yml", "version: 1\nitems: {}\n");
  std::vector<GrfFileSpec> files;
  files.push_back(Entry("data\\sprite\\몬스터\\Poring.spr", SpriteBytes()));
  files.push_back(Entry("data\\sprite\\몬스터\\Poring.act", AnimationBytes(1)));
  files.push_back(Entry("data\\sprite\\몬스터\\Poring_고글.act", AnimationBytes(2)));
  client.WriteFile("pets.grf", BuildGrf(files));
}

[[nodiscard]] ActorLook PetWearing(std::uint16_t accessory) {
  ActorLook look = Poring();
  look.pet_accessory = accessory;
  return look;
}

[[nodiscard]] std::size_t BodyActions(ActorWardrobe& wardrobe, const ActorLook& look) {
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  REQUIRE_FALSE(outfit->pieces().empty());
  REQUIRE(outfit->pieces().front().part == ActorPart::kBody);
  REQUIRE(outfit->pieces().front().animation != nullptr);
  return outfit->pieces().front().animation->actions().size();
}

[[nodiscard]] SpriteCatalog TableOnlyCatalog() {
  SpriteCatalog catalog{SpriteTablesFrom({{ro::render::SpriteTableFile::kClasses, kPoringSprite}})};
  ro::vfs::PetTables pets;
  REQUIRE(ro::vfs::ParsePetInfo(kPetInfo, pets).has_value());
  catalog.tables.pet_accessory_acts = std::move(pets.accessory_act_by_item);
  return catalog;
}

}  // namespace

TEST_CASE("Pet paré : le .act vient de la table, à côté du corps, sans extension",
          "[render][catalog][pet]") {
  const SpriteCatalog catalog = TableOnlyCatalog();
  const std::string body = "data\\sprite\\몬스터\\Poring";
  CHECK(PetAccessoryAnimationPath(catalog, PetWearing(kBag), body) ==
        "data\\sprite\\몬스터\\Poring_고글");
  CHECK(PetAccessoryAnimationPath(catalog, PetWearing(kUnlisted), body).empty());
  CHECK(PetAccessoryAnimationPath(catalog, PetWearing(0), body).empty());
  // Un joueur ne porte pas d'accessoire de pet, même si le champ dit le contraire.
  ActorLook player = PetWearing(kBag);
  player.player = true;
  CHECK(PetAccessoryAnimationPath(catalog, player, body).empty());

  // LE .SPR NE CHANGE PAS : seule l'animation du corps est remplacée.
  const std::vector<ActorPieceFiles> files = ActorFiles(catalog, PetWearing(kBag));
  REQUIRE(files.size() == 1);
  CHECK(files.front().candidates == std::vector<std::string>{body});
  CHECK(files.front().animation_path == "data\\sprite\\몬스터\\Poring_고글");
}

TEST_CASE("Pet paré : le corps prend l'animation de l'accessoire", "[render][wardrobe][pet]") {
  TempDirectory client;
  MakePetClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"pets.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  CHECK(BodyActions(wardrobe, PetWearing(0)) == kSpeciesActions);
  CHECK(BodyActions(wardrobe, PetWearing(kBag)) == kAccessoryActions);
  // L'ACCESSOIRE FAIT PARTIE DE L'APPARENCE : sans lui dans la clé, le premier
  // pet dessiné habillerait tous les autres.
  CHECK(wardrobe.OutfitFor(PetWearing(0)) != wardrobe.OutfitFor(PetWearing(kBag)));
  // Retiré, le corps revient à son .act.
  CHECK(BodyActions(wardrobe, PetWearing(0)) == kSpeciesActions);
}

TEST_CASE("Pet paré : un .act d'accessoire absent laisse le pet, sans l'accessoire",
          "[render][wardrobe][pet]") {
  TempDirectory client;
  MakePetClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"pets.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  CHECK(BodyActions(wardrobe, PetWearing(kMissingAct)) == kSpeciesActions);
  CHECK(BodyActions(wardrobe, PetWearing(kUnlisted)) == kSpeciesActions);
}
