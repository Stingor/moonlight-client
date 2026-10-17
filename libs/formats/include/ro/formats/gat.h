#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/water_field.h"

namespace ro::formats {

// Fichier .gat : la grille d'altitude et de franchissement d'une carte.
//
// En-tête de 14 octets — « GRAT », majeure u8, mineure u8, largeur i32,
// hauteur i32 — puis largeur × hauteur cellules de 20 octets.
//
// Référence : GRF/FileFormats/GatFormat/ de GRFEditor, et read_map() dans
// src/tool/mapcache.cpp de rAthena.

inline constexpr std::size_t kGatHeaderSize = 14;
inline constexpr std::size_t kGatCellSize = 20;

// Types de cellule tels qu'ils apparaissent dans le fichier.
enum class GatCellType : std::int32_t {
  kWalkable = 0,
  kBlocked = 1,
  kBlockedNoSnipe = 2,
  kWalkableWater = 3,
  kUnknown = 4,
  kBlockedSnipable = 5,
  kWalkable3 = 6,
};

// Bit de poids fort du type : marqueur d'EAU.
//
// Ce n'est ni du bruit ni une corruption, contrairement à ce que laissent
// entendre les traitements de rAthena (qui tronque à un octet) et de GRFEditor
// (qui propose de masquer le bit). Relevé sur 300 cartes réelles confrontées au
// cache du serveur :
//
//   type brut     valeur serveur
//   0x80000000    3  (28 339 fois)   ← marchable + eau
//   0x80000001    1  (541 166 fois)  ← bloquée : le drapeau ne change rien
//   0x80000002    2
//   0x80000005    5
//
// La règle qui rend compte des quatre lignes : le drapeau transforme une
// cellule MARCHABLE (0) en eau marchable (3), et n'a aucun effet sur les
// autres types. C'est la même sémantique que la règle de hauteur, par une
// autre voie — l'éditeur de carte a marqué l'eau explicitement plutôt que de
// la laisser déduire du niveau.
inline constexpr std::uint32_t kGatWaterFlag = 0x80000000U;

// Les deux types de cellule que la dérivation du serveur manipule : la cellule
// marchable ordinaire, et la même sous l'eau — marchable aussi.
inline constexpr std::uint32_t kGatWalkable = 0;
inline constexpr std::uint32_t kGatWalkableWater = 3;

struct GatCell {
  // Hauteurs des quatre coins. La disposition n'est pas celle qu'on devine :
  //
  //     2-----3
  //     | \   |
  //     |  \  |
  //     |   \ |
  //     0-----1
  //
  // L'indice 0 est le coin BAS-GAUCHE, et c'est celui-là que le serveur lit
  // pour décider si une cellule est sous l'eau.
  std::array<float, 4> corner_height{};

  // Conservé BRUT, drapeau d'eau compris (voir kGatWaterFlag).
  //
  // Ne rien masquer ici est délibéré : le bit de poids fort porte une
  // information que ni rAthena ni GRFEditor ne conservent, et l'effacer au
  // parsing la perdrait pour tout le monde. C'est DeriveServerCells qui
  // l'interprète.
  std::int32_t type = 0;
};

class GatMap {
 public:
  [[nodiscard]] static core::Result<GatMap> Parse(std::span<const std::uint8_t> data);

  [[nodiscard]] std::int32_t width() const noexcept { return width_; }
  [[nodiscard]] std::int32_t height() const noexcept { return height_; }
  [[nodiscard]] std::uint8_t major_version() const noexcept { return major_version_; }
  [[nodiscard]] std::uint8_t minor_version() const noexcept { return minor_version_; }

  [[nodiscard]] const std::vector<GatCell>& cells() const noexcept { return cells_; }

  // Indexation ligne par ligne : cells()[y * width + x].
  [[nodiscard]] const GatCell& at(std::int32_t x, std::int32_t y) const {
    return cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                  static_cast<std::size_t>(x)];
  }

  [[nodiscard]] bool InBounds(std::int32_t x, std::int32_t y) const noexcept {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
  }

  // LE RELIEF POSÉ À UNE SEULE ALTITUDE : les quatre coins de chaque cellule
  // prennent `height`, dans l'unité et le sens du fichier. Les types, drapeau
  // d'eau compris, ne bougent pas.
  void LevelCornerHeights(float height) noexcept;

  // LES QUATRE HAUTEURS DE LA CELLULE (x, y), dans l'ordre et le sens du
  // fichier : c'est par là que les pieds suivent le relief que retouche
  // l'éditeur de carte. Le type, qui porte la marchabilité, ne bouge pas. Une
  // cellule hors de la grille n'est pas touchée.
  void SetCornerHeights(std::int32_t x, std::int32_t y,
                        const std::array<float, 4>& heights) noexcept;

 private:
  GatMap() = default;

  std::vector<GatCell> cells_;
  std::int32_t width_ = 0;
  std::int32_t height_ = 0;
  std::uint8_t major_version_ = 0;
  std::uint8_t minor_version_ = 0;
};

// Comment comparer la hauteur d'une cellule au niveau d'eau du RSW.
//
// Les deux règles existent réellement, et elles ne donnent pas le même
// résultat dès que le niveau d'eau n'est pas entier. Le détail et les mesures
// sont dans docs/adr/0004-niveau-d-eau-brut.md.
enum class WaterLevelRule {
  // Comparaison au niveau BRUT du RSW.
  //
  // C'est ce que produit Mapcache Editor, et donc ce que contient réellement le
  // map_cache.dat que le serveur charge au démarrage. Défaut, parce que c'est
  // la grille effectivement en vigueur en jeu.
  kRawLevel,

  // Comparaison au niveau TRONQUÉ à l'entier, comme le fait
  // src/tool/mapcache.cpp de rAthena, dont la lecture du niveau rend un int32.
  //
  // À utiliser si le cache du serveur est régénéré avec l'outil de rAthena.
  kTruncatedLevel,
};

// Reproduit read_map() de rAthena : un octet par cellule, dans l'ordre du
// fichier GAT.
//
// C'est la grille sur laquelle le SERVEUR raisonne. Un client qui en dérive une
// autre propose des chemins que le serveur refuse, et le joueur voit son
// personnage se replacer tout seul. C'est pourquoi cette fonction ne
// « ressemble » pas à celle du serveur : elle en est la copie littérale,
// troncature du type à un octet comprise.
//
// `water` porte le niveau applicable à chaque cellule : absent, uniforme, ou
// par zones selon l'endroit où la carte range son eau.
[[nodiscard]] std::vector<std::uint8_t> DeriveServerCells(
    const GatMap& gat, const WaterField& water, WaterLevelRule rule = WaterLevelRule::kRawLevel);

}  // namespace ro::formats
