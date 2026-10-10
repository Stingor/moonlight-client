#pragma once

// Les morceaux de la grille de groupe partagés entre ses deux sources.

#include <imgui.h>

#include "ro/ui/party_grid.h"

namespace ro::ui::party_grid {

// Ce qu'une tuile doit savoir en plus d'elle-même pour se peindre.
struct TilePaint {
  float scale = 1.0F;
  bool hovered = false;
};

// Peint une tuile dans `top_left..bottom_right`. Pose `*status_hovered` quand le
// curseur est sur une de ses icônes d'état : la tuile tait alors son infobulle.
void DrawTile(ImDrawList& into, const PartyGridTileView& member, ImVec2 top_left,
              ImVec2 bottom_right, const PartyGridLook& look, const TilePaint& paint,
              bool* status_hovered);

// Redonne entier ce que la tuile découpe, plus la classe et la carte.
void DrawTileTooltip(const PartyGridTileView& member, const PartyGridLook& look);

// Le clic droit sur une tuile : le menu d'entité quand un acteur représente le
// membre, sinon le menu de repli, figé sur l'état du membre au moment du clic.
void AskMemberMenu(const PartyGridTileView& member, const PartyGridView& view,
                   PartyGridState& state, PartyGridAnswer& answer);

void DrawMemberMenu(PartyGridState& state, const PartyGridView& view, PartyGridAnswer& answer);

}  // namespace ro::ui::party_grid
