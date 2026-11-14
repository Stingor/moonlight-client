// LES RANGÉES DE LA FENÊTRE D'OPTIONS, vues de la souris et du clavier : le
// champ de texte qu'on tape, le texte relu trop long pour lui, la mise en garde
// qui vient sans aide, le bouton qui ramène une rangée à sa référence.

#include <optional>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/options_window.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 4;
// Le pas du balayage d'une bande : plus fin que la hauteur d'une rangée, et
// que la largeur du marqueur d'aide.
constexpr float kScanStepX = 4.0F;
constexpr float kScanStepY = 3.0F;
// La hauteur de la bande balayée sous le repère : une rangée et ses marges.
constexpr float kBandHeight = 30.0F;
constexpr float kMarkerHeight = 2.0F;
constexpr int kMarkerRow = 1;
constexpr int kProbedRow = 2;
constexpr const char* kOptionsWindow = "###options";

const ro::ui::SkinArt kNoArt;

// La fenêtre d'options à une section : un repère — un cadre plat qui dit où il
// est peint —, puis la rangée essayée, juste dessous. Le texte d'une rangée
// `kText` est tenu comme le tient la partie : ce que la fenêtre rend revient à
// l'image suivante.
struct OptionsProbe {
  ro::ui::UiContext ui;
  ro::ui::OptionsWindowState state;
  ro::ui::OptionRowView probed;
  std::string text;
  float band_top = 0.0F;
  // ⚠ LE BORD GAUCHE DU CONTENU, et non de la fenêtre : le champ de filtre de la
  // navigation tombe à la même hauteur que la rangée essayée.
  float band_left = 0.0F;

  explicit OptionsProbe(ro::ui::OptionRowView row) : probed(row) {
    state.open = true;
    probed.id = kProbedRow;
  }

  ro::ui::OptionsWindowAction Frame() {
    probed.text = text.c_str();
    const std::vector<ro::ui::OptionRowView> rows = {
        {.id = kMarkerRow,
         .kind = ro::ui::OptionKind::kCanvas,
         .canvas_height = kMarkerHeight,
         .paint =
             [this](ImDrawList&, ImVec2 min, ImVec2 max) {
               band_top = max.y;
               band_left = min.x;
             }},
        probed};
    const ro::ui::OptionSectionView section{.title = "Essai", .rows = rows};
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::OptionsWindowAction asked = ro::ui::DrawOptionsWindow(
        state, std::span<const ro::ui::OptionSectionView>(&section, 1), kNoArt);
    ui.EndFrame();
    if (asked.changed == kProbedRow) {
      text = asked.text;
    }
    return asked;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Balaie la bande de la rangée essayée jusqu'à ce que `found` soit vrai.
  template <typename Found>
  [[nodiscard]] std::optional<ImVec2> Scan(Found found) {
    Settle();
    const ImGuiWindow* window = ImGui::FindWindowByName(kOptionsWindow);
    REQUIRE(window != nullptr);
    const float top = band_top;
    ImGuiIO& io = ImGui::GetIO();
    for (float y = top; y < top + kBandHeight; y += kScanStepY) {
      for (float x = band_left; x < window->Pos.x + window->Size.x; x += kScanStepX) {
        io.AddMousePosEvent(x, y);
        static_cast<void>(Frame());
        if (found()) {
          return ImVec2(x, y);
        }
      }
    }
    return std::nullopt;
  }

  void Click(ImVec2 at) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(at.x, at.y);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    Settle();
  }

  void Press(ImGuiKey key) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(key, true);
    static_cast<void>(Frame());
    io.AddKeyEvent(key, false);
    Settle();
  }

  // Clique dans le champ, curseur porté en fin de texte.
  void FocusTextEnd() {
    const std::optional<ImVec2> field =
        Scan([] { return ImGui::GetMouseCursor() == ImGuiMouseCursor_TextInput; });
    REQUIRE(field.has_value());
    Click(*field);
    REQUIRE(GImGui->ActiveId != 0);
    REQUIRE(GImGui->InputTextState.ID == GImGui->ActiveId);
    Press(ImGuiKey_End);
  }
};

[[nodiscard]] bool TooltipShown() {
  for (const ImGuiWindow* window : GImGui->Windows) {
    if ((window->Flags & ImGuiWindowFlags_Tooltip) != 0 && window->Active) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Options : une frappe dans un champ de texte rend la rangée et son texte",
          "[ui][options]") {
  OptionsProbe probe({.label = "Fichier", .kind = ro::ui::OptionKind::kText});
  probe.FocusTextEnd();
  ImGui::GetIO().AddInputCharactersUTF8("Zoé");
  bool changed = false;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    changed = probe.Frame().changed == kProbedRow || changed;
  }
  CHECK(changed);
  CHECK(probe.text == "Zoé");
}

TEST_CASE("Options : un texte trop long se coupe à une frontière de caractère", "[ui][options]") {
  OptionsProbe probe({.label = "Fichier", .kind = ro::ui::OptionKind::kText});
  // « é » pèse deux octets, et le premier seul tient dans le champ.
  const std::string kept(ro::ui::kOptionTextBytes - 2, 'a');
  probe.text = kept + "é";
  probe.FocusTextEnd();
  CHECK(GImGui->InputTextState.TextLen == static_cast<int>(kept.size()));
  // Le caractère coupé ne prend pas la dernière place : une frappe y entre.
  ImGui::GetIO().AddInputCharactersUTF8("b");
  probe.Settle();
  CHECK(probe.text == kept + "b");
}

TEST_CASE("Options : une mise en garde sans aide a son marqueur", "[ui][options]") {
  OptionsProbe probe({.label = "Réglage",
                      .help_warning = "Ce réglage surprend.",
                      .kind = ro::ui::OptionKind::kToggle});
  CHECK(probe.Scan(TooltipShown).has_value());
}

TEST_CASE("Options : le bouton de retour rend sa rangée, et le dit", "[ui][options]") {
  OptionsProbe probe({.label = "Réglage",
                      .kind = ro::ui::OptionKind::kNote,
                      .revert = "Rétablir",
                      .revert_help = "Reprendre le réglage du thème."});
  // Sa bulle le trouve : rien d'autre dans la bande n'en montre.
  const std::optional<ImVec2> button = probe.Scan(TooltipShown);
  REQUIRE(button.has_value());
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const ro::ui::OptionsWindowAction asked = probe.Frame();
  CHECK(asked.changed == kProbedRow);
  CHECK(asked.revert);
}

TEST_CASE("Options : une rangée déjà à sa référence n'a pas de bouton de retour", "[ui][options]") {
  OptionsProbe probe({.label = "Réglage",
                      .kind = ro::ui::OptionKind::kNote,
                      .revert = "Rétablir",
                      .revert_help = "Reprendre le réglage du thème.",
                      .at_reference = true});
  CHECK_FALSE(probe.Scan(TooltipShown).has_value());
}

TEST_CASE("Options : les onglets qu'une page dessine reçoivent la demande et rendent leur lien",
          "[ui][options][lien]") {
  ro::ui::UiContext ui;
  ro::ui::OptionsWindowState state;
  state.open = true;
  std::optional<ro::ui::OptionBodyTabs> received;
  int link_to_report = -1;
  const std::vector<ro::ui::OptionSectionView> sections = {
      {.title = "Essai", .body = [&](const ro::ui::SkinArt&, ro::ui::OptionBodyTabs& tabs) {
         received = tabs;
         tabs.link = link_to_report;
       }}};
  const auto frame = [&](bool in_game) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::OptionsWindowAction asked =
        in_game ? ro::ui::DrawOptionsWindow(state, sections, kNoArt)
                : ro::ui::DrawLoginSettingsWindow(state, sections, kNoArt);
    ui.EndFrame();
    return asked;
  };
  static_cast<void>(frame(true));

  // UN LIEN QU'ON SUIT demande l'onglet : la page le reçoit une fois.
  state.section_tab_wanted = 1;
  static_cast<void>(frame(true));
  REQUIRE(received.has_value());
  CHECK(received->wanted == 1);
  CHECK(received->offers_links);
  CHECK(state.section_tab_wanted == -1);
  static_cast<void>(frame(true));
  CHECK(received->wanted == -1);

  // LE MAJ + CLIC D'UN DE SES ONGLETS devient celui de la section.
  link_to_report = 1;
  const ro::ui::OptionsWindowAction asked = frame(true);
  CHECK(asked.link_section == 0);
  CHECK(asked.link_tab == 1);

  // ⚠ SANS CHAT, LA PAGE NE L'OFFRE PAS.
  link_to_report = -1;
  static_cast<void>(frame(false));
  CHECK_FALSE(received->offers_links);
}

TEST_CASE("Options : un panneau se dessine dans la page, et son « changé » rend la rangée",
          "[ui][options]") {
  int drawn = 0;
  bool report_change = false;
  ro::ui::OptionRowView row{.kind = ro::ui::OptionKind::kPanel};
  row.panel = [&drawn, &report_change](const ro::ui::SkinArt&) {
    ++drawn;
    ImGui::TextUnformatted("panneau");
    return report_change;
  };
  OptionsProbe probe(row);
  probe.Settle();
  CHECK(drawn > 0);
  CHECK(probe.Frame().changed != kProbedRow);
  report_change = true;
  CHECK(probe.Frame().changed == kProbedRow);
}
