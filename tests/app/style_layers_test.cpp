// LES TEINTURES DE CALQUES QUE LE JOUEUR COMPOSE : leur seul écrivain, ce qui
// est porté contre ce qui est composé, et l'historique.
//
// ⚠ ÉPROUVÉES SUR CE QUI NE SE VOIT QU'EN JEU : une retouche qui reste « à
// valider » pour toujours après l'écho, un glissement qui fait vingt pas
// d'historique, une coupe neuve qui ne retrouve pas la teinture de l'ancienne.

#include "character/style_layers.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/palette_ramps.h"
#include "ro/render/style_set.h"

using moonclient::LayerAt;
using moonclient::LayerEdit;
using moonclient::NeutralDye;
using moonclient::StyleLayers;
using ro::render::LayerDye;
using ro::render::RampAdjust;
using ro::render::StyleLayer;

namespace {

constexpr std::uint8_t kRamps = 1;
constexpr std::uint8_t kClasses = 2;
constexpr LayerAt kHat{.layer = StyleLayer::kHeadgear, .key = 5};
constexpr LayerAt kOldHead{.layer = StyleLayer::kHead, .key = ro::render::HeadDyeKey(12, 3)};
constexpr LayerAt kNewHead{.layer = StyleLayer::kHead, .key = ro::render::HeadDyeKey(20, 3)};

[[nodiscard]] RampAdjust Red() { return RampAdjust{.hue = 0, .saturation = 80, .absolute = true}; }
[[nodiscard]] RampAdjust Blue() {
  return RampAdjust{.hue = 240, .saturation = 80, .absolute = true};
}

[[nodiscard]] LayerDye DyeAt(LayerAt at, std::uint8_t profile, RampAdjust first) {
  LayerDye dye;
  dye.layer = at.layer;
  dye.key = at.key;
  dye.profile = profile;
  dye.adjusts[0] = first;
  return dye;
}

}  // namespace

TEST_CASE("Calques : un calque non touché suit ce qu'on porte", "[app][styles][layers]") {
  StyleLayers layers;
  CHECK(NeutralDye(layers.Current(kHat, kRamps)));
  const std::array<LayerDye, 1> worn{DyeAt(kHat, kRamps, Red())};
  layers.Observe(worn, 1);
  CHECK(layers.Current(kHat, kRamps).adjusts[0].hue == Red().hue);
  CHECK_FALSE(layers.HasAnyEdits());
  // 🔴 UNE TEINTURE D'UN AUTRE PROFIL NE SE REPREND PAS RANG PAR RANG.
  CHECK(NeutralDye(layers.Current(kHat, kClasses)));
}

TEST_CASE("Calques : l'écho d'une validation efface la retouche", "[app][styles][layers]") {
  StyleLayers layers;
  const std::array<LayerEdit, 1> blue{LayerEdit{.rank = 0, .adjust = Blue()}};
  layers.Set(kHat, kRamps, blue);
  REQUIRE(layers.HasEdits(kHat));
  REQUIRE(layers.Pending().size() == 1);

  // Le serveur rend ce qu'on a validé : il n'y a plus rien à valider.
  const std::array<LayerDye, 1> echoed{DyeAt(kHat, kRamps, Blue())};
  layers.Observe(echoed, 2);
  CHECK_FALSE(layers.HasEdits(kHat));
  CHECK(layers.Pending().empty());
  CHECK(layers.Current(kHat, kRamps).adjusts[0].hue == Blue().hue);
}

TEST_CASE("Calques : retirer une teinture portée reste une retouche", "[app][styles][layers]") {
  StyleLayers layers;
  const std::array<LayerDye, 1> worn{DyeAt(kHat, kRamps, Red())};
  layers.Observe(worn, 1);
  const std::array<LayerEdit, 1> neutral{LayerEdit{.rank = 0}};
  layers.Set(kHat, kRamps, neutral);
  // 🔴 UNE TEINTURE NEUTRE SUR UN CALQUE TEINT EST À VALIDER : elle l'efface.
  REQUIRE(layers.Pending().size() == 1);
  CHECK(NeutralDye(layers.Pending().front()));
  // Et le pantin ne la peint pas.
  CHECK(layers.Composed().empty());
}

TEST_CASE("Calques : un trait fait un pas, défaire le rend", "[app][styles][layers]") {
  StyleLayers layers;
  constexpr std::uint32_t kStroke = 7;
  for (std::int16_t hue = 10; hue <= 50; hue += 10) {
    const std::array<LayerEdit, 1> step{
        LayerEdit{.rank = 0, .adjust = RampAdjust{.hue = hue, .absolute = true}}};
    layers.Set(kHat, kRamps, step, kStroke);
  }
  REQUIRE(layers.can_undo());
  REQUIRE(layers.Undo());
  CHECK_FALSE(layers.can_undo());
  CHECK_FALSE(layers.HasEdits(kHat));
  REQUIRE(layers.Redo());
  CHECK(layers.Current(kHat, kRamps).adjusts[0].hue == 50);

  // Un geste sans effet ne fait pas de pas.
  const std::uint32_t revision = layers.revision();
  const std::array<LayerEdit, 1> same{
      LayerEdit{.rank = 0, .adjust = RampAdjust{.hue = 50, .absolute = true}}};
  layers.Set(kHat, kRamps, same);
  CHECK(layers.revision() == revision);
}

TEST_CASE("Calques : le compte des pas croît au pas neuf, et à lui seul", "[app][styles][layers]") {
  StyleLayers layers;
  CHECK(layers.steps_taken() == 0);
  // Un trait prolongé fait un seul pas.
  constexpr std::uint32_t kStroke = 7;
  for (std::int16_t hue = 10; hue <= 30; hue += 10) {
    const std::array<LayerEdit, 1> step{
        LayerEdit{.rank = 0, .adjust = RampAdjust{.hue = hue, .absolute = true}}};
    layers.Set(kHat, kRamps, step, kStroke);
  }
  CHECK(layers.steps_taken() == 1);
  // Défaire et refaire parcourent l'historique, ils ne rangent rien.
  REQUIRE(layers.Undo());
  REQUIRE(layers.Redo());
  CHECK(layers.steps_taken() == 1);
  // 🔴 AU-DELÀ DU PLAFOND, le compte croît encore : c'est lui qui dit qu'un
  // geste a posé un pas quand l'historique plein en oublie un ancien.
  for (std::size_t step = 0; step <= moonclient::kLayerHistoryDepth; ++step) {
    const std::array<LayerEdit, 1> next{
        LayerEdit{.rank = 0,
                  .adjust = RampAdjust{.hue = static_cast<std::int16_t>(step % 2 == 0 ? 60 : 70),
                                       .absolute = true}}};
    layers.Set(kHat, kRamps, next);
  }
  CHECK(layers.steps_taken() == 2 + moonclient::kLayerHistoryDepth);
}

TEST_CASE("Calques : ce que le pantin peint est trié et sans neutre", "[app][styles][layers]") {
  StyleLayers layers;
  const std::array<LayerDye, 2> worn{DyeAt(kHat, kRamps, Red()), DyeAt(kOldHead, kRamps, Red())};
  layers.Observe(worn, 1);
  const std::array<LayerEdit, 1> blue{LayerEdit{.rank = 0, .adjust = Blue()}};
  layers.Set(kHat, kRamps, blue);
  const std::vector<LayerDye> composed = layers.Composed();
  REQUIRE(composed.size() == 2);
  CHECK(composed[0].layer == StyleLayer::kHead);
  CHECK(composed[1].layer == StyleLayer::kHeadgear);
  CHECK(composed[1].adjusts[0].hue == Blue().hue);
  // Avant / après montre ce qu'on porte, toujours rouge.
  CHECK(layers.worn()[0].adjusts[0].hue == Red().hue);

  layers.BackToWorn(kHat);
  CHECK_FALSE(layers.HasAnyEdits());
}

TEST_CASE("Calques : une coupe neuve retrouve la tête teinte d'avant", "[app][styles][layers]") {
  StyleLayers layers;
  CHECK_FALSE(layers.PreviousHead(kNewHead.key).has_value());
  const std::array<LayerDye, 1> worn{DyeAt(kOldHead, kRamps, Red())};
  layers.Observe(worn, 1);
  // Passage chez le styliste : le serveur ne dit plus rien de l'ancienne coupe.
  layers.Observe({}, 2);
  const auto previous = layers.PreviousHead(kNewHead.key);
  REQUIRE(previous.has_value());
  CHECK(previous->key == kOldHead.key);
  // L'ancienne coupe n'est pas « une autre » qu'elle-même.
  CHECK_FALSE(layers.PreviousHead(kOldHead.key).has_value());
}

// ── LE PROFIL MIXTE ─────────────────────────────────────────────────────────

TEST_CASE("Calques : un geste au-delà des pièces règle le reste", "[app][styles][layers][rest]") {
  StyleLayers layers;
  const std::array<LayerEdit, 2> edits{
      LayerEdit{.rank = 0, .adjust = Red()},
      LayerEdit{.rank = moonclient::kRestRankFirst + 12, .adjust = Blue()}};
  layers.Set(kHat, kRamps, edits);
  const LayerDye dye = layers.Current(kHat, kRamps);
  CHECK(dye.adjusts[0].saturation == Red().saturation);
  CHECK(dye.rest[12].hue == Blue().hue);
  // Le reste seul suffit à faire une teinture à valider.
  const std::array<LayerEdit, 1> undo_piece{LayerEdit{.rank = 0}};
  layers.Set(kHat, kRamps, undo_piece);
  CHECK(layers.HasEdits(kHat));
  CHECK_FALSE(NeutralDye(layers.Current(kHat, kRamps)));
}

TEST_CASE("Calques : repartir de zéro remet aussi le reste et les retouches",
          "[app][styles][layers][rest]") {
  // 🔴 LE DÉFAUT VU EN JEU : seules les pièces revenaient à neutre, et « Le
  // reste » gardait ses couleurs sur un calque qui se disait remis à zéro.
  StyleLayers layers;
  const std::array<LayerEdit, 2> edits{
      LayerEdit{.rank = 0, .adjust = Red()},
      LayerEdit{.rank = moonclient::kRestRankFirst + 3, .adjust = Blue()}};
  layers.Set(kHat, kRamps, edits);
  REQUIRE(layers.Retouch(kHat, kRamps, ro::render::PaletteRetouch{.index = 40, .rgb = {1, 2, 3}}));

  layers.Blank(kHat, kRamps);
  CHECK(NeutralDye(layers.Current(kHat, kRamps)));
  CHECK(layers.Current(kHat, kRamps).retouches.empty());
  // Un seul pas : défaire rend tout d'un coup.
  CHECK(layers.Undo());
  const LayerDye back = layers.Current(kHat, kRamps);
  CHECK(back.rest[3].hue == Blue().hue);
  CHECK(back.retouches.size() == 1);
}

TEST_CASE("Calques : les retouches se rangent par index, une par index",
          "[app][styles][layers][rest]") {
  StyleLayers layers;
  using ro::render::PaletteRetouch;
  CHECK(layers.Retouch(kHat, kRamps, PaletteRetouch{.index = 40, .rgb = {1, 2, 3}}));
  CHECK(layers.Retouch(kHat, kRamps, PaletteRetouch{.index = 7, .rgb = {4, 5, 6}}));
  CHECK(layers.Retouch(kHat, kRamps, PaletteRetouch{.index = 40, .rgb = {9, 9, 9}}));
  // L'index zéro et le magenta pur sont refusés.
  CHECK_FALSE(layers.Retouch(kHat, kRamps, PaletteRetouch{.index = 0, .rgb = {1, 1, 1}}));
  CHECK_FALSE(layers.Retouch(kHat, kRamps, PaletteRetouch{.index = 9, .rgb = {255, 0, 255}}));
  const LayerDye dye = layers.Current(kHat, kRamps);
  REQUIRE(dye.retouches.size() == 2);
  CHECK(dye.retouches[0].index == 7);
  CHECK(dye.retouches[1].index == 40);
  CHECK(dye.retouches[1].rgb == std::array<std::uint8_t, 3>{9, 9, 9});

  layers.Unretouch(kHat, kRamps, 7);
  CHECK(layers.Current(kHat, kRamps).retouches.size() == 1);
  CHECK(layers.Undo());
  CHECK(layers.Current(kHat, kRamps).retouches.size() == 2);
}

TEST_CASE("Calques : un calque plein de retouches en refuse une de plus",
          "[app][styles][layers][rest]") {
  StyleLayers layers;
  for (std::size_t index = 1; index <= moonclient::kLayerRetouchesMost; ++index) {
    REQUIRE(layers.Retouch(kHat, kRamps,
                           ro::render::PaletteRetouch{.index = static_cast<std::uint8_t>(index)}));
  }
  CHECK_FALSE(layers.Retouch(kHat, kRamps, ro::render::PaletteRetouch{.index = 200}));
  // Remplacer une retouche existante reste permis.
  CHECK(layers.Retouch(kHat, kRamps, ro::render::PaletteRetouch{.index = 1, .rgb = {7, 7, 7}}));
}
