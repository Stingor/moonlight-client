#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/sim/companion_ai.h"
#include "ro/sim/packed_position.h"
#include "ro/ui/companion_ai_window.h"

namespace moonclient {

// ── LES ZONES DE L'IA D'UN COMPAGNON, DESSINÉES AU SOL ──────────────────────
//
// Autour du MAÎTRE : la distance de suivi, l'éloignement maximum et le rayon
// d'engagement ; puis la case que le compagnon garde après « aller ici ». Ce
// fichier dit QUELLES CASES se peignent, et de quelle couleur ;
// `world/companion_zones_view.h` les pose sur le relief.
//
// 🔴 LA FORME EST CELLE QUE L'IA MESURE, par ses propres fonctions. Le suivi
// et l'éloignement comparent la distance euclidienne tronquée à l'entier
// (`ro::sim::WithinCells`) : un disque. L'engagement se juge dans le carré du
// serveur (`ro::sim::WithinSquareCells`). Une zone d'une autre forme mentirait
// sur la case où il fait demi-tour.

enum class CompanionZoneKind : std::uint8_t { kFollow, kLeash, kEngage };
enum class CompanionZoneShape : std::uint8_t { kDisc, kSquare };

// LA FORME QUE L'IA DONNE À CETTE ZONE.
[[nodiscard]] CompanionZoneShape ShapeOfZone(CompanionZoneKind kind) noexcept;

// VRAI QUAND LA CASE à ce décalage du maître est dans la zone, selon la
// fonction même que l'IA appelle.
[[nodiscard]] bool ZoneHolds(CompanionZoneShape shape, int radius_cells, int dx_cells,
                             int dy_cells) noexcept;

// LES DISTANCES D'UN PROFIL, en cases. Zéro veut dire « aucune » pour
// l'éloignement et l'engagement ; un suivi nul colle le compagnon à la case du
// maître.
struct CompanionZoneRadii {
  int follow_cells = 0;
  int leash_still_cells = 0;
  int leash_walking_cells = 0;
  int engage_still_cells = 0;
  int engage_walking_cells = 0;
};

[[nodiscard]] CompanionZoneRadii ZoneRadiiOf(const ro::sim::CompanionAiSettings& settings) noexcept;
[[nodiscard]] CompanionZoneRadii ZoneRadiiOf(const ro::ui::CompanionBaseFields& fields) noexcept;

struct CompanionZone {
  CompanionZoneKind kind = CompanionZoneKind::kFollow;
  int radius_cells = 0;
  // SURVOLÉE DANS LA FENÊTRE : elle se montre seule, et plus épaisse.
  bool emphasized = false;
};

struct CompanionZonesAsked {
  CompanionZoneRadii radii;
  // LE MAÎTRE MARCHE : l'IA prend alors l'éloignement et l'engagement « en
  // marche », sinon ceux « immobile ».
  bool owner_walking = false;
  // Le curseur de distance survolé dans la fenêtre.
  ro::ui::CompanionAiZoneSlider hovered = ro::ui::CompanionAiZoneSlider::kNone;
  // La case que le compagnon garde, voir `ro::sim::CompanionBrain::held_cell`.
  std::optional<ro::sim::CellPosition> held_cell;
};

struct CompanionZonesPlan {
  std::vector<CompanionZone> zones;
  std::optional<ro::sim::CellPosition> held_cell;
};

// CE QUI SE DESSINE. Sans survol : le suivi, puis l'éloignement et
// l'engagement en vigueur pour la marche du maître quand ils bornent quelque
// chose, puis la case gardée. Au survol d'un curseur : sa zone seule, quelle
// que soit la marche du maître, et rien quand elle vaut « aucune ».
[[nodiscard]] CompanionZonesPlan PlanCompanionZones(const CompanionZonesAsked& asked);

// QUAND ELLES SE MONTRENT : la fenêtre « IA du compagnon » ouverte, ou la case
// « Montrer les zones au sol » cochée.
[[nodiscard]] constexpr bool CompanionZonesShown(bool window_open, bool kept_shown) noexcept {
  return window_open || kept_shown;
}

// LE COMPAGNON DONT LES ZONES SE DESSINENT : celui de l'onglet de la fenêtre
// s'il est là, sinon l'autre ; rien sans compagnon.
[[nodiscard]] std::optional<ro::sim::AiCompanion> ZonesCompanion(ro::sim::AiCompanion tab,
                                                                 bool homunculus_present,
                                                                 bool mercenary_present) noexcept;

// LES COULEURS, en rouge, vert, bleu, opacité. ⚠ TRANSLUCIDES MAIS FRANCHES :
// le sol de nuit est sombre et celui d'un désert clair, et une teinte pâle se
// perdrait sur l'un des deux.
inline constexpr std::array<std::uint8_t, 4> kFollowZoneColor = {64, 150, 255, 140};
inline constexpr std::array<std::uint8_t, 4> kLeashZoneColor = {255, 150, 30, 140};
inline constexpr std::array<std::uint8_t, 4> kEngageZoneColor = {240, 45, 45, 140};
inline constexpr std::array<std::uint8_t, 4> kHeldCellColor = {80, 230, 120, 200};
// L'opacité d'une zone survolée, plus marquée.
inline constexpr std::uint8_t kEmphasizedZoneAlpha = 220;
// L'ÉPAISSEUR DU BORD, en cases : on peint le bord et non l'aplat, qui
// noierait le sol sous trois zones superposées.
inline constexpr int kZoneEdgeCells = 1;
inline constexpr int kEmphasizedZoneEdgeCells = 2;

[[nodiscard]] std::array<std::uint8_t, 4> ZoneColorOf(const CompanionZone& zone) noexcept;

// UNE CASE À PEINDRE, en coordonnées de la carte. ⚠ SIGNÉES : une zone près du
// bord déborde de la grille, et c'est la pose sur le relief qui l'écarte.
struct ZoneCellPaint {
  std::int32_t cell_x = 0;
  std::int32_t cell_y = 0;
  std::array<std::uint8_t, 4> color = {};
};

// LES CASES PEINTES : le bord de chaque zone autour de `owner_cell` —
// `kZoneEdgeCells` d'épaisseur, `kEmphasizedZoneEdgeCells` survolée —, puis la
// case gardée, pleine. Ajoutées à `into`.
void PaintCompanionZones(const CompanionZonesPlan& plan, ro::sim::CellPosition owner_cell,
                         std::vector<ZoneCellPaint>& into);

}  // namespace moonclient
