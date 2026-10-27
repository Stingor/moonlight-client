#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace moonclient {

// L'ASPECT DU SOL GÉNÉRÉ : ce qu'app pose sur les pièces que `sim` décide, et
// le sol qu'elle peint dessous. Rien ici ne change une case : une retouche de
// ce catalogue ne monte pas la version des règles (ADR 0134, T1).
//
// Les noms de fichiers sont ceux de l'installation, écrits en UTF-8 : les
// modèles sous `data\model\`, les textures sous `data\texture\`. Les modèles et
// les textures viennent des champs de Prontera (`roc-cli survivors pieces`,
// ADR 0134), sauf les repères, que ces champs n'ont pas.

// UN MODÈLE POSSIBLE pour une place, et son poids parmi les autres.
struct GroundModelChoice {
  std::string_view model;
  float scale = 1.0F;
  std::uint32_t weight = 1;
  // Le rayon de l'ombre cuite qu'il pose au sol, en cases ; 0 : aucune.
  float shade_cells = 0.0F;
};

// UNE PLACE DE MODÈLE DANS UNE PIÈCE : où il se pose dans l'emprise d'ORIGINE
// (non tournée, le nord en haut), en cases depuis son coin sud-ouest, un point
// sous une case bloquée ; et les modèles qui peuvent l'occuper.
struct PieceModelSlot {
  float east_cells = 0.0F;
  float north_cells = 0.0F;
  std::span<const GroundModelChoice> models;
  // Vrai : le modèle tourne d'un angle tiré (arbre, rocher, buisson) ; faux :
  // il suit le quart de tour de la pièce, plus `yaw_degrees`.
  bool any_yaw = true;
  float yaw_degrees = 0.0F;
};

// LE DESSIN D'UNE PIÈCE, par la clé de sa règle dans `sim`.
struct PieceLook {
  std::string_view key;
  std::span<const PieceModelSlot> slots;
};

// LE RÔLE D'UNE TEXTURE DE SOL. Une texture des champs couvre 4 × 4 cubes,
// soit un emplacement de 8 × 8 cases : l'herbe de fond se raccorde à
// elle-même ; une TACHE est un motif entier — de la terre bordée d'herbe —
// posé, tourné, sur l'emplacement d'une pièce ; une tache ÉPARSE se pose
// rarement sur un emplacement vide.
//
// LES CHEMINS se peignent cube par cube. Chaque coin de cube est de terre
// quand au moins la moitié des quatre cases qui le touchent sont de chemin ;
// un cube prend le carreau qui raccorde ses quatre coins, tourné : la terre
// pleine (kPathFull), un bord (kPathEdge), un coin de terre seul
// (kPathOuter) ou un coin d'herbe seul (kPathInner). Hors de la terre pleine,
// un carreau est UN cube de sa texture, le quart (`quarter_u`, `quarter_v`),
// qui montre, à son quart de tour nul, l'est vers les u croissants et le nord
// vers les v croissants : la terre à l'ouest pour un bord, au nord-ouest seul
// pour un coin de terre, partout sauf au sud-ouest pour un coin d'herbe. La
// lisière d'un carreau passe au milieu de ses côtés : d'un carreau à l'autre,
// elle se raccorde.
//
// CE QU'UN CARREAU LIT suit la case du monde là où la texture se prolonge :
// la terre pleine lit le cube de sa place dans le motif de 4 × 4, comme
// l'herbe, et un bord glisse le long de sa colonne. Un bord dont la texture
// déclare `east_quarter_u` ne fait jamais de demi-tour : la terre à l'est (ou
// au nord) lit, tournée comme celle de
// l'ouest (ou du sud), la colonne `east_quarter_u`, qui porte l'autre moitié
// de la même bande de terre — un chemin d'un cube de large y lit une bande
// entière, et le grain peint garde son sens. Un coin montre toujours son cube,
// tourné : aucun cube voisin de la texture ne le prolonge.
enum class GroundTextureRole : std::uint8_t {
  kGrass,
  kPatch,
  kScatter,
  kPathFull,
  kPathEdge,
  kPathOuter,
  kPathInner,
};

struct GroundTextureLook {
  std::string_view texture;
  GroundTextureRole role = GroundTextureRole::kGrass;
  // Parmi les textures de même rôle, la part de celle-ci.
  std::uint32_t weight = 1;
  // Le cube de la texture qu'un carreau de chemin montre, de 0 à 3 sur chaque
  // axe ; un bord glisse le long de sa colonne `quarter_u`.
  std::uint8_t quarter_u = 0;
  std::uint8_t quarter_v = 0;
  // La colonne d'un bord dont la terre est à l'est, à son quart de tour nul.
  // Une texture qui ne la déclare pas (kNoEastColumn) n'a que sa colonne
  // `quarter_u` : la terre à l'est ou au nord la lit retournée d'un demi-tour.
  std::uint8_t east_quarter_u = kNoEastColumn;
  static constexpr std::uint8_t kNoEastColumn = 0xFFU;
};

// LE SEMIS : un décor qui ne bloque pas — champignons, souches, touffes, fleurs.
struct SeedLook {
  std::string_view model;
  float scale = 1.0F;
  std::uint32_t weight = 1;
};

// UN BIOME : le sol, les pièces, le semis, et la carte dont il prend la lumière,
// le brouillard, le ciel et la musique.
struct BiomeLook {
  // La clé de la règle de terrain de `sim`.
  std::string_view terrain_key;
  std::string_view reference_map;
  std::span<const GroundTextureLook> textures;
  std::span<const PieceLook> pieces;
  std::span<const SeedLook> seeds;
  // Combien de semis par morceau de 32 × 32 cases.
  std::uint32_t seeds_per_chunk = 0;
  // LE SEMIS DE BORDURE : sur une case libre qui longe un chemin, à sa chance
  // pour mille.
  std::span<const SeedLook> verge_seeds;
  std::uint32_t verge_per_mille = 0;
  // Pour mille : la chance qu'un emplacement à pièce porte une tache, et
  // qu'un emplacement vide en porte une éparse.
  std::uint32_t patch_per_mille = 0;
  std::uint32_t scatter_per_mille = 0;
};

// L'aspect du terrain de clé `terrain_key`, ou nullptr.
[[nodiscard]] const BiomeLook* FindBiomeLook(std::string_view terrain_key);

// Le dessin de la pièce de clé `key` dans `biome`, ou nullptr.
[[nodiscard]] const PieceLook* FindPieceLook(const BiomeLook& biome, std::string_view key);

}  // namespace moonclient
