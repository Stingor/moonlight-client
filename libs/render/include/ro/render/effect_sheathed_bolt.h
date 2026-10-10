#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE TRAIT GAINÉ : un prisme à cinq faces tendu entre deux centres, peint DEUX
// FOIS — une âme mince, puis une gaine à peine plus large qui s'ajoute.
//
// Le dessinateur du sous-type 69, et le cas 69 du répartiteur de mouvement.
// Quatre identifiants l'emploient.
//
// ⚠ ON LA DIRAIT « ARC » ET « BALAYAGE », ET LA FIGURE NE BALAIE RIEN
// (ADR 0017). Elle est immobile du premier tic au dernier : le mouvement
// recalcule chaque tic un bout lointain qui ne change pas, et seul l'alpha
// vit. Chercher une rotation qui n'existe pas ferait tourner un trait droit.
//
// ⚠ ET SON CERCLE EST DEBOUT : le client prend l'azimut PLUS QUATRE-VINGT-DIX
// pour l'axe horizontal du pentagone, et la verticale pour l'autre. Le cercle
// est donc perpendiculaire au trait, et le coucher à plat en ferait un ruban.

// COMBIEN DE CÔTÉS, et de combien l'angle avance entre deux sommets.
//
// ⚠ SIX TOURS DE BOUCLE POUR CINQ CÔTÉS, comme la traînée pentagonale du
// sous-type 66 : soixante-douze degrés refont le tour en cinq pas, et le
// sixième sommet retombe sur le premier. Compter les tours donnerait six
// quads, dont un d'aire nulle.
inline constexpr std::size_t kSheathedBoltSides = 5;
inline constexpr std::int32_t kSheathedBoltStepDegrees = 72;

// DE COMBIEN LE PLAN DU CERCLE TOURNE par rapport à l'azimut du trait.
inline constexpr std::int32_t kSheathedBoltRingQuarterTurn = 90;

// LES DEUX PASSES, et ce qui les sépare.
//
// ⚠ LA GAINE S'AJOUTE ET L'ÂME NON, et c'est là toute la lueur. Peindre les
// deux du même mélange éteindrait le halo ; n'en peindre qu'une le supprimerait.
inline constexpr std::size_t kSheathedBoltPasses = 2;
inline constexpr std::int16_t kSheathedBoltCoreBlend = 1;
inline constexpr std::int16_t kSheathedBoltSheathBlend = 0;

// DE COMBIEN LA GAINE DÉBORDE L'ÂME, en unités du monde — sauf pour le style
// rose, dont la gaine vaut le DOUBLE de l'âme.
inline constexpr float kSheathedBoltSheathGrowth = 0.25F;
inline constexpr float kSheathedBoltDoubledSheathStyle = 3.0F;

// LES QUATRE STYLES QUE LA FIGURE ET LE MOUVEMENT NOMMENT, rangés en flottant
// dans la piste comme le client les range.
inline constexpr float kSheathedBoltWhiteStyle = 2.0F;
inline constexpr float kSheathedBoltPinkStyle = 3.0F;
inline constexpr float kSheathedBoltBlueStyle = 4.0F;
inline constexpr float kSheathedBoltDimStyle = 5.0F;
// ET CELUI QU'AUCUN DES DEUX NE NOMME, qu'un cinquième bâtisseur pose pourtant.
//
// ⚠ LE DÉFAUT N'EST PAS UN TROU DE LA TABLE : l'initialisation de l'éclat de
// nuage est le trait bleu à un style près, où elle écrit zéro là où le
// bleu écrit quatre. Il tombe donc sur la teinte par défaut — âme jaune, gaine
// ivoire — et sur la rampe d'alpha « sinon ». C'est le seul des cinq à montrer
// ce que la figure garde pour les styles qu'elle ne connaît pas.
inline constexpr float kSheathedBoltUnnamedStyle = 0.0F;

// LES TEINTES, en dur dans le dessinateur.
//
// ⚠ LES DEUX PASSES NE DIFFÈRENT QUE PAR DÉFAUT : les quatre styles nommés
// donnent la même couleur à l'âme et à la gaine, et seul un style inconnu
// passe du jaune à l'ivoire. C'est le seul écart connu, et le supprimer
// perdrait la seule chose que les deux passes ne partagent pas.
inline constexpr std::array<std::uint8_t, 3> kSheathedBoltWhite = {255, 255, 255};
inline constexpr std::array<std::uint8_t, 3> kSheathedBoltPink = {255, 89, 182};
inline constexpr std::array<std::uint8_t, 3> kSheathedBoltBlue = {55, 55, 255};
inline constexpr std::array<std::uint8_t, 3> kSheathedBoltDim = {55, 55, 55};
inline constexpr std::array<std::uint8_t, 3> kSheathedBoltCoreDefault = {255, 255, 55};
inline constexpr std::array<std::uint8_t, 3> kSheathedBoltSheathDefault = {255, 255, 200};

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017).
//
// La clé de décalage y est un RAYON, la première valeur une HAUTEUR, et la
// cinquième le style.
inline constexpr std::size_t kSheathedBoltSlotOfLift = 0;
inline constexpr std::size_t kSheathedBoltSlotOfStyle = 4;
// Les trois premières cases de queue portent le bout LOINTAIN du trait.
inline constexpr std::size_t kSheathedBoltSlotOfFarX = 0;
inline constexpr std::size_t kSheathedBoltSlotOfFarY = 1;
inline constexpr std::size_t kSheathedBoltSlotOfFarZ = 2;

// LA TEINTE D'UNE PASSE, selon le style que la piste porte.
[[nodiscard]] std::array<std::uint8_t, 3> EffectSheathedBoltColor(float style, bool sheath);

// LE RAYON D'UNE PASSE, selon le style que la piste porte.
[[nodiscard]] float EffectSheathedBoltRadius(float style, float core_radius, bool sheath);

// LES DIX QUADS D'UNE PISTE : cinq pour l'âme, cinq pour la gaine.
//
// Rend un lot VIDE quand la piste est éteinte ou que son alpha est nul.
//
// ⚠ LES COINS SONT ABSOLUS : le client pousse un au dix-huitième paramètre de
// sa primitive. L'ancre de la piste et son bout lointain sont déjà dans le
// monde — le bâtisseur y a recopié la position de l'effet, et le mouvement y
// pose l'autre bout. Ajouter l'origine du nœud doublerait cette position.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSheathedBoltQuads(
    const EffectTrack& track, const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 69 : un trait qui paraît puis s'efface.
//
// ⚠ IL NE DÉPLACE RIEN. Le bout lointain qu'il écrit à chaque tic vaut toujours
// la même chose — l'ancre plus la portée le long de l'azimut —, et ni l'ancre,
// ni la portée, ni l'azimut ne bougent. Y chercher une avancée ferait grandir
// un trait qui naît de sa longueur définitive.
//
// ⚠ ET IL NE SUIT PAS SON PORTEUR : le bâtisseur recopie la position une fois,
// au semis. Le faire suivre traînerait le trait derrière celui qui l'a lancé.
//
// Trois rampes d'alpha, choisies par le style :
//
//   - le style sombre monte de HUIT pendant vingt tics, puis descend de DEUX ;
//   - le style blanc monte de DEUX pendant cinq tics, puis descend d'UN ;
//   - tout autre monte de HUIT pendant cinq tics, puis descend de QUATRE.
void AdvanceSheathedBoltTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
