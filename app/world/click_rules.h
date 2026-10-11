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
