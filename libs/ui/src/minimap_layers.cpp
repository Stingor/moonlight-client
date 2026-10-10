#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"

#include "minimap_parts.h"

namespace ro::ui {
namespace {

// Le canevas de secours d'une carte SANS image. Seul son rapport compte : il
// rend la zone de dessin carrée, comme l'aurait fait une vraie image — presque
// toutes celles du jeu font 512 sur 512, quelle que soit la forme de la carte.
constexpr float kFallbackCanvasPixels = 512.0F;

// Les tailles des marqueurs, en parts de la demi-étendue de la flèche, et leur
// plancher en pixels : une flèche réglée toute petite ne doit pas réduire les
// icônes à un point.
constexpr float kArrowLeastHalf = 2.0F;
constexpr float kTownShare = 0.9F;
constexpr float kTownLeast = 5.0F;
// La demi-taille de l'icône d'une échoppe désignée, avant l'échelle de
// l'interface : celle du client d'origine.
constexpr float kStoreSearchMarkerHalfPixels = 4.0F;
// La demi-taille d'une marque de quête, même règle, et le rang de chaque flèche
// dans `MinimapView::quest_arrows`.
constexpr float kQuestMarkerHalfPixels = 4.0F;
constexpr std::size_t kQuestArrowLeft = 0;
constexpr std::size_t kQuestArrowRight = 1;
constexpr std::size_t kQuestArrowUp = 2;
constexpr std::size_t kQuestArrowDown = 3;
constexpr float kMemoShare = 0.8F;
constexpr float kMemoLeast = 4.0F;
// La tombe d'un MVP : un losange de la taille d'une épingle, doré pour un
// instant mérité, orangé pour une fenêtre, cerné d'un trait sombre.
constexpr float kTombShare = 0.8F;
constexpr float kTombLeast = 4.0F;
constexpr ImU32 kTombExactColor = IM_COL32(255, 216, 96, 235);
constexpr ImU32 kTombWindowColor = IM_COL32(232, 140, 60, 205);
constexpr ImU32 kTombOutline = IM_COL32(20, 20, 20, 200);
constexpr float kTombOutlineWidth = 1.0F;
constexpr float kMateShare = 0.5F;
constexpr float kMateLeast = 2.0F;
// Le liseré blanc autour d'un carré de groupe ou d'une pastille de guilde.
constexpr float kMateHalo = 1.0F;
constexpr float kViewpointShare = 0.22F;
constexpr float kViewpointLeast = 1.0F;
// La croix d'un repère : une barre de huit sur deux et une de deux sur huit, en
// demi-étendues — d'où ce facteur sur un seul axe.
constexpr float kViewpointArm = 4.0F;
constexpr std::uint32_t kViewpointBlinkMs = 1000;
constexpr float kRouteShare = 0.5F;
constexpr float kRouteLeast = 3.0F;
constexpr float kRouteHaloWidth = 3.0F;
constexpr float kRouteLineWidth = 1.5F;
constexpr float kGoalOutlineWidth = 1.5F;
// L'ombre portée de la flèche : un demi-quad plus grand de (2, 3), et un centre
// descendu d'un pixel. C'est elle qui rend la flèche lisible sur une carte claire.
constexpr float kShadowGrowX = 2.0F;
constexpr float kShadowGrowY = 3.0F;
constexpr float kShadowDrop = 1.0F;
constexpr float kDotRing = 1.0F;
// L'angle de la flèche est ce demi-tour moins l'orientation de l'acteur.
constexpr float kArrowTurnDeg = 180.0F;
constexpr float kDegreesPerHalfTurn = 180.0F;
constexpr float kHalf = 0.5F;

constexpr ImU32 kMemoFallback = IM_COL32(255, 210, 60, 235);
constexpr ImU32 kGuildHalo = IM_COL32(255, 255, 255, 170);
constexpr ImU32 kPartyHalo = IM_COL32(255, 255, 255, 150);
// L'ambre du guidage, doublé d'un liséré sombre : une ligne claire seule
// disparaît sur les cartes de ville, qui le sont aussi.
constexpr ImU32 kRouteDark = IM_COL32(0x20, 0x18, 0x08, 0xB0);
constexpr ImU32 kRouteColor = IM_COL32(0xFF, 0xC8, 0x4A, 0xE0);
constexpr ImU32 kArrowShadow = IM_COL32(0x33, 0x33, 0x33, 0x77);
constexpr ImU32 kDotRingColor = IM_COL32(0, 0, 0, 200);
constexpr ImU32 kFrameEdge = IM_COL32(0, 0, 0, 160);
constexpr int kOpaque = 255;
constexpr int kPercent = 100;

[[nodiscard]] float AtLeast(float value, float floor) { return std::max(value, floor); }

[[nodiscard]] float Fraction(float value, MinimapSpan span) {
  return span.high > span.low ? (value - span.low) / (span.high - span.low) : kHalf;
}

// IMPOSE LE FILTRAGE de l'image de carte pour la suite de la liste de dessin.
//
// 🔴 POSÉ DANS LES DEUX CAS : le moteur de rendu lie un échantillonneur avant
// chaque lot, et l'ambiant est le linéaire — ne rien faire, c'est lisser, et la
// case « Lisser l'image » paraîtrait morte.
void ImposeSampler(ImDrawList* into, bool smooth) {
  const ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
  const ImDrawCallback wanted =
      smooth ? platform.DrawCallback_SetSamplerLinear : platform.DrawCallback_SetSamplerNearest;
  if (wanted != nullptr) {
    into->AddCallback(wanted, nullptr);
  }
}

// Rend l'échantillonneur ambiant à ce qui suit dans la fenêtre.
void RestoreSampler(ImDrawList* into) {
  const ImDrawCallback ambient = ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear;
  if (ambient != nullptr) {
    into->AddCallback(ambient, nullptr);
  }
}

// Les quatre coins d'un quad pivoté de `degrees`, rotation d'écran USUELLE —
// sens horaire, Y vers le bas — dans l'ordre haut-gauche, haut-droit,
// bas-droit, bas-gauche qu'attend `AddImageQuad`.
//
// 🔴 PAS L'AUTRE CONVENTION, celle qui échange sinus et cosinus : elle vaut une
// rotation de 90° − θ, c'est-à-dire un MIROIR dans l'espace des angles. La
// flèche ne pointerait juste qu'au nord-est et au sud-ouest.
[[nodiscard]] std::array<ImVec2, 4> RotatedQuad(ImVec2 center, float half_width, float half_height,
                                                float degrees) {
  const float radians = degrees * std::numbers::pi_v<float> / kDegreesPerHalfTurn;
  const float sine = std::sin(radians);
  const float cosine = std::cos(radians);
  const std::array<float, 4> across = {-half_width, half_width, half_width, -half_width};
  const std::array<float, 4> down = {-half_height, -half_height, half_height, half_height};
  std::array<ImVec2, 4> corners{};
  for (std::size_t rank = 0; rank < corners.size(); ++rank) {
    corners[rank] = ImVec2(center.x + across[rank] * cosine - down[rank] * sine,
                           center.y + across[rank] * sine + down[rank] * cosine);
  }
  return corners;
}

void DrawImageQuad(ImDrawList* into, const SkinImage& image, const std::array<ImVec2, 4>& corners,
                   ImU32 tint) {
  into->AddImageQuad(image.id, corners[0], corners[1], corners[2], corners[3], ImVec2(0.0F, 0.0F),
                     ImVec2(1.0F, 0.0F), ImVec2(1.0F, 1.0F), ImVec2(0.0F, 1.0F), tint);
}

// Un losange, forme qu'aucun autre marqueur n'emploie.
[[nodiscard]] std::array<ImVec2, 4> Diamond(ImVec2 center, float half) {
  return {ImVec2(center.x, center.y - half), ImVec2(center.x + half, center.y),
          ImVec2(center.x, center.y + half), ImVec2(center.x - half, center.y)};
}

// ── LE FOND ─────────────────────────────────────────────────────────────────

void DrawBackdrop(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view,
                  const MinimapSettings& settings) {
  if (view.map_image.usable()) {
    const int percent = std::clamp(settings.map_percent, 0, kPercent);
    const ImU32 tint = IM_COL32(kOpaque, kOpaque, kOpaque, percent * kOpaque / kPercent);
    into->AddImage(view.map_image.id, frame.low, frame.high, ImVec2(frame.u.low, frame.v.low),
                   ImVec2(frame.u.high, frame.v.high), tint);
    return;
  }
  // DIRE CE QUI MANQUE SANS RIEN CACHER : le fond est posé AVANT les marqueurs,
  // qui restent — c'est précisément sur les cartes sans image qu'on cherche ses
  // coéquipiers.
  std::string line;
  if (view.map_name.empty()) {
    line = core::Tr("Carte inconnue");
  } else {
    line = std::string(core::Tr("Pas d'image pour")) + "\n" + std::string(view.map_name);
  }
  const ImVec2 size = ImGui::CalcTextSize(line.c_str());
  into->AddText(ImVec2(frame.origin.x + (frame.side - size.x) * kHalf,
                       frame.origin.y + (frame.side - size.y) * kHalf),
                ImGui::GetColorU32(ImGuiCol_TextDisabled), line.c_str());
}

// ── LES COMMODITÉS ET LES PORTAILS : de vrais items, pour que le clic ne lance
// pas un glisser. Une couche par liste ; `id` sépare leurs boutons.

void DrawTown(ImDrawList* into, const MinimapFrame& frame, std::span<const MinimapTownIcon> icons,
              std::string_view id, float half, bool interactive, MinimapAnswer& answer) {
  const float icon = AtLeast(half * kTownShare, kTownLeast);
  const ImVec2 saved = ImGui::GetCursorScreenPos();
  ImGui::PushID(id.data(), id.data() + id.size());
  for (std::size_t rank = 0; rank < icons.size(); ++rank) {
    const MinimapTownIcon& town = icons[rank];
    ImVec2 at;
    if (!town.image.usable() || !CellOnMinimap(frame, town.cell.x, town.cell.y, &at)) {
      continue;
    }
    into->AddImage(town.image.id, ImVec2(at.x - icon, at.y - icon),
                   ImVec2(at.x + icon, at.y + icon));
    if (!interactive) {
      continue;
    }
    // 🔴 UN ITEM IMGUI PAR ICÔNE, pas un test de survol : le corps de la
    // fenêtre se glisse, et un simple test laisserait le clic démarrer un
    // déplacement en même temps qu'il lance le guidage.
    ImGui::SetCursorScreenPos(ImVec2(at.x - icon, at.y - icon));
    ImGui::PushID(static_cast<int>(rank));
    static_cast<void>(ImGui::InvisibleButton("##mm_town", ImVec2(icon * 2.0F, icon * 2.0F)));
    if (ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (!town.label.empty()) {
        SkinTooltipFormat("%.*s", static_cast<int>(town.label.size()), town.label.data());
      }
    }
    if (ImGui::IsItemClicked()) {
      answer.guide_to = town.cell;
    }
    ImGui::PopID();
  }
  ImGui::PopID();
  ImGui::SetCursorScreenPos(saved);
}

void DrawMemos(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view, float half,
               bool interactive) {
  const float pin = AtLeast(half * kMemoShare, kMemoLeast);
  const ImVec2 saved = ImGui::GetCursorScreenPos();
  for (std::size_t rank = 0; rank < view.memos.size(); ++rank) {
    const MinimapMemo& memo = view.memos[rank];
    ImVec2 at;
    if (!CellOnMinimap(frame, memo.cell.x, memo.cell.y, &at)) {
      continue;
    }
    if (view.memo_point.usable()) {
      into->AddImage(view.memo_point.id, ImVec2(at.x - pin, at.y - pin),
                     ImVec2(at.x + pin, at.y + pin));
    } else {
      const std::array<ImVec2, 4> shape = Diamond(at, pin);
      into->AddConvexPolyFilled(shape.data(), static_cast<int>(shape.size()), kMemoFallback);
    }
    if (!interactive) {
      continue;
    }
    ImGui::SetCursorScreenPos(ImVec2(at.x - pin, at.y - pin));
    ImGui::PushID(static_cast<int>(rank));
    static_cast<void>(ImGui::InvisibleButton("##mm_memo_pt", ImVec2(pin * 2.0F, pin * 2.0F)));
    if (ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      const std::string name = memo.name.empty() ? core::Tr("(sans nom)") : std::string(memo.name);
      SkinTooltipFormat("%s\n%d, %d", name.c_str(), memo.cell.x, memo.cell.y);
    }
    ImGui::PopID();
  }
  ImGui::SetCursorScreenPos(saved);
}

// Les deux formes du jeu : pastille cernée pour la guilde, carré bordé pour le
// groupe. La couleur vient du serveur.
void DrawMates(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view,
               const MinimapSettings& settings, float half) {
  const float mate = AtLeast(half * kMateShare, kMateLeast);
  if (settings.show_guild) {
    for (const MinimapMate& member : view.guild) {
      ImVec2 at;
      if (CellOnMinimap(frame, member.cell.x, member.cell.y, &at)) {
        into->AddCircleFilled(at, mate + kMateHalo, kGuildHalo);
        into->AddCircleFilled(at, mate, member.color);
      }
    }
  }
  if (settings.show_party) {
    for (const MinimapMate& member : view.party) {
      ImVec2 at;
      if (CellOnMinimap(frame, member.cell.x, member.cell.y, &at)) {
        const float outer = mate + kMateHalo;
        into->AddRectFilled(ImVec2(at.x - outer, at.y - outer), ImVec2(at.x + outer, at.y + outer),
                            kPartyHalo);
        into->AddRectFilled(ImVec2(at.x - mate, at.y - mate), ImVec2(at.x + mate, at.y + mate),
                            member.color);
      }
    }
  }
}

void DrawViewpoints(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view,
                    float half) {
  const float bar = AtLeast(half * kViewpointShare, kViewpointLeast);
  for (const MinimapViewpoint& point : view.viewpoints) {
    ImVec2 at;
    if (point.age_ms % kViewpointBlinkMs >= kViewpointBlinkMs / 2 ||
        !CellOnMinimap(frame, point.cell.x, point.cell.y, &at)) {
      continue;
    }
    into->AddRectFilled(ImVec2(at.x - bar * kViewpointArm, at.y - bar),
                        ImVec2(at.x + bar * kViewpointArm, at.y + bar), point.color);
    into->AddRectFilled(ImVec2(at.x - bar, at.y - bar * kViewpointArm),
                        ImVec2(at.x + bar, at.y + bar * kViewpointArm), point.color);
  }
}

// Un point ramené dans le cadre, marge comprise : hors du cadrage, il garde sa
// DIRECTION au lieu de disparaître.
[[nodiscard]] ImVec2 ClampToFrame(const MinimapFrame& frame, ImVec2 point, float margin) {
  return {std::clamp(point.x, frame.low.x + margin,
                     std::max(frame.low.x + margin, frame.high.x - margin)),
          std::clamp(point.y, frame.low.y + margin,
                     std::max(frame.low.y + margin, frame.high.y - margin))};
}

// 🔴 COLLÉ AU BORD, PAS OMIS : c'est tout l'intérêt de l'objet qui le révèle.
void DrawBoss(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view, float half) {
  if (!view.boss.has_value() || !view.boss_icon.usable()) {
    return;
  }
  const float icon = AtLeast(half * kTownShare, kTownLeast);
  const ImVec2 at = ClampToFrame(frame,
                                 CellOnMinimapUnbounded(frame, static_cast<float>(view.boss->x),
                                                        static_cast<float>(view.boss->y)),
                                 icon);
  into->AddImage(view.boss_icon.id, ImVec2(at.x - icon, at.y - icon),
                 ImVec2(at.x + icon, at.y + icon));
}

// LES MARQUES DE QUÊTE : l'icône de sa couleur, centrée sur la case du PNJ.
//
// 🔴 COLLÉE AU BORD, PAS OMISE, et l'icône cède alors la place à la flèche de
// sa direction : la couleur se perd, la direction reste. La gauche et la droite
// passent avant le haut et le bas.
void DrawQuests(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view,
                bool interactive) {
  const float scale = ImGui::GetStyle().FontScaleMain;
  for (const MinimapQuest& quest : view.quests) {
    // L'ICÔNE DE LA SORTE DE QUÊTE à sa taille, sinon la pastille.
    const bool kind = quest.kind_image.usable();
    const float icon =
        kind ? quest.kind_image.width * 0.5F * scale : kQuestMarkerHalfPixels * scale;
    const ImVec2 wanted = CellOnMinimapUnbounded(frame, static_cast<float>(quest.cell.x),
                                                 static_cast<float>(quest.cell.y));
    const ImVec2 at = ClampToFrame(frame, wanted, icon);
    const SkinImage* image = kind ? &quest.kind_image : &quest.image;
    if (wanted.x < at.x) {
      image = &view.quest_arrows[kQuestArrowLeft];
    } else if (wanted.x > at.x) {
      image = &view.quest_arrows[kQuestArrowRight];
    } else if (wanted.y < at.y) {
      image = &view.quest_arrows[kQuestArrowUp];
    } else if (wanted.y > at.y) {
      image = &view.quest_arrows[kQuestArrowDown];
    }
    if (image->usable()) {
      into->AddImage(image->id, ImVec2(at.x - icon, at.y - icon), ImVec2(at.x + icon, at.y + icon));
    }
    // Survolée sur la carte, elle se nomme, comme le bouton de la grande carte.
    if (interactive && wanted.x == at.x && wanted.y == at.y && ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(ImVec2(at.x - icon, at.y - icon),
                                   ImVec2(at.x + icon, at.y + icon))) {
      SkinTooltipFormat("%s", core::Tr("PNJ de quête"));
    }
  }
}

// LES TOMBES DES MVP CONNUS DU GROUPE : une couche de plus, sans réglage ici —
// `app` a déjà filtré. ⚠ HORS DU CADRAGE, UNE TOMBE EST OMISE, et non collée au
// bord comme le boss : elle informe d'un lieu, pas d'une direction. La couleur
// dit la précision : dorée pour un instant mérité, orangée pour une fenêtre.
void DrawTombs(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view, float half) {
  const float size = AtLeast(half * kTombShare, kTombLeast);
  for (const MinimapTomb& tomb : view.tombs) {
    ImVec2 at;
    if (!CellOnMinimap(frame, tomb.cell.x, tomb.cell.y, &at)) {
      continue;
    }
    const std::array<ImVec2, 4> shape = Diamond(at, size);
    into->AddConvexPolyFilled(shape.data(), static_cast<int>(shape.size()),
                              tomb.exact ? kTombExactColor : kTombWindowColor);
    into->AddPolyline(shape.data(), static_cast<int>(shape.size()), kTombOutline,
                      ImDrawFlags_Closed, kTombOutlineWidth);
  }
}

// Interactive, chaque tombe est un item, comme une commodité : son infobulle
// dit le MVP, sa fenêtre et l'âge du signalement ; le clic ouvre le carnet sur
// sa ligne, le Ctrl+clic guide vers elle. Sans créneau, seul le guidage reste.
//
// 🔴 SOUMISES AVANT TOUTE AUTRE ICÔNE, dessinées par-dessus : de deux items qui
// se chevauchent, le PREMIER soumis garde le survol, et c'est la tombe, peinte
// au-dessus, que la souris désigne.
void SubmitTombs(const MinimapFrame& frame, const MinimapView& view, float half,
                 MinimapAnswer& answer) {
  const float size = AtLeast(half * kTombShare, kTombLeast);
  const ImVec2 saved = ImGui::GetCursorScreenPos();
  for (std::size_t rank = 0; rank < view.tombs.size(); ++rank) {
    const MinimapTomb& tomb = view.tombs[rank];
    ImVec2 at;
    if (!CellOnMinimap(frame, tomb.cell.x, tomb.cell.y, &at)) {
      continue;
    }
    ImGui::SetCursorScreenPos(ImVec2(at.x - size, at.y - size));
    ImGui::PushID(static_cast<int>(rank));
    static_cast<void>(ImGui::InvisibleButton("##mm_tomb", ImVec2(size * 2.0F, size * 2.0F)));
    if (ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      // Une ligne vide se saute : une tombe sans fenêtre connue n'a pas de
      // ligne blanche au milieu de son infobulle.
      std::string tip;
      for (const std::string_view line : {tomb.label, tomb.window_line, tomb.age_line}) {
        if (!line.empty()) {
          tip.append(line).push_back('\n');
        }
      }
      tip += tomb.slot_id != kMinimapNoTombSlot
                 ? core::Tr("Clic : ouvrir le carnet · Ctrl+clic : y guider")
                 : core::Tr("Ctrl+clic : y guider");
      SkinTooltipFormat("%s", tip.c_str());
    }
    if (ImGui::IsItemClicked()) {
      if (ImGui::GetIO().KeyCtrl) {
        answer.guide_to = tomb.cell;
      } else if (tomb.slot_id != kMinimapNoTombSlot) {
        answer.open_mvp_slot = tomb.slot_id;
      }
    }
    ImGui::PopID();
  }
  ImGui::SetCursorScreenPos(saved);
}

// L'ÉCHOPPE DÉSIGNÉE DANS LA RECHERCHE : collée au bord comme le boss, à sa
// taille à elle, et sans réglage qui l'éteigne — le client d'origine la montre
// toujours.
void DrawStoreSearch(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view) {
  if (!view.store_search.has_value() || !view.store_search_icon.usable()) {
    return;
  }
  const float icon = kStoreSearchMarkerHalfPixels * ImGui::GetStyle().FontScaleMain;
  const ImVec2 at =
      ClampToFrame(frame,
                   CellOnMinimapUnbounded(frame, static_cast<float>(view.store_search->x),
                                          static_cast<float>(view.store_search->y)),
                   icon);
  into->AddImage(view.store_search_icon.id, ImVec2(at.x - icon, at.y - icon),
                 ImVec2(at.x + icon, at.y + icon));
}

void DrawGoal(ImDrawList* into, ImVec2 center, float half) {
  const std::array<ImVec2, 4> shape = Diamond(center, half);
  into->AddConvexPolyFilled(shape.data(), static_cast<int>(shape.size()), kRouteColor);
  into->AddPolyline(shape.data(), static_cast<int>(shape.size()), kRouteDark, ImDrawFlags_Closed,
                    kGoalOutlineWidth);
}

// L'ITINÉRAIRE, après les marqueurs et avant le joueur : la ligne passe sur les
// icônes pour rester lisible, et rien ne recouvre la flèche.
void DrawRoute(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view, float half,
               ImVec2 player) {
  const float goal = AtLeast(half * kRouteShare, kRouteLeast);
  if (view.route_path.size() >= 2) {
    std::vector<ImVec2> points;
    points.reserve(view.route_path.size());
    for (const MinimapCell& cell : view.route_path) {
      points.push_back(
          CellOnMinimapUnbounded(frame, static_cast<float>(cell.x), static_cast<float>(cell.y)));
    }
    // La ligne sort du cadre et c'est la découpe qui la taille.
    into->PushClipRect(frame.low, frame.high, true);
    into->AddPolyline(points.data(), static_cast<int>(points.size()), kRouteDark, ImDrawFlags_None,
                      kRouteHaloWidth);
    into->AddPolyline(points.data(), static_cast<int>(points.size()), kRouteColor, ImDrawFlags_None,
                      kRouteLineWidth);
    into->PopClipRect();
    DrawGoal(into, ClampToFrame(frame, points.back(), goal), goal);
    return;
  }
  if (!view.route_exit.has_value()) {
    return;
  }
  // Repli : pas encore de chemin en cellules, mais une sortie connue — une
  // direction vaut mieux que rien.
  const ImVec2 exit =
      ClampToFrame(frame,
                   CellOnMinimapUnbounded(frame, static_cast<float>(view.route_exit->x),
                                          static_cast<float>(view.route_exit->y)),
                   goal);
  into->AddLine(player, exit, kRouteDark, kRouteHaloWidth);
  into->AddLine(player, exit, kRouteColor, kRouteLineWidth);
  DrawGoal(into, exit, goal);
}

// LE JOUEUR, EN DERNIER : rien ne doit le recouvrir.
void DrawPlayer(ImDrawList* into, const MinimapView& view, const MinimapSettings& settings,
                float half, ImVec2 at) {
  if (view.arrow.usable()) {
    const float degrees = kArrowTurnDeg - view.player_facing_deg;
    DrawImageQuad(into, view.arrow,
                  RotatedQuad(ImVec2(at.x, at.y + kShadowDrop), half + kShadowGrowX,
                              half + kShadowGrowY, degrees),
                  kArrowShadow);
    DrawImageQuad(into, view.arrow, RotatedQuad(at, half, half, degrees), settings.arrow_tint);
    return;
  }
  // Un point cerné de sombre, lisible sur fond clair comme sur fond foncé.
  into->AddCircleFilled(at, half + kDotRing, kDotRingColor);
  into->AddCircleFilled(at, half, settings.arrow_tint);
}

}  // namespace

MinimapFrame FrameMinimap(const MinimapView& view, float zoom, ImVec2 origin, float side) {
  MinimapFrame frame;
  frame.origin = origin;
  frame.side = side;
  frame.located = view.cells_width > 0 && view.cells_height > 0;
  frame.cells_width = view.cells_width;
  frame.cells_height = view.cells_height;
  if (frame.located) {
    const auto width = static_cast<float>(view.cells_width);
    const auto height = static_cast<float>(view.cells_height);
    frame.player_u = std::clamp(view.player_cell_x / width, 0.0F, 1.0F);
    frame.player_v = std::clamp((height - view.player_cell_y) / height, 0.0F, 1.0F);
  }
  // Une portée de 1/zoom centrée sur le joueur ; sans position, tout.
  const float span =
      frame.located ? 1.0F / std::clamp(zoom, kMinimapZoomLeast, kMinimapZoomMost) : 1.0F;
  frame.u = CenteredMinimapSpan(frame.player_u, span);
  frame.v = CenteredMinimapSpan(frame.player_v, span);

  // Le rapport de la PORTION VISIBLE : il ne diffère de celui de l'image que
  // lorsqu'une seule des deux portées bute sur un bord.
  const bool pictured = view.map_image.usable();
  const float canvas_width = pictured ? view.map_image.width : kFallbackCanvasPixels;
  const float canvas_height = pictured ? view.map_image.height : kFallbackCanvasPixels;
  const float shown_width = canvas_width * (frame.u.high - frame.u.low);
  const float shown_height = canvas_height * (frame.v.high - frame.v.low);
  const float scale = shown_width >= shown_height ? side / shown_width : side / shown_height;
  frame.draw_width = shown_width * scale;
  frame.draw_height = shown_height * scale;
  frame.low = ImVec2(origin.x + (side - frame.draw_width) * kHalf,
                     origin.y + (side - frame.draw_height) * kHalf);
  frame.high = ImVec2(frame.low.x + frame.draw_width, frame.low.y + frame.draw_height);
  return frame;
}

bool CellOnMinimap(const MinimapFrame& frame, int cell_x, int cell_y, ImVec2* at) {
  if (!frame.located) {
    return false;
  }
  const float along = static_cast<float>(cell_x) / static_cast<float>(frame.cells_width);
  const float down = (static_cast<float>(frame.cells_height) - static_cast<float>(cell_y)) /
                     static_cast<float>(frame.cells_height);
  if (along < frame.u.low || along > frame.u.high || down < frame.v.low || down > frame.v.high) {
    return false;
  }
  *at = CellOnMinimapUnbounded(frame, static_cast<float>(cell_x), static_cast<float>(cell_y));
  return true;
}

ImVec2 CellOnMinimapUnbounded(const MinimapFrame& frame, float cell_x, float cell_y) {
  if (!frame.located) {
    return ImVec2(frame.low.x + frame.draw_width * kHalf, frame.low.y + frame.draw_height * kHalf);
  }
  const float along = cell_x / static_cast<float>(frame.cells_width);
  const float down =
      (static_cast<float>(frame.cells_height) - cell_y) / static_cast<float>(frame.cells_height);
  return {frame.low.x + Fraction(along, frame.u) * frame.draw_width,
          frame.low.y + Fraction(down, frame.v) * frame.draw_height};
}

MinimapCell MinimapCellAt(const MinimapFrame& frame, ImVec2 point) {
  const float across = frame.draw_width > 0.0F ? (point.x - frame.low.x) / frame.draw_width : 0.0F;
  const float down = frame.draw_height > 0.0F ? (point.y - frame.low.y) / frame.draw_height : 0.0F;
  const float along = frame.u.low + across * (frame.u.high - frame.u.low);
  const float from_top = frame.v.low + down * (frame.v.high - frame.v.low);
  const auto width = static_cast<float>(frame.cells_width);
  const auto height = static_cast<float>(frame.cells_height);
  return {static_cast<int>(along * width), static_cast<int>(height - from_top * height)};
}

void DrawMinimapLayers(const MinimapFrame& frame, const MinimapView& view,
                       const MinimapSettings& settings, bool interactive, MinimapAnswer& answer) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  // Posé avant l'image ET gardé pour les marqueurs : eux aussi sont des images.
  ImposeSampler(into, settings.smooth);
  DrawBackdrop(into, frame, view, settings);
  // 🔴 LES MARQUEURS NE DÉPENDENT PAS DE L'IMAGE : une carte sans image garde
  // ses coéquipiers, ses commodités et son joueur.
  if (frame.located) {
    const float half = AtLeast(static_cast<float>(settings.arrow_half_px), kArrowLeastHalf);
    if (interactive) {
      SubmitTombs(frame, view, half, answer);
    }
    if (settings.show_town) {
      DrawTown(into, frame, view.town, "##mm_town", half, interactive, answer);
    }
    if (settings.show_portals) {
      DrawTown(into, frame, view.portals, "##mm_portals", half, interactive, answer);
    }
    DrawMemos(into, frame, view, half, interactive);
    if (settings.show_quests) {
      DrawQuests(into, frame, view, interactive);
    }
    DrawMates(into, frame, view, settings, half);
    DrawBattleMates(into, frame, view, half, interactive);
    if (settings.show_viewpoints) {
      DrawViewpoints(into, frame, view, half);
    }
    if (settings.show_boss) {
      DrawBoss(into, frame, view, half);
    }
    DrawTombs(into, frame, view, half);
    DrawStoreSearch(into, frame, view);
    const ImVec2 player(frame.low.x + Fraction(frame.player_u, frame.u) * frame.draw_width,
                        frame.low.y + Fraction(frame.player_v, frame.v) * frame.draw_height);
    if (settings.show_route) {
      DrawRoute(into, frame, view, half, player);
    }
    DrawPlayer(into, view, settings, half, player);
  }
  RestoreSampler(into);
  into->AddRect(frame.low, frame.high, kFrameEdge);
}

}  // namespace ro::ui
