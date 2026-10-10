#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/render/sprite_catalog.h"

namespace ro::render {

// CE QUE FAIT UN ACTEUR EN MODÈLE, et au rythme de quoi.
//
// Un modèle ne connaît que cinq mouvements, chacun un FICHIER d'animation :
// l'attente, la marche, l'attaque, la mort et la blessure. L'attente vient du
// fichier du modèle lui-même ; les quatre autres d'un dossier à part, rangées
// par le NUMÉRO que le nom du modèle porte après son premier soulignement.
// Un mouvement dont le fichier manque laisse le modèle dans sa pose de REPOS —
// l'Emperium n'a que l'attente, le drapeau de guilde n'a rien du tout.
enum class ActorModelMotion : std::uint8_t {
  kStand = 0,
  kMove = 1,
  kAttack = 2,
  kDead = 3,
  kDamage = 4,
};

inline constexpr std::array<std::string_view, 5> kActorModelMotionNames = {
    "stand", "move", "attack", "dead", "damage"};

// LE MOUVEMENT QU'UNE ACTION DU CLIENT DEMANDE, par le rang de l'action — huit
// fois son numéro, comme le client les range. Rien pour une action qui n'a pas
// de mouvement de modèle : le modèle retombe alors dans sa pose de repos.
[[nodiscard]] std::optional<ActorModelMotion> ActorModelMotionOfAction(std::uint32_t action_rank);

// Le numéro d'un modèle, lu dans son nom : les chiffres qui suivent le premier
// soulignement, jusqu'au point, quatre au plus. Rien sans soulignement.
[[nodiscard]] std::optional<std::int32_t> ActorModelIndex(std::string_view resource_name);

// La classe dont le nom désigne le fichier d'ATTENTE d'un numéro de modèle.
[[nodiscard]] std::optional<std::uint32_t> ActorModelStandJob(std::int32_t model_index);

// Où lire l'animation d'ATTENTE d'un numéro de modèle : le fichier que la table
// des classes nomme pour lui. Rien pour un numéro hors table, ni pour le
// drapeau de guilde, que le client ne cherche jamais.
[[nodiscard]] std::optional<std::string> ActorModelStandPath(const SpriteCatalog& catalog,
                                                             std::int32_t model_index);

// Où lire l'animation d'un mouvement autre que l'attente, ou rien quand le
// client ne la cherche pas : un numéro hors table, ou celui du drapeau.
[[nodiscard]] std::optional<std::string> ActorModelMotionPath(std::int32_t model_index,
                                                              ActorModelMotion motion);

// Le dossier des modèles d'acteur, où l'attente se lit par le nom de la classe.
inline constexpr std::string_view kActorModelFolder = "data\\model\\3dmob\\";

// L'HORLOGE D'UN MODÈLE avance deux fois plus vite que le temps réel — et plus
// vite encore pendant une attaque dont la cadence est plus brève que l'unité.
inline constexpr float kActorModelClockRate = 2.0F;
[[nodiscard]] float ActorModelAttackSpeed(std::uint32_t attack_motion_ms);

// L'ANGLE DU CORPS pour une direction du serveur, en degrés du client.
[[nodiscard]] float ActorBodyDegrees(std::uint8_t direction);

// L'angle du corps tourné vers un point, dans le repère du client : rien
// quand le point est celui de l'acteur.
[[nodiscard]] std::optional<float> ActorBodyDegreesToward(float from_x, float from_z, float to_x,
                                                          float to_z);

// L'ORIENTATION D'UN MODÈLE : posée d'emblée à l'opposé du corps, puis ramenée
// vers lui de six degrés par tic du jeu tant que l'écart dépasse douze.
[[nodiscard]] float InitialActorModelYaw(float body_degrees);
[[nodiscard]] float TurnActorModelYaw(float yaw_degrees, float body_degrees);

// LE RESTE D'UN MODÈLE MORT : l'acteur s'en va, et une copie joue sa mort sur
// place — pleine pendant les premiers tics, puis s'effaçant jusqu'à s'éteindre.
inline constexpr std::int32_t kActorModelRemnantHoldTicks = 355;
inline constexpr std::int32_t kActorModelRemnantLastTick = 600;
[[nodiscard]] std::uint8_t ActorModelRemnantAlpha(std::int32_t tick);

}  // namespace ro::render
