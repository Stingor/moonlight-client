#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES FIGURES QUI SE DESSINENT DEPUIS UNE PISTE.
//
// Elles diffèrent de celles d'`effect_node_world.h` sur le point qui décide de
// tout : leur géométrie ne vient pas des champs du nœud mais de l'ÉTAT qu'une
// piste entretient — son rayon, son inclinaison, et la longueur de chacun de
// ses segments. Voir `effect_track.h`.
//
// Toutes sortent par la primitive de ruban, qui pose le mélange et va chercher
// la teinte dans sa palette : `effect_trail_palette.h`.

// Combien de quads un ruban porte au plus : un de moins que ses segments, un
// segment ne faisant un quad qu'avec son voisin.
inline constexpr std::size_t kEffectTrackMaxQuads = kEffectTrackSlots - 1;

// LA JUPE D'ANNEAU : un ruban circulaire dont le bord extérieur se SOULÈVE.
//
// Ce n'est ni un anneau plat ni une couronne (ADR 0017). Un cercle intérieur
// est posé au plan de la piste ; pour chaque segment, un second point s'en
// écarte de la longueur du segment, inclinée — ce qui le lève plus qu'il ne
// l'éloigne tant que l'inclinaison est forte. La bande qui joint les deux est
// la jupe, et le mouvement la couche peu à peu.
//
// Seize sous-types mènent ici, et ils ne partagent que cette figure : leurs
// mouvements sont neuf différents. C'est le sous-type qui décide, jamais le
// dessinateur.
//
// `variant` est le numéro que le nœud porte pour sa teinte — voir la palette.
//
// ⚠ LE DERNIER SEGMENT REFERME EXACTEMENT LE CERCLE quand le balayage fait le
// tour : le client y remet l'angle de départ au lieu de l'accumuler, ce qui
// évite qu'un arrondi laisse une fente entre le premier quad et le dernier.
//
// Rend une liste VIDE quand la piste est éteinte, quand son balayage est
// négatif, ou quand elle n'a pas encore de longueur — un ruban sans aire ne se
// distingue pas d'une absence, et en soumettre serait payer un lot de rendu
// pour rien.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeEffectTrackSkirt(
    const EffectTrack& track, std::uint8_t variant, glm::vec3 anchor,
    const EffectRingOrientation& orientation = kEffectRingUpright);

// LE RUBAN DE TRAÎNÉE : la jupe d'anneau à trois différences près.
//
// Trois sous-types y mènent — 36, 110 et 169 — et ils ne partagent pas leur
// mouvement : le cent dix passe par les groupes radiaux, les deux autres
// travaillent sur place.
//
// Ce qui le sépare de `ComposeEffectTrackSkirt` :
//
//   - ONZE POINTS, PAS VINGT ET UN. Le pas angulaire vaut le balayage divisé par
//     DIX, et les cases au-delà de la dixième ne sont jamais lues ;
//   - IL NE SE SOULÈVE PAS. La jupe compare sa clé à deux flottants exacts pour
//     s'élever ; ce dessinateur-ci ne porte aucune de ces constantes. Lui
//     prêter l'élévation de sa sœur le décollerait du sol ;
//   - L'ORDONNÉE DE SON ANCRE EST UNE SENTINELLE. Voir ci-dessous.
//
// ⚠ `turn_degrees` N'EST LU QUE PAR LA SENTINELLE. À une valeur EXACTE de
// l'ordonnée de l'ancre, le client cesse de la lire comme une coordonnée : il
// tourne l'abscisse et la profondeur de l'ancre de ce cap et les ajoute aux
// deux points. C'est l'ADR 0017 sous sa forme la plus nette — et lire cette
// ordonnée comme une hauteur poserait le ruban hors de l'écran. L'ANNEAU DE
// NUAGE : la jupe d'anneau à trois différences près, et ce n'est pas la même
// liste que celle du ruban de traînée.
//
// Le sous-type 62 y mène, et SIX identifiants l'emploient.
//
// Ce qui le sépare de `ComposeEffectTrackSkirt` :
//
//   - SON PAS VAUT LE BALAYAGE DIVISÉ PAR VINGT ET UN, là où la jupe le divise
//     par VINGT. Un cran d'écart, et la boucle ne fait pas le même nombre de
//     tours ;
//   - SA CLÉ EST UNE HAUTEUR. La jupe compare la sienne à deux flottants exacts
//     pour s'élever ; celui-ci la prend TELLE QUELLE comme ordonnée de ses deux
//     bords. C'est l'ADR 0017 : le champ n'a pas de sens hors du dessinateur ;
//   - IL N'A AUCUNE GARDE D'ALPHA. La jupe abandonne une piste à alpha nul ;
//     celui-ci peint quand même, et son mouvement fait justement naître ses
//     pistes à zéro.
//
// ⚠ ET SA BOUCLE TOURNE PLUS QUE SES SEGMENTS N'EN COMPTENT. Elle avance d'un
// pas ENTIER tant que l'angle cumulé n'atteint pas le balayage : à trente
// degrés, le pas vaut un et la boucle fait trente tours pour vingt et un
// segments. Le client borne alors son index à vingt, si bien que les derniers
// quads se superposent sur la dernière longueur.
//
// ⚠ ET UN BALAYAGE SOUS VINGT ET UN FIGERAIT LE CLIENT : son pas tomberait à
// zéro et son angle cumulé n'avancerait jamais. Le bâtisseur part de trente et
// le mouvement ne fait que croître, si bien que le cas ne se présente pas —
// mais nous rendons une liste vide plutôt que de tourner sans fin.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeCloudRingQuads(
    const EffectTrack& track, std::uint8_t variant, glm::vec3 anchor,
    const EffectRingOrientation& orientation = kEffectRingUpright);

[[nodiscard]] std::vector<EffectWorldQuad> ComposeEffectTrackRibbon(
    const EffectTrack& track, std::uint8_t variant, glm::vec3 anchor, std::int16_t turn_degrees,
    const EffectRingOrientation& orientation = kEffectRingUpright);

// LE CAP QUE LA SENTINELLE DU RUBAN EMPLOIE, pris là où le client le range.
//
// ⚠ IL EST DANS UNE PISTE, ET PAS DANS CELLE QU'ON DESSINE : le mouvement des
// groupes radiaux l'écrit dans le champ de style de la TROISIÈME, et le
// dessinateur l'y relit à un déplacement absolu. Le chercher dans la piste
// courante rendrait un cap différent pour chacune des quatre.
[[nodiscard]] std::int16_t EffectRibbonTurnOf(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
