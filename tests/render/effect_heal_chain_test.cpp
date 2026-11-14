#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_rising_spark_seed.h"
#include "ro/render/effect_seeded_ring.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_track_figures.h"

// LE SOIN, DE BOUT EN BOUT — et c'est la seule façon de savoir qu'il se voit.
//
// ⚠ CHACUNE DES QUATRE PIÈCES PEUT ÊTRE JUSTE SANS QUE RIEN NE PARAISSE
// (ADR 0021). La table dit ce qu'on sème, le semis pose les pistes, le mouvement
// les fait vivre, la figure en tire des quads — et un test par pièce laisse
// passer une chaîne qui se rompt entre deux. Celui-ci part de la COMPÉTENCE et
// finit sur des quads qui ont une aire et une couleur.
namespace {

constexpr std::uint16_t kSkillHeal = 28;

// La teinte que le client donne à la marque onze, dans sa table de carrés
// teintés : c'est de là, et non de la texture, que vient le VERT du soin.
constexpr int kHealGreenRed = 220;
constexpr int kHealGreenGreen = 250;
constexpr int kHealGreenBlue = 220;

[[nodiscard]] float QuadArea(const ro::render::EffectWorldQuad& quad) {
  const glm::vec3 cote = quad.corners[1] - quad.corners[0];
  const glm::vec3 other = quad.corners[3] - quad.corners[0];
  return glm::length(glm::cross(cote, other));
}

}  // namespace

TEST_CASE("Soin : la compétence mène à un effet que trois tables servent", "[render][heal]") {
  const ro::render::ProceduralSkillEffect* shows =
      ro::render::DrawableProceduralEffectOfSkill(kSkillHeal);
  REQUIRE(shows != nullptr);
  REQUIRE(shows->effect_id == 325);
  // ⚠ LES DEUX FAMILLES SONT L'EFFET, pas l'une OU l'autre : l'anneau au pied et
  // les étincelles qui en montent viennent du même bâtisseur, et n'en montrer
  // qu'une donnerait un soin à moitié.
  CHECK_FALSE(ro::render::EffectSeededRingPatternsFor(325).empty());
  CHECK_FALSE(ro::render::EffectRisingSparkPatternsFor(325).empty());
  // Et il s'entend.
  CHECK(ro::render::KnowsProceduralEffect(325));
}

TEST_CASE("Soin : il sème SEIZE fois, pas deux", "[render][heal]") {
  // ⚠ UN EFFET N'EST PAS UN SEMIS MAIS UNE SUITE DE SEMIS, et c'est ce qui
  // manquait à Heal. Le client rappelle son bâtisseur à chaque tic : EF_HEAL4
  // ouvre son anneau au tic zéro et un SECOND au tic un, puis lâche DEUX suites
  // d'étincelles — l'une tous les neuf tics depuis le zéro, l'autre tous les six
  // depuis le premier, jusqu'au cinquantième. Deux anneaux et quatorze bouffées.
  //
  // Nous n'en jouions que deux — un anneau, une bouffée —, ce qui donnait un
  // soin bien trop pâle en vert et privé de son second anneau.
  //
  // ⚠ CE TEST A DIT NEUF, ET IL S'EST TROMPÉ PAR OÙ IL LISAIT. La cadence du
  // second nœud d'étincelles n'était pas relevée : l'horaire se cherchait dans
  // une fenêtre de vingt-six instructions, et le modulo qui la donne est à
  // quatorze cents octets de sa comparaison. Il en sortait un semis unique
  // « au tic un », là où le client ne sème rien avant le sixième.
  std::size_t rings = 0;
  std::size_t bouffees = 0;
  for (std::int32_t tick = 0; tick <= 60; ++tick) {
    for (const ro::render::EffectSeededRingPattern& pattern :
         ro::render::EffectSeededRingPatternsFor(325)) {
      if (ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
        ++rings;
      }
    }
    for (const ro::render::EffectRisingSparkPattern& pattern :
         ro::render::EffectRisingSparkPatternsFor(325)) {
      if (ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
        ++bouffees;
      }
    }
  }
  CHECK(rings == 2);
  CHECK(bouffees == 14);

  // ⚠ ET L'HORLOGE DOIT LES ATTENDRE. Un dernier tic qui s'arrêterait au
  // premier semis éteindrait le vert au milieu du soin ; la fenêtre borne son
  // horloge sur le plus tardif des trois familles.
  std::int32_t last = 0;
  for (const ro::render::EffectRisingSparkPattern& pattern :
       ro::render::EffectRisingSparkPatternsFor(325)) {
    last = std::max(last, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  CHECK(last == 50);
}

TEST_CASE("Soin : ses étincelles ont une AIRE et elles sont VERTES", "[render][heal]") {
  // ⚠ UN SEMIS JUSTE ET UNE FIGURE JUSTE PEUVENT NE RIEN MONTRER. Les étincelles
  // naissent à alpha nul et à taille tirée ; c'est le mouvement qui les lève.
  // Ce test le vérifie sur la vie du nœud, pas à sa naissance.
  const ro::render::EffectRisingSparkPattern& pattern =
      ro::render::EffectRisingSparkPatternsFor(325).front();
  std::uint32_t seed = 11U;
  ro::render::PooledEffectNode pooled =
      ro::render::SowEffectRisingSparkNode(pattern, glm::vec3(0.0F), seed);
  ro::render::EffectNodePool pool;
  pool.Add(pooled);

  std::size_t images_with_area = 0;
  bool verte = false;
  for (int tick = 0; tick < 40; ++tick) {
    pool.Advance(ro::render::kEffectTickMs);
    if (pool.size() == 0) {
      break;
    }
    const ro::render::PooledEffectNode& live = pool.nodes().front();
    const std::int16_t tour = live.tracks.front().style;
    const std::int16_t tilt = live.tracks[1].style;
    const std::uint8_t mark = live.tracks.front().mark[2];
    const std::uint8_t sub_mark = live.tracks.front().mark[3];
    for (const ro::render::EffectTrack& track : live.tracks) {
      const std::optional<ro::render::EffectWorldQuad> world_quad =
          ro::render::ComposeTintedRotatedQuad(track, mark, sub_mark, tour, tilt);
      if (!world_quad.has_value()) {
        continue;
      }
      if (QuadArea(*world_quad) > 0.0F && world_quad->tint[3] > 0) {
        ++images_with_area;
        // ⚠ UNE ÉTINCELLE S'AJOUTE, ELLE NE SE MÉLANGE PAS. Le client pousse
        // zéro au seizième paramètre de son traceur pour la marque onze, et sa
        // règle en fait le facteur UN — une ADDITION. Le mélange ordinaire
        // multipliait la teinte par une texture GRISE, ce qui rendait un gris à
        // halo blanc là où le client fait briller du vert. C'est le genre
        // d'écart qu'une capture ne dénonce pas toute seule.
        CHECK(world_quad->blend_destination == 2);
        CHECK(world_quad->blend_source == 5);
        verte = verte ||
                (world_quad->tint[0] == kHealGreenRed && world_quad->tint[1] == kHealGreenGreen &&
                 world_quad->tint[2] == kHealGreenBlue);
      }
    }
  }
  // Un carré qui n'a jamais d'aire visible est une figure qu'on croit portée.
  CHECK(images_with_area > 0);
  // ⚠ LE VERT N'EST NI DANS LA TEXTURE NI DANS LE SPRITE : `pok1.tga` est GRISE.
  // Il vient de la marque onze, et la perdre rendrait un soin blanc — ce qui
  // ressemble à un effet qui marche.
  CHECK(verte);
}

TEST_CASE("Soin : ses étincelles tiennent SOUS TOUS LES ANGLES DE CAMÉRA", "[render][heal]") {
  // ⚠ LA FENÊTRE POSE LES DEUX ANGLES SUR LA TROUPE AVANT DE L'AVANCER, et le
  // mouvement les range dans les pistes où la figure les relit. Un test qui
  // laisse les angles à zéro n'éprouve donc qu'UNE caméra — celle qu'aucun
  // joueur n'a. C'est la seule différence qui restait entre ce banc et le jeu.
  const ro::render::EffectRisingSparkPattern& pattern =
      ro::render::EffectRisingSparkPatternsFor(325).front();
  for (std::int16_t tour = -180; tour < 180; tour += 45) {
    for (std::int16_t tilt = 0; tilt <= 90; tilt += 30) {
      INFO("cap " << tour << ", inclinaison " << tilt);
      std::uint32_t seed = 11U;
      ro::render::EffectNodePool pool;
      pool.Add(ro::render::SowEffectRisingSparkNode(pattern, glm::vec3(0.0F), seed));
      std::size_t with_area = 0;
      for (int tick = 0; tick < 40 && pool.size() > 0; ++tick) {
        pool.FaceCamera(tour, tilt);
        pool.Advance(ro::render::kEffectTickMs);
        if (pool.size() == 0) {
          break;
        }
        const ro::render::PooledEffectNode& live = pool.nodes().front();
        for (const ro::render::EffectTrack& track : live.tracks) {
          const std::optional<ro::render::EffectWorldQuad> world_quad =
              ro::render::ComposeTintedRotatedQuad(track, live.tracks.front().mark[2],
                                                   live.tracks.front().mark[3],
                                                   live.tracks.front().style, live.tracks[1].style);
          if (world_quad.has_value() && QuadArea(*world_quad) > 0.0F && world_quad->tint[3] > 0) {
            ++with_area;
          }
        }
      }
      CHECK(with_area > 0);
    }
  }
}

TEST_CASE("Soin : son anneau tient AVEC LA CAMÉRA POSÉE", "[render][heal]") {
  // ⚠ LA FENÊTRE POSE LES ANGLES AVANT D'AVANCER, et le mouvement des groupes
  // radiaux LIT le cap. Un banc qui les laisse à zéro n'éprouve donc pas ce que
  // le jeu fait — et c'est la seule différence qui restait sur l'anneau.
  const ro::render::EffectSeededRingPattern& pattern =
      ro::render::EffectSeededRingPatternsFor(325).front();
  for (std::int16_t tour = -180; tour < 180; tour += 45) {
    INFO("cap " << tour);
    std::uint32_t seed = 4U;
    ro::render::EffectNodePool pool;
    pool.Add(ro::render::SowEffectSeededRingNode(pattern, glm::vec3(0.0F), seed,
                                                 ro::render::EffectNodeLifeFor(325)));
    std::size_t with_area = 0;
    for (int tick = 0; tick < 100 && pool.size() > 0; ++tick) {
      pool.FaceCamera(tour, 45);
      pool.Advance(ro::render::kEffectTickMs);
      if (pool.size() == 0) {
        break;
      }
      const ro::render::PooledEffectNode& live = pool.nodes().front();
      for (const ro::render::EffectTrack& track : live.tracks) {
        for (const ro::render::EffectWorldQuad& quad :
             ro::render::ComposeEffectTrackSkirt(track, live.trail_variant, live.ground_position)) {
          if (QuadArea(quad) > 0.0F && quad.tint[3] > 0) {
            ++with_area;
          }
        }
      }
    }
    CHECK(with_area > 0);
  }
}

TEST_CASE("Soin : son anneau rend des quads A CHAQUE TIC de sa vie", "[render][heal]") {
  // ⚠ COMPTER EN CUMUL LAISSE PASSER UNE FIGURE QUI NE PARAÎT QU'UN INSTANT, et
  // c'était le défaut des bancs voisins : un anneau qui rend quelque chose au
  // troisième tic puis plus rien les satisfaisait. On exige donc tic par tic,
  // et le premier tic muet nomme l'instant où la figure renonce.
  const ro::render::EffectSeededRingPattern& pattern =
      ro::render::EffectSeededRingPatternsFor(325).front();
  std::uint32_t seed = 4U;
  ro::render::EffectNodePool pool;
  pool.Add(ro::render::SowEffectSeededRingNode(pattern, glm::vec3(0.0F), seed,
                                               ro::render::EffectNodeLifeFor(325)));
  // ⚠ ET SA JUPE SE REFERME AVANT QUE LE NOEUD NE MEURE : mesuré, elle rend des
  // quads du premier tic au soixante-seizième, puis plus rien pendant que le
  // noeud finit la vie de son effet. Cette queue muette est NORMALE, et l'avoir prise
  // pour une panne m'a fait chercher au mauvais endroit.
  constexpr int kLastOpeningTick = 60;
  for (int tick = 1; tick <= kLastOpeningTick && pool.size() > 0; ++tick) {
    INFO("tic " << tick);
    pool.FaceCamera(30, 45);
    pool.Advance(ro::render::kEffectTickMs);
    if (pool.size() == 0) {
      break;
    }
    const ro::render::PooledEffectNode& live = pool.nodes().front();
    std::size_t here = 0;
    for (const ro::render::EffectTrack& track : live.tracks) {
      here += ro::render::ComposeEffectTrackSkirt(track, live.trail_variant, live.ground_position)
                  .size();
    }
    CHECK(here > 0);
  }
}

TEST_CASE("Soin : son ANNEAU s'ouvre au pied du soigné", "[render][heal]") {
  // ⚠ UN ANNEAU NAÎT SANS AIRE : ses longueurs de segment partent nulles, et
  // c'est le mouvement qui lui donne sa jupe. Le juger au tic du semis
  // conclurait qu'il ne se voit pas.
  const ro::render::EffectSeededRingPattern& pattern =
      ro::render::EffectSeededRingPatternsFor(325).front();
  std::uint32_t seed = 4U;
  ro::render::EffectNodePool pool;
  pool.Add(ro::render::SowEffectSeededRingNode(pattern, glm::vec3(0.0F), seed,
                                               ro::render::EffectNodeLifeFor(325)));

  std::size_t quads_with_area = 0;
  for (int tick = 0; tick < 60 && pool.size() > 0; ++tick) {
    pool.Advance(ro::render::kEffectTickMs);
    if (pool.size() == 0) {
      break;
    }
    const ro::render::PooledEffectNode& live = pool.nodes().front();
    for (const ro::render::EffectTrack& track : live.tracks) {
      for (const ro::render::EffectWorldQuad& quad :
           ro::render::ComposeEffectTrackSkirt(track, live.trail_variant, live.ground_position)) {
        if (QuadArea(quad) > 0.0F && quad.tint[3] > 0) {
          ++quads_with_area;
        }
      }
    }
  }
  CHECK(quads_with_area > 0);
}
