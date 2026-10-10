#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE RUBAN QUI TOURNE : un arc couché à plat, large d'une demi-largeur fixe,
// que le mouvement fait pivoter sur lui-même.
//
// Son mouvement est celui des cas 77 et 91 du répartiteur de mouvement. TROIS
// identifiants l'emploient, sur deux sous-types, et c'est la troisième famille
// de la voie B.
//
// ⚠ CE N'EST PAS LA JUPE DE `ComposeEffectTrackSkirt`, ET ELLE LUI RESSEMBLE.
// La jupe pose un cercle intérieur et un point qui s'en écarte de la longueur
// de CHAQUE segment ; celui-ci ne lit aucune longueur de segment et pose deux
// bords SYMÉTRIQUES de part et d'autre d'un cercle incliné. Les confondre
// donnerait un ruban sans aire, puisque les longueurs y naissent nulles.

// COMBIEN DE PAS le balayage se divise en.
//
// ⚠ VINGT, ET LE COMPTE DE QUADS N'EST PAS FIXE POUR AUTANT. Le pas est le
// balayage divisé par vingt EN ENTIERS, et la boucle avance tant qu'elle ne
// dépasse pas le balayage : un balayage qui n'est pas multiple de vingt donne
// donc un ou deux échantillons de plus, dont l'index de bande reste bloqué au
// vingtième.
inline constexpr std::int32_t kSpinRibbonSteps = 20;

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017).
//
// ⚠ LA CLÉ N'EST PAS UNE CLÉ ICI MAIS UNE DEMI-LARGEUR, et le champ de rayon
// est bien le rayon. La valeur zéro porte la hauteur du ruban au-dessus du
// nœud, la valeur un la vitesse à laquelle il tourne.
inline constexpr std::size_t kSpinRibbonSlotOfHeight = 0;
inline constexpr std::size_t kSpinRibbonSlotOfSpinSpeed = 1;

// LE RUBAN D'UNE PISTE.
//
// `variant` est le numéro de teinte du nœud — voir `effect_trail_palette.h`.
// `anchor` est l'origine du nœud : les deux bords lui sont RELATIFS.
//
// Rend un lot VIDE quand la piste est éteinte, quand son balayage est négatif,
// quand son alpha n'est pas strictement positif, ou quand son balayage est trop
// court pour donner un pas.
//
// ⚠ CE DERNIER CAS N'EST PAS DANS LE CLIENT, ET C'EST VOULU : un balayage de
// moins de vingt degrés y donne un pas NUL, et sa boucle ne s'arrête jamais.
// Les deux bâtisseurs tirent entre cent quatre-vingts et deux cent soixante-dix
// degrés, si bien que le client n'y arrive pas ; nous ne pouvons pas nous
// permettre de l'y suivre.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSpinRibbonQuads(const EffectTrack& track,
                                                                  std::uint8_t variant,
                                                                  glm::vec3 anchor);

// LE MOUVEMENT DU SOUS-TYPE 77 : le ruban tourne, et rien d'autre.
//
// Chaque piste avance son azimut de SA propre vitesse — trois à six degrés par
// tic —, si bien que les quatre rubans d'un nœud se décalent les uns des
// autres. Leur donner la même vitesse les figerait en bloc.
void AdvanceSpinRibbonTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LE MOUVEMENT DU SOUS-TYPE 91 : le même, plus une vie.
//
// ⚠ QUATRE CHOSES DE PLUS, et c'est pourquoi c'est un autre mouvement : les
// pistes démarrent en retard et chacune du sien, leur alpha monte puis retombe,
// leur rayon enfle d'un centième par tic, et leur inclinaison avance d'un degré
// — ou de DEUX, une fois sur deux, au tirage.
void AdvanceRisingSpinRibbonTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                   std::uint32_t& generator);

}  // namespace ro::render
