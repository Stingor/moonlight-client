// Les deux poignées du HUD PVP. Voir l'en-tête pour ce que cette couche fait —
// et surtout pour ce qu'elle NE dessine pas.

#include "ro/ui/pvp_hud_frames.h"

#include <array>

#include <imgui.h>

namespace ro::ui {
namespace {

// LES IDENTITÉS IMGUI DES DEUX CADRES. ⚠ STABLES ET UNIQUES : ImGui range l'état
// d'une fenêtre sous son nom, et deux cadres homonymes partageraient le leur —
// on en déplacerait un en croyant tenir l'autre.
constexpr std::array<const char*, kPvpHudPartCount> kPartIds = {
    "##hud_pvp_rang",
    "##hud_pvp_chrono",
};

[[nodiscard]] float ReferenceWidthOf(PvpHudPart part) noexcept {
  return part == PvpHudPart::kRank ? kPvpRankReferenceWidth : kPvpTimerReferenceWidth;
}

}  // namespace

float PvpHudScaleOf(PvpHudPart part, const HudRect& rect) noexcept {
  const float reference = ReferenceWidthOf(part);
  if (!(reference > 0.0F) || !(rect.width > 0.0F)) {
    return 1.0F;
  }
  return rect.width / reference;
}

bool PvpHudRectUnset(const HudRect& rect) noexcept { return !(rect.width > 0.0F); }

HudRect DefaultPvpHudRect(PvpHudPart part, glm::vec2 viewport_px) noexcept {
  if (part == PvpHudPart::kRank) {
    // Le compteur se range dans le coin bas-droit : sa boîte y finit.
    const render::PvpHudPlacement pose = render::DefaultPvpRankPlacement(viewport_px);
    return HudRect{.x = pose.corner_px.x - kPvpRankReferenceWidth,
                   .y = pose.corner_px.y - kPvpRankReferenceHeight,
                   .width = kPvpRankReferenceWidth,
                   .height = kPvpRankReferenceHeight};
  }
  // Le chronomètre se centre en haut : sa boîte y commence.
  const render::PvpHudPlacement pose = render::DefaultPvpStopwatchPlacement(viewport_px);
  return HudRect{.x = pose.corner_px.x - kPvpTimerReferenceWidth / 2.0F,
                 .y = pose.corner_px.y,
                 .width = kPvpTimerReferenceWidth,
                 .height = kPvpTimerReferenceHeight};
}

render::PvpHudPlacement PvpHudPlacementOf(PvpHudPart part, const PvpHudLook& look,
                                          glm::vec2 viewport_px) noexcept {
  if (PvpHudRectUnset(look.parts[Rank(part)])) {
    return part == PvpHudPart::kRank ? render::DefaultPvpRankPlacement(viewport_px)
                                     : render::DefaultPvpStopwatchPlacement(viewport_px);
  }
  // ⚠ LA PLACE RETENUE EST ANCRÉE : le rendu veut l'écran.
  const HudRect rect = HudOnScreen(look.parts[Rank(part)], ImVec2(viewport_px.x, viewport_px.y));
  const float ui_scale = PvpHudScaleOf(part, rect);
  if (part == PvpHudPart::kRank) {
    return render::PvpHudPlacement{
        .corner_px = glm::vec2{rect.x + rect.width, rect.y + rect.height},
        .scale = ui_scale,
    };
  }
  return render::PvpHudPlacement{
      .corner_px = glm::vec2{rect.x + rect.width / 2.0F, rect.y},
      .scale = ui_scale,
  };
}

bool PvpHudFrames(PvpHudLook& look, const AlignGrid& grid, HudGrip& grip, glm::vec2 viewport_px) {
  // 🔴 VERROUILLÉ, ON NE POSE RIEN DU TOUT. Un cadre verrouillé est déjà
  // transparent aux clics, mais l'ouvrir quand même coûterait deux fenêtres
  // ImGui par image pour rien — et surtout, le HUD PVP n'est visible qu'en zone
  // PVP, alors que ses cadres, eux, doivent se poser N'IMPORTE OÙ : c'est la
  // seule façon de composer son écran ailleurs qu'au milieu d'une mêlée.
  if (look.locked) {
    return false;
  }

  bool moved = false;
  // ⚠ TOUS MONTRÉS PENDANT LA POSE : la liste des frères sert à l'aimantation,
  // et un cadre qu'on ne pose pas n'y entre pas. Ici les deux se posent.
  std::array<bool, kPvpHudPartCount> shown{};
  shown.fill(true);

  for (std::size_t rank = 0; rank < kPvpHudPartCount; ++rank) {
    // ⚠ SEMÉ AVANT D'ÊTRE OUVERT : un cadre jamais posé n'a ni place ni taille,
    // et s'ouvrirait au coin haut-gauche — loin du dessin qu'il commande.
    if (PvpHudRectUnset(look.parts[rank])) {
      look.parts[rank] = HudAnchored(DefaultPvpHudRect(static_cast<PvpHudPart>(rank), viewport_px),
                                     ImVec2(viewport_px.x, viewport_px.y));
      moved = true;
    }

    HudFrameLook frame;
    frame.locked = false;
    frame.border = look.border;
    frame.grid = &grid;
    frame.sticky = look.sticky;
    frame.center_mark = look.center_mark;
    // ⚠ AUCUN FOND : ce que le joueur place est le DESSIN de la couche de rendu,
    // qui se pose à même le monde. Un fond opaque lui cacherait précisément ce
    // qu'il est en train de régler.
    frame.background.reset();

    HudSiblings siblings;
    siblings.frames = look.parts;
    siblings.shown = shown;
    siblings.mine = static_cast<int>(rank);

    // ⚠ C'EST `look.parts[rank]` QUI EST PASSÉ, et non une copie : le cadre écrit
    // dedans, et dans celui de son frère en déplacement de bloc.
    static_cast<void>(
        BeginHudFrame(kPartIds[rank], look.parts[rank], frame, siblings, grip, &moved));
    EndHudFrame(grip);
  }
  return moved;
}

}  // namespace ro::ui
