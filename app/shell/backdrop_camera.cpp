#include "shell/backdrop_camera.h"

#include <cmath>

namespace moonclient {
namespace {

constexpr float kMillisecondsPerSecond = 1000.0F;
constexpr float kFullTurnDegrees = 360.0F;

}  // namespace

ro::render::Camera BackdropCamera(glm::vec3 look_at, std::uint32_t elapsed_ms) {
  ro::render::Camera camera;
  camera.look_at = look_at;
  // ⚠ LE RECUL DE JEU COMME POINT DE DÉPART, et non la distance d'une caméra
  // qu'on aurait laissée quelque part : ici il n'y a pas de partie, donc pas de
  // molette. `kGameViewDistance` est le seul repère qui existe déjà.
  camera.distance = ro::render::kGameViewDistance * kBackdropDistanceFactor;
  // 🔴 POSITIVE, ET C'EST TOUT LE SUJET DE CETTE LIGNE. Le champ se lit
  // « hauteur au-dessus de l'horizon » : `Camera::Position` en tire
  // `y = look_at.y + distance × sin(élévation)`, donc l'œil monte. Le moteur du
  // client d'origine veut l'OPPOSÉ — son axe vertical pointe vers le bas —, et
  // s'y tromper ne donne pas une vue basculée mais une caméra sous le terrain,
  // que son garde-fou raccroche au sol : une vue à ras de terre, identique pour
  // toute valeur, indiscernable d'un réglage qui ne répond pas. Nous n'avons pas
  // ce garde-fou : chez nous, le mauvais signe montre le dessous du monde.
  camera.pitch_degrees = kBackdropTiltDegrees;
  // ⚠ LE CHAMP DU JEU, ET NON CELUI DE L'ÉDITEUR : voir l'en-tête.
  camera.fov_degrees = ro::render::kGameFovDegrees;
  // ⚠ REPLIÉ SUR UN TOUR. Ce n'est pas l'image qui en a besoin — un angle de
  // mille degrés se dessine comme deux cent quatre-vingts — mais le FLOTTANT :
  // un écran de connexion laissé ouvert une nuit accumulerait deux millions de
  // degrés, et l'incrément de six finirait par se perdre dans les décimales que
  // la mantisse n'a plus.
  const float turned_degrees =
      (static_cast<float>(elapsed_ms) / kMillisecondsPerSecond) * kBackdropSpinDegreesPerSecond;
  // ⚠ ET RETRANCHÉ : notre cap est l'OPPOSÉ de celui du client
  // (`NativeViewOf`), où la mise en scène fait croître le cap. Le décor tourne
  // donc dans le même sens quand le nôtre décroît.
  const float turned_natively = std::fmod(turned_degrees, kFullTurnDegrees);
  camera.yaw_degrees = turned_natively > 0.0F ? kFullTurnDegrees - turned_natively : 0.0F;
  return camera;
}

}  // namespace moonclient
