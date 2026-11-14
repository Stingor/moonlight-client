// LES SOURCES DE SON D'UNE CARTE : d'où part le bruit, quand il se réveille,
// quand il se rejoue. Tout dans le repère du client, abscisse et profondeur.

#include "ro/audio/ambient_sound.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/audio/sound_placement.h"

using Catch::Approx;
using ro::audio::AmbientSoundCue;
using ro::audio::AmbientSoundEmitter;
using ro::audio::AmbientSoundPoint;
using ro::audio::AmbientSoundSource;

namespace {

constexpr std::uint32_t kBornMs = 1000;
constexpr std::uint32_t kCycleMs = 4000;

// Une source de quarante sur vingt, qui porte à soixante et se rejoue toutes
// les quatre secondes.
AmbientSoundSource Pond() {
  AmbientSoundSource source;
  source.native_ground = {0.0F, 0.0F};
  source.width = 40;
  source.height = 20;
  source.range = 60.0F;
  source.cycle_seconds = 4.0F;
  source.volume = 0.5F;
  return source;
}

}  // namespace

TEST_CASE("le bruit part du bord le plus proche du joueur", "[audio][ambient]") {
  // Plein est : le milieu du bord droit.
  const std::array<float, 2> est = AmbientSoundPoint(Pond(), {100.0F, 0.0F});
  CHECK(est[0] == Approx(20.0F));
  CHECK(est[1] == Approx(0.0F));
  // Presque plein nord : le bord haut, décalé le long de la pente.
  const std::array<float, 2> nord = AmbientSoundPoint(Pond(), {10.0F, 100.0F});
  CHECK(nord[0] == Approx(1.0F));
  CHECK(nord[1] == Approx(10.0F));
}

TEST_CASE("le rapport des côtés se calcule en entiers", "[audio][ambient]") {
  // 🔴 VINGT SUR QUARANTE VAUT ZÉRO : dès que la pente n'est pas nulle, le bruit
  // se projette sur le bord haut, jusqu'à la verticale du joueur. En réels, le
  // rapport vaudrait un demi et le bruit partirait du bord droit, en (20, 2).
  const std::array<float, 2> point = AmbientSoundPoint(Pond(), {100.0F, 10.0F});
  CHECK(point[0] == Approx(100.0F));
  CHECK(point[1] == Approx(10.0F));
}

TEST_CASE("à l'ouest, la pente bornée fait partir le bruit du bord haut", "[audio][ambient]") {
  // L'écart négatif devient un millième avant de diviser : la pente s'envole, et
  // le bruit part du milieu du bord haut — pas du bord gauche, miroir de l'est.
  const std::array<float, 2> point = AmbientSoundPoint(Pond(), {-100.0F, 10.0F});
  CHECK(point[0] == Approx(0.001F).margin(1.0e-4));
  CHECK(point[1] == Approx(10.0F));
}

TEST_CASE("une source se réveille près du joueur, et sonne plus près encore", "[audio][ambient]") {
  // Trop loin de l'emprise entière : rien ne se calcule.
  AmbientSoundEmitter far(Pond(), kBornMs);
  CHECK_FALSE(far.Tick({300.0F, 0.0F}, kBornMs + 1).has_value());
  // Réveillée, mais le bord est à plus de la portée plus soixante-quinze.
  AmbientSoundEmitter reveillee(Pond(), kBornMs);
  CHECK_FALSE(reveillee.Tick({200.0F, 0.0F}, kBornMs + 1).has_value());
  // Assez près : elle sonne.
  AmbientSoundEmitter near_emitter(Pond(), kBornMs);
  CHECK(near_emitter.Tick({150.0F, 0.0F}, kBornMs + 1).has_value());
}

TEST_CASE("le réveil garde une source lointaine dont le point tombe sous le joueur",
          "[audio][ambient]") {
  // 🔴 LA DIVISION ENTIÈRE PROJETTE LE POINT JUSQU'À LA VERTICALE DU JOUEUR, même
  // à mille unités : sans la garde de réveil, qui mesure depuis la source, ce
  // point serait à portée et la source sonnerait à plein volume.
  const std::array<float, 2> listener = {1000.0F, 10.0F};
  CHECK(AmbientSoundPoint(Pond(), listener)[0] == Approx(1000.0F));
  AmbientSoundEmitter source(Pond(), kBornMs);
  CHECK_FALSE(source.Tick(listener, kBornMs + 1).has_value());
}

TEST_CASE("le premier bruit part tout de suite, puis un par cycle", "[audio][ambient]") {
  AmbientSoundEmitter source(Pond(), kBornMs);
  const std::array<float, 2> listener = {30.0F, 0.0F};
  // Née comme si elle venait de sonner il y a un cycle : il faut DÉPASSER
  // l'instant de sa naissance, pas l'atteindre.
  CHECK_FALSE(source.Tick(listener, kBornMs).has_value());
  CHECK(source.Tick(listener, kBornMs + 1).has_value());
  CHECK_FALSE(source.Tick(listener, kBornMs + 2).has_value());
  CHECK_FALSE(source.Tick(listener, kBornMs + 1 + kCycleMs).has_value());
  CHECK(source.Tick(listener, kBornMs + 2 + kCycleMs).has_value());
}

TEST_CASE("une source sans largeur ne sonne jamais", "[audio][ambient]") {
  AmbientSoundSource silent = Pond();
  silent.width = 0;
  AmbientSoundEmitter source(silent, kBornMs);
  CHECK_FALSE(source.Tick({0.0F, 0.0F}, kBornMs + 1).has_value());
}

TEST_CASE("le bruit porte à la portée, plein volume au sixième, en entiers", "[audio][ambient]") {
  AmbientSoundSource grande = Pond();
  grande.range = 100.0F;
  AmbientSoundEmitter source(grande, kBornMs);
  const std::optional<AmbientSoundCue> cue = source.Tick({30.0F, 0.0F}, kBornMs + 1);
  REQUIRE(cue.has_value());
  CHECK(cue->hearing_range == Approx(100.0F));
  // Cent sur six fait seize et deux tiers : seize.
  CHECK(cue->near_distance == Approx(16.0F));
  CHECK(cue->volume == Approx(0.5F));
}

TEST_CASE("un placement à portée réglable s'éteint à sa portée", "[audio][placement]") {
  const std::array<float, 3> ear = {0.0F, 0.0F, 0.0F};
  const std::array<float, 3> right = {1.0F, 0.0F, 0.0F};
  CHECK(ro::audio::PlaceSound(ear, right, {30.0F, 0.0F, 0.0F}, 60.0F, 10.0F).gain ==
        Approx(10.0F / 30.0F));
  CHECK(ro::audio::PlaceSound(ear, right, {5.0F, 0.0F, 0.0F}, 60.0F, 10.0F).gain == Approx(1.0F));
  CHECK(ro::audio::PlaceSound(ear, right, {60.0F, 0.0F, 0.0F}, 60.0F, 10.0F).gain == 0.0F);
}

TEST_CASE("l'emprise est le rectangle centré sur la source", "[audio][ambient]") {
  AmbientSoundSource source = Pond();
  source.native_ground = {100.0F, 50.0F};
  const std::vector<std::array<float, 2>> corners = ro::audio::AmbientSoundFootprint(source);
  REQUIRE(corners.size() == 4);
  CHECK(corners[0] == std::array<float, 2>{80.0F, 40.0F});
  CHECK(corners[2] == std::array<float, 2>{120.0F, 60.0F});
  // Une source sans largeur n'a pas d'emprise : elle ne sonne jamais.
  source.width = 0;
  CHECK(ro::audio::AmbientSoundFootprint(source).empty());
}

TEST_CASE("le cercle d'écoute est à la portée du point d'émission", "[audio][ambient]") {
  constexpr std::size_t kSegments = 16;
  const std::array<float, 2> point = {10.0F, -5.0F};
  const std::vector<std::array<float, 2>> circle =
      ro::audio::AmbientSoundHearingCircle(point, 60.0F, kSegments);
  REQUIRE(circle.size() == kSegments);
  for (const std::array<float, 2>& vertex : circle) {
    CHECK(std::hypot(vertex[0] - point[0], vertex[1] - point[1]) == Approx(60.0F));
  }
  CHECK(ro::audio::AmbientSoundHearingCircle(point, 0.0F, kSegments).empty());
}
