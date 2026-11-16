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

#include "ro/formats/image.h"
#include "ro/formats/palette.h"
#include "ro/render/actor_layers.h"
#include "ro/render/actor_palettes.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/hue_classes.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
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
using ro::render::HeadDyeKey;
using ro::render::kAnimationIdle;
using ro::render::kHueClassProfileNumber;
using ro::render::kLayerProfileNumber;
using ro::render::LayerDye;
using ro::render::LoadSpriteCatalog;
using ro::render::RampAdjust;
using ro::render::SkinAdjustable;
using ro::render::StyleLayer;
using ro::render::StyleSet;
using ro::test_support::AnimationBytes;
using ro::test_support::BodySpriteBytes;
using ro::test_support::BuildGrf;
using ro::test_support::BuildSpr;
using ro::test_support::Entry;
using ro::test_support::GrfFileSpec;
using ro::test_support::kBrokenHeadgear;
using ro::test_support::kGoggles;
using ro::test_support::kSummoner;
using ro::test_support::kWings;
using ro::test_support::MakeClient;
using ro::test_support::Novice;
using ro::test_support::SprBgraFrameSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::TempDirectory;
using ro::vfs::Vfs;

namespace {

// ── UN CLIENT DONT CHAQUE CALQUE A DE QUOI SE TEINDRE ───────────────────────
//
// Le client partagé porte des sprites de deux pixels, sur lesquels aucune
// rampe ne passe le seuil : une teinture n'y changerait rien, et l'essai
// passerait en comparant deux palettes intactes. Une seconde archive, montée
// AVANT lui, remplace donc la tête, les lunettes, les ailes et le corps par des
// sprites assez grands, en ajoute un en couleurs vraies et l'habit du GM.

constexpr std::int32_t kSide = 16;
constexpr std::uint8_t kShades = 8;
// Le premier index de la pièce rouge, et celui de la peau humaine.
constexpr std::uint8_t kRampFirst = 1;
constexpr std::uint8_t kSkinFirst = 129;
constexpr std::uint8_t kSkinShades = 7;
// Le dégradé chair des sprites humains, du clair au foncé : c'est à ses
// couleurs qu'un corps se reconnaît une peau.
constexpr std::array<std::array<std::uint8_t, 3>, kSkinShades> kSkinGradient = {{{255, 225, 207},
                                                                                 {255, 198, 178},
                                                                                 {246, 174, 159},
                                                                                 {220, 144, 132},
                                                                                 {189, 115, 107},
                                                                                 {158, 86, 82},
                                                                                 {130, 63, 59}}};
// La coupe du novice d'essai, et sa couleur de cheveux : aucune.
constexpr std::uint16_t kHair = 1;

// Un sprite palettisé : une moitié de ses pixels sur un dégradé rouge (index 1
// à 8), l'autre sur le dégradé chair de la peau (129 à 135). Deux images.
std::vector<std::uint8_t> LayeredSpriteBytes() {
  SprBuildOptions options;
  for (int frame = 0; frame < 2; ++frame) {
    SprIndexedFrameSpec spec{.width = kSide, .height = kSide, .indices = {}};
    for (std::int32_t pixel = 0; pixel < kSide * kSide; ++pixel) {
      const auto step = static_cast<std::uint8_t>(pixel / 2);
      spec.indices.push_back(pixel % 2 == 0
                                 ? static_cast<std::uint8_t>(kRampFirst + step % kShades)
                                 : static_cast<std::uint8_t>(kSkinFirst + step % kSkinShades));
    }
    options.indexed.push_back(std::move(spec));
  }
  options.palette.assign(ro::formats::kPaletteEntryCount, {0, 0, 0});
  for (std::uint8_t shade = 0; shade < kShades; ++shade) {
    const auto level = static_cast<std::uint8_t>(240 - shade * 25);
    options.palette[kRampFirst + shade] = {level, static_cast<std::uint8_t>(level / 6), 0};
  }
  for (std::uint8_t shade = 0; shade < kSkinShades; ++shade) {
    options.palette[kSkinFirst + shade] = {kSkinGradient[shade][0], kSkinGradient[shade][1],
                                           kSkinGradient[shade][2]};
  }
  return BuildSpr(options);
}

// LE MÊME, OÙ LA PEAU NE PÈSE QU'UN PIXEL PAR TEINTE : trop peu pour qu'une
// rampe la prenne, comme les épaules d'un cavalier sur sa monture.
std::vector<std::uint8_t> ThinSkinSpriteBytes() {
  SprBuildOptions options;
  SprIndexedFrameSpec spec{.width = kSide, .height = kSide, .indices = {}};
  for (std::int32_t pixel = 0; pixel < kSide * kSide; ++pixel) {
    spec.indices.push_back(pixel < kSkinShades
                               ? static_cast<std::uint8_t>(kSkinFirst + pixel)
                               : static_cast<std::uint8_t>(kRampFirst + pixel % kShades));
  }
  options.indexed = {spec, spec};
  options.palette.assign(ro::formats::kPaletteEntryCount, {0, 0, 0});
  for (std::uint8_t shade = 0; shade < kShades; ++shade) {
    const auto level = static_cast<std::uint8_t>(240 - shade * 25);
    options.palette[kRampFirst + shade] = {level, static_cast<std::uint8_t>(level / 6), 0};
  }
  for (std::uint8_t shade = 0; shade < kSkinShades; ++shade) {
    options.palette[kSkinFirst + shade] = {kSkinGradient[shade][0], kSkinGradient[shade][1],
                                           kSkinGradient[shade][2]};
  }
  return BuildSpr(options);
}

// Un sprite en couleurs vraies seulement : deux images rouge franc.
std::vector<std::uint8_t> TrueColorSpriteBytes() {
  SprBuildOptions options;
  for (int frame = 0; frame < 2; ++frame) {
    SprBgraFrameSpec spec{.width = kSide, .height = kSide, .pixels = {}, .alpha = {}};
    spec.pixels.assign(static_cast<std::size_t>(kSide) * kSide, {255, 0, 0});
    options.bgra.push_back(std::move(spec));
  }
  return BuildSpr(options);
}

// Son animation : celle des essais, mais chaque calque vise la section des
// images en couleurs vraies.
std::vector<std::uint8_t> TrueColorAnimationBytes() {
  ro::test_support::ActBuildOptions options;
  for (std::size_t action = 0; action < 2 * ro::render::kDirectionCount; ++action) {
    ro::test_support::ActActionSpec spec;
    for (std::int32_t frame = 0; frame < ro::test_support::kFramesPerAction; ++frame) {
      ro::test_support::ActFrameSpec frame_spec;
      ro::test_support::ActLayerSpec layer;
      layer.frame_index = frame;
      layer.section = 1;
      frame_spec.layers.push_back(layer);
      frame_spec.anchors.push_back(ro::test_support::ActAnchorSpec{});
      spec.frames.push_back(std::move(frame_spec));
    }
    options.actions.push_back(std::move(spec));
    options.speeds.push_back(ro::test_support::kFrameSpeed);
  }
  return ro::test_support::BuildAct(options);
}

void MakeLayeredClient(const TempDirectory& client) {
  MakeClient(client, LayeredSpriteBytes());
  std::vector<GrfFileSpec> files;
  for (const std::string_view base :
       {"data\\sprite\\인간족\\머리통\\남\\2_남", "data\\sprite\\악세사리\\남\\남_고글",
        "data\\sprite\\인간족\\몸통\\남\\운영자_남"}) {
    files.push_back(Entry(std::string(base) + ".spr", LayeredSpriteBytes()));
    files.push_back(Entry(std::string(base) + ".act", AnimationBytes(2)));
  }
  // L'autruche du Sniper, dont la peau ne se règle pas.
  files.push_back(
      Entry("data\\sprite\\인간족\\몸통\\남\\타조스나이퍼_남.spr", ThinSkinSpriteBytes()));
  files.push_back(Entry("data\\sprite\\인간족\\몸통\\남\\타조스나이퍼_남.act", AnimationBytes(2)));
  files.push_back(Entry("data\\sprite\\로브\\ailes\\ailes.spr", LayeredSpriteBytes()));
  // La coiffe abîmée du client partagé devient une coiffe en couleurs vraies.
  files.push_back(Entry("data\\sprite\\악세사리\\남\\남_casse.spr", TrueColorSpriteBytes()));
  files.push_back(Entry("data\\sprite\\악세사리\\남\\남_casse.act", TrueColorAnimationBytes()));
  client.WriteFile("layers.grf", BuildGrf(files));
}

struct Fixture {
  TempDirectory client;
  std::optional<Vfs> vfs;
  std::optional<ActorWardrobe> wardrobe;

  Fixture() {
    MakeLayeredClient(client);
    auto mounted = Vfs::Mount({.root = client.path(), .archives = {"layers.grf", "sprites.grf"}});
    REQUIRE(mounted.has_value());
    vfs.emplace(std::move(*mounted));
    auto catalog = LoadSpriteCatalog(*vfs);
    REQUIRE(catalog.has_value());
    wardrobe.emplace(*vfs, std::move(*catalog));
  }
};

// Un novice qui porte les lunettes en haut et les ailes.
ActorLook Dressed() {
  ActorLook look = Novice();
  look.hair = kHair;
  look.headgears = {0, 0, kGoggles};
  look.robe = kWings;
  return look;
}

// Une teinture qui fait pivoter la première pièce d'un demi-tour.
LayerDye HalfTurn(StyleLayer layer, std::uint32_t key) {
  LayerDye dye{.layer = layer, .key = key, .profile = kLayerProfileNumber, .adjusts = {}};
  dye.adjusts[0] = RampAdjust{.hue = 180};
  return dye;
}

// La peau imposée verte.
LayerDye GreenSkin() {
  LayerDye dye{.layer = StyleLayer::kSkin, .key = 0, .profile = kLayerProfileNumber, .adjusts = {}};
  dye.adjusts[0] = RampAdjust{.hue = 120, .saturation = 100, .absolute = true};
  return dye;
}

// La peau d'un corps, imposée bleue, sous la clé de ce corps.
LayerDye BlueBodySkin(std::uint32_t body_key) {
  LayerDye dye{.layer = StyleLayer::kBodySkin,
               .key = body_key,
               .profile = kLayerProfileNumber,
               .adjusts = {}};
  dye.adjusts[0] = RampAdjust{.hue = 240, .saturation = 100, .absolute = true};
  return dye;
}

const ActorPiece* PieceOf(const ActorOutfit& outfit, ActorPart part) {
  const auto found = std::ranges::find(outfit.pieces(), part, &ActorPiece::part);
  return found != outfit.pieces().end() ? &*found : nullptr;
}

// La couleur d'un index sur la palette que porte une pièce.
std::array<std::uint8_t, 3> ColorOf(const ActorPiece& piece, std::uint8_t index) {
  const ro::formats::Palette& palette =
      piece.palette != nullptr ? *piece.palette : *piece.sprite->palette();
  return {palette.entries[index][0], palette.entries[index][1], palette.entries[index][2]};
}

// Rouge d'origine, ou pivoté d'un demi-tour vers le cyan.
bool Reddish(std::array<std::uint8_t, 3> color) { return color[0] > color[2]; }

std::uint32_t Registered(ActorWardrobe& wardrobe, StyleSet set) {
  const std::uint32_t style = wardrobe.NewStyleId();
  REQUIRE(wardrobe.SetStyleSet(style, std::move(set)));
  return style;
}

}  // namespace

TEST_CASE("Calques : chaque pièce se teint par SA clé, et par elle seule",
          "[render][wardrobe][layers]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;

  StyleSet set;
  set.layers = {HalfTurn(StyleLayer::kHead, HeadDyeKey(kHair, 0)),
                HalfTurn(StyleLayer::kHeadgear, kGoggles), HalfTurn(StyleLayer::kGarment, kWings)};
  ActorLook look = Dressed();
  look.style = Registered(wardrobe, set);
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);

  for (const ActorPart part : {ActorPart::kHead, ActorPart::kHeadgearTop, ActorPart::kGarment}) {
    const ActorPiece* piece = PieceOf(*outfit, part);
    REQUIRE(piece != nullptr);
    CHECK(piece->palette_serial != 0);
    CHECK_FALSE(Reddish(ColorOf(*piece, kRampFirst)));
  }
  // 🔴 LE CORPS N'A NI RECETTE NI PEAU : il garde ses couleurs.
  const ActorPiece* body = PieceOf(*outfit, ActorPart::kBody);
  REQUIRE(body != nullptr);
  CHECK(body->palette_serial == 0);
  CHECK(Reddish(ColorOf(*body, kRampFirst)));

  // UNE AUTRE CLÉ NE DÉSIGNE PAS CETTE PIÈCE : une autre coupe, d'autres
  // lunettes, une autre robe gardent leurs couleurs.
  StyleSet elsewhere;
  elsewhere.layers = {HalfTurn(StyleLayer::kHead, HeadDyeKey(kHair + 1, 0)),
                      HalfTurn(StyleLayer::kHeadgear, kGoggles + 10),
                      HalfTurn(StyleLayer::kGarment, kWings + 10)};
  ActorLook other = Dressed();
  other.style = Registered(wardrobe, elsewhere);
  ActorOutfit* untouched = wardrobe.OutfitFor(other);
  REQUIRE(untouched != nullptr);
  for (const ActorPart part : {ActorPart::kHead, ActorPart::kHeadgearTop, ActorPart::kGarment}) {
    const ActorPiece* piece = PieceOf(*untouched, part);
    REQUIRE(piece != nullptr);
    CHECK(piece->palette_serial == 0);
    CHECK(Reddish(ColorOf(*piece, kRampFirst)));
  }
}

TEST_CASE("Calques : la peau est la même sur la tête et sur le corps, et passe en dernier",
          "[render][wardrobe][layers]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;

  // Une recette de corps qui fait tout pivoter, peau comprise si une rampe la
  // prend : la peau doit l'emporter.
  StyleSet set;
  ro::render::PaletteRecipe turned;
  for (RampAdjust& adjust : turned.ramps) {
    adjust.hue = 180;
  }
  set.body = turned;
  set.layers = {GreenSkin(), HalfTurn(StyleLayer::kHead, HeadDyeKey(kHair, 0))};
  ActorLook look = Dressed();
  look.style = Registered(wardrobe, set);
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  const ActorPiece* body = PieceOf(*outfit, ActorPart::kBody);
  const ActorPiece* head = PieceOf(*outfit, ActorPart::kHead);
  REQUIRE(body != nullptr);
  REQUIRE(head != nullptr);

  for (std::uint8_t shade = 0; shade < kSkinShades; ++shade) {
    const auto index = static_cast<std::uint8_t>(kSkinFirst + shade);
    const std::array<std::uint8_t, 3> on_body = ColorOf(*body, index);
    CHECK(on_body == ColorOf(*head, index));
    // Verte : le vert domine.
    CHECK(on_body[1] > on_body[0]);
    CHECK(on_body[1] > on_body[2]);
  }
  // Les cheveux suivent leur teinture, pas la peau.
  CHECK_FALSE(Reddish(ColorOf(*head, kRampFirst)));
  // ⚠ LES COIFFES ET LA CAPE N'ONT PAS DE PEAU : les mêmes index y restent ceux
  // du dessin.
  const ActorPiece* goggles = PieceOf(*outfit, ActorPart::kHeadgearTop);
  REQUIRE(goggles != nullptr);
  CHECK(ColorOf(*goggles, kSkinFirst) == std::array<std::uint8_t, 3>{255, 225, 207});
}

TEST_CASE("Calques : la peau du corps habille les bras, celle du visage la tête",
          "[render][wardrobe][layers]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  const ActorLook dressed = Dressed();
  const std::uint32_t body_key = wardrobe.BodyKeyFor(dressed);
  REQUIRE(body_key != 0);
  const auto green = [](std::array<std::uint8_t, 3> color) {
    return color[1] > color[0] && color[1] > color[2];
  };
  const auto blue = [](std::array<std::uint8_t, 3> color) {
    return color[2] > color[0] && color[2] > color[1];
  };
  // Le dessin : la peau d'origine du sprite d'essai.
  const std::array<std::uint8_t, 3> drawn{255, 225, 207};

  // Les index de peau d'une pièce, tous de la même famille.
  const auto all_of = [](const ActorPiece& piece, auto family) {
    for (std::uint8_t shade = 0; shade < kSkinShades; ++shade) {
      if (!family(ColorOf(piece, static_cast<std::uint8_t>(kSkinFirst + shade)))) {
        return false;
      }
    }
    return true;
  };
  const auto pieces = [&wardrobe, &dressed](StyleSet set) {
    ActorLook look = dressed;
    look.style = Registered(wardrobe, std::move(set));
    ActorOutfit* outfit = wardrobe.OutfitFor(look);
    REQUIRE(outfit != nullptr);
    const ActorPiece* body = PieceOf(*outfit, ActorPart::kBody);
    const ActorPiece* head = PieceOf(*outfit, ActorPart::kHead);
    REQUIRE(body != nullptr);
    REQUIRE(head != nullptr);
    return std::pair{body, head};
  };

  SECTION("chacune sur sa partie") {
    const auto [body, head] = pieces(StyleSet{.layers = {GreenSkin(), BlueBodySkin(body_key)}});
    CHECK(all_of(*body, blue));
    CHECK(all_of(*head, green));
  }

  SECTION("un corps sans peau propre prend celle du visage") {
    // La peau d'un AUTRE corps ne désigne pas celui-ci.
    const auto [body, head] = pieces(StyleSet{.layers = {GreenSkin(), BlueBodySkin(body_key + 1)}});
    CHECK(all_of(*body, green));
    CHECK(all_of(*head, green));
  }

  SECTION("la peau du corps seule laisse le visage du dessin") {
    const auto [body, head] = pieces(StyleSet{.layers = {BlueBodySkin(body_key)}});
    CHECK(all_of(*body, blue));
    CHECK(ColorOf(*head, kSkinFirst) == drawn);
  }

  SECTION("elle ne prend ni reste ni retouche, et un profil inconnu ne compte pas") {
    LayerDye odd = BlueBodySkin(body_key);
    odd.profile = kHueClassProfileNumber;
    const auto [body, head] = pieces(StyleSet{.layers = {GreenSkin(), odd}});
    CHECK(all_of(*body, green));
    CHECK_FALSE(ro::render::LayerTakesRest(BlueBodySkin(body_key)));
    CHECK_FALSE(ro::render::LayerTakesRetouches(BlueBodySkin(body_key)));
  }
}

TEST_CASE("Calques : le profil 2 teint une image en couleurs vraies au téléversement",
          "[render][wardrobe][layers]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;

  LayerDye blue{.layer = StyleLayer::kHeadgear,
                .key = kBrokenHeadgear,
                .profile = kHueClassProfileNumber,
                .adjusts = {}};
  // La classe des rouges, celle du secteur de 0°, passe au bleu.
  blue.adjusts[0] = RampAdjust{.hue = 240, .saturation = 100, .absolute = true};
  ActorLook look = Novice();
  look.headgears = {0, kBrokenHeadgear, 0};

  const auto first_image = [&wardrobe](const ActorLook& worn) {
    ActorOutfit* outfit = wardrobe.OutfitFor(worn);
    REQUIRE(outfit != nullptr);
    const ActorPiece* piece = PieceOf(*outfit, ActorPart::kHeadgearMiddle);
    REQUIRE(piece != nullptr);
    const ro::render::ComposedActor& composed = outfit->Composed(kAnimationIdle, 0, 0.0F);
    const auto key =
        std::ranges::find(composed.sources, piece->sprite, &ro::render::SpriteImageKey::sprite);
    REQUIRE(key != composed.sources.end());
    CHECK(key->section == 1);
    return std::pair{*key, ro::render::ResolveImage(*key)};
  };

  const auto [plain_key, plain] = first_image(look);
  CHECK(plain_key.tint == nullptr);
  REQUIRE(plain.pixels.size() >= 4);
  CHECK(plain.pixels[0] == 255);
  CHECK(plain.pixels[2] == 0);

  look.style = Registered(wardrobe, StyleSet{.layers = {blue}});
  const auto [tinted_key, tinted] = first_image(look);
  // 🔴 LA TEINTURE ENTRE DANS LA CLÉ DE TEXTURE, sous un numéro de série :
  // l'image rouge déjà téléversée ne se confond pas avec la bleue.
  CHECK(tinted_key.tint != nullptr);
  CHECK(tinted_key.palette_serial != 0);
  CHECK_FALSE(tinted_key == plain_key);
  REQUIRE(tinted.pixels.size() >= 4);
  CHECK(tinted.pixels[2] == 255);
  CHECK(tinted.pixels[0] == 0);

  // ⚠ AU PROFIL 1, UNE IMAGE EN COULEURS VRAIES NE SE TEINT PAS : aucune rampe
  // ne l'atteint.
  ActorLook ramps = Novice();
  ramps.headgears = {0, kBrokenHeadgear, 0};
  ramps.style =
      Registered(wardrobe, StyleSet{.layers = {HalfTurn(StyleLayer::kHeadgear, kBrokenHeadgear)}});
  CHECK(first_image(ramps).first.tint == nullptr);
}

TEST_CASE("Calques : les classes de teinte rangent chaque couleur dans son secteur",
          "[render][layers]") {
  using ro::render::HueClassOf;
  using ro::render::kGreyHueClass;
  CHECK(HueClassOf(255, 0, 0) == 0);
  CHECK(HueClassOf(0, 255, 0) == 4);
  CHECK(HueClassOf(0, 0, 255) == 8);
  // Un jaune tombe à 60°, le secteur deux.
  CHECK(HueClassOf(255, 255, 0) == 2);
  CHECK(HueClassOf(128, 128, 128) == kGreyHueClass);
  CHECK(HueClassOf(0, 0, 0) == kGreyHueClass);

  // Une palette réglée classe par classe : seul le rouge bouge, le transparent
  // jamais.
  ro::formats::Palette base{};
  base.entries[0] = {255, 0, 0, 0};
  base.entries[1] = {255, 0, 0, 0};
  base.entries[2] = {0, 255, 0, 0};
  ro::render::HueClassAdjusts adjusts{};
  adjusts[0] = RampAdjust{.hue = 240, .saturation = 100, .absolute = true};
  const ro::formats::Palette painted = ro::render::ApplyHueClasses(base, adjusts);
  CHECK(painted.entries[0] == base.entries[0]);
  CHECK(painted.entries[1][2] == 255);
  CHECK(painted.entries[2] == base.entries[2]);
}

TEST_CASE("Calques : l'habit du GM et la tête d'orc gardent leurs couleurs",
          "[render][wardrobe][layers]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;

  StyleSet set;
  ro::render::PaletteRecipe turned;
  turned.ramps[0].hue = 180;
  set.body = turned;
  set.layers = {GreenSkin(), HalfTurn(StyleLayer::kHead, HeadDyeKey(kHair, 0)),
                HalfTurn(StyleLayer::kGarment, kWings)};
  const std::uint32_t style = Registered(wardrobe, set);

  ActorLook gm = Dressed();
  gm.gm_outfit = true;
  gm.style = style;
  ActorOutfit* outfit = wardrobe.OutfitFor(gm);
  REQUIRE(outfit != nullptr);
  // 🔴 NI LE CORPS NI LA TÊTE, ni la peau sur l'un ou l'autre.
  for (const ActorPart part : {ActorPart::kBody, ActorPart::kHead}) {
    const ActorPiece* piece = PieceOf(*outfit, part);
    REQUIRE(piece != nullptr);
    CHECK(piece->palette_serial == 0);
    CHECK(ColorOf(*piece, kSkinFirst) == std::array<std::uint8_t, 3>{255, 225, 207});
  }
  // La cape reste celle du métier, et sa teinture avec elle.
  const ActorPiece* garment = PieceOf(*outfit, ActorPart::kGarment);
  REQUIRE(garment != nullptr);
  CHECK(garment->palette_serial != 0);
}

TEST_CASE("Calques : la peau ne se règle ni sur les montures listées ni sur un Doram",
          "[render][wardrobe][layers]") {
  // Les autruches, par leur nom de fichier, et le lion de la Royal Guard en
  // tenue alternative.
  CHECK_FALSE(SkinAdjustable("data\\sprite\\인간족\\몸통\\남\\타조_로드나이트_남"));
  CHECK_FALSE(SkinAdjustable("data\\sprite\\인간족\\몸통\\여\\costume_1\\사자로얄가드_여_1"));
  // Le lion ordinaire, le Peco Peco et un corps à pied restent réglables.
  CHECK(SkinAdjustable("data\\sprite\\인간족\\몸통\\여\\사자로얄가드_여"));
  CHECK(SkinAdjustable("data\\sprite\\인간족\\몸통\\남\\페코페코_기사_남"));
  CHECK(SkinAdjustable("data\\sprite\\인간족\\몸통\\남\\초보자_남"));

  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  ActorLook doram = Novice();
  doram.job = kSummoner;
  doram.style = Registered(wardrobe, StyleSet{.layers = {GreenSkin()}});
  ActorOutfit* outfit = wardrobe.OutfitFor(doram);
  REQUIRE(outfit != nullptr);
  const ActorPiece* body = PieceOf(*outfit, ActorPart::kBody);
  REQUIRE(body != nullptr);
  CHECK(body->palette_serial == 0);
}

TEST_CASE("Calques : sur une autruche, la peau rejoint le reste du corps",
          "[render][wardrobe][layers]") {
  // Le corps monté du Sniper sur l'autruche de la boutique.
  constexpr std::uint32_t kOstrichSniper = 4150;
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  ActorLook ostrich = Novice();
  ostrich.job = kOstrichSniper;

  // 🔴 LA MESURE DE L'ÉDITEUR : pas de peau, et ses pixels passent au reste.
  // Le sprite en a bien une, que les rampes ne prennent pas.
  const ActorWardrobe::BodyRamps mounted = wardrobe.BodyRampsFor(ostrich);
  REQUIRE(mounted.rest.total > 0);
  CHECK(mounted.skin.empty());
  for (const ro::render::PaletteRamp& ramp : mounted.ramps) {
    CHECK(ramp.start + ramp.length <= kSkinFirst);
  }
  CHECK(mounted.rest.covered + kSkinShades * 2 <= mounted.rest.total);

  // 🔴 LE DESSIN : le reste réglé atteint la peau de l'autruche.
  const auto painted_skin = [&wardrobe](ActorLook look) {
    LayerDye more{.layer = StyleLayer::kBodyMore,
                  .key = wardrobe.BodyKeyFor(look),
                  .profile = ro::render::kBodyContinuationProfileNumber,
                  .adjusts = {}};
    for (RampAdjust& adjust : more.rest) {
      adjust = RampAdjust{.hue = 240, .saturation = 100, .absolute = true};
    }
    look.style = Registered(wardrobe, StyleSet{.layers = {more}});
    ActorOutfit* outfit = wardrobe.OutfitFor(look);
    REQUIRE(outfit != nullptr);
    const ActorPiece* body = PieceOf(*outfit, ActorPart::kBody);
    REQUIRE(body != nullptr);
    return ColorOf(*body, kSkinFirst);
  };
  const std::array<std::uint8_t, 3> ridden = painted_skin(ostrich);
  CHECK(ridden[2] > ridden[0]);
}

TEST_CASE("Calques : un profil inconnu laisse la pièce dans ses couleurs",
          "[render][wardrobe][layers]") {
  Fixture fixture;
  ActorWardrobe& wardrobe = *fixture.wardrobe;
  LayerDye unknown = HalfTurn(StyleLayer::kHeadgear, kGoggles);
  unknown.profile = 9;
  ActorLook look = Dressed();
  look.style = Registered(wardrobe, StyleSet{.layers = {unknown}});
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  const ActorPiece* goggles = PieceOf(*outfit, ActorPart::kHeadgearTop);
  REQUIRE(goggles != nullptr);
  CHECK(goggles->palette_serial == 0);
  CHECK(Reddish(ColorOf(*goggles, kRampFirst)));
}

TEST_CASE("Calques : les rampes d'une pièce se mesurent sans la peau",
          "[render][wardrobe][layers]") {
  Fixture fixture;
  const ActorWardrobe::BodyRamps head =
      fixture.wardrobe->LayerRampsFor(Dressed(), ActorPart::kHead);
  REQUIRE_FALSE(head.ramps.empty());
  for (const ro::render::PaletteRamp& ramp : head.ramps) {
    CHECK(ramp.start + ramp.length <= kSkinFirst);
  }
  // Une pièce que l'apparence ne porte pas n'a rien à régler.
  CHECK(fixture.wardrobe->LayerRampsFor(Novice(), ActorPart::kGarment).ramps.empty());
}
