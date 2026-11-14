#include "ro/render/effect_quad_burst.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_quad_burst_seed.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_spread.h"

using Catch::Approx;
using ro::render::AdvanceQuadBurstTracks;
using ro::render::ComposeQuadBurstQuads;
using ro::render::EffectQuadBurstAlpha;
using ro::render::EffectQuadBurstColor;
using ro::render::EffectQuadBurstMarkOf;
using ro::render::EffectQuadBurstOffset;
using ro::render::EffectQuadBurstOffsetOf;
using ro::render::EffectQuadBurstPasses;
using ro::render::EffectQuadBurstPattern;
using ro::render::EffectQuadBurstPatternsFor;
using ro::render::EffectQuadBurstPatternTable;
using ro::render::EffectQuadBurstScaleOf;
using ro::render::EffectQuadBurstTextureOf;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kQuadBurstBlue;
using ro::render::kQuadBurstDrifts;
using ro::render::kQuadBurstEndlessLifeTicks;
using ro::render::kQuadBurstGhostTexture;
using ro::render::kQuadBurstGreen;
using ro::render::kQuadBurstGrey;
using ro::render::kQuadBurstLemon;
using ro::render::kQuadBurstMarkBlue;
using ro::render::kQuadBurstMarkFading;
using ro::render::kQuadBurstMarkGrey;
using ro::render::kQuadBurstMarkRising;
using ro::render::kQuadBurstMarkScattering;
using ro::render::kQuadBurstMarkWhite;
using ro::render::kQuadBurstOrange;
using ro::render::kQuadBurstParticles;
using ro::render::kQuadBurstRisingStarts;
using ro::render::kQuadBurstWhite;
using ro::render::kQuadBurstWhiteTexture;
using ro::render::SeedEffectQuadBurstTracks;
using ro::render::SetEffectQuadBurstOffset;
using ro::render::SowEffectQuadBurstNode;

namespace {

[[nodiscard]] EffectTrack Vivante(std::uint8_t mark) {
  EffectTrack track;
  track.active = true;
  track.radius_at_anchor = 2.4F;
  track.lift_key = 0.15F;
  track.mark[0] = mark;
  for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
    // Toutes AU-DESSUS du nœud dans le repère du client : hauteur négative.
    SetEffectQuadBurstOffset(track, particle, {0.0F, -10.0F, 0.0F});
  }
  return track;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Sown(std::uint8_t octet,
                                                                 std::uint32_t seed = 5U) {
  EffectQuadBurstPattern pattern;
  pattern.variant = octet;
  return SeedEffectQuadBurstTracks(pattern, seed);
}

}  // namespace

TEST_CASE("les quatre positions ne se suivent pas dans les cases", "[render][effect][burst]") {
  // ⚠ LA DEUXIÈME PARTICULE VIT DANS L'ANCRE, pas dans la queue, et la
  // quatrième aux cases ZÉRO à deux. Les lire dans l'ordre des cases
  // mélangerait les quatre — et leurs dérives, qui diffèrent par leurs signes.
  EffectTrack track;
  SetEffectQuadBurstOffset(track, 0, {1.0F, 2.0F, 3.0F});
  SetEffectQuadBurstOffset(track, 1, {4.0F, 5.0F, 6.0F});
  SetEffectQuadBurstOffset(track, 2, {7.0F, 8.0F, 9.0F});
  SetEffectQuadBurstOffset(track, 3, {10.0F, 11.0F, 12.0F});
  CHECK(track.tail[3] == Approx(1.0F));
  CHECK(track.anchor.x == Approx(4.0F));
  CHECK(track.tail[6] == Approx(7.0F));
  CHECK(track.tail[0] == Approx(10.0F));
  for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
    const EffectQuadBurstOffset lu = EffectQuadBurstOffsetOf(track, particle);
    CHECK(lu.sideways == Approx(1.0F + 3.0F * static_cast<float>(particle)));
    CHECK(lu.depth_below == Approx(2.0F + 3.0F * static_cast<float>(particle)));
    CHECK(lu.forward == Approx(3.0F + 3.0F * static_cast<float>(particle)));
  }
}

TEST_CASE("une piste rend QUATRE carres, et deux marques les doublent", "[render][effect][burst]") {
  // ⚠ QUATRE PARTICULES PAR PISTE, et deux marques peignent la leur DEUX FOIS :
  // c'est ainsi que le client double leur éclat.
  CHECK(ComposeQuadBurstQuads(Vivante(3), 0, 0, {}).size() == kQuadBurstParticles);
  CHECK(ComposeQuadBurstQuads(Vivante(kQuadBurstMarkWhite), 0, 0, {}).size() ==
        2 * kQuadBurstParticles);
  CHECK(ComposeQuadBurstQuads(Vivante(kQuadBurstMarkBlue), 0, 0, {}).size() ==
        2 * kQuadBurstParticles);
  CHECK(EffectQuadBurstPasses(kQuadBurstMarkWhite) == 2);
  CHECK(EffectQuadBurstPasses(kQuadBurstMarkRising) == 1);

  EffectTrack faded = Vivante(kQuadBurstMarkWhite);
  faded.active = false;
  CHECK(ComposeQuadBurstQuads(faded, 0, 0, {}).empty());
}

TEST_CASE("la marque onze ne peint que sa PREMIERE particule", "[render][effect][burst]") {
  // ⚠ LES TROIS AUTRES SONT CALCULÉES ET JETÉES. Les peindre quadruplerait le
  // gris.
  const std::vector<EffectWorldQuad> quads =
      ComposeQuadBurstQuads(Vivante(kQuadBurstMarkGrey), 0, 0, {});
  REQUIRE(quads.size() == 1);
  CHECK(quads.front().tint[0] == kQuadBurstGrey[0]);
}

TEST_CASE("une particule sous le noeud ne se montre pas", "[render][effect][burst]") {
  // ⚠ LE CLIENT NE PEINT QUE CE QUI EST AU-DESSUS DE LUI — hauteur négative
  // dans son repère. La seule exception est la marque quinze.
  EffectTrack basse = Vivante(kQuadBurstMarkWhite);
  for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
    SetEffectQuadBurstOffset(basse, particle, {0.0F, 50.0F, 0.0F});
  }
  CHECK(ComposeQuadBurstQuads(basse, 0, 0, {}).empty());

  EffectTrack fading = basse;
  fading.mark[0] = kQuadBurstMarkFading;
  CHECK(ComposeQuadBurstQuads(fading, 0, 0, {}).size() == kQuadBurstParticles);
}

TEST_CASE("le carre MONTE au lieu de s'enfoncer", "[render][effect][burst]") {
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : une particule à moins dix
  // chez lui est DIX unités au-dessus du nœud chez nous.
  const std::vector<EffectWorldQuad> quads =
      ComposeQuadBurstQuads(Vivante(kQuadBurstMarkRising), 0, 0, {});
  REQUIRE_FALSE(quads.empty());
  for (const glm::vec3& corner : quads.front().corners) {
    CHECK(corner.y == Approx(10.0F));
  }
}

TEST_CASE("les quatre rampes d'alpha", "[render][effect][burst]") {
  // La rampe ordinaire plafonne à deux cent cinquante et descend sous le seuil.
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkWhite, -10.0F, 0, true) == 250);
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkWhite, -30.0F, 0, true) == 0);
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkWhite, -22.0F, 0, true) == 200);

  // ⚠ ET LA MARQUE ONZE N'A PAS LE MÊME SEUIL SUR LA PREMIÈRE PARTICULE : moins
  // quarante chez elle, moins vingt chez les trois autres. La lisser ferait
  // apparaître la première vingt unités trop tôt.
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkGrey, -30.0F, 0, true) == 250);
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkGrey, -30.0F, 0, false) == 0);

  // Les deux marques qui montent : un plateau à cent, une chute en haut, rien
  // en bas.
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkRising, -20.0F, 0, true) == 100);
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkRising, -50.0F, 0, true) == 0);
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkRising, 0.0F, 0, true) == 0);
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkScattering, -20.0F, 0, true) == 100);

  // ⚠ LA MARQUE QUINZE NE REGARDE PAS SA HAUTEUR : son alpha ne dépend que de
  // sa propre marque d'usure.
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkFading, -500.0F, 0, true) == 255);
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkFading, 500.0F, 10, true) == 235);
  CHECK(EffectQuadBurstAlpha(kQuadBurstMarkFading, 0.0F, 200, true) == 0);

  // La rampe par défaut : un plateau bas, et une extinction franche au ras.
  CHECK(EffectQuadBurstAlpha(3, -2.0F, 0, true) == 50);
  CHECK(EffectQuadBurstAlpha(3, -0.25F, 0, true) == 25);
  CHECK(EffectQuadBurstAlpha(3, -6.0F, 0, true) == 20);
}

TEST_CASE("la marque quinze prend sa teinte d'une SIXIEME marque", "[render][effect][burst]") {
  CHECK(EffectQuadBurstColor(kQuadBurstMarkWhite, 0) == kQuadBurstWhite);
  CHECK(EffectQuadBurstColor(kQuadBurstMarkBlue, 0) == kQuadBurstBlue);
  CHECK(EffectQuadBurstColor(kQuadBurstMarkGrey, 0) == kQuadBurstGrey);
  CHECK(EffectQuadBurstColor(kQuadBurstMarkFading, 7) == kQuadBurstOrange);
  CHECK(EffectQuadBurstColor(kQuadBurstMarkFading, 8) == kQuadBurstLemon);
  CHECK(EffectQuadBurstColor(kQuadBurstMarkFading, 5) == kQuadBurstGreen);
  CHECK(EffectQuadBurstColor(kQuadBurstMarkScattering, 0) == kQuadBurstGreen);
}

TEST_CASE("les huit paires d'angles poursuivent leur cible", "[render][effect][burst]") {
  // ⚠ HUIT PAIRES POUR QUATRE PARTICULES : chacune en emploie deux. Les lire
  // comme seize angles indépendants supprimerait la dérive.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].value[0] = 100.0F;
  tracks[0].value[1] = 50.0F;
  tracks[0].value[2] = 100.0F;
  tracks[0].value[3] = 200.0F;
  std::uint32_t seed = 3U;
  AdvanceQuadBurstTracks(tracks, 0, 0, seed);
  // Le courant DESCEND de deux vers une cible plus basse, MONTE de trois vers
  // une plus haute.
  CHECK(tracks[0].value[0] == Approx(98.0F));
  CHECK(tracks[0].value[2] == Approx(103.0F));
}

TEST_CASE("les deux angles de camera se rangent dans les styles", "[render][effect][burst]") {
  // ⚠ UNE SEULE FOIS PAR TIC, ET NON PAR PISTE, et même quand toutes les
  // pistes sont éteintes.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  std::uint32_t seed = 1U;
  // 🔴 TELS QU'ILS ARRIVENT : ils sortent déjà de `EffectBillboard*Degrees`, qui
  // fait le calcul du client. Le refaire tournait la figure à l'envers de la
  // caméra. Voir `StoreEffectBillboardAngles`.
  AdvanceQuadBurstTracks(tracks, 90, 100, seed);
  CHECK(tracks[0].style == 90);
  CHECK(tracks[1].style == 100);
}

TEST_CASE("les quatre particules ne derivent pas dans le meme sens", "[render][effect][burst]") {
  // ⚠ LES SIGNES NE SUIVENT AUCUNE RÈGLE : la première s'écarte des deux
  // côtés, la deuxième rentre des deux, les deux autres font moitié-moitié.
  // Les aligner ferait dériver les quatre en bloc.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Vivante(kQuadBurstMarkWhite);
  // Un angle dont le sinus est franchement positif, pour les huit paires.
  for (std::size_t rang = 0; rang < 16; ++rang) {
    tracks[0].value[rang] = 90.0F;
  }
  std::uint32_t seed = 2U;
  AdvanceQuadBurstTracks(tracks, 0, 0, seed);
  for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
    const EffectQuadBurstOffset after = EffectQuadBurstOffsetOf(tracks[0], particle);
    CHECK(after.sideways * kQuadBurstDrifts[particle].drift_sideways > 0.0F);
    CHECK(after.forward * kQuadBurstDrifts[particle].drift_forward > 0.0F);
  }
  CHECK(kQuadBurstDrifts[0].drift_forward != kQuadBurstDrifts[1].drift_forward);
  CHECK(kQuadBurstDrifts[2].drift_forward != kQuadBurstDrifts[3].drift_forward);
}

TEST_CASE("la particule ordinaire DESCEND et RENAIT en haut", "[render][effect][burst]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Vivante(kQuadBurstMarkWhite);
  std::uint32_t seed = 8U;
  AdvanceQuadBurstTracks(tracks, 0, 0, seed);
  // Dans le repère du client, descendre c'est retrancher la clé.
  CHECK(EffectQuadBurstOffsetOf(tracks[0], 0).depth_below == Approx(-10.15F));

  // Tombée sous le seuil, elle renaît en haut et se recentre.
  SetEffectQuadBurstOffset(tracks[0], 0, {5.0F, -35.0F, 5.0F});
  AdvanceQuadBurstTracks(tracks, 0, 0, seed);
  const EffectQuadBurstOffset renee = EffectQuadBurstOffsetOf(tracks[0], 0);
  CHECK(renee.sideways == Approx(0.0F));
  CHECK(renee.forward == Approx(0.0F));
  CHECK(renee.depth_below >= -0.15F);
  CHECK(renee.depth_below < 100.0F);
}

TEST_CASE("la marque quatorze MONTE et se recale en haut", "[render][effect][burst]") {
  // ⚠ ELLE MONTE, LÀ OÙ TOUTES LES AUTRES DESCENDENT : le client AJOUTE sa clé
  // au lieu de la retrancher.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Vivante(kQuadBurstMarkRising);
  std::uint32_t seed = 8U;
  AdvanceQuadBurstTracks(tracks, 0, 0, seed);
  CHECK(EffectQuadBurstOffsetOf(tracks[0], 0).depth_below == Approx(-9.85F));

  SetEffectQuadBurstOffset(tracks[0], 0, {5.0F, 1.0F, 5.0F});
  AdvanceQuadBurstTracks(tracks, 0, 0, seed);
  const EffectQuadBurstOffset recalee = EffectQuadBurstOffsetOf(tracks[0], 0);
  CHECK(recalee.sideways == Approx(0.0F));
  CHECK(recalee.depth_below == Approx(-40.0F + 0.15F));
}

TEST_CASE("la marque quinze ne monte ni ne descend", "[render][effect][burst]") {
  // ⚠ SA HAUTEUR NE BOUGE JAMAIS, et c'est sa MARQUE qui l'éteint. La faire
  // monter la sortirait de son cadre.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Vivante(kQuadBurstMarkFading);
  for (std::size_t rang = 0; rang < 16; ++rang) {
    tracks[0].value[rang] = 90.0F;
  }
  SetEffectQuadBurstOffset(tracks[0], 0, {20.0F, 0.0F, 0.0F});
  std::uint32_t seed = 8U;
  AdvanceQuadBurstTracks(tracks, 0, 0, seed);
  CHECK(EffectQuadBurstOffsetOf(tracks[0], 0).depth_below == Approx(0.0F));
  // Passé la distance de déclenchement, sa marque d'usure monte de cinq.
  CHECK(tracks[0].mark[1] == 5);
  // Et elle se remet à zéro en ramenant la particule au centre.
  tracks[0].mark[1] = 118;
  AdvanceQuadBurstTracks(tracks, 0, 0, seed);
  CHECK(tracks[0].mark[1] == 0);
  CHECK(EffectQuadBurstOffsetOf(tracks[0], 0).sideways == Approx(0.0F));
}

TEST_CASE("un octet choisit SIX choses", "[render][effect][burst]") {
  // ⚠ L'IMAGE, LA MARQUE, LA TAILLE, LA VITESSE, LE NOMBRE DE PISTES VIVES ET
  // LA HAUTEUR DE DÉPART. Le lire comme un numéro de teinte en manquerait cinq.
  CHECK(EffectQuadBurstTextureOf(0) == "data\\texture\\effect\\freezing_circle.bmp");
  CHECK(EffectQuadBurstTextureOf(1) == kQuadBurstWhiteTexture);
  CHECK(EffectQuadBurstTextureOf(3) == kQuadBurstGhostTexture);
  CHECK(EffectQuadBurstMarkOf(0) == kQuadBurstMarkWhite);
  CHECK(EffectQuadBurstMarkOf(1) == kQuadBurstMarkBlue);
  CHECK(EffectQuadBurstMarkOf(2) == kQuadBurstMarkWhite);
  CHECK(EffectQuadBurstMarkOf(3) == kQuadBurstMarkGrey);
  CHECK(EffectQuadBurstMarkOf(4) == kQuadBurstMarkRising);
  CHECK(EffectQuadBurstMarkOf(5) == kQuadBurstMarkFading);
  CHECK(EffectQuadBurstMarkOf(6) == kQuadBurstMarkScattering);
  CHECK(EffectQuadBurstMarkOf(7) == kQuadBurstMarkFading);
  // ⚠ ET UN SEUL OCTET POSE UNE ÉCHELLE.
  CHECK(EffectQuadBurstScaleOf(2) == Approx(7.0F));
  CHECK(EffectQuadBurstScaleOf(1) == Approx(0.0F));

  // Les tailles, la vitesse et le nombre de pistes vives.
  CHECK(Sown(0)[0].radius_at_anchor == Approx(0.8F));
  CHECK(Sown(1)[0].radius_at_anchor == Approx(2.4F));
  CHECK(Sown(3)[0].radius_at_anchor == Approx(3.2F));
  CHECK(Sown(3)[0].lift_key == Approx(0.6F));
  CHECK(Sown(0)[0].lift_key == Approx(0.15F));
}

TEST_CASE("trois octets n'allument pas les quatre pistes", "[render][effect][burst]") {
  // ⚠ LE TROIS N'EN ALLUME QU'UNE, le sept et le huit deux. Le client écrit
  // quand même l'état des quatre.
  const std::array<EffectTrack, kEffectTracksPerNode> ghost = Sown(3);
  CHECK(ghost[0].active);
  CHECK_FALSE(ghost[1].active);
  CHECK_FALSE(ghost[3].active);
  const std::array<EffectTrack, kEffectTracksPerNode> sept = Sown(7);
  CHECK(sept[0].active);
  CHECK(sept[1].active);
  CHECK_FALSE(sept[2].active);
  for (const EffectTrack& track : Sown(1)) {
    CHECK(track.active);
  }
}

TEST_CASE("la marque quatorze part ECHELONNEE", "[render][effect][burst]") {
  // ⚠ QUARANTE, CINQUANTE, SOIXANTE, SOIXANTE-DIX : c'est ce qui fait la
  // colonne. Les poser à la même hauteur les ferait monter en un seul paquet.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Sown(4);
  for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
    CHECK(EffectQuadBurstOffsetOf(tracks[0], particle).depth_below ==
          Approx(kQuadBurstRisingStarts[particle]));
  }
  // Les deux marques qui s'effacent partent, elles, toutes à zéro.
  for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
    CHECK(EffectQuadBurstOffsetOf(Sown(5)[0], particle).depth_below == Approx(0.0F));
  }
  // Et la dispersée part sous moins quarante, chacune à sa hauteur.
  std::set<float> hauteurs;
  for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
    const float h = EffectQuadBurstOffsetOf(Sown(6)[0], particle).depth_below;
    CHECK(h <= -40.0F);
    CHECK(h > -80.0F);
    hauteurs.insert(h);
  }
  CHECK(hauteurs.size() > 1);
}

TEST_CASE("seul le fantome nait ECARTE", "[render][effect][burst]") {
  // ⚠ LES AUTRES PARTENT DE L'AXE. Leur donner un écart brouillerait la
  // colonne que les autres marques forment.
  bool vu_ecarte = false;
  for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
    const EffectQuadBurstOffset ghost = EffectQuadBurstOffsetOf(Sown(3)[0], particle);
    CHECK(ghost.sideways >= -8.0F);
    CHECK(ghost.sideways < 8.0F);
    vu_ecarte = vu_ecarte || ghost.sideways != 0.0F;
    CHECK(EffectQuadBurstOffsetOf(Sown(0)[0], particle).sideways == Approx(0.0F));
    CHECK(EffectQuadBurstOffsetOf(Sown(0)[0], particle).forward == Approx(0.0F));
  }
  CHECK(vu_ecarte);
  // Le dispersé s'écarte plus large, et seulement de côté.
  CHECK(EffectQuadBurstOffsetOf(Sown(6)[0], 0).forward == Approx(0.0F));
}

TEST_CASE("la marque quinze range l'octet dans sa SIXIEME marque", "[render][effect][burst]") {
  CHECK(Sown(5)[0].mark[kQuadBurstParticles + 1] == 5);
  CHECK(Sown(7)[0].mark[kQuadBurstParticles + 1] == 7);
  for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
    CHECK(Sown(5)[0].mark[particle + 1] == 0);
  }
}

TEST_CASE("la table des rafales porte HUIT sites", "[render][effect][burst]") {
  // ⚠ LE RELEVÉ DES NŒUDS N'EN ATTACHE QUE CINQ ; les xrefs du bâtisseur en
  // donnent huit. Les omettre laisserait trois effets incomplets.
  const std::span<const EffectQuadBurstPattern> table = EffectQuadBurstPatternTable();
  REQUIRE(table.size() == 8);
  CHECK(std::ranges::is_sorted(table, {}, &EffectQuadBurstPattern::effect_id));
  std::set<std::uint8_t> octets;
  for (const EffectQuadBurstPattern& pattern : table) {
    octets.insert(pattern.variant);
  }
  CHECK(octets.size() == 8);
  CHECK(EffectQuadBurstPatternsFor(692).front().variant == 3);
  CHECK(EffectQuadBurstPatternsFor(712).front().variant == 7);
  CHECK(EffectQuadBurstPatternsFor(1).empty());
}

TEST_CASE("un noeud de rafale ne meurt pas", "[render][effect][burst]") {
  // ⚠ CE SONT DES MARQUES DE CARTE, que le client renouvelle lui-même. Leur
  // donner une vie ordinaire les ferait disparaître en quelques secondes.
  EffectQuadBurstPattern pattern;
  pattern.variant = 6;
  std::uint32_t seed = 7U;
  const ro::render::PooledEffectNode node =
      SowEffectQuadBurstNode(pattern, {3.0F, 0.0F, 4.0F}, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kQuadBurst);
  CHECK(node.node.life_ticks == kQuadBurstEndlessLifeTicks);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths[0] == kQuadBurstWhiteTexture);
  CHECK(node.ground_position.x == Approx(3.0F));
}
