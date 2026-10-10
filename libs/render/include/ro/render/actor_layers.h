#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace ro::render {

// Un acteur RO se regarde sous huit directions.
//
// Cette constante vit ici plutôt qu'avec les images d'animation : c'est une
// propriété de l'ACTEUR, dont dépendent aussi bien l'index d'action que
// l'ordre de dessin.
inline constexpr std::size_t kDirectionCount = 8;

// Les neuf parties d'un acteur, dans la numérotation du client.
//
// Ces valeurs ne sont pas arbitraires : le jeu s'en sert comme index dans ses
// tables d'ordre de dessin, et les renuméroter romprait la correspondance avec
// la référence.
enum class ActorPart : std::uint8_t {
  kBody = 0,
  kHead = 1,
  kHeadgearLow = 2,
  kHeadgearMiddle = 3,
  kHeadgearTop = 4,
  kWeapon = 5,
  kWeaponTrail = 6,
  kShield = 7,
  kGarment = 8,
};

inline constexpr std::size_t kActorPartCount = 9;

// UN ENSEMBLE DE PARTIES, un bit par partie : ce qu'un effet fait à certains
// calques et pas aux autres.
using ActorPartSet = std::uint16_t;

[[nodiscard]] constexpr ActorPartSet PartBit(ActorPart part) noexcept {
  return static_cast<ActorPartSet>(1U << static_cast<unsigned>(part));
}

inline constexpr ActorPartSet kAllActorParts =
    static_cast<ActorPartSet>((1U << kActorPartCount) - 1U);

// Vrai si la direction montre le DOS de l'acteur.
//
// C'est le seul discriminant de l'ordre de dessin : le jeu n'a que deux tables,
// une de face et une de dos.
[[nodiscard]] bool IsBackView(std::size_t direction) noexcept;

// De quel côté du corps la cape passe, image par image.
//
// Le jeu la dessine dans une passe à part, avant ou après le reste de
// l'acteur, et demande à ses tables de couches laquelle : par métier et par
// action, la liste des images où elle passe DERRIÈRE. Tout ce que les tables
// ne listent pas se dessine devant.
struct GarmentPlacement {
  // Par index d'action, les images où la cape passe derrière le corps.
  std::map<std::size_t, std::vector<std::size_t>> behind_frames;

  [[nodiscard]] bool DrawsBehind(std::size_t action_index, std::size_t frame_index) const;
};

// L'ordre de dessin des parties présentes, de l'ARRIÈRE vers l'avant.
//
// Le jeu procède en deux temps, et les deux comptent :
//
//   1. un ordre d'ÉMISSION qui dépend du groupe de direction —
//        de face : corps, tête, coiffes bas/milieu/haut, cape, arme, traînée,
//                  bouclier
//        de dos  : bouclier d'abord, puis la même suite
//      ⚠ Le BOUCLIER est la seule pièce qui change de camp : de dos il passe
//      derrière le corps lui-même, le personnage le portant de l'autre côté.
//      ⚠ Le CORPS précède la TÊTE, donc passe DERRIÈRE elle. L'inverse ferait
//      recouvrir le bas du visage par la base du cou.
//
//   2. un RETRI où les coiffes et la cape reçoivent une clé bien supérieure à
//      celle des autres parties, et remontent donc au-dessus de l'arme dans
//      les deux orientations.
//
// La cape fait exception quand ses tables la mettent DERRIÈRE pour l'image en
// cours (garment_behind) : elle se dessine alors avant tout le reste, bouclier
// compris, comme dans la passe que le jeu lui consacre.
//
// ⚠ Le jeu tire la clé des coiffes d'une table Lua embarquée que ce client ne
// lit pas encore ; leur ordre RELATIF est ici celui de leur numéro de partie,
// ce qui reproduit la plage de valeurs observée mais pas d'éventuelles
// exceptions déclarées dans cette table.
//
//   3. DE FACE SEULEMENT, ce que les mains tiennent remonte au-dessus des
//      coiffes et de la cape : l'arme, puis la seconde arme d'un ambidextre
//      (la traînée quand `off_hand_weapon` dit qu'elle en porte une), puis le
//      bouclier. Le client d'origine laisse l'arme sous les coiffes, et un
//      grand chapeau la couvre ; ce client la met devant. De dos, rien ne
//      change.
[[nodiscard]] std::vector<ActorPart> PartDrawOrder(std::span<const ActorPart> present,
                                                   std::size_t direction, bool garment_behind,
                                                   bool off_hand_weapon = false);

}  // namespace ro::render
