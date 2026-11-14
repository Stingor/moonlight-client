#include "shell/backdrop_camera.h"

#include <cmath>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"

using moonclient::BackdropCamera;
using moonclient::kBackdropDistanceFactor;
using moonclient::kBackdropSpinDegreesPerSecond;
using moonclient::kBackdropTiltDegrees;

namespace {

// Un point de visée qui n'est PAS l'origine, sur les trois axes. Une pose
// juste par hasard le serait aussi à l'origine ; celle-ci ne pardonne pas
// qu'on oublie de partir du point regardé.
constexpr glm::vec3 kSomewhere{1500.0F, 40.0F, -900.0F};

constexpr std::uint32_t kOneSecondMs = 1000;

}  // namespace

TEST_CASE("Décor : l'œil est AU-DESSUS du point regardé", "[app][backdrop]") {
  // 🔴 LE SIGNE DE L'ÉLÉVATION, ET IL EST LE DÉFAUT QUI SE CACHE LE MIEUX. Le
  // moteur du client d'origine veut l'opposé du nôtre — son axe vertical pointe
  // vers le bas —, et un chiffre repris de lui sans changer de repère y donne
  // une caméra sous le terrain. Là-bas, son garde-fou la raccroche au sol et le
  // réglage a l'air de ne rien faire ; ici, on verrait le dessous du monde.
  const ro::render::Camera camera = BackdropCamera(kSomewhere, 0);
  const glm::vec3 eye = camera.Position();

  CHECK(eye.y > kSomewhere.y);
  // Et de la hauteur qu'un décor demande, pas de trois centimètres : assez haut
  // pour découvrir la ville.
  CHECK(eye.y - kSomewhere.y ==
        Catch::Approx(camera.distance * std::sin(glm::radians(camera.pitch_degrees))).margin(0.01));
}

TEST_CASE("Décor : il regarde bien le point qu'on lui donne", "[app][backdrop]") {
  const ro::render::Camera camera = BackdropCamera(kSomewhere, 0);
  CHECK(camera.look_at.x == Catch::Approx(kSomewhere.x));
  CHECK(camera.look_at.y == Catch::Approx(kSomewhere.y));
  CHECK(camera.look_at.z == Catch::Approx(kSomewhere.z));
}

TEST_CASE("Décor : plus bas et plus PRÈS que la vue de jeu", "[app][backdrop]") {
  // La mise en scène, en deux phrases : on s'abaisse pour découvrir la ville,
  // on s'approche pour qu'elle remplisse le cadre derrière un formulaire.
  //
  // ⚠ PLUS PRÈS, ET NON PLUS LOIN. Le décor reculait d'abord ; le joueur a
  // réglé la mise en scène à l'écran et l'a rapprochée. Ce qui compte pour cet
  // essai n'est pas le chiffre — il est en dur juste à côté — mais qu'il soit
  // bien LU : une pose écrite et jamais posée se voit à l'écran, pas ici.
  const ro::render::Camera camera = BackdropCamera(kSomewhere, 0);

  CHECK(camera.pitch_degrees < ro::render::kGameElevationDegrees);
  CHECK(camera.pitch_degrees == Catch::Approx(kBackdropTiltDegrees));
  CHECK(camera.distance < ro::render::kGameViewDistance);
  CHECK(camera.distance == Catch::Approx(ro::render::kGameViewDistance * kBackdropDistanceFactor));
}

TEST_CASE("Décor : le champ est celui du JEU, pas celui de l'éditeur", "[app][backdrop]") {
  // ⚠ `WholeMapCamera` emploie quarante-cinq degrés : c'est la vue
  // d'INSPECTION, qui cadre la carte entière vue de loin, inutilisable comme
  // fond. Le champ étroit est ce qui donne à
  // Ragnarok son aspect presque orthographique, et le décor le garde.
  constexpr float kEditorFovDegrees = 45.0F;
  const ro::render::Camera camera = BackdropCamera(kSomewhere, 0);

  CHECK(camera.fov_degrees == Catch::Approx(ro::render::kGameFovDegrees));
  CHECK(camera.fov_degrees != Catch::Approx(kEditorFovDegrees));
}

TEST_CASE("Décor : l'orbite avance de sa vitesse, chaque seconde", "[app][backdrop]") {
  // 🔴 NOTRE CAP DÉCROÎT : il est l'opposé de celui du client. Le faire croître
  // ferait tourner le décor à l'envers.
  constexpr float kFullTurn = 360.0F;
  CHECK(BackdropCamera(kSomewhere, 0).yaw_degrees == Catch::Approx(0.0F));
  CHECK(BackdropCamera(kSomewhere, kOneSecondMs).yaw_degrees ==
        Catch::Approx(kFullTurn - kBackdropSpinDegreesPerSecond));
  CHECK(BackdropCamera(kSomewhere, 10 * kOneSecondMs).yaw_degrees ==
        Catch::Approx(kFullTurn - 10.0F * kBackdropSpinDegreesPerSecond));
}

TEST_CASE("Décor : l'orbite se replie sur un tour", "[app][backdrop]") {
  // 🔴 CE N'EST PAS L'IMAGE QUI LE DEMANDE — mille degrés se dessinent comme
  // deux cent quatre-vingts — MAIS LE FLOTTANT. Un écran de connexion laissé
  // ouvert une nuit accumule deux millions de degrés, et l'incrément de six s'y
  // perd dans les décimales que la mantisse n'a plus : l'orbite s'arrêterait
  // toute seule, sans que rien ne le dise.
  constexpr std::uint32_t kOneMinuteMs = 60 * kOneSecondMs;
  // Une minute d'orbite fait un tour complet à six degrés par seconde.
  CHECK(BackdropCamera(kSomewhere, kOneMinuteMs).yaw_degrees == Catch::Approx(0.0F).margin(0.01));

  // Et une nuit entière reste dans le tour.
  constexpr std::uint32_t kEightHoursMs = 8U * 3600U * kOneSecondMs;
  const float after_a_night = BackdropCamera(kSomewhere, kEightHoursMs).yaw_degrees;
  CHECK(after_a_night >= 0.0F);
  CHECK(after_a_night < 360.0F);
}
