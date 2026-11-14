#include <set>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_ground_disc_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceGroundDiscTracks;
using ro::render::EffectGroundDiscPattern;
using ro::render::EffectGroundDiscPatternsFor;
using ro::render::EffectGroundDiscPatternTable;
using ro::render::EffectGroundDiscSeedFor;
using ro::render::EffectTrack;
using ro::render::kEffectTrackFullTurnDegrees;
using ro::render::kEffectTracksPerNode;
using ro::render::kSlotOfGroundDiscHeight;
using ro::render::kSlotOfGroundDiscVariant;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectGroundDiscTracks;
using ro::render::SowEffectGroundDiscNode;

namespace {

// L'OCTET QUE LE CERCLE MAGIQUE POSE AU MILIEU, et le seul qui contre-tourne.
constexpr std::uint8_t kCounterRotatingByte = 14;
constexpr std::uint32_t kMapMagicCircle = 687;

[[nodiscard]] EffectTrack TrackFromByteValue(std::uint8_t octet) {
  const EffectGroundDiscPattern pattern{kMapMagicCircle, "Effect_Build_GroundDiscFromParam1a",
                                        "data\\texture\\effect\\test.bmp", octet};
  return SeedEffectGroundDiscTracks(pattern).front();
}

}  // namespace

TEST_CASE("la table des disques au sol porte une rangee par appel", "[render][ground_disc]") {
  const std::span<const EffectGroundDiscPattern> table = EffectGroundDiscPatternTable();
  REQUIRE_FALSE(table.empty());

  // ⚠ TRIÉE PAR IDENTIFIANT, parce que la recherche est dichotomique. Un jour où
  // le générateur cesserait de trier, la recherche rendrait un span vide sans
  // que rien ne le dise.
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }

  // LE CERCLE MAGIQUE EN POSE TROIS, et deux d'entre eux partagent leur octet :
  // c'est la preuve qu'une rangée vaut un APPEL et non un identifiant.
  const std::span<const EffectGroundDiscPattern> circle =
      EffectGroundDiscPatternsFor(kMapMagicCircle);
  REQUIRE(circle.size() == 3);
  std::set<std::string_view> images;
  for (const EffectGroundDiscPattern& disque : circle) {
    images.insert(disque.texture);
  }
  CHECK(images.size() == 3);
  CHECK(circle[1].builder_argument == kCounterRotatingByte);
  CHECK(circle[0].builder_argument == circle[2].builder_argument);
}

TEST_CASE("l'octet decide de cinq valeurs a la fois", "[render][ground_disc]") {
  // LES DEUX OCTETS DU CERCLE MAGIQUE partagent tout sauf leur variante : c'est
  // le mouvement, et lui seul, qui les distingue.
  const auto four = EffectGroundDiscSeedFor(4);
  const auto quatorze = EffectGroundDiscSeedFor(kCounterRotatingByte);
  CHECK(four.node_style == quatorze.node_style);
  CHECK(four.radius_at_anchor == Approx(quatorze.radius_at_anchor));
  CHECK(four.alpha == quatorze.alpha);

  // ⚠ LE RAYON DE L'INSTANCE NE SE LIT QUE POUR CES DEUX OCTETS-LÀ, et seulement
  // s'il est STRICTEMENT positif.
  CHECK(EffectGroundDiscSeedFor(4, 12.5F).radius_at_anchor == Approx(12.5F));
  CHECK(EffectGroundDiscSeedFor(kCounterRotatingByte, 12.5F).radius_at_anchor == Approx(12.5F));
  CHECK(EffectGroundDiscSeedFor(4, 0.0F).radius_at_anchor == Approx(40.0F));
  CHECK(EffectGroundDiscSeedFor(5, 12.5F).radius_at_anchor == Approx(30.0F));

  // ⚠ LE TEST DU CLIENT EST NON SIGNÉ, et il désigne EXACTEMENT deux et trois.
  // Le lire signé donnerait le rayon large à zéro et à un.
  CHECK(EffectGroundDiscSeedFor(2).radius_at_anchor == Approx(40.0F));
  CHECK(EffectGroundDiscSeedFor(3).radius_at_anchor == Approx(40.0F));
  CHECK(EffectGroundDiscSeedFor(0).radius_at_anchor == Approx(30.0F));
  CHECK(EffectGroundDiscSeedFor(1).radius_at_anchor == Approx(30.0F));

  // LES TROIS STYLES, et ils ne se déduisent pas de l'ordre des octets.
  CHECK(EffectGroundDiscSeedFor(0).node_style == 110);
  CHECK(EffectGroundDiscSeedFor(4).node_style == 111);
  CHECK(EffectGroundDiscSeedFor(5).node_style == 112);

  // LE DISQUE LE PLUS PÂLE EST AUSSI CELUI QUI PART D'UN AZIMUT, et c'est le
  // même jeu de branches qui décide des deux.
  CHECK(EffectGroundDiscSeedFor(1).alpha == 50);
  CHECK(EffectGroundDiscSeedFor(1).azimuth_degrees == 358);
  CHECK(EffectGroundDiscSeedFor(0).alpha == 200);
  CHECK(EffectGroundDiscSeedFor(0).azimuth_degrees == 0);
  CHECK(EffectGroundDiscSeedFor(4).alpha == 250);
}

TEST_CASE("le semis n'allume qu'une piste sur quatre", "[render][ground_disc]") {
  const EffectGroundDiscPattern pattern{kMapMagicCircle, "Effect_Build_GroundDiscFromParam1a",
                                        "data\\texture\\effect\\test.bmp", 4};
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectGroundDiscTracks(pattern);
  CHECK(tracks[0].active);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    CHECK_FALSE(tracks[rang].active);
  }
  // L'OCTET SE RANGE DANS LA PISTE : c'est là que le mouvement le relira.
  CHECK(tracks[0].value[kSlotOfGroundDiscVariant] == Approx(4.0F));
  // LA HAUTEUR EST NÉGATIVE : le repère du client descend.
  CHECK(tracks[0].value[kSlotOfGroundDiscHeight] < 0.0F);
}

TEST_CASE("le disque de quatorze tourne a rebours", "[render][ground_disc]") {
  std::array<EffectTrack, kEffectTracksPerNode> a_l_endroit;
  a_l_endroit.front() = TrackFromByteValue(4);
  std::array<EffectTrack, kEffectTracksPerNode> a_rebours;
  a_rebours.front() = TrackFromByteValue(kCounterRotatingByte);

  // LES DEUX PARTENT DU MÊME AZIMUT : c'est ce qui rend l'écart lisible.
  REQUIRE(a_l_endroit.front().azimuth_degrees == a_rebours.front().azimuth_degrees);

  AdvanceGroundDiscTracks(a_l_endroit);
  AdvanceGroundDiscTracks(a_rebours);
  CHECK(a_l_endroit.front().azimuth_degrees == 1);
  // ⚠ ZÉRO MOINS UN REBOUCLE À TROIS CENT CINQUANTE-NEUF, et le client y arrive
  // en AJOUTANT un tour complet, sans reste de division.
  CHECK(a_rebours.front().azimuth_degrees == kEffectTrackFullTurnDegrees - 1);
}

TEST_CASE("un tour complet demande trois cent soixante tics", "[render][ground_disc]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks.front() = TrackFromByteValue(4);
  const std::int16_t start = tracks.front().azimuth_degrees;
  for (std::int32_t tick = 0; tick < kEffectTrackFullTurnDegrees; ++tick) {
    AdvanceGroundDiscTracks(tracks);
    // L'AZIMUT RESTE DANS LE TOUR à chaque pas, et pas seulement à la fin.
    CHECK(tracks.front().azimuth_degrees >= 0);
    CHECK(tracks.front().azimuth_degrees < kEffectTrackFullTurnDegrees);
  }
  CHECK(tracks.front().azimuth_degrees == start);
}

TEST_CASE("l'azimut de depart a trois cent cinquante-huit reboucle aussi",
          "[render][ground_disc]") {
  // LE DISQUE LE PLUS PÂLE PART À DEUX DEGRÉS DE LA FIN DU TOUR : deux tics
  // suffisent à le faire passer par zéro, ce qu'un rebouclage par reste de
  // division manquerait si la borne était mal placée.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks.front() = TrackFromByteValue(1);
  REQUIRE(tracks.front().azimuth_degrees == 358);
  AdvanceGroundDiscTracks(tracks);
  CHECK(tracks.front().azimuth_degrees == 359);
  AdvanceGroundDiscTracks(tracks);
  CHECK(tracks.front().azimuth_degrees == 0);
}

TEST_CASE("le noeud seme porte la forme et le style du disque", "[render][ground_disc]") {
  const EffectGroundDiscPattern pattern{kMapMagicCircle, "Effect_Build_GroundDiscFromParam1a",
                                        "data\\texture\\effect\\test.bmp", 5};
  const ro::render::PooledEffectNode node =
      SowEffectGroundDiscNode(pattern, glm::vec3{1.0F, 2.0F, 3.0F}, 60);
  CHECK(node.shape == PooledNodeShape::kGroundDisc);
  CHECK(node.trail_variant == 112);
  CHECK(node.node.life_ticks == 60);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == "data\\texture\\effect\\test.bmp");
  // L'ANCRE EST CELLE DU PORTEUR : le mouvement la recopiera à chaque tic.
  CHECK(node.ground_position.x == Approx(1.0F));
  CHECK(node.owner_position.z == Approx(3.0F));
}
