#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "ro/formats/image.h"
#include "ro/render/effect_magic_target.h"

namespace ro::render {

// LE MESSAGE D'UN GRAFFITI, TRACÉ AU SOL.
//
// Le client compose le message dans une TOILE carrée, lettre par lettre, avec
// la planche `effect\alpabet.bmp` — huit glyphes de trente-deux pixels par
// rangée —, puis la couche sur une grille de dix-huit cases sur dix-huit
// centrée sur l'unité. La toile tourne avec le regard qu'avait le joueur de la
// caméra quand l'unité a paru : le message se lit depuis là où il se tenait.

// LA PLANCHE DES LETTRES, dans les textures du jeu.
inline constexpr std::string_view kGraffitiAlphabetPath = "data\\texture\\effect\\alpabet.bmp";

// LE CÔTÉ DE LA TOILE, en pixels.
inline constexpr std::int32_t kGraffitiCanvasSide = 384;

// LE CÔTÉ DE LA GRILLE, en cases, et la case de l'unité en son milieu.
inline constexpr std::int32_t kGraffitiGridSide = 18;
inline constexpr std::int32_t kGraffitiGridHalf = kGraffitiGridSide / 2;

// LA TOILE d'un message, en RGBA : transparente hors des lettres.
//
// Les lettres, majuscules ou minuscules, et les chiffres occupent trente-deux
// pixels ; « ? » et « ! » seize, et tout autre signe seize pixels VIDES. Une
// espace avance de la largeur du signe qui la précède, et passe à la ligne si
// le mot qui la suit ne tient plus dans la ligne.
//
// ⚠ « ! » DESSINE LE ZÉRO DE LA PLANCHE : le client lui donne le rang de ce
// glyphe, et non celui du point d'exclamation, qui reste inemployé.
[[nodiscard]] formats::Image ComposeGraffitiCanvas(const formats::Image& alphabet,
                                                   std::string_view message);

// LES TROIS CENT VINGT-QUATRE CASES de la grille, chacune avec sa portion de
// toile, pour un joueur dont le corps faisait cet angle — en degrés du client.
[[nodiscard]] std::vector<EffectGroundCellPaint> ComposeGraffitiCells(float body_degrees);

// LE FLAG GRAFFITI : l'emblème d'une guilde peint au sol, une création du
// serveur Moonlight que le client d'origine ne connaît pas.
//
// Un carré de (2 × niveau + 1) cases de côté, centré sur l'unité, et l'emblème
// étiré dessus le haut vers le nord de la carte : trois cases au niveau un,
// onze au niveau cinq. Un niveau nul compte pour un.
[[nodiscard]] std::int32_t FlagGraffitiSide(std::int32_t level);
[[nodiscard]] std::vector<EffectGroundCellPaint> ComposeFlagGraffitiCells(std::int32_t level);

}  // namespace ro::render
