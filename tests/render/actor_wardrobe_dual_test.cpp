// Les deux armes d'un ambidextre, chacune sous le sprite de son objet.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_layers.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/vfs/vfs.h"

#include "support/actor_client.h"
#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using ro::render::ActorLook;
using ro::render::ActorOutfit;
using ro::render::ActorPart;
using ro::render::ActorPiece;
using ro::render::ActorWardrobe;
using ro::render::BodySpritePath;
using ro::render::ComposedActor;
using ro::render::DualWieldSpritePaths;
using ro::render::HeadSpritePath;
using ro::render::kAnimationAttack2;
using ro::render::kAnimationAttack3;
using ro::render::kDirectionCount;
using ro::render::LoadSpriteCatalog;
using ro::render::SpriteCatalog;
using ro::render::WeaponSpritePaths;
using ro::test_support::ActActionSpec;
using ro::test_support::ActAnchorSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::AnimationBytes;
using ro::test_support::BuildAct;
using ro::test_support::BuildGrf;
using ro::test_support::Entry;
using ro::test_support::GrfFileSpec;
using ro::test_support::kFrameSpeed;
using ro::test_support::SpriteBytes;
using ro::test_support::TempDirectory;
using ro::vfs::Vfs;

namespace {

constexpr std::uint32_t kAssassin = 12;
constexpr std::uint32_t kKnife = 1201;
constexpr std::uint32_t kCutter = 1202;
constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kGuard = 2101;
// Les animations d'un joueur, attaques comprises.
constexpr std::size_t kPlayerAnimations = 13;
// L'image que montre une arme à l'attaque 2, et partout ailleurs.
constexpr std::int32_t kAttack2Image = 1;
constexpr std::int32_t kOtherImage = 0;

ActorLook Assassin(std::uint32_t right, std::uint32_t left) {
  ActorLook look;
  look.player = true;
  look.job = kAssassin;
  look.sex = 1;
  look.hair = 1;
  look.weapon = right;
  look.shield = left;
  return look;
}

// Une arme qui montre une autre image à l'attaque 2 : c'est ce qui dit quelle
// action elle joue.
std::vector<std::uint8_t> WeaponAnimationBytes() {
  ActBuildOptions options;
  for (std::size_t action = 0; action < kPlayerAnimations * kDirectionCount; ++action) {
    ActLayerSpec layer;
    layer.frame_index = action / kDirectionCount == kAnimationAttack2 ? kAttack2Image : kOtherImage;
    ActFrameSpec frame;
    frame.layers.push_back(layer);
    frame.anchors.push_back(ActAnchorSpec{});
    ActActionSpec spec;
    spec.frames.push_back(std::move(frame));
    options.actions.push_back(std::move(spec));
    options.speeds.push_back(kFrameSpeed);
  }
  return BuildAct(options);
}

// Un client où l'assassin a son corps, sa tête, et un sprite par arme : la
// dague, l'épée, et les deux couples. Les chemins sont ceux que le catalogue
// produit lui-même.
struct DualClient {
  TempDirectory directory;
  std::optional<Vfs> vfs;
  std::optional<ActorWardrobe> wardrobe;
};

void Prepare(DualClient& client) {
  client.directory.WriteText("data/yaml_files/weapon_sprites.yml",
                             "version: 1\nweapons:\n  1: {sprite: \"_dagger\"}\n"
                             "  2: {sprite: \"_sword\"}\n  25: {sprite: \"_dagger_dagger\"}\n"
                             "  28: {sprite: \"_dagger_sword\"}\n");
  client.directory.WriteText("data/yaml_files/item_info.yml",
                             "version: 1\nitems: {1201: {name: \"Knife\", view: 1}, "
                             "1202: {name: \"Cutter\", view: 1}, "
                             "1101: {name: \"Sword\", view: 2}, "
                             "2101: {name: \"Guard\", view: 1}}\n");
  auto bare = Vfs::Mount({.root = client.directory.path(), .archives = {}});
  REQUIRE(bare.has_value());
  auto bare_catalog = LoadSpriteCatalog(*bare);
  REQUIRE(bare_catalog.has_value());
  const SpriteCatalog& catalog = *bare_catalog;

  std::vector<std::string> bases;
  const ActorLook plain = Assassin(0, 0);
  const std::optional<std::string> body = BodySpritePath(catalog, plain);
  REQUIRE(body.has_value());
  bases.push_back(*body);
  bases.push_back(HeadSpritePath(plain));
  for (const ActorLook& held : {Assassin(kKnife, 0), Assassin(kSword, 0), Assassin(kKnife, kCutter),
                                Assassin(kKnife, kSword)}) {
    const std::vector<std::string> weapon = WeaponSpritePaths(catalog, held);
    REQUIRE_FALSE(weapon.empty());
    bases.push_back(weapon.back());
  }

  std::vector<GrfFileSpec> files;
  for (std::size_t rank = 0; rank < bases.size(); ++rank) {
    const bool is_weapon = rank >= 2;
    files.push_back(Entry(bases[rank] + ".spr", SpriteBytes()));
    files.push_back(Entry(bases[rank] + ".act",
                          is_weapon ? WeaponAnimationBytes() : AnimationBytes(kPlayerAnimations)));
  }
  client.directory.WriteFile("sprites.grf", BuildGrf(files));

  auto mounted = Vfs::Mount({.root = client.directory.path(), .archives = {"sprites.grf"}});
  REQUIRE(mounted.has_value());
  client.vfs.emplace(std::move(*mounted));
  auto catalog_with_sprites = LoadSpriteCatalog(*client.vfs);
  REQUIRE(catalog_with_sprites.has_value());
  client.wardrobe.emplace(*client.vfs, std::move(*catalog_with_sprites));
}

const ActorPiece* PieceOf(const ActorOutfit& outfit, ActorPart part) {
  for (const ActorPiece& piece : outfit.pieces()) {
    if (piece.part == part) {
      return &piece;
    }
  }
  return nullptr;
}

// L'image que montre une pièce dans une composition, ou -1 si elle n'y est pas.
std::int32_t ImageOf(const ComposedActor& composed, const ActorPiece& piece) {
  for (const auto& source : composed.sources) {
    if (source.sprite == piece.sprite) {
      return source.image_index;
    }
  }
  return -1;
}

}  // namespace

TEST_CASE("Ambidextre : chaque main garde le sprite de son arme", "[render][wardrobe]") {
  DualClient client;
  Prepare(client);
  ActorOutfit* outfit = client.wardrobe->OutfitFor(Assassin(kKnife, kSword));
  REQUIRE(outfit != nullptr);

  const ActorPiece* right = PieceOf(*outfit, ActorPart::kWeapon);
  const ActorPiece* left = PieceOf(*outfit, ActorPart::kWeaponTrail);
  REQUIRE(right != nullptr);
  REQUIRE(left != nullptr);
  CHECK(right->sprite != left->sprite);
  CHECK(right->single_weapon_sprite);
  CHECK(left->single_weapon_sprite);

  // Les mêmes fichiers que si chaque main tenait son arme seule.
  ActorOutfit* knife = client.wardrobe->OutfitFor(Assassin(kKnife, 0));
  ActorOutfit* sword = client.wardrobe->OutfitFor(Assassin(kSword, 0));
  REQUIRE(knife != nullptr);
  REQUIRE(sword != nullptr);
  CHECK(right->sprite == PieceOf(*knife, ActorPart::kWeapon)->sprite);
  CHECK(left->sprite == PieceOf(*sword, ActorPart::kWeapon)->sprite);
  CHECK(client.wardrobe->missing().empty());
}

TEST_CASE("Ambidextre : l'attaque 3 du corps fait jouer l'attaque 2 aux deux armes",
          "[render][wardrobe]") {
  DualClient client;
  Prepare(client);
  ActorOutfit* outfit = client.wardrobe->OutfitFor(Assassin(kKnife, kSword));
  REQUIRE(outfit != nullptr);
  const ActorPiece* right = PieceOf(*outfit, ActorPart::kWeapon);
  const ActorPiece* left = PieceOf(*outfit, ActorPart::kWeaponTrail);
  REQUIRE(right != nullptr);
  REQUIRE(left != nullptr);

  const std::size_t direction = 3;
  const ComposedActor& attack = outfit->Composed(kAnimationAttack3, direction, 0.0F);
  CHECK(ImageOf(attack, *right) == kAttack2Image);
  CHECK(ImageOf(attack, *left) == kAttack2Image);

  // Une arme tenue seule joue l'action du corps telle quelle.
  ActorOutfit* knife = client.wardrobe->OutfitFor(Assassin(kKnife, 0));
  REQUIRE(knife != nullptr);
  const ActorPiece* alone = PieceOf(*knife, ActorPart::kWeapon);
  REQUIRE(alone != nullptr);
  CHECK_FALSE(alone->single_weapon_sprite);
  CHECK(ImageOf(knife->Composed(kAnimationAttack3, direction, 0.0F), *alone) == kOtherImage);
}

TEST_CASE("Ambidextre : deux armes sous le même sprite gardent la pièce du couple",
          "[render][wardrobe]") {
  DualClient client;
  Prepare(client);
  // Deux dagues sans sprite propre retombent toutes deux sur la dague : l'une
  // couvrirait l'autre.
  ActorOutfit* outfit = client.wardrobe->OutfitFor(Assassin(kKnife, kCutter));
  REQUIRE(outfit != nullptr);
  const ActorPiece* pair = PieceOf(*outfit, ActorPart::kWeapon);
  REQUIRE(pair != nullptr);
  CHECK_FALSE(pair->single_weapon_sprite);
  CHECK(PieceOf(*outfit, ActorPart::kWeaponTrail) == nullptr);
  ActorOutfit* knife = client.wardrobe->OutfitFor(Assassin(kKnife, 0));
  REQUIRE(knife != nullptr);
  CHECK(pair->sprite != PieceOf(*knife, ActorPart::kWeapon)->sprite);
}

TEST_CASE("Ambidextre : la main gauche seule armée, ou un bouclier", "[render][wardrobe]") {
  DualClient client;
  Prepare(client);

  // Seule la gauche tient une arme : elle seule se dessine, au rang de la
  // traînée.
  ActorOutfit* left_only = client.wardrobe->OutfitFor(Assassin(0, kSword));
  REQUIRE(left_only != nullptr);
  CHECK(PieceOf(*left_only, ActorPart::kWeapon) == nullptr);
  const ActorPiece* left = PieceOf(*left_only, ActorPart::kWeaponTrail);
  REQUIRE(left != nullptr);
  CHECK(left->single_weapon_sprite);

  // Un bouclier n'est pas une arme : rien ne se sépare.
  CHECK_FALSE(
      DualWieldSpritePaths(client.wardrobe->catalog(), Assassin(kKnife, kGuard)).has_value());
  CHECK_FALSE(DualWieldSpritePaths(client.wardrobe->catalog(), Assassin(kKnife, 0)).has_value());
}
