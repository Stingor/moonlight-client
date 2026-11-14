// L'ONGLET GUILDE, dessiné image par image sans jeu autour.
//
// 🔴 CE BANC PORTE CE QUI CASSE SANS SE VOIR :
//
//   · une boîte demandée depuis un MENU s'ouvre à l'image suivante, au niveau de
//     l'onglet — ouverte dans le menu, elle naîtrait sous un autre identifiant
//     et ne paraîtrait jamais ;
//   · un clic sur l'emblème ouvre l'éditeur SUR l'emblème en place ;
//   · le tableau des membres range TOUS les membres.

#include "ro/ui/guild_tab.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::GuildAction;
using ro::ui::GuildBanView;
using ro::ui::GuildMemberView;
using ro::ui::GuildPositionView;
using ro::ui::GuildRelationView;
using ro::ui::GuildSkillView;
using ro::ui::GuildTabState;
using ro::ui::GuildVerb;
using ro::ui::GuildView;
using ro::ui::SkillNeedView;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPiecePixels = 8.0F;
constexpr ImVec2 kTabSize(760.0F, 640.0F);

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPiecePixels, .height = kPiecePixels});
  }
  return art;
}

// Une guilde de trois membres, deux postes nommés, une alliance, une
// expulsion et deux compétences dont l'une réclame l'autre.
struct GuildProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  GuildTabState state;
  GuildView view;
  bool smooth = false;
  std::vector<GuildMemberView> members;
  std::array<GuildPositionView, ro::ui::kGuildTabPositions> positions = {};
  std::vector<GuildRelationView> relations;
  std::vector<GuildBanView> bans;
  std::vector<GuildSkillView> skills;
  std::vector<SkillNeedView> needs;

  GuildProbe() {
    view.in_guild = true;
    view.guild_id = 42;
    view.name = "Moonlight";
    view.master_name = "Stingor";
    view.level = 5;
    view.member_count = 3;
    view.member_most = 16;
    view.online = 2;
    view.exp = 500;
    view.next_exp = 1000;
    view.master = true;
    view.position_found = true;
    view.rights_known = true;
    view.may_invite = true;
    view.may_expel = true;
    view.may_storage = true;
    members.push_back({.account_id = 1,
                       .char_id = 11,
                       .name = "Stingor",
                       .job = "Lord Knight",
                       .level = 99,
                       .position = 0,
                       .online = true,
                       .self = true,
                       .master = true});
    members.push_back({.account_id = 2,
                       .char_id = 12,
                       .name = "Bob",
                       .job = "Priest",
                       .level = 80,
                       .position = 3,
                       .position_label = "Handyman"});
    members.push_back({.account_id = 3,
                       .char_id = 13,
                       .name = "alice",
                       .job = "Hunter",
                       .level = 90,
                       .position = 3,
                       .online = true});
    view.members = members;
    positions[0] = {.name = "Guild Master",
                    .may_invite = true,
                    .may_expel = true,
                    .may_storage = true,
                    .has_name = true,
                    .has_rights = true};
    positions[3] = {.name = "Handyman", .pay_rate = 10, .has_name = true, .has_rights = true};
    view.positions = positions;
    relations.push_back({.guild_id = 7, .enemy = false, .name = "Les Porings"});
    relations.push_back({.guild_id = 8, .enemy = true, .name = "Les Drops"});
    view.relations = relations;
    bans.push_back({.name = "Eve", .reason = "trop bavarde"});
    view.bans = bans;
    view.bans_known = true;
    needs.push_back({.skill_id = 10000, .level = 1});
    GuildSkillView approval;
    approval.cell.skill_id = 10000;
    approval.cell.name = "Official Guild Approval";
    approval.cell.learned = 1;
    approval.cell.max_level = 1;
    approval.technical = "GD_APPROVAL";
    GuildSkillView extension;
    extension.cell.skill_id = 10004;
    extension.cell.name = "Guild Extension";
    extension.cell.max_level = 10;
    extension.cell.raisable = true;
    extension.cell.needs = needs;
    extension.depth = 1;
    extension.technical = "GD_EXTENSION";
    skills = {approval, extension};
    view.skills = skills;
    view.skill_points = 2;
    view.skills_known = true;
    view.skill_tree_known = true;
    view.transparency_limit = 80;
  }

  GuildAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowSize(kTabSize);
    ImGui::Begin("banc_guilde");
    GuildAction action = ro::ui::DrawGuildTab(state, view, smooth, art);
    ImGui::End();
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

[[nodiscard]] bool AnyPopupOpen() { return !GImGui->OpenPopupStack.empty(); }

}  // namespace

TEST_CASE("Guilde : le storage ne se colle pas derrière le texte de l'annonce", "[ui][guild]") {
  // 🔴 SIGNALÉ EN JEU : chez un simple membre, le bouton suivait la dernière
  // ligne de l'annonce et partait hors de la fenêtre. Selon la longueur du
  // texte, la dernière ligne est courte ou pleine : on les essaie toutes.
  GuildProbe probe;
  probe.view.master = false;
  probe.view.notice_subject = "blyat";
  float widest = 0.0F;
  float room = 0.0F;
  for (std::size_t length = 40; length <= 200; length += 3) {
    const std::string body(length, 'W');
    probe.view.notice_body = body;
    probe.Settle();
    const ImGuiWindow* window = ImGui::FindWindowByName("banc_guilde");
    REQUIRE(window != nullptr);
    widest = std::max(widest, window->ContentSize.x);
    room = window->ContentRegionRect.GetWidth();
  }
  CHECK(widest <= room + 1.0F);
}

TEST_CASE("Guilde : sans guilde, l'onglet ne demande rien de lui-même", "[ui][guild]") {
  GuildProbe probe;
  probe.view = GuildView();
  probe.view.creation_answer = "Guilde créée.";
  probe.view.creation_succeeded = true;
  for (int frame = 0; frame < 4; ++frame) {
    CHECK(probe.Frame().verb == GuildVerb::kNothing);
  }
}

TEST_CASE("Guilde : le tableau range tous les membres", "[ui][guild]") {
  GuildProbe probe;
  probe.Settle();
  CHECK(probe.state.member_order.size() == probe.members.size());
  CHECK(probe.Frame().verb == GuildVerb::kNothing);
}

TEST_CASE("Guilde : une expulsion demandée d'un menu ouvre sa boîte à l'image suivante",
          "[ui][guild]") {
  GuildProbe probe;
  probe.Settle();
  REQUIRE_FALSE(AnyPopupOpen());
  probe.state.target_account = 2;
  probe.state.target_char = 12;
  probe.state.target_name = "Bob";
  probe.state.ask_expel = true;
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.ask_expel);
  CHECK(AnyPopupOpen());
}

TEST_CASE("Guilde : l'emblème cliqué ouvre l'éditeur sur l'emblème en place", "[ui][guild]") {
  GuildProbe probe;
  probe.Settle();
  probe.state.emblem.ask_open = true;
  probe.state.emblem.load_current = true;
  const GuildAction action = probe.Frame();
  CHECK(action.verb == GuildVerb::kOpenEmblemEditor);
  CHECK(action.from_current);
  CHECK_FALSE(probe.state.emblem.ask_open);
  CHECK(AnyPopupOpen());
  // L'onglet « Dessiner » se dessine dans la boîte, et vide le canevas.
  probe.Settle();
  CHECK(probe.state.emblem.canvas.started);
}

TEST_CASE("Guilde : les compétences se dessinent en grille comme en liste", "[ui][guild]") {
  GuildProbe probe;
  probe.state.skill_grid = true;
  probe.Settle();
  probe.state.skill_grid = false;
  probe.Settle();
  CHECK(probe.Frame().verb == GuildVerb::kNothing);
}

TEST_CASE("Guilde : l'invitation reçue attend sa réponse", "[ui][guild]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  bool accepted = true;
  for (int frame = 0; frame < 3; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    CHECK_FALSE(ro::ui::DrawGuildInvitation("Les Porings", accepted, art));
    ui.EndFrame();
  }
  CHECK(AnyPopupOpen());
}

TEST_CASE("Guilde : l'alliance proposée s'ouvre seule, et Échap la refuse", "[ui][guild]") {
  // 🔴 TOUTE FERMETURE EST UNE RÉPONSE : sans elle, le serveur refuserait
  // toute autre proposition d'alliance jusqu'à la déconnexion.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  bool accepted = true;
  for (int frame = 0; frame < 3; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    CHECK_FALSE(ro::ui::DrawGuildAllianceOffer("Les Porings", accepted, art));
    ui.EndFrame();
  }
  REQUIRE(AnyPopupOpen());
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
  ui.BeginFrame(kViewport, kFrameSeconds);
  const bool answered = ro::ui::DrawGuildAllianceOffer("Les Porings", accepted, art);
  ui.EndFrame();
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
  CHECK(answered);
  CHECK_FALSE(accepted);
}

// ── « Inviter dans mon carnet MVP » ─────────────────────────────────────────

namespace {

constexpr const char* kMvpInviteLabel = "Inviter dans mon carnet MVP";
constexpr const char* kMvpRefusal = "Le carnet de chasse MVP est éteint dans les réglages.";
// Le balayage qui cherche une ligne, puis une entrée : son pas, et la marge
// qui le tient dans le texte.
constexpr float kSweepStepPixels = 3.0F;
constexpr float kSweepInsetPixels = 30.0F;
constexpr int kMenuFrames = 3;

// L'onglet, avec ce que ses fenêtres ont écrit à cette image — menus et
// infobulles compris.
struct LoggedGuild {
  GuildProbe probe;
  std::string logged;
  GuildAction answered;

  GuildAction Frame() {
    probe.ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    ImGui::SetNextWindowSize(kTabSize);
    ImGui::Begin("banc_guilde");
    GuildAction action = ro::ui::DrawGuildTab(probe.state, probe.view, probe.smooth, probe.art);
    ImGui::End();
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    probe.ui.EndFrame();
    if (action.verb != GuildVerb::kNothing) {
      answered = action;
    }
    return action;
  }

  void Frames(int count) {
    for (int frame = 0; frame < count; ++frame) {
      static_cast<void>(Frame());
    }
  }

  void MoveTo(ImVec2 at) {
    ImGui::GetIO().AddMousePosEvent(at.x, at.y);
    static_cast<void>(Frame());
  }

  void Click(ImGuiMouseButton button) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(button, true);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(button, false);
    Frames(kMenuFrames);
  }

  // Ouvre le menu du membre `char_id` : la liste se balaie au clic droit, et
  // un menu ouvert sur un autre se referme.
  bool OpenMenuOf(std::uint32_t char_id) {
    Frames(kMenuFrames);
    const ImGuiWindow* bench = ImGui::FindWindowByName("banc_guilde");
    REQUIRE(bench != nullptr);
    const float x = bench->Pos.x + kSweepInsetPixels;
    for (float y = bench->Pos.y; y < bench->Pos.y + bench->Size.y; y += kSweepStepPixels) {
      MoveTo(ImVec2(x, y));
      Click(ImGuiMouseButton_Right);
      if (AnyPopupOpen() && probe.state.selected_char == char_id) {
        return true;
      }
      if (AnyPopupOpen()) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiKey_Escape, true);
        static_cast<void>(Frame());
        io.AddKeyEvent(ImGuiKey_Escape, false);
        Frames(kMenuFrames);
      }
    }
    return false;
  }

  // Survole les entrées du menu ouvert, de haut en bas, jusqu'à ce que `seen`
  // paraisse dans ce qui s'écrit, ou que l'entrée `label` soit survolée.
  bool HoverEntry(const char* label, const char* seen) {
    REQUIRE(AnyPopupOpen());
    ImGuiWindow* menu = GImGui->OpenPopupStack.back().Window;
    REQUIRE(menu != nullptr);
    const ImGuiID wanted = menu->GetID(label);
    const float x = menu->Pos.x + kSweepInsetPixels;
    for (float y = menu->Pos.y; y < menu->Pos.y + menu->Size.y; y += kSweepStepPixels) {
      MoveTo(ImVec2(x, y));
      static_cast<void>(Frame());
      if (GImGui->HoveredId == wanted ||
          (seen != nullptr && logged.find(seen) != std::string::npos)) {
        return true;
      }
    }
    return false;
  }
};

}  // namespace

TEST_CASE("Guilde : un membre s'invite dans le carnet MVP par son menu", "[ui][guild][mvp]") {
  LoggedGuild bench;
  constexpr std::uint32_t kBob = 12;
  REQUIRE(bench.OpenMenuOf(kBob));
  // OFFERTE : l'entrée est là, et son clic rend le nom du membre.
  CHECK(bench.logged.find(kMvpInviteLabel) != std::string::npos);
  REQUIRE(bench.HoverEntry(kMvpInviteLabel, nullptr));
  // SON INFOBULLE DIT LA RÈGLE.
  bench.Frames(kMenuFrames);
  CHECK(bench.logged.find("Guilde et liste d'amis uniquement.") != std::string::npos);
  bench.Click(ImGuiMouseButton_Left);
  CHECK(bench.answered.verb == GuildVerb::kInviteMvp);
  CHECK(bench.answered.text == "Bob");
}

TEST_CASE("Guilde : refusée, l'invitation au carnet MVP porte sa raison", "[ui][guild][mvp]") {
  LoggedGuild bench;
  for (GuildMemberView& member : bench.probe.members) {
    member.mvp_refusal = kMvpRefusal;
  }
  constexpr std::uint32_t kBob = 12;
  REQUIRE(bench.OpenMenuOf(kBob));
  CHECK(bench.logged.find(kMvpInviteLabel) != std::string::npos);
  // ⚠ GRISÉE, ELLE PARLE AU SURVOL : sa raison, pas la règle.
  CHECK(bench.HoverEntry(kMvpInviteLabel, kMvpRefusal));
  bench.Click(ImGuiMouseButton_Left);
  CHECK(bench.answered.verb != GuildVerb::kInviteMvp);
}

TEST_CASE("Guilde : carnet éteint, l'invitation au carnet MVP n'est pas offerte",
          "[ui][guild][mvp]") {
  LoggedGuild bench;
  bench.probe.view.mvp_invites_withdrawn = true;
  constexpr std::uint32_t kBob = 12;
  REQUIRE(bench.OpenMenuOf(kBob));
  CHECK(bench.logged.find(kMvpInviteLabel) == std::string::npos);
  CHECK(bench.logged.find("Envoyer un courrier") != std::string::npos);
}

TEST_CASE("Guilde : on ne s'invite pas soi-même dans son carnet MVP", "[ui][guild][mvp]") {
  LoggedGuild bench;
  constexpr std::uint32_t kSelfChar = 11;
  REQUIRE(bench.OpenMenuOf(kSelfChar));
  CHECK(bench.logged.find(kMvpInviteLabel) == std::string::npos);
}
