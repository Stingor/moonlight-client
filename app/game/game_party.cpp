// LE GROUPE ET LES AMIS DANS LA PARTIE : la fenêtre Groupe / Amis, la grille de
// groupe, et ce que leurs gestes demandent au serveur ou au reste du jeu.

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "ro/net/friends.h"
#include "ro/net/party.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "chat/player_link_judge.h"
#include "game/game.h"
#include "options/option_sections.h"
#include "world/click_action.h"
#include "world/map_stage.h"

namespace moonclient {
namespace {

// Une couleur d'ImGui (ABGR) en couleur de conversation (0xRRGGBB).
[[nodiscard]] std::uint32_t ChatColorOf(ImU32 color) {
  constexpr unsigned kChannelBits = 8;
  constexpr std::uint32_t kChannelMask = 0xFF;
  const std::uint32_t red = color & kChannelMask;
  const std::uint32_t green = (color >> kChannelBits) & kChannelMask;
  const std::uint32_t blue = (color >> (2 * kChannelBits)) & kChannelMask;
  return (red << (2 * kChannelBits)) | (green << kChannelBits) | blue;
}

// Le nom part dans la case du destinataire, et le clavier dans la phrase : la
// barre se déplie, comme pour un lien qu'on vient de poser.
void WhisperTo(ro::ui::ChatInputState& input, std::string_view name) {
  if (name.empty()) {
    return;
  }
  std::array<char, ro::ui::kChatWhisperNameSize>& box = input.whisper_to;
  box.fill('\0');
  std::copy_n(name.begin(), std::min(name.size(), box.size() - 1), box.begin());
  input.unfolded = true;
  input.focus_on_whisper = false;
  input.wants_focus = true;
}

}  // namespace

void Game::RefreshParty(std::uint32_t now_ms) {
  PartyOptions& page = settings_->options().pages().party;
  ro::ui::PartyFriendWindowState& window = windows_.party.window;
  if (page.NoteTab(window.tab)) {
    settings_->file().Touch();
  }
  window.buff_preview = page.window_preview();
  const ro::sim::WalkMap* walk = stage_.has_value() ? PlannedMapOf(*stage_) : nullptr;
  // « INVITER DANS MON CARNET MVP » : le carnet seul sait qui peut y entrer ;
  // éteint, l'entrée disparaît.
  party_panel_->WithdrawsMvpInvites(!settings_->options().pages().mvp_tracker.settings().enabled);
  party_panel_->JudgesMvpInvites([this](std::string_view name, bool same_account) {
    return JudgeMvpInvite(
        name,
        MvpInviteCircle{.book = pieces_.mvp_tracker.book(),
                        .guild = guild_book_,
                        .friends = friends_,
                        .enabled = settings_->options().pages().mvp_tracker.settings().enabled,
                        .link_ready = pieces_.mvp_tracker.link_ready()},
        same_account);
  });
  party_panel_->Refresh(party_, friends_, world_, page, window,
                        PartyFrameFacts{.char_id = ticket_.character_id,
                                        .map_width_cells = walk != nullptr ? walk->width() : 0,
                                        .map_height_cells = walk != nullptr ? walk->height() : 0,
                                        .now_ms = now_ms});
  if (link_.is_open()) {
    party_panel_->Poll(link_, party_, page, window, world_.player_id(), now_ms);
  }
  windows_.party.view = &party_panel_->window();
  windows_.party.look = &page.window();
  windows_.party.grid_view = &party_panel_->grid();
  windows_.party.grid_look = &page.grid();
}

void Game::ApplyPartyAnswers(std::uint32_t now_ms) {
  PartyOptions& page = settings_->options().pages().party;
  const ro::ui::PartyFriendAction asked = std::exchange(windows_.party.answer, {});
  ro::ui::PartyGridAnswer tile = std::exchange(windows_.party.grid_answer, {});
  // Les faux membres du mode réglage n'existent pas chez le serveur.
  if (IsFakeMember(tile.cast_account_id)) {
    tile.cast_account_id = 0;
  }
  if (IsFakeMember(tile.target_account_id)) {
    tile.target_account_id = 0;
  }
  if (IsFakeMember(tile.verb_account_id)) {
    tile.verb = ro::ui::PartyGridVerb::kNone;
  }
  const std::uint32_t own = world_.player_id();
  party_panel_->NoteCast(tile.cast_account_id);
  if (tile.moved) {
    settings_->file().Touch();
  }
  if (asked.settings_asked) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kPartyFriend);
  }
  if (asked.grid_toggled) {
    page.grid().shown = asked.grid_enabled;
    settings_->file().Touch();
  }
  if (!asked.chat_line.empty()) {
    world_.RecordChat(ro::sim::ChatKind::kParty, ro::sim::ChatType::kPartySetup, 0, asked.chat_line,
                      now_ms, ChatColorOf(ro::ui::kPartyOptionsChatColor));
  }
  if (std::exchange(windows_.party.disband_confirmed, false) && link_.is_open()) {
    ReportRefusal(ro::net::RequestPartyLeave(link_));
  }
  if (!link_.is_open()) {
    return;
  }

  // Un membre qu'un acteur représente : son menu d'entité, ou la désignation.
  const auto open_menu = [this, own, now_ms](std::uint32_t account_id) {
    if (const ro::sim::Actor* actor = world_.Find(account_id); actor != nullptr) {
      OpenEntityMenu(*actor, ClassifyEntity(*actor, own), now_ms);
    }
  };
  switch (asked.verb) {
    case ro::ui::PartyFriendVerb::kNothing:
      break;
    case ro::ui::PartyFriendVerb::kWhisper:
      WhisperTo(windows_.chat.main.input, asked.name);
      break;
    case ro::ui::PartyFriendVerb::kWhisperWindow:
      pieces_.whispers.Open(asked.name);
      break;
    case ro::ui::PartyFriendVerb::kMakeLeader:
      ReportRefusal(ro::net::RequestPartyLeader(link_, asked.account_id));
      break;
    case ro::ui::PartyFriendVerb::kExpel:
      ReportRefusal(ro::net::RequestPartyExpel(link_, asked.account_id, asked.name));
      break;
    case ro::ui::PartyFriendVerb::kLeave:
      windows_.party.ask_disband = LeaveParty(link_, party_, own);
      break;
    case ro::ui::PartyFriendVerb::kRemoveFriend:
      ReportRefusal(ro::net::RequestFriendRemoval(link_, asked.account_id, asked.char_id));
      break;
    case ro::ui::PartyFriendVerb::kCreateParty:
      ReportRefusal(ro::net::RequestPartyCreation(link_, asked.name, false, false));
      break;
    case ro::ui::PartyFriendVerb::kInvite:
      ReportRefusal(ro::net::RequestPartyInvite(link_, asked.name));
      break;
    case ro::ui::PartyFriendVerb::kAddFriend:
      ReportRefusal(ro::net::RequestFriend(link_, asked.name));
      break;
    case ro::ui::PartyFriendVerb::kAnswerParty:
      ReportRefusal(ro::net::AnswerPartyInvitation(link_, asked.party_id, asked.accept));
      party_.ForgetInvitation();
      break;
    case ro::ui::PartyFriendVerb::kAnswerFriend:
      ReportRefusal(
          ro::net::AnswerFriendRequest(link_, asked.account_id, asked.char_id, asked.accept));
      friends_.ForgetInvitation();
      break;
    case ro::ui::PartyFriendVerb::kPartyOptions:
      ReportRefusal(ro::net::RequestPartyOptions(
          link_, ro::sim::PartyOptions{.exp_shared = asked.exp == 1,
                                       .pickup_shared = asked.pickup == 1,
                                       .loot_shared = asked.share == 1}));
      break;
    case ro::ui::PartyFriendVerb::kTarget:
      // Par le chemin d'un clic sur l'entité : sans effet hors du mode Ciblage.
      static_cast<void>(targeting_.Designate(world_, asked.account_id));
      break;
    case ro::ui::PartyFriendVerb::kEntityMenu:
      open_menu(asked.account_id);
      break;
    case ro::ui::PartyFriendVerb::kInviteMvp:
      pieces_.mvp_tracker.Invite(asked.name, link_);
      break;
  }

  if (tile.target_account_id != 0) {
    static_cast<void>(targeting_.Designate(world_, tile.target_account_id));
  }
  switch (tile.verb) {
    case ro::ui::PartyGridVerb::kNone:
      break;
    case ro::ui::PartyGridVerb::kEntityMenu:
      open_menu(tile.verb_account_id);
      break;
    case ro::ui::PartyGridVerb::kWhisper:
      WhisperTo(windows_.chat.main.input, tile.verb_name);
      break;
    case ro::ui::PartyGridVerb::kWhisperWindow:
      pieces_.whispers.Open(tile.verb_name);
      break;
    case ro::ui::PartyGridVerb::kMakeLeader:
      ReportRefusal(ro::net::RequestPartyLeader(link_, tile.verb_account_id));
      break;
    case ro::ui::PartyGridVerb::kKick:
      ReportRefusal(ro::net::RequestPartyExpel(link_, tile.verb_account_id, tile.verb_name));
      break;
  }
}

}  // namespace moonclient
