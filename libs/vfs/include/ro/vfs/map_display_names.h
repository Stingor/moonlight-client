#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LE NOM LISIBLE D'UNE CARTE, tiré de son nom de fichier.
//
// Le serveur ne désigne une carte que par son fichier — « prontera.gat ». Le nom
// qu'on montre au joueur, et le bandeau de son arrivée, se lisent dans
// `data\yaml_files\map_names.yml`, que `scripts/yaml_tables/map_names.py` tire
// de la liste des cartes du serveur et de la table des noms de l'installation :
//
//     version: 1
//     maps:
//       prontera:
//         name: Prontera, Capital of Rune Midgard
//         announced: true
//         subtitle: Capital of the Rune-Midgarts Kingdom
//         title: Prontera
//         background: village
//
// Tous les champs sont facultatifs. Une carte absente n'a pas de nom.
inline constexpr std::string_view kMapNamesPath = "data\\yaml_files\\map_names.yml";

// L'image d'une carte qui n'en nomme aucune.
inline constexpr std::string_view kMapSignNoBackground = "noname";

// LE BANDEAU QU'UNE CARTE MONTRE À L'ARRIVÉE : une image de fond, un sous-titre
// et un titre.
struct MapSign {
  // Le fond, par son nom de fichier sans dossier ni extension : `village`.
  std::string background{kMapSignNoBackground};
  // En UTF-8 : ils se dessinent tels quels.
  std::string subtitle;
  std::string title;
  // `announced` : LE JEU NE MONTRE QUE CEUX-LÀ. Ce client peut montrer aussi
  // les autres, pourvu qu'ils aient un titre.
  bool announced = false;

  [[nodiscard]] bool titled() const noexcept { return !subtitle.empty() || !title.empty(); }
};

class MapDisplayNames {
 public:
  // Lit la table par le VFS.
  [[nodiscard]] static core::Result<MapDisplayNames> Load(Vfs& vfs);

  // Le texte de `map_names.yml`. Refuse un texte qui n'est pas du YAML, dont
  // `version` n'est pas 1, sans table `maps`, ou dont une carte ou un champ a
  // une forme inattendue.
  [[nodiscard]] static core::Result<MapDisplayNames> Parse(std::string_view yaml_text);

  // Le nom lisible d'une carte désignée par son fichier `.gat`, ou rien si la
  // table ne la nomme pas. ⚠ RIEN N'EST PAS UNE ERREUR : c'est à l'appelant de
  // dire ce que le client écrit alors.
  [[nodiscard]] std::optional<std::string_view> DisplayNameOf(std::string_view gat_name) const;

  [[nodiscard]] std::size_t size() const noexcept { return names_.size(); }

  // LE BANDEAU D'ARRIVÉE D'UNE CARTE, ou nul quand elle n'a de quoi en montrer
  // aucun : ni annonce, ni titre.
  //
  // 🔴 LE JEU NE MONTRE QUE LES CARTES `announced`, et la traduction livrée
  // n'en annonce que deux cent seize, presque toutes récentes — pas une ville.
  // Le titre, lui, est posé sur presque toutes. C'est à l'appelant de choisir
  // entre les deux : voir `MapSign::announced`.
  //
  // ⚠ ANNONCÉE SANS SOUS-TITRE NI TITRE, le bandeau montre son image seule.
  [[nodiscard]] const MapSign* SignOf(std::string_view gat_name) const;

 private:
  // Indexées par le nom de la carte, sans extension.
  std::map<std::string, std::string, std::less<>> names_;
  std::map<std::string, MapSign, std::less<>> signs_;
};

// LA CLÉ DE LA TABLE POUR UN NOM DE FICHIER DE CARTE.
//
// Deux retouches, dans cet ordre :
// - un nom de cinq caractères ou plus qui finit par une extension de trois
//   lettres la perd — « prontera.gat » devient « prontera » ;
// - s'il porte un « @ » à la position quatre ou au-delà, les trois caractères
//   qui commencent quatre positions avant lui disparaissent : la copie d'une
//   carte d'instance, « 0001@tower », se range sous « 1@tower ».
[[nodiscard]] std::string MapNameKey(std::string_view gat_name);

}  // namespace ro::vfs
