#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/formats/gnd.h"
#include "ro/render/camera.h"
#include "ro/render/model_mesh.h"

namespace ro::render {

// CE QUI SE TIENT ENTRE L'ŒIL ET LE JOUEUR.
//
// À chaque image, le client trace un segment de l'œil de la caméra vers le
// torse de son joueur, et marque chaque pose du décor dont la BOÎTE ORIENTÉE le
// coupe. Tout ce fichier compte DANS SON REPÈRE, bornes et tolérances
// comprises : l'ordonnée y descend, la profondeur y croît avec la rangée, et
// l'origine est au centre de la carte. Ce qui vient de notre monde change de
// repère à l'entrée — voir `ClientFrame`.

// LE PASSAGE DE NOTRE MONDE AU REPÈRE DU CLIENT, pour une carte.
//
// Deux axes retournés, et deux origines décalées : notre abscisse part du bord
// de la carte et la sienne de son centre ; notre profondeur décroît avec la
// rangée et part d'un cube au-delà du bord.
struct ClientFrame {
  float x_offset = 0.0F;
  float z_offset = 0.0F;

  [[nodiscard]] glm::vec3 PointOf(const glm::vec3& world) const noexcept {
    return {world.x - x_offset, -world.y, z_offset - world.z};
  }
  [[nodiscard]] static glm::vec3 DirectionOf(const glm::vec3& world) noexcept {
    return {world.x, -world.y, -world.z};
  }
};

[[nodiscard]] ClientFrame ClientFrameOf(const formats::GndMap& gnd);

// LA BOÎTE ORIENTÉE D'UNE POSE DU DÉCOR, dans le repère du client : l'emprise
// du modèle à son image zéro, portée par la pose. Le client la calcule une
// fois, au chargement, et elle ne suit pas l'animation.
struct SightBox {
  // Le long de chacun des trois axes.
  std::array<float, 3> half_extents{};
  glm::vec3 center{};
  // Normés.
  std::array<glm::vec3, 3> axes{};
  // ET SON EMPRISE ALIGNÉE AUX AXES : les huit coins portés, bornés.
  glm::vec3 lowest{};
  glm::vec3 highest{};
};

[[nodiscard]] SightBox SightBoxOf(const glm::mat4& instance, const ModelMesh& mesh,
                                  const ClientFrame& frame);

// LE SEGMENT DU REGARD, dans le repère du client.
struct SightSegment {
  glm::vec3 origin{};
  // Normée.
  glm::vec3 direction{};
  float length = 0.0F;
};

// LE TEST DU CLIENT : axe par axe, l'entrée et la sortie de la droite dans la
// tranche de la boîte, bornées à ±999 999. Une droite parallèle à une tranche
// — à un cent-millième près — la coupe si son origine y tient.
//
// ⚠ ET SA CONCLUSION N'EST PAS CELLE D'UN SEGMENT ORDINAIRE : quand l'origine
// est DANS la boîte, c'est la SORTIE qui doit tomber avant la longueur. Un œil
// enfermé dans un modèle qui englobe aussi le joueur ne le voile donc pas.
[[nodiscard]] bool SegmentMeetsBox(const SightSegment& segment, const SightBox& box);

// LE SEGMENT QUE LE CLIENT TRACE VERS SON JOUEUR.
//
// Il part de l'œil — remonté au terrain quand il y plongerait — et passe par le
// point de l'écran situé
// vingt-trois pixels — rapportés à une hauteur de 480 — au-dessus de l'ancre du
// sprite, c'est-à-dire des pieds projetés. Le client tronque ces coordonnées à
// l'entier, comme sa propre ancre.
//
// 🔴 SA LONGUEUR ADDITIONNE UNE ALTITUDE ET UN RECUL : l'ordonnée du joueur
// dans son repère — qui DESCEND — plus le recul de la caméra, moins seize. Sur
// un terrain haut, le segment raccourcit d'autant ; c'est la règle du client.
//
// `viewport_width` et `viewport_height` sont en pixels, ceux de la fenêtre.
[[nodiscard]] SightSegment PlayerSightSegment(const Camera& camera, float aspect_ratio,
                                              std::int32_t viewport_width,
                                              std::int32_t viewport_height,
                                              const glm::vec3& player_world,
                                              const ClientFrame& frame);

// CE QUE LA CAMÉRA VOIT : ses six plans, tournés vers l'intérieur, dans le
// repère du client.
struct SightFrustum {
  std::array<glm::vec4, 6> planes{};
};

[[nodiscard]] SightFrustum SightFrustumOf(const Camera& camera, float aspect_ratio,
                                          const ClientFrame& frame);

// Où tombe une boîte par rapport à ce que la caméra voit.
enum class FrustumSide : std::uint8_t { kOutside, kInside, kAcross };

// Plan par plan, comme le client : la boîte est dehors dès qu'elle l'est d'un
// seul, à cheval si elle coupe l'un d'eux.
[[nodiscard]] FrustumSide BoxAgainstFrustum(const SightFrustum& frustum, const SightBox& box);

}  // namespace ro::render
