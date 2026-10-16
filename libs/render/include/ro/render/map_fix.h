#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/rsw.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LES RETOUCHES DE CARTE DU STAFF : voir docs/adr/0117.
//
// Certaines cartes officielles portent des défauts que le client d'origine
// montre aussi, parce qu'ils sont dans leurs DONNÉES. La carte appartient à
// l'installation du joueur et ne se réécrit pas : la table
// `data\yaml_files\map_fix.yml` dit comment la retoucher au chargement, avant
// que les maillages ne se bâtissent.
//
// Le visuel seulement : la marchabilité (GAT) n'est jamais touchée, c'est le
// serveur qui décide où l'on marche.
//
// 🔴 CHAQUE RETOUCHE DÉSIGNE SA CIBLE DE FAÇON VÉRIFIABLE : un modèle par son
// rang dans le RSW, son fichier et sa position d'origine. Une cible qui ne
// correspond plus — la carte a changé sous un patch — n'est pas retouchée, et
// le rapport le dit : déplacer le mauvais objet serait pire que ne rien faire.
inline constexpr std::string_view kMapFixPath = "data\\yaml_files\\map_fix.yml";

// L'écart toléré entre la position d'origine qu'une retouche attend et celle
// du RSW, en unités du fichier, sur chaque axe : de quoi absorber l'arrondi
// d'une position recopiée à trois décimales.
inline constexpr float kMapFixOriginTolerance = 0.01F;

// LA RETOUCHE D'UN MODÈLE. Les champs absents gardent la valeur du RSW.
struct ModelFix {
  // Le rang du modèle dans la liste du RSW, compté à partir de zéro.
  std::size_t model_index = 0;
  // Son fichier, relatif à data\model\, en UTF-8.
  std::string model_file;
  // Sa position dans le RSW d'origine, dans le repère du fichier.
  std::array<float, 3> origin{};
  std::optional<std::array<float, 3>> position;
  std::optional<std::array<float, 3>> rotation;
  std::optional<std::array<float, 3>> scale;
  // Vrai : le modèle n'est pas dessiné.
  bool hidden = false;
  // Pourquoi : la table refuse une retouche qui ne le dit pas.
  std::string why;
};

// Les retouches d'une carte.
struct MapFixes {
  std::vector<ModelFix> models;
};

class MapFixTable {
 public:
  // Le texte de `map_fix.yml`. Refuse un texte qui n'est pas du YAML, dont la
  // version n'est pas gérée, ou dont une retouche est malformée — un champ
  // manquant, un vecteur qui n'a pas trois composantes, une raison vide.
  [[nodiscard]] static core::Result<MapFixTable> Parse(std::string_view yaml_text);

  // Les retouches de `map_name`, vides pour une carte que la table ne nomme pas.
  [[nodiscard]] MapFixes FixesOf(std::string_view map_name) const;

  [[nodiscard]] std::size_t map_count() const noexcept { return by_map_.size(); }

 private:
  std::map<std::string, MapFixes, std::less<>> by_map_;
};

// Ce que l'application a fait : les retouches posées, et une ligne par
// retouche refusée, qui dit pourquoi.
struct MapFixReport {
  std::size_t applied = 0;
  std::vector<std::string> refused;
};

// Pose `fixes` sur `rsw`. Toutes les cibles se vérifient sur le RSW d'ORIGINE :
// les modèles masqués ne se retirent qu'après, sans décaler les rangs que les
// autres retouches désignent.
[[nodiscard]] MapFixReport ApplyMapFixes(const MapFixes& fixes, formats::RswScene& rsw);

// Les retouches de `map_name` lues dans l'installation. Une table absente ou
// illisible n'en donne aucune, et le dit au journal.
[[nodiscard]] MapFixes LoadMapFixes(vfs::Vfs& vfs, std::string_view map_name);

}  // namespace ro::render
