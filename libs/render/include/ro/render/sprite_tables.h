#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LES TABLES DE SPRITES : ce qui traduit les identifiants qu'annonce le
// serveur — une classe, un accessoire, une classe d'arme, une robe — en noms de
// fichiers, et ce qui règle l'ombre d'une créature, le bruit d'une arme et le
// côté du corps où passe une cape.
//
// Elles se lisent dans `data\yaml_files\`, que `scripts/yaml_tables/
// sprite_tables.py` produit (le .act d'un accessoire de familier, dans
// `pet_info.yml`, que produit `pet_info.py`). Chaque fichier porte en tête sa
// source et sa forme. Le texte est en UTF-8, et un nom s'y lit tel quel : le
// catalogue le joint aux dossiers des gabarits sans autre traduction.
//
// LA FORME EST STRICTE, celle que le script écrit et que lit le curseur
// partagé `ro/formats/strict_text.h` — du YAML valide :
//
//   · `version: 1` en tête ;
//   · des sections `<nom>:` (minuscules et soulignés), chacune suivie de ses
//     entrées `<identifiant>: <valeur>`, une par ligne ;
//   · une valeur est un entier sans signe, ou une table en ligne
//     `{<champ>: <valeur>, …}` dont un champ est un nom ou un entier sans
//     signe, et sa valeur un texte entre guillemets doubles aux échappements
//     de JSON, un nombre, `true`, `false`, ou une suite `[…]` d'entiers sans
//     signe ;
//   · des commentaires `#` et des blancs partout entre deux éléments.
//
// ⚠ TOUT AUTRE TEXTE EST REFUSÉ, même du YAML valide — un texte sans
// guillemets, un champ inconnu, un identifiant redit —, et le message dit le
// fichier et la ligne. Ces sept cents kilo-octets se lisent au démarrage : un
// analyseur YAML général y met près de dix fois le temps que prenaient les
// tables Lua d'origine, ce lecteur près de quatre fois moins (`roc-cli sprite
// tables` le mesure).

// Par ACTION, les images où la cape passe derrière le corps.
using GarmentLayerFrames = std::map<std::size_t, std::vector<std::size_t>>;
// Par MÉTIER.
using GarmentLayerTable = std::unordered_map<std::uint32_t, GarmentLayerFrames>;

struct SpriteTables {
  // class_sprites.yml — par classe de PNJ ou de monstre : le nom de son sprite
  // (« .gr2 » y désigne un modèle 3D), le rayon qu'elle occupe au sol en
  // cellules, son facteur d'ombre.
  std::unordered_map<std::uint32_t, std::string> class_sprites;
  std::unordered_map<std::uint32_t, double> class_radii;
  std::unordered_map<std::uint32_t, double> class_shadows;

  // headgear_sprites.yml — par apparence de coiffe : le nom de son sprite, à
  // suffixer au jeton de sexe, et son nom symbolique.
  std::unordered_map<std::uint32_t, std::string> headgear_sprites;
  std::unordered_map<std::uint32_t, std::string> headgear_symbols;

  // weapon_sprites.yml — par classe d'arme : son sprite, son nom symbolique, la
  // FAMILLE dont une classe étendue prend l'animation, le bruit et le sprite
  // de repli, et le bruit d'un coup.
  std::unordered_map<std::uint32_t, std::string> weapon_sprites;
  std::unordered_map<std::uint32_t, std::string> weapon_symbols;
  std::unordered_map<std::uint32_t, std::uint32_t> weapon_families;
  // ⚠ EN CP949, comme le client le donne : celui qui le joue le transcode, au
  // même endroit que le bruit d'un .act. Le fichier l'écrit en UTF-8.
  std::unordered_map<std::uint32_t, std::string> weapon_hit_sounds;

  // garment_sprites.yml — par robe : le nom de son dossier, son nom
  // symbolique, et si elle prend les PETITES tables de calques.
  std::unordered_map<std::uint32_t, std::string> garment_sprites;
  std::unordered_map<std::uint32_t, std::string> garment_symbols;
  std::unordered_set<std::uint32_t> small_layer_garments;

  // garment_layers.yml — le métier dont un métier prend la table de calques,
  // puis les quatre tables, par taille et par sexe.
  std::unordered_map<std::uint32_t, std::uint32_t> layer_inherited_jobs;
  GarmentLayerTable big_female_layers;
  GarmentLayerTable big_male_layers;
  GarmentLayerTable small_female_layers;
  GarmentLayerTable small_male_layers;

  // pet_info.yml — par accessoire de familier, le .act de son corps paré, avec
  // son extension.
  std::unordered_map<std::uint32_t, std::string> pet_accessory_acts;
};

// L'entrée d'une table, ou nullptr.
template <typename Value>
[[nodiscard]] const Value* FindSpriteEntry(const std::unordered_map<std::uint32_t, Value>& table,
                                           std::uint32_t id) {
  const auto found = table.find(id);
  return found != table.end() ? &found->second : nullptr;
}

// Les fichiers, dans l'ordre où ils se lisent.
enum class SpriteTableFile : std::uint8_t { kClasses, kHeadgears, kWeapons, kGarments, kLayers };
struct SpriteTablePath {
  SpriteTableFile file;
  std::string_view path;
};
inline constexpr std::array<SpriteTablePath, 5> kSpriteTablePaths = {{
    {SpriteTableFile::kClasses, "data\\yaml_files\\class_sprites.yml"},
    {SpriteTableFile::kHeadgears, "data\\yaml_files\\headgear_sprites.yml"},
    {SpriteTableFile::kWeapons, "data\\yaml_files\\weapon_sprites.yml"},
    {SpriteTableFile::kGarments, "data\\yaml_files\\garment_sprites.yml"},
    {SpriteTableFile::kLayers, "data\\yaml_files\\garment_layers.yml"},
}};
inline constexpr std::uint32_t kSpriteTablesVersion = 1;

// Ajoute à `tables` ce que porte le texte d'un fichier. Refuse un texte d'une
// autre forme (voir plus haut), d'une autre version, ou dont un champ n'a pas
// son type — et `tables` peut alors en avoir reçu une partie.
[[nodiscard]] core::Result<void> ParseSpriteTableText(SpriteTableFile file,
                                                      std::string_view yaml_text,
                                                      SpriteTables& tables);

// Lit les cinq fichiers et les .act des accessoires de familier.
//
// UN FICHIER ABSENT laisse sa table vide, et le reste se lit : une
// installation sans capes habille encore ses personnages. UN FICHIER ILLISIBLE
// refuse le tout, en se nommant : mieux vaut le savoir qu'habiller tout le
// monde en Poring.
[[nodiscard]] core::Result<SpriteTables> ReadSpriteTables(vfs::Vfs& vfs);

}  // namespace ro::render
