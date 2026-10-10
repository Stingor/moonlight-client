#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA COQUILLE SPHÉRIQUE : la figure du sous-type 61.
//
// Il y a bien une sphère, mais on n'en voit qu'une CEINTURE — trois latitudes
// sur un demi-tour de longitude —, et elle est peinte TROIS FOIS sur trois
// rayons voisins. Ce n'est pas une boule, c'est un bouclier
// bombé de quinze panneaux, et le seul effet qui l'emploie s'appelle EF_GUARD.
//
// Chaque panneau est un carré TANGENT à la sphère : ses deux axes locaux sont
// la direction du parallèle et celle du méridien, si bien qu'il épouse la
// courbure au lieu de faire face à la caméra. Cette figure n'est donc PAS un
// billboard, et rien de son orientation ne vient de la vue.

// COMBIEN DE PANNEAUX LA CEINTURE PORTE, et de combien ils s'écartent.
//
// ⚠ CINQ COLONNES AU PAS DE QUARANTE-CINQ DEGRÉS FONT UN DEMI-TOUR, pas un
// tour : la coquille est OUVERTE derrière. C'est ce qui en fait un bouclier —
// il protège d'un côté. Boucler la boucle sur huit colonnes en ferait une
// boule, et le dos cesserait d'être découvert.
inline constexpr std::size_t kSphereShellRowCount = 3;
inline constexpr std::size_t kSphereShellColumnCount = 5;
inline constexpr std::int32_t kSphereShellStepDegrees = 45;

// COMBIEN DE COQUILLES SE SUPERPOSENT, et sur combien de quads en tout.
inline constexpr std::size_t kSphereShellLayerCount = 3;
inline constexpr std::size_t kSphereShellQuadsPerTrack =
    kSphereShellRowCount * kSphereShellColumnCount * kSphereShellLayerCount;

// LA DEMI-TAILLE D'UN PANNEAU DE L'ÉQUATEUR, dont les rangées du dessus et du
// dessous retranchent leur rang.
//
// ⚠ LE CLIENT RANGE CETTE TAILLE DANS LE RAYON À L'ANCRE DE LA PISTE, et l'y
// relit aussitôt pour les deux coquilles suivantes. C'est un BROUILLON, pas un
// état : le bâtisseur y sème UN, et cette valeur n'est jamais lue — la figure
// l'écrase avant que quiconque la regarde. La calculer ici plutôt que
// d'écrire dans la piste donne exactement la même image.
inline constexpr float kSphereShellEquatorHalfSize = 3.0F;

// DE COMBIEN CHAQUE COQUILLE S'ÉLOIGNE DE LA CLÉ, et où son centre se pose.
//
// ⚠ LA PREMIÈRE N'EST PAS CENTRÉE COMME LES DEUX AUTRES : son centre est DEUX
// unités plus haut. Les trois coquilles ne sont donc pas concentriques, et leur
// décalage se voit à l'équateur — c'est ce qui donne son épaisseur au bouclier.
// Les centrer ensemble les superposerait exactement.
inline constexpr std::array<float, kSphereShellLayerCount> kSphereShellRadiusGrowth = {0.0F, 0.2F,
                                                                                       0.4F};
inline constexpr float kSphereShellInnerCenterLift = 2.0F;

// LE MÉLANGE ET L'IMAGE DE CHAQUE COQUILLE.
//
// ⚠ LES DEUX COQUILLES INTÉRIEURES S'AJOUTENT, LA TROISIÈME SE MÉLANGE. Le
// client passe le mode ZÉRO aux deux premières — l'ADDITION, voir
// `EffectParticleBlend` — et le mode UN à la dernière, qui est le mélange
// ordinaire par l'alpha. Leur donner le même mode ferait un halo deux fois trop
// clair ou un bord deux fois trop opaque, selon le sens de l'erreur.
//
// ⚠ ET LA TROISIÈME COQUILLE PORTE LA SECONDE IMAGE, sauf pour une marque. La
// marque CINQ la ramène à la première image et au mélange additif : c'est le
// seul des cinq identifiants qui ne montre jamais `guardK2.tga`.
inline constexpr std::size_t kSphereShellOuterLayer = kSphereShellLayerCount - 1;
inline constexpr std::uint8_t kSphereShellPlainOuterMark = 5;

// LES TEINTES, une par marque — et la marque zéro en a TROIS.
//
// ⚠ LES QUATRE MARQUES NOMMÉES PEIGNENT LEURS TROIS COQUILLES DE LA MÊME
// COULEUR, la marque zéro NON : ses trois verts diffèrent d'un cran chacun, du
// plus sombre au plus clair, et c'est la seule à faire varier sa teinte d'une
// coquille à l'autre. Lui donner un vert unique aplatirait le bouclier vert —
// celui d'EF_GUARD, l'identifiant le plus courant des cinq.
inline constexpr std::array<std::uint8_t, 3> kSphereShellAmber = {255, 155, 0};
inline constexpr std::array<std::uint8_t, 3> kSphereShellOrange = {255, 100, 0};
inline constexpr std::array<std::uint8_t, 3> kSphereShellPink = {255, 150, 255};
inline constexpr std::array<std::uint8_t, 3> kSphereShellBlue = {100, 100, 255};
inline constexpr std::array<std::array<std::uint8_t, 3>, kSphereShellLayerCount>
    kSphereShellGreens = {{{205, 255, 205}, {155, 255, 155}, {225, 255, 225}}};

// LA TEINTE D'UNE COQUILLE, selon la marque que le site a posée.
[[nodiscard]] std::array<std::uint8_t, 3> EffectSphereShellColor(std::uint8_t mark,
                                                                 std::size_t layer);

// LES QUARANTE-CINQ PANNEAUX D'UNE PISTE, dans l'ordre du client : rangée,
// colonne, puis les trois coquilles.
//
// ⚠ LA SEULE GARDE DU CLIENT EST LA PISTE ÉTEINTE. Il ne regarde NI l'alpha NI
// l'arc, contrairement à toutes les autres figures de ce lot : une coquille
// d'alpha nul sort bien quarante-cinq quads invisibles. En ajouter une serait
// inventer une règle, et la différence se verrait au premier tic — la piste
// naît justement à alpha zéro.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSphereShellQuads(const EffectTrack& track,
                                                                   glm::vec3 anchor);

}  // namespace ro::render
