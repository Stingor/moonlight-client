#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <imgui.h>

#include "ro/render/battle_score_hud.h"
#include "ro/render/battlefield_art.h"
#include "ro/render/camera.h"
#include "ro/render/screen_overlay.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/battlefield.h"
#include "ro/sim/world_fwd.h"

#include "world/actor_visibility.h"

namespace moonclient {

// ── CE QUE LE COMBAT DU CHAMP DE BATAILLE MONTRE ────────────────────────────
//
// Le monde traduit en données d'écran : la marque au-dessus des ennemis, les
// chiffres du score, la couleur d'un camp. Rien hors d'une carte de champ de
// bataille.

// LES MARQUES DES ENNEMIS, au-dessus de leur tête. Un ennemi caché ne se marque
// pas : la marque dirait où il est.
//
// SAUF POUR UN ADMINISTRATEUR (`sees_concealed`) : il voit la marque des ennemis
// cachés, posée à leur place sur le sol, à la taille d'un corps ordinaire —
// celui qu'on ne dessine pas n'a pas d'échelle propre.
//
// ⚠ AUCUN ACTEUR N'EST EXEMPTÉ COMME ADMINISTRATEUR : rien de ce que le serveur
// dit d'un AUTRE joueur ne porte son niveau de groupe.
[[nodiscard]] std::vector<ro::render::ScreenQuad> EnemyMarkersOnScreen(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    std::span<const ConcealedPlace> concealed, bool sees_concealed, const ro::sim::World& world,
    const ro::render::BattlefieldArt& art, float viewport_width, float viewport_height);

// LE SCORE À PEINDRE, ou rien : hors d'une carte de champ de bataille, ou tant
// que le score n'est pas dit. `highlight_own` surligne le camp du joueur quand
// il est connu.
[[nodiscard]] std::optional<ro::render::BattleScoreFigures> BattleScoreOf(
    const ro::sim::World& world, bool highlight_own);

// La couleur d'un camp, sur la minimap ; un camp inconnu a la sienne.
[[nodiscard]] ImU32 BattleSideColor(ro::sim::BattleSide side);

}  // namespace moonclient
