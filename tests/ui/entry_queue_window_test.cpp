// LES FENÊTRES DE L'INSCRIPTION AU CHAMP DE BATAILLE : le temps d'attente
// lisible, les verbes rendus après confirmation, les boutons éteints hors
// liaison, la modale de l'invitation et la boîte de résultat — sur de vraies
// images ImGui, à la souris.

#include "ro/ui/entry_queue_window.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::ui::EntryQueueAction;
using ro::ui::EntryQueueFieldView;
using ro::ui::EntryQueueVerb;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
// Le pas du balayage : plus fin que le plus petit bouton dans les deux sens.
constexpr float kScanStepX = 10.0F;
constexpr float kScanStepY = 5.0F;

constexpr const char* kList = "###entry_queue";
constexpr const char* kStandby = "###entry_queue_standby";
constexpr const char* kReady = "###entry_queue_ready";
constexpr const char* kNotice = "###entry_queue_notice";
constexpr const char* kConfirm = "##entry_queue_confirm";
constexpr std::uint32_t kBadgeItem = 7828;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

// Une récompense iconée, une en texte seul.
const std::array<ro::ui::EntryQueueRewardView, 2> kRewards = {
    ro::ui::EntryQueueRewardView{
        .icon = ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels},
        .text = "3 × Insigne de bravoure",
        .item_id = kBadgeItem},
    ro::ui::EntryQueueRewardView{.text = "10 points KVM"}};

[[nodiscard]] EntryQueueFieldView Field(const char* name, bool party_apply) {
  return EntryQueueFieldView{.display_name = name,
                             .victory = "Une Réserve est détruite.",
                             .help = "Deux camps, une Réserve chacun.",
                             .reward_win = kRewards,
                             .reward_draw = std::span(kRewards).first(1),
                             .reward_lose = {},
                             .condition_text = "Niveau 80 minimum",
                             .team_text = "3 contre 3",
                             .private_apply = true,
                             .party_apply = party_apply,
                             .guild_apply = true};
}

[[nodiscard]] ImGuiWindow* Shown(const char* name) {
  ImGuiWindow* window = ImGui::FindWindowByName(name);
  return window != nullptr && window->Active ? window : nullptr;
}

struct QueueProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::EntryQueueWindowState state;
  std::array<EntryQueueFieldView, 2> fields = {Field("Tierra Gorge", true),
                                               Field("Flavius", false)};
  ro::ui::EntryQueueView view;

  QueueProbe() {
    view.fields = fields;
    view.link_ready = true;
  }

  EntryQueueAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const EntryQueueAction action = ro::ui::DrawEntryQueueWindows(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Balaie la fenêtre jusqu'à survoler le bouton `label` ; rend le point.
  std::optional<ImVec2> Find(const char* window_name, const char* label) {
    ImGuiWindow* window = Shown(window_name);
    REQUIRE(window != nullptr);
    const ImGuiID id = window->GetID(label);
    const ImVec2 low = window->Pos;
    const ImVec2 high(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y);
    for (float y = low.y + 1.0F; y < high.y; y += kScanStepY) {
      for (float x = low.x + 1.0F; x < high.x; x += kScanStepX) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        static_cast<void>(Frame());
        if (GImGui->HoveredId == id) {
          return ImVec2(x, y);
        }
      }
    }
    return std::nullopt;
  }

  // Un clic complet ; rend la première demande qu'il a fait naître.
  EntryQueueAction Click(ImVec2 at) {
    ImGuiIO& io = ImGui::GetIO();
    EntryQueueAction kept;
    const auto keep = [&kept](const EntryQueueAction& found) {
      if (kept.verb == EntryQueueVerb::kNothing) {
        kept = found;
      }
    };
    io.AddMousePosEvent(at.x, at.y);
    keep(Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    keep(Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    keep(Frame());
    keep(Frame());
    return kept;
  }

  // Balaie la liste jusqu'à survoler le lien de la récompense : son survol
  // nomme l'objet pour l'aperçu. Rend le point.
  std::optional<ImVec2> FindReward() {
    ImGuiWindow* window = Shown(kList);
    REQUIRE(window != nullptr);
    for (float y = window->Pos.y + 1.0F; y < window->Pos.y + window->Size.y; y += kScanStepY) {
      for (float x = window->Pos.x + 1.0F; x < window->Pos.x + window->Size.x; x += kScanStepX) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        if (Frame().link.hovered_id == kBadgeItem) {
          return ImVec2(x, y);
        }
      }
    }
    return std::nullopt;
  }

  EntryQueueAction ClickButton(const char* window_name, const char* label) {
    const std::optional<ImVec2> at = Find(window_name, label);
    REQUIRE(at.has_value());
    return Click(*at);
  }
};

}  // namespace

TEST_CASE("Inscription au champ : le clic sur le lien d'une récompense la décrit, rien d'autre",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.state.open = true;
  probe.Settle();
  const std::optional<ImVec2> link = probe.FindReward();
  REQUIRE(link.has_value());
  CHECK(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);
  ImGuiIO& io = ImGui::GetIO();
  ro::ui::ChatLinkAsk described;
  EntryQueueVerb verb = EntryQueueVerb::kNothing;
  for (int step = 0; step < 3; ++step) {
    if (step == 1) {
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    } else if (step == 2) {
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    }
    const EntryQueueAction action = probe.Frame();
    if (described.gesture == ro::ui::ChatLinkGesture::kNothing) {
      described = action.link;
    }
    verb = verb != EntryQueueVerb::kNothing ? verb : action.verb;
  }
  CHECK(described.gesture == ro::ui::ChatLinkGesture::kDescribe);
  CHECK(described.item_id == kBadgeItem);
  CHECK(verb == EntryQueueVerb::kNothing);
  CHECK(probe.state.confirming == EntryQueueVerb::kNothing);
}

TEST_CASE("Inscription au champ : le clic droit sur une récompense ouvre le menu des liens d'objet",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.state.open = true;
  probe.Settle();
  const std::optional<ImVec2> link = probe.FindReward();
  REQUIRE(link.has_value());
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  CHECK(probe.state.reward_menu.item_id == kBadgeItem);
  CHECK(probe.state.reward_menu.label == "3 × Insigne de bravoure");
  CHECK_FALSE(probe.state.reward_menu.armed);
  CHECK(GImGui->OpenPopupStack.Size == 1);
  CHECK(probe.state.confirming == EntryQueueVerb::kNothing);
}

TEST_CASE("Inscription au champ : le temps d'attente se lit en heures, minutes et secondes",
          "[ui][battleground]") {
  CHECK(ro::ui::FormatQueueWait(0) == "0 s");
  CHECK(ro::ui::FormatQueueWait(12) == "12 s");
  CHECK(ro::ui::FormatQueueWait(245) == "4 min 05 s");
  CHECK(ro::ui::FormatQueueWait(3600) == "1 h 00 min 00 s");
  CHECK(ro::ui::FormatQueueWait(3725) == "1 h 02 min 05 s");
}

TEST_CASE("Inscription au champ : l'inscription part après confirmation, pour le terrain choisi",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.state.open = true;
  probe.state.selected = 1;
  probe.Settle();

  // 🔴 LE BOUTON NE FAIT QUE DEMANDER : rien ne part avant le « oui ».
  const EntryQueueAction asked = probe.ClickButton(kList, "Solo");
  CHECK(asked.verb == EntryQueueVerb::kNothing);
  CHECK(probe.state.confirming == EntryQueueVerb::kApplySolo);
  REQUIRE(Shown(kConfirm) != nullptr);

  const EntryQueueAction confirmed = probe.ClickButton(kConfirm, "Oui");
  CHECK(confirmed.verb == EntryQueueVerb::kApplySolo);
  CHECK(confirmed.field_rank == 1);
  CHECK(confirmed.confirmed);
  CHECK(probe.state.confirming == EntryQueueVerb::kNothing);
}

TEST_CASE("Inscription au champ : une inscription refusée ne pose pas sa question",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.fields[1].party_apply = true;
  probe.fields[1].party_refusal = "[Vous devez être dans une party.]";
  probe.state.open = true;
  probe.state.selected = 1;
  probe.Settle();

  // 🔴 LE CLIC PART TOUT DE SUITE, NON CONFIRMÉ : `app` en dira le refus.
  const EntryQueueAction asked = probe.ClickButton(kList, "Party");
  CHECK(asked.verb == EntryQueueVerb::kApplyParty);
  CHECK(asked.field_rank == 1);
  CHECK_FALSE(asked.confirmed);
  CHECK(probe.state.confirming == EntryQueueVerb::kNothing);
  CHECK(Shown(kConfirm) == nullptr);
}

TEST_CASE("Inscription au champ : « Non » dans la confirmation n'envoie rien",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.state.open = true;
  probe.Settle();
  static_cast<void>(probe.ClickButton(kList, "Guilde"));
  REQUIRE(probe.state.confirming == EntryQueueVerb::kApplyGuild);
  const EntryQueueAction answer = probe.ClickButton(kConfirm, "Non");
  CHECK(answer.verb == EntryQueueVerb::kNothing);
  CHECK(probe.state.confirming == EntryQueueVerb::kNothing);
}

TEST_CASE("Inscription au champ : la file entrée pendant la confirmation l'annule",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.state.open = true;
  probe.Settle();
  static_cast<void>(probe.ClickButton(kList, "Solo"));
  REQUIRE(Shown(kConfirm) != nullptr);
  // La réponse du serveur arrive pendant que la boîte attend.
  probe.view.queued = true;
  probe.view.queued_name = "Tierra Gorge";
  const EntryQueueAction answer = probe.ClickButton(kConfirm, "Oui");
  CHECK(answer.verb == EntryQueueVerb::kNothing);
}

TEST_CASE("Inscription au champ : hors liaison, aucun bouton qui envoie n'est actif",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.state.open = true;
  probe.view.link_ready = false;
  probe.Settle();
  for (const char* label : {"Solo", "Party", "Guilde"}) {
    INFO(label);
    // Le bouton se survole — c'est ce qui porte sa raison —, mais le clic ne
    // demande rien.
    const EntryQueueAction answer = probe.ClickButton(kList, label);
    CHECK(answer.verb == EntryQueueVerb::kNothing);
    CHECK(probe.state.confirming == EntryQueueVerb::kNothing);
    CHECK(Shown(kConfirm) == nullptr);
  }
}

TEST_CASE("Inscription au champ : un mode que le terrain refuse reste éteint",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.state.open = true;
  probe.state.selected = 1;  // Flavius n'accepte pas les parties
  probe.Settle();
  static_cast<void>(probe.ClickButton(kList, "Party"));
  CHECK(probe.state.confirming == EntryQueueVerb::kNothing);
  // Le même bouton, sur un terrain qui l'accepte, demande sa confirmation.
  probe.state.selected = 0;
  probe.Settle();
  static_cast<void>(probe.ClickButton(kList, "Party"));
  CHECK(probe.state.confirming == EntryQueueVerb::kApplyParty);
}

TEST_CASE("Inscription au champ : « en attente » annule après confirmation, ou se ferme seule",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.state.standby_open = true;
  probe.view.queued = true;
  probe.view.queued_name = "Tierra Gorge";
  probe.view.count = 4;
  probe.view.waited_s = 65;
  probe.Settle();

  SECTION("annuler demande, puis part au « oui »") {
    const EntryQueueAction asked = probe.ClickButton(kStandby, "Annuler l'inscription");
    CHECK(asked.verb == EntryQueueVerb::kNothing);
    REQUIRE(probe.state.confirming == EntryQueueVerb::kCancel);
    const EntryQueueAction answer = probe.ClickButton(kConfirm, "Quitter la file");
    CHECK(answer.verb == EntryQueueVerb::kCancel);
  }
  SECTION("fermer garde la file") {
    const EntryQueueAction answer = probe.ClickButton(kStandby, "Fermer (rester en file)");
    CHECK(answer.verb == EntryQueueVerb::kNothing);
    CHECK_FALSE(probe.state.standby_open);
  }
  SECTION("pendant l'invitation, l'annulation est éteinte") {
    probe.view.invited = true;
    probe.view.invited_name = "Tierra Gorge";
    probe.view.invitation_s_left = 12;
    // La modale de l'invitation prend la main : on la congédie d'abord en
    // laissant croire qu'elle a déjà été montrée.
    probe.state.asked_ready = true;
    probe.Settle();
    static_cast<void>(probe.ClickButton(kStandby, "Annuler l'inscription"));
    CHECK(probe.state.confirming == EntryQueueVerb::kNothing);
  }
  SECTION("hors file, la fenêtre se referme d'elle-même") {
    probe.view.queued = false;
    static_cast<void>(probe.Frame());
    CHECK_FALSE(probe.state.standby_open);
  }
}

TEST_CASE("Inscription au champ : l'invitation s'impose, entrer ou refuser", "[ui][battleground]") {
  QueueProbe probe;
  probe.view.queued = true;
  probe.view.invited = true;
  probe.view.invited_name = "Flavius";
  probe.view.invitation_s_left = 17;
  probe.Settle();
  REQUIRE(Shown(kReady) != nullptr);
  CHECK(probe.state.asked_ready);

  SECTION("entrer") { CHECK(probe.ClickButton(kReady, "Entrer").verb == EntryQueueVerb::kAccept); }
  SECTION("refuser") {
    CHECK(probe.ClickButton(kReady, "Refuser").verb == EntryQueueVerb::kDecline);
  }
  SECTION("expirée, elle n'offre plus que de se fermer") {
    probe.view.invitation_s_left = 0;
    probe.Settle();
    CHECK(probe.ClickButton(kReady, "Entrer").verb == EntryQueueVerb::kNothing);
    static_cast<void>(probe.ClickButton(kReady, "Fermer"));
    probe.Settle();
    CHECK(Shown(kReady) == nullptr);
    // ⚠ ET ELLE NE REVIENT PAS D'ELLE-MÊME pour la même invitation.
    CHECK(probe.state.asked_ready);
  }
  SECTION("l'invitation retirée la referme") {
    probe.view.invited = false;
    probe.Settle();
    CHECK(Shown(kReady) == nullptr);
    CHECK_FALSE(probe.state.asked_ready);
  }
}

TEST_CASE("Inscription au champ : la boîte de résultat se lit, puis se dit vue",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.view.notice = "Votre niveau ne permet pas d'entrer sur ce terrain.";
  probe.Settle();
  REQUIRE(Shown(kNotice) != nullptr);
  CHECK(probe.ClickButton(kNotice, "Fermer").verb == EntryQueueVerb::kNoticeSeen);
}

TEST_CASE("Inscription au champ : la boîte de résultat attend qu'une confirmation se ferme",
          "[ui][battleground]") {
  QueueProbe probe;
  probe.state.open = true;
  probe.Settle();
  static_cast<void>(probe.ClickButton(kList, "Solo"));
  REQUIRE(Shown(kConfirm) != nullptr);
  probe.view.notice = "Inscription refusée.";
  probe.Settle();
  // 🔴 ELLE NE FERME PAS LA CONFIRMATION OUVERTE : elle attend son tour.
  CHECK(Shown(kConfirm) != nullptr);
  CHECK(Shown(kNotice) == nullptr);
  static_cast<void>(probe.ClickButton(kConfirm, "Non"));
  probe.Settle();
  CHECK(Shown(kNotice) != nullptr);
}
