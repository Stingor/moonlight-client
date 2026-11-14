// LE CARNET DE CHASSE MVP, dessiné hors jeu.
//
// Ce qui se vérifie sans écran : ce que la fenêtre dessine selon l'état du
// carnet, la modale d'invitation, le groupe, la hauteur des rangs que le
// découpage suppose, l'ordre de tri, le glisser d'un rang hors de la fenêtre,
// les lignes détachées et leur aimantage, le rang visé, le bandeau d'alerte, et
// les gestes sur le nom d'un MVP.

#include "ro/ui/mvp_tracker_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

#include "mvp_tracker_probe.h"

using namespace mvp_tracker_probe;

TEST_CASE("Carnet MVP : fermé, rien que les surcouches", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  const MvpTrackerAction action = probe.Frame();
  CHECK(Window(kTracker) == nullptr);
  CHECK(action.pin_slot == kMvpNoSlotView);
  CHECK(action.removed_line == -1);
  CHECK_FALSE(action.open_settings);
}

TEST_CASE("Carnet MVP : éteint, rien ne se dessine, et l'invitation montrée se referme",
          "[ui][mvp]") {
  TrackerProbe probe;
  probe.view.group_id = 0;
  probe.view.pending_invite = 42;
  probe.view.pending_invite_name = "Les Faucheurs";
  const std::array<ro::ui::MvpAlertView, 1> alerts = {
      {{.slot_id = 0, .label = "Baphomet", .map = "prt_maze03"}}};
  probe.view.alerts = alerts;
  probe.lines = {MvpPinnedLineView{
      .line = 0, .slot_id = 0, .mob_id = 1000, .map = "prt_maze03", .x = 100, .y = 100}};
  constexpr const char* kLine = "##mvpline_1000_prt_maze03";
  probe.Settle();
  // ALLUMÉ : la fenêtre, le bandeau, la ligne et la modale.
  REQUIRE(Shown("###mvp_invite"));
  REQUIRE(Shown(kTracker));
  REQUIRE(Shown("##mvp_alert"));
  REQUIRE(Shown(kLine));

  probe.view.enabled = false;
  probe.Settle();
  CHECK_FALSE(Shown("###mvp_invite"));
  CHECK_FALSE(Shown(kTracker));
  CHECK_FALSE(Shown("##mvp_alert"));
  CHECK_FALSE(Shown(kLine));
  // ⚠ L'ÉTAT DE LA FENÊTRE EST À `app` : `ui` ne le baisse pas, il ne dessine
  // seulement plus.
  CHECK(probe.state.open);

  // RALLUMÉ, tout revient.
  probe.view.enabled = true;
  probe.Settle();
  CHECK(Shown("###mvp_invite"));
  CHECK(Shown(kLine));
}

TEST_CASE("Carnet MVP : une invitation s'impose, fenêtre fermée", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.view.group_id = 0;
  probe.view.pending_invite = 42;
  probe.view.pending_invite_name = "Les Faucheurs";
  probe.Settle();
  REQUIRE(Shown("###mvp_invite"));
  CHECK(Window(kTracker) == nullptr);

  const std::optional<ImVec2> decline = FindButton(probe, "###mvp_invite", "Refuser");
  REQUIRE(decline.has_value());
  const Click click = ClickAt(probe, *decline);
  CHECK(click.released.decline_invite);
  CHECK_FALSE(click.released.accept_invite);
  // Répondue, elle attend le serveur : elle ne part qu'avec l'invitation.
  probe.Settle();
  CHECK(Shown("###mvp_invite"));
  probe.view.pending_invite = 0;
  probe.Settle();
  CHECK_FALSE(Shown("###mvp_invite"));
  // Une NOUVELLE invitation, elle, s'ouvre.
  probe.view.pending_invite = 43;
  probe.Settle();
  CHECK(Shown("###mvp_invite"));
}

TEST_CASE("Carnet MVP : Échap ne congédie pas l'invitation", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.view.pending_invite = 42;
  probe.view.pending_invite_name = "Les Faucheurs";
  probe.Settle();
  REQUIRE(Shown("###mvp_invite"));
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiKey_Escape, true);
  static_cast<void>(probe.Frame());
  io.AddKeyEvent(ImGuiKey_Escape, false);
  probe.Settle();
  CHECK(Shown("###mvp_invite"));
}

TEST_CASE("Carnet MVP : déjà dans un groupe, « Accepter » est grisé", "[ui][mvp]") {
  for (const bool already : {true, false}) {
    INFO(already);
    TrackerProbe probe;
    probe.state.open = false;
    probe.view.group_id = already ? kGroup : 0;
    probe.view.pending_invite = 42;
    probe.view.pending_invite_name = "Les Faucheurs";
    probe.Settle();
    const std::optional<ImVec2> decline = FindButton(probe, "###mvp_invite", "Refuser");
    REQUIRE(decline.has_value());
    // « Accepter » est à gauche de « Refuser », sur la même ligne.
    const ImGuiWindow* modal = Window("###mvp_invite");
    bool accepted = false;
    for (float x = modal->Pos.x + 2.0F; x < decline->x - kScanStepX && !accepted; x += kScanStepX) {
      accepted = ClickAt(probe, ImVec2(x, decline->y)).released.accept_invite;
    }
    CHECK(accepted == !already);
  }
}

TEST_CASE("Carnet MVP : sans catalogue ni groupe, pas de table", "[ui][mvp]") {
  TrackerProbe waiting;
  waiting.view.catalog_known = false;
  waiting.Settle();
  REQUIRE(Shown(kTracker));
  CHECK(SlotTable() == nullptr);
}

TEST_CASE("Carnet MVP : sans groupe, « Créer » ne part qu'avec un nom", "[ui][mvp]") {
  TrackerProbe probe;
  probe.view.group_id = 0;
  probe.Settle();
  CHECK(SlotTable() == nullptr);
  const std::optional<ImVec2> create = FindButton(probe, kTracker, "Créer");
  REQUIRE(create.has_value());
  CHECK(ClickAt(probe, *create).released.create_group.empty());
  probe.state.group_name = {'B', 'a', 'p', 'h', 'o'};
  CHECK(ClickAt(probe, *create).released.create_group == "Bapho");
  CHECK(probe.state.group_name[0] == '\0');
}

TEST_CASE("Carnet MVP : « Exclure » sur la première ligne d'un autre compte", "[ui][mvp]") {
  TrackerProbe probe;
  probe.members = {MvpMemberView{.user_id = 1, .level = 99, .online = true, .name = "Ann"},
                   MvpMemberView{.user_id = 1, .level = 50, .name = "Ann2"},
                   MvpMemberView{.user_id = 2, .level = 80, .online = true, .name = "Bob"},
                   MvpMemberView{.user_id = 2, .level = 60, .name = "Bob2"},
                   MvpMemberView{.user_id = 3, .level = 70, .name = "Cid"},
                   // Un compte sans personnage : aucun nom à exclure.
                   MvpMemberView{.user_id = 4}};
  probe.Settle();
  REQUIRE(SlotTable() != nullptr);
  // L'en-tête du groupe est REPLIÉ par défaut.
  CHECK(TableOf(kTracker, "##mvp_members") == nullptr);
  const std::optional<ImVec2> header = FindButton(probe, kTracker, "###mvp_group");
  REQUIRE(header.has_value());
  static_cast<void>(ClickAt(probe, *header));
  probe.Settle();
  const ImGuiTable* members = TableOf(kTracker, "##mvp_members");
  REQUIRE(members != nullptr);
  std::set<std::string> kicked;
  const float x = members->Columns[3].WorkMinX + 4.0F;
  for (float y = members->OuterRect.Min.y; y < members->OuterRect.Max.y; y += kScanStepY) {
    if (const std::string name = ClickAt(probe, ImVec2(x, y)).released.kick_member; !name.empty()) {
      kicked.insert(name);
    }
  }
  CHECK(kicked == std::set<std::string>{"Bob", "Cid"});
  // 🔴 LA LIGNE DU COMPTE SANS PERSONNAGE, la dernière, n'offre aucun bouton :
  // rien ne s'y survole.
  REQUIRE(members->RowPosY2 > members->RowPosY1);
  bool offered = false;
  for (float y = members->RowPosY1 + 1.0F; y < members->RowPosY2; y += 1.0F) {
    ImGui::GetIO().AddMousePosEvent(x, y);
    static_cast<void>(probe.Frame());
    offered = offered || GImGui->HoveredId != 0;
  }
  CHECK_FALSE(offered);
}

TEST_CASE("Carnet MVP : un nom accentué de 23 caractères se tape en entier", "[ui][mvp]") {
  TrackerProbe probe;
  probe.members = {MvpMemberView{.user_id = 1, .level = 99, .online = true, .name = "Ann"}};
  probe.Settle();
  const std::optional<ImVec2> header = FindButton(probe, kTracker, "###mvp_group");
  REQUIRE(header.has_value());
  static_cast<void>(ClickAt(probe, *header));
  probe.Settle();
  const std::optional<ImVec2> field = FindButton(probe, kTracker, "##mvp_invite");
  REQUIRE(field.has_value());
  static_cast<void>(ClickAt(probe, *field));
  // 23 caractères du fil, deux octets chacun en UTF-8.
  std::string name;
  for (std::size_t letter = 0; letter < ro::ui::kMvpNameMostWireChars; ++letter) {
    name += "é";
  }
  ImGui::GetIO().AddInputCharactersUTF8(name.c_str());
  probe.Settle();
  CHECK(std::string(probe.state.invite_name.data()) == name);
  const std::optional<ImVec2> invite = FindButton(probe, kTracker, "Inviter");
  REQUIRE(invite.has_value());
  CHECK(ClickAt(probe, *invite).released.invite_member == name);
}

TEST_CASE("Carnet MVP : « Quitter le groupe ? » ne survit pas au groupe", "[ui][mvp]") {
  TrackerProbe probe;
  probe.members = {MvpMemberView{.user_id = 1, .level = 99, .online = true, .name = "Ann"}};
  probe.Settle();
  const std::optional<ImVec2> header = FindButton(probe, kTracker, "###mvp_group");
  REQUIRE(header.has_value());
  static_cast<void>(ClickAt(probe, *header));
  const std::optional<ImVec2> leave = FindButton(probe, kTracker, "Quitter");
  REQUIRE(leave.has_value());
  static_cast<void>(ClickAt(probe, *leave));
  REQUIRE(probe.state.confirm_leave);
  // Fermée, puis exclu : la question tombe, même sans rien dessiner.
  probe.state.open = false;
  probe.view.group_id = 0;
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.confirm_leave);
  // Un nouveau groupe, la fenêtre rouverte : elle n'est pas reposée.
  probe.view.group_id = kGroup + 1;
  probe.state.open = true;
  probe.Settle();
  CHECK_FALSE(probe.state.confirm_leave);
}

TEST_CASE("Carnet MVP : la hauteur donnée au découpage est celle des rangs", "[ui][mvp]") {
  for (const bool sprites : {false, true}) {
    INFO(sprites);
    // Tous les rangs à l'écran : le dernier dessiné est le dernier de la table.
    TrackerProbe probe(sprites ? 4 : 10);
    probe.state.look.show_sprites = sprites;
    for (const MvpSlotView& slot : probe.slots) {
      probe.sprites.push_back(
          MvpSpriteArt{.mob_id = slot.sprite_mob_id,
                       .image = ro::ui::SkinImage{.id = 4, .width = 64.0F, .height = 64.0F}});
    }
    // Le premier rang n'a pas de sprite : il garde la hauteur des autres, sans
    // quoi tous les suivants descendraient moins que le découpage ne le croit.
    probe.slots.front().sprite_mob_id = 0;
    probe.slots.front().observed = true;
    probe.Settle();
    const ImGuiTable* table = SlotTable();
    REQUIRE(table != nullptr);
    const float expected = probe.RowHeight(sprites);
    // Le découpage pose la fin de la table là où il la PRÉVOIT ; le dernier rang
    // commence là où les précédents ont RÉELLEMENT mené. L'écart est une hauteur
    // de rang seulement si aucun n'a dérivé.
    CHECK(table->RowPosY2 - table->RowPosY1 == Catch::Approx(expected));
    if (sprites) {
      CHECK(expected == Catch::Approx(64.0F + 2.0F * ImGui::GetStyle().CellPadding.y));
    } else {
      CHECK(expected < 64.0F);
    }
  }
}

TEST_CASE("Carnet MVP : une poignée lâchée hors du carnet détache la ligne", "[ui][mvp]") {
  for (const bool outside : {true, false}) {
    INFO(outside);
    TrackerProbe probe;
    probe.Settle();
    const ImGuiTable* table = SlotTable();
    REQUIRE(table != nullptr);
    const float row_height = probe.RowHeight(false);
    // Le tri par défaut suit le nom : le rang 1 est le créneau 1.
    const ImVec2 grip = CellAt(*table, 0, 1, row_height, 2.0F);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(grip.x, grip.y);
    static_cast<void>(probe.Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    static_cast<void>(probe.Frame());
    io.AddMousePosEvent(grip.x + 40.0F, grip.y + 3.0F);
    static_cast<void>(probe.Frame());
    REQUIRE(probe.state.drag_slot == 1);
    // Hors de l'écran même : la place se borne à lui.
    const ImVec2 drop = outside ? ImVec2(2000.0F, 1000.0F) : ImVec2(grip.x + 60.0F, grip.y);
    io.AddMousePosEvent(drop.x, drop.y);
    static_cast<void>(probe.Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    const MvpTrackerAction action = probe.Frame();
    CHECK(probe.state.drag_slot == kMvpNoSlotView);
    if (outside) {
      CHECK(action.pin_slot == 1);
      CHECK(action.pin_x == 1600);
      CHECK(action.pin_y == 900);
    } else {
      CHECK(action.pin_slot == kMvpNoSlotView);
    }
  }
}

TEST_CASE("Carnet MVP : un rang lâché sur un nom n'en joue pas le clic", "[ui][mvp]") {
  for (const bool shift : {false, true}) {
    INFO(shift);
    TrackerProbe probe;
    probe.slots[2].observed = true;
    probe.slots[2].kill_time_s = kNow - 600;
    probe.Settle();
    const ImGuiTable* table = SlotTable();
    REQUIRE(table != nullptr);
    const float row_height = probe.RowHeight(false);
    const ImVec2 grip = CellAt(*table, 0, 1, row_height, 2.0F);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(grip.x, grip.y);
    static_cast<void>(probe.Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    static_cast<void>(probe.Frame());
    io.AddMousePosEvent(grip.x + 40.0F, grip.y + 3.0F);
    static_cast<void>(probe.Frame());
    REQUIRE(probe.state.drag_slot == 1);
    // Le nom d'un rang dessiné APRÈS la poignée, Maj tenue ou non.
    const ImVec2 name = CellAt(*table, 2, 2, row_height);
    io.AddMousePosEvent(name.x, name.y);
    io.AddKeyEvent(ImGuiMod_Shift, shift);
    static_cast<void>(probe.Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    const MvpTrackerAction action = probe.Frame();
    io.AddKeyEvent(ImGuiMod_Shift, false);
    static_cast<void>(probe.Frame());
    CHECK(probe.state.drag_slot == kMvpNoSlotView);
    CHECK(action.pin_slot == kMvpNoSlotView);
    CHECK(action.name_link.gesture == ChatLinkGesture::kNothing);
    // Le survol, lui, reste : l'aperçu suit la souris.
    CHECK(action.name_link.hovering);
    // Et le clic suivant sur ce nom joue de nouveau.
    CHECK(ClickAt(probe, name).released.name_link.gesture == ChatLinkGesture::kDescribe);
  }
}

TEST_CASE("Carnet MVP : un rang tiré pendant une perte de focus ne se lâche plus", "[ui][mvp]") {
  TrackerProbe probe;
  probe.Settle();
  const ImGuiTable* table = SlotTable();
  REQUIRE(table != nullptr);
  const float row_height = probe.RowHeight(false);
  const ImVec2 grip = CellAt(*table, 0, 1, row_height, 2.0F);
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(grip.x, grip.y);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMousePosEvent(grip.x + 40.0F, grip.y + 3.0F);
  static_cast<void>(probe.Frame());
  REQUIRE(probe.state.drag_slot == 1);
  // 🔴 ALT+TAB EN PLEIN GESTE : ImGui vide la souris, et le relâchement ne se
  // verra jamais.
  io.AddFocusEvent(false);
  static_cast<void>(probe.Frame());
  io.AddFocusEvent(true);
  const ImVec2 far(1500.0F, 850.0F);
  io.AddMousePosEvent(far.x, far.y);
  static_cast<void>(probe.Frame());
  CHECK(probe.state.drag_slot == kMvpNoSlotView);
  // Un simple clic, loin du carnet, ne sème aucune ligne.
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  CHECK(probe.Frame().pin_slot == kMvpNoSlotView);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  CHECK(probe.Frame().pin_slot == kMvpNoSlotView);
  CHECK(probe.state.drag_slot == kMvpNoSlotView);
}

TEST_CASE("Carnet MVP : une ligne tirée pendant une perte de focus ne suit plus", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.lines = {MvpPinnedLineView{
      .line = 0, .slot_id = 0, .mob_id = 1000, .map = "prt_maze03", .x = 100, .y = 100}};
  probe.Settle();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(105.0F, 105.0F);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  REQUIRE(probe.state.line_drag == 0);
  io.AddFocusEvent(false);
  static_cast<void>(probe.Frame());
  io.AddFocusEvent(true);
  io.AddMousePosEvent(800.0F, 600.0F);
  static_cast<void>(probe.Frame());
  CHECK(probe.state.line_drag == -1);
  // 🔴 UN GLISSER AILLEURS, loin de la ligne : elle reste où elle est.
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMousePosEvent(900.0F, 700.0F);
  const MvpTrackerAction dragged = probe.Frame();
  CHECK(dragged.moved_line < 0);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const MvpTrackerAction released = probe.Frame();
  CHECK(released.moved_line < 0);
  CHECK_FALSE(released.lines_released);
}

TEST_CASE("Carnet MVP : un clic droit remet une ligne détachée au carnet", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  for (int rank = 0; rank < 3; ++rank) {
    const MvpSlotView& slot = probe.slots[static_cast<std::size_t>(rank)];
    probe.lines.push_back(MvpPinnedLineView{.line = rank,
                                            .slot_id = slot.slot_id,
                                            .mob_id = slot.mob_id,
                                            .map = slot.map,
                                            .x = 100,
                                            .y = 100 + 200 * rank});
  }
  probe.Settle();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(110.0F, 305.0F);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  const MvpTrackerAction action = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  CHECK(action.removed_line == 1);
  // Retirée APRÈS la boucle : les trois ont été soumises à cette image.
  for (const MvpPinnedLineView& line : probe.lines) {
    const std::string name =
        "##mvpline_" + std::to_string(line.mob_id) + "_" + std::string(line.map);
    INFO(name);
    CHECK(Shown(name.c_str()));
  }
}

TEST_CASE("Carnet MVP : une ligne tirée s'aimante aux lignes visibles", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  // Deux lignes de largeurs bien distinctes : aligner leurs FINS ne tombe pas
  // près d'aligner leurs débuts.
  probe.labels[1] = "Golden Thief Bug, le plus long des noms";
  probe.slots[1].label = probe.labels[1];
  probe.lines = {
      MvpPinnedLineView{
          .line = 0, .slot_id = 0, .mob_id = 1000, .map = "prt_maze03", .x = 100, .y = 100},
      MvpPinnedLineView{
          .line = 1, .slot_id = 1, .mob_id = 1001, .map = "gef_fild10", .x = 400, .y = 400},
      // Non résolue : son créneau n'est pas au catalogue. Elle est plus près que
      // la première, mais on ne s'aimante pas à ce qu'on ne voit pas.
      MvpPinnedLineView{
          .line = 2, .slot_id = 77, .mob_id = 1077, .map = "nowhere", .x = 104, .y = 700}};
  probe.Settle();
  CHECK(Window("##mvpline_1077_nowhere") == nullptr);
  const float first_height = probe.state.line_sizes[0].y;
  REQUIRE(first_height > 0.0F);
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(405.0F, 405.0F);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  REQUIRE(probe.state.line_drag == 1);
  // Voulue en (103, bas de la première + 4) : à moins de dix pixels de son bord
  // gauche et de son bas.
  io.AddMousePosEvent(108.0F, 100.0F + first_height + 9.0F);
  const MvpTrackerAction moved = probe.Frame();
  CHECK(moved.moved_line == 1);
  CHECK(moved.moved_x == 100);
  CHECK(moved.moved_y == static_cast<int>(100.0F + first_height + 0.5F));
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const MvpTrackerAction released = probe.Frame();
  CHECK(released.lines_released);
  CHECK(probe.state.line_drag == -1);
}

namespace {

// Ce que fait `app` d'un déplacement : la ligne prend sa nouvelle place.
void ApplyMove(std::vector<MvpPinnedLineView>& lines, const MvpTrackerAction& action) {
  for (MvpPinnedLineView& line : lines) {
    if (line.line == action.moved_line) {
      line.x = action.moved_x;
      line.y = action.moved_y;
    }
  }
}

}  // namespace

TEST_CASE("Carnet MVP : les lignes posées hors de l'écran y reviennent toutes", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  // Toutes paraissent à la même image, comme au catalogue reçu après la relecture
  // des lignes d'un écran plus grand ; la dernière déborde par le coin.
  probe.lines = {
      MvpPinnedLineView{
          .line = 0, .slot_id = 0, .mob_id = 1000, .map = "prt_maze03", .x = 1800, .y = 100},
      MvpPinnedLineView{
          .line = 1, .slot_id = 1, .mob_id = 1001, .map = "gef_fild10", .x = 1700, .y = 950},
      MvpPinnedLineView{
          .line = 2, .slot_id = 2, .mob_id = 1002, .map = "pay_fild11", .x = 1590, .y = 890}};
  std::set<int> moved_lines;
  constexpr int kFramesForThree = kSettleFrames + 3;
  for (int frame = 0; frame < kFramesForThree; ++frame) {
    const MvpTrackerAction action = probe.Frame();
    if (action.moved_line >= 0) {
      CHECK(moved_lines.insert(action.moved_line).second);
      CHECK(action.lines_released);
      ApplyMove(probe.lines, action);
    }
  }
  CHECK(moved_lines == std::set<int>{0, 1, 2});
  for (const MvpPinnedLineView& line : probe.lines) {
    const ImVec2 size = probe.state.line_sizes[static_cast<std::size_t>(line.line)];
    REQUIRE(size.x > 0.0F);
    CHECK(static_cast<float>(line.x) + size.x <= 1600.5F);
    CHECK(static_cast<float>(line.y) + size.y <= 900.5F);
  }
}

TEST_CASE("Carnet MVP : un tirage en cours ne prive pas une ligne de sa borne", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.lines = {MvpPinnedLineView{
      .line = 0, .slot_id = 0, .mob_id = 1000, .map = "prt_maze03", .x = 100, .y = 100}};
  probe.Settle();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(105.0F, 105.0F);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  REQUIRE(probe.state.line_drag == 0);
  io.AddMousePosEvent(300.0F, 300.0F);
  const MvpPinnedLineView outside{
      .line = 1, .slot_id = 1, .mob_id = 1001, .map = "gef_fild10", .x = 1590, .y = 890};
  SECTION("la ligne tirée se dessine avant la ligne à borner") { probe.lines.push_back(outside); }
  SECTION("la ligne tirée se dessine après la ligne à borner") {
    probe.lines.insert(probe.lines.begin(), outside);
  }
  bool bounded = false;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    const MvpTrackerAction action = probe.Frame();
    if (action.moved_line == 1) {
      bounded = true;
    }
    ApplyMove(probe.lines, action);
  }
  CHECK(bounded);
  const auto placed = std::ranges::find(probe.lines, 1, &MvpPinnedLineView::line);
  REQUIRE(placed != probe.lines.end());
  const ImVec2 size = probe.state.line_sizes[1];
  CHECK(static_cast<float>(placed->x) + size.x <= 1600.5F);
  CHECK(static_cast<float>(placed->y) + size.y <= 900.5F);
  // Le tirage reprend à l'image suivante.
  io.AddMousePosEvent(310.0F, 310.0F);
  CHECK(probe.Frame().moved_line == 0);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  static_cast<void>(probe.Frame());
}

TEST_CASE("Carnet MVP : le rang visé défile une fois, et s'éteint", "[ui][mvp]") {
  TrackerProbe probe(40);
  probe.state.focus_slot = 39;
  probe.state.focus_ms = kNowMs;
  probe.Settle();
  const ImGuiTable* table = SlotTable();
  REQUIRE(table != nullptr);
  CHECK(probe.state.focus_scrolled);
  CHECK(table->InnerWindow->Scroll.y > 0.0F);
  CHECK(probe.state.focus_slot == 39);
  probe.view.now_ms = kNowMs + ro::ui::kMvpFocusMs;
  probe.Settle();
  CHECK(probe.state.focus_slot == kMvpNoSlotView);
}

TEST_CASE("Carnet MVP : une demande périmée ne fait pas défiler", "[ui][mvp]") {
  TrackerProbe probe(40);
  probe.state.focus_slot = 39;
  probe.state.focus_ms = kNowMs - ro::ui::kMvpFocusMs;
  probe.Settle();
  const ImGuiTable* table = SlotTable();
  REQUIRE(table != nullptr);
  CHECK(probe.state.focus_slot == kMvpNoSlotView);
  CHECK_FALSE(probe.state.focus_scrolled);
  CHECK(table->InnerWindow->Scroll.y == 0.0F);
}

TEST_CASE("Carnet MVP : le bandeau d'alerte suit l'interrupteur", "[ui][mvp]") {
  for (const bool enabled : {true, false}) {
    INFO(enabled);
    TrackerProbe probe;
    probe.view.enabled = enabled;
    const std::array<ro::ui::MvpAlertView, 1> alerts = {
        {{.slot_id = 0, .label = "Baphomet", .map = "prt_maze03"}}};
    probe.view.alerts = alerts;
    probe.Settle();
    CHECK(Shown("##mvp_alert") == enabled);
    if (enabled) {
      CHECK((Window("##mvp_alert")->Flags & ImGuiWindowFlags_NoSavedSettings) != 0);
    }
  }
}

TEST_CASE("Carnet MVP : le nom d'un MVP est un lien de monstre", "[ui][mvp]") {
  TrackerProbe probe;
  // Le créneau 0 a une heure de mort ; le créneau 1 n'a jamais été vu.
  probe.slots[0].observed = true;
  probe.slots[0].kill_time_s = kNow - 600;
  probe.Settle();
  const ImGuiTable* table = SlotTable();
  REQUIRE(table != nullptr);
  const float row_height = probe.RowHeight(false);

  const ImVec2 observed = CellAt(*table, 2, 0, row_height);
  const Click share = ClickAt(probe, observed, ImGuiMouseButton_Left, true);
  CHECK(share.released.name_link.gesture == ChatLinkGesture::kShareRespawn);
  CHECK(share.released.name_link.item_id == 1000);
  CHECK(share.released.name_link.token == 0);
  CHECK(share.released.name_link.hovered_id == 1000);

  const Click repost =
      ClickAt(probe, CellAt(*table, 2, 1, row_height), ImGuiMouseButton_Left, true);
  CHECK(repost.released.name_link.gesture == ChatLinkGesture::kRepost);
  CHECK(repost.released.name_link.token == 1);

  const Click describe = ClickAt(probe, observed);
  CHECK(describe.released.name_link.gesture == ChatLinkGesture::kDescribe);

  // Le clic droit ouvre le menu au niveau de la FENÊTRE, hors des rangs.
  static_cast<void>(ClickAt(probe, observed, ImGuiMouseButton_Right));
  CHECK(probe.state.menu_slot == 0);
  CHECK(PopupOpenIn(kTracker, "##mvp_link_menu"));
}

TEST_CASE("Carnet MVP : l'étoile demande la bascule, la source ouvre la saisie", "[ui][mvp]") {
  TrackerProbe probe;
  probe.slots[2].favorite = true;
  probe.Settle();
  const ImGuiTable* table = SlotTable();
  REQUIRE(table != nullptr);
  const float row_height = probe.RowHeight(false);
  const MvpTrackerAction star = ClickAt(probe, CellAt(*table, 1, 2, row_height)).released;
  CHECK(star.favorite_slot == 2);
  CHECK_FALSE(star.favorite_on);

  static_cast<void>(ClickAt(probe, CellAt(*table, 7, 1, row_height)));
  CHECK(probe.state.manual_slot == 1);
  REQUIRE(PopupOpenIn(kTracker, "##mvp_manual"));
  // Le champ a le clavier dès l'ouverture : on y tape sans le cliquer.
  ImGui::GetIO().AddInputCharactersUTF8("1430");
  probe.Settle();
  CHECK(std::string_view(probe.state.manual_time.data()) == "1430");
  ImGuiWindow* popup = GImGui->OpenPopupStack.back().Window;
  REQUIRE(popup != nullptr);
  const std::optional<ImVec2> send = FindHovered(probe, AreaOf(*popup), popup->GetID("Envoyer"));
  REQUIRE(send.has_value());
  const MvpTrackerAction sent = ClickAt(probe, *send).released;
  CHECK(sent.manual_slot == 1);
  CHECK(sent.manual_kill_time_s == ro::ui::ParseMvpKillTime("1430", kNow));
}

TEST_CASE("Carnet MVP : les favoris d'abord, l'échéance inconnue à la fin", "[ui][mvp]") {
  TrackerProbe probe(4);
  probe.slots[2].favorite = true;
  probe.slots[3].favorite = true;
  // « Retour » : deux créneaux sans fenêtre, deux avec.
  probe.slots[1].window_known = true;
  probe.slots[1].window_from_s = kNow + 600;
  probe.slots[3].window_known = true;
  probe.slots[3].window_from_s = kNow + 60;
  probe.Settle();
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{0, 1, 2, 3});

  SortBy(probe, 1);
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{2, 3, 0, 1});
  // Décroissant : les favoris en fin, et le nom départage À REBOURS lui aussi.
  SortBy(probe, 1);
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{1, 0, 3, 2});

  SortBy(probe, 5);
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{3, 1, 0, 2});
  SortBy(probe, 5);
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{1, 3, 0, 2});
}

TEST_CASE("Carnet MVP : « Âge » met la plus récente devant", "[ui][mvp]") {
  TrackerProbe probe(4);
  probe.slots[0].observed = true;
  probe.slots[0].reported_at_s = kNow - 3000;
  probe.slots[2].observed = true;
  probe.slots[2].reported_at_s = kNow - 60;
  probe.Settle();
  SortBy(probe, 6);
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{2, 0, 1, 3});
  SortBy(probe, 6);
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{0, 2, 1, 3});
}

TEST_CASE("Carnet MVP : un créneau renommé refait le tri", "[ui][mvp]") {
  TrackerProbe probe(3);
  probe.Settle();
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{0, 1, 2});
  probe.labels[0] = "Zeus";
  probe.slots[0].label = probe.labels[0];
  probe.Settle();
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{1, 2, 0});
}

TEST_CASE("Carnet MVP : le filtre ôte les rangs qui ne le passent pas", "[ui][mvp]") {
  TrackerProbe probe(4);
  probe.state.look.show_filter = true;
  probe.state.filter = {'g', 'e', 'f'};
  probe.Settle();
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{1});
  // Éteint, le champ est ignoré, pas vidé.
  probe.state.look.show_filter = false;
  probe.Settle();
  CHECK(OrderedSlots(probe).size() == 4);
  CHECK(probe.state.filter[0] == 'g');
}

TEST_CASE("Carnet MVP : un créneau écarté qui disparaît refait l'ordre", "[ui][mvp]") {
  TrackerProbe probe(3);
  probe.state.look.show_filter = true;
  probe.state.filter = {'g', 'e', 'f'};
  probe.Settle();
  REQUIRE(OrderedSlots(probe) == std::vector<std::uint16_t>{1});
  // Le rang retenu garde son empreinte, mais pas sa place dans le catalogue.
  probe.slots.erase(probe.slots.begin());
  probe.Settle();
  CHECK(probe.state.order == std::vector<std::size_t>{0});
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{1});
  CHECK(std::ranges::all_of(probe.state.order,
                            [&](std::size_t index) { return index < probe.slots.size(); }));
}

TEST_CASE("Carnet MVP : repliée, la barre dit la prochaine échéance", "[ui][mvp]") {
  TrackerProbe probe;
  probe.slots[1].window_known = true;
  probe.slots[1].window_from_s = kNow + 8040;
  probe.slots[1].window_to_s = kNow + 9000;
  probe.Settle();
  CHECK(probe.state.title == "Carnet de chasse MVP###mvp_tracker");
  Window(kTracker)->Collapsed = true;
  probe.Settle();
  CHECK(probe.state.was_collapsed);
  CHECK(probe.state.title == ro::ui::MvpCollapsedTitle(probe.view.slots, kNow) + "###mvp_tracker");
  CHECK(probe.state.title == "MVP : Mvp B1 dans 2h14###mvp_tracker");
}

TEST_CASE("Carnet MVP : l'aperçu d'un lien se dessine en infobulle", "[ui][mvp]") {
  TrackerProbe probe;
  probe.ui.BeginFrame(kViewport, kFrameSeconds);
  ro::ui::DrawMvpLinkTip(ro::ui::MvpLinkTip{
      .name = "Baphomet", .map = "prt_maze03", .respawn_line = "Retour à 21:12 (Convex Mirror)"});
  probe.ui.EndFrame();
  CHECK(Shown("##Tooltip_00"));
}

TEST_CASE("Carnet MVP : à l'échelle double, le plan de la carte tient dans sa bulle", "[ui][mvp]") {
  constexpr float kDoubleScale = 2.0F;
  constexpr float kPreviewSidePixels = 180.0F;
  constexpr float kRadarPixels = 128.0F;
  TrackerProbe probe;
  probe.ui.SetInterfaceScale(kDoubleScale);
  probe.view.map_preview = ro::ui::MvpMapPreview{
      .map = probe.slots[0].map,
      .image = ro::ui::SkinImage{.id = 4, .width = kRadarPixels, .height = kRadarPixels}};
  probe.Settle();
  const ImGuiTable* table = SlotTable();
  REQUIRE(table != nullptr);
  const ImVec2 map_cell = CellAt(*table, 3, 0, probe.RowHeight(false));
  ImGui::GetIO().AddMousePosEvent(map_cell.x, map_cell.y);
  probe.Settle();
  REQUIRE(Shown("##Tooltip_00"));
  const ImGuiWindow* tip = Window("##Tooltip_00");
  CHECK(tip->Size.x + 0.5F >= kPreviewSidePixels * kDoubleScale + 2.0F * tip->WindowPadding.x);
}
