#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/gr2_model.h"
#include "ro/formats/image.h"
#include "ro/render/actor_model_pose.h"
#include "ro/render/camera.h"
#include "ro/render/map_effect_math.h"

namespace ro::render {

// UN ACTEUR EN MODÈLE, DANS LA SCÈNE : ce que le jeu en montre à une image.
//
// Le client ne dessine pas ces acteurs comme le décor. Il déforme leurs sommets
// lui-même, les éclaire lui-même d'une lumière qui n'a rien à voir avec celle
// de la carte, et les passe à la même file de dessin que les sprites. Ce module
// reproduit ce calcul, sans GPU : il rend des triangles déjà placés dans le
// monde, déjà teintés, que le renderer n'a plus qu'à peindre.
//
// ⚠ LE REPÈRE. Le client calcule dans le sien — l'ordonnée et la profondeur
// opposées aux nôtres — et c'est là que ses règles s'écrivent : son décalage de
// deux unités, sa rotation d'après le nom du fichier, sa lumière tirée de la
// caméra. On les applique donc dans SON repère, puis on convertit.

// Un modèle chargé, prêt à poser.
struct ActorModelResource {
  formats::GrannyScene scene;
  // Pour chaque maillage, le rang dans le squelette de chacun de ses os.
  std::vector<std::vector<std::int32_t>> bone_maps;
  // VRAI QUAND LE NOM DU FICHIER CONTIENT « 90 » : le modèle a été exporté
  // debout, et un quart de tour le redresse. Sinon, c'est un demi-tour.
  bool quarter_turn_upright = false;
};

[[nodiscard]] ActorModelResource MakeActorModelResource(formats::GrannyScene scene,
                                                        std::string_view file_name);

// COMMENT LE MODÈLE SE POSE. Tous se redressent de la même façon ; ce qui
// change est la place et la lumière. Un acteur ordinaire se tient DEBOUT,
// décalé de deux unités. Le drapeau de guilde se pose SUR sa case, porte
// l'emblème de sa guilde et s'éclaire d'une autre lumière. Le RESTE d'un
// modèle mort se pose sur sa case, et s'éclaire d'une troisième lumière.
enum class ActorModelStance : std::uint8_t { kUpright, kGrounded, kRemnant };

// Le maillage d'un drapeau qui porte l'EMBLÈME de sa guilde.
inline constexpr std::size_t kGuildFlagEmblemMesh = 1;

struct ActorModelPlacement {
  // Les pieds de l'acteur, dans NOTRE repère.
  glm::vec3 ground_position{0.0F};
  // L'orientation du modèle, en degrés, dans le repère du client.
  float yaw_degrees = 0.0F;
  ActorModelStance stance = ActorModelStance::kUpright;
  // L'opacité de chaque sommet : pleine pour un acteur, décroissante pour un
  // reste qui s'efface.
  std::uint8_t alpha = 255;
  // La classe de l'acteur : un maillage d'une classe peut faire naître un effet.
  // Voir `ActorModelMeshEvent`.
  std::uint32_t job = 0;
};

// UN EFFET QU'UN MAILLAGE FAIT NAÎTRE en se peignant.
//
// Une seule classe en porte un : le dragon zombie, dont le vingt-neuvième
// maillage souffle l'effet 373 depuis son soixante-cinquième sommet, à chaque
// peinture. ⚠ SON MODÈLE N'EST PAS LIVRÉ, si bien que rien de tout cela ne se
// voit dans l'installation : la règle est portée pour le jour où il le serait.
//
// L'écart est le sommet DÉFORMÉ tel quel, dans le repère du fichier, que la
// naissance de l'effet fait tourner de l'opposé de l'orientation du modèle —
// voir `RotatedSpawnOffset`.
struct ActorModelMeshEvent {
  std::uint32_t effect_id = 0;
  // Où l'effet naît, dans NOTRE repère : les pieds, plus l'écart tourné.
  glm::vec3 position{0.0F};
};

inline constexpr std::uint32_t kZombieDragonBreathEffect = 373;
inline constexpr std::size_t kZombieDragonBreathMesh = 28;
inline constexpr std::size_t kZombieDragonBreathVertex = 64;

// L'ÉCART D'UN EFFET QUI NAÎT PAR SON IDENTIFIANT, tourné dans le plan du sol
// d'un angle en degrés — tronqué à l'entier, puis lu dans la table de degrés
// du client, négatifs compris. Dans le repère du client.
[[nodiscard]] glm::vec3 RotatedSpawnOffset(const glm::vec3& client_offset, float angle_degrees);

// La direction de la lumière dans l'espace de la VUE du client : un point à
// mille unités sur l'axe de droite. Voir `UprightLightDirection`.
inline constexpr glm::vec3 kActorModelViewLight{1000.0F, 0.0F, 0.0F};
// Celle d'un reste : trois cents unités vers le HAUT de l'écran — l'axe vertical
// de la vue du client descend.
inline constexpr glm::vec3 kActorModelRemnantViewLight{0.0F, -300.0F, 0.0F};

// Ce qui se peint d'un maillage : des triangles, sommet par sommet.
struct ActorModelVertex {
  glm::vec3 position{0.0F};
  glm::vec2 uv{0.0F};
  std::array<std::uint8_t, 4> color{255, 255, 255, 255};
};

// Une texture, par son identité : la ressource qui la porte et son rang, ou
// l'emblème qu'un drapeau reçoit.
struct ActorModelTexture {
  const void* owner = nullptr;
  std::uint64_t key = 0;
  const formats::GrannyTextureImage* image = nullptr;
  const formats::Image* emblem = nullptr;
};

struct DrawnActorModelMesh {
  ActorModelTexture texture;
  std::vector<ActorModelVertex> vertices;
};

struct DrawnActorModel {
  std::vector<DrawnActorModelMesh> meshes;
  // Les effets que ses maillages font naître à cette peinture.
  std::vector<ActorModelMeshEvent> mesh_events;
};

// La matrice qui mène un point du modèle au monde du CLIENT : le redressement,
// l'orientation, puis la place — décalée de deux unités pour qui se tient
// debout.
[[nodiscard]] glm::mat4 ActorModelToClientWorld(const ActorModelResource& resource,
                                                const ActorModelPlacement& placement);

// Notre repère et celui du client : ordonnée et profondeur opposées.
[[nodiscard]] glm::vec3 ClientFromWorld(const glm::vec3& world);
[[nodiscard]] glm::vec3 WorldFromClient(const glm::vec3& client);

// LA LUMIÈRE D'UN MODÈLE DEBOUT : le point de `kActorModelViewLight`, ramené de
// l'espace de la vue au monde du client COMME UN POINT — œil compris —, puis
// normé. Elle dépend donc de là où se tient la caméra, et c'est la règle.
[[nodiscard]] glm::vec3 UprightLightDirection(const Camera& camera, const MapEffectFrame& map);

// LA LUMIÈRE DU DRAPEAU DE GUILDE : la direction de visée, passée par la matrice
// de vue du client comme un point, puis normée.
[[nodiscard]] glm::vec3 GroundedLightDirection(const Camera& camera, const MapEffectFrame& map);

// LA LUMIÈRE D'UN RESTE : le point de `kActorModelRemnantViewLight`, ramené au
// monde comme celui d'un modèle debout.
[[nodiscard]] glm::vec3 RemnantLightDirection(const Camera& camera, const MapEffectFrame& map);

// La luminance d'un sommet, de 0 à 255.
[[nodiscard]] std::uint8_t ActorModelLuminance(ActorModelStance stance, const glm::vec3& normal,
                                               const glm::vec3& light);

// Pose, éclaire et place le modèle. Un maillage sans texture ne se dessine
// pas ; celui de l'emblème d'un drapeau prend `emblem`, et disparaît sans lui.
[[nodiscard]] DrawnActorModel ComposeActorModel(const ActorModelResource& resource,
                                                const ActorModelAnimation* animation,
                                                float local_seconds,
                                                const ActorModelPlacement& placement,
                                                const Camera& camera, const MapEffectFrame& map,
                                                const formats::Image* emblem);

// LE CENTRE DU RECTANGLE QU'ON CLIQUE, dans notre repère : l'origine du modèle,
// posée comme lui.
[[nodiscard]] glm::vec3 ActorModelPickCenter(const ActorModelPlacement& placement);

// Le côté de la boîte du clic, en unités du monde. Le client en fait un carré
// d'écran de demi-côté `√(côté² / 2)` autour du centre. Deux grands monstres
// en ont une de trente — leurs modèles ne sont pas livrés.
inline constexpr float kActorModelPickBoxSide = 10.0F;
inline constexpr float kLargeActorModelPickBoxSide = 30.0F;
[[nodiscard]] float ActorModelPickBoxSide(std::uint32_t job);

// La pose d'une classe : sur sa case pour le drapeau de guilde, debout sinon.
[[nodiscard]] ActorModelStance ActorModelStanceOf(std::uint32_t job);

}  // namespace ro::render
