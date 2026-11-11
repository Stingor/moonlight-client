#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "ro/render/effect_magic_target.h"

namespace ro::render {

// LE CERCLE DE VISÉE : le carré que le client peint à même le terrain, autour de
// la case visée, pendant qu'on incante un sort posé au sol.
//
// Une seule image couvre tout le carré, découpée case par case. Elle tourne, et
// son opacité bat au rythme de sa rotation. Le carré vit ce que dure
// l'incantation.
//
// ⚠ CE N'EST PAS LA GRILLE DE VISÉE D'EF_GROUNDSAMPLE (`effect_magic_target.h`),
// qui a toujours sept cases de côté et deux teintes lavande : le cercle a la
// taille de la zone du sort.

// LE CÔTÉ DU CARRÉ, en cases, pour une compétence : sa portée de zone plus deux.
// Zéro quand la compétence n'en montre aucun ; deux pour une compétence que la
// table ne nomme pas.
[[nodiscard]] std::int32_t GroundScopeSide(std::uint16_t skill_id);

// L'IMAGE du carré, relative au dossier des textures : la cible ordinaire, ou
// le cercle propre de trois compétences.
[[nodiscard]] std::string_view GroundScopeTexture(std::uint16_t skill_id, bool minimal_effects);

// L'ANGLE, en radians, après `ticks` tics : il avance d'un demi-radian divisé par
// le côté à chaque tic.
[[nodiscard]] float GroundScopeAngle(std::int32_t side, std::uint32_t ticks);

// LA TEINTE, en RGBA : blanche, d'une opacité de 128 relevée de quarante fois le
// sinus de l'angle quand il est positif. Celle de CD_COMPETENTIA ne garde que
// six dixièmes de cette opacité.
[[nodiscard]] std::array<std::uint8_t, 4> GroundScopeColor(std::uint16_t skill_id, float angle);

// LES CASES À PEINDRE, en écart à la case visée, dans l'ordre du client : ligne
// après ligne.
[[nodiscard]] std::vector<EffectGroundCellPaint> ComposeGroundScopeCells(
    std::int32_t side, float angle, const std::array<std::uint8_t, 4>& color);

}  // namespace ro::render
