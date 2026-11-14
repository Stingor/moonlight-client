#include "ro/render/effect_screen_burst.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_screen_burst_seed.h"
#include "ro/render/effect_screen_node.h"

using Catch::Approx;
using ro::render::AdvanceEffectScreenNode;
using ro::render::ComposeScreenRingQuads;
using ro::render::ComposeScreenWedgeQuad;
using ro::render::EffectScreenBurstPattern;
using ro::render::EffectScreenBurstPatternsFor;
using ro::render::EffectScreenBurstPatternTable;
using ro::render::EffectScreenFlag;
using ro::render::EffectScreenNode;
using ro::render::EffectScreenQuad;
using ro::render::EffectScreenShape;
using ro::render::kScreenRingTextureStep;
using ro::render::SowEffectScreenBurstNodes;

namespace {

[[nodiscard]] EffectScreenNode TestRing(float step, float radius, float width) {
  EffectScreenNode node;
  node.shape = EffectScreenShape::kRing;
  node.angle_degrees = step;
  node.outer_radius = radius;
  node.band_width = width;
  node.life_ticks = 40;
  return node;
}

[[nodiscard]] EffectScreenNode TestBurst(float ouverture, float radius) {
  EffectScreenNode node;
  node.shape = EffectScreenShape::kWedge;
  node.angle_degrees = ouverture;
  node.half_height = radius;
  node.turn_degrees = 0.0F;
  node.life_ticks = 40;
  return node;
}

// Le rayon d'un coin par rapport au centre.
[[nodiscard]] float RadiusOf(glm::vec2 corner, glm::vec2 centre) {
  const glm::vec2 offset = corner - centre;
  return std::sqrt(offset.x * offset.x + offset.y * offset.y);
}

}  // namespace

TEST_CASE("la couronne au pas du constructeur rend DIX bandes", "[render][screen_burst]") {
  // ⚠ TRENTE-SIX DEGRÉS, LE DÉFAUT DU CONSTRUCTEUR, que trois
  // bâtisseurs sur quatre ne posent jamais.
  const std::vector<EffectScreenQuad> bands =
      ComposeScreenRingQuads(TestRing(ro::render::kEffectScreenDefaultStepDegrees, 100.0F, 100.0F));
  REQUIRE(bands.size() == 10);
}

TEST_CASE("la couronne au pas de dix en rend TRENTE-SIX", "[render][screen_burst]") {
  // Celle de Stone Curse, la seule qui pose son pas — et la seule qui se
  // referme, puisque trois cent soixante est multiple de dix.
  const std::vector<EffectScreenQuad> bands = ComposeScreenRingQuads(TestRing(10.0F, 50.0F, 10.0F));
  REQUIRE(bands.size() == 36);
}

TEST_CASE("la couronne au pas du constructeur SE REFERME", "[render][screen_burst]") {
  // 🔴 TRENTE-SIX, ET NON TRENTE-CINQ : dix bandes de trente-six degrés font le
  // tour exact, le dernier bord retombant sur le premier. À trente-cinq, dix
  // bandes laissaient un trou de dix degrés — le disque de Bash montrait un
  // secteur vide, que le client natif n'a pas. Le balayage lui-même s'arrête à
  // `360 − pas`, chez le client comme ici ; c'est le pas qui ferme, ou non.
  const EffectScreenNode node =
      TestRing(ro::render::kEffectScreenDefaultStepDegrees, 100.0F, 100.0F);
  const std::vector<EffectScreenQuad> bands = ComposeScreenRingQuads(node);
  REQUIRE(bands.size() == 10);
  const glm::vec2 centre{0.0F, 0.0F};
  const glm::vec2 first = bands.front().corners[3];
  const glm::vec2 last = bands.back().corners[2];
  REQUIRE(RadiusOf(first, centre) == Approx(RadiusOf(last, centre)).epsilon(1e-4));
  // Les deux bords coïncident : aucun trou.
  CHECK(std::abs(first.x - last.x) + std::abs(first.y - last.y) < 0.01F);

  // Et à trente-cinq, le trou : c'est ce que le défaut faux montrait.
  const std::vector<EffectScreenQuad> trouees =
      ComposeScreenRingQuads(TestRing(35.0F, 100.0F, 100.0F));
  REQUIRE(trouees.size() == 10);
  const glm::vec2 open_edge = trouees.back().corners[2];
  CHECK(std::abs(first.x - open_edge.x) + std::abs(first.y - open_edge.y) > 1.0F);
}

TEST_CASE("un pas nul ne rend AUCUNE bande", "[render][screen_burst]") {
  // ⚠ LE CLIENT, LUI, TOURNERAIT SANS FIN : son angle cumulé n'avancerait
  // jamais. Le défaut de trente-six fait que le cas ne se présente pas, mais
  // un patron malformé ne doit pas figer le rendu.
  REQUIRE(ComposeScreenRingQuads(TestRing(0.0F, 100.0F, 100.0F)).empty());
  REQUIRE(ComposeScreenRingQuads(TestRing(-5.0F, 100.0F, 100.0F)).empty());
}

TEST_CASE("une largeur egale au rayon fait un DISQUE PLEIN", "[render][screen_burst]") {
  const glm::vec2 centre{0.0F, 0.0F};
  const std::vector<EffectScreenQuad> bands =
      ComposeScreenRingQuads(TestRing(35.0F, 100.0F, 100.0F));
  // Les deux premiers coins sont le bord INTÉRIEUR : ils tombent au centre.
  REQUIRE(RadiusOf(bands[0].corners[0], centre) == Approx(0.0F).margin(1e-4));
  REQUIRE(RadiusOf(bands[0].corners[1], centre) == Approx(0.0F).margin(1e-4));
  // Les deux derniers sont le bord extérieur.
  REQUIRE(RadiusOf(bands[0].corners[2], centre) == Approx(100.0F).epsilon(1e-4));
}

TEST_CASE("une largeur plus petite laisse un TROU au centre", "[render][screen_burst]") {
  const glm::vec2 centre{0.0F, 0.0F};
  const std::vector<EffectScreenQuad> bands = ComposeScreenRingQuads(TestRing(10.0F, 50.0F, 10.0F));
  // ⚠ LA LARGEUR SE RETRANCHE : c'est une ÉPAISSEUR, pas un second rayon. La
  // lire comme un rayon intérieur donnerait une bande de quarante au lieu de
  // dix.
  REQUIRE(RadiusOf(bands[0].corners[0], centre) == Approx(40.0F).epsilon(1e-4));
  REQUIRE(RadiusOf(bands[0].corners[3], centre) == Approx(50.0F).epsilon(1e-4));
}

TEST_CASE("l'image de la couronne se repete tous les QUATRE segments", "[render][screen_burst]") {
  const std::vector<EffectScreenQuad> bands =
      ComposeScreenRingQuads(TestRing(35.0F, 100.0F, 100.0F));
  for (std::size_t rang = 0; rang < bands.size(); ++rang) {
    const float expected = kScreenRingTextureStep * static_cast<float>(rang % 4);
    REQUIRE(bands[rang].texture_uv[0].x == Approx(expected));
  }
  // ⚠ ET L'ORDONNÉE VAUT UN AU BORD INTÉRIEUR, zéro à l'extérieur : l'image se
  // lit du centre vers le dehors.
  REQUIRE(bands[0].texture_uv[0].y == Approx(1.0F));
  REQUIRE(bands[0].texture_uv[3].y == Approx(0.0F));
}

TEST_CASE("l'eclat est un TRIANGLE, son dernier coin reprend la pointe", "[render][screen_burst]") {
  const EffectScreenQuad shard = ComposeScreenWedgeQuad(TestBurst(2.0F, 40.0F));
  REQUIRE(shard.corners[0].x == Approx(shard.corners[3].x));
  REQUIRE(shard.corners[0].y == Approx(shard.corners[3].y));
  REQUIRE(shard.texture_uv[0].x == Approx(shard.texture_uv[3].x));
}

TEST_CASE("l'eclat pointe vers la DROITE a l'angle zero", "[render][screen_burst]") {
  const EffectScreenQuad shard = ComposeScreenWedgeQuad(TestBurst(2.0F, 40.0F));
  // ⚠ COSINUS EN ABSCISSE, SINUS EN ORDONNÉE, comme un point de la couronne :
  // lire les deux tables sous leur nom le ferait pointer vers le haut.
  REQUIRE(shard.corners[1].x > shard.corners[0].x);
  REQUIRE(shard.corners[1].x == Approx(40.0F).epsilon(1e-2));
  REQUIRE(std::abs(shard.corners[1].y) < 2.0F);
}

TEST_CASE("la pointe de l'eclat est au centre et sa base au rayon", "[render][screen_burst]") {
  const glm::vec2 centre{0.0F, 0.0F};
  const EffectScreenQuad shard = ComposeScreenWedgeQuad(TestBurst(5.0F, 30.0F));
  REQUIRE(RadiusOf(shard.corners[0], centre) == Approx(0.0F).margin(1e-4));
  REQUIRE(RadiusOf(shard.corners[1], centre) == Approx(30.0F).epsilon(1e-4));
  REQUIRE(RadiusOf(shard.corners[2], centre) == Approx(30.0F).epsilon(1e-4));
}

TEST_CASE("le centre suit le porteur, sauf en absolu", "[render][screen_burst]") {
  EffectScreenNode node = TestBurst(2.0F, 10.0F);
  node.screen_offset = {0.0F, -40.0F};
  const EffectScreenQuad follow = ComposeScreenWedgeQuad(node, glm::vec2{300.0F, 200.0F});
  REQUIRE(follow.corners[0].x == Approx(300.0F));
  // ⚠ L'ORDONNÉE D'ÉCRAN DESCEND : moins quarante ÉLÈVE.
  REQUIRE(follow.corners[0].y == Approx(160.0F));

  node.flags |= static_cast<std::uint32_t>(EffectScreenFlag::kScreenAbsolute);
  const EffectScreenQuad absolu = ComposeScreenWedgeQuad(node, glm::vec2{300.0F, 200.0F});
  REQUIRE(absolu.corners[0].x == Approx(0.0F));
}

TEST_CASE("le rayon de la couronne ACCELERE, et la largeur se recalcule",
          "[render][screen_burst]") {
  EffectScreenNode node = TestRing(10.0F, 0.0F, 0.0F);
  node.outer_radius_step = 7.0F;
  node.outer_radius_acceleration = -0.175F;
  node.band_width_target = 10.0F;
  std::uint32_t generator = 1U;

  REQUIRE(AdvanceEffectScreenNode(node, generator));
  // Le pas gagne son accélération AVANT que le rayon gagne le pas.
  REQUIRE(node.outer_radius_step == Approx(6.825F));
  REQUIRE(node.outer_radius == Approx(6.825F));
  // ⚠ LA LARGEUR EST BRIDÉE PAR LE RAYON : une bande ne peut pas dépasser le
  // centre. Au premier tic elle vaut donc le rayon, pas les dix voulus.
  REQUIRE(node.band_width == Approx(6.825F));

  for (int tick = 0; tick < 5; ++tick) {
    static_cast<void>(AdvanceEffectScreenNode(node, generator));
  }
  REQUIRE(node.band_width == Approx(10.0F));
}

TEST_CASE("le drapeau UN pousse la bande jusqu'au centre", "[render][screen_burst]") {
  EffectScreenNode node = TestRing(35.0F, 100.0F, 0.0F);
  node.band_width_target = 10.0F;
  node.flags = static_cast<std::uint32_t>(EffectScreenFlag::kHangsBelowAnchor);
  std::uint32_t generator = 1U;
  static_cast<void>(AdvanceEffectScreenNode(node, generator));
  // ⚠ LE MÊME DRAPEAU FAIT PENDRE LE QUAD ET REMPLIT LA COURONNE : le sens
  // appartient à la figure, pas au bit.
  REQUIRE(node.band_width == Approx(node.outer_radius));
}

TEST_CASE("l'eclat tourne en RALENTISSANT et son rayon croit", "[render][screen_burst]") {
  EffectScreenNode node = TestBurst(1.0F, 20.0F);
  node.turn_speed = 3.0F;
  node.turn_acceleration = -(3.0F / 40.0F) / 1.5F;
  node.half_height_step = 2.5F;
  std::uint32_t generator = 1U;

  const float start_speed = node.turn_speed;
  static_cast<void>(AdvanceEffectScreenNode(node, generator));
  REQUIRE(node.turn_speed < start_speed);
  REQUIRE(node.turn_degrees == Approx(node.turn_speed));
  REQUIRE(node.half_height == Approx(22.5F));
}

TEST_CASE("la demi-ouverture croit du pas de DEMI-LARGEUR", "[render][screen_burst]") {
  EffectScreenNode node = TestBurst(1.0F, 20.0F);
  node.half_width_step = 0.25F;
  std::uint32_t generator = 1U;
  static_cast<void>(AdvanceEffectScreenNode(node, generator));
  // ⚠ UN CHAMP QUE CETTE FIGURE N'EMPLOIE À RIEN D'AUTRE. Lui donner un pas à
  // lui inventerait un champ que le client n'a pas.
  REQUIRE(node.angle_degrees == Approx(1.25F));
}

TEST_CASE("le rayon de l'eclat REBONDIT sous zero", "[render][screen_burst]") {
  EffectScreenNode node = TestBurst(1.0F, -5.0F);
  node.half_height_step = -3.0F;
  std::uint32_t generator = 1U;
  static_cast<void>(AdvanceEffectScreenNode(node, generator));
  // Le rayon repart de zéro, son pas retourné : l'éclat qui rentrait ressort.
  REQUIRE(node.half_height_step == Approx(3.0F));
  REQUIRE(node.half_height == Approx(3.0F));
}

TEST_CASE("le second pas de l'eclat ne remplace que DEUX champs", "[render][screen_burst]") {
  EffectScreenNode node = TestBurst(1.0F, 0.0F);
  // La troisième mise à jour lit l'âge DEUX : l'âge ne croît qu'après.
  node.second_step_tick = 2;
  node.turn_speed = 5.0F;
  node.turn_acceleration = -0.1F;
  node.half_height_step = 4.0F;
  node.turn_speed_after = 1.5F;
  node.turn_acceleration_after = 0.0F;
  node.half_height_step_after = 0.0F;
  std::uint32_t generator = 1U;

  static_cast<void>(AdvanceEffectScreenNode(node, generator));
  static_cast<void>(AdvanceEffectScreenNode(node, generator));
  const float radius_before = node.half_height;
  static_cast<void>(AdvanceEffectScreenNode(node, generator));

  // ⚠ ET SANS AUCUNE SENTINELLE : les deux valeurs s'écrivent telles quelles.
  // Employer la règle du quad laisserait la rotation décélérer.
  REQUIRE(node.turn_speed == Approx(1.5F));
  REQUIRE(node.turn_acceleration == Approx(0.0F));
  // Le rayon se fige : son pas est devenu nul.
  REQUIRE(node.half_height == Approx(radius_before));
}

TEST_CASE("les deux formes meurent par leur VIE, pas par leur image", "[render][screen_burst]") {
  std::uint32_t generator = 1U;
  for (const EffectScreenShape forme : {EffectScreenShape::kRing, EffectScreenShape::kWedge}) {
    EffectScreenNode node =
        forme == EffectScreenShape::kRing ? TestRing(35.0F, 10.0F, 1.0F) : TestBurst(1.0F, 10.0F);
    node.life_ticks = 5;
    // ⚠ LE QUAD MOURRAIT AU TROISIÈME TIC, quand son animation dépasse sa
    // dernière image. Celles-ci n'en avancent aucune, et vivent VIE + 1 mises
    // à jour : la mort tombe quand l'âge crû dépasse la vie.
    for (int tick = 0; tick < 5; ++tick) {
      REQUIRE(AdvanceEffectScreenNode(node, generator));
    }
    REQUIRE_FALSE(AdvanceEffectScreenNode(node, generator));
  }
}

TEST_CASE("une couronne ne tire RIEN, un eclat tire cinq fois", "[render][screen_burst]") {
  const std::span<const EffectScreenBurstPattern> table = EffectScreenBurstPatternTable();
  const auto crown = std::ranges::find_if(
      table, [](const EffectScreenBurstPattern& p) { return p.shape == EffectScreenShape::kRing; });
  REQUIRE(crown != table.end());
  std::uint32_t before = 9U;
  const std::vector<EffectScreenNode> rings = SowEffectScreenBurstNodes(*crown, before);
  REQUIRE(rings.size() == 1);
  REQUIRE(before == 9U);
}

TEST_CASE("un rayon a etendue nulle ne consomme AUCUN tirage", "[render][screen_burst]") {
  // ⚠ C'EST CE QUI DISTINGUE « toujours cette valeur » de « un tirage qui tombe
  // toujours pareil ». Le second décalerait tout ce qui suit. L'identifiant 98
  // part d'un rayon nul et tire, lui, sa vitesse d'après.
  EffectScreenBurstPattern pattern = EffectScreenBurstPatternsFor(98U)[0];
  REQUIRE(pattern.radius.spread == 0);
  REQUIRE(pattern.turn_speed_after.spread == 10);

  std::uint32_t with = 3U;
  std::uint32_t sans = 3U;
  const std::vector<EffectScreenNode> unique = SowEffectScreenBurstNodes(pattern, with);
  pattern.count = 2;
  static_cast<void>(SowEffectScreenBurstNodes(pattern, sans));
  REQUIRE(unique[0].half_height == Approx(0.0F));
  REQUIRE(with != sans);
}

TEST_CASE("la table des eclats d'ecran porte huit identifiants sur onze lignes",
          "[render][screen_burst]") {
  const std::span<const EffectScreenBurstPattern> table = EffectScreenBurstPatternTable();
  REQUIRE(table.size() == 11);
  REQUIRE(std::ranges::is_sorted(table, {}, &EffectScreenBurstPattern::effect_id));
  for (const std::uint32_t effect_id : {16U, 40U, 82U, 98U, 99U, 121U, 128U, 215U}) {
    REQUIRE_FALSE(EffectScreenBurstPatternsFor(effect_id).empty());
  }
  REQUIRE(EffectScreenBurstPatternsFor(1U).empty());
}

TEST_CASE("cinq des sept identifiants posent les DEUX formes", "[render][screen_burst]") {
  std::size_t two_shapes = 0;
  for (const std::uint32_t effect_id : {16U, 40U, 82U, 98U, 99U, 128U, 215U}) {
    const std::span<const EffectScreenBurstPattern> lines = EffectScreenBurstPatternsFor(effect_id);
    if (lines.size() == 2) {
      ++two_shapes;
    }
  }
  // 16, 40 et 82 posent les deux ; 98, 99 et 215 n'ont qu'un éclat ; 128 n'a
  // qu'une couronne.
  REQUIRE(two_shapes == 3);
}

TEST_CASE("Signum Crucis seme au TRENTIEME tic, les six autres au premier",
          "[render][screen_burst]") {
  for (const EffectScreenBurstPattern& pattern : EffectScreenBurstPatternTable()) {
    if (pattern.effect_id == 40U) {
      REQUIRE(pattern.sow_tick == 30);
    } else {
      REQUIRE(pattern.sow_tick == 0);
    }
  }
}

TEST_CASE("l'identifiant 82 ne seme que CINQ eclats, et ils RENTRENT", "[render][screen_burst]") {
  const std::span<const EffectScreenBurstPattern> lines = EffectScreenBurstPatternsFor(82U);
  const auto shard = std::ranges::find_if(lines, [](const EffectScreenBurstPattern& p) {
    return p.shape == EffectScreenShape::kWedge;
  });
  REQUIRE(shard != lines.end());
  // ⚠ CINQ, ET JAMAIS VINGT : la branche à vingt du client teste l'âge de
  // l'effet DANS un bloc qu'il n'atteint que si cet âge est nul.
  REQUIRE(shard->count == 5);
  // ⚠ ET SON RAYON DÉCROÎT : il part de trois cent cinquante et rentre vers le
  // centre. C'est l'onde qui se referme.
  REQUIRE(shard->radius.floor == Approx(350.0F));
  REQUIRE(shard->radius_step.divisor < 0.0F);
}

TEST_CASE("seules les couronnes de Stone Curse et de Sonic Blow posent leur pas angulaire",
          "[render][screen_burst]") {
  for (const EffectScreenBurstPattern& pattern : EffectScreenBurstPatternTable()) {
    if (pattern.shape != EffectScreenShape::kRing) {
      continue;
    }
    // ⚠ TRENTE-SIX EST LE DÉFAUT DU CONSTRUCTEUR, pas un choix :
    // trois bâtisseurs sur cinq ne posent jamais ce champ.
    const bool plays_step_sound = pattern.effect_id == 121U || pattern.effect_id == 128U;
    const float expected = plays_step_sound ? 10.0F : ro::render::kEffectScreenDefaultStepDegrees;
    REQUIRE(pattern.angle_degrees == Approx(expected));
  }
}

TEST_CASE("la couronne de Sonic Blow est celle de Stone Curse, a la vie de son effet",
          "[render][screen_burst]") {
  const std::span<const EffectScreenBurstPattern> sonic = EffectScreenBurstPatternsFor(121U);
  const std::span<const EffectScreenBurstPattern> stone = EffectScreenBurstPatternsFor(128U);
  REQUIRE(sonic.size() == 1);
  REQUIRE(stone.size() == 1);
  const EffectScreenBurstPattern& a = sonic[0];
  const EffectScreenBurstPattern& b = stone[0];
  REQUIRE(a.shape == EffectScreenShape::kRing);
  REQUIRE(a.sow_tick == 0);
  REQUIRE(a.texture == b.texture);
  REQUIRE(a.screen_offset_y == Approx(-20.0F));
  REQUIRE(a.fade_out_tick == 0);
  REQUIRE(a.outer_radius == Approx(0.0F));
  REQUIRE(a.outer_radius_step == Approx(7.0F));
  REQUIRE(a.band_width_target == Approx(10.0F));
  REQUIRE(a.alpha == Approx(b.alpha));
  // ⚠ SA VIE EST CELLE DE L'INSTANCE — quarante tics déclarés —, et son
  // freinage la suit : `-(7 / (vie + 40)) * 2`.
  REQUIRE(a.life_ticks == 40);
  REQUIRE(a.outer_radius_acceleration == Approx(-(7.0F / 80.0F) * 2.0F));
}
