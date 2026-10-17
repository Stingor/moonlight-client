#include "ro/ui/cart_choice_window.h"

#include <string>

#include <imgui.h>

#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// LE DÉCALAGE PROPRE À CHAQUE TYPE, dans sa case : de combien le bouton glisse
// à droite, et à quelle hauteur de sa rangée il commence. Le client le règle
// type par type pour que chaque charrette tombe au milieu de sa case.
struct ButtonShift {
  float right = 0.0F;
  float top = 0.0F;
};

[[nodiscard]] ButtonShift ShiftFor(std::uint8_t cart_type) noexcept {
  switch (cart_type) {
    case 1:
    case 2:
    case 10:
      return {0.0F, 15.0F};
    case 4:
      return {18.0F, 18.0F};
    case 5:
      return {15.0F, 23.0F};
    case 6:
    case 7:
    case 8:
    case 9:
      return {13.0F, 28.0F};
    case 11:
      return {16.0F, 15.0F};
    case 12:
      return {0.0F, 5.0F};
    case 13:
      return {4.0F, 18.0F};
    default:
      return {0.0F, 28.0F};
  }
}

}  // namespace

ImVec2 CartChoiceButtonOrigin(std::size_t rank, std::uint8_t cart_type) noexcept {
  const ButtonShift shift = ShiftFor(cart_type);
  const auto column = static_cast<float>(rank % kCartChoiceColumns);
  const auto row = static_cast<float>(rank / kCartChoiceColumns);
  return ImVec2(kCartChoiceColumnStep * column + shift.right,
                kCartChoiceButtonRowStep * row + shift.top);
}

float CartChoiceWindowHeight(std::size_t count) noexcept {
  const std::size_t rows = count == 0 ? 1 : (count - 1) / kCartChoiceColumns + 1;
  return kCartChoiceWindowRowHeight * static_cast<float>(rows);
}

std::optional<std::size_t> CartChoiceButtonAt(std::span<const std::uint8_t> types,
                                              ImVec2 point) noexcept {
  // LÀ OÙ DEUX CASES SE RECOUVRENT, LE CENTRE LE PLUS PROCHE L'EMPORTE : un
  // clic sur une charrette va à cette charrette, et non à la case voisine qui
  // déborde sur elle.
  std::optional<std::size_t> found;
  float nearest = 0.0F;
  const float half = kCartChoiceButtonSide / 2.0F;
  for (std::size_t rank = 0; rank < types.size(); ++rank) {
    const ImVec2 origin = CartChoiceButtonOrigin(rank, types[rank]);
    if (point.x < origin.x || point.x >= origin.x + kCartChoiceButtonSide || point.y < origin.y ||
        point.y >= origin.y + kCartChoiceButtonSide) {
      continue;
    }
    const float dx = point.x - (origin.x + half);
    const float dy = point.y - (origin.y + half);
    const float distance = (dx * dx) + (dy * dy);
    if (!found.has_value() || distance < nearest) {
      found = rank;
      nearest = distance;
    }
  }
  return found;
}

CartChoiceWindowAction DrawCartChoiceWindow(CartChoiceWindowState& state,
                                            const CartChoiceView& view, const SkinArt& art) {
  CartChoiceWindowAction action;
  if (!state.open) {
    state.was_open = false;
    state.pressed.reset();
    return action;
  }
  if (!state.was_open) {
    state.was_open = true;
    state.pressed.reset();
    ImGui::SetNextWindowFocus();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  }

  // 🔴 LA TAILLE EST CELLE DU CLIENT, ET IMPOSÉE : les boutons s'y posent à des
  // coordonnées fixes depuis le coin de la fenêtre.
  const float scale = Scale();
  ImGui::SetNextWindowSize(
      ImVec2(kCartChoiceWindowWidth * scale, CartChoiceWindowHeight(view.types.size()) * scale),
      ImGuiCond_Always);

  // Le titre change avec la langue, l'identité jamais : la place et la taille
  // de la fenêtre y restent attachées.
  std::string name(view.title);
  name += "###";
  name += kCartChoiceWindowId;
  bool open = true;
  const bool begun =
      BeginSkinnedWindow(name.c_str(), &open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (!open) {
    action.closed = true;
  }
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }

  const ImVec2 corner = ImGui::GetWindowPos();
  ImDrawList* const into = ImGui::GetWindowDrawList();
  for (std::size_t rank = 0; rank < view.types.size(); ++rank) {
    if (rank >= view.images.size() || !view.images[rank].usable()) {
      continue;
    }
    const ImVec2 origin = CartChoiceButtonOrigin(rank, view.types[rank]);
    const ImVec2 top_left(corner.x + origin.x * scale, corner.y + origin.y * scale);
    into->AddImage(view.images[rank].id, top_left,
                   ImVec2(top_left.x + kCartChoiceButtonSide * scale,
                          top_left.y + kCartChoiceButtonSide * scale));
  }

  // LE CLIC SE LIT SUR TOUT LE CORPS, puis se range au bouton qu'il touche : les
  // boutons se chevauchent, et c'est le centre le plus proche qui tranche.
  //
  // ⚠ UNE SEULE ZONE CLIQUABLE COUVRE LE CORPS : sans elle, appuyer sur un
  // bouton ferait glisser la fenêtre au lieu de le presser.
  const ImVec2 body = ImGui::GetContentRegionAvail();
  if (body.x > 0.0F && body.y > 0.0F) {
    ImGui::InvisibleButton("##cart_buttons", body);
  }
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  // 🔴 LA ZONE CLIQUABLE DEVIENT L'ÉLÉMENT ACTIF dès qu'on appuie, et une
  // fenêtre dont un élément est actif ne se dit plus survolée : sans ce
  // drapeau, aucun clic posé sur les images n'était vu.
  const std::optional<std::size_t> under =
      ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
          ? CartChoiceButtonAt(view.types,
                               ImVec2((mouse.x - corner.x) / scale, (mouse.y - corner.y) / scale))
          : std::nullopt;
  if (under.has_value()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      state.pressed = under;
    }
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    if (state.pressed.has_value() && under == state.pressed) {
      action.chosen = view.types[*state.pressed];
    }
    state.pressed.reset();
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
