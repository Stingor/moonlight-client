// LA FENÊTRE AMIS / GROUPE, dessinée image par image sans jeu autour.
//
// Ce banc porte les règles qui se trompent sans se voir — la carte courte, le
// pourcentage d'une jauge, la raison d'un « -EXP », les droits du chef — et les
// gestes qui ne rendent leur intention qu'au bon endroit : le clic droit d'une
// ligne qui a un sprite, le clic gauche qui cible.

#include "ro/ui/party_friend_window.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::FriendView;
using ro::ui::PartyFriendAction;
using ro::ui::PartyFriendLook;
using ro::ui::PartyFriendTab;
using ro::ui::PartyFriendVerb;
using ro::ui::PartyFriendView;
using ro::ui::PartyFriendWindowState;
using ro::ui::PartyMemberView;
using ro::ui::StatusCellView;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPiecePixels = 8.0F;
constexpr float kScanStepPixels = 3.0F;
constexpr float kScanColumnPixels = 60.0F;
constexpr const char* kWindowId = "###party_friends";

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPiecePixels, .height = kPiecePixels});
  }
  return art;
}

// Un groupe de trois : le joueur, chef, à Prontera ; Bob à portée, avec un
// sprite ; Alice hors ligne. Deux amis, dont un en ligne.
struct PartyProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  PartyFriendWindowState state;
  PartyFriendView view;
  PartyFriendLook look;
  std::array<PartyMemberView, 3> members = {};
  std::array<FriendView, 2> friends = {};
  std::array<StatusCellView, 2> statuses = {};

  PartyProbe() {
    state.open = true;
    const ro::ui::SkinImage picture{.id = 1, .width = 32.0F, .height = 32.0F};
    members[0] = {.account_id = 1,
                  .char_id = 11,
                  .name = "Stingor",
                  .map_raw = "prontera.gat",
                  .map_name = "Prontera, Capital of Rune-Midgard",
                  .leader = true,
                  .is_me = true,
                  .job_name = "Lord Knight",
                  .base_level = 99,
                  .job_icon = picture,
                  .position_known = true,
                  .x = 150,
                  .y = 180,
                  .minimap = picture,
                  .map_width_cells = 300,
                  .map_height_cells = 300,
                  .hp_known = true,
                  .hp = 500,
                  .max_hp = 1000,
                  .sp_known = true,
                  .sp = 50,
                  .max_sp = 100};
    statuses[0].abbreviation = "BL";
    statuses[1].abbreviation = "AG";
    members[1] = {.account_id = 2,
                  .char_id = 12,
                  .name = "Bob",
                  .map_raw = "prontera.gat",
                  .job_name = "Priest",
                  .base_level = 80,
                  .job_icon = picture,
                  .actor_present = true,
                  .hp_known = true,
                  .hp = 1,
                  .max_hp = 1000,
                  .share_flags = ro::ui::kPartyShareIdle,
                  .statuses = statuses};
    members[2] = {.account_id = 3,
                  .char_id = 13,
                  .name = "Alice",
                  .map_raw = "geffen.gat",
                  .offline = true,
                  .job_name = "Hunter",
                  .base_level = 90};
    friends[0] = {.account_id = 4, .char_id = 14, .name = "Eve", .online = true};
    friends[1] = {.account_id = 5, .char_id = 15, .name = "Zed"};
    view.in_party = true;
    view.i_am_leader = true;
    view.party_name = "Moonlight";
    view.options = {.known = true, .exp = 1, .pickup = 0, .share = 1};
    view.share_known = true;
    view.share_idle_seconds = 120;
    view.members = members;
    view.friends = friends;
    view.crown = picture;
    view.grid_offered = true;
  }

  PartyFriendAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    PartyFriendAction action = ro::ui::DrawPartyFriendWindow(state, view, look, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Presse et relâche `button` au point donné ; rend la première intention.
  PartyFriendAction Click(ImVec2 at, ImGuiMouseButton button) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(at.x, at.y);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(button, true);
    PartyFriendAction pressed = Frame();
    io.AddMouseButtonEvent(button, false);
    const PartyFriendAction released = Frame();
    return pressed.verb != PartyFriendVerb::kNothing ? pressed : released;
  }
};

[[nodiscard]] bool AnyPopupOpen() { return !GImGui->OpenPopupStack.empty(); }

// Remonte la fenêtre depuis le bas, un clic tous les trois pixels, jusqu'à ce
// qu'un clic rende `wanted`. Par le bas : les lignes sont sous les boutons.
[[nodiscard]] bool ScanUpFor(PartyProbe& probe, ImGuiMouseButton button, PartyFriendVerb wanted,
                             PartyFriendAction* found) {
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowId);
  REQUIRE(window != nullptr);
  const float x = window->Pos.x + kScanColumnPixels;
  for (float y = window->Pos.y + window->Size.y - 1.0F; y > window->Pos.y; y -= kScanStepPixels) {
    const PartyFriendAction action = probe.Click(ImVec2(x, y), button);
    if (action.verb == wanted) {
      *found = action;
      return true;
    }
    if (AnyPopupOpen()) {
      ImGui::ClosePopupsExceptModals();
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Amis/Groupe : le nom court s'arrête à la virgule ou à la parenthèse",
          "[ui][party_window]") {
  CHECK(ro::ui::ShortMapName("Gonryun, the Hermit Land (Kunlun)") == "Gonryun");
  CHECK(ro::ui::ShortMapName("Juperos (Ruins)") == "Juperos");
  CHECK(ro::ui::ShortMapName("Prontera") == "Prontera");
  CHECK(ro::ui::ShortMapName("") == "");
}

TEST_CASE("Amis/Groupe : une jauge ne dit jamais zéro pour cent d'un vivant",
          "[ui][party_window]") {
  using ro::ui::PartyGaugeText;
  CHECK(ro::ui::PartyGaugeCaption(1, 1000, static_cast<int>(PartyGaugeText::kPercent)) == "1 %");
  CHECK(ro::ui::PartyGaugeCaption(0, 1000, static_cast<int>(PartyGaugeText::kPercent)) == "0 %");
  CHECK(ro::ui::PartyGaugeCaption(500, 1000, static_cast<int>(PartyGaugeText::kNumbers)) ==
        "500/1000");
  CHECK(ro::ui::PartyGaugeCaption(500, 1000, static_cast<int>(PartyGaugeText::kBoth)) ==
        "500/1000 (50 %)");
  CHECK(ro::ui::PartyGaugeCaption(500, 1000, static_cast<int>(PartyGaugeText::kNone)).empty());
  CHECK(ro::ui::PartyGaugeCaption(5, 0, static_cast<int>(PartyGaugeText::kNumbers)).empty());
}

TEST_CASE("Amis/Groupe : le partage d'EXP croise le serveur et la carte", "[ui][party_window]") {
  PartyProbe probe;
  CHECK(ro::ui::PartyShareFlags(probe.view, probe.members[0]) == 0);
  CHECK(ro::ui::PartyShareFlags(probe.view, probe.members[1]) == ro::ui::kPartyShareIdle);
  // Hors ligne, la pastille dit déjà tout.
  CHECK(ro::ui::PartyShareFlags(probe.view, probe.members[2]) == 0);
  probe.members[2].offline = false;
  CHECK(ro::ui::PartyShareFlags(probe.view, probe.members[2]) == ro::ui::kPartyShareOtherMap);
  // Tant que le serveur n'a rien dit, la fenêtre se tait.
  probe.view.share_known = false;
  CHECK(ro::ui::PartyShareFlags(probe.view, probe.members[1]) == 0);

  CHECK(ro::ui::PartyShareReason(0, 120).empty());
  CHECK(ro::ui::PartyShareReason(ro::ui::kPartyShareIdle | ro::ui::kPartyShareDead, 120)
            .find("120") != std::string::npos);
  CHECK(ro::ui::PartyShareReason(ro::ui::kPartyShareDead | ro::ui::kPartyShareOtherMap, 0) ==
        ro::ui::PartyShareReason(ro::ui::kPartyShareDead, 0));
}

TEST_CASE("Amis/Groupe : passer la main exige un membre en ligne sur la carte du chef",
          "[ui][party_window]") {
  PartyProbe probe;
  CHECK(ro::ui::PartyLeaderRefusal(probe.view, probe.members[1]) == nullptr);
  CHECK(ro::ui::PartyLeaderRefusal(probe.view, probe.members[0]) != nullptr);
  CHECK(ro::ui::PartyLeaderRefusal(probe.view, probe.members[2]) != nullptr);
  probe.members[2].offline = false;
  CHECK(ro::ui::PartyLeaderRefusal(probe.view, probe.members[2]) != nullptr);
  probe.view.i_am_leader = false;
  CHECK(ro::ui::PartyLeaderRefusal(probe.view, probe.members[1]) != nullptr);
}

TEST_CASE("Amis/Groupe : les deux onglets se dessinent, vides comme pleins", "[ui][party_window]") {
  PartyProbe probe;
  probe.look.show_buffs = true;
  probe.look.show_sp = true;
  probe.look.bars_stacked = true;
  probe.look.head_mode = ro::ui::kPartyHeadInParty | ro::ui::kPartyHeadInFriends;
  probe.Settle();
  CHECK(ImGui::FindWindowByName(kWindowId) != nullptr);
  probe.state.tab = PartyFriendTab::kFriends;
  probe.Settle();
  probe.look.text_in_bars = true;
  probe.look.map_mode = static_cast<int>(ro::ui::PartyMapMode::kShort);
  probe.state.tab = PartyFriendTab::kParty;
  probe.Settle();

  probe.view = PartyFriendView();
  probe.Settle();
  probe.state.tab = PartyFriendTab::kFriends;
  probe.Settle();
  CHECK(probe.Frame().verb == PartyFriendVerb::kNothing);
}

TEST_CASE("Amis/Groupe : un changement de réglage s'annonce, le premier relevé non",
          "[ui][party_window]") {
  PartyProbe probe;
  probe.state.open = false;
  CHECK(probe.Frame().chat_line.empty());
  CHECK(probe.state.opt_share == 1);
  CHECK(probe.Frame().chat_line.empty());
  probe.view.options.pickup = 1;
  const PartyFriendAction changed = probe.Frame();
  CHECK_FALSE(changed.chat_line.empty());
  CHECK(probe.state.opt_pickup == 1);
  CHECK(probe.Frame().chat_line.empty());
  // Hors groupe, rien ne s'annonce.
  probe.view.in_party = false;
  probe.view.options.exp = 0;
  CHECK(probe.Frame().chat_line.empty());
}

TEST_CASE("Amis/Groupe : la demande reçue s'ouvre fenêtre fermée et se répond",
          "[ui][party_window]") {
  PartyProbe probe;
  probe.state.open = false;
  probe.view.invite = {.active = true, .is_friend = false, .party_id = 77, .name = "Porings"};
  probe.Settle();
  CHECK(AnyPopupOpen());
  CHECK(ImGui::FindWindowByName(kWindowId) == nullptr);
  probe.view.invite.active = false;
  probe.Settle();
  CHECK_FALSE(AnyPopupOpen());
}

TEST_CASE("Amis/Groupe : une confirmation demandée d'un menu s'ouvre à l'image suivante",
          "[ui][party_window]") {
  PartyProbe probe;
  probe.Settle();
  REQUIRE_FALSE(AnyPopupOpen());
  probe.state.ask_confirm = true;
  probe.state.confirm = PartyFriendVerb::kExpel;
  probe.state.confirm_account = 2;
  probe.state.confirm_name = "Bob";
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.ask_confirm);
  CHECK(AnyPopupOpen());
}

TEST_CASE("Amis/Groupe : le clic droit sur un membre visible ouvre le menu d'entité",
          "[ui][party_window]") {
  PartyProbe probe;
  probe.look.show_tooltip = false;
  probe.Settle();
  PartyFriendAction found;
  REQUIRE(ScanUpFor(probe, ImGuiMouseButton_Right, PartyFriendVerb::kEntityMenu, &found));
  CHECK(found.account_id == 2);
  CHECK(found.name == "Bob");
}

TEST_CASE("Amis/Groupe : le clic gauche cible, seulement si le réglage le veut",
          "[ui][party_window]") {
  PartyFriendAction found;
  {
    PartyProbe untouched;
    untouched.look.show_tooltip = false;
    untouched.Settle();
    CHECK_FALSE(ScanUpFor(untouched, ImGuiMouseButton_Left, PartyFriendVerb::kTarget, &found));
  }
  PartyProbe probe;
  probe.look.show_tooltip = false;
  probe.look.click_targets = true;
  probe.Settle();
  REQUIRE(ScanUpFor(probe, ImGuiMouseButton_Left, PartyFriendVerb::kTarget, &found));
  // Alice est hors ligne : la première ligne ciblable par le bas est Bob.
  CHECK(found.account_id == 2);
}

namespace {

constexpr const char* kMvpInviteLabel = "Inviter dans mon carnet MVP";
constexpr const char* kMvpRefusal = "Ni dans ta guilde ni dans tes amis.";

// Ouvre le menu de la ligne la plus basse : un clic droit tous les trois
// pixels, en remontant, jusqu'à ce qu'un menu s'ouvre.
[[nodiscard]] bool OpenLowestRowMenu(PartyProbe& probe) {
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowId);
  REQUIRE(window != nullptr);
  const float x = window->Pos.x + kScanColumnPixels;
  for (float y = window->Pos.y + window->Size.y - 1.0F; y > window->Pos.y; y -= kScanStepPixels) {
    static_cast<void>(probe.Click(ImVec2(x, y), ImGuiMouseButton_Right));
    if (AnyPopupOpen()) {
      return true;
    }
  }
  return false;
}

// Le point de l'entrée `label` du menu ouvert, trouvé au survol ; rien si le
// menu ne la porte pas. ⚠ UNE ENTRÉE GRISÉE SE SURVOLE AUSSI : ImGui pose son
// identifiant survolé, sans la rendre cliquable.
[[nodiscard]] std::optional<ImVec2> FindMenuEntry(PartyProbe& probe, const char* label) {
  REQUIRE(AnyPopupOpen());
  const ImGuiWindow* menu = GImGui->OpenPopupStack.back().Window;
  REQUIRE(menu != nullptr);
  const ImGuiID wanted = ImHashStr(label, 0, menu->ID);
  ImGuiIO& io = ImGui::GetIO();
  const float x = menu->Pos.x + kScanColumnPixels;
  for (float y = menu->Pos.y + 1.0F; y < menu->Pos.y + menu->Size.y; y += kScanStepPixels) {
    io.AddMousePosEvent(x, y);
    static_cast<void>(probe.Frame());
    if (GImGui->HoveredId == wanted) {
      return ImVec2(x, y);
    }
  }
  return std::nullopt;
}

}  // namespace

TEST_CASE("Amis/Groupe : « Inviter dans mon carnet MVP » rend le nom du membre",
          "[ui][party_window]") {
  PartyProbe probe;
  probe.look.show_tooltip = false;
  probe.Settle();
  // La ligne la plus basse est Alice : hors ligne, sans sprite, donc ce menu.
  REQUIRE(OpenLowestRowMenu(probe));
  const std::optional<ImVec2> entry = FindMenuEntry(probe, kMvpInviteLabel);
  REQUIRE(entry.has_value());
  const PartyFriendAction action = probe.Click(*entry, ImGuiMouseButton_Left);
  CHECK(action.verb == PartyFriendVerb::kInviteMvp);
  CHECK(action.name == "Alice");
}

TEST_CASE("Amis/Groupe : refusée par le carnet, l'invitation est grisée et ne rend rien",
          "[ui][party_window]") {
  PartyProbe probe;
  probe.look.show_tooltip = false;
  probe.members[2].mvp_refusal = kMvpRefusal;
  probe.Settle();
  REQUIRE(OpenLowestRowMenu(probe));
  const std::optional<ImVec2> entry = FindMenuEntry(probe, kMvpInviteLabel);
  REQUIRE(entry.has_value());
  CHECK(GImGui->HoveredIdIsDisabled);
  const PartyFriendAction action = probe.Click(*entry, ImGuiMouseButton_Left);
  CHECK(action.verb == PartyFriendVerb::kNothing);
}

TEST_CASE("Amis/Groupe : carnet éteint, l'invitation au carnet n'est dans aucun menu",
          "[ui][party_window]") {
  for (const PartyFriendTab tab : {PartyFriendTab::kParty, PartyFriendTab::kFriends}) {
    INFO(static_cast<int>(tab));
    PartyProbe probe;
    probe.look.show_tooltip = false;
    probe.state.tab = tab;
    probe.view.mvp_invites_withdrawn = true;
    probe.Settle();
    REQUIRE(OpenLowestRowMenu(probe));
    CHECK_FALSE(FindMenuEntry(probe, kMvpInviteLabel).has_value());
    CHECK(FindMenuEntry(probe, "Copier le nom").has_value());
  }
}

TEST_CASE("Amis/Groupe : un ami s'invite aussi dans le carnet MVP", "[ui][party_window]") {
  PartyProbe probe;
  probe.state.tab = PartyFriendTab::kFriends;
  probe.Settle();
  // Le dernier ami est Zed, hors ligne.
  REQUIRE(OpenLowestRowMenu(probe));
  const std::optional<ImVec2> entry = FindMenuEntry(probe, kMvpInviteLabel);
  REQUIRE(entry.has_value());
  const PartyFriendAction action = probe.Click(*entry, ImGuiMouseButton_Left);
  CHECK(action.verb == PartyFriendVerb::kInviteMvp);
  CHECK(action.name == "Zed");
}
