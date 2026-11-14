// « MONTRER MON ÉQUIPEMENT » ET « VOIR LES COSTUMES » DANS LA FICHE : ce que
// chaque case montre, ce qu'elle demande, sur quel onglet, et qu'elle tient sous
// le plancher de la fenêtre.
//
// 🔴 LA CASE NE CHANGE PAS D'ELLE-MÊME. Le réglage appartient au serveur : un
// clic le DEMANDE, et la case bascule à son écho. Une case qui se cocherait
// seule montrerait un état que le serveur n'a peut-être pas retenu.

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/render/skin_pieces.h"
#include "ro/ui/character_window.h"
#include "ro/ui/ui_context.h"

#include "support/temp_directory.h"

using ro::render::SkinPiece;
using ro::ui::CharacterTab;
using ro::ui::CharacterWindowAction;
using ro::ui::CompanionCaseView;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr std::string_view kShowEquipLabel = "Montrer mon équipement";
constexpr std::string_view kCostumesLabel = "Voir les costumes";
// La hauteur qu'on demande pour tomber sur le plancher : trop petite pour être
// jamais permise.
constexpr float kFloorProbePixels = 1.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

// La fiche ouverte sur l'onglet Équipement, dessinée image par image.
struct SheetProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::CharacterWindowState state;
  ro::ui::CharacterWindowView view;
  ro::ui::CharacterHeaderView header;
  std::array<ro::ui::EquipSlotView, ro::ui::kEquipSlotCount> slots{};
  ro::ui::CharacterStatsView stats;
  ro::ui::PresetTabView presets;
  ro::ui::CharacterTitlesView titles;
  ro::ui::GuildView guild;
  // TOUS LES COMPAGNONS QUI EXISTENT, empilés sous la colonne de gauche : c'est
  // la fiche la plus haute. `companion_count` en montre moins.
  std::array<CompanionCaseView, ro::ui::kCompanionCaseMost> companions = {
      CompanionCaseView{.rank = 0}, CompanionCaseView{.rank = 1}, CompanionCaseView{.rank = 2}};
  std::size_t companion_count = ro::ui::kCompanionCaseMost;

  SheetProbe() {
    state.open = true;
    state.tab = CharacterTab::kEquipment;
    view.equipment_shown = true;
  }

  void OpenCostumeTab() {
    state.tab = CharacterTab::kCostume;
    state.tab_request = CharacterTab::kCostume;
  }

  CharacterWindowAction Frame() {
    view.companions = std::span<const CompanionCaseView>(companions).first(companion_count);
    ui.BeginFrame(kViewport, kFrameSeconds);
    const CharacterWindowAction action =
        ro::ui::DrawCharacterWindow(state, view, header, slots, stats, presets, titles, guild, {},
                                    ro::ui::SkillPointTally{}, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

[[nodiscard]] ImGuiWindow* Sheet() { return ImGui::FindWindowByName("###character"); }

// L'enfant du mannequin, que la fiche nomme « mannequin ».
[[nodiscard]] ImGuiWindow* DollPane() {
  const ImGuiWindow* sheet = Sheet();
  if (sheet == nullptr) {
    return nullptr;
  }
  for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    if (window->ParentWindow == sheet &&
        std::string_view(window->Name).find("/mannequin_") != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// Balaie le volet du mannequin jusqu'à survoler la case de ce libellé.
[[nodiscard]] bool HoverCheckBox(SheetProbe& probe, std::string_view label) {
  const ImGuiWindow* pane = DollPane();
  REQUIRE(pane != nullptr);
  const ImGuiID wanted = ImHashStr(label.data(), label.size(), pane->ID);
  ImGuiIO& io = ImGui::GetIO();
  for (float y = pane->Pos.y; y < pane->Pos.y + pane->Size.y; y += 3.0F) {
    for (float x = pane->Pos.x; x < pane->Pos.x + pane->Size.x; x += 10.0F) {
      io.AddMousePosEvent(x, y);
      static_cast<void>(probe.Frame());
      if (ImGui::GetHoveredID() == wanted) {
        return true;
      }
    }
  }
  return false;
}

// Un clic entier — enfoncé puis relâché — sur ce que la souris survole. Rend
// ce que la fiche a demandé à l'image du relâché.
[[nodiscard]] CharacterWindowAction Click(SheetProbe& probe) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  return probe.Frame();
}

}  // namespace

namespace {

// Ce que la fiche montre une fois descendue sur son plancher.
struct FloorReading {
  float sheet_height = 0.0F;
  float pane_content = 0.0F;
  float pane_room = 0.0F;
  float pane_overflow = 0.0F;
};

[[nodiscard]] FloorReading ReadFloor(std::size_t companion_count,
                                     std::string_view gif_message = {}) {
  SheetProbe probe;
  probe.companion_count = companion_count;
  probe.view.doll_gif.message = gif_message;
  probe.Settle();
  REQUIRE(Sheet() != nullptr);
  // ⚠ D'ABORD HAUTE, PUIS ÉCRASÉE : la hauteur d'ouverture peut déjà être le
  // plancher, et la descente ne prouverait alors rien.
  ImGui::SetWindowSize("###character", ImVec2(Sheet()->Size.x, kViewport.height_px));
  probe.Settle();
  REQUIRE(DollPane() != nullptr);
  const float tall = DollPane()->Size.y;
  ImGui::SetWindowSize("###character", ImVec2(Sheet()->Size.x, kFloorProbePixels));
  probe.Settle();
  const ImGuiWindow* pane = DollPane();
  REQUIRE(pane != nullptr);
  // L'essai ne vaut que si la fenêtre est bien descendue sur son plancher.
  REQUIRE(pane->Size.y < tall);
  return FloorReading{.sheet_height = Sheet()->Size.y,
                      .pane_content = pane->ContentSize.y,
                      .pane_room = pane->Size.y,
                      .pane_overflow = pane->ScrollMax.y};
}

}  // namespace

TEST_CASE("Montrer mon équipement : la case tient sous le plancher, trois compagnons compris",
          "[ui][fiche][show_equip]") {
  // ⚠ SOUS LE PLANCHER, LA DERNIÈRE RANGÉE SE FERAIT COUPER — et c'est la case,
  // posée sous les poses. Elle ne se verrait pas comme un manque de place mais
  // comme une case absente.
  for (std::size_t count = 0; count <= ro::ui::kCompanionCaseMost; ++count) {
    const FloorReading floor = ReadFloor(count);
    INFO(count << " compagnon(s) — contenu du volet : " << floor.pane_content
               << " px, place : " << floor.pane_room << " px");
    CHECK(floor.pane_overflow == 0.0F);
  }
}

TEST_CASE("Montrer mon équipement : la case tient sous le plancher, sous le GIF et son message",
          "[ui][fiche][show_equip]") {
  // 🔴 LE GIF AJOUTE SES RANGÉES ENTRE LES POSES ET LA CASE : la ligne
  // d'échelle, puis un message replié sur plusieurs lignes. Le plancher les
  // MESURE ; une constante devinée laissait la case repasser sous le bord.
  constexpr std::string_view kLongMessage =
      "GIF enregistré : avatar_Un_pseudo_assez_long_pour_replier_Combat_(animé)_d3.gif, "
      "et encore quelques mots pour faire une troisième ligne dans le volet";
  for (std::size_t count = 0; count <= ro::ui::kCompanionCaseMost; ++count) {
    const FloorReading floor = ReadFloor(count, kLongMessage);
    INFO(count << " compagnon(s) — contenu du volet : " << floor.pane_content
               << " px, place : " << floor.pane_room << " px");
    CHECK(floor.pane_overflow == 0.0F);
  }
  // Et le message fait bien grandir le plancher.
  CHECK(ReadFloor(0, kLongMessage).sheet_height > ReadFloor(0).sheet_height);
}

TEST_CASE("Montrer mon équipement : la case tient sous le plancher, bouton du GIF sous les poses",
          "[ui][fiche][show_equip]") {
  // Un libellé traduit trop long pour la ligne des poses fait passer le bouton
  // dessous : une rangée de plus, que le plancher doit loger aussi.
  const ro::test_support::TempDirectory folder;
  folder.WriteText("long.txt",
                   "fr: Générer le GIF\n"
                   "tr: Générer le GIF animé du personnage, fond transparent\n");
  REQUIRE(ro::core::LoadLanguage(folder.path() / "long.txt", "xx").has_value());
  const FloorReading wrapped = ReadFloor(ro::ui::kCompanionCaseMost);
  ro::core::UseFrench();
  const FloorReading one_line = ReadFloor(ro::ui::kCompanionCaseMost);
  INFO("contenu du volet : " << wrapped.pane_content << " px, place : " << wrapped.pane_room
                             << " px");
  CHECK(wrapped.pane_content > one_line.pane_content);
  CHECK(wrapped.pane_overflow == 0.0F);
}

TEST_CASE("Montrer mon équipement : le plancher ne grandit qu'au-delà de deux compagnons",
          "[ui][fiche][show_equip]") {
  // La fiche à deux compagnons ou moins garde la hauteur qu'elle avait : seul
  // le troisième lui ajoute sa rangée.
  const float bare = ReadFloor(0).sheet_height;
  CHECK(ReadFloor(1).sheet_height == bare);
  CHECK(ReadFloor(2).sheet_height == bare);
  CHECK(ReadFloor(ro::ui::kCompanionCaseMost).sheet_height > bare);
  // Et elle reste dans l'écran.
  CHECK(ReadFloor(ro::ui::kCompanionCaseMost).sheet_height <= kViewport.height_px);
}

TEST_CASE("Montrer mon équipement : un clic demande l'autre état, sans le montrer",
          "[ui][fiche][show_equip]") {
  SheetProbe probe;
  probe.Settle();
  REQUIRE(HoverCheckBox(probe, kShowEquipLabel));
  const CharacterWindowAction released = Click(probe);
  REQUIRE(released.show_equipment.has_value());
  CHECK_FALSE(*released.show_equipment);
  CHECK_FALSE(released.show_costumes.has_value());
  // La fenêtre n'a rien retenu : c'est l'écho du serveur qui le fera.
  CHECK(probe.view.equipment_shown == true);
  // Et l'image suivante ne redemande rien.
  CHECK_FALSE(probe.Frame().show_equipment.has_value());
}

TEST_CASE("Réglages de la fiche : chaque onglet porte SA case, et pas celle de l'autre",
          "[ui][fiche][show_equip][show_costumes]") {
  SECTION("l'onglet Équipement") {
    SheetProbe probe;
    probe.Settle();
    CHECK_FALSE(HoverCheckBox(probe, kCostumesLabel));
  }
  SECTION("l'onglet Costume") {
    SheetProbe probe;
    probe.OpenCostumeTab();
    probe.Settle();
    REQUIRE(probe.state.tab == CharacterTab::kCostume);
    CHECK_FALSE(HoverCheckBox(probe, kShowEquipLabel));
    CHECK(HoverCheckBox(probe, kCostumesLabel));
  }
}

TEST_CASE("Voir les costumes : un clic demande l'autre état, sans le montrer",
          "[ui][fiche][show_costumes]") {
  // ⚠ MASQUÉS, ET L'ÉQUIPEMENT MONTRÉ : ni le défaut de la case, ni l'état de
  // sa sœur. Une case qui lirait l'un ou l'autre demanderait l'inverse.
  SheetProbe probe;
  probe.OpenCostumeTab();
  probe.view.costumes_shown = false;
  probe.Settle();
  REQUIRE(probe.view.equipment_shown == true);
  REQUIRE(HoverCheckBox(probe, kCostumesLabel));
  const CharacterWindowAction released = Click(probe);
  REQUIRE(released.show_costumes.has_value());
  CHECK(*released.show_costumes);
  CHECK_FALSE(released.show_equipment.has_value());
  CHECK(probe.view.costumes_shown == false);
  CHECK_FALSE(probe.Frame().show_costumes.has_value());
}

TEST_CASE("Voir les costumes : cochée tant que le serveur n'a rien dit",
          "[ui][fiche][show_costumes]") {
  // 🔴 À L'INVERSE DE SA SŒUR : un personnage neuf montre ses costumes. Une case
  // décochée d'office ferait demander à les MONTRER au premier clic — ce qui ne
  // changerait rien, et le joueur croirait la case cassée.
  SheetProbe probe;
  probe.OpenCostumeTab();
  probe.Settle();
  REQUIRE_FALSE(probe.view.costumes_shown.has_value());
  REQUIRE(HoverCheckBox(probe, kCostumesLabel));
  const CharacterWindowAction released = Click(probe);
  REQUIRE(released.show_costumes.has_value());
  CHECK_FALSE(*released.show_costumes);
}
