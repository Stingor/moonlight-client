#pragma once

#include <cstdint>

#include "ro/render/cursor.h"
#include "ro/sim/actor.h"

#include "world/mouse_engagement.h"

namespace moonclient {

// ── LES RÈGLES DU CLIC GAUCHE SUR LE MONDE ──────────────────────────────────
//
// Ce que le clic décide sans rien envoyer : à part du routage, qui parle au
// serveur, pour qu'elles s'éprouvent seules.

// LA SOURIS FIGÉE NE TOUCHE PLUS AU MONDE : ni marche, ni attaque, ni contact,
// ni forme de curseur pour une entité, ni objet lâché sur le sol. Restent
// actifs l'appui qui désarme la compétence armée, le menu du clic droit et le
// relâchement du bouton.
[[nodiscard]] constexpr bool WorldTakesClicks(MouseRoute route) noexcept { return !route.frozen; }

// CTRL TENU, LE CLIC AU SOL NE BOUGE PAS LE PERSONNAGE : ni marche, ni
// demi-tour — au clic comme à sa reprise, bouton tenu. Un sort au sol part
// quand même : il se résout avant.
[[nodiscard]] constexpr bool GroundClickMoves(bool ctrl_held) noexcept { return !ctrl_held; }

// MAJ TENUE (sans `/noshift`), OU ALT : le clic sur un monstre est retenu. Il ne
// frappe pas, n'engage rien, et ne descend pas au sol non plus.
//
// ⚠ `/noshift` RELÈVE MAJ ICI, il ne l'inverse pas : la touche tenue sous le
// réglage laisse frapper.
[[nodiscard]] constexpr bool MonsterClickWithheld(bool shift_held, bool no_shift,
                                                  bool alt_held) noexcept {
  return (shift_held && !no_shift) || alt_held;
}

// La forme du curseur sur un monstre quand MAJ tenue retient le clic : la
// flèche, au lieu de l'épée. ALT ne change pas la forme. Toute autre forme passe
// telle quelle.
[[nodiscard]] ro::render::CursorShape ShapeUnderShift(ro::render::CursorShape shape,
                                                      const ro::sim::Actor* hovered,
                                                      bool shift_held, bool no_shift);

// ASSIS OU CACHÉ (Hiding), LE PERSONNAGE NE PREND PAS D'ORDRE D'ATTAQUE : le clic
// lève encore la flèche, mais aucun coup ne s'arme.
[[nodiscard]] bool TakesAttackOrder(const ro::sim::Actor& self);

// MONTÉ SUR UNE MONTURE DE TRÉSORERIE (l'état `kStatusAllRiding`), LE COUP NE
// PART PAS : à l'instant d'envoyer l'attaque, d'un coup comme sans relâche, le
// message « Unable to attack while riding. » s'écrit en rouge à la place.
// L'approche, l'engagement et la flèche ont eu lieu. Le serveur, lui, ne
// refuse que les skills sous cet état.
[[nodiscard]] bool RidingWithholdsAttack(const ro::sim::Actor& self);

// Les compétences qu'un personnage CACHÉ lance encore sur une entité, aux noms
// de la table des compétences du serveur.
inline constexpr std::uint16_t kGrimtoothSkillId = 137;    // AS_GRIMTOOTH
inline constexpr std::uint16_t kBackStabSkillId = 212;     // RG_BACKSTAP
inline constexpr std::uint16_t kShadowLeapSkillId = 529;   // NJ_SHADOWJUMP
inline constexpr std::uint16_t kShadowSlashSkillId = 530;  // NJ_KIRIKAGE
inline constexpr std::uint16_t kDarkSlashSkillId = 592;    // DE_SLASH

// ASSIS, LE PERSONNAGE NE PREND PAS L'ORDRE D'UNE COMPÉTENCE VISÉE SUR UNE
// ENTITÉ ; CACHÉ (Hiding), il ne prend que celles qui se lancent depuis
// l'ombre — voir les cinq identifiants ci-dessus. Le Cloaking n'est pas le
// Hiding. Le refus est muet : rien ne part, et la visée se désarme.
[[nodiscard]] bool TakesSkillOrder(const ro::sim::Actor& self, std::uint16_t skill_id);

// ── LA GARDE DE POIDS ───────────────────────────────────────────────────────
//
// À partir de cette part du poids maximum, en pour cent, le personnage ne
// prend plus d'ordre d'action sur une entité : ni attaque, ni compétence
// visée, ni suivi d'un joueur.
inline constexpr std::int64_t kOverweightActionPercent = 90;

// La compétence armée que la garde ne regarde pas du tout. La table du serveur
// y range GD_APPROVAL.
inline constexpr std::uint16_t kWeightUnguardedSkillId = 10000;
// La compétence armée que la garde laisse partir APRÈS avoir écrit son refus.
inline constexpr std::uint16_t kWeightWarnedSkillId = 40000;

enum class WeightVerdict : std::uint8_t {
  // Rien à dire, l'action part.
  kPasses,
  // Le refus s'écrit, et l'action part quand même.
  kWarnsAndPasses,
  // Le refus s'écrit, et l'action est abandonnée.
  kRefuses,
};

// CE QUE LA GARDE DE POIDS FAIT D'UN ORDRE. `skill_id` est la compétence armée,
// zéro pour une attaque ordinaire ou un suivi.
//
// La part se compte en ENTIERS, comme le serveur : cent fois le poids, divisé
// par le poids maximum ramené à un s'il est plus petit. Un poids que le serveur
// n'a pas encore dit, négatif, passe.
[[nodiscard]] WeightVerdict WeightGuardOf(std::int64_t weight, std::int64_t max_weight,
                                          std::uint16_t skill_id);

// UN CLIC QUI PARLE À UN PNJ : ni un portail, qui se franchit en marchant, ni un
// compagnon — pet, homoncule, mercenaire, élémentaire —, qui ne parle pas.
[[nodiscard]] bool ClickContactsNpc(const ro::sim::Actor& npc);

// L'ACTION DU COUP QUI PART À PORTÉE : sans relâche (sept) ou un seul (zéro).
//
// Un clic continu frappe sans relâche. Un clic qui ne se poursuit pas frappe
// AUSSI sans relâche tant que le bouton reste tenu sur cette attaque — c'est le
// relâchement qui l'arrête, par une demande d'arrêt —, et un seul coup s'il a
// été relâché avant d'arriver. Sans geste noté pour cette cible (une unité au
// sol), le clic décide seul.
[[nodiscard]] int BasicAttackAction(bool repeat, const PlayerAction& action,
                                    std::uint32_t target_id);

// ── LE DOUBLE-CLIC ──────────────────────────────────────────────────────────

// Le délai d'un double-clic, en millisecondes.
inline constexpr std::uint32_t kDoubleClickMs = 500;

// LE DOUBLE-CLIC EXIGE UNE SOURIS IMMOBILE : le second appui doit tomber moins de
// `kDoubleClickMs` après le premier, SANS AUCUN mouvement entre les deux — un
// aller-retour au même point ne compte pas. Il n'y a pas de tolérance.
//
// Un troisième appui ne fait pas un second double avec le deuxième.
class DoubleClickClock {
 public:
  // La souris a bougé.
  void NoteMotion() noexcept { armed_ = false; }

  // Un appui du bouton. Rend vrai s'il est le second d'un double-clic.
  [[nodiscard]] bool NotePress(std::uint32_t now_ms) noexcept;

 private:
  bool armed_ = false;
  std::uint32_t pressed_ms_ = 0;
};

}  // namespace moonclient
