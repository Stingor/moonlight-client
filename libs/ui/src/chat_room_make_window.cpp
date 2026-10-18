#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_room_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "chat_room_parts.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

using chat_room::Owned;
using chat_room::Typed;

// La largeur du formulaire ; sa hauteur suit le contenu.
constexpr float kMakeWidthPixels = 330.0F;
// Ce que la limite laisse à sa droite, en tailles de police : le mot « pers. ».
constexpr float kLimitUnitFonts = 4.0F;
// Ce que le mot de passe laisse à l'œil.
constexpr float kEyeFonts = 2.2F;
// Combien de salons l'annuaire montre avant de défiler.
constexpr float kNearbyRowsShown = 4.0F;

constexpr const char* kRecentPopup = "##chatroom_recent";

// UN LIBELLÉ DE COLONNE, puis le curseur posé à sa droite.
void Label(const char* text, float column) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(text);
  ImGui::SameLine(column);
}

void DrawTitleRow(ChatRoomMakeState& state, const ChatRoomMakeView& view, const SkinArt& art,
                  float column, bool opening, ChatRoomMakeAction& action) {
  Label(Owned(view.words.title_label).c_str(), column);
  const bool faulty = view.error_field == ChatRoomField::kTitle;
  if (faulty) {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, chat_room::kFaultyFieldColor);
  }
  // Le bouton des derniers titres, au bout du champ.
  const char* const recent_label = "...##chatroom_recent_button";
  const bool has_recent = !view.recent_titles.empty();
  const float recent_width =
      has_recent ? SkinSmallButtonWidth(recent_label, art) + ImGui::GetStyle().ItemSpacing.x : 0.0F;
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - recent_width);
  if (opening) {
    ImGui::SetKeyboardFocusHere();
  }
  // ⚠ ENTRÉE VAUT « CRÉER », comme le bouton par défaut du client d'origine.
  if (ImGui::InputText("##chatroom_title", state.title.data(), state.title.size(),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    action.create = true;
  } else if (faulty && ImGui::IsItemEdited()) {
    action.edited_faulty_field = true;
  }
  if (faulty) {
    ImGui::PopStyleColor();
  }
  if (has_recent) {
    ImGui::SameLine();
    if (SkinSmallButton(recent_label, art)) {
      ImGui::OpenPopup(kRecentPopup);
    }
    SkinTooltip(core::Tr("Les derniers titres"));
    if (BeginSkinPopup(kRecentPopup)) {
      for (const std::string& recent : view.recent_titles) {
        if (ImGui::Selectable(chat_room::Shown(recent).c_str())) {
          const std::size_t kept = std::min(recent.size(), state.title.size() - 1);
          std::copy_n(recent.begin(), kept, state.title.begin());
          state.title[kept] = '\0';
          action.edited_faulty_field = faulty;
        }
      }
      EndSkinPopup();
    }
  }

  // LE COMPTEUR, SOUS LE CHAMP : le plafond du serveur est en octets, et le
  // client d'origine laisse découvrir la coupe après coup.
  const std::string_view title = Typed(state.title);
  ImGui::SetCursorPosX(column);
  chat_room::DrawByteCounter(title.size(), view.limits.title_bytes);
  if (view.ascii_required && !chat_room::IsAscii(title)) {
    ImGui::SameLine();
    ColoredText(PaintingLook().bad_text, core::Tr("(caractères non anglais)"));
    ImGui::SameLine();
    SkinHelpMarker(Owned(view.words.english_only).c_str());
  }
  if (!view.title_taken.empty()) {
    ImGui::SetCursorPosX(column);
    ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().warn_text);
    ImGui::PushTextWrapPos(0.0F);
    Unformatted(view.title_taken);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }
}

void DrawAccessRows(ChatRoomMakeState& state, const ChatRoomMakeView& view, const SkinArt& art,
                    float column, ChatRoomMakeAction& action) {
  // ── LA LIMITE : de deux à vingt, et non six valeurs dont trois visibles ────
  Label(Owned(view.words.limit_label).c_str(), column);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x -
                          ImGui::GetFontSize() * kLimitUnitFonts);
  static_cast<void>(SkinSliderInt("##chatroom_limit", &state.limit, view.limits.fewest_seats,
                                  view.limits.most_seats, art));
  ImGui::SameLine();
  ImGui::TextUnformatted(Owned(view.words.people_unit).c_str());

  // ── L'ACCÈS : le vrai type du salon, que le mot de passe accompagne ───────
  Label(core::Tr("Accès"), column);
  if (SkinRadio(Owned(view.words.public_word).c_str(), state.is_public, art)) {
    state.is_public = true;
  }
  ImGui::SameLine();
  if (SkinRadio(Owned(view.words.private_word).c_str(), !state.is_public, art)) {
    state.is_public = false;
  }

  // ── LE MOT DE PASSE, grisé sur un salon public, où il ne sert à rien ──────
  ImGui::BeginDisabled(state.is_public);
  Label(Owned(view.words.password_label).c_str(), column);
  const bool faulty = view.error_field == ChatRoomField::kPassword;
  if (faulty) {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, chat_room::kFaultyFieldColor);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * kEyeFonts);
  static_cast<void>(ImGui::InputText("##chatroom_password", state.password.data(),
                                     state.password.size(),
                                     state.show_password ? 0 : ImGuiInputTextFlags_Password));
  if (faulty && ImGui::IsItemEdited()) {
    action.edited_faulty_field = true;
  }
  if (faulty) {
    ImGui::PopStyleColor();
  }
  ImGui::SameLine();
  if (SkinSmallButton(state.show_password ? "*##chatroom_eye" : "o##chatroom_eye", art)) {
    state.show_password = !state.show_password;
  }
  SkinTooltip(core::Tr("Afficher / masquer le mot de passe"));
  // LA RÈGLE DES QUATRE CARACTÈRES, dite AVANT le clic et non après.
  if (!state.is_public) {
    const std::size_t typed = chat_room::PasswordBytes(Typed(state.password));
    ImGui::SetCursorPosX(column);
    if (typed < view.limits.fewest_password_bytes) {
      ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().warn_text);
      std::array<char, chat_room::kCounterBytes> rule{};
      std::snprintf(rule.data(), rule.size(), core::Tr("%zu à %zu caractères"),
                    view.limits.fewest_password_bytes, view.limits.password_bytes);
      ImGui::TextUnformatted(rule.data());
      ImGui::PopStyleColor();
    } else {
      chat_room::DrawByteCounter(typed, view.limits.password_bytes);
    }
  }
  ImGui::EndDisabled();
}

void DrawNearby(const ChatRoomMakeView& view, ChatRoomMakeAction& action) {
  std::array<char, chat_room::kLineBytes> header{};
  std::snprintf(header.data(), header.size(), core::Tr("Salons proches (%zu)"), view.nearby.size());
  if (!ImGui::CollapsingHeader(header.data())) {
    return;
  }
  if (view.nearby.empty()) {
    FaintText(core::Tr("Aucun salon en vue."));
    return;
  }
  const float row = ImGui::GetTextLineHeightWithSpacing();
  const float shown = std::min(static_cast<float>(view.nearby.size()), kNearbyRowsShown);
  if (!ImGui::BeginChild("##chatroom_nearby", ImVec2(0.0F, row * shown + row * 0.5F), true)) {
    ImGui::EndChild();
    return;
  }
  for (const ChatRoomNearbyView& room : view.nearby) {
    std::array<char, chat_room::kLineBytes> line{};
    const bool full = room.users >= room.limit && room.limit != 0;
    const std::string title = chat_room::Shown(room.title);
    std::snprintf(line.data(), line.size(), "%s (%u/%u)%s%s##nearby_%u", title.c_str(),
                  static_cast<unsigned>(room.users), static_cast<unsigned>(room.limit),
                  room.locked ? " - " : "", room.locked ? core::Tr("privé") : "",
                  static_cast<unsigned>(room.room_id));
    ImGui::BeginDisabled(!room.joinable);
    if (ImGui::Selectable(line.data())) {
      action.join_room = room.room_id;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      std::array<char, chat_room::kLineBytes> where{};
      std::snprintf(where.data(), where.size(), core::Tr("À %d cases. Clic : rejoindre."),
                    room.distance_cells);
      SkinTooltip(full ? core::Tr("Ce salon est plein.") : where.data());
    }
  }
  ImGui::EndChild();
}

}  // namespace

ChatRoomMakeAction DrawChatRoomMakeWindow(ChatRoomMakeState& state, const ChatRoomMakeView& view,
                                          const SkinArt& art) {
  ChatRoomMakeAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  const bool opening = state.raise;
  if (state.raise) {
    const ImGuiViewport* screen = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  // Largeur imposée, hauteur qui suit le contenu.
  ImGui::SetNextWindowSize(ImVec2(kMakeWidthPixels * scale, 0.0F), ImGuiCond_Always);
  const std::string name = Owned(view.words.making_room) + "###" + kChatRoomMakeWindowId;
  bool open = true;
  const bool body = BeginSkinnedWindow(name.c_str(), &open, art, SkinnedTitle::kBar,
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
  if (body && open) {
    // La colonne des libellés, mesurée sur le plus long DANS LA LANGUE COURANTE.
    const float column =
        chat_room::LabelColumn({view.words.title_label, view.words.limit_label,
                                view.words.password_label, std::string_view(core::Tr("Accès"))});
    DrawTitleRow(state, view, art, column, opening, action);
    ImGui::Spacing();
    DrawAccessRows(state, view, art, column, action);

    if (!view.error.empty()) {
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().bad_text);
      ImGui::PushTextWrapPos(0.0F);
      Unformatted(view.error);
      ImGui::PopTextWrapPos();
      ImGui::PopStyleColor();
    }
    if (!view.blocker.empty()) {
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().warn_text);
      ImGui::PushTextWrapPos(0.0F);
      Unformatted(view.blocker);
      ImGui::PopTextWrapPos();
      ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    DrawNearby(view, action);
    ImGui::Separator();

    // ── LES BOUTONS, à droite ───────────────────────────────────────────────
    const std::string_view title = Typed(state.title);
    const std::size_t password = chat_room::PasswordBytes(Typed(state.password));
    const bool can_send = !view.waiting && view.blocker.empty() && !title.empty() &&
                          title.size() <= view.limits.title_bytes &&
                          (!view.ascii_required || chat_room::IsAscii(title)) &&
                          (state.is_public || (password >= view.limits.fewest_password_bytes &&
                                               password <= view.limits.password_bytes));
    // « Envoi... » compte : c'est le libellé du bouton pendant l'attente, et il
    // est plus large que « Créer ».
    const std::array<const char*, 3> labels = {core::Tr("Créer"), core::Tr("Annuler"),
                                               core::Tr("Envoi...")};
    const float width = SkinWidestButton(labels, art);
    const float total = width * 2.0F + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         std::max(0.0F, ImGui::GetContentRegionAvail().x - total));
    ImGui::BeginDisabled(!can_send);
    if (SkinButton(view.waiting ? core::Tr("Envoi...") : core::Tr("Créer"), art,
                   ImVec2(width, 0.0F))) {
      action.create = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (SkinButton(core::Tr("Annuler"), art, ImVec2(width, 0.0F))) {
      open = false;
    }
    // Entrée dans le titre ne crée que ce que le bouton créerait.
    action.create = action.create && can_send;
  }
  EndSkinnedWindow();
  if (!open) {
    state.open = false;
    action = ChatRoomMakeAction{};
    action.closed = true;
  }
  return action;
}

}  // namespace ro::ui
