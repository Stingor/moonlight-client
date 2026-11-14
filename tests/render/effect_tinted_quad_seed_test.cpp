#include "ro/render/effect_tinted_quad_seed.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_heal_sparkle_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_tinted_quad.h"

using ro::render::ComposeTintedRotatedQuad;
using ro::render::EffectTintedQuadNodePattern;
using ro::render::EffectTintedQuadNodePatternsFor;
using ro::render::EffectTintedQuadNodePatternTable;
using ro::render::EffectTintedQuadStyleFor;
using ro::render::EffectTintedQuadStyleSeed;
using ro::render::EffectTintedQuadStyleSeedFor;
using ro::render::EffectTintedQuadStyleSeedTable;
using ro::render::EffectTintedQuadStyleTextures;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectNodeDefaultLifeTicks;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectTintedQuadTracks;
using ro::render::SowEffectTintedQuadNode;

namespace {

// Le seul style qui pose une sous-marque, et le seul qui pousse son carré.
constexpr std::uint8_t kStyleThatDelegates = 9;
constexpr std::uint8_t kStyleThatNudges = 4;
// La marque de dessin qui délègue à une sous-marque.
constexpr std::uint8_t kDrawMarkThatDelegates = 9;
// La clé qui déclenche le décalage, telle que la figure la lit.
constexpr float kNudgeKeyThatShifts = 10.0F;
// Un style ordinaire, et un que personne n'appelle.
constexpr std::uint8_t kPlainStyle = 0;
constexpr std::uint8_t kStyleNobodyCalls = 200;

}  // namespace

TEST_CASE("Carrés : la table des nœuds est TRIÉE par identifiant", "[render][table]") {
  const std::span<const EffectTintedQuadNodePattern> table = EffectTintedQuadNodePatternTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
}

TEST_CASE("Carrés : tout nœud nomme un style que la table des styles porte", "[render][table]") {
  for (const EffectTintedQuadNodePattern& pattern : EffectTintedQuadNodePatternTable()) {
    INFO("effet " << pattern.effect_id << ", style " << static_cast<int>(pattern.style));
    CHECK(EffectTintedQuadStyleSeedFor(pattern.style) != nullptr);
  }
}

TEST_CASE("Carrés : un nœud nomme sa texture, ou son STYLE la lui choisit", "[render][table]") {
  // ⚠ LA RÈGLE A DEUX BRANCHES, et c'est le client qui en décide. Sept styles
  // lisent la texture que leur site d'appel passe ; les trois autres n'y
  // touchent pas et TIRENT AU SORT dans une liste que le style porte. Pour
  // ceux-là, le site n'a rien à passer — les trois y mettent d'ailleurs une
  // table et non un chemin, ce qui écartait ces appels tant que le C++ ne
  // savait pas que le style pouvait répondre à leur place.
  //
  // Une rangée sans chemin ET sans liste de style serait invisible, donc pire
  // qu'absente : on croirait l'identifiant traité.
  std::size_t choisies_par_le_style = 0;
  for (const EffectTintedQuadNodePattern& pattern : EffectTintedQuadNodePatternTable()) {
    INFO("effet " << pattern.effect_id << " : " << pattern.texture);
    const EffectTintedQuadStyleSeed* seed = EffectTintedQuadStyleSeedFor(pattern.style);
    REQUIRE(seed != nullptr);
    const std::span<const std::string_view> siennes = EffectTintedQuadStyleTextures(*seed);
    if (siennes.empty()) {
      CHECK(pattern.texture.find('.') != std::string_view::npos);
      continue;
    }
    // ⚠ ET LE CHEMIN DU PATRON DOIT ÊTRE VIDE quand le style choisit : y laisser
    // celui du site ferait croire qu'il sert à quelque chose.
    CHECK(pattern.texture.empty());
    ++choisies_par_le_style;
  }
  CHECK(choisies_par_le_style == 3);

  // Et toute texture qu'un style peut tirer est un chemin, elle aussi.
  for (const EffectTintedQuadStyleSeed& seed : EffectTintedQuadStyleSeedTable()) {
    for (const std::string_view path : EffectTintedQuadStyleTextures(seed)) {
      INFO("style " << static_cast<int>(seed.style) << " : " << path);
      CHECK(path.starts_with("data\\texture\\"));
      CHECK(path.find('.') != std::string_view::npos);
    }
  }
}

TEST_CASE("Carrés : le style qui tire sa texture les emploie TOUTES", "[render][table]") {
  // ⚠ LE RANG D'UNE TEXTURE EST LA VALEUR QUE LE TIRAGE DOIT RENDRE. Une liste
  // rangée dans un autre ordre échangerait deux gemmes sans que rien ne le
  // signale ; une liste tronquée en perdrait une une fois sur huit. Semer assez
  // de fois doit donc faire sortir chacune au moins une fois, et JAMAIS autre
  // chose.
  for (const EffectTintedQuadNodePattern& pattern : EffectTintedQuadNodePatternTable()) {
    const EffectTintedQuadStyleSeed* seed = EffectTintedQuadStyleSeedFor(pattern.style);
    REQUIRE(seed != nullptr);
    const std::span<const std::string_view> siennes = EffectTintedQuadStyleTextures(*seed);
    if (siennes.empty()) {
      continue;
    }
    std::vector<bool> seen(siennes.size(), false);
    for (std::uint32_t draw_seed = 1; draw_seed <= 400U; ++draw_seed) {
      std::uint32_t generator = draw_seed;
      const PooledEffectNode node =
          SowEffectTintedQuadNode(pattern, glm::vec3{0.0F, 0.0F, 0.0F}, 10, generator);
      REQUIRE(node.resource_paths.size() == 1);
      const auto found = std::ranges::find(siennes, node.resource_paths.front());
      INFO("effet " << pattern.effect_id << " a tiré " << node.resource_paths.front());
      REQUIRE(found != siennes.end());
      seen[static_cast<std::size_t>(found - siennes.begin())] = true;
    }
    for (std::size_t rang = 0; rang < seen.size(); ++rang) {
      INFO("effet " << pattern.effect_id << ", texture de rang " << rang << " jamais tirée");
      CHECK(seen[rang]);
    }
  }
}

TEST_CASE("Carrés : un seul style ouvre les QUATRE pistes", "[render][table]") {
  // ⚠ ET IL NE LEUR DONNE RIEN D'AUTRE. Les trois pistes de plus naissent au
  // point zéro, de rayon nul, l'alpha à zéro : invisibles jusqu'à ce que leur
  // mouvement les remplisse. Leur donner l'état de la première inventerait trois
  // carrés que le client ne montre pas.
  std::size_t opening_everything = 0;
  for (const EffectTintedQuadStyleSeed& seed : EffectTintedQuadStyleSeedTable()) {
    if (!seed.lights_every_track) {
      continue;
    }
    ++opening_everything;
    std::uint32_t generator = 7U;
    const std::array<EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectTintedQuadTracks(seed, glm::vec3{3.0F, 1.0F, -2.0F}, generator);
    for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
      INFO("style " << static_cast<int>(seed.style) << ", piste " << rang);
      CHECK(tracks[rang].active);
      CHECK(tracks[rang].alpha == 0);
      CHECK(tracks[rang].radius_at_anchor == 0.0F);
      CHECK(tracks[rang].anchor.x == 0.0F);
    }
    // La première, elle, reste celle que tous les styles sèment.
    CHECK(tracks[0].alpha == 200);
    CHECK(tracks[0].radius_at_anchor > 0.0F);
  }
  CHECK(opening_everything == 1);

  // Les neuf autres n'ouvrent QUE la première.
  for (const EffectTintedQuadStyleSeed& seed : EffectTintedQuadStyleSeedTable()) {
    if (seed.lights_every_track) {
      continue;
    }
    std::uint32_t generator = 7U;
    const std::array<EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectTintedQuadTracks(seed, glm::vec3{3.0F, 1.0F, -2.0F}, generator);
    INFO("style " << static_cast<int>(seed.style));
    CHECK(tracks[1].active == false);
    CHECK(tracks[2].active == false);
    CHECK(tracks[3].active == false);
  }
}

TEST_CASE("Carrés : le SEMIS et la FIGURE se recoupent sans se connaître", "[render][table]") {
  // ⚠ C'EST LA MEILLEURE PREUVE QU'ON AIT DES DEUX. Le bâtisseur et le
  // dessinateur sont portés séparément, et ils s'accordent : le seul style qui
  // pose une sous-marque est exactement celui dont la marque de dessin est la
  // seule à en consulter une.
  std::size_t with_submark = 0;
  for (const EffectTintedQuadStyleSeed& seed : EffectTintedQuadStyleSeedTable()) {
    if (seed.sub_mark == 0) {
      continue;
    }
    ++with_submark;
    INFO("style " << static_cast<int>(seed.style));
    CHECK(seed.draw_mark == kDrawMarkThatDelegates);
    // Et sa sous-marque dessine vraiment : une sous-marque hors des six
    // branches du client laisserait ce style muet.
    CHECK(EffectTintedQuadStyleFor(seed.draw_mark, seed.sub_mark).draws);
  }
  CHECK(with_submark == 1);
}

TEST_CASE("Carrés : la clé de décalage ne prend sa valeur déclenchante qu'une fois",
          "[render][table]") {
  std::size_t poussent = 0;
  for (const EffectTintedQuadStyleSeed& seed : EffectTintedQuadStyleSeedTable()) {
    if (seed.nudge_key == kNudgeKeyThatShifts) {
      ++poussent;
    }
    // Aucune autre valeur que zéro ou la valeur déclenchante : une clé
    // intermédiaire voudrait dire qu'une branche manque.
    CHECK((seed.nudge_key == 0.0F || seed.nudge_key == kNudgeKeyThatShifts));
  }
  CHECK(poussent == 1);
}

TEST_CASE("Carrés : un style qu'aucun site n'appelle n'a PAS de semis", "[render][table]") {
  CHECK(EffectTintedQuadStyleSeedFor(kStyleNobodyCalls) == nullptr);
}

TEST_CASE("Carrés : le semis n'allume QU'UNE piste sur quatre", "[render][track]") {
  const EffectTintedQuadStyleSeed* seed = EffectTintedQuadStyleSeedFor(kPlainStyle);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 1;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectTintedQuadTracks(*seed, {}, generator);
  CHECK(tracks[0].active);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    INFO("piste " << rang);
    CHECK_FALSE(tracks[rang].active);
  }
}

TEST_CASE("Carrés : l'alpha naît PLEIN, et le carré se montre au premier tic", "[render][track]") {
  // ⚠ C'EST LÀ QUE CE BÂTISSEUR DIFFÈRE DE CELUI DE L'ARC, dont l'alpha naît nul
  // et attend son mouvement. Le supposer semblable laisserait l'écran vide.
  const EffectTintedQuadStyleSeed* seed = EffectTintedQuadStyleSeedFor(kPlainStyle);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 2;
  const auto tracks = SeedEffectTintedQuadTracks(*seed, {}, generator);
  CHECK(tracks[0].alpha > 0);
  const std::optional<EffectWorldQuad> quad =
      ComposeTintedRotatedQuad(tracks[0], seed->draw_mark, seed->sub_mark, 0, 0);
  REQUIRE(quad.has_value());
  CHECK(glm::length(quad->corners[1] - quad->corners[0]) > 0.0F);
}

TEST_CASE("Carrés : le style qui pousse pousse VRAIMENT, une fois semé", "[render][track]") {
  const EffectTintedQuadStyleSeed* nudged = EffectTintedQuadStyleSeedFor(kStyleThatNudges);
  const EffectTintedQuadStyleSeed* straight = EffectTintedQuadStyleSeedFor(kPlainStyle);
  REQUIRE(nudged != nullptr);
  REQUIRE(straight != nullptr);
  REQUIRE(nudged->nudge_key == kNudgeKeyThatShifts);
  REQUIRE(straight->nudge_key == 0.0F);
  std::uint32_t generator = 3;
  const auto shifted = SeedEffectTintedQuadTracks(*nudged, {}, generator);
  const auto centre = SeedEffectTintedQuadTracks(*straight, {}, generator);
  CHECK(shifted[0].value[9] == Catch::Approx(kNudgeKeyThatShifts));
  CHECK(centre[0].value[9] == Catch::Approx(0.0F));
}

TEST_CASE("Carrés : l'élévation du semis MONTE dans notre repère", "[render][track]") {
  const EffectTintedQuadStyleSeed* seed = EffectTintedQuadStyleSeedFor(kPlainStyle);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 4;
  const glm::vec3 point{7.0F, 20.0F, -2.0F};
  const auto tracks = SeedEffectTintedQuadTracks(*seed, point, generator);
  // Le client retranche six à la hauteur de son effet, dans un repère qui
  // DESCEND : le carré se tient donc au-dessus, et chez nous l'ordonnée croît.
  CHECK(tracks[0].anchor.y > point.y);
  CHECK(tracks[0].anchor.x == Catch::Approx(point.x));
  CHECK(tracks[0].anchor.z == Catch::Approx(point.z));
}

TEST_CASE("Carrés : le nœud semé porte sa forme et la vie qu'on lui donne", "[render][pool]") {
  const std::span<const EffectTintedQuadNodePattern> table = EffectTintedQuadNodePatternTable();
  REQUIRE_FALSE(table.empty());
  std::uint32_t generator = 9;
  const ro::render::PooledEffectNode node = SowEffectTintedQuadNode(
      table.front(), {1.0F, 2.0F, 3.0F}, kEffectNodeDefaultLifeTicks, generator);
  CHECK(node.shape == PooledNodeShape::kTintedQuads);
  CHECK(node.resource_paths.size() == 1);
  CHECK(node.node.life_ticks == kEffectNodeDefaultLifeTicks);
  CHECK(node.tracks[0].active);
}

TEST_CASE("Carrés : la troupe ne fait PAS battre ces pistes, et c'est mesuré", "[render][pool]") {
  // Le mouvement du sous-type 47 n'est pas porté. La forme doit donc rester
  // identique d'un tic à l'autre : la faire battre par le mouvement d'une autre
  // figure lui donnerait un rythme qui n'est pas le sien, ce qui aurait l'air de
  // marcher — et c'est bien pire que l'immobilité.
  const std::span<const EffectTintedQuadNodePattern> table = EffectTintedQuadNodePatternTable();
  REQUIRE_FALSE(table.empty());
  std::uint32_t generator = 6;
  ro::render::EffectNodePool pool;
  pool.Add(SowEffectTintedQuadNode(table.front(), {}, kEffectNodeDefaultLifeTicks, generator));
  const EffectTrack before = pool.nodes().front().tracks.front();
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    pool.Advance(1000 / ro::render::kEffectTicksPerSecond);
  }
  REQUIRE_FALSE(pool.nodes().empty());
  const EffectTrack after = pool.nodes().front().tracks.front();
  CHECK(after.alpha == before.alpha);
  CHECK(after.degrees == before.degrees);
  CHECK(after.radius_at_anchor == Catch::Approx(before.radius_at_anchor));
}

TEST_CASE("Étincelle du soin : la table dit ce que le relevé a mesuré", "[render][healsparkle]") {
  // UN SEUL bâtisseur, UN SEUL identifiant : c'est ce qui fait que cette famille
  // n'a ni crible ni variantes.
  const std::span<const ro::render::EffectHealSparklePattern> table =
      ro::render::EffectHealSparklePatternTable();
  REQUIRE(table.size() == 1);
  CHECK(table.front().effect_id == 309);  // EF_FIRSTAID
  CHECK(table.front().builder == "Effect_Build_HealPikapika");
  CHECK(table.front().texture == "data\\texture\\effect\\pikapika2.bmp");
  CHECK(table.front().mark == 1);
  CHECK(table.front().radius == Catch::Approx(10.0F));
  CHECK(ro::render::EffectHealSparklePatternsFor(309).size() == 1);
  CHECK(ro::render::EffectHealSparklePatternsFor(310).empty());
}

TEST_CASE("Étincelle du soin : UNE seule piste, et elle naît invisible", "[render][healsparkle]") {
  // ⚠ LES TROIS AUTRES RESTENT ÉTEINTES. Le bâtisseur n'allume que la première,
  // et le mouvement comme la figure ne travaillent que sur elle : en allumer
  // quatre montrerait quatre étincelles là où le client en pose une.
  std::uint32_t seed = 1;
  const std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks =
      ro::render::SeedEffectHealSparkleTracks(ro::render::EffectHealSparklePatternTable().front(),
                                              seed);
  CHECK(tracks[0].active);
  CHECK_FALSE(tracks[1].active);
  CHECK_FALSE(tracks[2].active);
  CHECK_FALSE(tracks[3].active);
  // Alpha nul : rien ne se montre tant que le mouvement n'a pas tourné.
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].radius_at_anchor == Catch::Approx(10.0F));
  CHECK(tracks[0].mark[2] == 1);
}

TEST_CASE("Étincelle du soin : l'alpha paraît, tient, puis s'efface", "[render][healsparkle]") {
  // La courbe entière du cas 58 : quatre par tic pendant vingt tics, le palier
  // jusqu'au centième, puis un par tic jusqu'à zéro.
  std::uint32_t seed = 1;
  std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks =
      ro::render::SeedEffectHealSparkleTracks(ro::render::EffectHealSparklePatternTable().front(),
                                              seed);
  for (std::int32_t tick = 1; tick <= 20; ++tick) {
    ro::render::AdvanceHealSparkleTracks(tracks, 0, 0);
    CHECK(tracks[0].alpha == 4 * tick);
  }
  CHECK(tracks[0].alpha == 80);
  // LE PALIER : quatre-vingts tics sans bouger.
  for (std::int32_t tick = 21; tick <= 100; ++tick) {
    ro::render::AdvanceHealSparkleTracks(tracks, 0, 0);
  }
  CHECK(tracks[0].alpha == 80);
  // L'EFFACEMENT : un par tic, et le plancher tient.
  for (std::int32_t tick = 0; tick < 80; ++tick) {
    ro::render::AdvanceHealSparkleTracks(tracks, 0, 0);
  }
  CHECK(tracks[0].alpha == 0);
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    ro::render::AdvanceHealSparkleTracks(tracks, 0, 0);
  }
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("Étincelle du soin : l'angle propre tourne de trois et se replie",
          "[render][healsparkle]") {
  // C'est lui, et lui seul, qui fait respirer le rayon de la figure.
  std::uint32_t seed = 1;
  std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks =
      ro::render::SeedEffectHealSparkleTracks(ro::render::EffectHealSparklePatternTable().front(),
                                              seed);
  constexpr std::size_t kSlotOfSpinDegrees = 8;
  // ⚠ IL NAÎT TIRÉ AU SORT : on mesure le PAS, et non la valeur. Le comparer à
  // trois supposerait un départ à zéro, que le bâtisseur ne fait pas.
  const float start = tracks[0].value[kSlotOfSpinDegrees];
  CHECK(start >= 0.0F);
  CHECK(start < 360.0F);
  ro::render::AdvanceHealSparkleTracks(tracks, 0, 0);
  // ⚠ ET LE PAS SE REPLIE, puisque le départ est tiré au sort : un tirage à 358
  // ressort à 1 au tic suivant. L'écrire `depart + 3` supposerait un départ bas,
  // ce que rien ne garantit — et c'est le tirage lui-même qui l'a montré.
  const float expected = start + 3.0F >= 360.0F ? start + 3.0F - 360.0F : start + 3.0F;
  CHECK(tracks[0].value[kSlotOfSpinDegrees] == Catch::Approx(expected));
  for (std::int32_t tick = 0; tick < 119; ++tick) {
    ro::render::AdvanceHealSparkleTracks(tracks, 0, 0);
  }
  // Cent vingt tics à trois degrés font le tour : il se replie sur son départ.
  CHECK(tracks[0].value[kSlotOfSpinDegrees] == Catch::Approx(start));
}

TEST_CASE("Étincelle du soin : les deux angles de caméra vont dans les STYLES",
          "[render][healsparkle]") {
  // ⚠ ET ILS Y VONT MÊME QUAND LA PISTE EST ÉTEINTE : le client les range avant
  // de tester l'allumage. Une figure qui les relirait sur un nœud éteint les
  // trouverait quand même à jour.
  std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  ro::render::AdvanceHealSparkleTracks(tracks, 137, 42);
  CHECK(tracks[0].style == 137);
  CHECK(tracks[1].style == 42);
  CHECK_FALSE(tracks[0].active);
  CHECK(tracks[0].ticks == 0);
}

TEST_CASE("Étincelle du soin : elle s'élève de douze au-dessus de son porteur",
          "[render][healsparkle]") {
  // 🔴 CE QUI MANQUAIT AU PREMIER CÔTE À CÔTE. Notre étincelle paraissait aux
  // PIEDS, celle du natif à hauteur de poitrine. Le bâtisseur écrit
  // `ancre_y = instance.y - 12.0`, et le repère du client DESCEND :
  // une soustraction y ÉLÈVE. Recopier le signe au lieu du sens l'enfoncerait de
  // douze unités sous le sol.
  //
  // ⚠ ET C'EST UN DÉCALAGE, PAS UNE POSITION. Le bâtisseur range la position
  // entière de l'instance dans l'ancre ; chez nous c'est l'ORIGINE DU NŒUD qui
  // la porte, et la figure l'ajoute pour toute marque relative — celle de
  // l'étincelle l'est. Seule la DIFFÉRENCE appartient donc à la piste.
  std::uint32_t seed = 1;
  const std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks =
      ro::render::SeedEffectHealSparkleTracks(ro::render::EffectHealSparklePatternTable().front(),
                                              seed);
  CHECK(tracks[0].anchor.x == Catch::Approx(0.0F));
  CHECK(tracks[0].anchor.y == Catch::Approx(12.0F));
  CHECK(tracks[0].anchor.z == Catch::Approx(0.0F));
  CHECK(ro::render::kHealSparkleLiftAboveOwner == Catch::Approx(12.0F));
}

TEST_CASE("Étincelle du soin : deux angles TIRÉS, et deux soins ne se ressemblent pas",
          "[render][healsparkle]") {
  // 🔴 L'AUTRE MOITIÉ DE CE QUI SE VOYAIT. Le bâtisseur tire l'angle des COINS
  // et l'angle PROPRE, chacun sur le tour entier. Les laisser à zéro donnait la
  // même étincelle, toujours dans le même sens.
  constexpr std::size_t kSlotOfSpinDegrees = 8;
  std::uint32_t first = 1;
  std::uint32_t second = 7;
  const std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> one =
      ro::render::SeedEffectHealSparkleTracks(ro::render::EffectHealSparklePatternTable().front(),
                                              first);
  const std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> other =
      ro::render::SeedEffectHealSparkleTracks(ro::render::EffectHealSparklePatternTable().front(),
                                              second);
  // Les deux angles restent dans le tour.
  for (const std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode>& tracks :
       {one, other}) {
    CHECK(tracks[0].degrees >= 0);
    CHECK(tracks[0].degrees < 360);
    CHECK(tracks[0].value[kSlotOfSpinDegrees] >= 0.0F);
    CHECK(tracks[0].value[kSlotOfSpinDegrees] < 360.0F);
  }
  // ⚠ ET LA GRAINE AVANCE ENTRE LES DEUX TIRAGES : si elle ne le faisait pas,
  // les deux angles d'une même étincelle seraient égaux, ce qui ne se verrait
  // qu'en regardant très longtemps.
  CHECK(static_cast<float>(one[0].degrees) != Catch::Approx(one[0].value[kSlotOfSpinDegrees]));
  // Deux semis de graines différentes ne donnent pas la même étincelle.
  CHECK(one[0].degrees != other[0].degrees);
}

TEST_CASE("Étincelle du soin : l'ancre est le porteur au semis, douze plus haut",
          "[render][healsparkle]") {
  // La position de l'instance, l'ordonnée moins douze dans un repère qui
  // descend. La figure n'y ajoute rien ensuite.
  const glm::vec3 bearer{500.0F, 7.0F, 300.0F};
  std::uint32_t seed = 1;
  const ro::render::PooledEffectNode node = ro::render::SowEffectHealSparkleNode(
      ro::render::EffectHealSparklePatternTable().front(), bearer, 100, seed);
  const glm::vec3 anchor = node.tracks.front().anchor;
  CHECK(anchor.x == Catch::Approx(bearer.x));
  CHECK(anchor.y == Catch::Approx(bearer.y + ro::render::kHealSparkleLiftAboveOwner));
  CHECK(anchor.z == Catch::Approx(bearer.z));
}
