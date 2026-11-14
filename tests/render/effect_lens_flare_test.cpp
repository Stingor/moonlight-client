// LA GERBE DE REFLETS D'UN IMPACT — EF_HIT2, celui qu'on voit à chaque coup.
//
// 🔴 CET EFFET NE PARAISSAIT PAS DU TOUT, et pour une raison qui était une bonne
// règle mal appliquée : sa texture est « choisie par branche », ce que le crible
// des patrons refuse à raison quand la garde porte sur le MONDE — l'ordre du
// code n'en laisse qu'une, celle de la dernière branche, et la donner à tous
// montre la mauvaise image à tous sauf un. Ici la branche porte sur le RANG
// DANS LA BOUCLE : déterministe, et différente pour chaque exemplaire.

#include <array>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_lens_flare_seed.h"
#include "ro/render/effect_screen_node.h"

using ro::render::EffectLensFlareNode;
using ro::render::EffectLensFlarePattern;
using ro::render::EffectLensFlarePatternsFor;
using ro::render::EffectLensFlarePatternTable;
using ro::render::EffectScreenShape;
using ro::render::SowEffectLensFlareNodes;

namespace {

constexpr std::uint32_t kHit2 = 1;

}  // namespace

TEST_CASE("Reflets : le tour donne le compte, et non un nombre écrit", "[render][lens_flare]") {
  // ⚠ LE CLIENT NE COMPTE PAS SES REFLETS : il accumule un angle par pas de
  // quarante-cinq degrés et s'arrête à trois cent soixante. Huit en sort, mais
  // c'est le QUOTIENT qui le dit — écrire huit ferait mentir le patron le jour
  // où le pas change.
  const std::span<const EffectLensFlarePattern> own_entries = EffectLensFlarePatternsFor(kHit2);
  REQUIRE(own_entries.size() == 1);
  CHECK(own_entries.front().turn_step_degrees == 45.0F);
  CHECK(own_entries.front().full_turn_degrees == 360.0F);

  std::uint32_t generator = 1;
  const std::vector<EffectLensFlareNode> spray =
      SowEffectLensFlareNodes(own_entries.front(), generator);
  CHECK(spray.size() == 8);
}

TEST_CASE("Reflets : l'image ALTERNE d'un rang à l'autre", "[render][lens_flare]") {
  // 🔴 LA PARITÉ DU RANG DÉCIDE : les rangs pairs prennent la première image,
  // les impairs la seconde. Donner la même à tous ferait une gerbe uniforme là
  // où le client en mêle deux.
  const std::span<const EffectLensFlarePattern> own_entries = EffectLensFlarePatternsFor(kHit2);
  REQUIRE(own_entries.size() == 1);
  std::uint32_t generator = 7;
  const std::vector<EffectLensFlareNode> spray =
      SowEffectLensFlareNodes(own_entries.front(), generator);
  REQUIRE(spray.size() == 8);
  for (std::size_t rang = 0; rang < spray.size(); ++rang) {
    INFO("rang " << rang);
    CHECK(spray[rang].texture ==
          (rang % 2 == 0 ? own_entries.front().even_texture : own_entries.front().odd_texture));
  }
  CHECK(own_entries.front().even_texture != own_entries.front().odd_texture);
}

TEST_CASE("Reflets : chacun est tiré à part", "[render][lens_flare]") {
  // Le générateur avance d'un reflet au suivant : repartir de la même graine
  // rejouerait huit fois le même, et la gerbe serait une seule aiguille.
  const std::span<const EffectLensFlarePattern> own_entries = EffectLensFlarePatternsFor(kHit2);
  REQUIRE(own_entries.size() == 1);
  std::uint32_t generator = 3;
  const std::vector<EffectLensFlareNode> spray =
      SowEffectLensFlareNodes(own_entries.front(), generator);
  REQUIRE(spray.size() == 8);

  bool life_differs = false;
  bool width_differs = false;
  for (const EffectLensFlareNode& reflet : spray) {
    life_differs = life_differs || reflet.node.life_ticks != spray.front().node.life_ticks;
    width_differs = width_differs || reflet.node.half_width != spray.front().node.half_width;
  }
  CHECK(life_differs);
  CHECK(width_differs);
}

TEST_CASE("Reflets : le cap et l'angle du quad sont le MÊME nombre", "[render][lens_flare]") {
  // ⚠ Le client écrit la même valeur au cap et à l'angle : le reflet se
  // couche dans la direction où il part. Les séparer le ferait glisser de
  // travers.
  const std::span<const EffectLensFlarePattern> own_entries = EffectLensFlarePatternsFor(kHit2);
  REQUIRE(own_entries.size() == 1);
  std::uint32_t generator = 11;
  for (const EffectLensFlareNode& reflet :
       SowEffectLensFlareNodes(own_entries.front(), generator)) {
    CHECK(reflet.node.shape == EffectScreenShape::kQuad);
    CHECK(reflet.node.heading_degrees == reflet.node.turn_degrees);
    // ⚠ ET LA VIE N'EST JAMAIS NULLE : trois pentes la prennent pour diviseur.
    CHECK(reflet.node.life_ticks > 0);
  }
}

TEST_CASE("Reflets : l'alpha part de ZÉRO et monte", "[render][lens_flare]") {
  // Le reflet s'allume au lieu de surgir : le client pose l'alpha à zéro et lui
  // donne un huitième du plafond par tic — huit tics pour s'allumer.
  const std::span<const EffectLensFlarePattern> own_entries = EffectLensFlarePatternsFor(kHit2);
  REQUIRE(own_entries.size() == 1);
  std::uint32_t generator = 13;
  const std::vector<EffectLensFlareNode> spray =
      SowEffectLensFlareNodes(own_entries.front(), generator);
  REQUIRE(!spray.empty());
  CHECK(spray.front().node.alpha == 0.0F);
  CHECK(spray.front().node.alpha_step > 0.0F);
  CHECK(spray.front().node.alpha_step * 8.0F == spray.front().node.alpha_max);
}

TEST_CASE("Reflets : un quad hérite du BOUCLAGE de son constructeur", "[render][lens_flare]") {
  // 🔴 LE DÉFAUT EST VRAI, pas faux : un nœud neuf boucle. Le supposer faux
  // tuait tout nœud à une seule image au quatrième tic, la cadence par défaut,
  // quelle que soit la vie écrite par son bâtisseur. Un reflet de vingt tics
  // n'en vivait que quatre.
  const ro::render::EffectScreenNode fresh;
  CHECK(fresh.frame_loops);
  CHECK(fresh.ticks_per_frame == 4);
  CHECK(fresh.texture_count == 1);
}

TEST_CASE("Reflets : le reflet S'ÉTIRE, et c'est toute sa puissance", "[render][lens_flare]") {
  // 🔴 CE QUI MANQUAIT QUAND LA GERBE PARAISSAIT FAIBLE. Sa demi-hauteur gagne
  // un pas à chaque tic, et ce pas gagne son accélération : une aiguille de
  // vingt pixels en fait plus de soixante au dixième tic. Sans eux, la gerbe
  // garde la taille de sa naissance — ce qui ne se lit pas comme une gerbe
  // tronquée, mais comme une gerbe FAIBLE.
  //
  // ⚠ CE SONT `half_height_step` ET SON ACCÉLÉRATION, les mêmes champs que ceux
  // des nœuds d'écran.
  const std::span<const EffectLensFlarePattern> own_entries = EffectLensFlarePatternsFor(kHit2);
  REQUIRE(own_entries.size() == 1);
  CHECK(own_entries.front().half_height_step > 0.0F);
  CHECK(own_entries.front().half_height_acceleration > 0.0F);

  std::uint32_t generator = 5;
  const std::vector<EffectLensFlareNode> spray =
      SowEffectLensFlareNodes(own_entries.front(), generator);
  REQUIRE(!spray.empty());
  for (const EffectLensFlareNode& reflet : spray) {
    CHECK(reflet.node.half_height_step == own_entries.front().half_height_step);
    CHECK(reflet.node.half_height_acceleration == own_entries.front().half_height_acceleration);
  }
}

TEST_CASE("Reflets : la table ne porte que ce qui est mesuré", "[render][lens_flare]") {
  for (const EffectLensFlarePattern& pattern : EffectLensFlarePatternTable()) {
    INFO("effet " << pattern.effect_id);
    CHECK(pattern.turn_step_degrees > 0.0F);
    CHECK(pattern.full_turn_degrees > 0.0F);
    CHECK_FALSE(pattern.even_texture.empty());
    CHECK_FALSE(pattern.odd_texture.empty());
    CHECK_FALSE(pattern.client_function.empty());
  }
}

TEST_CASE("Reflets : 183 est la MÊME gerbe, bleutée ; 401 n'en a aucune", "[render][lens_flare]") {
  // 🔴 UN BÂTISSEUR, DEUX GERBES. 183 reprend les huit branches, les tirages et
  // l'étirement du coup ordinaire ; seule sa teinte diffère, le rouge et le
  // vert rabattus à cent vingt. Un nœud d'écran isolé n'en montrait qu'un
  // reflet, immobile et sans vie tirée.
  //
  // ⚠ 401 appelle le même bâtisseur, mais jamais au tic zéro : il ne sème rien.
  CHECK(EffectLensFlarePatternsFor(401).empty());
  constexpr std::array<std::int32_t, 3> kBleute = {120, 120, 255};
  const std::span<const EffectLensFlarePattern> ordinary = EffectLensFlarePatternsFor(kHit2);
  REQUIRE(ordinary.size() == 1);
  CHECK(ordinary.front().color == std::array<std::int32_t, 3>{255, 255, 255});
  for (const std::uint32_t effect_id : {183U}) {
    INFO("effet " << effect_id);
    const std::span<const EffectLensFlarePattern> own_entries =
        EffectLensFlarePatternsFor(effect_id);
    REQUIRE(own_entries.size() == 1);
    CHECK(own_entries.front().color == kBleute);
    std::uint32_t generator = 21;
    const std::vector<EffectLensFlareNode> spray =
        SowEffectLensFlareNodes(own_entries.front(), generator);
    REQUIRE(spray.size() == 8);
    for (const EffectLensFlareNode& reflet : spray) {
      CHECK(reflet.node.color == kBleute);
      CHECK(reflet.node.life_ticks >= 10);
      CHECK(reflet.node.life_ticks <= 29);
    }
  }
}

TEST_CASE("Reflets : le plafond de l'alpha est 254, celui du constructeur",
          "[render][lens_flare]") {
  // Le bâtisseur ne réécrit pas le plafond : la montée en est le huitième,
  // 31,75 par tic et non 31,875.
  for (const EffectLensFlarePattern& pattern : EffectLensFlarePatternTable()) {
    INFO("effet " << pattern.effect_id);
    CHECK(pattern.alpha_max == 254.0F);
    std::uint32_t generator = 2;
    const std::vector<EffectLensFlareNode> spray = SowEffectLensFlareNodes(pattern, generator);
    REQUIRE_FALSE(spray.empty());
    CHECK(spray.front().node.alpha_step == 31.75F);
  }
}
