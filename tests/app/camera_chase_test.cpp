#include "input/camera_chase.h"

#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ro/render/terrain_mesh.h"
#include "ro/sim/walk_path.h"

using Catch::Matchers::WithinRel;
using moonclient::CatchUpTimeMs;
using moonclient::ChaseLagUnits;
using moonclient::kClientFrameMs;
using moonclient::RemainingLagAfter;
using moonclient::TeleportUnits;

namespace {

// Le retard que le lissage laisse derrière une cible qui avance à `chase_speed`,
// une fois l'équilibre atteint. C'est la boucle de CameraTracking::Follow, sans
// la caméra : on n'y mesure que le retard.
float SteadyLag(float speed_units_per_ms, float image_ms) {
  float lag = 0.0F;
  // Cent images suffisent largement : le reste décroît d'un dixième par image.
  for (int image = 0; image < 100; ++image) {
    lag = (lag + speed_units_per_ms * image_ms) * RemainingLagAfter(image_ms);
  }
  return lag;
}

float SpeedOf(std::uint16_t speed_ms_per_cell) {
  return ro::render::kWorldUnitsPerCell / static_cast<float>(speed_ms_per_cell);
}

}  // namespace

TEST_CASE("Caméra : la constante de temps MAJORE le retard, à toute cadence", "[app][camera]") {
  // C'est ce qui autorise à DÉRIVER le seuil au lieu de le poser : le retard
  // d'équilibre vaut au plus vitesse × durée, quelle que soit la cadence
  // d'images. Sans cette majoration, le seuil dépendrait du matériel du joueur.
  //
  // ⚠ MAJORE, ET NE VAUT PAS. Le retard d'équilibre glisse avec la cadence —
  // la constante de temps est prise à la limite des images courtes. C'est elle
  // le majorant, et le seuil n'a besoin que de cela.
  const float chase_speed = SpeedOf(ro::sim::kFastestStepMs);
  const float majorant = chase_speed * CatchUpTimeMs();
  for (const float image_ms : {kClientFrameMs / 4.0F, kClientFrameMs / 2.0F, kClientFrameMs,
                               kClientFrameMs * 3.0F, kClientFrameMs * 10.0F}) {
    const float lag = SteadyLag(chase_speed, image_ms);
    CHECK(lag <= majorant);
    // Et il n'en est pas si loin : le majorant reste du bon ordre de grandeur,
    // sans quoi le seuil qu'on en tire serait bien plus large que nécessaire.
    CHECK(lag > majorant / 3.0F);
  }
}

TEST_CASE("Caméra : la marche la plus rapide ne franchit pas le seuil", "[app][camera]") {
  // 🔴 C'EST LE DÉFAUT QUE CET ESSAI TIENT. Un seuil de quinze unités — trois
  // cellules — se franchissait dès cinquante millisecondes par case, bien avant
  // la borne du serveur : la caméra se croyait téléportée à chaque image, se
  // posait, relissait, et tout l'écran tremblait. Voir ro::sim::kFastestStepMs.
  for (const std::uint16_t speed : {std::uint16_t{150}, std::uint16_t{100}, std::uint16_t{60},
                                    std::uint16_t{40}, ro::sim::kFastestStepMs}) {
    const float lag = SteadyLag(SpeedOf(speed), kClientFrameMs);
    CHECK(lag < TeleportUnits());
  }
}

TEST_CASE("Caméra : une téléportation franchit le seuil", "[app][camera]") {
  // L'autre moitié : le seuil relevé doit encore SÉPARER. Un changement de
  // carte, ou un warp, déplace de dizaines de cellules — sans commune mesure
  // avec le retard qu'une marche laisse.
  constexpr float kWarpCells = 20.0F;
  CHECK(kWarpCells * ro::render::kWorldUnitsPerCell > TeleportUnits());
  // Et le retard d'une marche reste sous la MOITIÉ de ce saut-là : les deux cas
  // ne se touchent pas.
  CHECK(ChaseLagUnits() < kWarpCells * ro::render::kWorldUnitsPerCell / 2.0F);
}
