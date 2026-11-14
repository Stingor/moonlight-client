#include <array>
#include <cmath>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::AdvanceHomingTrailTracks;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;

namespace {

// Les cases que ce mouvement lit, aux mêmes rangs que dans le client.
constexpr std::size_t kSlotOfSpeed = 1;
constexpr std::size_t kSlotOfRise = 2;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfPhase = 1;
constexpr std::size_t kMarkOfKind = 2;
constexpr std::uint8_t kKindThatBursts = 1;
constexpr std::uint8_t kPhaseOfHoming = 1;
constexpr std::uint8_t kPhaseOfFall = 2;

// Un nœud prêt à voler : quatre pistes allumées, une tête qui avance d'une
// unité par tic et monte d'un dixième.
std::array<EffectTrack, kEffectTracksPerNode> UnProjectile(std::uint8_t genre, float key) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.alpha = 200;
  }
  EffectTrack& head = tracks.front();
  head.value[kSlotOfSpeed] = 1.0F;
  head.value[kSlotOfRise] = 0.1F;
  head.mark[kMarkOfKind] = genre;
  head.mark[kMarkOfTexture] = 7;
  head.lift_key = key;
  head.anchor = {0.0F, 10.0F, 0.0F};
  return tracks;
}

constexpr glm::vec3 kOwner = {20.0F, 0.0F, 0.0F};
constexpr glm::vec3 kAim = {10.0F, 0.0F, 0.0F};

}  // namespace

TEST_CASE("Traînée guidée : les deux angles de caméra se posent MÊME éteinte", "[render][track]") {
  // ⚠ LE CLIENT LES ÉCRIT AVANT de regarder si sa première piste est allumée :
  // un nœud qui se réveille trouve des angles à jour, et non ceux d'une caméra
  // qui n'existe plus.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  std::uint32_t seed = 1U;
  // ⚠ ILS ARRIVENT DÉJÀ COMPLÉTÉS : la troupe passe `EffectBillboardTurnDegrees`
  // et `…TiltDegrees`, et le mouvement les range tels quels. Cet essai attendait
  // qu'il les complète une seconde fois — la figure recevait alors l'angle brut
  // de la caméra. Voir `effect_billboard_facing_test.cpp`.
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 270, 60, seed);
  CHECK(tracks[0].style == 270);
  CHECK(tracks[1].style == 60);
  // Et rien d'autre n'a bougé : la tête est éteinte.
  CHECK(tracks[0].ticks == 0);
}

TEST_CASE("Traînée guidée : la tête VISE au premier tic, puis avance droit", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = UnProjectile(0, 100.0F);
  std::uint32_t seed = 1U;
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  // La cible est droit devant sur l'abscisse : cap nul.
  CHECK(tracks[0].azimuth_degrees == 0);
  CHECK(tracks[0].anchor.x == Catch::Approx(1.0F));
  CHECK(tracks[0].anchor.z == Catch::Approx(0.0F));

  const float before = tracks[0].anchor.x;
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[0].anchor.x == Catch::Approx(before + 1.0F));
}

TEST_CASE("Traînée guidée : l'azimut est celui du CLIENT, le pas est le nôtre", "[render][track]") {
  // ⚠ UNE CIBLE VERS NOTRE PROFONDEUR POSITIVE est vers la négative du client :
  // son cap y vaut deux cent soixante-dix. C'est dans ce repère que le virage
  // ajoute ses cinq degrés ; le pas, lui, avance bien vers la cible.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = UnProjectile(0, 100.0F);
  std::uint32_t seed = 1U;
  AdvanceHomingTrailTracks(tracks, kOwner, {0.0F, 0.0F, 10.0F}, 0, 0, seed);
  CHECK(tracks[0].azimuth_degrees == 270);
  CHECK(tracks[0].anchor.x == Catch::Approx(0.0F).margin(1e-4F));
  CHECK(tracks[0].anchor.z == Catch::Approx(1.0F));
}

TEST_CASE("Traînée guidée : L'ALLER MONTE, le retour descend", "[render][track]") {
  // ⚠ LES DEUX SIGNES SE RETOURNENT, ET PAS DANS LE MÊME SENS. Le client ajoute
  // sa montée à l'aller et la retranche une fois retombé, dans un repère qui
  // DESCEND. N'en retourner qu'un donnerait un projectile qui part bien et
  // remonte à l'arrivée.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = UnProjectile(0, 100.0F);
  std::uint32_t seed = 1U;
  const float start = tracks[0].anchor.y;
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[0].anchor.y == Catch::Approx(start - 0.1F));

  tracks[0].mark[kMarkOfPhase] = kPhaseOfFall;
  const float at_peak = tracks[0].anchor.y;
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[0].anchor.y == Catch::Approx(at_peak + 0.1F));
}

TEST_CASE("Traînée guidée : passé la clé, celui qui NE PART PAS en éclats VIRE",
          "[render][track]") {
  // Clé de deux et vitesse de un : la distance la passe au troisième tic.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = UnProjectile(0, 2.0F);
  std::uint32_t seed = 1U;
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[0].mark[kMarkOfPhase] != kPhaseOfHoming);
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[0].mark[kMarkOfPhase] == kPhaseOfHoming);
  // Sa vitesse n'a pas changé : seul celui qui éclate la triple.
  CHECK(tracks[0].value[kSlotOfSpeed] == Catch::Approx(1.0F));
}

TEST_CASE("Traînée guidée : celui qui ÉCLATE saute le virage et TRIPLE sa vitesse",
          "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = UnProjectile(kKindThatBursts, 2.0F);
  std::uint32_t seed = 12345U;
  for (int tick = 0; tick < 3; ++tick) {
    AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  }
  CHECK(tracks[0].mark[kMarkOfPhase] == kPhaseOfFall);
  CHECK(tracks[0].value[kSlotOfSpeed] == Catch::Approx(3.0F));
}

TEST_CASE("Traînée guidée : celui qui ÉCLATE s'allume en dix tics", "[render][track]") {
  // ⚠ ET LUI SEUL. Un projectile qui se guide garde l'alpha de son semis ;
  // l'allumer aussi ferait apparaître une volée là où le client n'en montre pas.
  std::array<EffectTrack, kEffectTracksPerNode> that_bursts = UnProjectile(kKindThatBursts, 500.0F);
  std::array<EffectTrack, kEffectTracksPerNode> that_aims = UnProjectile(0, 500.0F);
  that_bursts[0].alpha = 0;
  that_aims[0].alpha = 0;
  std::uint32_t seed = 1U;
  for (int tick = 0; tick < 10; ++tick) {
    AdvanceHomingTrailTracks(that_bursts, kOwner, kAim, 0, 0, seed);
    AdvanceHomingTrailTracks(that_aims, kOwner, kAim, 0, 0, seed);
  }
  CHECK(that_bursts[0].alpha == 250);
  CHECK(that_aims[0].alpha == 0);

  // Et il plafonne à 255 : le onzième tic n'ajoute plus rien.
  AdvanceHomingTrailTracks(that_bursts, kOwner, kAim, 0, 0, seed);
  CHECK(that_bursts[0].alpha == 250);
}

TEST_CASE("Traînée guidée : la cascade DÉPLIE, et son premier saut coûte six fois",
          "[render][track]") {
  // ⚠ LE PREMIER SAUT COÛTE CENT CINQUANTE, les suivants vingt-cinq. Un pas
  // unique donnerait soit une traînée trop présente juste derrière la tête, soit
  // trois crans invisibles.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = UnProjectile(kKindThatBursts, 500.0F);
  for (EffectTrack& track : tracks) {
    track.alpha = 0;
  }
  tracks[0].alpha = 255;
  tracks[0].mark[kMarkOfTexture] = 9;
  std::uint32_t seed = 1U;

  // Au premier tic, aucun cran ne reçoit : chacun attend son rang.
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[1].alpha == 0);
  CHECK(tracks[2].alpha == 0);
  CHECK(tracks[3].alpha == 0);

  // Au deuxième, le premier cran reçoit — et la tête vient de regagner son
  // alpha, plafonné à 255.
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[1].alpha == 255 - 150);
  CHECK(tracks[1].mark[kMarkOfTexture] == 9);
  CHECK(tracks[2].alpha == 0);

  // Au troisième, le deuxième cran reçoit du premier : vingt-cinq de moins.
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[2].alpha == 255 - 150 - 25);
}

TEST_CASE("Traînée guidée : la cascade ne recopie NI le cap NI les tics", "[render][track]") {
  // C'est ce qui courbe la traînée derrière un projectile qui vire : chaque cran
  // garde le cap qu'il avait en recevant sa place.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = UnProjectile(kKindThatBursts, 500.0F);
  tracks[1].azimuth_degrees = 111;
  tracks[1].ticks = 42;
  std::uint32_t seed = 1U;
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[1].azimuth_degrees == 111);
  CHECK(tracks[1].ticks == 42);
  // Mais l'ancre, elle, a bien sauté.
  CHECK(tracks[1].anchor.x == Catch::Approx(tracks[0].anchor.x - 1.0F));
}

TEST_CASE("Traînée guidée : une piste RETARDÉE ne bouge pas", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = UnProjectile(0, 100.0F);
  tracks[0].ticks = -3;
  std::uint32_t seed = 1U;
  const glm::vec3 start = tracks[0].anchor;
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[0].ticks == -2);
  CHECK(tracks[0].anchor.x == Catch::Approx(start.x));
  CHECK(tracks[0].anchor.y == Catch::Approx(start.y));
}

TEST_CASE("Traînée guidée : la tête tourne À REBOURS", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = UnProjectile(0, 100.0F);
  tracks[0].degrees = 0;
  std::uint32_t seed = 1U;
  AdvanceHomingTrailTracks(tracks, kOwner, kAim, 0, 0, seed);
  CHECK(tracks[0].degrees == 345);
}
