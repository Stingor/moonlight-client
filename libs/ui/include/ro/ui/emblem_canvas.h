#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ro::ui {

// ── LE CANEVAS DE L'ÉDITEUR D'EMBLÈME : 24 × 24 pixels peints en jeu ───────
//
// Pas de fichier à préparer : on peint les 576 pixels, et c'est `app` qui en
// fait le BMP qui part au serveur. Le magenta pur est la couleur « vide » —
// c'est la teinte que le jeu rend transparente.
//
// ⚠ UN ÉTAT D'ÉDITEUR, PAS UN ÉTAT DE JEU : comme le texte d'un champ de
// saisie, il appartient à la fenêtre qui le dessine. Les opérations sont des
// fonctions libres, pour s'essayer sans rien dessiner.

inline constexpr int kEmblemCanvasSide = 24;
inline constexpr std::size_t kEmblemCanvasPixels =
    static_cast<std::size_t>(kEmblemCanvasSide) * static_cast<std::size_t>(kEmblemCanvasSide);
// `0xFF00FF`, que le jeu rend transparent.
inline constexpr std::uint32_t kEmblemCanvasClear = 0xFF00FFU;

// L'épaisseur du trait, en pixels de côté.
inline constexpr int kEmblemBrushMost = 3;
// Ce que « Annuler » garde en mémoire, en coups de pinceau.
inline constexpr std::size_t kEmblemUndoMost = 40;

// Les trois premiers outils peignent au fil du geste ; les trois suivants se
// tirent d'un point à l'autre et ne s'appliquent qu'au relâchement.
enum class EmblemTool : std::uint8_t { kPencil, kEraser, kFill, kLine, kRect, kEllipse };

inline constexpr std::size_t kEmblemToolCount = 6;

using EmblemCanvasPixels = std::array<std::uint32_t, kEmblemCanvasPixels>;

// La couleur de départ du pinceau : un rouge franc, qui se voit sur le vide.
inline constexpr std::array<float, 3> kEmblemFirstInk = {0.85F, 0.15F, 0.15F};

struct EmblemCanvas {
  // `0xRRGGBB`, rangée zéro en haut ; `kEmblemCanvasClear` est le vide.
  EmblemCanvasPixels pixel = {};
  // ⚠ FAUX TANT QU'IL N'A PAS ÉTÉ VIDÉ UNE PREMIÈRE FOIS : un tableau de zéros
  // est un emblème NOIR, pas un emblème vide.
  bool started = false;
  EmblemTool tool = EmblemTool::kPencil;
  int brush = 1;
  // Chaque trait est répété en miroir gauche/droite.
  bool mirror = false;
  // Rectangle et ellipse pleins plutôt qu'en contour.
  bool filled = false;
  std::array<float, 3> color = kEmblemFirstInk;
  // 🔴 LA TRANSPARENCE EST UNE COULEUR, PAS UN OUTIL : sans ça, « remplir de
  // vide » ou « tracer une forme vide » obligeraient à passer par la gomme, qui
  // ne sait que peindre à main levée.
  bool color_clear = false;
  // Un coup de souris est en cours : il ne compte que pour une annulation.
  bool stroke_open = false;
  // La dernière case peinte, pour relier le trait d'une image à l'autre.
  int last_x = -1;
  int last_y = -1;
  // La forme en cours de tirage : rien n'est peint tant que le bouton tient.
  bool shape_active = false;
  int shape_x0 = 0;
  int shape_y0 = 0;
  // Tirée au clic droit : elle efface.
  bool shape_erase = false;
  // Monte à chaque changement : c'est elle qui dit que le BMP est à refaire.
  int revision = 0;
  std::vector<EmblemCanvasPixels> undo;
};

// La couleur du pinceau : la teinte choisie, ou le vide.
[[nodiscard]] std::uint32_t EmblemInk(const EmblemCanvas& canvas);
[[nodiscard]] std::uint32_t PackEmblemColor(const std::array<float, 3>& rgb);

// Vide tout. ⚠ SANS EMPILER : c'est à l'appelant de décider si l'état d'avant
// mérite d'être annulable.
void ClearEmblemCanvas(EmblemCanvas& canvas);

// Empile l'état courant avant un coup de pinceau : « Annuler » remonte coup par
// coup, et non pixel par pixel.
void PushEmblemUndo(EmblemCanvas& canvas);
// Rend faux quand il n'y a rien à annuler.
bool UndoEmblemCanvas(EmblemCanvas& canvas);

// Le pinceau posé sur une case : un carré de `brush` pixels de côté, et son
// miroir si demandé.
void PaintEmblemCell(EmblemCanvas& canvas, int column, int row, std::uint32_t color);
// Relie deux cases : à soixante images par seconde la souris saute plusieurs
// pixels entre deux images, et un trait rapide laisserait des pointillés.
void StrokeEmblemCanvas(EmblemCanvas& canvas, int from_column, int from_row, int to_column,
                        int to_row, std::uint32_t color);
// Remplit la zone de même couleur, en quatre-connexité.
void FillEmblemCanvas(EmblemCanvas& canvas, int column, int row, std::uint32_t color);

// LE MASQUE D'UNE FORME : l'aperçu pendant le tirage ET la peinture au
// relâchement, donc ce qu'on voit est exactement ce qu'on obtient.
using EmblemMask = std::array<bool, kEmblemCanvasPixels>;
[[nodiscard]] EmblemMask EmblemShapeMask(const EmblemCanvas& canvas, int to_column, int to_row);
void ApplyEmblemMask(EmblemCanvas& canvas, const EmblemMask& mask, std::uint32_t color);

// Remplace le dessin par des pixels venus d'ailleurs — un fichier, l'emblème en
// place, une icône d'objet. ⚠ ANNULABLE : le dessin en cours ne se perd pas.
void LoadEmblemCanvas(EmblemCanvas& canvas, const EmblemCanvasPixels& pixels);

// Le dessin porte-t-il au moins un pixel non vide ?
[[nodiscard]] bool EmblemCanvasHasInk(const EmblemCanvas& canvas);

}  // namespace ro::ui
