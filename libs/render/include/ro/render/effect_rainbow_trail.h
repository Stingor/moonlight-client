#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ARC-EN-CIEL : sept bandes de couleur, emboîtées et balayées ensemble.
//
// Le dessinateur du sous-type 93. Il sort par une TROISIÈME voie de dessin, et
// il est le SEUL à l'emprunter.
//
// ⚠ CETTE VOIE-LÀ N'A PAS DE PALETTE. Celle du ruban choisit sa teinte dans un
// switch de cent un cas sur un champ du nœud ; celle-ci la REÇOIT, trois octets
// à la fois. C'est pourquoi les sept couleurs vivent ici, dans la figure, et non
// dans une table partagée : il n'y a personne avec qui les partager. Le compte
// tranche — un seul dessinateur l'appelle, sur un seul site.
//
// ⚠ ET SON ORDONNÉE EST DOUBLÉE. Le client écrit deux fois le sinus au lieu de
// le multiplier : l'arc est une DEMI-ELLIPSE deux fois plus haute que large, et
// non un demi-cercle. La lire comme un cercle l'écraserait de moitié.
//
// ⚠ SES BANDES SE CHEVAUCHENT D'UNE BANDE ENTIÈRE. La bande de rang N va de
// l'échelle « un moins cinq centièmes de N » à celle de N plus un : le bord
// extérieur d'une bande est le bord intérieur de la suivante, sans trou ni
// recouvrement. Leur donner chacune son épaisseur laisserait sept fentes.

// COMBIEN DE BANDES, et de combien leur échelle décroît d'une bande à l'autre.
inline constexpr std::size_t kRainbowTrailBands = 7;
inline constexpr float kRainbowTrailBandStep = 0.05F;

// EN COMBIEN DE SEGMENTS l'arc se coupe, et sur combien la coordonnée de texture
// se compte — la voie divise le rang par vingt et un, comme celle du ruban.
inline constexpr std::int32_t kRainbowTrailSegments = 20;
inline constexpr float kRainbowTrailTextureSpan = 21.0F;

// LES SEPT COULEURS, de la bande la plus large à la plus étroite : rouge, orangé,
// jaune, vert, bleu, indigo, violet.
[[nodiscard]] std::array<std::uint8_t, 3> EffectRainbowTrailBandTint(std::size_t band);

// LES QUADS D'UNE PISTE : jusqu'à sept fois vingt et un, ou un lot VIDE.
//
// Rend vide quand la piste est éteinte, quand son alpha est nul ou quand son
// balayage est négatif.
//
// ⚠ ET AUSSI QUAND LE BALAYAGE EST TROP COURT POUR UN PAS : le client divise par
// vingt sans se garder, et sa boucle ne finirait jamais. Le bâtisseur du
// sous-type 93 pose un demi-tour, si bien que le cas ne se présente pas.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeRainbowTrailQuads(const EffectTrack& track,
                                                                    glm::vec3 node_origin);

}  // namespace ro::render
