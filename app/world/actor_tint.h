#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/render/actor_effect_table.h"
#include "ro/sim/actor.h"

#include "world/ailment_tints.h"

// LA COULEUR DU CORPS D'UN ACTEUR, depuis ce que le monde en tient : l'état du
// corps, les effets qu'il porte à leur âge, la pose, le fondu, la lumière de la
// case.
//
// Elle vit hors de la composition de la scène pour se vérifier sans garde-robe
// ni fenêtre : de la trame reçue à la couleur que le quad du corps recevra.
namespace moonclient {

// Blanc opaque : la couleur d'un corps que rien ne teint.
inline constexpr std::array<std::uint8_t, 4> kPlainTint = {255, 255, 255, 255};

// Un mélange bon marché : on ne lui demande que de ne pas se répéter d'un
// acteur à l'autre ni d'un instant au suivant.
[[nodiscard]] std::uint32_t ScrambleSeed(std::uint32_t seed);

// La graine du tirage de la traînée colorée d'un acteur, à l'âge donné en tics.
[[nodiscard]] std::uint32_t BlurFlickerSeed(std::uint32_t actor_id, std::uint32_t age_ticks);

// La teinte de l'acteur entier à cet instant : gris sombre pour un pétrifié,
// gris pour qui se pétrifie, bleu de glace pour un gelé, rose pour un
// empoisonné, magenta sous le poison mortel, rouge pour un maudit — voir
// `ro::render::AilmentTint` ; celle de ses effets, à leur âge, sinon ; blanc
// sans effet.
[[nodiscard]] std::array<std::uint8_t, 4> TintOf(const ro::sim::Actor& actor, std::uint32_t now_ms);

// LA TEINTE D'UN ACTEUR à cette image, les composantes TIRÉES AU SORT posées.
//
// ⚠ `render` compose la teinte mais ne tire rien : il doit rester sans hasard,
// sans quoi une image de référence ne se compare plus à rien. La table dit
// qu'une composante est tirée — `kTintRandom` —, et c'est ici qu'on tire.
//
// `EF_MAGICCRASHER2` en est le seul cas connu : le client y refait ses trois
// composantes à chaque tic, pendant soixante tics, d'un `rand() % 255`.
[[nodiscard]] std::array<std::uint8_t, 4> TintAt(const ro::sim::Actor& actor, std::uint32_t now_ms);

// LA MÊME, avec ce que la liste de l'acteur ne dit pas : l'écriture de la
// traînée colorée à son rang, et les images sans tic qui font remonter la
// couleur — voir `ro::render::TintContext`.
[[nodiscard]] std::array<std::uint8_t, 4> TintAt(
    const ro::sim::Actor& actor, std::uint32_t now_ms,
    const std::optional<ro::render::TintEveryTickWrite>& every_tick_write,
    const ro::render::TintIdleFramesSince& idle_frames_since);

// LA TEINTE DE SES SEULS EFFETS, sans celle des états : voir `TintAt`.
[[nodiscard]] std::array<std::uint8_t, 4> EffectsTintAt(
    const ro::sim::Actor& actor, std::uint32_t now_ms,
    const std::optional<ro::render::TintEveryTickWrite>& every_tick_write,
    const ro::render::TintIdleFramesSince& idle_frames_since);

// L'EFFET `effect_id` VIT-IL ENCORE sur l'acteur, à son âge ? La liste de
// l'acteur garde ses effets au-delà de leur vie : c'est la durée qui tranche.
[[nodiscard]] bool StillCarriesEffect(const ro::sim::Actor& actor, std::uint32_t effect_id,
                                      std::uint32_t now_ms);

// LES EFFETS QU'UN ACTEUR JOUE À CET INSTANT, avec leur ÂGE en tics, rangés
// dans `played`, qu'elle vide d'abord : un appelant qui les relit pour chaque
// acteur y garde sa mémoire d'un acteur à l'autre.
//
// C'est la forme que `render` attend : le monde garde des instants, la table
// compte en tics, et cette fonction fait la conversion une fois pour toutes.
void PlayedEffectsOf(const ro::sim::Actor& actor, std::uint32_t now_ms,
                     std::vector<ro::render::ActorEffectPlay>& played);

// Ce que la scène sait du corps en plus de l'acteur : la traînée et les images
// sans tic, l'alpha de sa pose et celui de sa disparition, l'éclat d'un objet
// au sol, et la lumière de sa case.
struct BodyTintInputs {
  std::optional<ro::render::TintEveryTickWrite> blur_write;
  ro::render::TintIdleFramesSince idle_frames_since;
  std::uint8_t pose_alpha = 255;
  std::uint8_t vanish_alpha = 255;
  bool sparkling = false;
  // Les facteurs de la lumière sous la case, rouge, vert, bleu ; rien hors
  // carte éclairée.
  std::optional<std::array<float, 3>> ground_light;
  bool cold = false;
  // LA TEINTE QUI RESTE après les changements d'état, quand la scène la tient.
  // Nulle : celle que les états imposent à cet instant, sans mémoire.
  const AilmentTints* ailment_tints = nullptr;
};

// LA COULEUR QUE LE QUAD DU CORPS REÇOIT à cette image, rouge, vert, bleu,
// alpha : celle que le renderer multiplie à chaque texel du sprite.
[[nodiscard]] std::array<std::uint8_t, 4> BodyTintOf(const ro::sim::Actor& actor,
                                                     std::uint32_t now_ms,
                                                     const BodyTintInputs& inputs);

}  // namespace moonclient
