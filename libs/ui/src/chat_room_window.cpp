#include "ro/ui/chat_room_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
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

// Les mesures de la salle.
constexpr float kFirstWidthPixels = 460.0F;
constexpr float kFirstHeightPixels = 280.0F;
constexpr float kSmallestWidthPixels = 300.0F;
constexpr float kSmallestHeightPixels = 160.0F;
// Ce qu'on la décale sous le centre de l'écran, à l'ouverture.
constexpr float kBelowCenterPixels = 60.0F;
// LA LISTE DES MEMBRES PREND LE CINQUIÈME DE LA LARGEUR, comme au client
// d'origine, avec un plancher pour qu'un nom y tienne toujours.
constexpr float kMembersShare = 0.2F;
constexpr float kFewestMembersPixels = 96.0F;
// Les rangées que les deux volets gardent au moins, sous une barre qui grandit.
constexpr float kFewestPaneRows = 3.0F;
// Les rangées du volet « Réglages » : quatre champs et le bouton.
constexpr float kSettingsFieldRows = 4.0F;
constexpr float kLimitUnitFonts = 4.0F;
// Ce que le mot de passe laisse à l'œil.
constexpr float kEyeFonts = 2.2F;

// Le chef, marqué d'un ambre SOMBRE : l'or pâle s'effaçait sur le corps clair.
constexpr ImU32 kOwnerColor = IM_COL32(158, 102, 0, 255);

constexpr const char* kMemberMenu = "##chatroom_member_menu";

// LA HAUTEUR DU VOLET : le séparateur, quatre rangées de champs, la ligne d'un
// refus quand il y en a un, et le bouton — à la hauteur de son IMAGE, plus
// haute qu'une rangée d'ImGui.
[[nodiscard]] float SettingsPaneHeight(const SkinArt& art, bool with_error) {
  const float spacing = ImGui::GetStyle().ItemSpacing.y;
  return spacing * 2.0F + ImGui::GetFrameHeightWithSpacing() * kSettingsFieldRows +
         (with_error ? ImGui::GetTextLineHeightWithSpacing() : 0.0F) + SkinButtonHeight(art) +
         spacing;
}

void CopyInto(std::span<char> buffer, std::string_view text) {
  // ⚠ COUPÉ SANS CASSER UN CARACTÈRE : un tampon plein d'un nom coréen finirait
  // sur un demi-caractère.
  text = core::WithoutDanglingUtf8(text.substr(0, std::min(text.size(), buffer.size() - 1)));
  const std::size_t kept = text.size();
  std::copy_n(text.begin(), kept, buffer.begin());
  buffer[kept] = '\0';
}

// Le libellé d'une ligne du menu d'un membre.
[[nodiscard]] std::string MemberVerbLabel(ChatRoomMemberVerb verb, const ChatRoomView& view) {
  switch (verb) {
    case ChatRoomMemberVerb::kExpel:
      return Owned(view.words.kick);
    case ChatRoomMemberVerb::kGiveOwner:
      return Owned(view.words.give_owner);
    case ChatRoomMemberVerb::kBefriend:
      return core::Tr("Ajouter en ami");
    case ChatRoomMemberVerb::kSaveChat:
      return core::Tr("Enregistrer le chat");
  }
  return {};
}

void DrawMembers(ChatRoomWindowState& state, const ChatRoomView& view,
                 ChatRoomWindowAction& action) {
  for (const ChatRoomMemberView& member : view.members) {
    const std::string name(member.name);
    // SÉLECTIONNER UN MEMBRE, C'EST LUI CHUCHOTER : le nom va dans la box de
    // la barre, où le joueur LIT à qui il parle. Le client d'origine détournait
    // la saisie sans rien montrer.
    if (!member.me) {
      const std::string id = "##chatroom_member_" + chat_room::Shown(name);
      if (ImGui::Selectable(id.c_str(), false, ImGuiSelectableFlags_AllowOverlap,
                            ImVec2(0.0F, ImGui::GetTextLineHeight())) &&
          view.input != nullptr) {
        CopyInto(view.input->whisper_to, member.name);
      }
      if (ImGui::IsItemHovered()) {
        std::array<char, chat_room::kLineBytes> tip{};
        std::snprintf(tip.data(), tip.size(), core::Tr("Chuchoter à %s."), name.c_str());
        SkinTooltip(tip.data());
      }
      ImGui::SameLine(0.0F, 0.0F);
    }
    if (member.owner) {
      ImGui::PushStyleColor(ImGuiCol_Text, kOwnerColor);
      ImGui::Text("* %s", name.c_str());
      ImGui::PopStyleColor();
    } else {
      ImGui::TextUnformatted(name.c_str());
    }
    // LE MENU D'UN MEMBRE, au clic droit sur son nom.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      state.menu_member = name;
      ImGui::OpenPopup(kMemberMenu);
    }
  }
  if (BeginSkinPopup(kMemberMenu)) {
    // ⚠ LE MEMBRE SE RELIT À CHAQUE IMAGE : parti entre-temps, il n'a plus de
    // menu, et ses droits ont pu changer depuis le clic.
    const auto aimed = std::ranges::find(view.members, std::string_view(state.menu_member),
                                         &ChatRoomMemberView::name);
    if (aimed == view.members.end()) {
      ImGui::CloseCurrentPopup();
    } else {
      FaintText(state.menu_member.c_str());
      ImGui::Separator();
      for (const ChatRoomMemberEntry& entry : ChatRoomMemberMenu(*aimed, view)) {
        if (ImGui::MenuItem(MemberVerbLabel(entry.verb, view).c_str(), nullptr, false,
                            entry.refusal == nullptr)) {
          ChooseChatRoomMemberVerb(entry.verb, state.menu_member, action);
        }
        if (entry.refusal != nullptr) {
          SkinTooltip(entry.refusal);
        }
      }
    }
    EndSkinPopup();
  }
}

void DrawSettingsPane(ChatRoomWindowState& state, const ChatRoomView& view, const SkinArt& art,
                      ChatRoomWindowAction& action) {
  ImGui::Separator();
  const float column =
      chat_room::LabelColumn({view.words.title_label, view.words.limit_label,
                              view.words.password_label, std::string_view(core::Tr("Accès"))});
  const auto label = [column](const char* text) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
    ImGui::SameLine(column);
  };
  label(Owned(view.words.title_label).c_str());
  ImGui::SetNextItemWidth(-1.0F);
  static_cast<void>(ImGui::InputText("##chatroom_set_title", state.settings_title.data(),
                                     state.settings_title.size()));
  label(Owned(view.words.limit_label).c_str());
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x -
                          ImGui::GetFontSize() * kLimitUnitFonts);
  static_cast<void>(SkinSliderInt("##chatroom_set_limit", &state.settings_limit,
                                  view.limits.fewest_seats, view.limits.most_seats, art));
  ImGui::SameLine();
  ImGui::TextUnformatted(Owned(view.words.people_unit).c_str());
  label(core::Tr("Accès"));
  if (SkinRadio(Owned(view.words.public_word).c_str(), state.settings_public, art)) {
    state.settings_public = true;
  }
  ImGui::SameLine();
  if (SkinRadio(Owned(view.words.private_word).c_str(), !state.settings_public, art)) {
    state.settings_public = false;
  }
  ImGui::BeginDisabled(state.settings_public);
  label(Owned(view.words.password_label).c_str());
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * kEyeFonts);
  static_cast<void>(ImGui::InputText(
      "##chatroom_set_password", state.settings_password.data(), state.settings_password.size(),
      state.settings_show_password ? 0 : ImGuiInputTextFlags_Password));
  ImGui::SameLine();
  if (SkinSmallButton(state.settings_show_password ? "*##chatroom_set_eye" : "o##chatroom_set_eye",
                      art)) {
    state.settings_show_password = !state.settings_show_password;
  }
  SkinTooltip(core::Tr("Afficher / masquer le mot de passe"));
  ImGui::EndDisabled();
  if (!view.settings_error.empty()) {
    ColoredText(PaintingLook().bad_text, view.settings_error);
  }

  const std::string_view title = Typed(state.settings_title);
  const std::size_t password = chat_room::PasswordBytes(Typed(state.settings_password));
  const bool ok =
      !title.empty() && title.size() <= view.limits.title_bytes && chat_room::IsAscii(title) &&
      (state.settings_public ||
       (password >= view.limits.fewest_password_bytes && password <= view.limits.password_bytes));
  ImGui::BeginDisabled(!ok);
  // ⚠ LE VOLET NE SE FERME PAS ICI : c'est l'étape qui le referme une fois les
  // réglages partis, et le garde ouvert sur un refus.
  if (SkinButton(core::Tr("Appliquer"), art)) {
    action.apply_settings = true;
  }
  ImGui::EndDisabled();
}

void LoadSettings(ChatRoomWindowState& state, const ChatRoomView& view) {
  CopyInto(state.settings_title, view.title);
  CopyInto(state.settings_password, view.password);
  state.settings_limit = std::clamp(view.limit, view.limits.fewest_seats, view.limits.most_seats);
  state.settings_public = view.is_public;
}

}  // namespace

ChatRoomWindowAction DrawChatRoomWindow(ChatRoomWindowState& state, const ChatRoomView& view,
                                        const SkinArt& art) {
  ChatRoomWindowAction action;
  const float scale = Scale();
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(screen->GetCenter().x, screen->GetCenter().y + kBelowCenterPixels * scale),
      ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(kSmallestWidthPixels * scale, kSmallestHeightPixels * scale),
      ImVec2(FLT_MAX, FLT_MAX));
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  // LE VOLET N'APPARTIENT QU'AU CHEF. Sa bascule a eu lieu à l'image
  // précédente : la fenêtre grandit — ou rétrécit — MAINTENANT, et le volet
  // ne se dessinera qu'une fois la place prise.
  const bool wanted = state.settings_wanted && view.owned;
  if (wanted != state.settings_shown) {
    if (state.last_height > 0.0F) {
      const float delta = wanted ? SettingsPaneHeight(art, !view.settings_error.empty())
                                 : -SettingsPaneHeight(art, !view.settings_error.empty());
      ImGui::SetNextWindowSize(ImVec2(state.last_width, std::max(kSmallestHeightPixels * scale,
                                                                 state.last_height + delta)),
                               ImGuiCond_Always);
    }
    state.settings_shown = wanted;
  }

  // Le titre change avec le salon, l'identité jamais : la place de la fenêtre
  // survit d'un salon à l'autre.
  const std::string name = std::string(view.caption) + "###" + kChatRoomWindowId;
  bool open = true;
  const bool body =
      BeginSkinnedWindow(name.c_str(), &open, art, SkinnedTitle::kBar, ImGuiWindowFlags_NoCollapse);
  if (body && open) {
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float settings_height =
        state.settings_shown ? SettingsPaneHeight(art, !view.settings_error.empty()) : 0.0F;
    const float buttons_height = SkinButtonHeight(art) + spacing;
    // LA BARRE DIT SA HAUTEUR : elle grandit avec la phrase, plafonnée pour
    // laisser aux deux volets trois rangées.
    const float input_most = ImGui::GetContentRegionAvail().y - settings_height - buttons_height -
                             ImGui::GetFrameHeightWithSpacing() * kFewestPaneRows;
    const float input_height = view.input != nullptr && view.preview != nullptr
                                   ? ChatInputHeight(*view.input, *view.preview, input_most, art)
                                   : ImGui::GetFrameHeight();
    const float panes_height = std::max(ImGui::GetFrameHeightWithSpacing(),
                                        ImGui::GetContentRegionAvail().y - input_height - spacing -
                                            buttons_height - spacing - settings_height);
    const float total_width = ImGui::GetContentRegionAvail().x;
    const float members_width = std::max(kFewestMembersPixels * scale, total_width * kMembersShare);
    const float lines_width = total_width - members_width - ImGui::GetStyle().ItemSpacing.x;

    // ── LE FIL : le même moteur que la conversation, liens compris ─────────
    if (ImGui::BeginChild("##chatroom_lines", ImVec2(lines_width, panes_height), true)) {
      action.link = DrawChatLog(view.lines, view.log_style, ImGui::GetContentRegionAvail().y, art,
                                view.layout)
                        .link;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("##chatroom_members", ImVec2(members_width, panes_height), true)) {
      DrawMembers(state, view, action);
    }
    ImGui::EndChild();

    // ── LA BARRE DE LA CONVERSATION ─────────────────────────────────────────
    if (view.input != nullptr && view.preview != nullptr) {
      action.said = DrawChatInput(*view.input, *view.preview, &action.input, art, view.input_links,
                                  view.link_facts);
    } else {
      ColoredText(PaintingLook().bad_text,
                  core::Tr("La conversation est fermée : rien ne peut s'écrire ici."));
    }

    // ── LES BOUTONS ─────────────────────────────────────────────────────────
    // ⚠ LE MOT EST GARDÉ LE TEMPS DE LA MESURE : la pointe d'un temporaire
    // pendrait dès la fin de la déclaration.
    const std::string setup = Owned(view.words.room_setup);
    const std::array<const char*, 2> labels = {core::Tr("Quitter"), setup.c_str()};
    const float width = SkinWidestButton(labels, art);
    if (SkinButton(core::Tr("Quitter"), art, ImVec2(width, 0.0F))) {
      action.leave = true;
    }
    if (view.owned) {
      ImGui::SameLine();
      // UN VOLET DE CETTE FENÊTRE, et non une fenêtre détachée : ces réglages
      // n'appartiennent qu'à cette salle.
      if (SkinToggleButton(Owned(view.words.room_setup).c_str(), state.settings_wanted, art,
                           ImVec2(width, 0.0F))) {
        state.settings_wanted = !state.settings_wanted;
        if (state.settings_wanted) {
          LoadSettings(state, view);
        }
      }
    }
    if (state.settings_shown) {
      DrawSettingsPane(state, view, art, action);
    }
    state.last_width = ImGui::GetWindowSize().x;
    state.last_height = ImGui::GetWindowSize().y;
  }
  EndSkinnedWindow();
  // LA CROIX QUITTE LE SALON, comme le « /q » qu'elle porte chez le client
  // d'origine : la masquer laisserait le joueur occupant sans le voir.
  if (!open) {
    action.leave = true;
  }
  return action;
}

std::vector<ChatRoomMemberEntry> ChatRoomMemberMenu(const ChatRoomMemberView& member,
                                                    const ChatRoomView& view) {
  std::vector<ChatRoomMemberEntry> entries;
  if (!member.me) {
    if (view.owned) {
      entries.push_back(ChatRoomMemberEntry{.verb = ChatRoomMemberVerb::kExpel});
      entries.push_back(ChatRoomMemberEntry{.verb = ChatRoomMemberVerb::kGiveOwner});
    }
    // LES MÊMES REFUS QUE LE MENU D'UN PSEUDO : pas deux fois le même, pas
    // au-delà de la liste.
    ChatRoomMemberEntry befriend{.verb = ChatRoomMemberVerb::kBefriend};
    if (member.befriended) {
      befriend.refusal = core::Tr("Déjà dans votre liste d'amis.");
    } else if (view.friends_full) {
      befriend.refusal = core::Tr("Votre liste d'amis est pleine.");
    }
    entries.push_back(befriend);
  }
  entries.push_back(ChatRoomMemberEntry{.verb = ChatRoomMemberVerb::kSaveChat});
  return entries;
}

void ChooseChatRoomMemberVerb(ChatRoomMemberVerb verb, std::string_view member,
                              ChatRoomWindowAction& action) {
  switch (verb) {
    case ChatRoomMemberVerb::kExpel:
      action.expel = std::string(member);
      return;
    case ChatRoomMemberVerb::kGiveOwner:
      action.give_owner = std::string(member);
      return;
    case ChatRoomMemberVerb::kBefriend:
      action.befriend = std::string(member);
      return;
    case ChatRoomMemberVerb::kSaveChat:
      action.save_chat = true;
      return;
  }
}

}  // namespace ro::ui
