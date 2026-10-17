#include "ro/ui/party_friend_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link_offer.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "party_friend_parts.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

// Placement et taille de première ouverture, en pixels d'écran.
constexpr float kSpawnX = 420.0F;
constexpr float kSpawnY = 120.0F;
constexpr float kSpawnWidth = 330.0F;
constexpr float kSpawnHeight = 326.0F;
// Le plancher : les deux onglets et une ligne de membre lisible.
constexpr float kLeastWidthPixels = 260.0F;
constexpr float kLeastHeightPixels = 150.0F;
constexpr float kLeastCheckBoxPixels = 10.0F;
constexpr float kNameFieldPixels = 150.0F;
constexpr float kPercent = 100.0F;

[[nodiscard]] std::string_view Typed(const std::array<char, 64>& buffer) {
  const auto end = std::find(buffer.begin(), buffer.end(), '\0');
  return {buffer.data(), static_cast<std::size_t>(end - buffer.begin())};
}

void Clear(std::array<char, 64>& buffer) { buffer.fill('\0'); }

// Relève les réglages du groupe chez le serveur : un changement recale ce que
// le joueur éditait, et s'annonce — sauf au premier relevé, qui n'est pas un
// changement, et hors groupe.
void NoteOptions(PartyFriendWindowState& state, const PartyFriendView& view,
                 PartyFriendAction& action) {
  const PartyOptionsView& now = view.options;
  if (!now.known) {
    state.seen.known = false;
    return;
  }
  if (state.seen.known && state.seen.exp == now.exp && state.seen.pickup == now.pickup &&
      state.seen.share == now.share) {
    return;
  }
  const bool was_known = state.seen.known;
  state.seen = now;
  state.opt_exp = now.exp;
  state.opt_pickup = now.pickup;
  state.opt_share = now.share;
  if (!was_known || !view.in_party) {
    return;
  }
  action.chat_line = PartyOptionsAnnouncement(now);
}

void Radio(const char* label, int* chosen, int value) {
  static_cast<void>(ImGui::RadioButton(label, chosen, value));
}

// Les trois choix du groupe. Le serveur ne les accepte que du chef : le reste
// se grise. Le constat du partage, lui, vaut pour tout le monde.
void DrawPartyOptions(PartyFriendWindowState& state, const PartyFriendView& view,
                      const SkinArt& art, PartyFriendAction& action) {
  if (!ImGui::CollapsingHeader(core::Tr("Réglages du groupe"))) {
    return;
  }
  const bool leader = view.i_am_leader;
  if (!leader) {
    ImGui::TextDisabled("%s", core::Tr("Seul le chef de groupe peut les modifier."));
    ImGui::BeginDisabled();
  }

  // UN IDENTIFIANT PAR GROUPE : trois radios portent le même libellé.
  ImGui::TextUnformatted(core::Tr("Expérience"));
  ImGui::PushID("exp");
  Radio(core::Tr("Individuelle"), &state.opt_exp, 0);
  ImGui::SameLine();
  Radio(core::Tr("Partagée"), &state.opt_exp, 1);
  ImGui::PopID();

  ImGui::TextUnformatted(core::Tr("Ramassage"));
  ImGui::PushID("pickup");
  Radio(core::Tr("Individuel"), &state.opt_pickup, 0);
  ImGui::SameLine();
  Radio(core::Tr("Groupe"), &state.opt_pickup, 1);
  ImGui::PopID();

  ImGui::TextUnformatted(core::Tr("Partage des items"));
  ImGui::PushID("share");
  Radio(core::Tr("Individuel"), &state.opt_share, 0);
  ImGui::SameLine();
  Radio(core::Tr("Partagé"), &state.opt_share, 1);
  ImGui::PopID();

  const PartyOptionsView& now = view.options;
  const bool dirty =
      state.opt_exp != now.exp || state.opt_pickup != now.pickup || state.opt_share != now.share;
  if (dirty) {
    ImGui::Spacing();
    if (SkinButton(core::Tr("Appliquer"), art)) {
      action.verb = PartyFriendVerb::kPartyOptions;
      action.exp = state.opt_exp;
      action.pickup = state.opt_pickup;
      action.share = state.opt_share;
    }
    ImGui::SameLine();
    if (SkinButton(core::Tr("Annuler"), art)) {
      state.opt_exp = now.exp;
      state.opt_pickup = now.pickup;
      state.opt_share = now.share;
    }
  }
  if (!leader) {
    ImGui::EndDisabled();
  }

  // Le serveur écarte du partage en silence : l'écarté ne reçoit rien, et le
  // chef ne voit rien non plus. On le dit sous « Expérience : Partagée ».
  if (now.exp != 1 || !view.share_known) {
    return;
  }
  ImGui::Spacing();
  int excluded = 0;
  for (const PartyMemberView& member : view.members) {
    const std::string why =
        PartyShareReason(PartyShareFlags(view, member), view.share_idle_seconds);
    if (why.empty()) {
      continue;
    }
    if (excluded == 0) {
      ImGui::TextDisabled("%s", core::Tr("Ne reçoivent pas d'EXP de vos kills :"));
    }
    const std::string name(member.name);
    ImGui::BulletText("%s (%s)", name.c_str(), why.c_str());
    ++excluded;
  }
  if (excluded == 0) {
    ImGui::TextDisabled("%s", core::Tr("Tout le groupe reçoit sa part."));
  }
}

void DrawPartyTab(PartyFriendWindowState& state, const PartyFriendView& view,
                  const PartyFriendLook& look, const SkinArt& art, PartyFriendAction& action) {
  // LA RECHERCHE DE GROUPE, dans le groupe ou hors de lui : on y cherche des
  // compagnons comme on y en recrute.
  if (SkinButton(core::Tr("Recherche de groupe"), art)) {
    action.booking_asked = true;
  }
  if (!view.in_party) {
    ImGui::SetNextItemWidth(Px(kNameFieldPixels));
    ImGui::InputText("##partyname", state.new_party_name.data(), state.new_party_name.size());
    ImGui::SameLine();
    const std::string_view name = Typed(state.new_party_name);
    ImGui::BeginDisabled(name.empty());
    if (SkinButton(core::Tr("Créer un groupe"), art)) {
      action.verb = PartyFriendVerb::kCreateParty;
      action.name = std::string(name);
      Clear(state.new_party_name);
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::TextDisabled("%s", core::Tr("Vous n'êtes dans aucun groupe."));
    return;
  }

  if (!view.party_name.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Groupe :"));
    ImGui::SameLine();
    ImGui::TextUnformatted(view.party_name.data(), view.party_name.data() + view.party_name.size());
  }

  // Le serveur n'accepte d'invitation que du chef : le champ n'est qu'à lui, et
  // « Quitter » reprend alors le début de la ligne.
  if (view.i_am_leader) {
    ImGui::SetNextItemWidth(Px(kNameFieldPixels));
    const bool submitted =
        ImGui::InputText("##invitename", state.invite_name.data(), state.invite_name.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    const std::string_view name = Typed(state.invite_name);
    ImGui::BeginDisabled(name.empty());
    const bool clicked = SkinButton(core::Tr("Inviter"), art);
    ImGui::EndDisabled();
    if (!name.empty() && (clicked || submitted)) {
      action.verb = PartyFriendVerb::kInvite;
      action.name = std::string(name);
      Clear(state.invite_name);
    }
    ImGui::SameLine();
  }
  if (SkinButton(core::Tr("Quitter le groupe"), art)) {
    const PartyMemberView* me = PartyMe(view);
    AskPartyConfirm(state, PartyFriendVerb::kLeave, me != nullptr ? me->account_id : 0,
                    me != nullptr ? me->char_id : 0, {});
  }

  DrawPartyOptions(state, view, art, action);

  // L'espace entre lignes EST l'espacement d'ImGui : à zéro, elles se touchent.
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(ImGui::GetStyle().ItemSpacing.x,
                             Px(static_cast<float>(std::max(0, look.row_spacing)))));
  for (std::size_t rank = 0; rank < view.members.size(); ++rank) {
    ImGui::PushID(static_cast<int>(rank));
    DrawPartyRow(state, view, view.members[rank], look, action);
    ImGui::PopID();
  }
  ImGui::PopStyleVar();

  ImGui::TextDisabled("%s %d/%d", core::Tr("Membres :"), static_cast<int>(view.members.size()),
                      kPartyMembersMost);
}

void DrawFriendTab(PartyFriendWindowState& state, const PartyFriendView& view,
                   const PartyFriendLook& look, const SkinArt& art, PartyFriendAction& action) {
  // Le serveur refuse l'ajout quand la liste est pleine : on ne le propose pas.
  const bool full = static_cast<int>(view.friends.size()) >= kFriendsMost;
  ImGui::SetNextItemWidth(Px(kNameFieldPixels));
  const bool submitted =
      ImGui::InputText("##friendname", state.friend_name.data(), state.friend_name.size(),
                       ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  const std::string_view name = Typed(state.friend_name);
  const bool can_add = !name.empty() && !full;
  ImGui::BeginDisabled(!can_add);
  const bool clicked = SkinButton(core::Tr("Ajouter un ami"), art);
  ImGui::EndDisabled();
  if (full && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", core::Tr("Votre liste d'amis est pleine."));
  }
  if (can_add && (clicked || submitted)) {
    action.verb = PartyFriendVerb::kAddFriend;
    action.name = std::string(name);
    Clear(state.friend_name);
  }
  ImGui::Separator();

  if (view.friends.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Votre liste d'amis est vide."));
    return;
  }
  for (std::size_t rank = 0; rank < view.friends.size(); ++rank) {
    ImGui::PushID(static_cast<int>(rank));
    DrawFriendRow(state, view, view.friends[rank], look, action);
    ImGui::PopID();
  }
  ImGui::Separator();
  ImGui::TextDisabled("%s %d/%d", core::Tr("Amis :"), static_cast<int>(view.friends.size()),
                      kFriendsMost);
}

void DrawBody(PartyFriendWindowState& state, const PartyFriendView& view,
              const PartyFriendLook& look, const SkinArt& art, PartyFriendAction& action) {
  // MAJ + CLIC SUR UN ONGLET : son lien dans le chat, et l'onglet choisi au
  // passage. ⚠ LU SUR LA GÉOMÉTRIE : voir `chat_link_offer.h`.
  const auto tab_button = [&state, &action, &art](const char* label, PartyFriendTab tab) {
    const bool pressed = SkinSmallToggleButton(label, state.tab == tab, art);
    const bool linked = OfferChatLink(true);
    if (linked) {
      action.link_tab = tab;
    }
    if (pressed || linked) {
      state.tab = tab;
    }
  };
  tab_button(core::Tr("Groupe"), PartyFriendTab::kParty);
  ImGui::SameLine();
  tab_button(core::Tr("Amis"), PartyFriendTab::kFriends);
  // La case de la grille, sur l'onglet du groupe seulement : elle ne montre que
  // lui. Recentrée sur la hauteur du bouton qui la précède.
  if (state.tab == PartyFriendTab::kParty && view.grid_offered) {
    const float row_height = ImGui::GetItemRectSize().y;
    const float box_height = std::max(Px(kLeastCheckBoxPixels), ImGui::GetTextLineHeight());
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0F, (row_height - box_height) * 0.5F));
    bool grid = view.grid_enabled;
    if (SkinCheckBox(core::Tr("Grille"), &grid, art)) {
      action.grid_toggled = true;
      action.grid_enabled = grid;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltipFormat("%s", core::Tr("Affiche le groupe en grille de tuiles. C'est le même "
                                       "réglage que dans le panneau des options."));
    }
  }
  ImGui::Separator();

  if (state.tab == PartyFriendTab::kParty) {
    DrawPartyTab(state, view, look, art, action);
  } else {
    DrawFriendTab(state, view, look, art, action);
  }
  DrawPartyConfirm(state, art, action);
}

}  // namespace

const PartyMemberView* PartyMe(const PartyFriendView& view) {
  for (const PartyMemberView& member : view.members) {
    if (member.is_me) {
      return &member;
    }
  }
  return nullptr;
}

std::string_view ShortMapName(std::string_view full) {
  for (std::size_t at = 0; at < full.size(); ++at) {
    if (full[at] == ',' || (full[at] == ' ' && at + 1 < full.size() && full[at + 1] == '(')) {
      return full.substr(0, at);
    }
  }
  return full;
}

std::uint8_t PartyShareFlags(const PartyFriendView& view, const PartyMemberView& member) {
  if (!view.share_known || member.offline) {
    return 0;
  }
  std::uint8_t flags = member.share_flags;
  if (!member.map_raw.empty() && !member.is_me) {
    const PartyMemberView* me = PartyMe(view);
    if (me != nullptr && !me->map_raw.empty() && me->map_raw != member.map_raw) {
      flags |= kPartyShareOtherMap;
    }
  }
  return flags;
}

std::string PartyShareReason(std::uint8_t flags, int idle_seconds) {
  if ((flags & kPartyShareIdle) != 0) {
    std::array<char, 160> line = {};
    std::snprintf(line.data(), line.size(), core::Tr("inactif depuis plus de %d s"), idle_seconds);
    return line.data();
  }
  if ((flags & kPartyShareBusy) != 0) {
    return core::Tr("en salon, en échoppe ou en achat automatique");
  }
  if ((flags & kPartyShareDead) != 0) {
    return core::Tr("mort");
  }
  if ((flags & kPartyShareOtherMap) != 0) {
    return core::Tr("sur une autre carte que vous");
  }
  return {};
}

std::string PartyGaugeCaption(int current, int most, int mode) {
  if (most <= 0) {
    return {};
  }
  const float fraction =
      std::clamp(static_cast<float>(current) / static_cast<float>(most), 0.0F, 1.0F);
  const int percent = current > 0 ? std::max(1, static_cast<int>(fraction * kPercent)) : 0;
  std::array<char, 64> text = {};
  switch (static_cast<PartyGaugeText>(mode)) {
    case PartyGaugeText::kNumbers:
      std::snprintf(text.data(), text.size(), "%d/%d", current, most);
      break;
    case PartyGaugeText::kPercent:
      std::snprintf(text.data(), text.size(), "%d %%", percent);
      break;
    case PartyGaugeText::kBoth:
      std::snprintf(text.data(), text.size(), "%d/%d (%d %%)", current, most, percent);
      break;
    case PartyGaugeText::kNone:
      break;
  }
  return text.data();
}

std::string PartyOptionsAnnouncement(const PartyOptionsView& options) {
  const char* exp = options.exp == 1 ? core::Tr("Partagée") : core::Tr("Individuelle");
  const char* pickup = options.pickup == 1 ? core::Tr("Groupe") : core::Tr("Individuel");
  const char* share = options.share == 1 ? core::Tr("Partagé") : core::Tr("Individuel");
  std::array<char, 256> line = {};
  std::snprintf(line.data(), line.size(), "%s - %s : %s | %s : %s | %s : %s", core::Tr("Groupe"),
                core::Tr("Expérience"), exp, core::Tr("Ramassage"), pickup, core::Tr("Partage"),
                share);
  return line.data();
}

const char* PartyLeaderRefusal(const PartyFriendView& view, const PartyMemberView& member) {
  if (!view.i_am_leader || member.is_me) {
    return core::Tr("Seul le chef de groupe peut passer la main.");
  }
  if (member.offline) {
    return core::Tr("Ce membre est hors ligne.");
  }
  const PartyMemberView* me = PartyMe(view);
  if (me != nullptr && me->map_raw != member.map_raw) {
    return core::Tr("Le serveur ne passe la main qu'à un membre sur votre carte.");
  }
  return nullptr;
}

PartyFriendAction DrawPartyFriendWindow(PartyFriendWindowState& state, const PartyFriendView& view,
                                        const PartyFriendLook& look, const SkinArt& art) {
  PartyFriendAction action;
  NoteOptions(state, view, action);
  // HORS DE LA FENÊTRE : une demande reçue se répond fenêtre fermée ou repliée.
  DrawPartyInvite(view, art, action);
  if (!state.open) {
    return action;
  }

  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(std::min(kSpawnX, std::max(0.0F, screen.x - kSpawnWidth)),
                                 std::min(kSpawnY, std::max(0.0F, screen.y - kSpawnHeight))),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kSpawnWidth, kSpawnHeight), ImGuiCond_FirstUseEver);
  // Le plancher se mesure dans la langue courante : les deux onglets et la case
  // de la grille, même sur l'onglet qui la masque, pour ne pas sauter.
  const ImGuiStyle& style = ImGui::GetStyle();
  const float top_row = SkinSmallButtonWidth(core::Tr("Groupe"), art) +
                        SkinSmallButtonWidth(core::Tr("Amis"), art) +
                        SkinCheckBoxWidth(core::Tr("Grille"), art) + style.ItemSpacing.x * 2.0F +
                        style.WindowPadding.x * 2.0F;
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(std::max(Px(kLeastWidthPixels), top_row), Px(kLeastHeightPixels)),
      ImVec2(FLT_MAX, FLT_MAX));
  SetNextWindowBulletClickable(core::Tr("Réglages Groupe / Amis"));
  SetNextWindowPinnable(&state.pinned);
  static WindowName name(kPartyFriendWindowTitle, kPartyFriendWindowId);
  const ImGuiWindowFlags flags = look.lock_size ? ImGuiWindowFlags_NoResize : ImGuiWindowFlags_None;
  const bool body_shown =
      BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar, flags);
  // La puce se lit avant le demi-tour : repliée, la barre répond encore.
  if (TitleBulletClicked()) {
    action.settings_asked = true;
  }
  if (body_shown) {
    DrawBody(state, view, look, art, action);
  }
  EndSkinnedWindow();
  if (!state.open) {
    action.closed = true;
  }
  return action;
}

}  // namespace ro::ui
