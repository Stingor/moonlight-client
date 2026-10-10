#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_screen_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE VOILE : la nuit qui tombe autour de celui qui le porte.
//
// C'est la figure du sous-type 71. Onze identifiants l'emploient — dont
// l'aveuglement, l'effet 334 —, tous bâtis par le même bâtisseur.
//
// ⚠ CETTE FIGURE NE PEUT QU'ASSOMBRIR, CE N'EST PAS UN HALO (ADR 0017). Sa
// teinte n'est pas un paramètre : elle vaut dix par canal, ou trente quand la
// piste porte une marque. Une image nommée « fullb » peinte en dix sur dix sur
// dix est un noir, jamais un halo.
//
// CE QUE LE JEU DESSINE, au zoom par défaut :
//
//   - un CŒUR carré, de côté deux fois le rayon, tourné vers l'œil et centré sur
//     le porteur. Chacun de ses quatre quadrants porte la première image
//     ENTIÈRE, son coin transparent au centre : les quatre, en miroir, font un
//     disque clair qui s'assombrit en anneaux vers le bord ;
//   - un CADRE qui l'entoure, peint de la seconde image — un blanc plein —, donc
//     un noir uni ;
//   - le tout en mélange ORDINAIRE, sous l'alpha de la piste, qui monte d'une
//     unité par tic.
//
// 🔴 CE CLIENT LE PEINT À L'ÉCRAN, ET C'EST UN CHOIX. Posé dans le monde, le
// voile suit le zoom : dézoomé, le cadre ne couvre plus l'écran et la carte
// reparaît éclairée au-delà ; et le cœur, redressé vers l'œil au pied du
// porteur, s'enfonce à moitié dans le sol. Ce client garde donc l'aspect du
// zoom par défaut QUEL QUE SOIT le zoom : le cœur a le rayon en pixels qu'il
// aurait au recul du jeu, centré sur la place d'écran du porteur, et le cadre
// couvre TOUT l'écran hors du cœur. L'aspect au zoom par défaut, lui, reste
// celui du jeu : mêmes images, même teinte, même alpha, même mélange.

// COMBIEN DE QUADS le voile compte : quatre pour le cœur, quatre pour le cadre.
inline constexpr std::size_t kDarkVeilCoreQuads = 4;
inline constexpr std::size_t kDarkVeilFrameQuads = 4;
inline constexpr std::size_t kDarkVeilQuads = kDarkVeilCoreQuads + kDarkVeilFrameQuads;

// LES DEUX IMAGES, dans l'ordre où le bâtisseur les charge : le cœur puis le
// cadre. La figure ne les mélange jamais.
inline constexpr std::size_t kDarkVeilCoreTexture = 0;
inline constexpr std::size_t kDarkVeilFrameTexture = 1;

// LA MARQUE QUE CETTE FIGURE LIT, et son seul effet est d'éclaircir le voile.
inline constexpr std::size_t kMarkOfVeilShade = 0;
inline constexpr std::uint8_t kDarkVeilPaler = 1;

// LES DEUX NOIRS que la figure connaît, par canal. Rien d'autre n'est possible.
inline constexpr std::uint8_t kDarkVeilDeepChannel = 10;
inline constexpr std::uint8_t kDarkVeilPaleChannel = 30;

// LE MÉLANGE que la figure demande : l'ordinaire, par l'alpha de la source.
// C'est celui que la voie d'écran applique.
inline constexpr std::int32_t kDarkVeilOrdinaryBlend = 1;

// LE RAYON DU CŒUR EN PIXELS : ce qu'un rayon du monde mesure à l'écran quand
// il est vu au recul et sous le champ du jeu (`kGameViewDistance`,
// `kGameFovDegrees`), sur une fenêtre de cette hauteur.
//
// ⚠ IL NE LIT PAS LA CAMÉRA, et c'est tout le choix : un zoom ne le change
// pas. La hauteur de la fenêtre, elle, le change — le voile garde la même part
// de l'écran à toutes les résolutions, comme la scène qu'il recouvre.
[[nodiscard]] float DarkVeilScreenRadiusPixels(float radius_at_anchor, float viewport_height);

// LES QUADS D'ÉCRAN D'UNE PISTE : le cœur d'abord, le cadre ensuite.
//
// `bearer_pixel` est la place d'écran du porteur ; un porteur sans place —
// derrière la caméra — laisse l'écran entier au cadre. Le cadre se découpe en
// quatre bandes qui ne se recouvrent jamais : un recouvrement doublerait le
// noir pendant qu'il monte. Une bande qui tombe hors de l'écran ne sort pas.
//
// Rend un lot VIDE quand la piste est éteinte ou que son alpha est nul. ⚠ ET
// C'EST LE CAS AU PREMIER TIC, toujours : le bâtisseur sème l'alpha à zéro et
// c'est le mouvement qui le monte d'une unité par tic.
[[nodiscard]] std::vector<EffectScreenQuad> ComposeScreenDarkVeilQuads(
    const EffectTrack& track, std::optional<glm::vec2> bearer_pixel, float viewport_width,
    float viewport_height, const EffectOwnerTint& owner = {});

// LE CŒUR QUI REJOINT LE NOIR SANS ARÊTE : l'image du cœur, son opacité étirée
// pour atteindre le plein exactement au bord du carré.
//
// 🔴 L'IMAGE DU JEU N'Y ARRIVE PAS. Son dégradé est un disque parfait autour
// du coin transparent, mais il ne devient opaque qu'au-delà du cercle inscrit :
// au milieu d'un bord du carré, il ne vaut encore que 245 sur 255, et le plein
// n'est atteint qu'aux coins. Le cadre, lui, est opaque partout. Posés côte à
// côte, les deux laissent passer quatre pour cent de la scène d'un côté et rien
// de l'autre : l'arête du carré se lit sur tout son pourtour.
//
// CE CLIENT ÉTIRE DONC L'OPACITÉ du rapport qui porte le bord le plus clair au
// plein, et la borne au plein : le dégradé garde sa forme et ses anneaux,
// s'assombrit de quatre pour cent au plus, et touche le noir EXACTEMENT sur le
// cercle inscrit. Au-delà, jusqu'aux coins, tout est opaque — comme le cadre.
// Le disque est rond et rien ne marque le carré.
//
// Ni agrandir le cœur ni teinter le cadre n'y suffisaient : l'image est lue
// jusqu'à son bord, qui reste à 245 à toute taille, et un cadre à 245
// laisserait voir la scène sur tout l'écran.
//
// Le coin transparent de l'image est en bas à gauche : ses deux bords opposés
// — la dernière colonne et la première rangée — sont ceux qui touchent le
// cadre. Une image sans alpha plein ou vide sort telle quelle.
[[nodiscard]] formats::Image DarkVeilCoreJoinedToFrame(const formats::Image& core);

// LE NOM sous lequel une fenêtre range ce cœur étiré, à côté de l'image lue :
// le chemin de l'image, suivi de ce suffixe. Aucun chemin d'archive ne le
// porte.
inline constexpr std::string_view kDarkVeilJoinedCoreSuffix = "#joined_to_frame";

}  // namespace ro::render
