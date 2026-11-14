#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/log.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/style_set.h"
#include "ro/sim/style.h"
#include "ro/vfs/vfs.h"

#include "character/style_recipe.h"
#include "support/actor_client.h"
#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"
#include "world/actor_styles.h"

using moonclient::ActorStyles;
using moonclient::FromWire;
using moonclient::ToWire;
using moonclient::WornDyes;
using ro::render::ActorLook;
using ro::render::ActorOutfit;
using ro::render::ActorPart;
using ro::render::ActorPiece;
using ro::render::ActorWardrobe;
using ro::render::HeadDyeKey;
using ro::render::LoadSpriteCatalog;
using ro::render::StyleLayer;
using ro::sim::StyleBook;
using ro::sim::StyleLayerKind;
using ro::sim::StyleLayerRecipe;
using ro::test_support::AnimationBytes;
using ro::test_support::BuildGrf;
using ro::test_support::BuildSpr;
using ro::test_support::Entry;
using ro::test_support::GrfFileSpec;
using ro::test_support::kGoggles;
using ro::test_support::kWings;
using ro::test_support::MakeClient;
using ro::test_support::Novice;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::TempDirectory;
using ro::vfs::Vfs;

namespace {

constexpr std::uint32_t kPlayer = 2000;
constexpr std::uint32_t kOther = 2001;
constexpr std::uint16_t kHair = 1;
constexpr std::uint8_t kProfile = 1;
constexpr std::int16_t kSomeHueTurn = 40;

StyleLayerRecipe Told(StyleLayerKind layer, std::uint32_t key, std::int16_t hue = kSomeHueTurn) {
  StyleLayerRecipe told{.layer = layer, .key = key, .profile = kProfile, .adjusts = {}};
  told.adjusts[0].hue = hue;
  return told;
}

ActorLook Dressed() {
  ActorLook look = Novice();
  look.hair = kHair;
  look.headgears = {0, 0, kGoggles};
  look.robe = kWings;
  return look;
}

struct Fixture {
  TempDirectory client;
  std::optional<Vfs> vfs;
  std::optional<ActorWardrobe> wardrobe;

  explicit Fixture(std::vector<GrfFileSpec> extra = {}) {
    MakeClient(client, ro::test_support::BodySpriteBytes());
    std::vector<std::string> archives = {"sprites.grf"};
    if (!extra.empty()) {
      client.WriteFile("extra.grf", BuildGrf(extra));
      archives.insert(archives.begin(), "extra.grf");
    }
    auto mounted = Vfs::Mount({.root = client.path(), .archives = archives});
    REQUIRE(mounted.has_value());
    vfs.emplace(std::move(*mounted));
    auto catalog = LoadSpriteCatalog(*vfs);
    REQUIRE(catalog.has_value());
    wardrobe.emplace(*vfs, std::move(*catalog));
  }
};

}  // namespace

TEST_CASE("Styles des calques : la traduction garde chaque champ, et refuse un calque inconnu",
          "[app][styles][layers]") {
  StyleLayerRecipe told = Told(StyleLayerKind::kGarment, kWings);
  told.profile = 2;
  told.adjusts[12] =
      ro::sim::StyleAdjust{.hue = 200, .saturation = 50, .value = -20, .absolute = true};
  const std::optional<ro::render::LayerDye> dye = FromWire(told);
  REQUIRE(dye.has_value());
  CHECK(dye->layer == StyleLayer::kGarment);
  CHECK(dye->key == kWings);
  CHECK(dye->profile == 2);
  CHECK(dye->adjusts[12].hue == 200);
  CHECK(dye->adjusts[12].absolute);
  const StyleLayerRecipe back = ToWire(*dye);
  CHECK(back.layer == told.layer);
  CHECK(back.adjusts[12].value == -20);

  StyleLayerRecipe unknown = told;
  unknown.layer = static_cast<StyleLayerKind>(9);
  CHECK_FALSE(FromWire(unknown).has_value());
}

TEST_CASE("Styles des calques : seules les teintures des pièces portées entrent dans l'ensemble",
          "[app][styles][layers]") {
  const std::vector<StyleLayerRecipe> told = {
      Told(StyleLayerKind::kGarment, kWings),
      Told(StyleLayerKind::kHeadgear, 77),
      Told(StyleLayerKind::kHead, HeadDyeKey(kHair, 0)),
      Told(StyleLayerKind::kHead, HeadDyeKey(kHair + 1, 0)),
      Told(StyleLayerKind::kSkin, 0),
      Told(StyleLayerKind::kHeadgear, kGoggles),
  };
  const std::vector<ro::render::LayerDye> worn = WornDyes(told, Dressed());
  // La coiffe 77 n'est pas portée, et la seconde coupe n'est pas celle-ci.
  REQUIRE(worn.size() == 4);
  // Triées par calque, puis par clé.
  CHECK(worn[0].layer == StyleLayer::kHead);
  CHECK(worn[1].layer == StyleLayer::kHeadgear);
  CHECK(worn[2].layer == StyleLayer::kGarment);
  CHECK(worn[3].layer == StyleLayer::kSkin);
}

TEST_CASE("Styles des calques : toutes les suites du corps entrent, le corps choisit la sienne",
          "[app][styles][layers]") {
  const std::vector<StyleLayerRecipe> told = {
      Told(StyleLayerKind::kBodyMore, 0xA1),
      Told(StyleLayerKind::kBodyMore, 0xB2),
      Told(StyleLayerKind::kBodyMore, 0),
  };
  const std::vector<ro::render::LayerDye> worn = WornDyes(told, Dressed());
  // Une suite sans clé ne désigne aucun corps.
  REQUIRE(worn.size() == 2);
  CHECK(worn[0].layer == StyleLayer::kBodyMore);
  CHECK(worn[0].key == 0xA1);
  CHECK(worn[1].key == 0xB2);
}

TEST_CASE("Styles des calques : un compagnon prend la teinture de SA classe, un joueur aucune",
          "[app][styles][layers][companion]") {
  constexpr std::uint32_t kPoring = 1002;
  constexpr std::uint32_t kPoporing = 1031;
  const std::vector<StyleLayerRecipe> told = {
      Told(StyleLayerKind::kCompanion, kPoporing),
      Told(StyleLayerKind::kCompanion, kPoring),
  };
  const ActorLook pet = ro::test_support::Poring();
  const std::vector<ro::render::LayerDye> worn = WornDyes(told, pet);
  REQUIRE(worn.size() == 1);
  CHECK(worn[0].layer == StyleLayer::kCompanion);
  CHECK(worn[0].key == kPoring);
  // Un joueur de même numéro de métier ne porte pas ce calque.
  ActorLook player = Dressed();
  player.job = kPoring;
  CHECK(WornDyes(told, player).empty());

  // Et la garde-robe le peint, sous le gid du compagnon.
  Fixture fixture;
  StyleBook book;
  constexpr std::uint32_t kCompanionGid = 110000042;
  book.RememberLayers(kCompanionGid, {Told(StyleLayerKind::kCompanion, kPoring)});
  ActorStyles styles;
  ActorLook dressed = pet;
  styles.Dress(kCompanionGid, dressed, *fixture.wardrobe, book);
  CHECK(dressed.style != 0);
  // Le Poring sauvage d'à côté, sans bloc, n'en prend aucun.
  ActorLook wild = pet;
  styles.Dress(kCompanionGid + 1, wild, *fixture.wardrobe, book);
  CHECK(wild.style == 0);
}

TEST_CASE("Styles des calques : une tête de groupe emprunte le style que la carte a posé",
          "[app][styles][layers]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  StyleBook book;
  book.RememberLayers(kPlayer, {Told(StyleLayerKind::kHead, HeadDyeKey(kHair, 0))});
  ActorStyles styles;
  CHECK(styles.Worn(kPlayer) == 0);
  ActorLook mine = Dressed();
  styles.Dress(kPlayer, mine, wardrobe, book);
  REQUIRE(mine.style != 0);
  CHECK(styles.Worn(kPlayer) == mine.style);
  // Celui que la carte n'a pas habillé n'a rien à prêter.
  CHECK(styles.Worn(kOther) == 0);
}

TEST_CASE("Styles des calques : une tête hors de vue s'habille sous son char_id",
          "[app][styles][layers][absent]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  StyleBook book;
  const StyleLayerRecipe head = Told(StyleLayerKind::kHead, HeadDyeKey(kHair, 0));
  book.RememberLayers(kPlayer, {head});
  // ⚠ LE MÊME NOMBRE, UN AUTRE ESPACE : ce char_id n'est pas le gid kPlayer.
  constexpr std::uint32_t kCharId = kPlayer;
  ro::sim::AbsentStyle absent;
  absent.shown = true;
  absent.layers = {head};
  book.RememberAbsent(kCharId, absent);

  ActorStyles styles;
  ActorLook seen = Dressed();
  styles.Dress(kPlayer, seen, wardrobe, book);
  ActorLook far = Dressed();
  styles.DressAbsent(kCharId, far, wardrobe, book);
  REQUIRE(far.style != 0);
  // La même recette partage son identifiant, en vue comme hors de vue.
  CHECK(far.style == seen.style);
  CHECK(styles.Worn(kPlayer) == seen.style);

  // « Rien à montrer » : la tête reprend ses couleurs, et le balayage rend
  // l'identifiant que plus personne ne porte.
  book.RememberAbsent(kCharId, ro::sim::AbsentStyle{});
  book.RememberLayers(kPlayer, {});
  ActorLook bare = Dressed();
  styles.DressAbsent(kCharId, bare, wardrobe, book);
  CHECK(bare.style == 0);
  styles.Dress(kPlayer, bare, wardrobe, book);
  styles.Sweep(wardrobe, book);
  CHECK(wardrobe.SetStyle(far.style, ro::render::PaletteRecipe{}));
}

TEST_CASE("Styles des calques : des teintures sans recette de corps habillent l'acteur",
          "[app][styles][layers]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  StyleBook book;
  book.RememberLayers(kPlayer, {Told(StyleLayerKind::kHeadgear, kGoggles)});
  // L'autre porte la même teinture de coiffe, plus celle d'un chapeau rangé.
  book.RememberLayers(
      kOther, {Told(StyleLayerKind::kHeadgear, kGoggles), Told(StyleLayerKind::kHeadgear, 77)});
  ActorStyles styles;
  ActorLook mine = Dressed();
  styles.Dress(kPlayer, mine, wardrobe, book);
  ActorLook theirs = Dressed();
  styles.Dress(kOther, theirs, wardrobe, book);
  REQUIRE(mine.style != 0);
  // 🔴 CE QUI N'EST PAS PORTÉ NE SÉPARE PAS DEUX JOUEURS.
  CHECK(theirs.style == mine.style);

  // Un réglage change : un identifiant neuf.
  book.RememberLayers(kPlayer, {Told(StyleLayerKind::kHeadgear, kGoggles, kSomeHueTurn + 1)});
  ActorLook changed = Dressed();
  styles.Dress(kPlayer, changed, wardrobe, book);
  CHECK(changed.style != 0);
  CHECK(changed.style != mine.style);

  // Plus rien : l'apparence native, et l'identifiant rendu au balayage.
  book.RememberLayers(kPlayer, {});
  CHECK(book.FindLayers(kPlayer) == nullptr);
  ActorLook bare = Dressed();
  styles.Dress(kPlayer, bare, wardrobe, book);
  CHECK(bare.style == 0);
  styles.Sweep(wardrobe, book);
  CHECK(wardrobe.SetStyle(changed.style, ro::render::PaletteRecipe{}));
}

TEST_CASE("Styles des calques : un départ oublie les teintures, un effacement du corps non",
          "[app][styles][layers]") {
  StyleBook book;
  book.RememberLayers(kPlayer, {Told(StyleLayerKind::kSkin, 0)});
  const std::uint32_t revision = book.RevisionOf(kPlayer);
  CHECK(revision != 0);
  book.Forget(kPlayer);
  CHECK(book.FindLayers(kPlayer) != nullptr);
  CHECK(book.RevisionOf(kPlayer) > revision);
  book.ForgetDeparted(kPlayer);
  CHECK(book.FindLayers(kPlayer) == nullptr);
  CHECK(book.RevisionOf(kPlayer) == 0);
  book.RememberLayers(kOther, {Told(StyleLayerKind::kSkin, 0)});
  book.RememberLayers(kPlayer, {Told(StyleLayerKind::kSkin, 0)});
  book.ForgetDepartedExcept(kPlayer);
  CHECK(book.FindLayers(kOther) == nullptr);
  CHECK(book.FindLayers(kPlayer) != nullptr);
}

// ── LE BANC : ce que coûte un joueur entièrement teint ──────────────────────

namespace {

// Des pièces de la taille des vraies. Un corps : 110 images de 60 sur 60 ; une
// tête, une coiffe, une cape : 40 images de 30 sur 30.
constexpr std::int32_t kBodyFrames = 110;
constexpr std::int32_t kBodySide = 60;
constexpr std::int32_t kPieceFrames = 40;
constexpr std::int32_t kPieceSide = 30;
// Huit pièces de seize nuances, plus la peau en 129 à 135.
constexpr int kPieces = 8;
constexpr int kShades = 16;
constexpr int kSkinFirst = 129;
constexpr int kSkinShades = 7;
constexpr int kBrightest = 255;
constexpr int kShadeStep = 12;
constexpr std::int16_t kBenchHueTurn = 30;
// Une seconde de jeu à soixante images, pour cent joueurs.
constexpr int kFrames = 6000;

std::vector<std::uint8_t> BenchSpriteBytes(std::int32_t frames, std::int32_t side) {
  constexpr int kPainted = kPieces * kShades;
  SprBuildOptions options;
  for (std::int32_t frame = 0; frame < frames; ++frame) {
    SprIndexedFrameSpec spec{.width = side, .height = side, .indices = {}};
    for (std::int32_t pixel = 0; pixel < side * side; ++pixel) {
      const int at = (pixel + frame) % (kPainted + kSkinShades);
      spec.indices.push_back(
          static_cast<std::uint8_t>(at < kPainted ? 1 + at : kSkinFirst + (at - kPainted)));
    }
    options.indexed.push_back(std::move(spec));
  }
  options.palette.assign(ro::formats::kPaletteEntryCount, {0, 0, 0});
  constexpr std::array<std::array<int, 3>, kPieces> kHues = {{{255, 40, 40},
                                                              {40, 255, 40},
                                                              {40, 40, 255},
                                                              {255, 255, 40},
                                                              {40, 255, 255},
                                                              {255, 40, 255},
                                                              {255, 140, 40},
                                                              {140, 40, 255}}};
  for (int piece = 0; piece < kPieces; ++piece) {
    for (int shade = 0; shade < kShades; ++shade) {
      const int level = kBrightest - shade * kShadeStep;
      const auto dim = [level](int channel) {
        return static_cast<std::uint8_t>(channel * level / kBrightest);
      };
      options.palette[static_cast<std::size_t>(1 + piece * kShades + shade)] = {
          dim(kHues[static_cast<std::size_t>(piece)][0]),
          dim(kHues[static_cast<std::size_t>(piece)][1]),
          dim(kHues[static_cast<std::size_t>(piece)][2])};
    }
  }
  for (int shade = 0; shade < kSkinShades; ++shade) {
    const auto level = static_cast<std::uint8_t>(kBrightest - shade * 2 * kShadeStep);
    options.palette[static_cast<std::size_t>(kSkinFirst + shade)] = {
        level, static_cast<std::uint8_t>(level * 7 / 8), static_cast<std::uint8_t>(level * 3 / 4)};
  }
  return BuildSpr(options);
}

[[nodiscard]] double MillisecondsSince(std::chrono::steady_clock::time_point since) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - since)
      .count();
}

}  // namespace

// Un banc, lancé à la main : il mesure, il ne juge pas. Ses chiffres ne valent
// que pour la configuration bâtie, qu'il faut citer avec eux.
TEST_CASE("Banc styles des calques : un joueur entièrement teint, image après image",
          "[app][styles][layers][.bench]") {
  std::vector<GrfFileSpec> extra;
  for (const std::string_view base :
       {"data\\sprite\\인간족\\머리통\\남\\2_남", "data\\sprite\\악세사리\\남\\남_고글"}) {
    extra.push_back(Entry(std::string(base) + ".spr", BenchSpriteBytes(kPieceFrames, kPieceSide)));
    extra.push_back(Entry(std::string(base) + ".act", AnimationBytes(2)));
  }
  extra.push_back(
      Entry("data\\sprite\\로브\\ailes\\ailes.spr", BenchSpriteBytes(kPieceFrames, kPieceSide)));
  extra.push_back(Entry("data\\sprite\\인간족\\몸통\\남\\초보자_남.spr",
                        BenchSpriteBytes(kBodyFrames, kBodySide)));
  Fixture fixture(std::move(extra));
  ActorWardrobe& wardrobe = *fixture.wardrobe;

  // Les trois coiffes portent la même apparence : trois pièces à teindre.
  ActorLook worn = Dressed();
  worn.headgears = {kGoggles, kGoggles, kGoggles};
  const std::uint32_t body_key = wardrobe.BodyKeyFor(worn);
  REQUIRE(body_key != 0);

  ro::sim::StyleRecipe body;
  for (ro::sim::StyleAdjust& adjust : body.ramps) {
    adjust.hue = kBenchHueTurn;
  }
  std::vector<StyleLayerRecipe> layers;
  for (const auto& [layer, key] : std::array<std::pair<StyleLayerKind, std::uint32_t>, 4>{
           {{StyleLayerKind::kHead, HeadDyeKey(kHair, 0)},
            {StyleLayerKind::kHeadgear, kGoggles},
            {StyleLayerKind::kGarment, kWings},
            {StyleLayerKind::kSkin, 0}}}) {
    StyleLayerRecipe told{.layer = layer, .key = key, .profile = kProfile, .adjusts = {}};
    for (std::size_t rank = 0; rank < ro::render::kMaxRamps; ++rank) {
      told.adjusts[rank].hue = kBenchHueTurn;
    }
    layers.push_back(told);
  }

  const auto measure = [&](std::string_view what, bool with_body, bool with_layers) {
    StyleBook book;
    if (with_body) {
      book.BeginBatchFor(kPlayer);
      book.Remember(kPlayer, body_key, body, /*is_fallback=*/true);
    }
    if (with_layers) {
      book.RememberLayers(kPlayer, layers);
    }
    ActorStyles styles;
    // La première image : les palettes se peignent.
    const auto first = std::chrono::steady_clock::now();
    ActorLook look = worn;
    styles.Dress(kPlayer, look, wardrobe, book);
    ActorOutfit* outfit = wardrobe.OutfitFor(look);
    REQUIRE(outfit != nullptr);
    std::size_t painted = 0;
    for (const ActorPiece& piece : outfit->pieces()) {
      painted += piece.palette_serial != 0 ? 1 : 0;
    }
    const double first_ms = MillisecondsSince(first);
    // Les suivantes : ce que chaque image paie pour ce joueur.
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < kFrames; ++frame) {
      ActorLook again = worn;
      styles.Dress(kPlayer, again, wardrobe, book);
      ActorOutfit* drawn = wardrobe.OutfitFor(again);
      REQUIRE(drawn != nullptr);
      static_cast<void>(drawn->Composed(ro::render::kAnimationIdle, 0, 0.0F));
    }
    const double spent_ms = MillisecondsSince(start);
    ro::core::LogInfo(
        "banc calques : {} — première image {:.3f} ms, {} pièce(s) peinte(s) ; puis {:.3f} µs "
        "par image",
        what, first_ms, painted, spent_ms * 1000.0 / kFrames);
    styles.Sweep(wardrobe, StyleBook{});
    return painted;
  };
  CHECK(measure("sans style", false, false) == 0);
  CHECK(measure("corps seul", true, false) == 1);
  // Corps, tête, trois coiffes, cape.
  CHECK(measure("entièrement teint", true, true) == 6);
}
