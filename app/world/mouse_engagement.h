#pragma once

#include <cstdint>

#include "ro/sim/map_mode.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// ── L'ENGAGEMENT DE LA SOURIS ───────────────────────────────────────────────
//
// Un clic d'ACTION — frapper un monstre, viser un joueur, lancer une compétence
// sur quelqu'un — engage la souris sur l'entité cliquée. La flèche de sélection
// ne se montre que tant que l'engagement tient, et au-dessus de cette entité-là.
//
// L'engagement retombe de quatre façons : un appui frais n'importe où quand il
// est CONTINU, toute marche demandée au clic, la fin de l'action du personnage,
// et — quand il n'est pas continu — le moment où la souris, bouton relâché, ne
// désigne plus rien.
//
// C'est un état d'ENTRÉE, pas du monde : il n'a rien à faire dans ce qu'une
// capture réseau rejoue. Et il est distinct de la cible du mode Ciblage, qui vit
// dans le monde et que le mode vide quand on l'éteint.

// Un point de la fenêtre, en pixels.
struct PointerPx {
  float x = 0.0F;
  float y = 0.0F;
};

// Ce que le personnage est en train de faire au nom du dernier clic : c'est la
// fin de cette action qui fait le plus souvent retomber l'engagement.
enum class PlayerDeed : std::uint8_t {
  kNothing,
  kAttack,
  kSkillOnTarget,
  kPickup,
};

struct PlayerAction {
  PlayerDeed deed = PlayerDeed::kNothing;
  std::uint32_t target_id = 0;
  // Une attaque qui ne se poursuit pas : elle s'arrête au relâchement si elle
  // a déjà frappé, sinon au premier coup.
  bool stops_on_release = false;
  bool released = false;
  bool struck = false;
  // La cible a été vue vivante depuis le début de l'action : sa mort l'achève.
  // Une cible déjà morte, elle, fait attendre sans fin.
  bool target_seen_alive = false;
};

struct MouseEngagement {
  bool engaged = false;
  // Ctrl, ou `/noctrl`, au moment du clic d'action : l'engagement ne retombe
  // plus quand la souris quitte tout, mais au prochain appui. Il survit au
  // désengagement, jusqu'au clic d'action suivant.
  bool continuous = false;
  // La dernière entité cliquée pour agir. Elle survit au désengagement : la
  // flèche la retrouve si un nouvel engagement ne la remplace pas.
  std::uint32_t clicked_id = 0;
  // Un autre joueur survolé : il retient l'engagement jusqu'au premier
  // mouvement de la souris, même s'il s'éloigne du curseur.
  std::uint32_t hovered_player_id = 0;
  // Le bouton gauche est tenu, où qu'on l'ait enfoncé.
  bool left_held = false;
  // Il a été enfoncé depuis la dernière passe de la souris.
  bool fresh_press = false;
  PointerPx last_mouse;
  PlayerAction action;
  // La carte a annoncé le siège depuis l'arrivée : l'attaque ne se poursuit
  // plus. Une annonce qui le retire ne le lève pas ; seule une AUTRE carte le
  // fait.
  bool siege_map = false;
  // Le personnage est mort : la souris reste figée jusqu'à ce qu'il se relève
  // ou qu'une carte l'accueille.
  bool death_freeze = false;
  // UN COMMERCE EST OUVERT — boutique de PNJ ou de vote, échoppe, troc — : la
  // souris est figée dans le monde comme pendant un dialogue, et c'est ce que
  // fait le client d'origine. Posé à chaque image par la boucle.
  bool trade_freeze = false;
};

// CE QUI ARRÊTE LA SOURIS DANS LE MONDE.
struct MouseRoute {
  // Le personnage est mort, un dialogue de PNJ retient le monde, ou le coffre
  // est ouvert : la souris ne fait plus rien retomber ni ne lève rien, et la
  // flèche ne bouge plus.
  bool frozen = false;
  // Un sort au sol attend sa case : la souris ne désigne plus d'entité, mais la
  // flèche décide encore.
  bool suspended = false;

  [[nodiscard]] bool runs() const noexcept { return !frozen && !suspended; }
};

// Ce que la souris a sous elle à cette image.
struct EngagementFrame {
  // Une entité désignable sous le curseur, fenêtres comprises : la souris
  // au-dessus d'un panneau désigne encore ce qui est dessous.
  bool anything_under = false;
  // Un autre joueur sous le curseur, jamais soi. Zéro : aucun.
  std::uint32_t player_under = 0;
  // Ce joueur se frappe, et il vit.
  bool player_attackable = false;
  // Le curseur est au-dessus d'une fenêtre.
  bool over_interface = false;
  PointerPx mouse;
};

// Retombe, et oublie le joueur survolé. La dernière entité cliquée reste.
void Disengage(MouseEngagement& engagement);

// Un appui gauche, dans le monde ou sur une fenêtre. Engagé et continu, il
// dégage avant que le clic ne soit traité — le clic peut alors ré-engager sur
// sa propre cible. Ni le second appui d'un double-clic, ni une souris arrêtée
// ne dégagent.
void NoteLeftPress(MouseEngagement& engagement, bool second_of_double, MouseRoute route);

// Le bouton gauche relâché, où que ce soit.
void NoteRelease(MouseEngagement& engagement);

// La fenêtre a perdu le focus : le bouton n'est plus tenu, sans avoir été
// relâché pour autant.
void NoteFocusLost(MouseEngagement& engagement);

// Un clic d'action sur une entité : l'engagement se lève sur elle.
void NoteEngagingClick(MouseEngagement& engagement, std::uint32_t id, bool continuous);

// L'action que le clic laisse au personnage. Elle remplace la précédente sans
// rien faire retomber.
void NoteDeed(MouseEngagement& engagement, PlayerDeed deed, std::uint32_t target_id,
              bool stops_on_release);

// L'action s'achève, et l'engagement retombe avec elle.
void EndDeed(MouseEngagement& engagement);

// Une attaque vient de partir vers sa cible.
void NoteStrike(MouseEngagement& engagement, std::uint32_t target_id);

// Le serveur dit la cible trop loin : l'attaque reprend sur elle, sans rien
// relever. Sur la même cible, le geste qui l'avait lancée reste ce qu'il était.
void NoteAttackRearmed(MouseEngagement& engagement, std::uint32_t target_id, bool stops_on_release);

// L'action du personnage, relue à chaque image : elle s'achève, et
// l'engagement avec elle, quand sa cible quitte la vue ou meurt. Une compétence
// sur soi ne cherche pas sa cible.
void AdvanceDeed(MouseEngagement& engagement, const ro::sim::World& world);

// Une passe de la souris sur ce qu'elle désigne : la retombée d'un engagement
// qui n'est pas continu, puis le joueur survolé. Deux passes par image, la
// seconde sans mouvement : un joueur quitté fait retomber dans l'image même.
void Settle(MouseEngagement& engagement, const EngagementFrame& frame, const ro::sim::World& world);

// Une image où la souris est arrêtée : rien ne retombe ni ne se lève, mais le
// curseur reste suivi et l'appui consommé — à la reprise, elle ne croira pas
// qu'il vient de bouger.
void SkipSettle(MouseEngagement& engagement, PointerPx mouse);

// L'arrivée sur une carte. La sélection et le survol tombent toujours ; une
// AUTRE carte fait aussi tomber l'engagement et le siège, que la même carte
// conserve.
void EnterMap(MouseEngagement& engagement, bool same_map);

// LA SOURIS EST FIGÉE DANS LE MONDE : la mort, un commerce, le coffre, ou le
// dialogue d'un PNJ qui retient le monde — sauf tant que le joueur porte une
// barre au-dessus de la tête (`own_bar_shown`) : incantation, compétence
// annoncée ou barre de progression d'un script. Sous celle-ci, un pas demandé
// est la façon d'annuler.
[[nodiscard]] bool WorldMouseFrozen(const MouseEngagement& engagement, const ro::sim::World& world,
                                    bool own_bar_shown);

// Le siège de la carte : annoncé maintenant, ou depuis l'arrivée.
[[nodiscard]] bool SiegeMap(const MouseEngagement& engagement, const ro::sim::MapMode& mode);
void NoteMapMode(MouseEngagement& engagement, const ro::sim::MapMode& mode);

}  // namespace moonclient
