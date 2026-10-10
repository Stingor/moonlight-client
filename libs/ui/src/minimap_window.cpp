#include "ro/ui/minimap_window.h"

#include <algorithm>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/window_bounds.h"

#include "minimap_parts.h"

namespace ro::ui {
namespace {

// Le temps de survol, en secondes, avant le rappel « Maj » : calé sur le délai
// des infobulles, pour qu'un curseur qui ne fait que traverser la carte n'en
// déclenche aucune.
constexpr float kHintHoverSeconds = 0.6F;

// Les poignées de redimensionnement, sombres pour se voir sur une carte claire.
// Une poignée invisible n'existe pas pour le joueur.
constexpr ImU32 kGripColor = IM_COL32(30, 34, 44, 130);
constexpr ImU32 kGripHoveredColor = IM_COL32(30, 34, 44, 200);
constexpr ImU32 kGripActiveColor = IM_COL32(30, 34, 44, 255);
constexpr int kGripColorCount = 3;

// Le plafond des tours qui accordent la hauteur du nom replié au côté de la
// carte : un garde-fou, l'accord vient bien avant.
constexpr int kCaptionFitPasses = 4;

// Le bouton de réglages, dans le coin HAUT-gauche : les deux coins du bas
// portent les poignées d'ImGui, et un bouton posé là les couvrirait.
constexpr float kButtonInsetPixels = 2.0F;
// Faute d'art : un petit carré sombre à trois traits, pour que les réglages
// restent atteignables.
constexpr float kFallbackButtonPixels = 12.0F;
constexpr float kFallbackRounding = 2.0F;
constexpr float kFallbackStrokeInset = 3.0F;
constexpr float kFallbackStrokeWidth = 1.5F;
constexpr int kFallbackStrokes = 3;
constexpr ImU32 kFallbackButtonColor = IM_COL32(30, 34, 44, 200);
constexpr ImU32 kFallbackButtonHovered = IM_COL32(60, 70, 96, 230);
constexpr ImU32 kFallbackStrokeColor = IM_COL32(240, 240, 240, 255);
constexpr std::size_t kNormalFace = 0;
constexpr std::size_t kHoveredFace = 1;
constexpr std::size_t kPressedFace = 2;

constexpr float kMemoFieldPixels = 180.0F;
// Le sous-menu des réglages est contraint en largeur : curseurs et nuancier
// l'étireraient sur la moitié de l'écran.
constexpr float kSettingsMenuLeastWidth = 300.0F;
constexpr float kSettingsMenuMostWidth = 440.0F;
constexpr float kSettingsMenuScreenShare = 0.8F;
constexpr float kPercent = 100.0F;

constexpr const char* kMemoPopupId = "##mm_memo_popup";
constexpr const char* kSettingsPopupId = "##mm_cfg_popup";

// LES COULEURS D'UN CORPS DE FENÊTRE DU JEU, pour les deux menus.
//
// 🔴 SANS ELLES, un bouton habillé peint son libellé avec le texte ambiant —
// clair — sur son art clair : illisible. Posées AVANT `BeginPopup` et retirées
// qu'il s'ouvre ou non, sinon la pile de styles se déséquilibre les images où
// il est fermé.
constexpr int kPopupColorCount = 5;
void PushPopupColors() {
  ImGui::PushStyleColor(ImGuiCol_PopupBg, kSkinBodyColor);
  ImGui::PushStyleColor(ImGuiCol_Border, kSkinBorderColor);
  ImGui::PushStyleColor(ImGuiCol_Text, kSkinTextColor);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, kSkinFaintTextColor);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, kSkinInputColor);
}
void PopPopupColors() { ImGui::PopStyleColor(kPopupColorCount); }

// La contrainte de taille : la zone de carte reste carrée.
void KeepSquare(ImGuiSizeCallbackData* data) {
  const auto* fit = static_cast<const MinimapSquareFit*>(data->UserData);
  data->DesiredSize = FitMinimapSquare(data->DesiredSize, *fit);
}

// LE COIN OÙ LA FENÊTRE SE MONTRE : celui que le joueur a glissé, ou, tant qu'il
// ne l'a pas fait, le coin haut-droit de l'écran du moment — ramené, dans les
// deux cas, pour que la fenêtre de cette taille tienne entière dans l'écran.
[[nodiscard]] ImVec2 CornerOf(const MinimapSettings& settings, ImVec2 size) {
  ImVec2 wanted(static_cast<float>(settings.pos_x), static_cast<float>(settings.pos_y));
  if (settings.pos_x == kMinimapUnplaced && settings.pos_y == kMinimapUnplaced) {
    const float margin = kMinimapScreenMargin * ImGui::GetStyle().FontScaleMain;
    wanted = ImVec2(ImGui::GetIO().DisplaySize.x - margin - size.x, margin);
  }
  return ClampWindowPosToScreen(wanted, size);
}

// LE JOUEUR TIENT LA FENÊTRE : il la glisse, tire une poignée, ou presse un de
// ses boutons. Seul ce geste écrit la position et le côté dans les réglages ;
// hors de lui, la fenêtre reçoit à chaque image la place qu'ils donnent sur
// l'écran du moment.
[[nodiscard]] bool HeldByPlayer(const ImGuiWindow* window) {
  const ImGuiContext& context = *ImGui::GetCurrentContext();
  return window != nullptr && context.ActiveId != 0 && context.ActiveIdWindow == window;
}

// Maj au-dessus de la carte l'engage, et un geste en cours la retient.
//
// 🔴 LE SURVOL SE TRANCHE SUR LE RECTANGLE DES RÉGLAGES, pas par ImGui : en
// mode traverser les clics la fenêtre ne reçoit rien, et ImGui ne teste même
// pas son rectangle.
void Engage(MinimapWindowState& state, const MinimapSettings& settings, ImVec2 corner,
            ImVec2 size) {
  const ImGuiIO& io = ImGui::GetIO();
  const float left = corner.x;
  const float top = corner.y;
  const bool over = io.MousePos.x >= left && io.MousePos.x < left + size.x &&
                    io.MousePos.y >= top && io.MousePos.y < top + size.y;
  if (io.KeyShift && over) {
    state.shift_engaged = true;
  } else if (!ImGui::IsAnyItemActive()) {
    // Relâcher Maj au milieu d'un glissement couperait le geste net.
    state.shift_engaged = false;
  }
  // LE RAPPEL DU GESTE, après un temps de survol : rien d'autre à l'écran ne
  // dit que la carte redevient saisissable. Sans Maj la souris traverse, donc
  // la bulle ne recouvre que ce qu'on regardait déjà.
  const bool engaged = state.shift_engaged || state.menu_open;
  if (settings.click_through && over && !engaged) {
    state.hint_hover_seconds += io.DeltaTime;
    if (state.hint_hover_seconds >= kHintHoverSeconds) {
      SkinTooltipFormat("%s",
                        core::Tr("Maintenir Maj pour saisir la minimap\nSans Maj la souris la "
                                 "traverse : ni déplacement, ni poignées, ni molette, ni "
                                 "bouton de réglages."));
    }
  } else {
    state.hint_hover_seconds = 0.0F;
  }
}

void DrawSettingsButton(const MinimapView& view, ImVec2 origin) {
  const SkinImage& normal = view.settings_button[kNormalFace];
  const ImVec2 at(origin.x + kButtonInsetPixels, origin.y + kButtonInsetPixels);
  const ImVec2 size = normal.usable() ? ImVec2(normal.width, normal.height)
                                      : ImVec2(kFallbackButtonPixels, kFallbackButtonPixels);
  const ImVec2 saved = ImGui::GetCursorScreenPos();
  ImGui::SetCursorScreenPos(at);
  static_cast<void>(ImGui::InvisibleButton("##mm_cfg", size));
  const bool hovered = ImGui::IsItemHovered();
  const bool pressed = ImGui::IsItemActive();
  if (ImGui::IsItemClicked()) {
    ImGui::OpenPopup(kSettingsPopupId);
  }
  if (hovered) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    SkinTooltipFormat("%s", core::Tr("Réglages de la minimap"));
  }
  ImGui::SetCursorScreenPos(saved);

  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 far(at.x + size.x, at.y + size.y);
  if (normal.usable()) {
    // Une face manquante retombe sur la normale plutôt que sur un trou.
    const SkinImage& wanted = view.settings_button[pressed   ? kPressedFace
                                                   : hovered ? kHoveredFace
                                                             : kNormalFace];
    into->AddImage(wanted.usable() ? wanted.id : normal.id, at, far);
    return;
  }
  into->AddRectFilled(at, far, hovered ? kFallbackButtonHovered : kFallbackButtonColor,
                      kFallbackRounding);
  const float gap =
      (size.y - 2.0F * kFallbackStrokeInset) / static_cast<float>(kFallbackStrokes - 1);
  for (int stroke = 0; stroke < kFallbackStrokes; ++stroke) {
    const float y = at.y + kFallbackStrokeInset + gap * static_cast<float>(stroke);
    into->AddLine(ImVec2(at.x + kFallbackStrokeInset, y), ImVec2(far.x - kFallbackStrokeInset, y),
                  kFallbackStrokeColor, kFallbackStrokeWidth);
  }
}

// Le menu du clic droit : poser un marqueur, et la liste de ceux de la carte.
bool DrawMemoPopup(MinimapWindowState& state, const MinimapView& view, const SkinArt& art,
                   MinimapAnswer& answer) {
  PushPopupColors();
  const bool open = BeginSkinPopup(kMemoPopupId);
  if (open) {
    ImGui::Text(core::Tr("Marqueur en %d, %d"), state.memo_cell.x, state.memo_cell.y);
    const bool full = view.memos.size() >= static_cast<std::size_t>(kMinimapMemoCap);
    const bool placeable = !full && !view.map_name.empty();
    ImGui::SetNextItemWidth(kMemoFieldPixels);
    // Poser un marqueur est un geste rapide : le champ prend le clavier dès
    // l'ouverture, et Entrée valide.
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }
    const bool entered =
        ImGui::InputText("##mm_memo_name", state.memo_name.data(), state.memo_name.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::BeginDisabled(!placeable);
    const bool added = SkinButton(core::Tr("Ajouter"), art);
    ImGui::EndDisabled();
    if ((added || entered) && placeable && state.memo_name[0] != '\0') {
      answer.add_memo = MinimapMemoAdd{.cell = state.memo_cell, .name = state.memo_name.data()};
      state.memo_name.fill('\0');
      ImGui::CloseCurrentPopup();
    }
    if (full) {
      ImGui::TextDisabled("%s", core::Tr("Maximum atteint pour cette carte."));
    }
    if (state.staff_warp && !view.map_name.empty()) {
      ImGui::Separator();
      if (SkinButton(core::Tr("Téléporter ici"), art)) {
        answer.warp_to = state.memo_cell;
        ImGui::CloseCurrentPopup();
      }
      SkinTooltip(core::Tr("Commande du staff : @warp map,x,y."));
    }
    if (!view.memos.empty()) {
      ImGui::Separator();
      for (std::size_t rank = 0; rank < view.memos.size(); ++rank) {
        const MinimapMemo& memo = view.memos[rank];
        ImGui::PushID(static_cast<int>(rank));
        if (SkinSmallButton("X", art)) {
          answer.remove_memo = rank;
        }
        ImGui::SameLine();
        ImGui::Text("%d, %d  %.*s", memo.cell.x, memo.cell.y, static_cast<int>(memo.name.size()),
                    memo.name.data());
        ImGui::PopID();
      }
    }
    EndSkinPopup();
  }
  PopPopupColors();
  return open;
}

// Le menu du bouton : les deux autres écrans de carte, puis les réglages.
bool DrawSettingsPopup(MinimapSettings& settings, MinimapWindowState& state, const SkinArt& art,
                       MinimapAnswer& answer) {
  PushPopupColors();
  const bool open = BeginSkinPopup(kSettingsPopupId);
  if (open) {
    if (ImGui::MenuItem(core::Tr("Carte du monde"))) {
      answer.open_world_map = true;
    }
    if (ImGui::MenuItem(core::Tr("Navigation"))) {
      answer.open_navigation = true;
    }
    ImGui::Separator();
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(kSettingsMenuLeastWidth, 0.0F),
        ImVec2(kSettingsMenuMostWidth, ImGui::GetIO().DisplaySize.y * kSettingsMenuScreenShare));
    if (ImGui::BeginMenu(core::Tr("Réglages"))) {
      if (DrawMinimapSettings(settings, art)) {
        state.save_pending = true;
      }
      ImGui::EndMenu();
    }
    EndSkinPopup();
  }
  PopPopupColors();
  return open;
}

// « carte,x,y », la forme sous laquelle un joueur lit et annonce une position,
// précédée du nom lisible replié sur la largeur de la carte.
void DrawCaption(const MinimapSettings& settings, const MinimapView& view, bool named, bool located,
                 float side) {
  if (named) {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + side);
    ImGui::TextUnformatted(view.display_name.data(),
                           view.display_name.data() + view.display_name.size());
    ImGui::PopTextWrapPos();
  }
  if (!settings.show_coords) {
    return;
  }
  const int map_bytes = static_cast<int>(view.map_name.size());
  if (!view.map_name.empty() && located) {
    ImGui::Text("%.*s,%d,%d", map_bytes, view.map_name.data(), static_cast<int>(view.player_cell_x),
                static_cast<int>(view.player_cell_y));
  } else if (!view.map_name.empty()) {
    ImGui::TextDisabled("%.*s", map_bytes, view.map_name.data());
  } else {
    ImGui::TextDisabled("-");
  }
}

}  // namespace

MinimapAnswer DrawMinimapWindow(MinimapSettings& settings, MinimapWindowState& state,
                                const MinimapView& view, const SkinArt& art) {
  MinimapAnswer answer;
  const ImGuiStyle& style = ImGui::GetStyle();
  const ImGuiIO& io = ImGui::GetIO();

  MinimapSquareFit fit{
      .pad_x = style.WindowPadding.x * 2.0F,
      .pad_y = style.WindowPadding.y * 2.0F,
      .extra_height = settings.show_coords ? ImGui::GetTextLineHeightWithSpacing() : 0.0F};
  // La hauteur du nom entre dans la contrainte AVANT l'ouverture : replié, il
  // peut prendre plusieurs lignes, et la fenêtre doit déjà pouvoir les porter.
  //
  // 🔴 REPLIÉ SUR LE CÔTÉ QUI SERA VRAIMENT DESSINÉ, pas sur celui du réglage :
  // un côté que la part d'écran rabote replie le nom sur plus de lignes, et une
  // hauteur comptée sur le côté du réglage en coupait la fin. Le côté dépend
  // lui-même de cette hauteur : on recompte jusqu'à ce que les deux s'accordent.
  // Chaque tour ne peut qu'ajouter des lignes et rétrécir le côté, et un nom
  // n'a que quelques mots, donc l'accord vient en peu de tours.
  const bool named = settings.show_map_name && !view.display_name.empty();
  // La FENÊTRE entière tient dans la part d'écran permise ; sur un écran trop
  // petit pour le minimum, c'est le minimum qui gagne.
  const float most_width = io.DisplaySize.x * kMinimapScreenShare - fit.pad_x;
  const auto most_side_for = [&](float extra_height) {
    const float most_height = io.DisplaySize.y * kMinimapScreenShare - fit.pad_y - extra_height;
    return std::max(std::min(most_width, most_height), kMinimapLeastSide);
  };
  const auto side_within = [&](float most_side) {
    return std::clamp(static_cast<float>(settings.side_px), kMinimapLeastSide, most_side);
  };
  if (named) {
    const std::string shown(view.display_name);
    float caption_height = 0.0F;
    for (int pass = 0; pass < kCaptionFitPasses; ++pass) {
      const float wrap = side_within(most_side_for(fit.extra_height + caption_height));
      const float counted =
          ImGui::CalcTextSize(shown.c_str(), nullptr, false, wrap).y + style.ItemSpacing.y;
      if (counted == caption_height) {
        break;
      }
      caption_height = counted;
    }
    fit.extra_height += caption_height;
  }
  fit.most_side = most_side_for(fit.extra_height);
  float side = side_within(fit.most_side);

  const ImVec2 window_size(side + fit.pad_x, side + fit.pad_y + fit.extra_height);
  const ImVec2 corner = CornerOf(settings, window_size);
  Engage(state, settings, corner, window_size);
  const bool interactive = !settings.click_through || state.shift_engaged || state.menu_open;

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                           ImGuiWindowFlags_NoTitleBar;
  if (!interactive) {
    flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize;
  }
  // ⚠ UN NOM STABLE : il porte l'identité de la fenêtre. Le faire varier avec
  // la carte lui ferait perdre sa place à chaque déplacement.
  const std::string name = std::string("###") + kMinimapWindowId;
  // 🔴 LA PLACE ET LA TAILLE S'IMPOSENT À CHAQUE IMAGE, sauf pendant le geste du
  // joueur. Imposées une seule fois, elles resteraient celles du premier écran
  // vu — inconnu, ou plus petit que celui qui suit —, et la fenêtre ne suivrait
  // plus ni son coin ni l'écran. Un réglage changé ailleurs — réinitialisé,
  // relu d'un fichier — s'impose de la même façon.
  const bool held = HeldByPlayer(ImGui::FindWindowByName(name.c_str()));
  if (!held) {
    ImGui::SetNextWindowPos(corner, ImGuiCond_Always);
    ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
  }
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(kMinimapLeastSide + fit.pad_x, kMinimapLeastSide + fit.pad_y + fit.extra_height),
      ImVec2(fit.most_side + fit.pad_x, fit.most_side + fit.pad_y + fit.extra_height), KeepSquare,
      &fit);
  ImGui::SetNextWindowBgAlpha(static_cast<float>(settings.backdrop_percent) / kPercent);
  ImGui::PushStyleColor(ImGuiCol_ResizeGrip, kGripColor);
  ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, kGripHoveredColor);
  ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, kGripActiveColor);

  if (ImGui::Begin(name.c_str(), nullptr, flags)) {
    // ⚠ COMPARÉ AU COIN OÙ ELLE SE MONTRE, pas au réglage : une minimap jamais
    // glissée ne s'écrit pas dans le fichier tant qu'elle reste à son coin, et
    // une place ramenée dans l'écran ne remplace pas celle du joueur.
    if (held) {
      const ImVec2 at = ImGui::GetWindowPos();
      if (static_cast<int>(at.x) != static_cast<int>(corner.x) ||
          static_cast<int>(at.y) != static_cast<int>(corner.y)) {
        settings.pos_x = static_cast<int>(at.x);
        settings.pos_y = static_cast<int>(at.y);
        state.save_pending = true;
      }
    }
    // 🔴 LE CÔTÉ VIENT DE LA FENÊTRE, pas du réglage : la poignée commande, et
    // la relire ici rend le redimensionnement persistant sans second chemin.
    // ⚠ ÉCRIT SOUS LE SEUL GESTE, et comparé au côté montré : un côté rabattu
    // par un petit écran ne remplace pas celui que le joueur a choisi.
    const float shown_side = side;
    const float available = ImGui::GetContentRegionAvail().x;
    if (available >= kMinimapLeastSide && available <= fit.most_side) {
      side = available;
    }
    if (held && static_cast<int>(side) != static_cast<int>(shown_side)) {
      settings.side_px = static_cast<int>(side);
      state.save_pending = true;
    }
    // Le carré est réservé d'abord : sans image, la fenêtre ne se rétracte pas
    // sous le curseur.
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(side, side));

    if (interactive) {
      const bool over_map =
          ImGui::IsWindowHovered() &&
          ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + side, origin.y + side));
      const float notches = WheelOver(state.wheel, ImGui::GetID("##mm_zone"), over_map);
      if (notches != 0.0F) {
        const float zoom = StepMinimapZoom(settings.zoom, notches);
        if (zoom != settings.zoom) {
          settings.zoom = zoom;
          state.save_pending = true;
        }
      }
    }

    const MinimapFrame frame = FrameMinimap(view, settings.zoom, origin, side);
    DrawMinimapLayers(frame, view, settings, interactive, answer);

    // Le clic DROIT ne déplace pas la fenêtre : un test de survol suffit ici.
    if (interactive && frame.located && ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(frame.low, frame.high) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      state.memo_cell = MinimapCellAt(frame, io.MousePos);
      state.memo_name.fill('\0');
      ImGui::OpenPopup(kMemoPopupId);
    }

    // Les menus se construisent sous `interactive`, pas sous l'engagement : un
    // menu ouvert doit survivre au relâchement de Maj, et c'est lui qui
    // prolonge l'engagement.
    bool menu_open_now = false;
    if (interactive) {
      DrawSettingsButton(view, origin);
      menu_open_now |= DrawMemoPopup(state, view, art, answer);
      menu_open_now |= DrawSettingsPopup(settings, state, art, answer);
    }
    state.menu_open = menu_open_now;
    DrawCaption(settings, view, named, frame.located, side);
  }
  ImGui::End();
  ImGui::PopStyleColor(kGripColorCount);

  // L'annonce attend la fin du geste : un glissement garde un item actif tout
  // du long, et l'on n'écrit pas un fichier soixante fois par seconde.
  if (state.save_pending && !ImGui::IsAnyItemActive()) {
    answer.settings_changed = true;
    state.save_pending = false;
  }
  return answer;
}

}  // namespace ro::ui
