#include "ro/render/effect_swarm_seed.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_oriented_pair.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceBillboardSwarmTracks;
using ro::render::ComposeOrientedPairQuads;
using ro::render::EffectNodeScheduleSowsAt;
using ro::render::EffectOrientedPairRequestsOf;
using ro::render::EffectSwarmHeadingDegrees;
using ro::render::EffectSwarmKind;
using ro::render::EffectSwarmPattern;
using ro::render::EffectSwarmPatternsFor;
using ro::render::EffectSwarmPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectNarrowSwarm;
using ro::render::kEffectSwarmSchedule;
using ro::render::kEffectTracksPerNode;
using ro::render::kSwarmBirthHeight;
using ro::render::kSwarmLevelDegrees;
using ro::render::kSwarmNarrowHeadingBias;
using ro::render::kSwarmNarrowSpeedFloor;
using ro::render::kSwarmRadiusFloor;
using ro::render::kSwarmSmallRadiusFloor;
using ro::render::kSwarmSmallRadiusMark;
using ro::render::kSwarmWideSpeedFloor;
using ro::render::SeedEffectSwarmTracks;
using ro::render::SowEffectSwarmNode;

namespace {

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Sown(
    EffectSwarmKind genre, std::uint8_t mark = 0, glm::vec3 target = {1.0F, 0.0F, 0.0F},
    std::uint32_t seed = 7U) {
  EffectSwarmPattern pattern;
  pattern.kind = genre;
  pattern.mark = mark;
  return SeedEffectSwarmTracks(pattern, target, seed);
}

}  // namespace

TEST_CASE("l'essaim seme TROIS bouffees a dix tics d'intervalle", "[render][effect][swarm]") {
  // ⚠ LE CLIENT RAPPELLE SON BÂTISSEUR À CHAQUE TIC et le garde derrière un
  // modulo : trois bouffées, aux tics zéro, dix et vingt. Tout semer au premier
  // tic donnerait un tiers de la durée et trois fois la densité.
  CHECK(EffectNodeScheduleSowsAt(kEffectSwarmSchedule, 0));
  CHECK(EffectNodeScheduleSowsAt(kEffectSwarmSchedule, 10));
  CHECK(EffectNodeScheduleSowsAt(kEffectSwarmSchedule, 20));
  CHECK_FALSE(EffectNodeScheduleSowsAt(kEffectSwarmSchedule, 1));
  CHECK_FALSE(EffectNodeScheduleSowsAt(kEffectSwarmSchedule, 9));
  CHECK_FALSE(EffectNodeScheduleSowsAt(kEffectSwarmSchedule, 30));
}

TEST_CASE("l'etroit allume UNE piste, le large QUATRE", "[render][effect][swarm]") {
  // ⚠ C'EST LA DIFFÉRENCE LA PLUS VISIBLE DES DEUX BÂTISSEURS : un nœud large
  // porte quatre essaims indépendants. En allumer une seule donnerait le quart
  // de la matière ; en allumer quatre à l'étroit la quadruplerait.
  const std::array<EffectTrack, kEffectTracksPerNode> narrow = Sown(EffectSwarmKind::kNarrow);
  CHECK(narrow[0].active);
  CHECK_FALSE(narrow[1].active);
  CHECK_FALSE(narrow[3].active);

  const std::array<EffectTrack, kEffectTracksPerNode> large = Sown(EffectSwarmKind::kWide);
  for (const EffectTrack& track : large) {
    CHECK(track.active);
  }
}

TEST_CASE("les quatre pistes du large ne tirent pas les memes nombres", "[render][effect][swarm]") {
  // ⚠ CHACUNE TIRE POUR ELLE-MÊME. Leur donner les mêmes nombres ferait quatre
  // essaims superposés, c'est-à-dire un seul quatre fois plus opaque — et cela
  // ressemblerait à une figure trop lumineuse, pas à un semis raté.
  const std::array<EffectTrack, kEffectTracksPerNode> large = Sown(EffectSwarmKind::kWide);
  std::set<float> vitesses;
  std::set<float> caps;
  for (const EffectTrack& track : large) {
    vitesses.insert(track.lift_key);
    caps.insert(track.value[2]);
  }
  CHECK(caps.size() > 1);
  CHECK(vitesses.size() > 1);
}

TEST_CASE("les deux vitesses ne sont pas du meme ordre", "[render][effect][swarm]") {
  // ⚠ L'ÉTROIT TIRE PAR CENTIÈMES, LE LARGE PAR MILLIÈMES : l'un varie d'un
  // quart, l'autre d'un vingtième, et le large va deux fois moins vite.
  for (std::uint32_t seed = 1U; seed < 40U; ++seed) {
    const float etroite = Sown(EffectSwarmKind::kNarrow, 0, {1.0F, 0.0F, 0.0F}, seed)[0].lift_key;
    const float width = Sown(EffectSwarmKind::kWide, 0, {1.0F, 0.0F, 0.0F}, seed)[0].lift_key;
    CHECK(etroite >= kSwarmNarrowSpeedFloor);
    CHECK(etroite <= Approx(1.0F));
    CHECK(width >= kSwarmWideSpeedFloor);
    CHECK(width <= Approx(0.42F));
  }
}

TEST_CASE("seule la marque trois ouvre le petit rayon", "[render][effect][swarm]") {
  // ⚠ C'EST LA SEULE PLACE DU SEMIS OÙ L'OCTET DU SITE DÉCIDE D'AUTRE CHOSE QUE
  // D'UNE TEINTE, et seul le large la regarde. La donner à l'étroit
  // rapetisserait des carrés que le client garde grands.
  bool seen_small = false;
  for (std::uint32_t seed = 1U; seed < 60U; ++seed) {
    const float little =
        Sown(EffectSwarmKind::kWide, kSwarmSmallRadiusMark, {1.0F, 0.0F, 0.0F}, seed)[0]
            .radius_at_anchor;
    CHECK(little >= kSwarmSmallRadiusFloor);
    CHECK(little <= Approx(5.0F));
    seen_small = seen_small || little < kSwarmRadiusFloor;

    const float ordinary =
        Sown(EffectSwarmKind::kWide, 0, {1.0F, 0.0F, 0.0F}, seed)[0].radius_at_anchor;
    CHECK(ordinary >= kSwarmRadiusFloor);
    const float etroite =
        Sown(EffectSwarmKind::kNarrow, kSwarmSmallRadiusMark, {1.0F, 0.0F, 0.0F}, seed)[0]
            .radius_at_anchor;
    CHECK(etroite >= kSwarmRadiusFloor);
  }
  // Et le petit rayon descend VRAIMENT sous le plancher ordinaire.
  CHECK(seen_small);
}

TEST_CASE("le cap vers la cible se range dans l'INCLINAISON", "[render][effect][swarm]") {
  // ⚠ ADR 0017 EN PLEINE LUMIÈRE : le champ s'appelle `tilt_degrees` parce
  // qu'un anneau y lit une inclinaison, et cet essaim y met la direction du
  // sort. Tous ses caps tirés s'en écartent.
  const std::array<EffectTrack, kEffectTracksPerNode> vers_l_est =
      Sown(EffectSwarmKind::kNarrow, 0, {10.0F, 0.0F, 0.0F});
  CHECK(vers_l_est[0].tilt_degrees == 0);
  CHECK(vers_l_est[0].value[0] == Approx(0.0F));

  // ⚠ LE CAP EST CELUI DU CLIENT : notre profondeur positive est la sienne
  // négative, soit trois quarts de tour.
  const std::array<EffectTrack, kEffectTracksPerNode> vers_le_sud =
      Sown(EffectSwarmKind::kNarrow, 0, {0.0F, 0.0F, 10.0F});
  CHECK(vers_le_sud[0].tilt_degrees == 270);
  CHECK(vers_le_sud[0].value[0] == Approx(270.0F));
  CHECK(vers_le_sud[0].value[5] == Approx(270.0F));
}

TEST_CASE("le cap d'un vecteur suit la table du client", "[render][effect][swarm]") {
  // ⚠ LE CLIENT NE PREND PAS UN ARC TANGENTE : il balaie sa table de cosinus et
  // s'arrête au premier degré qui passe dessous. Le résultat est arrondi VERS
  // LE HAUT, et un `atan2` arrondi autrement décalerait tous les caps semés.
  CHECK(EffectSwarmHeadingDegrees(1.0F, 0.0F) == 0);
  CHECK(EffectSwarmHeadingDegrees(0.0F, 1.0F) == 90);
  CHECK(EffectSwarmHeadingDegrees(-1.0F, 0.0F) == 180);
  CHECK(EffectSwarmHeadingDegrees(0.0F, -1.0F) == 270);
  // Un angle qui tombe entre deux degrés monte au suivant.
  CHECK(EffectSwarmHeadingDegrees(std::cos(0.55F), std::sin(0.55F)) == 32);
  // ⚠ ET UN VECTEUR NUL REND ZÉRO. C'est mesuré et non choisi : la division du
  // client donne un NaN, toutes ses comparaisons sont fausses, et sa boucle
  // sort au premier degré.
  CHECK(EffectSwarmHeadingDegrees(0.0F, 0.0F) == 0);
}

TEST_CASE("les caps tires restent a quarante-cinq degres de la cible", "[render][effect][swarm]") {
  // ⚠ C'EST CE QUE « ÉTROIT » VEUT DIRE, et cela se voit au SEMIS autant qu'au
  // mouvement : quatre-vingt-onze valeurs centrées sur la direction du sort.
  //
  // ⚠ CETTE DIRECTION EST UN CAP DU CLIENT : une cible à notre profondeur
  // positive est à la sienne négative, soit trois quarts de tour.
  for (std::uint32_t seed = 1U; seed < 50U; ++seed) {
    const EffectTrack track = Sown(EffectSwarmKind::kNarrow, 0, {0.0F, 0.0F, 10.0F}, seed)[0];
    for (const std::size_t rang :
         {std::size_t{1}, std::size_t{2}, std::size_t{6}, std::size_t{7}}) {
      const float offset = track.value[rang] - 270.0F;
      CHECK(std::abs(offset) <= static_cast<float>(kSwarmNarrowHeadingBias));
    }
  }
}

TEST_CASE("l'essaim file vers sa cible dans notre repere", "[render][effect][swarm]") {
  // 🔴 LE CAP SE TIRE DANS LE REPÈRE DU CLIENT ET LE PAS REVIENT DANS LE NÔTRE
  // par `S` : une cible à notre profondeur NÉGATIVE attire les deux positions
  // vers les profondeurs négatives. Le pas recopié sans être nié les envoyait
  // à l'opposé.
  for (std::uint32_t seed = 1U; seed < 20U; ++seed) {
    std::array<EffectTrack, kEffectTracksPerNode> tracks =
        Sown(EffectSwarmKind::kNarrow, 0, {0.0F, 0.0F, -10.0F}, seed);
    std::uint32_t generator = seed;
    AdvanceBillboardSwarmTracks(tracks, kEffectNarrowSwarm, 0, 0, generator);
    CHECK(tracks[0].tail[5] < 0.0F);
    CHECK(tracks[0].anchor.z < 0.0F);
  }
}

TEST_CASE("les quatre inclinaisons partent a l'horizontale", "[render][effect][swarm]") {
  // ⚠ CENT QUATRE-VINGTS DEGRÉS, ET NON ZÉRO : le mouvement tire ses cibles
  // neuves entre cent soixante-quinze et cent quatre-vingt-cinq. Les semer à
  // zéro ferait partir l'essaim vers le haut et ne jamais revenir.
  const EffectTrack track = Sown(EffectSwarmKind::kNarrow)[0];
  CHECK(track.value[3] == Approx(kSwarmLevelDegrees));
  CHECK(track.value[4] == Approx(kSwarmLevelDegrees));
  CHECK(track.value[8] == Approx(kSwarmLevelDegrees));
  CHECK(track.value[9] == Approx(kSwarmLevelDegrees));
}

TEST_CASE("les deux positions naissent AU-DESSUS du noeud", "[render][effect][swarm]") {
  // ⚠ LE CLIENT RETRANCHE CINQ, ET SON REPÈRE DESCEND : recopier le signe
  // enterrerait l'essaim.
  const EffectTrack track = Sown(EffectSwarmKind::kNarrow)[0];
  CHECK(track.tail[4] == Approx(kSwarmBirthHeight));
  CHECK(track.anchor.y == Approx(kSwarmBirthHeight));
  CHECK(track.tail[3] == Approx(0.0F));
  CHECK(track.anchor.x == Approx(0.0F));
}

TEST_CASE("les deux alphas naissent a zero et le mouvement les monte", "[render][effect][swarm]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = Sown(EffectSwarmKind::kNarrow);
  CHECK(tracks[0].value[12] == Approx(0.0F));
  CHECK(tracks[0].value[13] == Approx(0.0F));
  std::uint32_t seed = 3U;
  AdvanceBillboardSwarmTracks(tracks, kEffectNarrowSwarm, 0, 0, seed);
  CHECK(tracks[0].value[12] > 0.0F);
  CHECK(tracks[0].value[13] > 0.0F);
}

TEST_CASE("une piste semee finit par rendre des quads", "[render][effect][swarm]") {
  // LE SEMIS ET LA FIGURE SE RENCONTRENT ICI : sans alpha, rien ne sort ; après
  // un tic de mouvement, les deux positions paraissent.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = Sown(EffectSwarmKind::kNarrow);
  std::size_t before = 0;
  for (const auto& request : EffectOrientedPairRequestsOf(tracks[0], 0, 0)) {
    before += ComposeOrientedPairQuads(request, {}).size();
  }
  CHECK(before == 0);

  std::uint32_t seed = 3U;
  AdvanceBillboardSwarmTracks(tracks, kEffectNarrowSwarm, 0, 0, seed);
  std::size_t after = 0;
  for (const auto& request : EffectOrientedPairRequestsOf(tracks[0], 0, 0)) {
    after += ComposeOrientedPairQuads(request, {}).size();
  }
  CHECK(after == 4);
}

TEST_CASE("la table des essaims est triee et compte ses appels", "[render][effect][swarm]") {
  const std::span<const EffectSwarmPattern> table = EffectSwarmPatternTable();
  REQUIRE(table.size() == 18);
  CHECK(std::ranges::is_sorted(table, {}, &EffectSwarmPattern::effect_id));

  // ⚠ QUATRE APPELS POUR 969, ET CE N'EST PAS UNE BASCULE : le site appelle
  // quatre fois de suite en alternant deux images. N'en garder que deux
  // donnerait la moitié de l'essaim.
  CHECK(EffectSwarmPatternsFor(969).size() == 4);
  CHECK(EffectSwarmPatternsFor(969).front().kind == EffectSwarmKind::kWide);
  CHECK(EffectSwarmPatternsFor(854).size() == 2);
  CHECK(EffectSwarmPatternsFor(854).front().kind == EffectSwarmKind::kNarrow);
  // ⚠ SIX POUR 1584 : trois images, chacune sous DEUX dossiers que la bascule
  // de réglage départage. La fenêtre écarte celle qui ne se lit pas.
  CHECK(EffectSwarmPatternsFor(1584).size() == 6);
  CHECK(EffectSwarmPatternsFor(1594).size() == 4);
  CHECK(EffectSwarmPatternsFor(1).empty());
}

TEST_CASE("un noeud d'essaim porte UNE image et sa forme", "[render][effect][swarm]") {
  EffectSwarmPattern pattern;
  pattern.texture = "data\\texture\\effect\\melody_a.bmp";
  pattern.kind = EffectSwarmKind::kWide;
  std::uint32_t seed = 5U;
  const ro::render::PooledEffectNode node =
      SowEffectSwarmNode(pattern, {2.0F, 0.0F, 3.0F}, {}, 100, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kBillboardSwarm);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths[0] == "data\\texture\\effect\\melody_a.bmp");
  CHECK(node.owner_position.x == Approx(2.0F));
}
