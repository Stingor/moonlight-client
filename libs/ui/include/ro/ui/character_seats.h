#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>

namespace ro::ui {

// LA SCÈNE DU BANQUET : vingt-cinq sièges autour d'une table, et le personnage
// qu'on y assoit.
//
// 🔴 À TROIS DÉCIMALES, ET À GARDER TELLES. Ces coordonnées ont été calées à
// l'œil sur le décor livré, siège par siège ; les recalculer « plus
// proprement » donnerait une scène qui RESSEMBLE sans être la même, et le
// joueur reconnaîtrait l'écart sans savoir le nommer.
//
// ⚠ NORMALISÉES, DONC PORTABLES TELLES QUELLES : le décor se dessine étiré
// plein cadre, si bien qu'une table calée en 1920 × 1080 vaut en 1280 × 720.
// Rien ici ne dépend de la résolution.

// LE DOSSIER OÙ VIVENT LES DÉCORS, et le préfixe de leur chemin.
//
// 🔴 UN DÉCOR SE NOMME RELATIVEMENT À LUI, comme le fait le gestionnaire de
// textures natif, qui ajoute `data\texture\` à tout `.bmp`. Garder cette forme
// (« lobby_hall.bmp », « lobby\taverne.bmp ») plutôt que le chemin entier,
// c'est ce qui laisse relire tel quel le fichier de mise en page. Notre VFS,
// lui, n'ajoute rien : le préfixe se pose au chargement.
inline constexpr std::string_view kBackgroundRoot = "data\\texture\\";

// LE DÉCOR LIVRÉ AVEC LE CLIENT — 1264 × 842, vingt-quatre bits, dans
// `moonlight.grf`.
//
// ⚠ IL EST ÉTIRÉ PLEIN CADRE : son rapport est de trois
// pour deux, donc il se déforme un peu sur un écran large. C'est ce que les
// joueurs de ce serveur ont sous les yeux, et les sièges sont calés dessus.
inline constexpr std::string_view kBanquetBackground = "lobby_hall.bmp";

// ⚠ FIXE, et le joueur n'en ajoute pas : la pagination des bancs repose sur ce
// découpage, et le décor livré est numéroté 1 à 25.
inline constexpr int kSeatCount = 25;

// LES SIÈGES D'HONNEUR — deux trônes et trois petites tables — NE SE PAGINENT
// PAS : ils portent toujours les personnages 0 à 4, sur toutes les pages. Le
// serveur en autorise bien plus que vingt-cinq (`MAX_CHARS`), et ce sont les
// vingt BANCS qui défilent.
inline constexpr int kHonourSeats = 5;
inline constexpr int kBenchSeats = kSeatCount - kHonourSeats;

// LE RAPPORT LARGEUR/HAUTEUR DU CADRE D'UN SIÈGE : l'encombrement d'un sprite
// debout. ⚠ IL NE SERT PLUS À AJUSTER LE PANTIN — celui-ci garde les
// proportions de son fichier — mais à poser le cadre cliquable et à mesurer la
// référence d'échelle.
inline constexpr float kSeatBoxAspect = 0.62F;

// DE COMBIEN LE CADRE CLIQUABLE DÉBORDE SOUS LA LIGNE DU SIÈGE, en fraction de
// la hauteur d'écran. Un siège se clique un peu plus bas que le point où le
// personnage pose les pieds.
inline constexpr float kSeatGrabBelow = 0.02F;

// DE COMBIEN LA BOÎTE DU PANTIN DÉPASSE LA TAILLE D'UN PERSONNAGE STANDARD, en
// hauteur puis en largeur.
//
// 🔴 CE N'EST PAS LE CADRE DU SIÈGE, et les deux ne peuvent pas être le même :
// toute la tablée est dessinée à UNE SEULE échelle, celle qui met un
// personnage standard à la hauteur du siège. Un grand chapeau, une monture,
// une arme d'hast dépassent donc — c'est voulu, c'est ce qui rend deux voisins
// comparables — et il faut que l'image ait la place de les montrer.
//
// ⚠ CE SONT DES BORNES : la scène compose une TEXTURE, qui finit quelque
// part. Elles sont donc généreuses, et c'est gratuit : l'image est rastérisée
// en pixels de SPRITE, donc une boîte deux fois plus grande à l'écran ne coûte
// pas un pixel de plus quand l'échelle grandit d'autant. Au-delà, c'est coupé.
inline constexpr float kSeatHeadroom = 1.75F;
inline constexpr float kSeatSpread = 2.20F;

// LES TYPES D'ACTION du fichier d'animation d'un personnage. Une pose vaut
// `anim * 8 + dir`.
//
// ⚠ CE SONT DES INDEX DE FICHIER, et le composeur replie une action absente sur
// l'attente : une valeur inexacte donne une pose inattendue, jamais un pantin
// cassé. 0, 1, 2, 4, 6 et 8 portent les poses nommées ci-dessous. 11 et 12 existent dans les
// fichiers — treize types en tout — sans porter de nom sûr : ils se proposent
// tels quels, l'aperçu tranche mieux qu'un libellé inventé.
inline constexpr int kSeatAnimStand = 0;
inline constexpr int kSeatAnimWalk = 1;
inline constexpr int kSeatAnimSit = 2;
inline constexpr int kSeatAnimPick = 3;
inline constexpr int kSeatAnimFight = 4;
inline constexpr int kSeatAnimAttack = 5;
inline constexpr int kSeatAnimHurt = 6;
inline constexpr int kSeatAnimFrozen = 7;
inline constexpr int kSeatAnimDie = 8;
inline constexpr int kSeatAnimAttack2 = 9;
inline constexpr int kSeatAnimAttack3 = 10;
inline constexpr int kSeatAnimExtra1 = 11;
inline constexpr int kSeatAnimExtra2 = 12;
inline constexpr int kSeatAnimationTypes = 13;

// La pose d'usine : les convives du banquet sont assis, de face.
inline constexpr int kSeatAnimation = kSeatAnimSit;
inline constexpr int kSeatDirection = 0;
inline constexpr int kSeatDirections = 8;

// LA TÊTE DANS L'AXE DU CORPS. Un et deux la penchent d'un côté ou de l'autre.
//
// 🔴 C'EST UN INDEX D'IMAGE, ni une action ni une direction : un sprite de tête
// porte trois images par action — les trois inclinaisons que `/doridori` fait
// alterner. Et cet index est PARTAGÉ par tout l'acteur, corps compris : le
// cou s'accroche en comparant les ancres des deux sur la MÊME image.
inline constexpr int kHeadAlongBody = -1;
inline constexpr int kHeadTurnMost = 2;

// L'IMAGE DE L'ACTION laissée au composeur.
inline constexpr int kAutomaticFrame = -1;

// UN SIÈGE.
//
// ⚠ `y` EST LA LIGNE DU SIÈGE — le point où le personnage pose les PIEDS, donc
// le BAS de son cadre —, et non son centre ni sa tête. L'ancrer autrement
// ferait flotter ou enfoncer toute la tablée d'un demi-personnage.
//
// `height` est la hauteur d'un personnage STANDARD, en fraction de la hauteur
// d'écran. ⚠ « Standard » et non « de ce pantin » : les sprites gardent les
// proportions de leur fichier plutôt que d'être étirés au cadre, si bien qu'un
// personnage naturellement plus grand — monture, grand chapeau — déborde vers
// le haut. C'est voulu : c'est ce qui donne à la tablée une échelle commune.
//
// ⚠ LA POSE EST PAR SIÈGE ET NON GLOBALE : sur un décor de taverne, les
// convives sont assis et le videur reste debout près de la porte.
struct CharacterSeat {
  float x = 0.5F;
  float y = 0.5F;
  float height = 0.115F;
  // L'orientation du corps, 0 à 7 — zéro est la face.
  int direction = kSeatDirection;
  // Le type d'action du fichier : voir `kSeatAnim…`.
  int animation = kSeatAnimation;
  // Voir `kHeadAlongBody`.
  int head_turn = kHeadAlongBody;
  // L'image imposée dans l'action, ou `kAutomaticFrame`.
  int frame = kAutomaticFrame;
  // JOUER l'animation du corps plutôt que la figer. ⚠ SANS EFFET au repos et
  // assis : leurs images sont des orientations du regard, pas un mouvement.
  // Ce qu'il porte, lui, vit toujours.
  bool animated = false;
};

// LA TABLE D'USINE, calée sur le décor livré et numérotée comme lui : 1 grand
// trône, 2 petit trône, 3-5 petites tables, 6-14 rangée haute, 15/16 bouts,
// 17-25 rangée basse.
//
// 🔴 SEPT ORDONNÉES DISTINCTES, ET NON CINQ. Le petit trône est neuf millièmes
// de hauteur SOUS le grand, et le bout droit deux millièmes sous le bout
// gauche. Ce sont exactement les écarts qu'un portage « rangé » efface, et la
// perspective du décor les demande.
inline constexpr std::array<CharacterSeat, kSeatCount> kBanquetSeats = {{
    {0.460F, 0.295F, 0.115F},  // 1  grand trône
    {0.545F, 0.304F, 0.115F},  // 2  petit trône
    {0.414F, 0.453F, 0.115F},  // 3  petite table gauche
    {0.497F, 0.453F, 0.115F},  // 4  petite table milieu
    {0.579F, 0.453F, 0.115F},  // 5  petite table droite
    {0.142F, 0.589F, 0.115F},  // 6  rangée haute
    {0.232F, 0.589F, 0.115F},  // 7
    {0.321F, 0.589F, 0.115F},  // 8
    {0.408F, 0.589F, 0.115F},  // 9
    {0.498F, 0.589F, 0.115F},  // 10
    {0.586F, 0.589F, 0.115F},  // 11
    {0.674F, 0.589F, 0.115F},  // 12
    {0.763F, 0.589F, 0.115F},  // 13
    {0.852F, 0.589F, 0.115F},  // 14
    {0.041F, 0.677F, 0.115F},  // 15 bout gauche
    {0.954F, 0.679F, 0.115F},  // 16 bout droit
    {0.111F, 0.837F, 0.115F},  // 17 rangée basse
    {0.207F, 0.837F, 0.115F},  // 18
    {0.305F, 0.837F, 0.115F},  // 19
    {0.401F, 0.837F, 0.115F},  // 20
    {0.497F, 0.837F, 0.115F},  // 21
    {0.594F, 0.837F, 0.115F},  // 22
    {0.689F, 0.837F, 0.115F},  // 23
    {0.782F, 0.837F, 0.115F},  // 24
    {0.880F, 0.837F, 0.115F},  // 25
}};

// L'EMPLACEMENT DE PERSONNAGE ASSIS À CE SIÈGE, pour cette page.
//
// ⚠ LES CINQ PREMIERS NE BOUGENT PAS : ce sont les sièges d'honneur, et ils
// portent les personnages 0 à 4 sur toutes les pages. Les paginer aussi ferait
// disparaître le personnage principal dès qu'on tourne la page.
[[nodiscard]] constexpr int SeatedSlot(int seat, int page) noexcept {
  return seat < kHonourSeats ? seat : seat + page * kBenchSeats;
}

// UN SIÈGE MESURÉ, en pixels d'écran.
//
// 🔴 UNE SEULE MESURE POUR DEUX LECTEURS, et c'est la raison d'être de la
// structure. Celui qui DESSINE le pantin (la fenêtre) et celui qui le COMPOSE
// (`app`, qui tient la garde-robe) partent tous deux d'un siège normalisé et
// d'une taille d'écran ; les laisser refaire le calcul chacun de son côté,
// c'est les laisser DIVERGER d'un demi-pixel un jour, et le pantin ne tombe
// alors plus dans son cadre.
struct SeatFrame {
  // LE POINT D'ASSISE : où le personnage pose les pieds.
  float centre_x = 0.0F;
  float chair_y = 0.0F;
  // La hauteur d'un personnage STANDARD à ce siège : c'est elle qui fixe
  // l'échelle commune de la tablée.
  float standard_height = 0.0F;
  // LA ZONE CLIQUABLE, aux proportions d'un sprite debout.
  float grab_left = 0.0F;
  float grab_top = 0.0F;
  float grab_right = 0.0F;
  float grab_bottom = 0.0F;
  // LA BOÎTE DU PANTIN. ⚠ SON BAS EST LA LIGNE DU SIÈGE, et non le bas de la
  // zone cliquable : c'est là que les pieds tombent.
  float doll_width = 0.0F;
  float doll_height = 0.0F;
};

[[nodiscard]] constexpr SeatFrame SeatFrameAt(const CharacterSeat& seat, float display_width,
                                              float display_height) noexcept {
  SeatFrame frame;
  frame.centre_x = seat.x * display_width;
  frame.chair_y = seat.y * display_height;
  frame.standard_height = seat.height * display_height;
  const float grab_half = frame.standard_height * kSeatBoxAspect * 0.5F;
  frame.grab_left = frame.centre_x - grab_half;
  frame.grab_right = frame.centre_x + grab_half;
  frame.grab_top = frame.chair_y - frame.standard_height;
  frame.grab_bottom = frame.chair_y + kSeatGrabBelow * display_height;
  frame.doll_width = frame.standard_height * kSeatBoxAspect * kSeatSpread;
  frame.doll_height = frame.standard_height * kSeatHeadroom;
  return frame;
}

// Combien de pages il faut pour `slots` emplacements.
[[nodiscard]] constexpr int SeatPages(int slots) noexcept {
  if (slots <= kHonourSeats) {
    return 1;
  }
  // Arrondi au supérieur : le reste tient sur une page de plus, même s'il n'y a
  // qu'un personnage dessus.
  return (slots - kHonourSeats + kBenchSeats - 1) / kBenchSeats;
}

// UN PAS DES FLÈCHES sur la tablée.
enum class SeatStep : std::uint8_t { kLeft, kRight, kUp, kDown };

// UN CONVIVE QU'ON PEUT DÉSIGNER, à son point d'assise en pixels d'écran.
struct SeatPoint {
  int slot = -1;
  float x = 0.0F;
  float y = 0.0F;
};

// L'ÉCART LATÉRAL PÈSE DOUBLE face à l'avancée : on suit la rangée plutôt que
// de couper en diagonale vers un convive à peine plus proche.
inline constexpr float kSeatStepSidePenalty = 2.0F;

// L'EMPLACEMENT OÙ MÈNE UNE FLÈCHE depuis `from_slot`, ou -1 s'il n'y a
// personne dans cette direction.
//
// 🔴 PAR LA GÉOMÉTRIE, ET NON PAR LE NUMÉRO : la mise en page appartient au
// joueur, qui déplace les sièges où il veut. « Droite » mène à qui est assis à
// droite, quel que soit son rang dans la liste du serveur.
//
// ⚠ DANS UN QUART DE TOUR CENTRÉ SUR LA DIRECTION : un convive n'est candidat
// que s'il est plus avancé dans la direction qu'écarté d'elle. Sans cette
// garde, « bas » depuis le bout gauche de la table mènerait au bout droit,
// assis deux pixels plus bas. Les quatre quarts couvrent le plan : chaque
// convive reste joignable par l'une des flèches.
//
// Si `from_slot` n'est pas sur la tablée — rien n'est désigné, ou le désigné
// est sur une autre page —, la flèche désigne le plus petit emplacement
// présent.
[[nodiscard]] inline int NearestSeatToward(std::span<const SeatPoint> seats, int from_slot,
                                           SeatStep step) noexcept {
  const SeatPoint* from = nullptr;
  int lowest_slot = -1;
  for (const SeatPoint& seat : seats) {
    if (seat.slot == from_slot) {
      from = &seat;
    }
    if (lowest_slot < 0 || seat.slot < lowest_slot) {
      lowest_slot = seat.slot;
    }
  }
  if (from == nullptr) {
    return lowest_slot;
  }
  const bool horizontal = step == SeatStep::kLeft || step == SeatStep::kRight;
  const bool forward = step == SeatStep::kRight || step == SeatStep::kDown;
  int best_slot = -1;
  float best_cost = 0.0F;
  for (const SeatPoint& seat : seats) {
    if (seat.slot == from_slot) {
      continue;
    }
    const float dx = seat.x - from->x;
    const float dy = seat.y - from->y;
    const float signed_along = horizontal ? dx : dy;
    const float along = forward ? signed_along : -signed_along;
    const float aside = std::fabs(horizontal ? dy : dx);
    if (along <= 0.0F || along < aside) {
      continue;
    }
    const float cost = along + aside * kSeatStepSidePenalty;
    if (best_slot < 0 || cost < best_cost) {
      best_slot = seat.slot;
      best_cost = cost;
    }
  }
  return best_slot;
}

}  // namespace ro::ui
