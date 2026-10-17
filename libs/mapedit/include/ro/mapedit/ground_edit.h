#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/mapedit/map_edit.h"

namespace ro::mapedit {

// LE RELIEF RETOUCHÉ : voir `GroundEdit` et docs/adr/0118, phase 3.
//
// Une levée s'applique à TOUS les coins de cube qui touchent son point —
// jusqu'à quatre, le fichier dupliquant chaque coin entre cubes voisins. Une
// falaise, dont les cubes portent des hauteurs différentes au même point,
// monte donc d'un bloc et garde sa marche.
//
// ⚠ LE FICHIER COMPTE SES HAUTEURS VERS LE BAS : lever de `lift` retranche
// `lift` des hauteurs du GND comme de celles de la GAT.

// Pose les retouches du relief sur `gnd`. Un point hors de la grille des coins
// n'est pas posé, et le rapport le dit.
[[nodiscard]] MapEditReport ApplyGroundEdits(std::span<const GroundEdit> edits,
                                             formats::GndMap& gnd);
[[nodiscard]] MapEditReport ApplyGroundEdits(const MapEdit& edit, formats::GndMap& gnd);

// LA MÊME LEVÉE SUR LES HAUTEURS DE LA GRILLE DE MARCHE, pour que les pieds
// suivent le sol. Une cellule vaut un demi-cube : un coin de cellule qui tombe
// entre deux points de la grille des coins prend la levée interpolée
// bilinéairement entre eux. `gnd_width` × `gnd_height` : la taille du sol en
// cubes, qui borne la grille des coins ; au-delà, la levée est nulle.
//
// 🔴 LES TYPES NE BOUGENT JAMAIS : la marchabilité reste celle du serveur.
void LiftWalkHeights(std::span<const GroundEdit> edits, std::int32_t gnd_width,
                     std::int32_t gnd_height, formats::GatMap& gat);

// LA HAUTEUR DU POINT (x, y) de la grille des coins, dans le sens du MONDE
// (vers le haut) : la moyenne des coins de cube qui le touchent. Rien hors de
// la grille.
[[nodiscard]] std::optional<float> CornerPointHeight(const formats::GndMap& gnd, std::int32_t x,
                                                     std::int32_t y);

// LA BROSSE DE RELIEF de l'éditeur.
enum class GroundBrushMode : std::uint8_t {
  // Lève le sol, ou l'abaisse.
  kRaise = 0,
  // Tire chaque point vers une hauteur donnée : celle du centre au premier
  // appui.
  kFlatten = 1,
  // Tire chaque point vers la moyenne de ses quatre voisins.
  kSmooth = 2,
};

struct GroundBrush {
  // Le rayon, en cubes : en points de la grille des coins.
  float radius_cubes = 1.0F;
  // Ce que la brosse déplace en une seconde, au centre, en unités du monde.
  float units_per_second = 1.0F;
  GroundBrushMode mode = GroundBrushMode::kRaise;
};

// La part de la force à `distance` du centre, dans la même unité que `radius` :
// 1 au centre, 0 au rayon et au-delà, une retombée en cosinus entre les deux,
// qui part et arrive à plat.
[[nodiscard]] float BrushFalloff(float distance, float radius);

// UN PAS DE BROSSE de `seconds`, centré en (`center_x`, `center_y`) dans la
// grille des coins : les levées à AJOUTER au document, une par point touché,
// rangées comme lui. `lower` : la brosse Lever abaisse ; les autres modes
// l'ignorent. `flatten_height` : la hauteur que vise Aplanir, dans le sens du
// monde. Un point n'est jamais tiré au-delà de la hauteur qu'il vise.
[[nodiscard]] std::vector<GroundEdit> BrushStroke(const formats::GndMap& gnd,
                                                  const GroundBrush& brush, float center_x,
                                                  float center_y, float seconds, bool lower,
                                                  float flatten_height);

}  // namespace ro::mapedit
