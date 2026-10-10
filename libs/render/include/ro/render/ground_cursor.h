#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/render/ground_quad.h"

namespace ro::render {

// LE CURSEUR AU SOL : le cadre qui se pose sur la case que la souris désigne.
//
// Ce n'est pas le curseur de la souris — celui-là est un sprite d'écran, voir
// cursor.h. Celui-ci est un QUAD DE TERRAIN, qui épouse le relief de la case
// et se laisse couvrir par qui s'y tient.
//
// Le client le pose sur la case désignée, avec une couleur selon ce qu'elle
// est, et le dessine avec les cases du terrain.
//
// ⚠ CHEZ LE CLIENT C'EST UN RÉGLAGE : un raccourci bascule un drapeau, nul au
// démarrage, et rien n'est dessiné tant qu'il l'est. Nous le montrons TOUJOURS.
// Le jour où il se règle, c'est l'appelant qui s'abstiendra d'appeler ; rien
// ici n'a à connaître le réglage.

// La texture du cadre, dans l'installation du joueur.
//
// 32 × 32, entièrement transparente SAUF quatre équerres dans les coins : neuf
// pixels le long des bords extérieurs, quatre d'épaisseur. Elle ne porte
// qu'UNE couleur, le blanc opaque — tout ce qu'on voit du cadre vient donc de
// la teinte ci-dessous, et rien de la texture.
inline constexpr std::string_view kGroundCursorTexturePath = "data\\texture\\grid.tga";

// La teinte d'une case OUVERTE, en RGBA.
//
// Celle du client : 0x6600FFAA — en ARGB, donc un vert bleuté à quarante pour
// cent d'opacité. Il en pose deux autres, rouge pour un mur et jaune pour une
// case qu'on ne franchit qu'en tirant, mais il CACHE le cadre dans les deux cas
// : elles ne se voient jamais, et les porter ici laisserait croire le
// contraire.
inline constexpr std::array<std::uint8_t, 4> kGroundCursorColor = {0x00, 0xFF, 0xAA, 0x66};

// Le cadre à poser sur cette case, ou rien si elle n'en veut pas.
//
// Rend nullopt hors de la grille ET sur une case qui REFUSE la marche — un mur
// ou une case seulement tirable. C'est la règle du client, et elle vaut d'être
// dite : le curseur au sol n'est pas une décoration, il annonce qu'un clic
// mènerait là. Le poser sur un mur promettrait un pas qui n'aura pas lieu.
[[nodiscard]] std::optional<GroundQuad> ComposeGroundCursor(const formats::GndMap& gnd,
                                                            const formats::GatMap& gat,
                                                            std::int32_t cell_x,
                                                            std::int32_t cell_y);

}  // namespace ro::render
