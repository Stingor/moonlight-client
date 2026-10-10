#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA TENTURE AU SOL : la figure du sous-type 48.
//
// ⚠ CE N'EST PAS UN BILLBOARD — rien de son orientation ne vient de la caméra
// —, et sa teinte ne vient pas du nœud mais d'une CASE de sa piste. C'est un
// rectangle DEBOUT, dont la base est un segment posé au sol et dont le haut est
// ce même segment soulevé.
//
// Les cinq identifiants qui l'emploient sont les marques au sol des chants et
// des danses : Dissonance, Assassin Cross, Ugly Dance, Don't Forget Me, Service
// For You. Le semis en fait une croix, un treillis ou une gerbe, selon l'octet
// de son site.

// LES CINQ TEINTES, choisies par la CASE ZÉRO de la piste.
//
// ⚠ C'EST UNE CASE, PAS UN CHAMP DE NŒUD : le bâtisseur y range l'octet de son
// site, et la figure le compare à quatre FLOTTANTS. Le chercher dans le nœud
// n'y trouverait rien, et la tenture sortirait blanche pour les cinq.
inline constexpr std::array<std::uint8_t, 3> kGroundCurtainPink = {255, 155, 155};
inline constexpr std::array<std::uint8_t, 3> kGroundCurtainGreen = {155, 255, 155};
inline constexpr std::array<std::uint8_t, 3> kGroundCurtainYellow = {255, 255, 100};
inline constexpr std::array<std::uint8_t, 3> kGroundCurtainSalmon = {255, 150, 150};
inline constexpr std::array<std::uint8_t, 3> kGroundCurtainPlain = {255, 255, 255};

// ⚠ LES DEUX PREMIERS STYLES SE MÉLANGENT, LES TROIS AUTRES S'AJOUTENT. Le
// client passe le mode UN aux styles un et deux, le mode ZÉRO — l'ADDITION —
// aux styles trois, quatre et au défaut. Les traiter tous pareil rendrait deux
// marques au sol laiteuses ou trois d'entre elles brûlées.
inline constexpr float kGroundCurtainFirstBlendedStyle = 1.0F;
inline constexpr float kGroundCurtainLastBlendedStyle = 2.0F;

// LA TEINTE ET LE MODE D'UN STYLE.
[[nodiscard]] std::array<std::uint8_t, 3> EffectGroundCurtainColor(float style);
[[nodiscard]] std::int16_t EffectGroundCurtainBlendMode(float style);

// LE RECTANGLE DEBOUT D'UNE PISTE, ou rien.
//
// Sa base va de l'ANCRE — dont seuls le x et le z comptent — à la troisième et
// à la sixième case de queue ; sa hauteur est la CLÉ, que le mouvement fait
// respirer.
//
// Rend `nullopt` quand la piste est éteinte ou son alpha nul : c'est la garde
// du client, mot pour mot.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeGroundCurtainQuad(const EffectTrack& track,
                                                                      glm::vec3 anchor);

}  // namespace ro::render
