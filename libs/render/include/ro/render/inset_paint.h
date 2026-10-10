#pragma once

#include <cstdint>

namespace ro::render {

// ── PEINDRE DANS UN RECTANGLE DE L'IMAGE ─────────────────────────────────────
//
// Pour une scène qui se peint AU MILIEU de l'interface — l'effet de chapeau sur
// le pantin d'un aperçu —, par-dessus ce qui est déjà dans l'image, et non dans
// une texture à part.
//
// 🔴 PAR-DESSUS, ET NON HORS ÉCRAN, parce qu'un effet se MÉLANGE à ce qu'il a
// derrière lui : la plupart s'ajoutent. Peint dans une texture transparente,
// il n'aurait rien à éclaircir, et aucune transparence ne rend une addition
// une fois l'image posée sur le jeu.

// Un rectangle, en pixels de l'image, compté depuis son coin BAS-gauche —
// celui d'OpenGL.
struct InsetRect {
  std::int32_t left = 0;
  std::int32_t bottom = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
};

// Pose la vue sur `view` — elle peut déborder de l'image —, borne la peinture à
// `clip`, et y efface la profondeur : ce qui était peint là ne cache rien de ce
// qui vient.
void BeginInsetPaint(const InsetRect& view, const InsetRect& clip);

// Rend ce que la peinture a changé et que l'appelant ne reposera pas : l'unité
// de texture active, l'épreuve de profondeur.
void EndInsetPaint();

}  // namespace ro::render
