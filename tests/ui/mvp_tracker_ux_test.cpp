// LE CARNET DE CHASSE MVP, ce que le joueur en voit au-delà de la table : le
// nom du groupe borné au fil, la carte nommée, l'alerte, la propriété,
// l'invitation qui attend le serveur, le premier plan.

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

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/mvp_tracker_window.h"

#include "mvp_tracker_probe.h"

using namespace mvp_tracker_probe;
using ro::ui::kMvpGroupNameMostWireChars;
using ro::ui::MvpAlertView;

namespace {

// La colonne « Délai » de la table, par index de déclaration.
constexpr int kDelayColumn = 4;
constexpr int kMapColumn = 3;
constexpr int kNameColumn = 2;

// Les noms qu'« Exclure » rend, d'un balayage de la colonne d'action.
[[nodiscard]] std::set<std::string> KickedNames(TrackerProbe& probe) {
  const ImGuiTable* members = TableOf(kTracker, "##mvp_members");
  REQUIRE(members != nullptr);
  std::set<std::string> kicked;
  const float x = members->Columns[3].WorkMinX + 4.0F;
  for (float y = members->OuterRect.Min.y; y < members->OuterRect.Max.y; y += kScanStepY) {
    if (const std::string name = ClickAt(probe, ImVec2(x, y)).released.kick_member; !name.empty()) {
      kicked.insert(name);
    }
  }
  return kicked;
}

[[nodiscard]] TrackerProbe& WithTwoAccounts(TrackerProbe& probe) {
  probe.members = {MvpMemberView{.user_id = 1, .level = 99, .online = true, .name = "Ann"},
                   MvpMemberView{.user_id = 2, .level = 80, .online = true, .name = "Bob"}};
  probe.Settle();
  OpenGroupHeader(probe);
  return probe;
}

// « Dissoudre », cherché sur la ligne de « Quitter », à sa droite : un balayage
// de toute la fenêtre serait lent pour prouver une absence.
[[nodiscard]] std::optional<ImVec2> FindDissolve(TrackerProbe& probe) {
  const std::optional<ImVec2> leave = FindButton(probe, kTracker, "Quitter");
  REQUIRE(leave.has_value());
  ImGuiWindow* window = Window(kTracker);
  REQUIRE(window != nullptr);
  const ImRect line(leave->x, leave->y - 1.0F, window->Pos.x + window->Size.x, leave->y + 1.0F);
  return FindHovered(probe, line, window->GetID("Dissoudre"));
}

// Le bouton `label` de la confirmation de dissolution, rangé sous son nom.
[[nodiscard]] std::optional<ImVec2> FindDissolveButton(TrackerProbe& probe, const char* label) {
  ImGuiWindow* window = Window(kTracker);
  REQUIRE(window != nullptr);
  const ImGuiID id = ImHashStr(label, 0, window->GetID("dissolve"));
  return FindHovered(probe, AreaOf(*window), id);
}

}  // namespace

TEST_CASE("Carnet MVP : le nom de groupe tient 31 caractères du fil, en UTF-8", "[ui][mvp]") {
  // Le serveur lit 31 octets du fil ; un caractère CP1252 en fait un, et au
  // pire trois une fois en UTF-8.
  CHECK(kMvpGroupNameMostWireChars == 31);
  const ro::ui::MvpTrackerWindowState state;
  CHECK(state.group_name.size() == kMvpGroupNameMostWireChars * 3 + 1);

  // La carte montrée : lisible si l'installation la nomme, interne sinon.
  MvpSlotView slot{.map = "prt_maze03"};
  CHECK(ro::ui::MvpMapShown(slot) == "prt_maze03");
  slot.map_label = "Labyrinthe de Prontera, 3F";
  CHECK(ro::ui::MvpMapShown(slot) == "Labyrinthe de Prontera, 3F");
}

TEST_CASE("Carnet MVP : la saisie du nom de groupe s'arrête au fil, Entrée crée", "[ui][mvp]") {
  TrackerProbe probe;
  probe.view.group_id = 0;
  probe.Settle();
  const std::optional<ImVec2> field = FindButton(probe, kTracker, "##mvp_group_name");
  REQUIRE(field.has_value());
  static_cast<void>(ClickAt(probe, *field));
  // Quarante « é » : le fil n'en prend que 31, deux octets chacun en UTF-8.
  std::string typed;
  for (int letter = 0; letter < 40; ++letter) {
    typed += "é";
  }
  ImGui::GetIO().AddInputCharactersUTF8(typed.c_str());
  probe.Settle();
  CHECK(std::string_view(probe.state.group_name.data()).size() == 62);
  const Click enter = PressEnter(probe);
  CHECK(enter.pressed.create_group == typed.substr(0, 62));
  CHECK(probe.state.group_name[0] == '\0');
}

TEST_CASE("Carnet MVP : Entrée invite le personnage saisi", "[ui][mvp]") {
  TrackerProbe probe;
  static_cast<void>(WithTwoAccounts(probe));
  const std::optional<ImVec2> field = FindButton(probe, kTracker, "##mvp_invite");
  REQUIRE(field.has_value());
  static_cast<void>(ClickAt(probe, *field));
  ImGui::GetIO().AddInputCharactersUTF8("Cid");
  probe.Settle();
  CHECK(PressEnter(probe).pressed.invite_member == "Cid");
}

TEST_CASE("Carnet MVP : une ligne du bandeau d'alerte ouvre le carnet sur elle", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  const std::array<MvpAlertView, 2> alerts = {
      {{.slot_id = 3, .label = "Baphomet", .map = "prt_maze03"},
       {.slot_id = 5, .label = "Drake", .map = "treasure02", .exact = true, .at_clock = "14:10"}}};
  probe.view.alerts = alerts;
  probe.Settle();
  const ImGuiWindow* banner = Window("##mvp_alert");
  REQUIRE(banner != nullptr);
  const ImRect area = AreaOf(*banner);
  std::set<std::uint16_t> clicked;
  for (float y = area.Min.y + 1.0F; y < area.Max.y; y += kScanStepY) {
    const std::uint16_t slot = ClickAt(probe, ImVec2(area.GetCenter().x, y)).released.alert_slot;
    if (slot != kMvpNoSlotView) {
      clicked.insert(slot);
    }
  }
  CHECK(clicked == std::set<std::uint16_t>{3, 5});
}

TEST_CASE("Carnet MVP : la modale d'invitation attend la réponse du serveur", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.view.group_id = 0;
  probe.view.pending_invite = 42;
  probe.view.pending_invite_name = "Les Faucheurs";
  probe.Settle();
  const std::optional<ImVec2> accept = FindButton(probe, "###mvp_invite", "Accepter");
  REQUIRE(accept.has_value());
  CHECK(ClickAt(probe, *accept).released.accept_invite);
  probe.Settle();
  CHECK(Shown("###mvp_invite"));

  // La réponse partie, les deux boutons se grisent.
  probe.view.invite_waiting = true;
  probe.Settle();
  CHECK_FALSE(ClickAt(probe, *accept).released.accept_invite);
  const std::optional<ImVec2> decline = FindButton(probe, "###mvp_invite", "Refuser");
  REQUIRE(decline.has_value());
  CHECK_FALSE(ClickAt(probe, *decline).released.decline_invite);

  // L'invitation disparue du serveur : seul « Fermer », qui n'envoie rien.
  probe.view.invite_waiting = false;
  probe.view.invite_refusal = 9;
  probe.view.invite_gone = true;
  probe.Settle();
  CHECK_FALSE(ClickAt(probe, *accept).released.accept_invite);
  const std::optional<ImVec2> close = FindButton(probe, "###mvp_invite", "Fermer");
  REQUIRE(close.has_value());
  const Click closed = ClickAt(probe, *close);
  CHECK(closed.released.dismiss_invite);
  CHECK_FALSE(closed.released.accept_invite);
  CHECK_FALSE(closed.released.decline_invite);
}

TEST_CASE("Carnet MVP : « Exclure » se grise quand on sait n'être pas propriétaire", "[ui][mvp]") {
  for (const bool owner_known : {false, true}) {
    for (const bool i_own : {false, true}) {
      INFO(owner_known);
      INFO(i_own);
      TrackerProbe probe;
      probe.view.owner_known = owner_known;
      probe.view.i_own = i_own;
      static_cast<void>(WithTwoAccounts(probe));
      const bool forbidden = owner_known && !i_own;
      CHECK(KickedNames(probe) ==
            (forbidden ? std::set<std::string>{} : std::set<std::string>{"Bob"}));
      // Dissoudre n'est offert qu'au propriétaire SU.
      CHECK(FindDissolve(probe).has_value() == (owner_known && i_own));
    }
  }
}

TEST_CASE("Carnet MVP : « Dissoudre » demande confirmation", "[ui][mvp]") {
  TrackerProbe probe;
  probe.view.owner_known = true;
  probe.view.i_own = true;
  static_cast<void>(WithTwoAccounts(probe));
  const std::optional<ImVec2> dissolve = FindDissolve(probe);
  REQUIRE(dissolve.has_value());
  CHECK_FALSE(ClickAt(probe, *dissolve).released.dissolve_group);
  REQUIRE(probe.state.confirm_dissolve);
  CHECK_FALSE(probe.state.confirm_leave);

  const std::optional<ImVec2> cancel = FindDissolveButton(probe, "Annuler");
  REQUIRE(cancel.has_value());
  CHECK_FALSE(ClickAt(probe, *cancel).released.dissolve_group);
  CHECK_FALSE(probe.state.confirm_dissolve);

  static_cast<void>(ClickAt(probe, *dissolve));
  const std::optional<ImVec2> confirm = FindDissolveButton(probe, "Confirmer");
  REQUIRE(confirm.has_value());
  CHECK(ClickAt(probe, *confirm).released.dissolve_group);
  CHECK_FALSE(probe.state.confirm_dissolve);

  // Le groupe parti, la question tombe avec lui.
  probe.state.confirm_dissolve = true;
  probe.view.group_id = 0;
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.confirm_dissolve);
}

TEST_CASE("Carnet MVP : une ouverture demandée met le carnet au premier plan", "[ui][mvp]") {
  TrackerProbe probe;
  probe.Settle();
  // Une autre fenêtre prend le focus.
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    probe.ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawMvpTrackerWindow(probe.state, probe.view, probe.art));
    ImGui::Begin("autre");
    if (frame == 0) {
      ImGui::SetWindowFocus();
    }
    ImGui::End();
    probe.ui.EndFrame();
  }
  REQUIRE(GImGui->NavWindow == Window("autre"));
  probe.state.raise = true;
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.raise);
  CHECK(GImGui->NavWindow == Window(kTracker));
}

TEST_CASE("Carnet MVP : la colonne « Délai » suit le réglage et le rapporte", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.look.show_delay = true;
  std::vector<std::optional<bool>> reported;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    reported.push_back(probe.Frame().delay_shown);
  }
  // Le réglage s'impose sans rien rapporter.
  CHECK(reported == std::vector<std::optional<bool>>(kSettleFrames));
  ImGuiTable* table = SlotTable();
  REQUIRE(table != nullptr);
  CHECK(table->Columns[kDelayColumn].IsEnabled);

  // Le réglage éteint : la colonne se masque.
  probe.state.look.show_delay = false;
  probe.Settle();
  CHECK_FALSE(table->Columns[kDelayColumn].IsEnabled);

  // Le clic droit de l'en-tête la montre : la table le rapporte.
  table->Columns[kDelayColumn].IsUserEnabledNextFrame = true;
  std::optional<bool> shown;
  for (int frame = 0; frame < kSettleFrames && !shown.has_value(); ++frame) {
    shown = probe.Frame().delay_shown;
  }
  CHECK(shown == std::optional<bool>(true));
}

TEST_CASE("Carnet MVP : la colonne « Carte » trie sur le nom montré", "[ui][mvp]") {
  TrackerProbe probe;
  // Montrés : « Zeta », « gef_fild10 », « Alberta ».
  probe.slots[0].map_label = "Zeta";
  probe.slots[2].map_label = "Alberta";
  probe.Settle();
  SortBy(probe, kMapColumn);
  CHECK(OrderedSlots(probe) == std::vector<std::uint16_t>{2, 0, 1});
}

TEST_CASE("Carnet MVP : le survol d'un nom dit son créneau", "[ui][mvp]") {
  TrackerProbe probe;
  probe.Settle();
  const ImGuiTable* table = SlotTable();
  REQUIRE(table != nullptr);
  const ImVec2 name = CellAt(*table, kNameColumn, 1, probe.RowHeight(false));
  ImGui::GetIO().AddMousePosEvent(name.x, name.y);
  static_cast<void>(probe.Frame());
  const MvpTrackerAction hovered = probe.Frame();
  CHECK(hovered.name_link.hovering);
  CHECK(hovered.name_link.hovered_token == 1);
}

TEST_CASE("Carnet MVP : une ligne détachée survolée dit ses gestes", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.lines = {MvpPinnedLineView{
      .line = 0, .slot_id = 0, .mob_id = 1000, .map = "prt_maze03", .x = 100, .y = 100}};
  probe.Settle();
  CHECK_FALSE(Shown("##Tooltip_00"));
  ImGui::GetIO().AddMousePosEvent(105.0F, 105.0F);
  probe.Settle();
  REQUIRE(Shown("##Tooltip_00"));
  // Le texte, relevé dans le journal d'ImGui : les deux gestes, en clair.
  probe.ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::LogToBuffer();
  static_cast<void>(ro::ui::DrawMvpTrackerWindow(probe.state, probe.view, probe.art));
  const std::string logged = GImGui->LogBuffer.c_str();
  ImGui::LogFinish();
  probe.ui.EndFrame();
  INFO(logged);
  CHECK(logged.find("Glisser pour déplacer · Clic droit : remettre au carnet") !=
        std::string::npos);
}

TEST_CASE("Carnet MVP : un refus de l'invitation offre « Fermer »", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.view.group_id = 0;
  probe.view.pending_invite = 42;
  probe.view.pending_invite_name = "Les Faucheurs";
  probe.Settle();
  // Sans refus, aucune sortie hors des deux réponses.
  ImGuiWindow* modal = Window("###mvp_invite");
  REQUIRE(modal != nullptr);
  CHECK_FALSE(FindHovered(probe, AreaOf(*modal), modal->GetID("Fermer")).has_value());
  // 🔴 UN REFUS QUELCONQUE : la modale bloque tout, elle doit offrir de sortir.
  probe.view.invite_refusal = 12;
  probe.Settle();
  const std::optional<ImVec2> close = FindButton(probe, "###mvp_invite", "Fermer");
  REQUIRE(close.has_value());
  CHECK(ClickAt(probe, *close).released.dismiss_invite);
  // Les réponses restent offertes à côté.
  CHECK(FindButton(probe, "###mvp_invite", "Refuser").has_value());
}

TEST_CASE("Carnet MVP : la modale d'invitation revient après un autre surgissant", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.view.group_id = 0;
  probe.view.pending_invite = 42;
  probe.view.pending_invite_name = "Les Faucheurs";
  probe.Settle();
  REQUIRE(Shown("###mvp_invite"));
  // Un surgissant de premier niveau s'ouvre ailleurs, puis se ferme.
  constexpr int kMenuFrames = 3;
  for (int frame = 0; frame < kMenuFrames; ++frame) {
    probe.ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawMvpTrackerWindow(probe.state, probe.view, probe.art));
    ImGui::Begin("autre");
    if (frame == 0) {
      ImGui::OpenPopup("menu");
    }
    if (ImGui::BeginPopup("menu")) {
      if (frame == kMenuFrames - 1) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    ImGui::End();
    probe.ui.EndFrame();
  }
  CHECK_FALSE(Shown("###mvp_invite"));
  probe.Settle();
  CHECK(Shown("###mvp_invite"));
}

TEST_CASE("Carnet MVP : Entrée sur un champ vide lui laisse le clavier", "[ui][mvp]") {
  TrackerProbe probe;
  probe.view.group_id = 0;
  probe.Settle();
  const std::optional<ImVec2> field = FindButton(probe, kTracker, "##mvp_group_name");
  REQUIRE(field.has_value());
  static_cast<void>(ClickAt(probe, *field));
  const ImGuiID field_id = Window(kTracker)->GetID("##mvp_group_name");
  REQUIRE(GImGui->ActiveId == field_id);
  CHECK(PressEnter(probe).pressed.create_group.empty());
  probe.Settle();
  CHECK(GImGui->ActiveId == field_id);
}

TEST_CASE("Carnet MVP : une frappe au début d'un nom plein n'en ampute pas la fin", "[ui][mvp]") {
  TrackerProbe probe;
  probe.view.group_id = 0;
  probe.Settle();
  const std::optional<ImVec2> field = FindButton(probe, kTracker, "##mvp_group_name");
  REQUIRE(field.has_value());
  static_cast<void>(ClickAt(probe, *field));
  const std::string full = std::string(kMvpGroupNameMostWireChars - 1, 'a') + "Z";
  ImGui::GetIO().AddInputCharactersUTF8(full.c_str());
  probe.Settle();
  REQUIRE(std::string_view(probe.state.group_name.data()) == full);
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiKey_Home, true);
  static_cast<void>(probe.Frame());
  io.AddKeyEvent(ImGuiKey_Home, false);
  static_cast<void>(probe.Frame());
  io.AddInputCharactersUTF8("X");
  probe.Settle();
  CHECK(std::string_view(probe.state.group_name.data()) == full);
}

TEST_CASE("Carnet MVP : hors liaison vérifiée, « Créer » se grise et garde le nom", "[ui][mvp]") {
  TrackerProbe probe;
  probe.view.link_ready = false;
  probe.view.group_id = 0;
  probe.Settle();
  const std::optional<ImVec2> field = FindButton(probe, kTracker, "##mvp_group_name");
  REQUIRE(field.has_value());
  static_cast<void>(ClickAt(probe, *field));
  ImGui::GetIO().AddInputCharactersUTF8("Bapho");
  probe.Settle();
  // Ni Entrée ni le bouton : rien ne part, et le nom reste dans son champ.
  const ImGuiID field_id = Window(kTracker)->GetID("##mvp_group_name");
  REQUIRE(GImGui->ActiveId == field_id);
  const Click enter = PressEnter(probe);
  CHECK(enter.pressed.create_group.empty());
  CHECK(enter.released.create_group.empty());
  // 🔴 ENTRÉE QUI N'ENVOIE RIEN LAISSE LE CLAVIER AU CHAMP : sinon la frappe
  // suivante partirait dans le jeu.
  probe.Settle();
  CHECK(GImGui->ActiveId == field_id);
  const std::optional<ImVec2> create = FindButton(probe, kTracker, "Créer");
  REQUIRE(create.has_value());
  CHECK(ClickAt(probe, *create).released.create_group.empty());
  CHECK(std::string_view(probe.state.group_name.data()) == "Bapho");
  // Grisé, il dit pourquoi.
  ImGui::GetIO().AddMousePosEvent(create->x, create->y);
  probe.Settle();
  CHECK(Shown("##Tooltip_00"));
  // La liaison vérifiée, le même geste part.
  probe.view.link_ready = true;
  probe.Settle();
  CHECK(ClickAt(probe, *create).released.create_group == "Bapho");
}

TEST_CASE("Carnet MVP : hors liaison vérifiée, le groupe et la table n'envoient rien",
          "[ui][mvp]") {
  TrackerProbe probe;
  probe.view.link_ready = false;
  static_cast<void>(WithTwoAccounts(probe));
  // Inviter : le nom tapé reste.
  const std::optional<ImVec2> field = FindButton(probe, kTracker, "##mvp_invite");
  REQUIRE(field.has_value());
  static_cast<void>(ClickAt(probe, *field));
  ImGui::GetIO().AddInputCharactersUTF8("Cid");
  probe.Settle();
  const ImGuiID field_id = Window(kTracker)->GetID("##mvp_invite");
  REQUIRE(GImGui->ActiveId == field_id);
  CHECK(PressEnter(probe).pressed.invite_member.empty());
  // Entrée qui n'envoie rien laisse le clavier au champ.
  probe.Settle();
  CHECK(GImGui->ActiveId == field_id);
  const std::optional<ImVec2> invite = FindButton(probe, kTracker, "Inviter");
  REQUIRE(invite.has_value());
  CHECK(ClickAt(probe, *invite).released.invite_member.empty());
  CHECK(std::string_view(probe.state.invite_name.data()) == "Cid");
  // Exclure, Quitter : grisés.
  CHECK(KickedNames(probe).empty());
  const std::optional<ImVec2> leave = FindButton(probe, kTracker, "Quitter");
  REQUIRE(leave.has_value());
  static_cast<void>(ClickAt(probe, *leave));
  CHECK_FALSE(probe.state.confirm_leave);

  // L'étoile ne bascule pas.
  const ImGuiTable* table = SlotTable();
  REQUIRE(table != nullptr);
  const float row_height = probe.RowHeight(false);
  CHECK(ClickAt(probe, CellAt(*table, 1, 2, row_height)).released.favorite_slot == kMvpNoSlotView);

  // La saisie s'ouvre — elle est locale —, mais l'heure tapée ne part pas, et
  // le surgissant reste ouvert avec elle.
  static_cast<void>(ClickAt(probe, CellAt(*table, 7, 1, row_height)));
  REQUIRE(PopupOpenIn(kTracker, "##mvp_manual"));
  ImGui::GetIO().AddInputCharactersUTF8("1430");
  probe.Settle();
  const ImGuiID time_field_id = GImGui->ActiveId;
  REQUIRE(time_field_id != 0);
  CHECK(PressEnter(probe).pressed.manual_slot == kMvpNoSlotView);
  probe.Settle();
  CHECK(PopupOpenIn(kTracker, "##mvp_manual"));
  CHECK(std::string_view(probe.state.manual_time.data()) == "1430");
  CHECK(GImGui->ActiveId == time_field_id);
}

TEST_CASE("Carnet MVP : hors liaison vérifiée, l'invitation ne se répond pas", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.view.link_ready = false;
  probe.view.group_id = 0;
  probe.view.pending_invite = 42;
  probe.view.pending_invite_name = "Les Faucheurs";
  probe.Settle();
  const std::optional<ImVec2> accept = FindButton(probe, "###mvp_invite", "Accepter");
  REQUIRE(accept.has_value());
  CHECK_FALSE(ClickAt(probe, *accept).released.accept_invite);
  const std::optional<ImVec2> decline = FindButton(probe, "###mvp_invite", "Refuser");
  REQUIRE(decline.has_value());
  CHECK_FALSE(ClickAt(probe, *decline).released.decline_invite);
  probe.view.link_ready = true;
  probe.Settle();
  CHECK(ClickAt(probe, *accept).released.accept_invite);
}

TEST_CASE("Carnet MVP : hors liaison vérifiée, « Plus tard » sort de l'invitation", "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.view.link_ready = false;
  probe.view.group_id = 0;
  probe.view.pending_invite = 42;
  probe.view.pending_invite_name = "Les Faucheurs";
  probe.Settle();
  REQUIRE(Shown("###mvp_invite"));
  // 🔴 LA MODALE BLOQUE TOUTE L'INTERFACE : sans réponse possible, elle offre
  // une sortie, qui n'envoie rien et ne congédie pas.
  const std::optional<ImVec2> later = FindButton(probe, "###mvp_invite", "Plus tard");
  REQUIRE(later.has_value());
  const Click click = ClickAt(probe, *later);
  CHECK_FALSE(click.released.accept_invite);
  CHECK_FALSE(click.released.decline_invite);
  CHECK_FALSE(click.released.dismiss_invite);
  probe.Settle();
  CHECK_FALSE(Shown("###mvp_invite"));
  // Elle ne se redemande pas tant que la liaison n'est pas revenue…
  probe.Settle();
  CHECK_FALSE(Shown("###mvp_invite"));
  // … et revient avec elle, sans « Plus tard ».
  probe.view.link_ready = true;
  probe.Settle();
  REQUIRE(Shown("###mvp_invite"));
  CHECK_FALSE(FindButton(probe, "###mvp_invite", "Plus tard").has_value());
}

TEST_CASE("Carnet MVP : une invitation qui arrive attend qu'un surgissant ouvert se ferme",
          "[ui][mvp]") {
  TrackerProbe probe;
  probe.state.open = false;
  probe.view.group_id = 0;
  probe.Settle();
  // Un menu s'ouvre ailleurs ; l'invitation arrive pendant qu'il l'est.
  constexpr int kMenuFrames = 4;
  constexpr int kInviteFrame = 1;
  for (int frame = 0; frame <= kMenuFrames; ++frame) {
    if (frame == kInviteFrame) {
      probe.view.pending_invite = 42;
      probe.view.pending_invite_name = "Les Faucheurs";
    }
    probe.Bind();
    probe.ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawMvpTrackerWindow(probe.state, probe.view, probe.art));
    ImGui::Begin("autre");
    if (frame == 0) {
      ImGui::OpenPopup("menu");
    }
    const bool menu_open = ImGui::BeginPopup("menu");
    if (menu_open) {
      if (frame == kMenuFrames) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    ImGui::End();
    probe.ui.EndFrame();
    if (frame > kInviteFrame && frame < kMenuFrames) {
      // Le menu n'est pas chassé, et l'invitation ne s'est pas imposée.
      CHECK(menu_open);
      CHECK_FALSE(Shown("###mvp_invite"));
    }
  }
  // Le menu fermé, elle vient.
  probe.Settle();
  CHECK(Shown("###mvp_invite"));
}

TEST_CASE("Carnet MVP : nos lignes disent « (vous) », et ne s'excluent pas", "[ui][mvp]") {
  TrackerProbe probe;
  // Le propriétaire est inconnu : « Exclure » reste offert aux autres comptes.
  probe.members = {
      MvpMemberView{.user_id = 1, .level = 99, .online = true, .name = "Ann"},
      MvpMemberView{.user_id = 2, .level = 80, .online = true, .name = "Bob", .our_account = true},
      MvpMemberView{.user_id = 2, .level = 60, .name = "Bob2", .our_account = true},
      MvpMemberView{.user_id = 3, .level = 70, .name = "Cid"}};
  probe.Settle();
  OpenGroupHeader(probe);
  CHECK(KickedNames(probe) == std::set<std::string>{"Cid"});

  probe.ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::LogToBuffer();
  static_cast<void>(ro::ui::DrawMvpTrackerWindow(probe.state, probe.view, probe.art));
  const std::string logged = GImGui->LogBuffer.c_str();
  ImGui::LogFinish();
  probe.ui.EndFrame();
  INFO(logged);
  // Une marque par session de notre compte, aucune ailleurs.
  std::size_t marks = 0;
  for (std::size_t at = logged.find("(vous)"); at != std::string::npos;
       at = logged.find("(vous)", at + 1)) {
    ++marks;
  }
  CHECK(marks == 2);
}

TEST_CASE("Carnet MVP : une saisie sans effet se dit, sans le rouge d'un refus", "[ui][mvp]") {
  constexpr auto kKept = std::uint8_t{14};
  CHECK_FALSE(ro::ui::MvpResultRefuses(kKept));
  CHECK_FALSE(ro::ui::MvpResultRefuses(0));
  CHECK(ro::ui::MvpResultRefuses(7));
  // Un code inconnu de cette version reste un refus : il s'affiche en rouge.
  CHECK(ro::ui::MvpResultRefuses(200));

  TrackerProbe probe;
  probe.view.result_code = kKept;
  probe.view.result_shown = true;
  probe.Settle();
  probe.ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::LogToBuffer();
  static_cast<void>(ro::ui::DrawMvpTrackerWindow(probe.state, probe.view, probe.art));
  const std::string logged = GImGui->LogBuffer.c_str();
  ImGui::LogFinish();
  probe.ui.EndFrame();
  INFO(logged);
  CHECK(logged.find(ro::ui::MvpResultText(kKept)) != std::string::npos);
}
