#include "ro/render/effect_spiral_fan_seed.h"

#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_spiral_fan.h"

using ro::render::ComposeSpiralFanQuads;
using ro::render::EffectNodePool;
using ro::render::EffectSpiralFanNodePattern;
using ro::render::EffectSpiralFanNodePatternsFor;
using ro::render::EffectSpiralFanNodePatternTable;
using ro::render::EffectSpiralFanStyleSeed;
using ro::render::EffectSpiralFanStyleSeedAt;
using ro::render::EffectSpiralFanStyleSeedTable;
using ro::render::FanTracksFollowTheirOwner;
using ro::render::PooledNodeShape;
using ro::render::SowEffectSpiralFanNode;

namespace {

constexpr std::size_t kMarkOfStyle = 4;
constexpr std::size_t kSlotOfFirstBlade = 1;
// Le premier compteur de rotation, la première des sept lunes, et un rang que la
// table n'a pas.
constexpr std::uint32_t kFirstEffect = 758;
constexpr std::uint32_t kFirstMoonEffect = 467;
constexpr std::size_t kSeedNobodyHas = 200;
// La marque que le second bâtisseur pose, et qui fait suivre le porteur.
constexpr std::uint8_t kMarkThatFollowsTheOwner = 1;
// Assez de tics pour que la montée ait mené chaque lame à son plafond, puis
// assez pour que les premières soient retombées à zéro.
constexpr std::int32_t kTicksOfRise = 19;
constexpr std::int32_t kTicksOfFade = 20;
constexpr std::int32_t kLifeTicks = 200;
constexpr std::int32_t kMillisecondsPerTick = 1000 / ro::render::kEffectTicksPerSecond;
// Dix lames, et l'aura que ce style pose sur chacune.
constexpr std::size_t kQuadsWhenAllBladesShine = 20;

}  // namespace

TEST_CASE("Éventail : la table des nœuds est TRIÉE par identifiant", "[render][table]") {
  const std::span<const EffectSpiralFanNodePattern> table = EffectSpiralFanNodePatternTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
}

TEST_CASE("Éventail : tout nœud nomme un semis porté, et tout semis une texture",
          "[render][table]") {
  for (const EffectSpiralFanNodePattern& pattern : EffectSpiralFanNodePatternTable()) {
    INFO("effet " << pattern.effect_id);
    CHECK(EffectSpiralFanStyleSeedAt(pattern.seed) != nullptr);
    // ⚠ UN PATRON SANS TEXTURE N'EST PAS INCOMPLET quand il TIRE son icône : le
    // bâtisseur la compose alors à l'exécution, et le patron ne peut rien en
    // dire. Exiger une texture de tous écarterait justement ceux-là.
    if (pattern.draws_its_icon) {
      CHECK(pattern.texture.empty());
    } else {
      CHECK(pattern.texture.starts_with("data\\texture\\"));
    }
  }
  CHECK(EffectSpiralFanStyleSeedAt(kSeedNobodyHas) == nullptr);
  for (const EffectSpiralFanStyleSeed& seed : EffectSpiralFanStyleSeedTable()) {
    INFO("marque " << static_cast<int>(seed.draw_mark));
    CHECK(seed.radius_at_anchor > 0.0F);
  }
}

TEST_CASE("Éventail : DEUX bâtisseurs, et le style zéro n'y veut pas dire la même chose",
          "[render][table]") {
  // ⚠ C'EST POURQUOI LE NŒUD PORTE UN RANG ET NON UN STYLE. Le style zéro vaut
  // la marque cinq chez un bâtisseur et la marque un chez l'autre ; garder le
  // style comme clef ferait qu'un des deux écraserait l'autre, et l'ordre des
  // tables déciderait de qui gagne.
  const std::span<const EffectSpiralFanNodePattern> table = EffectSpiralFanNodePatternTable();
  std::set<std::string_view> fonctions;
  for (const EffectSpiralFanNodePattern& pattern : table) {
    fonctions.insert(pattern.client_function);
  }
  CHECK(fonctions.size() == 2);

  const auto mark_of = [](std::uint32_t effect_id) {
    const std::span<const EffectSpiralFanNodePattern> patterns =
        EffectSpiralFanNodePatternsFor(effect_id);
    REQUIRE(patterns.size() == 1);
    const EffectSpiralFanStyleSeed* seed = EffectSpiralFanStyleSeedAt(patterns.front().seed);
    REQUIRE(seed != nullptr);
    return seed->draw_mark;
  };
  // Les deux passent zéro à leur bâtisseur, et n'obtiennent pas la même marque.
  CHECK(mark_of(kFirstEffect) == 5);
  CHECK(mark_of(kFirstMoonEffect) == kMarkThatFollowsTheOwner);
}

TEST_CASE("Éventail : une marque fait SUIVRE LE PORTEUR, les autres non", "[render][pool]") {
  // ⚠ MÊME CASE, DEUX SENS — ADR 0017. La marque cinq dit « bouquet serré », la
  // marque un dit « suis le personnage ». Y voir un numéro de style planterait
  // les sept lunes là où le lanceur se tenait au premier tic.
  const std::span<const EffectSpiralFanNodePattern> lune =
      EffectSpiralFanNodePatternsFor(kFirstMoonEffect);
  REQUIRE_FALSE(lune.empty());
  ro::render::PooledEffectNode suiveur = SowEffectSpiralFanNode(lune.front(), {}, kLifeTicks);
  CHECK(FanTracksFollowTheirOwner(suiveur.tracks));

  const std::span<const EffectSpiralFanNodePattern> counter =
      EffectSpiralFanNodePatternsFor(kFirstEffect);
  REQUIRE_FALSE(counter.empty());
  const ro::render::PooledEffectNode planted =
      SowEffectSpiralFanNode(counter.front(), {}, kLifeTicks);
  CHECK_FALSE(FanTracksFollowTheirOwner(planted.tracks));

  // La troupe emporte le nœud suiveur là où son porteur s'est rendu.
  EffectNodePool pool;
  suiveur.owner_position = {30.0F, 0.0F, -12.0F};
  pool.Add(suiveur);
  pool.Advance(kMillisecondsPerTick);
  REQUIRE_FALSE(pool.nodes().empty());
  CHECK(pool.nodes().front().ground_position.x == Catch::Approx(30.0F));
  CHECK(pool.nodes().front().ground_position.z == Catch::Approx(-12.0F));
}

TEST_CASE("Éventail : les deux BOUCLES, et leur icône composée", "[render][table]") {
  // ⚠ CES DEUX-LÀ ONT ATTENDU DEUX LOTS, et pour une raison qui n était pas une
  // figure : leur phase est CALCULÉE — le cas rappelle le bâtisseur en boucle —
  // et leur image n est écrite nulle part en dur. Le client la COMPOSE à
  // l exécution, à partir d un nom que le Lua lui rend.
  const std::span<const EffectSpiralFanNodePattern> recus = EffectSpiralFanNodePatternsFor(803);
  REQUIRE(recus.size() == 1);
  // DIX SEMIS, tous les trente tics : c est ce que la boucle du cas fait.
  CHECK(recus.front().sow_count == 10);
  CHECK(recus.front().sow_period_ticks == 30);
  // ⚠ ET SON PATRON NE NOMME AUCUNE IMAGE : il la TIRE. Un patron sans texture
  // n est donc pas un patron incomplet.
  CHECK(recus.front().draws_its_icon);
  CHECK(recus.front().texture.empty());

  // Le frère, lui, ne tire rien : son style nomme la compétence copiée.
  const std::span<const EffectSpiralFanNodePattern> copies = EffectSpiralFanNodePatternsFor(808);
  REQUIRE(copies.size() == 1);
  CHECK(copies.front().sow_count == 5);
  CHECK_FALSE(copies.front().draws_its_icon);
  CHECK(copies.front().texture.ends_with("SC_REPRODUCE.bmp"));

  // ⚠ LES DEUX PARTAGENT LEUR SEMIS, et ce n est pas celui des sept lunes : la
  // branche « phase positive » du bâtisseur pose un autre rayon, une autre
  // marque et un décalage de côté.
  CHECK(recus.front().seed == copies.front().seed);
  const EffectSpiralFanStyleSeed* seed = ro::render::EffectSpiralFanStyleSeedAt(recus.front().seed);
  REQUIRE(seed != nullptr);
  CHECK(seed->draw_mark == 4);
  CHECK(seed->radius_at_anchor == Catch::Approx(5.0F));
  CHECK(seed->sideways == Catch::Approx(1.0F));
}

TEST_CASE("Éventail : le tirage d icône reste dans les deux plages du client", "[render][table]") {
  // ⚠ DEUX PLAGES, ET UN PILE OU FACE ENTRE ELLES : douze sorts de mage, treize
  // de sorcier. Le tirage ne doit jamais sortir de la table des icônes — sans
  // quoi l effet serait muet un semis sur dix, et rien ne le dirait.
  std::uint32_t generator = 1U;
  for (int attempt = 0; attempt < 200; ++attempt) {
    const std::string_view icone = ro::render::EffectSpiralFanDrawnIcon(generator);
    INFO("essai " << attempt);
    REQUIRE_FALSE(icone.empty());
    CHECK(icone.ends_with(".bmp"));
  }
}

TEST_CASE("Éventail : le retard recule le compteur ET rallonge la vie", "[render][pool]") {
  const std::span<const EffectSpiralFanNodePattern> recus = EffectSpiralFanNodePatternsFor(803);
  REQUIRE(recus.size() == 1);
  std::uint32_t generator = 3U;
  const std::string_view icone = ro::render::EffectSpiralFanDrawnIcon(generator);
  const ro::render::PooledEffectNode neuvieme =
      SowEffectSpiralFanNode(recus.front(), {0.0F, 0.0F, 0.0F}, kLifeTicks, -270, icone);
  CHECK(neuvieme.tracks.front().ticks == -270);
  // ⚠ LA VIE RATTRAPE LE RETARD, et il le faut : le nœud du client NAÎT au tic de
  // sa phase. Sans ce rattrapage, le dernier des dix serait mort avant d avoir
  // commencé.
  CHECK(neuvieme.node.life_ticks == kLifeTicks + 270);
  REQUIRE(neuvieme.resource_paths.size() == 1);
  CHECK(neuvieme.resource_paths.front() == icone);
}

TEST_CASE("Éventail : le nœud semé est INVISIBLE, et une seule piste est vive", "[render][pool]") {
  const std::span<const EffectSpiralFanNodePattern> patterns =
      EffectSpiralFanNodePatternsFor(kFirstEffect);
  REQUIRE(patterns.size() == 1);
  const ro::render::PooledEffectNode node =
      SowEffectSpiralFanNode(patterns.front(), {3.0F, 0.0F, 4.0F}, kLifeTicks);
  CHECK(node.shape == PooledNodeShape::kSpiralFan);
  CHECK(node.resource_paths.size() == 1);
  CHECK(node.tracks.front().active);
  // ⚠ LES TROIS AUTRES RESTENT ÉTEINTES POUR DE BON : aucun mouvement ne les
  // rallume, et les allumer donnerait quatre éventails superposés.
  for (std::size_t rang = 1; rang < node.tracks.size(); ++rang) {
    INFO("piste " << rang);
    CHECK_FALSE(node.tracks[rang].active);
  }
  CHECK(node.tracks.front().mark[kMarkOfStyle] ==
        EffectSpiralFanStyleSeedAt(patterns.front().seed)->draw_mark);
  // ⚠ ET RIEN NE SE MONTRE AU TIC DU SEMIS : les dix lames naissent éteintes.
  CHECK(node.tracks.front().value[kSlotOfFirstBlade] == Catch::Approx(0.0F));
  CHECK(ComposeSpiralFanQuads(node.tracks.front(), 0, 0, node.ground_position).empty());
}

TEST_CASE("Éventail : la troupe le FERME lame par lame, du bord vers le centre", "[render][pool]") {
  const std::span<const EffectSpiralFanNodePattern> patterns =
      EffectSpiralFanNodePatternsFor(kFirstEffect);
  REQUIRE_FALSE(patterns.empty());
  EffectNodePool pool;
  pool.Add(SowEffectSpiralFanNode(patterns.front(), {}, kLifeTicks));

  const auto quads_maintenant = [&pool]() {
    const ro::render::PooledEffectNode& node = pool.nodes().front();
    return ComposeSpiralFanQuads(node.tracks.front(), node.tracks.front().style,
                                 node.tracks[1].style, node.ground_position);
  };

  pool.Advance(kMillisecondsPerTick);
  REQUIRE_FALSE(pool.nodes().empty());
  // ⚠ LES DIX LAMES S'ALLUMENT AU MÊME TIC, et au même éclat. Le mouvement les
  // monte ensemble ; ce n'est pas là que l'éventail se dévoile.
  const std::vector<ro::render::EffectWorldQuad> at_first_tick = quads_maintenant();
  CHECK(at_first_tick.size() == kQuadsWhenAllBladesShine);
  CHECK(at_first_tick.front().tint[3] == at_first_tick.back().tint[3]);

  // ⚠ C'EST LA MONTÉE QUI ÉCARTE LES ÉCLATS, parce que chaque lame a son propre
  // plafond. Sans ces plafonds, l'éventail resterait une plaque d'un seul ton.
  for (std::int32_t tick = 1; tick < kTicksOfRise; ++tick) {
    pool.Advance(kMillisecondsPerTick);
  }
  const std::vector<ro::render::EffectWorldQuad> at_peak = quads_maintenant();
  REQUIRE(at_peak.size() == kQuadsWhenAllBladesShine);
  CHECK(at_peak.front().tint[3] > at_peak.back().tint[3]);

  // ⚠ ET C'EST À LA FERMETURE QUE LES LAMES S'ÉTEIGNENT UNE À UNE : celles du
  // bord, parties de plus bas, atteignent zéro les premières.
  for (std::int32_t tick = 0; tick < kTicksOfFade; ++tick) {
    pool.Advance(kMillisecondsPerTick);
  }
  REQUIRE_FALSE(pool.nodes().empty());
  const std::vector<ro::render::EffectWorldQuad> en_fermant = quads_maintenant();
  CHECK(!en_fermant.empty());
  CHECK(en_fermant.size() < kQuadsWhenAllBladesShine);
}
