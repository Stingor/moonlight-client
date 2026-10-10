#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE BILLBOARD DE PALETTE : un quad debout, dont la teinte croise deux champs.
//
// C'est la figure du sous-type 132. Elle sort par la voie A, une seule fois.
// Sept identifiants l'emploient, tous posés par le même bâtisseur : ce sont les
// éclats de gel qui montent en spirale.
//
// La figure elle-même est simple — deux points diamétralement opposés sur un
// cercle, et les deux mêmes une taille plus bas, ce qui fait un quad debout
// orienté par l'azimut. Presque tout le travail est dans la TEINTE.
//
// ⚠ SA TEINTE CROISE DEUX CHAMPS : une case choisit une PALETTE parmi sept, une
// autre une NUANCE parmi quatre, et la palette dit quels canaux reçoivent la
// nuance tandis que les autres restent pleins. Sept fois quatre cas, plus une
// exception. Chercher une seule table de couleurs n'en trouverait aucune.

// COMBIEN DE NUANCES, et ce que chacune vaut sur le canal qu'elle teinte.
inline constexpr std::size_t kPaletteBillboardShades = 4;
inline constexpr std::array<std::uint8_t, kPaletteBillboardShades> kPaletteBillboardShadeValues = {
    105, 5, 55, 155};

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017).
//
// ⚠ LA TAILLE N'EST PAS SEMÉE, ELLE EST CALCULÉE. Le bâtisseur laisse cette case
// à zéro et la figure ne rend alors rien ; c'est le mouvement qui l'y écrit à
// chaque tic. Un quad absent au premier tic n'est donc pas un quad manquant.
inline constexpr std::size_t kSlotOfBillboardSize = 2;
inline constexpr std::size_t kSlotOfBillboardShade = 3;
inline constexpr std::size_t kSlotOfBillboardLift = 4;
inline constexpr std::size_t kSlotOfBillboardPalette = 5;

// LA COULEUR qu'une palette et une nuance donnent ensemble.
//
// Toute palette hors des sept relevées rend celle du cas par défaut du client —
// qui n'est pas une absence mais une branche à part entière.
[[nodiscard]] std::array<std::uint8_t, 3> EffectPaletteBillboardColor(std::uint8_t palette,
                                                                      std::uint8_t shade);

// LE QUAD D'UNE PISTE, ou rien.
//
// ⚠ TROIS CONDITIONS, ET NON UNE : la piste vive, son alpha positif, ET son
// compteur positif. La troisième compte — le semis retarde trois pistes sur
// quatre, et les dessiner d'emblée ferait partir les quatre éclats ensemble.
// S'y ajoute la taille, qu'un quad nul ne mérite pas d'être soumis.
[[nodiscard]] std::optional<EffectWorldQuad> ComposePaletteBillboardQuad(
    const EffectTrack& track, glm::vec3 node_origin, const EffectOwnerTint& owner = {});

}  // namespace ro::render
