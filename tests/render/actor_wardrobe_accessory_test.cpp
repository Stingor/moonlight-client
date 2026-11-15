// LA TEINTURE DE L'OISEAU ET DU CHARIOT : sous le numéro de leur image, sur la
// palette embarquée de leur sprite, pour le porteur qui la tient.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/palette.h"
#include "ro/formats/spr.h"
#include "ro/render/accessory_look.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/companion_cart.h"
#include "ro/render/companion_falcon.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/style_set.h"
#include "ro/vfs/vfs.h"

#include "support/actor_client.h"
#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using ro::render::AccessoryKind;
using ro::render::ActorLook;
using ro::render::ActorOutfit;
using ro::render::ActorPart;
using ro::render::ActorPiece;
using ro::render::ActorWardrobe;
using ro::render::kLayerProfileNumber;
using ro::render::LayerDye;
using ro::render::LoadSpriteCatalog;
using ro::render::RampAdjust;
using ro::render::SpriteImageKey;
using ro::render::StyleLayer;
using ro::render::StyleSet;
using ro::test_support::AnimationBytes;
using ro::test_support::BuildGrf;
using ro::test_support::BuildSpr;
using ro::test_support::Entry;
using ro::test_support::GrfFileSpec;
using ro::test_support::MakeClient;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::TempDirectory;
using ro::vfs::Vfs;

namespace {

constexpr std::int32_t kSide = 16;
constexpr std::uint8_t kShades = 8;
constexpr std::uint8_t kRampFirst = 1;
// Le métier du tireur d'élite, qui a son faucon à lui, et le type 2 de chariot.
constexpr std::uint32_t kSniper = 4012;
constexpr std::uint32_t kCartTwo = 2;

// Un sprite palettisé de deux images, tout entier sur un dégradé rouge de huit
// index (1 à 8).
std::vector<std::uint8_t> RedSpriteBytes() {
  SprBuildOptions options;
  for (int frame = 0; frame < 2; ++frame) {
    SprIndexedFrameSpec spec{.width = kSide, .height = kSide, .indices = {}};
    for (std::int32_t pixel = 0; pixel < kSide * kSide; ++pixel) {
      spec.indices.push_back(static_cast<std::uint8_t>(kRampFirst + (pixel / 2) % kShades));
    }
    options.indexed.push_back(std::move(spec));
  }
  options.palette.assign(ro::formats::kPaletteEntryCount, {0, 0, 0});
  for (std::uint8_t step = 0; step < kShades; ++step) {
    const auto level = static_cast<std::uint8_t>(240 - step * 25);
    options.palette[kRampFirst + step] = {level, static_cast<std::uint8_t>(level / 6), 0};
  }
  return BuildSpr(options);
}

// Le chemin de l'.act qui accompagne un .spr.
std::string ActOf(std::string_view spr) {
  std::string act(spr.substr(0, spr.size() - std::string_view(".spr").size()));
  return act + ".act";
}

// Le client d'essai, avec le faucon ordinaire et le chariot de type 2.
struct Fixture {
  TempDirectory client;
  std::optional<Vfs> vfs;
  std::optional<ActorWardrobe> wardrobe;
  std::optional<ro::formats::SprSprite> hawk;

  Fixture() {
    MakeClient(client);
    const std::string_view hawk_path =
        *ro::render::AccessorySpritePath(AccessoryKind::kFalcon, ro::render::kFalconDyeHawk);
    const std::string_view cart_path =
        *ro::render::AccessorySpritePath(AccessoryKind::kCart, kCartTwo);
    std::vector<GrfFileSpec> files;
    files.push_back(Entry(std::string(hawk_path), RedSpriteBytes()));
    files.push_back(Entry(ActOf(hawk_path), AnimationBytes(1)));
    files.push_back(Entry(std::string(cart_path), RedSpriteBytes()));
    files.push_back(Entry(ActOf(cart_path), AnimationBytes(1)));
    client.WriteFile("accessories.grf", BuildGrf(files));
    auto mounted =
        Vfs::Mount({.root = client.path(), .archives = {"accessories.grf", "sprites.grf"}});
    REQUIRE(mounted.has_value());
    vfs.emplace(std::move(*mounted));
    auto catalog = LoadSpriteCatalog(*vfs);
    REQUIRE(catalog.has_value());
    wardrobe.emplace(*vfs, std::move(*catalog));
    auto bytes = vfs->Read(std::string(hawk_path));
    REQUIRE(bytes.has_value());
    auto parsed = ro::formats::SprSprite::Parse(*bytes);
    REQUIRE(parsed.has_value());
    hawk.emplace(std::move(*parsed));
  }
};

// Un demi-tour sur chaque pièce.
LayerDye HalfTurn(StyleLayer layer, std::uint32_t key) {
  LayerDye dye{.layer = layer, .key = key, .profile = kLayerProfileNumber, .adjusts = {}};
  for (std::size_t rank = 0; rank < ro::render::kMaxRamps; ++rank) {
    dye.adjusts[rank] = RampAdjust{.hue = 180};
  }
  return dye;
}

std::uint32_t Registered(ActorWardrobe& wardrobe, LayerDye dye) {
  const std::uint32_t style = wardrobe.NewStyleId();
  StyleSet set;
  set.layers = {std::move(dye)};
  REQUIRE(wardrobe.SetStyleSet(style, std::move(set)));
  return style;
}

bool Reddish(const ro::formats::Palette& palette, std::uint8_t index) {
  return palette.entries[index][0] > palette.entries[index][2];
}

ActorLook AccessoryLook(AccessoryKind kind, std::uint8_t key) {
  ActorLook look;
  look.accessory = kind;
  look.accessory_key = key;
  return look;
}

}  // namespace

TEST_CASE("Accessoire : chaque image a sa clé, jamais nulle", "[render][accessory]") {
  // Le faucon du bébé chasseur a l'image du faucon ordinaire : il partage sa clé.
  std::set<std::string_view> hawk_images;
  for (const ro::render::FalconArt& art : ro::render::AllFalconArts()) {
    CHECK(art.dye_key != 0);
    const std::optional<ro::render::FalconArt> keyed = ro::render::FalconArtOfDyeKey(art.dye_key);
    REQUIRE(keyed.has_value());
    CHECK(keyed->spr_path == art.spr_path);
  }
  CHECK(ro::render::PlayerFalconArtFor(kSniper).dye_key == ro::render::kFalconDyeSniperHawk);
  CHECK_FALSE(ro::render::FalconArtOfDyeKey(0).has_value());

  // Le bébé prend la clé de l'adulte, le super novice la sienne.
  for (const ro::render::CartArt& art : ro::render::AllCartArts()) {
    CHECK(art.dye_key != 0);
    REQUIRE(ro::render::CartArtOfDyeKey(art.dye_key).has_value());
  }
  constexpr std::uint32_t kNovice = 0;
  constexpr std::uint32_t kSuperNovice = 23;
  constexpr std::uint32_t kBabyMerchant = 4028;
  CHECK(ro::render::CartArtFor(static_cast<std::int32_t>(kCartTwo), kNovice, 0)->dye_key ==
        kCartTwo);
  CHECK(ro::render::CartArtFor(static_cast<std::int32_t>(kCartTwo), kBabyMerchant, 0)->dye_key ==
        kCartTwo);
  CHECK(ro::render::CartArtFor(static_cast<std::int32_t>(kCartTwo), kSuperNovice, 0)->dye_key ==
        ro::render::kSuperNoviceCartDyeKey);
  CHECK_FALSE(ro::render::CartArtOfDyeKey(0).has_value());
}

TEST_CASE("Accessoire : l'oiseau prend la teinture de sa clé, et d'elle seule",
          "[render][accessory]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  const ro::formats::SprSprite& hawk = *fixture.hawk;

  const std::uint32_t dyed =
      Registered(wardrobe, HalfTurn(StyleLayer::kFalcon, ro::render::kFalconDyeHawk));
  const SpriteImageKey painted =
      wardrobe.AccessoryImage(dyed, hawk, StyleLayer::kFalcon, ro::render::kFalconDyeHawk);
  REQUIRE(painted.palette != nullptr);
  CHECK(painted.palette_serial != 0);
  CHECK_FALSE(Reddish(*painted.palette, kRampFirst));

  // Une teinture d'un autre oiseau, d'un autre calque, ou aucun style : rien.
  const std::uint32_t other =
      Registered(wardrobe, HalfTurn(StyleLayer::kFalcon, ro::render::kFalconDyeOwl));
  CHECK(wardrobe.AccessoryImage(other, hawk, StyleLayer::kFalcon, ro::render::kFalconDyeHawk)
            .palette == nullptr);
  const std::uint32_t cart =
      Registered(wardrobe, HalfTurn(StyleLayer::kCart, ro::render::kFalconDyeHawk));
  CHECK(wardrobe.AccessoryImage(cart, hawk, StyleLayer::kFalcon, ro::render::kFalconDyeHawk)
            .palette == nullptr);
  const SpriteImageKey native =
      wardrobe.AccessoryImage(0, hawk, StyleLayer::kFalcon, ro::render::kFalconDyeHawk);
  CHECK(native.sprite == &hawk);
  CHECK(native.palette == nullptr);
  CHECK(native.palette_serial == 0);
}

TEST_CASE("Accessoire : montré seul, il se dessine et se teint comme un corps",
          "[render][accessory]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  ActorLook look = AccessoryLook(AccessoryKind::kCart, static_cast<std::uint8_t>(kCartTwo));

  // Les pièces se mesurent comme celles d'une coiffe.
  const ActorWardrobe::BodyRamps measure = wardrobe.LayerRampsFor(look, ActorPart::kBody);
  CHECK_FALSE(measure.ramps.empty());

  look.style = Registered(wardrobe, HalfTurn(StyleLayer::kCart, kCartTwo));
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  const auto body = std::ranges::find(outfit->pieces(), ActorPart::kBody, &ActorPiece::part);
  REQUIRE(body != outfit->pieces().end());
  REQUIRE(body->palette != nullptr);
  CHECK(body->palette_serial != 0);
  CHECK_FALSE(Reddish(*body->palette, kRampFirst));

  // Sous la clé d'un autre chariot, il garde ses couleurs.
  ActorLook other = AccessoryLook(AccessoryKind::kCart, static_cast<std::uint8_t>(kCartTwo));
  other.style = Registered(wardrobe, HalfTurn(StyleLayer::kCart, kCartTwo + 1));
  ActorOutfit* plain = wardrobe.OutfitFor(other);
  REQUIRE(plain != nullptr);
  const auto kept = std::ranges::find(plain->pieces(), ActorPart::kBody, &ActorPiece::part);
  REQUIRE(kept != plain->pieces().end());
  CHECK(kept->palette_serial == 0);
}
