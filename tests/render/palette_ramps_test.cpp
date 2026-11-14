#include "ro/render/palette_ramps.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/palette.h"

using ro::formats::kPaletteEntryCount;
using ro::formats::Palette;
using ro::render::AdjustToReach;
using ro::render::ApplyRecipe;
using ro::render::DetectRamps;
using ro::render::kRampMinPixels;
using ro::render::MergeServerPalette;
using ro::render::PaletteRamp;
using ro::render::PaletteRecipe;
using ro::render::RampAdjust;
using ro::render::RampColor;
using ro::render::RampValueCeiling;

namespace {

// De quoi franchir le seuil de surface sans y penser à chaque cas.
constexpr int kEnough = kRampMinPixels;

using Usage = std::array<int, kPaletteEntryCount>;

void Paint(Palette& palette, std::size_t index, std::uint8_t red, std::uint8_t green,
           std::uint8_t blue, std::uint8_t fourth = 255) {
  palette.entries[index] = {red, green, blue, fourth};
}

// Un dégradé d'une teinte donnée : `length` index qui descendent du clair au
// foncé, ce qu'est une pièce de costume dans une palette RO.
void PaintRamp(Palette& palette, Usage& usage, std::size_t start, std::size_t length,
               std::uint8_t red, std::uint8_t green, std::uint8_t blue, int pixels = kEnough) {
  for (std::size_t step = 0; step < length; ++step) {
    // La luminosité descend d'un pas par index : c'est la forme que la
    // détection attend, et remonter couperait la rampe.
    const auto shade = [&](std::uint8_t channel) {
      const int scaled = channel * static_cast<int>(length - step) / static_cast<int>(length);
      return static_cast<std::uint8_t>(scaled);
    };
    Paint(palette, start + step, shade(red), shade(green), shade(blue));
    usage[start + step] = pixels;
  }
}

}  // namespace

TEST_CASE("Rampes : une plage noire d'un bout à l'autre se rapatrie du sprite",
          "[render][palette]") {
  Palette sprite;
  Palette server;
  // Le serveur a écrit une pièce : elle reste.
  Paint(server, 1, 200, 30, 30);
  Paint(server, 2, 100, 15, 15);
  Paint(sprite, 1, 10, 10, 10);
  Paint(sprite, 2, 5, 5, 5);
  // Puis une plage définie mais NOIRE d'un bout à l'autre : du remplissage.
  Paint(server, 4, 2, 2, 2);
  Paint(server, 5, 1, 1, 1);
  Paint(sprite, 4, 40, 80, 160);
  Paint(sprite, 5, 20, 40, 80);
  // L'entrée 3 est le séparateur : quatre octets nuls.
  Paint(sprite, 3, 77, 77, 77);

  const Palette merged = MergeServerPalette(sprite, &server);

  CHECK(merged.entries[1] == server.entries[1]);
  CHECK(merged.entries[2] == server.entries[2]);
  // Le séparateur non défini se comble depuis le sprite.
  CHECK(merged.entries[3] == sprite.entries[3]);
  // La plage noire entière cède la place au sprite.
  CHECK(merged.entries[4] == sprite.entries[4]);
  CHECK(merged.entries[5] == sprite.entries[5]);
}

TEST_CASE("Rampes : un dégradé qui FINIT dans le noir est une ombre, pas un trou",
          "[render][palette]") {
  Palette sprite;
  Palette server;
  // Une pièce qui descend jusqu'au noir : légitime, et le sprite ne doit pas
  // la reprendre — sinon on repeindrait les contours du personnage.
  Paint(server, 1, 180, 90, 20);
  Paint(server, 2, 90, 45, 10);
  Paint(server, 3, 2, 1, 0);
  Paint(sprite, 1, 9, 9, 9);
  Paint(sprite, 2, 9, 9, 9);
  Paint(sprite, 3, 9, 9, 9);

  const Palette merged = MergeServerPalette(sprite, &server);

  CHECK(merged.entries[1] == server.entries[1]);
  CHECK(merged.entries[3] == server.entries[3]);
}

TEST_CASE("Rampes : sans palette de serveur, celle du sprite fait foi", "[render][palette]") {
  Palette sprite;
  Paint(sprite, 7, 12, 34, 56);
  CHECK(MergeServerPalette(sprite, nullptr).entries[7] == sprite.entries[7]);
}

TEST_CASE("Rampes : un changement de teinte coupe, un index jamais peint aussi",
          "[render][palette]") {
  Palette palette;
  Usage usage{};
  PaintRamp(palette, usage, 1, 4, 240, 40, 40);  // rouge
  PaintRamp(palette, usage, 5, 4, 40, 40, 240);  // bleu, collé au rouge
  // L'index 9 n'est jamais peint : il coupe de lui-même.
  PaintRamp(palette, usage, 10, 4, 40, 240, 40);  // vert

  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);

  REQUIRE(ramps.size() == 3);
  // Le classement est celui du score ; les plages, elles, sont sûres.
  std::vector<int> starts;
  for (const PaletteRamp& ramp : ramps) {
    starts.push_back(ramp.start);
    CHECK(ramp.length == 4);
  }
  std::sort(starts.begin(), starts.end());
  CHECK(starts == std::vector<int>{1, 5, 10});
}

TEST_CASE("Rampes : une remontée de luminosité coupe", "[render][palette]") {
  Palette palette;
  Usage usage{};
  // Deux dégradés de la MÊME teinte, collés : seule la remontée les sépare.
  PaintRamp(palette, usage, 1, 3, 240, 40, 40);
  PaintRamp(palette, usage, 4, 3, 240, 40, 40);

  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);

  REQUIRE(ramps.size() == 2);
}

TEST_CASE("Rampes : une plage trop petite ne vaut pas un curseur", "[render][palette]") {
  Palette palette;
  Usage usage{};
  PaintRamp(palette, usage, 1, 4, 240, 40, 40, kRampMinPixels / 8);
  CHECK(DetectRamps(palette, usage).empty());
}

TEST_CASE("Rampes : une pièce petite mais FRANCHE passe devant un grand aplat terne",
          "[render][palette]") {
  Palette palette;
  Usage usage{};
  // Un gris deux fois plus étendu, et une rune rouge vive.
  PaintRamp(palette, usage, 1, 4, 130, 130, 130, kEnough * 2);
  PaintRamp(palette, usage, 10, 4, 255, 0, 0, kEnough * 2);

  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);

  REQUIRE(ramps.size() == 2);
  // 🔴 À SURFACE ÉGALE, LA FRANCHISE TRANCHE : c'est ce qui a été mesuré sur
  // 265 des 421 corps, dont une couleur saturée tombait au rang neuf.
  CHECK(ramps[0].start == 10);
  CHECK(ramps[0].hue.has_value());
  CHECK_FALSE(ramps[1].hue.has_value());
}

TEST_CASE("Rampes : jamais plus que le plafond du protocole", "[render][palette]") {
  Palette palette;
  Usage usage{};
  // Douze pièces d'une teinte chacune, séparées par un index non peint.
  for (std::size_t rank = 0; rank < 12; ++rank) {
    const auto tone = static_cast<std::uint8_t>(20 + rank * 20);
    PaintRamp(palette, usage, 1 + rank * 5, 4, tone, 255, 60);
  }
  CHECK(DetectRamps(palette, usage).size() == ro::render::kMaxRamps);
}

TEST_CASE("Rampes : la suite du corps rend les rampes 9 à 12, sans toucher aux huit premières",
          "[render][palette]") {
  constexpr std::size_t kPieces = 14;
  Palette palette;
  Usage usage{};
  // Des pièces de plus en plus peintes : la plus peinte d'abord au classement.
  for (std::size_t rank = 0; rank < kPieces; ++rank) {
    const auto tone = static_cast<std::uint8_t>(20 + rank * 15);
    PaintRamp(palette, usage, 1 + rank * 5, 4, tone, 255, 60,
              kEnough * static_cast<int>(kPieces - rank));
  }
  const std::vector<PaletteRamp> first = DetectRamps(palette, usage);
  const std::vector<PaletteRamp> more = ro::render::DetectBodyContinuation(palette, usage);
  REQUIRE(first.size() == ro::render::kMaxRamps);
  REQUIRE(more.size() == ro::render::kBodyContinuationRamps);
  // Aucune rampe de la suite n'est l'une des huit premières.
  for (const PaletteRamp& ramp : more) {
    CHECK(std::ranges::none_of(
        first, [&ramp](const PaletteRamp& kept) { return kept.start == ramp.start; }));
  }
  // Les plus visibles d'abord, ici comme là : la suite est moins peinte.
  CHECK(more.front().pixels <= first.back().pixels);
}

TEST_CASE("Rampes : une palette de huit pièces n'a pas de suite", "[render][palette]") {
  Palette palette;
  Usage usage{};
  for (std::size_t rank = 0; rank < ro::render::kMaxRamps; ++rank) {
    PaintRamp(palette, usage, 1 + rank * 5, 4, static_cast<std::uint8_t>(30 + rank * 20), 255, 60);
  }
  CHECK(ro::render::DetectBodyContinuation(palette, usage).empty());
}

TEST_CASE("Rampes : la pastille évite les index noirs", "[render][palette]") {
  Palette palette;
  Usage usage{};
  // Une pièce qui monte jusqu'au blanc mais dont le MILIEU est noir pur : la
  // règle naïve y prenait sa couleur, et la pastille s'affichait noire.
  Paint(palette, 1, 255, 255, 255);
  Paint(palette, 2, 0, 0, 0);
  Paint(palette, 3, 0, 0, 0);
  Paint(palette, 4, 0, 0, 0);
  for (std::size_t index = 1; index <= 4; ++index) {
    usage[index] = kEnough;
  }

  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);

  REQUIRE(ramps.size() == 1);
  CHECK(ramps[0].ref == 1);
}

TEST_CASE("Rampes : un réglage relatif pivote la teinte et garde le dégradé", "[render][palette]") {
  Palette palette;
  Usage usage{};
  PaintRamp(palette, usage, 1, 4, 240, 0, 0);  // rouge pur
  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);
  REQUIRE(ramps.size() == 1);

  PaletteRecipe recipe;
  recipe.ramps[0] = RampAdjust{.hue = 120};  // rouge → vert
  const Palette painted = ApplyRecipe(palette, ramps, recipe);

  // La composante verte prend la place de la rouge, index par index : c'est le
  // dégradé qui est préservé, pas seulement la teinte du premier.
  for (std::size_t index = 1; index <= 4; ++index) {
    CHECK(painted.entries[index][1] == palette.entries[index][0]);
    CHECK(painted.entries[index][0] == 0);
    // ⚠ L'OCTET D'ALPHA RESTE CELUI DU FICHIER.
    CHECK(painted.entries[index][3] == palette.entries[index][3]);
  }
}

TEST_CASE("Rampes : une teinte pivotée vers le BAS ne sort pas du cercle", "[render][palette]") {
  Palette palette;
  Usage usage{};
  PaintRamp(palette, usage, 1, 4, 0, 240, 0);  // vert
  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);
  REQUIRE(ramps.size() == 1);

  PaletteRecipe up;
  up.ramps[0] = RampAdjust{.hue = 240};
  PaletteRecipe down;
  down.ramps[0] = RampAdjust{.hue = -120};

  // 🔴 LE MODULO DU C++ GARDE LE SIGNE DU DIVIDENDE : sans le repli, une teinte
  // descendue sous zéro atterrirait dans un tout autre secteur que chez la
  // référence, dont le modulo rend toujours du positif.
  CHECK(ApplyRecipe(palette, ramps, down).entries[1] == ApplyRecipe(palette, ramps, up).entries[1]);
}

TEST_CASE("Rampes : le mode absolu impose la teinte, la luminosité reste au dessin",
          "[render][palette]") {
  Palette palette;
  Usage usage{};
  // Une pièce BEIGE, presque terne : en relatif, doubler sa saturation ne la
  // rend pas vive. C'est la raison d'être du second mode.
  PaintRamp(palette, usage, 1, 4, 200, 180, 150);
  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);
  REQUIRE(ramps.size() == 1);

  PaletteRecipe recipe;
  recipe.ramps[0] = RampAdjust{.hue = 240, .saturation = 100, .absolute = true};
  const Palette painted = ApplyRecipe(palette, ramps, recipe);

  for (std::size_t index = 1; index <= 4; ++index) {
    // Bleu pur : saturation pleine, teinte imposée, rouge et vert éteints.
    CHECK(painted.entries[index][0] == 0);
    CHECK(painted.entries[index][1] == 0);
    // La luminosité, elle, reste celle du dégradé — le plus clair en tête.
    CHECK(painted.entries[index][2] == palette.entries[index][0]);
  }
  CHECK(painted.entries[1][2] > painted.entries[4][2]);
}

TEST_CASE("Rampes : un réglage neutre ne touche à rien", "[render][palette]") {
  Palette palette;
  Usage usage{};
  PaintRamp(palette, usage, 1, 4, 240, 40, 40);
  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);
  REQUIRE(ramps.size() == 1);

  const Palette painted = ApplyRecipe(palette, ramps, PaletteRecipe{});
  CHECK(painted.entries == palette.entries);
}

// ── CE QUE L'ÉDITEUR MANIPULE ───────────────────────────────────────────────

TEST_CASE("Rampes : la pastille est la couleur de l'index représentatif", "[render][palette]") {
  Palette palette;
  Usage usage{};
  PaintRamp(palette, usage, 1, 4, 240, 40, 40);
  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);
  REQUIRE(ramps.size() == 1);

  const std::array<std::uint8_t, 3> color = RampColor(palette, ramps[0]);
  const auto& entry = palette.entries[ramps[0].ref];
  CHECK(color[0] == entry[0]);
  CHECK(color[1] == entry[1]);
  CHECK(color[2] == entry[2]);
}

TEST_CASE("Rampes : une pièce peinte sombre ne peut pas être éclaircie", "[render][palette]") {
  Palette palette;
  Usage usage{};
  // Une zone d'ombre : l'artiste l'a peinte foncée, et le réglage garde le
  // modelé au lieu de l'aplatir.
  PaintRamp(palette, usage, 1, 4, 80, 20, 20);
  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);
  REQUIRE(ramps.size() == 1);

  const int ceiling = RampValueCeiling(palette, ramps[0]);
  // 🔴 LE DOUBLE DE SA CLARTÉ, et c'est exactement ce que rend « +100 » de
  // luminosité : les deux bornes doivent bouger ensemble, sinon la fenêtre
  // annonce une clarté que le réglage n'atteint pas.
  const int brightest = palette.entries[ramps[0].ref][0];
  CHECK(ceiling == brightest * 2);

  PaletteRecipe recipe;
  recipe.ramps[0] = RampAdjust{.hue = 0, .saturation = 0, .value = 100};
  const Palette painted = ApplyRecipe(palette, ramps, recipe);
  CHECK(painted.entries[ramps[0].ref][0] == ceiling);
}

TEST_CASE("Rampes : une pièce claire plafonne à blanc, jamais au-delà", "[render][palette]") {
  Palette palette;
  Usage usage{};
  // ⚠ UN SEUL INDEX, pour que la pastille SOIT le clair : sur un dégradé, elle
  // tombe au milieu, et c'est de LUI que le plafond se déduit — un dégradé qui
  // part de 250 a son milieu bien plus bas.
  PaintRamp(palette, usage, 1, 1, 250, 250, 250);
  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);
  REQUIRE(ramps.size() == 1);
  CHECK(RampValueCeiling(palette, ramps[0]) == 255);
}

TEST_CASE("Rampes : désigner une couleur rend le réglage qui y mène", "[render][palette]") {
  Palette palette;
  Usage usage{};
  // Une pièce BEIGE, terne : c'est justement le cas qu'un réglage relatif ne
  // saurait pas satisfaire.
  PaintRamp(palette, usage, 1, 4, 200, 180, 150);
  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);
  REQUIRE(ramps.size() == 1);

  // Un bleu franc, à la même clarté que la pièce.
  const std::array<std::uint8_t, 3> target = {0, 0, 200};
  const RampAdjust adjust = AdjustToReach(palette, ramps[0], target);

  // 🔴 TOUJOURS ABSOLU : en relatif, une pièce terne resterait terne et le
  // joueur obtiendrait un bleu délavé là où il a désigné un bleu franc.
  CHECK(adjust.absolute);
  CHECK(adjust.hue == 240);
  CHECK(adjust.saturation == 100);

  // Et le réglage AMÈNE bien la pastille à la couleur demandée.
  PaletteRecipe recipe;
  recipe.ramps[0] = adjust;
  const Palette painted = ApplyRecipe(palette, ramps, recipe);
  const std::array<std::uint8_t, 3> reached = RampColor(painted, ramps[0]);
  CHECK(reached[0] == target[0]);
  CHECK(reached[1] == target[1]);
  CHECK(reached[2] == target[2]);
}

TEST_CASE("Rampes : deux clics sur la même couleur rendent le même réglage", "[render][palette]") {
  Palette palette;
  Usage usage{};
  PaintRamp(palette, usage, 1, 6, 180, 90, 60);
  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);
  REQUIRE(ramps.size() == 1);

  // 🔴 LE RÉGLAGE SE CALCULE DEPUIS LA BASE, jamais depuis le résultat courant :
  // partir du peint ferait s'additionner les réglages à chaque clic, et la
  // couleur dériverait sous la main du joueur.
  const std::array<std::uint8_t, 3> target = {30, 200, 90};
  const RampAdjust first = AdjustToReach(palette, ramps[0], target);
  PaletteRecipe recipe;
  recipe.ramps[0] = first;
  const Palette painted = ApplyRecipe(palette, ramps, recipe);
  const RampAdjust again = AdjustToReach(palette, ramps[0], RampColor(painted, ramps[0]));

  CHECK(again.hue == first.hue);
  CHECK(again.saturation == first.saturation);
  CHECK(again.value == first.value);
  CHECK(again.absolute == first.absolute);
}

TEST_CASE("Rampes : une base noire ne rend pas un réglage infini", "[render][palette]") {
  Palette palette;
  Usage usage{};
  // Un index représentatif noir n'arrive que sur une rampe entièrement noire —
  // la pastille s'y rabat sur le milieu nu. La division par sa clarté n'a
  // alors pas de sens, et la garde vaut mieux qu'une valeur inventée.
  for (std::size_t index = 1; index <= 4; ++index) {
    Paint(palette, index, 0, 0, 0);
    usage[index] = kEnough;
  }
  const std::vector<PaletteRamp> ramps = DetectRamps(palette, usage);
  REQUIRE(ramps.size() == 1);

  const RampAdjust adjust = AdjustToReach(palette, ramps[0], {255, 255, 255});
  CHECK(adjust.value == 0);
}

// ── LE PROFIL 1 : LES CALQUES ───────────────────────────────────────────────

namespace {

using ro::render::DetectLayerRamps;
using ro::render::LayerProfile;
using ro::render::LayerRampProfile;
using ro::render::RampLayer;
using ro::render::SkinRace;

constexpr std::uint8_t kProfile = ro::render::kLayerProfileNumber;

LayerRampProfile ProfileFor(RampLayer layer) {
  return *LayerProfile(kProfile, layer, SkinRace::kHuman);
}

// Les coupes du profil 1 au seuil du corps et sans exclusion.
const LayerRampProfile kBodyLike{.min_pixels = kRampMinPixels, .excluded = {}};

// Deux index collés, l'un après l'autre, chacun assez peint pour une coiffe.
std::vector<PaletteRamp> TwoEntries(std::array<std::uint8_t, 3> first,
                                    std::array<std::uint8_t, 3> second) {
  Palette palette;
  Usage usage{};
  Paint(palette, 1, first[0], first[1], first[2]);
  Paint(palette, 2, second[0], second[1], second[2]);
  usage[1] = ro::render::kAccessoryRampMinPixels;
  usage[2] = ro::render::kAccessoryRampMinPixels;
  return DetectLayerRamps(palette, usage, ProfileFor(RampLayer::kHeadgear));
}

}  // namespace

TEST_CASE("Profil 1 : un numéro de profil inconnu ne découpe rien", "[render][palette]") {
  CHECK_FALSE(LayerProfile(0, RampLayer::kHead, SkinRace::kHuman).has_value());
  CHECK_FALSE(LayerProfile(kProfile + 1, RampLayer::kGarment, SkinRace::kDoram).has_value());
  CHECK(LayerProfile(kProfile, RampLayer::kHeadgear, SkinRace::kHuman).has_value());
}

TEST_CASE("Profil 1 : une coiffe et une cape se règlent dès 50 pixels, une tête à 200",
          "[render][palette]") {
  Palette palette;
  Usage usage{};
  constexpr int kSmallPiece = 100;
  PaintRamp(palette, usage, 1, 4, 240, 40, 40, kSmallPiece / 4);

  CHECK(DetectLayerRamps(palette, usage, ProfileFor(RampLayer::kHeadgear)).size() == 1);
  CHECK(DetectLayerRamps(palette, usage, ProfileFor(RampLayer::kGarment)).size() == 1);
  CHECK(DetectLayerRamps(palette, usage, ProfileFor(RampLayer::kHead)).empty());
  CHECK(DetectRamps(palette, usage).empty());
}

TEST_CASE("Profil 1 : la peau coupe la rampe d'une tête et n'y entre jamais", "[render][palette]") {
  Palette palette;
  Usage usage{};
  // Un seul dégradé qui traverse tout l'ensemble de peau humain.
  constexpr std::size_t kFirst = 120;
  constexpr std::size_t kLength = 24;
  PaintRamp(palette, usage, kFirst, kLength, 250, 200, 160);

  const std::vector<PaletteRamp> head =
      DetectLayerRamps(palette, usage, ProfileFor(RampLayer::kHead));
  REQUIRE_FALSE(head.empty());
  for (const PaletteRamp& ramp : head) {
    for (const std::uint8_t skin : ro::render::SkinIndices(SkinRace::kHuman)) {
      CHECK((skin < ramp.start || skin >= ramp.start + ramp.length));
    }
  }
  // Une coiffe n'a pas de peau : le dégradé reste entier.
  const std::vector<PaletteRamp> hat =
      DetectLayerRamps(palette, usage, ProfileFor(RampLayer::kHeadgear));
  REQUIRE(hat.size() == 1);
  CHECK(hat[0].start == kFirst);
  CHECK(hat[0].length == kLength);
}

TEST_CASE("Profil 1 : les ensembles de peau sont figés par race", "[render][palette]") {
  // Ils décident de ce que les rangs d'une teinture de tête désignent : les
  // changer change le sens des teintures déjà partagées.
  const std::span<const std::uint8_t> human = ro::render::SkinIndices(SkinRace::kHuman);
  CHECK(std::vector<int>(human.begin(), human.end()) ==
        std::vector<int>{129, 130, 131, 132, 133, 134, 135});
  CHECK(ro::render::SkinIndices(SkinRace::kDoram).empty());
  // Sans peau, une tête Doram ne perd aucun index au profit du calque Peau.
  CHECK(ProfileFor(RampLayer::kHead).excluded.size() == human.size());
  CHECK(LayerProfile(kProfile, RampLayer::kHead, SkinRace::kDoram)->excluded.empty());
}

TEST_CASE("Profil 1 : un écart de teinte de 40 degrés pile ne coupe pas, 41 si",
          "[render][palette]") {
  // Rouge pur, puis 60 × 170 / 255 = 40 degrés exactement, puis 41,2.
  CHECK(TwoEntries({255, 0, 0}, {255, 170, 0}).size() == 1);
  CHECK(TwoEntries({255, 0, 0}, {255, 175, 0}).size() == 2);
}

TEST_CASE("Profil 1 : une saturation de 12 % pile porte une teinte", "[render][palette]") {
  // 24 / 200 = 12 % : la rampe prend la teinte rouge, et le jaune qui suit la
  // coupe. À 23 / 200, c'est un gris, et le jaune ne coupe rien.
  CHECK(TwoEntries({200, 176, 176}, {190, 190, 100}).size() == 2);
  CHECK(TwoEntries({200, 177, 177}, {190, 190, 100}).size() == 1);
}

TEST_CASE("Profil 1 : une remontée de 20 niveaux ne coupe pas, 21 si", "[render][palette]") {
  // 8 % de 255 valent 20,4 niveaux.
  CHECK(TwoEntries({100, 100, 100}, {120, 120, 120}).size() == 1);
  CHECK(TwoEntries({100, 100, 100}, {121, 121, 121}).size() == 2);
}

TEST_CASE("Profil 1 : la teinte d'une pièce s'arrondit au degré, la moitié vers le haut",
          "[render][palette]") {
  // 60 × (81 − 80) / 120 = 0,5 degré.
  const std::vector<PaletteRamp> ramps = TwoEntries({200, 81, 80}, {150, 61, 60});
  REQUIRE(ramps.size() == 1);
  CHECK(ramps[0].hue == 1);
}

TEST_CASE("Profil 1 : hors des égalités pile, ses coupes rendent celles du corps",
          "[render][palette]") {
  Palette palette;
  Usage usage{};
  PaintRamp(palette, usage, 1, 6, 240, 40, 40);
  PaintRamp(palette, usage, 7, 5, 40, 40, 240);
  PaintRamp(palette, usage, 12, 4, 240, 40, 40);
  PaintRamp(palette, usage, 20, 8, 130, 130, 130, kEnough * 2);
  PaintRamp(palette, usage, 30, 5, 200, 180, 150);

  const std::vector<PaletteRamp> body = DetectRamps(palette, usage);
  const std::vector<PaletteRamp> layer = DetectLayerRamps(palette, usage, kBodyLike);
  REQUIRE(body.size() == layer.size());
  for (std::size_t rank = 0; rank < body.size(); ++rank) {
    CHECK(body[rank].start == layer[rank].start);
    CHECK(body[rank].length == layer[rank].length);
    CHECK(body[rank].pixels == layer[rank].pixels);
    CHECK(body[rank].hue == layer[rank].hue);
    CHECK(body[rank].ref == layer[rank].ref);
  }
}

TEST_CASE("Rampes : la pipette rend la pièce d'un index, ou aucune", "[render][palette]") {
  // 🔴 « AUCUNE » EST UNE VRAIE RÉPONSE : au-delà des huit plus grands
  // dégradés, le reste du corps n'a pas de commande — c'est exactement la part
  // qu'annonce « Réglable : N% ». Rendre la rampe la plus proche ferait
  // désigner une pièce que le clic ne changerait pas.
  const std::vector<PaletteRamp> ramps = {
      PaletteRamp{.start = 10, .length = 5},
      PaletteRamp{.start = 40, .length = 3},
  };
  CHECK(ro::render::RampAt(ramps, 10) == std::size_t{0});
  CHECK(ro::render::RampAt(ramps, 14) == std::size_t{0});
  CHECK(ro::render::RampAt(ramps, 40) == std::size_t{1});
  CHECK(ro::render::RampAt(ramps, 42) == std::size_t{1});
  // Juste après la fin d'une rampe, et dans l'intervalle entre les deux.
  CHECK_FALSE(ro::render::RampAt(ramps, 15).has_value());
  CHECK_FALSE(ro::render::RampAt(ramps, 39).has_value());
  CHECK_FALSE(ro::render::RampAt(ramps, 43).has_value());
  // L'index transparent n'appartient à rien non plus.
  CHECK_FALSE(ro::render::RampAt(ramps, 0).has_value());
  CHECK_FALSE(ro::render::RampAt({}, 12).has_value());
}
