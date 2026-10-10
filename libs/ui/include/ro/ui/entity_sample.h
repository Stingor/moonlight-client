#pragma once

#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/render/entity_names.h"
#include "ro/ui/cast_bars.h"
#include "ro/ui/entity_bars.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LE CADRE D'ESSAI DE LA PAGE « ENTITÉS » ─────────────────────────────────
//
// Une entité dessinée COMME SUR LA CARTE, au zoom courant, avec ses barres, son
// nom et sa zone cliquable posés comme le réglage les posera sur elle : même point des pieds, même
// haut du sprite, même empilement — les barres sous les pieds poussent le nom vers le bas, comme en
// jeu. `app` compose la vignette et ses mots ; ici on ne fait que placer et peindre.
//
// ⚠ LE NOM S'ÉCRIT DANS LA POLICE DE L'INTERFACE, et non dans l'atlas gravé des
// noms, qui ne se peint pas dans une page. La composition, la place, la couleur,
// l'ombre et l'opacité sont celles du jeu ; le dessin des lettres peut différer.

// UNE ENTITÉ EN VIGNETTE, où tombent ses ancrages dans l'image — en pixels
// d'image —, et ce que son étiquette écrit.
struct EntitySample {
  SkinImage sprite;
  ImVec2 feet{0.0F, 0.0F};
  float head_y = 0.0F;
  // CE QUE L'IMAGE PEINT, toutes pièces comprises, dans l'image : c'est cette
  // boîte que le jeu élargit au plancher pour en faire la zone qui répond au
  // clic.
  ImVec2 body_min{0.0F, 0.0F};
  ImVec2 body_max{0.0F, 0.0F};
  // L'échelle des écarts, celle du jeu au zoom courant : voir
  // `ro::render::ScreenAnchorScale`.
  float anchor_scale = 1.0F;
  // LA STATURE RÉSERVÉE, en pixels d'écran : celle du plus grand des
  // représentants qu'un cadre fait tourner. ⚠ SANS ELLE, LE CADRE CHANGEAIT DE
  // HAUTEUR À CHAQUE TOUR, et les réglages qu'on visait se dérobaient dessous.
  float reserved_stature = 0.0F;
  // DES MOTS PLAUSIBLES POUR L'ESPÈCE, que le réglage filtre comme en jeu : une
  // case « le titre » décochée l'efface ici aussi.
  std::string name;
  std::string title;
  std::string aside;
  std::string banner;
  // CE QUI SUIT LA SECONDE LIGNE, entre crochets : le rang dans la guilde pour
  // un joueur, l'élément pour un monstre (`@showmobinfo`).
  std::string badge;
  // L'EMBLÈME DE GUILDE posé à gauche de l'étiquette, ou une image vide.
  SkinImage emblem;
  // LE SORT QU'IL INCANTE dans le cadre, plausible pour l'espèce.
  std::string cast_skill;
};

// CE QUE LE CADRE MONTRE : les barres d'une espèce, le nom d'une autre — l'une
// ou l'autre peut manquer. Le groupe n'a pas de réglage de nom à lui, mais son
// nom s'écrit comme celui des joueurs.
struct EntitySampleShown {
  std::optional<EntityBarKind> bars;
  std::optional<render::NamePlateKind> name;
};

// La hauteur ordinaire du cadre, la moindre place gardée sous les pieds, et
// celle gardée au-dessus de la tête — de quoi y poser deux barres écartées.
inline constexpr float kEntitySampleHeightPixels = 240.0F;
inline constexpr float kEntitySampleFloorPixels = 48.0F;
inline constexpr float kEntitySampleHeadroomPixels = 72.0F;

// CE QUE LE JOUEUR A RÉGLÉ, que le cadre applique à son entité.
struct EntitySampleLooks {
  const EntityBarsLook& bars;
  const render::EntityNamesLook& names;
  const CastBarsLook& cast;
};

// Où l'entité tombe dans un cadre : l'image, ses pieds et le haut du sprite, en
// pixels d'écran. Les pieds sont centrés, `floor_pixels` gardés dessous.
struct EntitySamplePlace {
  ImVec2 image_min{0.0F, 0.0F};
  ImVec2 image_max{0.0F, 0.0F};
  ImVec2 feet{0.0F, 0.0F};
  float head_y = 0.0F;
};
[[nodiscard]] EntitySamplePlace PlaceEntitySample(ImVec2 frame_min, ImVec2 frame_max,
                                                  const EntitySample& sample, float floor_pixels);

// LES LIGNES DE L'ÉTIQUETTE, selon le réglage de son espèce — la composition du
// jeu, titre, parenthèse et seconde ligne comprises.
[[nodiscard]] std::vector<std::string> EntitySampleNameLines(const EntitySample& sample,
                                                             const render::EntityNameLook& look);

// L'ÉCART D'UNE LIGNE DE NOM À LA SUIVANTE, pour cette taille de police : celui
// d'une police ordinaire, un peu plus que sa taille.
[[nodiscard]] float EntitySampleLineStep(float font_pixels);

// LA PLACE SOUS LES PIEDS : les barres qui s'y posent, puis le nom, et une
// marge. Jamais moins que `kEntitySampleFloorPixels`.
[[nodiscard]] float EntitySampleFloor(const EntitySample& sample, const EntityBarsLook& bars,
                                      const render::EntityNamesLook& names,
                                      const EntitySampleShown& shown);

// LA PLACE AU-DESSUS DE LA TÊTE : de quoi y poser la barre de cast et son
// étiquette, telles qu'on les a réglées, pour une police d'interface de
// `font_pixels`. Jamais moins que `kEntitySampleHeadroomPixels`.
[[nodiscard]] float EntitySampleHeadroom(const EntitySample& sample, const CastBarsLook& cast,
                                         const EntitySampleShown& shown, float font_pixels);

// LA HAUTEUR DU CADRE : la stature de l'entité, la place au-dessus de sa tête,
// celle sous ses pieds. Un Baphomet ne tient pas dans le cadre d'un Poring.
[[nodiscard]] float EntitySampleHeight(const EntitySample& sample, float floor_pixels,
                                       float headroom_pixels = kEntitySampleHeadroomPixels);

// ── LA ZONE QUI RÉPOND AU CLIC ──────────────────────────────────────────────
//
// Le cadre la dessine autour de l'entité : EN GRIS celle du client tel quel, EN
// JAUNE celle du réglage. C'est le rectangle du jeu — la boîte du sprite
// élargie au plancher, autour de son centre —, en pixels d'écran comme le
// sprite du cadre. À réglage d'origine, les deux se confondent : seul le jaune
// se peint.

// Les deux planchers de la famille de l'entité, en pixels. Zéro : pas de zone.
struct EntityPickZones {
  float game_floor_pixels = 0.0F;
  float floor_pixels = 0.0F;
};

// La zone qui répond au clic le long d'un axe, élargie au plancher.
[[nodiscard]] ScreenBand PickZoneSpan(float low, float high, float floor_pixels);

// LE CADRE ENTIER : le fond, le sol, l'entité, ses barres, son nom, sa barre
// de cast au-dessus de la tête.
//
// ⚠ IL IGNORE « QUAND LA MONTRER » : une barre ou un nom réglés sur « la cible »
// ne paraîtraient pas pendant qu'on choisit leur couleur. Le mode réglage fait
// onduler les barres et boucler l'incantation ; hors de lui, elles sont
// entamées, pour se lire.
void PaintEntitySample(ImDrawList& into, ImVec2 frame_min, ImVec2 frame_max,
                       const EntitySample& sample, const EntitySampleLooks& looks,
                       const EntitySampleShown& shown, const EntityPickZones& zones,
                       double seconds);

}  // namespace ro::ui
