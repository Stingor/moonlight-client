#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/render/sprite_catalog.h"

namespace ro::render {

// CE QUE DÉSIGNE L'APPARENCE D'UN OBJET — le `view` de sa fiche, le « ViewID »
// de la table d'origine —, lu dans les tables mêmes qui habillent un acteur.
//
// Le nombre seul ne dit rien : il se lit dans une table différente selon la
// pièce que l'objet habille. Une arme y donne sa CLASSE, une coiffe son
// ACCESSOIRE, un bouclier sa FORME, une robe ou des ailes leur SPRITE DE DOS.
// Ce n'est pas à ce module de dire quelle pièce l'objet habille : c'est son
// type et ses emplacements, que seul le serveur connaît.

enum class ViewedPiece : std::uint8_t {
  kNothing,   // l'apparence ne se dessine pas : rien à lire
  kWeapon,    // une classe d'arme : weapon_sprites.yml
  kHeadgear,  // un accessoire de tête : headgear_sprites.yml
  kShield,    // une forme de bouclier
  kGarment,   // ce qui se porte au dos, robe ou ailes : garment_sprites.yml
};

struct ViewMeaning {
  // Le nom symbolique que la table donne au numéro —
  // `WEAPONTYPE_SHORTSWORD`, `ACCESSORY_GOGGLE`… —, ou vide : une forme de
  // bouclier n'en a jamais.
  std::string symbol;
  // Le nom du sprite, en UTF-8, sans le souligné qui le joint au gabarit du
  // chemin : `단검` pour `_단검`.
  std::string sprite;

  [[nodiscard]] bool operator==(const ViewMeaning& other) const = default;
};

// nullopt pour `kNothing`, et pour un numéro dont la table ne donne ni nom
// symbolique ni sprite : une apparence inconnue n'est pas une erreur, elle se
// dit telle.
[[nodiscard]] std::optional<ViewMeaning> DescribeItemView(const SpriteCatalog& catalog,
                                                          ViewedPiece piece, std::uint16_t view_id);

}  // namespace ro::render
