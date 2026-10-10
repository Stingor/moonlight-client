#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA TRAÎNÉE PENTAGONALE : un tube à cinq côtés tendu entre deux centres.
//
// C'est le dessinateur du sous-type 66. Il sort par la voie A, une seule fois
// par côté. Sept identifiants l'emploient, tous bâtis par le même bâtisseur de
// couches empilées : ce sont les nuages qui s'élèvent.
//
// ⚠ SIX TOURS DE BOUCLE, ET LA FIGURE EST UN PENTAGONE (ADR 0017). Le client
// boucle six fois, ce qui fait bien six SOMMETS — mais son pas angulaire vaut
// SOIXANTE-DOUZE degrés, si bien que le sixième retombe exactement sur le
// premier et referme la figure. Cinq côtés, donc, et cinq quads. Compter les
// tours de boucle au lieu des côtés en donnerait six, dont un d'aire nulle.
//
// Ce qu'elle fait : deux cercles de MÊME RAYON, autour de DEUX CENTRES
// différents que la piste porte, et un quad par côté entre les deux. Le rayon
// vient de la case que l'anneau prend pour une clé de décalage — ADR 0017
// encore.

// COMBIEN DE CÔTÉS, et de combien l'angle avance entre deux sommets.
inline constexpr std::size_t kPentagonTrailSides = 5;
inline constexpr std::int32_t kPentagonTrailStepDegrees = 72;

// LA MARQUE QUE CETTE FIGURE LIT, et son seul effet est de changer la teinte.
inline constexpr std::size_t kMarkOfTrailShade = 0;
// La seule valeur qui donne le bleu. Toute autre donne le vert pâle.
inline constexpr std::uint8_t kPentagonTrailBlue = 13;

// LES DEUX TEINTES, en dur dans le dessinateur comme celles du voile.
inline constexpr std::array<std::uint8_t, 3> kPentagonTrailBlueColor = {80, 80, 255};
inline constexpr std::array<std::uint8_t, 3> kPentagonTrailPaleColor = {230, 255, 230};

// LES SIX CASES DE QUEUE QUE LA PISTE PORTE : le centre du premier cercle, puis
// celui du second. Leur sens appartient à cette figure et à elle seule.
inline constexpr std::size_t kTailSlotOfNearCenterX = 0;
inline constexpr std::size_t kTailSlotOfNearCenterY = 1;
inline constexpr std::size_t kTailSlotOfNearCenterZ = 2;
inline constexpr std::size_t kTailSlotOfFarCenterX = 6;
inline constexpr std::size_t kTailSlotOfFarCenterY = 7;
inline constexpr std::size_t kTailSlotOfFarCenterZ = 8;

// LES CINQ QUADS D'UNE PISTE.
//
// Rend un lot VIDE quand la piste est éteinte ou que son alpha est nul.
//
// ⚠ LES COINS SONT ABSOLUS : le client pousse un au dix-huitième paramètre de sa
// primitive, si bien que l'origine du nœud n'entre PAS. Les deux centres que la
// piste porte sont déjà dans le monde — le bâtisseur y a recopié la position de
// l'effet. Ajouter l'origine du nœud doublerait cette position.
[[nodiscard]] std::vector<EffectWorldQuad> ComposePentagonTrailQuads(
    const EffectTrack& track, const EffectOwnerTint& owner = {});

}  // namespace ro::render
