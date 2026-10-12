#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "ro/render/icon_texture.h"
#include "ro/vfs/status_table.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LE DOSSIER DES ICÔNES D'ÉTAT.
//
// 🔴 PAS SOUS LA RACINE D'INTERFACE, contrairement aux icônes d'objet : celles-ci
// vivent avec les EFFETS. Mesuré : 19 822 fichiers sous `data\texture\effect`,
// et 466 des 533 images que la table des états nomme s'y retrouvent.
//
// ⚠ LES SOIXANTE-SEPT AUTRES SONT NOMMÉES ET ABSENTES. Ce n'est pas une panne :
// la table du client décrit plus d'états que cette installation n'en dessine, et
// une case sans image se replie sur son abréviation.
inline constexpr std::string_view kStatusIconFolder = "data\\texture\\effect\\";

// LA RACINE DES TEXTURES, pour les chemins que l'EXÉCUTABLE porte : ceux-là
// s'écrivent « effect\STEELBODY.tga » — relatifs à cette racine-ci, et non au
// dossier des effets. Deux préfixes, parce que les deux sources ne partent pas
// du même endroit, et les confondre chercherait « effect\effect\… ».
inline constexpr std::string_view kTextureFolder = "data\\texture\\";

// LES ICÔNES D'ÉTAT, LUES À LA DEMANDE.
//
// Même règle que les icônes d'objet : rien au démarrage, et une absence se
// retient — sans quoi on redemanderait le même fichier manquant soixante fois
// par seconde.
//
// ⚠ ET CE SONT DES TGA, pas des bitmaps. Ils portent un VRAI canal alpha : pas
// de clé de couleur magenta ici, elle mangerait les pixels roses d'une icône.
class StatusIcons {
 public:
  // Les deux vivent plus longtemps que ce cache : les icônes se lisent au fil du
  // jeu, pas une fois pour toutes.
  StatusIcons(vfs::Vfs& vfs, const vfs::StatusTable& table);

  // LE MÉTIER DU JOUEUR, dont six états tirent une autre image.
  //
  // ⚠ IL VIDE LE CACHE QUAND IL CHANGE, et il le faut : six entrées déjà lues
  // désigneraient sinon l'image de l'ancienne classe. Un changement de métier
  // est rare, et relire quelques icônes ne coûte rien — distinguer les six de
  // toutes les autres coûterait plus cher à écrire qu'à exécuter.
  void SetJob(std::uint32_t job_id);

  // L'icône d'un état. Rend une texture inutilisable quand la table ne nomme pas
  // d'image, ou quand l'installation ne l'a pas — et c'est un cas NORMAL.
  [[nodiscard]] const IconTexture& Get(std::uint32_t effect_id);

 private:
  vfs::Vfs* vfs_;
  const vfs::StatusTable* table_;
  std::uint32_t job_id_ = 0;
  std::unordered_map<std::uint32_t, IconTexture> icons_;
};

// Le chemin de l'image d'un état, en CP949, ou vide.
//
// 🔴 LA TABLE REND UN NOM NU — « BD_Sleep.tga », sans dossier — : sans le
// dossier remis ici, TOUTES les
// icônes venues du Lua échouent, en silence. Un nom qui porterait déjà un
// séparateur est pris tel quel ; relevé, aucun des 533 n'en porte, mais la table
// n'est pas la nôtre.
[[nodiscard]] std::string StatusIconPath(const vfs::StatusTable& table, std::uint32_t effect_id,
                                         std::uint32_t job_id);

}  // namespace ro::render
