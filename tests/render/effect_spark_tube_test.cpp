#include "ro/render/effect_spark_tube.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <span>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spark_tube_seed.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"

using Catch::Approx;
using ro::render::AdvanceSparkTubeTracks;
using ro::render::ComposeSparkTubeQuads;
using ro::render::EffectNodeScheduleSowsAt;
using ro::render::EffectSparkTubeBuilder;
using ro::render::EffectSparkTubeColor;
using ro::render::EffectSparkTubePattern;
using ro::render::EffectSparkTubePatternsFor;
using ro::render::EffectSparkTubePatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kSparkTubeAlphaFadePerPoint;
using ro::render::kSparkTubeAngleCeilingDegrees;
using ro::render::kSparkTubeAngleStepDegrees;
using ro::render::kSparkTubeFaces;
using ro::render::kSparkTubeFadesAfterTick;
using ro::render::kSparkTubeHaloStyle;
using ro::render::kSparkTubePreRollTicks;
using ro::render::kSparkTubeTexture;
using ro::render::SeedEffectSparkTubeTracks;
using ro::render::SowEffectSparkTubeNode;

namespace {

// UNE PISTE COMME LE MOUVEMENT LA LAISSE après quelques tics : une courbe de
// trois points déjà ouverte.
[[nodiscard]] EffectTrack Deployed(std::int16_t style = 0, float points = 3.0F) {
  EffectTrack track;
  track.active = true;
  track.alpha = 200;
  track.ticks = 5;
  track.style = style;
  track.lift_key = 0.1F;
  track.radius_at_anchor = 6.0F;
  track.tilt_degrees = 0;
  track.tail[0] = 1.0F;
  track.tail[1] = points;
  track.anchor = {10.0F, 0.0F, 20.0F};
  for (std::size_t rang = 0; rang <= static_cast<std::size_t>(points); ++rang) {
    track.value[rang] = 60.0F - 6.0F * static_cast<float>(rang);
  }
  return track;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Sown(EffectSparkTubeBuilder batisseur,
                                                                 std::uint8_t octet,
                                                                 std::uint32_t seed = 9U,
                                                                 glm::vec3 target = {}) {
  EffectSparkTubePattern pattern;
  pattern.builder = batisseur;
  pattern.style = octet;
  return SeedEffectSparkTubeTracks(pattern, {3.0F, 0.0F, 4.0F}, target, seed);
}

}  // namespace

TEST_CASE("le tube a TROIS faces et se referme", "[render][effect][spark]") {
  // ⚠ TROIS FACES, ET QUATRE PAS : le dernier retombe sur le premier. Trois pas
  // laisseraient une fente sur toute la longueur du tube.
  const std::vector<EffectWorldQuad> quads = ComposeSparkTubeQuads(Deployed());
  REQUIRE(quads.size() == kSparkTubeFaces * 3);

  // Les trois faces d'un tronçon se referment : l'arête de SORTIE de la
  // dernière est l'arête d'ENTRÉE de la première — le pas de trois cent
  // soixante retombe sur celui de zéro.
  CHECK(quads[2].corners[0].x == Approx(quads[0].corners[3].x).margin(1e-4F));
  CHECK(quads[2].corners[0].y == Approx(quads[0].corners[3].y).margin(1e-4F));
  CHECK(quads[2].corners[0].z == Approx(quads[0].corners[3].z).margin(1e-4F));
}

TEST_CASE("le tube ne rend rien tant que son compteur est negatif", "[render][effect][spark]") {
  // ⚠ C'EST LE RETARD que le bâtisseur a tiré. L'oublier ferait paraître les
  // quatre étincelles ensemble.
  EffectTrack track = Deployed();
  track.ticks = -3;
  CHECK(ComposeSparkTubeQuads(track).empty());
  track.ticks = 0;
  CHECK(ComposeSparkTubeQuads(track).empty());
  track = Deployed();
  track.active = false;
  CHECK(ComposeSparkTubeQuads(track).empty());
  track = Deployed();
  track.alpha = 0;
  CHECK(ComposeSparkTubeQuads(track).empty());
}

TEST_CASE("la courbe s'ELOIGNE et RETOMBE", "[render][effect][spark]") {
  // ⚠ LA DISTANCE SUIT L'ANGLE, LA HAUTEUR SON SINUS. Prendre le sinus pour les
  // deux ramènerait l'étincelle à son point de départ.
  EffectTrack track = Deployed();
  track.tilt_degrees = 0;
  // Un point à quatre-vingt-dix degrés est au plus haut ; à cent quatre-vingts,
  // il est revenu à la hauteur du sol mais DEUX fois plus loin.
  const auto reach_and_height = [&track](float angle) {
    for (std::size_t rang = 0; rang < 4; ++rang) {
      track.value[rang] = angle;
    }
    const std::vector<EffectWorldQuad> quads = ComposeSparkTubeQuads(track);
    REQUIRE_FALSE(quads.empty());
    // Le centre du premier tronçon, moins l'ancre.
    const glm::vec3 corner = quads.front().corners[0] - track.anchor;
    return std::pair<float, float>{corner.x, corner.y};
  };
  const auto [far_90, high_90] = reach_and_height(90.0F);
  const auto [far_180, high_180] = reach_and_height(180.0F);
  CHECK(far_180 == Approx(2.0F * far_90).margin(0.01F));
  CHECK(high_90 > high_180);
  // ⚠ NOTRE REPÈRE MONTE : à quatre-vingt-dix degrés l'étincelle est AU-DESSUS
  // de son ancre. Recopier le signe du client l'enfoncerait.
  CHECK(high_90 > 0.0F);
}

TEST_CASE("l'alpha tombe d'un tronçon au suivant", "[render][effect][spark]") {
  // ⚠ C'EST CE QUI FAIT LA TRAÎNÉE : la tête est pleine, la queue s'efface.
  const std::vector<EffectWorldQuad> quads = ComposeSparkTubeQuads(Deployed());
  REQUIRE(quads.size() == kSparkTubeFaces * 3);
  CHECK(quads[0].tint[3] == 200);
  CHECK(quads[kSparkTubeFaces].tint[3] == 200 - kSparkTubeAlphaFadePerPoint);
  CHECK(quads[2 * kSparkTubeFaces].tint[3] == 200 - 2 * kSparkTubeAlphaFadePerPoint);
}

TEST_CASE("un tronçon dont l'alpha est epuise ne sort pas", "[render][effect][spark]") {
  // Le client compare l'alpha DIMINUÉ à zéro, tronçon par tronçon : une longue
  // courbe à faible alpha s'arrête en chemin.
  EffectTrack track = Deployed(0, 5.0F);
  track.alpha = 15;
  const std::vector<EffectWorldQuad> quads = ComposeSparkTubeQuads(track);
  CHECK(quads.size() == kSparkTubeFaces * 2);
}

TEST_CASE("la teinte change le long de la courbe", "[render][effect][spark]") {
  // ⚠ LA SECONDE MOITIÉ DE LA TRAÎNÉE : tête et queue n'ont pas la même
  // couleur, et chaque style la fait varier sur un canal différent.
  CHECK(EffectSparkTubeColor(0, 0) == std::array<std::uint8_t, 3>{255, 200, 0});
  CHECK(EffectSparkTubeColor(0, 2) == std::array<std::uint8_t, 3>{255, 200, 60});
  CHECK(EffectSparkTubeColor(1, 0) == std::array<std::uint8_t, 3>{10, 10, 255});
  CHECK(EffectSparkTubeColor(1, 2) == std::array<std::uint8_t, 3>{54, 54, 255});
  CHECK(EffectSparkTubeColor(2, 2) == std::array<std::uint8_t, 3>{255, 200, 100});
  CHECK(EffectSparkTubeColor(3, 0) == std::array<std::uint8_t, 3>{105, 105, 105});
  CHECK(EffectSparkTubeColor(9, 1) == std::array<std::uint8_t, 3>{135, 135, 135});
}

TEST_CASE("seul un style porte un second tube", "[render][effect][spark]") {
  // ⚠ LE CLIENT OUVRE SA BOUCLE POUR TOUS LES STYLES À PARTIR DE TROIS, puis
  // n'émet que pour le trois EXACTEMENT : le quatre calcule et ne montre rien.
  const std::vector<EffectWorldQuad> sans = ComposeSparkTubeQuads(Deployed(1));
  CHECK(sans.size() == kSparkTubeFaces * 3);
  const std::vector<EffectWorldQuad> with = ComposeSparkTubeQuads(Deployed(kSparkTubeHaloStyle));
  CHECK(with.size() == 2 * kSparkTubeFaces * 3);
  const std::vector<EffectWorldQuad> au_dela = ComposeSparkTubeQuads(Deployed(4));
  CHECK(au_dela.size() == kSparkTubeFaces * 3);

  // Le second tube est BLEU, à la moitié de l'alpha, et QUATRE fois plus large.
  CHECK(with[kSparkTubeFaces].tint[0] == 0);
  CHECK(with[kSparkTubeFaces].tint[1] == 0);
  CHECK(with[kSparkTubeFaces].tint[2] == 255);
  CHECK(with[kSparkTubeFaces].tint[3] == with[0].tint[3] / 2);
}

TEST_CASE("les coins du tube sont ABSOLUS", "[render][effect][spark]") {
  // ⚠ L'ANCRE DE LA PISTE EST UN POINT DU MONDE. Y ajouter l'origine du nœud
  // doublerait la position — et le client passe bien le drapeau d'absolu.
  EffectTrack far_point = Deployed();
  far_point.anchor = {100.0F, 0.0F, 200.0F};
  const std::vector<EffectWorldQuad> here = ComposeSparkTubeQuads(Deployed());
  const std::vector<EffectWorldQuad> over_there = ComposeSparkTubeQuads(far_point);
  REQUIRE(here.size() == over_there.size());
  CHECK(over_there[0].corners[0].x == Approx(here[0].corners[0].x + 90.0F));
  CHECK(over_there[0].corners[0].z == Approx(here[0].corners[0].z + 180.0F));
}

TEST_CASE("le mouvement fait GLISSER les cases", "[render][effect][spark]") {
  // ⚠ C'EST UN TAMPON CIRCULAIRE, ET C'EST TOUTE LA FIGURE. Sans le glissement,
  // les points de la courbe seraient tous égaux et le tube aurait une longueur
  // nulle.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = 0;
  tracks[0].tail[1] = 3.0F;
  AdvanceSparkTubeTracks(tracks, {1.0F, 2.0F, 3.0F});
  CHECK(tracks[0].value[0] == Approx(kSparkTubeAngleStepDegrees));
  CHECK(tracks[0].value[1] == Approx(kSparkTubeAngleStepDegrees));
  AdvanceSparkTubeTracks(tracks, {1.0F, 2.0F, 3.0F});
  CHECK(tracks[0].value[0] == Approx(2.0F * kSparkTubeAngleStepDegrees));
  CHECK(tracks[0].value[1] == Approx(2.0F * kSparkTubeAngleStepDegrees));
  CHECK(tracks[0].value[2] == Approx(kSparkTubeAngleStepDegrees));
  // ⚠ LA QUEUE MET UN TIC DE PLUS À SE REMPLIR QU'ON NE CROIT : le glissement
  // recopie la case zéro APRÈS l'avoir avancée, si bien que les deux premières
  // sont toujours égales et que la courbe n'a sa longueur qu'au tic suivant.
  CHECK(tracks[0].value[3] == Approx(0.0F));
  AdvanceSparkTubeTracks(tracks, {1.0F, 2.0F, 3.0F});
  CHECK(tracks[0].value[3] == Approx(kSparkTubeAngleStepDegrees));
  // ⚠ ET L'ANCRE SUIT LE PORTEUR, piste par piste : le dessinateur pose ses
  // coins en absolu, et sans cette recopie l'étincelle resterait à l'origine.
  CHECK(tracks[0].anchor.x == Approx(1.0F));
  CHECK(tracks[0].anchor.z == Approx(3.0F));
}

TEST_CASE("le mouvement ne fait rien tant que le retard court", "[render][effect][spark]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = -3;
  tracks[0].alpha = 200;
  tracks[0].tail[1] = 3.0F;
  AdvanceSparkTubeTracks(tracks, {5.0F, 0.0F, 5.0F});
  CHECK(tracks[0].ticks == -2);
  CHECK(tracks[0].value[0] == Approx(0.0F));
  // ⚠ IL NE SUIT MÊME PAS SON PORTEUR tant que le retard court.
  CHECK(tracks[0].anchor.x == Approx(0.0F));
}

TEST_CASE("l'angle plafonne et l'alpha finit par tomber", "[render][effect][spark]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = 0;
  tracks[0].alpha = 200;
  tracks[0].tail[1] = 3.0F;
  for (std::int32_t tick = 0; tick < 40; ++tick) {
    AdvanceSparkTubeTracks(tracks, {});
  }
  // ⚠ L'ANGLE PLAFONNE À CENT QUATRE-VINGTS : au-delà, le sinus redeviendrait
  // positif et l'étincelle repartirait vers le haut.
  CHECK(tracks[0].value[0] == Approx(kSparkTubeAngleCeilingDegrees));
  CHECK(tracks[0].ticks == 40);
  CHECK(tracks[0].alpha < 200);
  CHECK(tracks[0].alpha == 200 - 5 * (40 - kSparkTubeFadesAfterTick));
}

TEST_CASE("les quatre pistes partent en decale", "[render][effect][spark]") {
  // ⚠ LE RETARD EST TIRÉ POUR CHAQUE PISTE. Les semer à zéro les ferait partir
  // ensemble, et la rafale deviendrait une gerbe.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Sown(EffectSparkTubeBuilder::kRising, 3);
  std::set<std::int32_t> retards;
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.ticks <= -30);
    CHECK(track.ticks >= -70);
    retards.insert(track.ticks);
  }
  CHECK(retards.size() > 1);
}

TEST_CASE("un seul cas nait deja deploye", "[render][effect][spark]") {
  // ⚠ LE PREMIER BÂTISSEUR JOUE DIX TICS D'AVANCE pour son octet UN. Ne pas le
  // faire donnerait à EF_STORMKICK un départ que le client ne montre pas.
  const EffectTrack brief = Sown(EffectSparkTubeBuilder::kRising, 1)[0];
  CHECK(brief.value[0] ==
        Approx(kSparkTubeAngleStepDegrees * static_cast<float>(kSparkTubePreRollTicks)));
  CHECK(brief.value[1] > 0.0F);
  CHECK(brief.tail[1] == Approx(3.0F));

  const EffectTrack ordinary = Sown(EffectSparkTubeBuilder::kRising, 3)[0];
  CHECK(ordinary.value[0] == Approx(0.0F));
  CHECK(ordinary.tail[1] == Approx(4.0F));
}

TEST_CASE("l'etincelle dirigee vise a l'ENVERS", "[render][effect][spark]") {
  // ⚠ LE CLIENT SOUSTRAIT LA CIBLE DE LA POSITION : le cap S'ÉLOIGNE d'elle.
  // Prendre l'autre sens la ferait partir vers le coup au lieu d'en jaillir.
  const std::array<EffectTrack, kEffectTracksPerNode> vers_l_est =
      Sown(EffectSparkTubeBuilder::kDirected, 3, 9U, {10.0F, 0.0F, 0.0F});
  for (const EffectTrack& track : vers_l_est) {
    // Le cap de fuite est l'ouest — cent quatre-vingts —, à soixante degrés
    // près.
    const std::int32_t offset = std::abs(static_cast<std::int32_t>(track.tilt_degrees) - 180);
    CHECK(offset <= 60);
  }
  // Et sa courbe n'a que trois points, là où le bâtisseur tardif en a quatre.
  CHECK(vers_l_est[0].tail[1] == Approx(3.0F));
  CHECK(Sown(EffectSparkTubeBuilder::kRisingLate, 3)[0].tail[1] == Approx(4.0F));
}

TEST_CASE("le cap de la piste est celui du client", "[render][effect][spark]") {
  // ⚠ UN CAP DE QUATRE-VINGT-DIX porte la courbe vers la profondeur POSITIVE du
  // client, donc NÉGATIVE chez nous : la garder positive tracerait l'étincelle
  // en miroir.
  EffectTrack track = Deployed();
  track.tilt_degrees = 90;
  const std::vector<EffectWorldQuad> quads = ComposeSparkTubeQuads(track);
  REQUIRE_FALSE(quads.empty());
  CHECK(quads.front().corners[0].z < track.anchor.z);
}

TEST_CASE("l'etincelle nait deja visible", "[render][effect][spark]") {
  // ⚠ CENT VINGT-CINQ AU MINIMUM, contrairement à presque toute la famille : ce
  // mouvement ne sait que faire TOMBER l'alpha, jamais le monter. La semer à
  // zéro laisserait une étincelle invisible jusqu'à sa mort.
  for (std::uint32_t seed = 1U; seed < 30U; ++seed) {
    for (const EffectTrack& track : Sown(EffectSparkTubeBuilder::kRisingLate, 3, seed)) {
      CHECK(track.alpha >= 125);
      CHECK(track.alpha <= 245);
    }
  }
}

TEST_CASE("les fenetres de semis different par batisseur", "[render][effect][spark]") {
  // ⚠ ELLE VIT DANS LE BÂTISSEUR, PAS DANS LE SITE, et l'un d'eux ne commence
  // qu'au SIXIÈME tic.
  const std::span<const EffectSparkTubePattern> table = EffectSparkTubePatternTable();
  REQUIRE(table.size() == 4);
  CHECK(std::ranges::is_sorted(table, {}, &EffectSparkTubePattern::effect_id));

  REQUIRE_FALSE(EffectSparkTubePatternsFor(429).empty());
  const EffectSparkTubePattern& tardif = EffectSparkTubePatternsFor(429).front();
  CHECK_FALSE(EffectNodeScheduleSowsAt(tardif.schedule, 0));
  CHECK(EffectNodeScheduleSowsAt(tardif.schedule, 6));
  CHECK(EffectNodeScheduleSowsAt(tardif.schedule, 9));
  CHECK_FALSE(EffectNodeScheduleSowsAt(tardif.schedule, 10));

  REQUIRE_FALSE(EffectSparkTubePatternsFor(434).empty());
  const EffectSparkTubePattern& court = EffectSparkTubePatternsFor(434).front();
  CHECK(EffectNodeScheduleSowsAt(court.schedule, 0));
  CHECK(EffectNodeScheduleSowsAt(court.schedule, 1));
  CHECK_FALSE(EffectNodeScheduleSowsAt(court.schedule, 2));

  // ⚠ ET L'IDENTIFIANT 428 EST ABSENT, parce que le client ne le montre pas :
  // son octet zéro donne une fenêtre VIDE.
  CHECK(EffectSparkTubePatternsFor(428).empty());
}

TEST_CASE("un noeud d'etincelle porte l'image ecrite en dur", "[render][effect][spark]") {
  EffectSparkTubePattern pattern;
  pattern.builder = EffectSparkTubeBuilder::kRising;
  pattern.style = 3;
  std::uint32_t seed = 2U;
  const ro::render::PooledEffectNode node =
      SowEffectSparkTubeNode(pattern, {7.0F, 0.0F, 8.0F}, {}, 100, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kSparkTube);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths[0] == kSparkTubeTexture);
  // ⚠ L'ANCRE DE CHAQUE PISTE EST LE POINT DU MONDE, pas zéro.
  CHECK(node.tracks[0].anchor.x == Approx(7.0F));
  CHECK(node.tracks[3].anchor.z == Approx(8.0F));
}
