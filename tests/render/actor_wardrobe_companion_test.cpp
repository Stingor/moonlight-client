// LA TEINTURE D'UN COMPAGNON : le corps d'un familier ou d'un homoncule, sous
// la clé de sa classe, sur la palette embarquée de son sprite.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/palette.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/style_set.h"
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
using ro::render::kLayerProfileNumber;
using ro::render::LayerDye;
using ro::render::LoadSpriteCatalog;
using ro::render::RampAdjust;
using ro::render::StyleLayer;
using ro::render::StyleSet;
using ro::test_support::AnimationBytes;
using ro::test_support::BuildGrf;
using ro::test_support::BuildSpr;
using ro::test_support::Entry;
using ro::test_support::GrfFileSpec;
using ro::test_support::MakeClient;
using ro::test_support::Poring;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::TempDirectory;
using ro::vfs::Vfs;

namespace {

constexpr std::int32_t kSide = 16;
constexpr std::uint8_t kShades = 8;
constexpr std::uint8_t kRampFirst = 1;
// Le Poring du client d'essai, une classe de familier voisine, et le Lif.
constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kPoporing = 1031;
constexpr std::uint32_t kLif = 6001;

enum class Shade : std::uint8_t { kRed, kGrey };

// Un sprite palettisé de deux images, tout entier sur un dégradé de huit index
// (1 à 8) : rouge, ou gris comme un Lunatic.
std::vector<std::uint8_t> CompanionSpriteBytes(Shade shade) {
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
    const auto green_blue = shade == Shade::kRed ? static_cast<std::uint8_t>(level / 6) : level;
    options.palette[kRampFirst + step] = {level, green_blue,
                                          shade == Shade::kRed ? std::uint8_t{0} : level};
  }
  return BuildSpr(options);
}

// Le client d'essai, dont le Poring devient assez grand pour avoir des pièces,
// avec un Lif rouge et un Lunatic gris.
struct Fixture {
  TempDirectory client;
  std::optional<Vfs> vfs;
  std::optional<ActorWardrobe> wardrobe;

  Fixture() {
    MakeClient(client);
    std::vector<GrfFileSpec> files;
    files.push_back(Entry("data\\sprite\\몬스터\\Poring.spr", CompanionSpriteBytes(Shade::kRed)));
    files.push_back(Entry("data\\sprite\\몬스터\\Poring.act", AnimationBytes(1)));
    files.push_back(Entry("data\\sprite\\homun\\lif.spr", CompanionSpriteBytes(Shade::kGrey)));
    files.push_back(Entry("data\\sprite\\homun\\lif.act", AnimationBytes(1)));
    client.WriteFile("companions.grf", BuildGrf(files));
    auto mounted =
        Vfs::Mount({.root = client.path(), .archives = {"companions.grf", "sprites.grf"}});
    REQUIRE(mounted.has_value());
    vfs.emplace(std::move(*mounted));
    auto catalog = LoadSpriteCatalog(*vfs);
    REQUIRE(catalog.has_value());
    wardrobe.emplace(*vfs, std::move(*catalog));
  }
};

ActorLook Lif() {
  ActorLook look;
  look.job = kLif;
  return look;
}

// Un demi-tour sur chaque pièce.
LayerDye HalfTurn(std::uint32_t pet_class) {
  LayerDye dye{.layer = StyleLayer::kCompanion,
               .key = pet_class,
               .profile = kLayerProfileNumber,
               .adjusts = {}};
  for (std::size_t rank = 0; rank < ro::render::kMaxRamps; ++rank) {
    dye.adjusts[rank] = RampAdjust{.hue = 180};
  }
  return dye;
}

// Vert franc, imposé : la seule teinture qui colore un gris.
LayerDye AbsoluteGreen(std::uint32_t pet_class) {
  LayerDye dye = HalfTurn(pet_class);
  for (std::size_t rank = 0; rank < ro::render::kMaxRamps; ++rank) {
    dye.adjusts[rank] = RampAdjust{.hue = 120, .saturation = 100, .absolute = true};
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

const ActorPiece& BodyOf(ActorWardrobe& wardrobe, const ActorLook& look) {
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  const auto found = std::ranges::find(outfit->pieces(), ActorPart::kBody, &ActorPiece::part);
  REQUIRE(found != outfit->pieces().end());
  return *found;
}

std::array<std::uint8_t, 3> ColorOf(const ActorPiece& piece, std::uint8_t index) {
  const ro::formats::Palette& palette =
      piece.palette != nullptr ? *piece.palette : *piece.sprite->palette();
  return {palette.entries[index][0], palette.entries[index][1], palette.entries[index][2]};
}

bool Reddish(std::array<std::uint8_t, 3> color) { return color[0] > color[2]; }
bool Greenish(std::array<std::uint8_t, 3> color) {
  return color[1] > color[0] && color[1] > color[2];
}

}  // namespace

TEST_CASE("Compagnon : le familier se teint sous la clé de sa classe",
          "[render][wardrobe][companion]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  ActorLook look = Poring();
  look.style = Registered(wardrobe, HalfTurn(kPoring));
  const ActorPiece& body = BodyOf(wardrobe, look);
  CHECK(body.palette_serial != 0);
  for (std::uint8_t step = 0; step < kShades; ++step) {
    CHECK_FALSE(Reddish(ColorOf(body, static_cast<std::uint8_t>(kRampFirst + step))));
  }
}

TEST_CASE("Compagnon : une teinture d'une autre classe ne peint rien",
          "[render][wardrobe][companion]") {
  // Un familier qui a évolué garde ses couleurs : la teinture était faite pour
  // les rampes d'un autre sprite.
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  ActorLook look = Poring();
  look.style = Registered(wardrobe, HalfTurn(kPoporing));
  const ActorPiece& body = BodyOf(wardrobe, look);
  CHECK(body.palette_serial == 0);
  CHECK(Reddish(ColorOf(body, kRampFirst)));
}

TEST_CASE("Compagnon : la teinture appartient à l'acteur, pas au fichier",
          "[render][wardrobe][companion]") {
  // Le Poring sauvage partage le sprite du familier teint : il garde le sien.
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  ActorLook pet = Poring();
  pet.style = Registered(wardrobe, HalfTurn(kPoring));
  const ActorPiece& dyed = BodyOf(wardrobe, pet);
  const ActorLook wild_look = Poring();
  const ActorPiece& wild = BodyOf(wardrobe, wild_look);
  CHECK(dyed.sprite == wild.sprite);
  CHECK(wild.palette_serial == 0);
  CHECK(Reddish(ColorOf(wild, kRampFirst)));
  CHECK_FALSE(Reddish(ColorOf(dyed, kRampFirst)));
}

TEST_CASE("Compagnon : l'homoncule se teint, et un gris ne se colore qu'en absolu",
          "[render][wardrobe][companion]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;

  // Une rotation de teinte laisse un gris gris.
  ActorLook turned = Lif();
  turned.style = Registered(wardrobe, HalfTurn(kLif));
  const ActorPiece& grey = BodyOf(wardrobe, turned);
  const std::array<std::uint8_t, 3> kept = ColorOf(grey, kRampFirst);
  CHECK(kept[0] == kept[1]);
  CHECK(kept[1] == kept[2]);

  ActorLook green = Lif();
  green.style = Registered(wardrobe, AbsoluteGreen(kLif));
  const ActorPiece& painted = BodyOf(wardrobe, green);
  CHECK(painted.palette_serial != 0);
  for (std::uint8_t step = 0; step < kShades; ++step) {
    CHECK(Greenish(ColorOf(painted, static_cast<std::uint8_t>(kRampFirst + step))));
  }
}

TEST_CASE("Compagnon : un objet au sol ne prend pas de teinture", "[render][wardrobe][companion]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  constexpr std::uint32_t kPotion = 501;
  ActorLook potion;
  potion.item = true;
  potion.job = kPotion;
  potion.style = Registered(wardrobe, HalfTurn(kPotion));
  const ActorPiece& body = BodyOf(wardrobe, potion);
  CHECK(body.palette_serial == 0);
}
