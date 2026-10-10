#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "ro/render/skill_held_pose.h"

namespace ro::render {

// ── LES POSES MENÉES QUE LA TABLE DES COMPÉTENCES NE DIT PAS ────────────────
//
// Une pose MENÉE est une pose que le client conduit lui-même, image par image,
// au lieu de dérouler l'animation du fichier : celle d'une compétence à
// l'annonce ou à l'impact (voir `skill_held_pose.h`), et les deux ci-dessous.
// Tant qu'elle tient, une blessure qui porte N'A PAS LIEU ; et la motion
// suivante que le client pose la termine.

// L'EMPLOI ORDINAIRE de seize métiers mène une pose minutée sur l'action
// d'incantation, une image toutes les quatre avances : la danse et le chant un
// instant sur l'image zéro, le ninja des images deux à cinq, le canonnier de
// zéro à trois. Rien pour les autres métiers.
[[nodiscard]] std::optional<SkillHeldPose> UseLedPoseOf(std::uint32_t job) noexcept;

// ── LES ÉTATS D'ACTION QUI MÈNENT LEUR POSE ────────────────────────────────
//
// Un état d'action de 16 à 57 — hors 29, 45, 52 à 54 — ne déroule PAS
// l'animation du rang qu'il pose : le client y mène une pose image par image,
// qu'il soit posé par une incantation, un emploi ou un coup. Le Sniper qui arme
// Sharpshooting (état 18) tient ainsi l'image zéro de son tir, immobile, toute
// la barre ; le taekwon qui lance son coup de pied pivotant (état 32) passe par
// les images 4, 5, 6, 7 et 2 de son rang onze.
//
// ⚠ L'ÉCHÉANCE NE REND PAS L'ACTEUR À SON ANIMATION, et la fin de la barre non
// plus : passé la durée de la pose, l'image montrée en dernier reste, jusqu'à
// la motion suivante — sauf pour les états que la fin ramène au repos, voir
// `StateRestsWhenFinished`. Seule la garde de la blessure cesse à l'échéance.
//
// ⚠ L'IMAGE EST CELLE DU RANG QUE L'ÉTAT POSE, sauf trois cas : la dernière
// image de l'état 20 est l'image 5 de la marche, l'état 31 joue l'attaque (rang
// 10) quel que soit ce que l'état pose, et deux états lisent au-delà : le 26
// donne d'autres images à la lignée du Soul Linker, le 38 tire au sort entre
// deux poses.
inline constexpr std::int8_t kLedCastState = 19;
inline constexpr std::uint32_t kLedCastPoseMs = 3000;

// Vrai quand cet état mène sa pose au lieu de jouer son rang.
[[nodiscard]] bool StateLeadsPose(std::int8_t action_state) noexcept;

// L'image que mène l'état, et si sa durée court encore.
struct StateLedFrame {
  HeldPoseFrame frame;
  // Vrai tant que la pose n'a pas atteint sa durée : c'est ce qui écarte la
  // blessure. L'image, elle, reste au-delà.
  bool running = false;
};

// Ce que montre l'état `action_state`, posé sur le rang `rank`, `elapsed_ms`
// après sa pose ; rien pour un état qui ne mène pas de pose. `tick_ms` est la
// durée d'une avance de l'acteur. `draw` est le tirage au sort du client, zéro
// ou un, que seuls les états 37, 38 et 41 lisent. `job` est le métier montré,
// que seul l'état 26 lit.
[[nodiscard]] std::optional<StateLedFrame> StateLedFrameAt(std::int8_t action_state,
                                                           std::uint8_t rank,
                                                           std::uint32_t elapsed_ms,
                                                           std::uint32_t tick_ms, std::uint8_t draw,
                                                           std::uint32_t job) noexcept;

// 🔴 VRAI QUAND LA FIN DU GESTE D'ÉTAT `action_state` RAMÈNE L'ACTEUR AU REPOS,
// à l'image même où il se déclare fini : l'attente, l'attaque et sa variante,
// la blessure, le ramassage, l'emploi ordinaire, l'état 12, et 45, 49, 51, 52
// à 54, 56 à 59. Une pose menée MINUTÉE s'y déclare finie à son échéance, et le
// reste de la passe ne se joue pas. Les autres états gardent leur dernière
// image — l'incantation, l'assise, les états qui mènent leur pose.
[[nodiscard]] bool StateRestsWhenFinished(std::int8_t action_state) noexcept;

// L'ÉTAT QUE L'ACTEUR QUITTE au repos qui suit un geste d'état `action_state` :
// le client inscrit 49 comme l'emploi ordinaire, et fait passer 51 à l'attaque
// quand sa pose finit — il se repose donc en garde. Les autres, tels quels.
[[nodiscard]] std::int8_t StateLeftByFinishedMotion(std::int8_t action_state) noexcept;

// ── CE QU'UN ÉTAT FAIT NAÎTRE EN SE POSANT ─────────────────────────────────
//
// Trois familles d'états touchent aux effets de leur PORTEUR au moment où le
// client les pose, sans décalage : 19 et 46 retirent tout ce qu'il porte de
// deux effets avant d'en faire naître deux, 21 à 25 font naître l'effet 449.
// L'état 19 n'en fait rien pour un Doram.
enum class StateEffectKind : std::uint8_t {
  // Retire TOUTES les instances de l'effet que le porteur tient.
  kRemove,
  // Fait naître l'effet sur le porteur.
  kSpawn,
};

struct StateEffectStep {
  StateEffectKind kind = StateEffectKind::kSpawn;
  std::uint16_t effect_id = 0;
};

// Les gestes d'effet de l'état `action_state` pour un porteur de métier montré
// `job`, dans l'ordre du client ; vide pour les autres états.
[[nodiscard]] std::span<const StateEffectStep> StateEffectsOf(std::int8_t action_state,
                                                              std::uint32_t job) noexcept;

// Vrai quand `at_ms` tombe dans [début, début + durée).
[[nodiscard]] constexpr bool LedPoseWithin(std::uint32_t at_ms, std::uint32_t start_ms,
                                           std::uint32_t duration_ms) noexcept {
  return at_ms >= start_ms && at_ms - start_ms < duration_ms;
}

}  // namespace ro::render
