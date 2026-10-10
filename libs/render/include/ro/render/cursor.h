#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ro::render {

// Le curseur du jeu : un sprite, pas un curseur du système.
//
// Le client cache celui de Windows et dessine le sien, une action du fichier
// data\sprite\cursors par forme. Chaque action est ANIMÉE : la flèche
// scintille, la main se referme, l'anneau de ciblage tourne.
//
// Les formes ci-dessous sont celles que le client demande selon ce qui est sous
// la souris, et les numéros d'action sont ceux du fichier.
inline constexpr std::string_view kCursorSpritePath = "data\\sprite\\cursors";

enum class CursorShape : std::uint8_t {
  // La flèche ordinaire : le sol, le vide, tout ce qui ne se désigne pas.
  kArrow,
  // Une bulle de dialogue : un PNJ à qui parler.
  kTalk,
  // Une main qui pointe : un élément d'interface.
  kPoint,
  // Une épée : une cible qu'on peut frapper.
  kAttack,
  // Une porte qui s'ouvre : un portail de carte.
  kWarp,
  // Une main qui se referme : un objet au sol.
  kPickUp,
  // Un anneau posé au sol : une compétence armée attend qu'on désigne.
  kGroundTarget,
  // LE MÊME ANNEAU, DEUX FOIS PLUS VIF : la compétence armée peut frapper ce
  // que la souris désigne.
  //
  // ⚠ CE N'EST PAS UN AUTRE DESSIN. Les actions 10 et 11 portent les mêmes
  // vingt images, aux mêmes deux calques ; seule la cadence les sépare — six
  // tics l'image contre trois. C'est l'ACCÉLÉRATION qui dit « celui-là, oui ».
  kTargetLocked,
  // Un panneau d'interdiction : une case où l'on ne peut pas aller.
  kForbidden,
  // La caméra tourne sous le bouton tenu.
  kRotate,
};

// Vrai si une case REFUSE qu'on y aille, au sens du curseur.
//
// Le client ne consulte pas une marchabilité générale : il compare le type de
// la cellule à DEUX valeurs, un et cinq, et à rien d'autre. Les autres types
// infranchissables — l'eau profonde en particulier — gardent la flèche. Une
// souris qui ne désigne aucune case, le ciel ou le dehors de la carte, est
// refusée elle aussi ; c'est à l'appelant de le dire, lui seul sait viser.
//
// Le drapeau d'EAU se retire avant la comparaison. Il ne change qu'une cellule
// marchable en eau marchable et laisse les autres types intacts ; le garder
// ferait passer pour franchissable un mur écrit 0x80000001, ce qu'un demi-
// million de cellules sont sur les cartes réelles. Voir kGatWaterFlag.
[[nodiscard]] bool CellRefusesWalk(std::int32_t cell_type);

// Le tic de l'horloge du curseur, en millisecondes.
inline constexpr float kCursorTickMs = 24.0F;

// Comment une forme s'anime.
//
// ⚠ LA CADENCE NE VIENT PAS DU FICHIER. Le client demande bien son délai au
// .act, puis n'en fait rien : il compte des tics de
// vingt-quatre millisecondes et divise par un facteur écrit en dur, propre à
// chaque forme. Suivre le fichier fait tourner la flèche deux fois trop vite.
struct CursorMotion {
  // L'action du fichier qui porte cette forme.
  std::size_t action = 0;
  // Combien de tics une image dure.
  float ticks_per_frame = 4.0F;
  // Vrai si l'animation joue UNE fois et se fige sur sa dernière image, au lieu
  // de reboucler.
  bool plays_once = false;
  // Vrai si elle n'avance que tant que le bouton gauche est enfoncé, et revient
  // à sa première image dès qu'il est relâché.
  bool while_pressed = false;
};

// L'action du fichier qui porte cette forme.
[[nodiscard]] std::size_t CursorAction(CursorShape shape);

// Comment cette forme s'anime, d'après la table du client.
[[nodiscard]] CursorMotion CursorMotionOf(CursorShape shape);

}  // namespace ro::render
