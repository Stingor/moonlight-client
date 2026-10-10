#pragma once

#include <string_view>

#include "ro/render/gl_texture.h"

namespace ro::render {

// Une icône téléversée. Les dimensions sont celles du bitmap : le client les
// dessine à vingt-quatre pixels, et qui les montre plus grand les agrandit.
struct IconTexture {
  GlTexture texture;
  int width_px = 0;
  int height_px = 0;

  [[nodiscard]] bool usable() const noexcept { return texture.valid(); }
};

// LE DOSSIER DES ICÔNES, sous celui des textures d'interface.
//
// ⚠ UN SEUL DOSSIER POUR DEUX FAMILLES : le client y range les icônes de
// compétence ET celles des objets. Elles ne se distinguent que par leur nom de
// fichier — le nom INTERNE pour une compétence (« SM_BASH »), le nom de
// RESSOURCE pour un objet, qui est en coréen. Deux constantes diraient deux
// dossiers, et enverraient chercher au mauvais endroit le jour où l'une bouge.
inline constexpr std::string_view kIconFolder = "item\\";
inline constexpr std::string_view kIconSuffix = ".bmp";

// L'ILLUSTRATION DE COLLECTION : la grande image que le client montre dans une
// description, là où la case du sac ne montre qu'une icône de 24 pixels.
//
// 🔴 SOUS LA MÊME RACINE QUE LES ICÔNES, et c'est mesuré : onze mille cent
// entrées sous `유저인터페이스\collection`, contre NEUF sous `sprite\collection`
// où je l'avais d'abord cherchée. Tout le reste retombait alors sur l'icône de
// vingt-quatre pixels — une description avec la vignette d'une case de sac.
//
// ⚠ LE NOM DE RESSOURCE EST LE MÊME que celui de l'icône : c'est le dossier qui
// change, pas la clé.
inline constexpr std::string_view kCollectionFolder = "collection\\";

// L'ILLUSTRATION D'UNE CARTE : le dessin en pied, celui que le client montre à
// la place de l'image de collection quand l'objet est une carte.
//
// 🔴 SA CLÉ N'EST PAS LE NOM DE RESSOURCE DE L'OBJET, contrairement aux deux
// dossiers ci-dessus. Toutes les cartes partagent la même ressource — le dos de
// carte générique de la case de sac —, et c'est `num2cardillustnametable.txt`
// qui nomme le dessin de chacune. Voir `vfs::CardIllustrations`.
//
// ⚠ MILLE QUATRE CENT VINGT ET UN FICHIERS pour mille cinq cent quatre entrées
// de table : trente-huit cartes n'ont pas leur dessin dans cette installation,
// et se rabattent sur l'image de collection. Une absence n'est pas une panne.
inline constexpr std::string_view kCardIllustrationFolder = "cardbmp\\";

}  // namespace ro::render
