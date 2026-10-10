#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_four_particle_slots.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES LUCIOLES : quatre carrés qui errent autour du nœud, s'éloignent, se
// replacent au hasard, et SCINTILLENT.
//
// La figure sort par la voie A, et le mouvement est celui du cas 130 du
// répartiteur. TROIS identifiants l'emploient.
//
// ⚠ IL N'Y A AUCUN POLYÈDRE (ADR 0017). Ce sont quatre carrés indépendants,
// chacun tourné vers la caméra — la SEPTIÈME figure du lot bâtie sur le carré à
// quarante-cinq degrés.
//
// ⚠ ET SA DISPOSITION DE PARTICULES EST CELLE DE LA RONDE du sous-type 27 :
// voir `effect_four_particle_slots.h`. C'est le même rangement du client, et
// c'est tout ce que les deux familles ont en commun.

inline constexpr std::size_t kWanderingMoteParticles = kEffectFourParticles;

// COMBIEN DE PAIRES D'ANGLES la piste poursuit : HUIT, comme la rafale, et des
// MÊMES pas — deux en descente, trois en montée.
inline constexpr std::size_t kWanderingMoteAnglePairs = 8;

// LES CASES DE MARQUE : le style, la teinte, le scintillement, l'éclat.
//
// ⚠ L'ÉCLAT EST UN OCTET SIGNÉ, et le dessinateur en fait le DOUBLE : un octet
// devenu négatif ne peint rien du tout. Le lire non signé peindrait à plein.
inline constexpr std::size_t kWanderingMoteMarkOfStyle = 0;
inline constexpr std::size_t kWanderingMoteMarkOfColor = 5;
inline constexpr std::size_t kWanderingMoteMarkOfFirstTwinkle = 7;
inline constexpr std::size_t kWanderingMoteMarkOfFirstGlow = 11;

// L'ÉCLAT : son plafond, son pas de montée, et ce que le dessinateur en fait.
inline constexpr std::int16_t kWanderingMoteGlowCap = 120;
inline constexpr std::int16_t kWanderingMoteGlowRise = 1;
inline constexpr std::int16_t kWanderingMoteGlowToAlpha = 2;

// LE STYLE QUI NE GARDE QU'UNE LUCIOLE, et qui la fait monter au lieu d'errer.
inline constexpr std::uint8_t kWanderingMoteLoneStyle = 1;

// LES QUATRE TEINTES QUE LE DESSINATEUR NOMME, et celle où il retombe.
inline constexpr std::uint8_t kWanderingMotePinkColor = 7;
inline constexpr std::uint8_t kWanderingMoteYellowColor = 8;
inline constexpr std::uint8_t kWanderingMoteWhiteColor = 10;
inline constexpr std::array<std::uint8_t, 3> kWanderingMotePink = {250, 70, 100};
inline constexpr std::array<std::uint8_t, 3> kWanderingMoteYellow = {250, 250, 10};
inline constexpr std::array<std::uint8_t, 3> kWanderingMoteWhite = {250, 250, 250};
inline constexpr std::array<std::uint8_t, 3> kWanderingMoteGreen = {70, 120, 100};

// LA TEINTE d'une piste, et L'ALPHA d'une de ses lucioles.
[[nodiscard]] std::array<std::uint8_t, 3> EffectWanderingMoteColor(std::uint8_t mark);
[[nodiscard]] std::uint8_t EffectWanderingMoteAlpha(std::int8_t glow);

// LES QUATRE CARRÉS d'une piste.
//
// `turn_degrees` et `tilt_degrees` sont les deux angles de caméra, que le
// mouvement range dans les styles des pistes zéro et un.
//
// ⚠ AUCUNE GARDE DE HAUTEUR, au contraire de la ronde : les quatre carrés se
// peignent où qu'ils soient. Leur en imposer une en ferait disparaître la
// moitié.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeWanderingMoteQuads(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees,
    glm::vec3 anchor, const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 130 : elles errent, fuient, reviennent, scintillent.
//
// ⚠ SON COMPTEUR AVANCE MÊME SUR UNE PISTE ÉTEINTE : le client incrémente avant
// de tester. C'est sans effet aujourd'hui et ce serait faux de le déplacer.
void AdvanceWanderingMoteTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                                std::uint32_t& generator);

}  // namespace ro::render
