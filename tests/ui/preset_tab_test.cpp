// L'onglet des presets : chaque bouton rend son geste, sur sa ligne, et
// l'attente d'une touche — qui avale tout le clavier — ne survit pas à ce qui
// la montre.

#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/character_window.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::CharacterTab;
using ro::ui::CharacterWindowAction;
using ro::ui::PresetPieceView;
using ro::ui::PresetRowView;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr std::size_t kCapacity = 5;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

// La fiche ouverte sur l'onglet des presets, dessinée image par image.
struct PresetProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::CharacterWindowState state;
  ro::ui::CharacterHeaderView header;
  std::array<ro::ui::EquipSlotView, ro::ui::kEquipSlotCount> slots{};
  ro::ui::CharacterStatsView stats;
  ro::ui::CharacterTitlesView titles;
  ro::ui::GuildView guild;
  std::array<PresetPieceView, 2> pieces = {
      PresetPieceView{.name = "+7 Épée", .refine_level = 7},
      PresetPieceView{.name = "Flèche", .kind = ro::ui::PresetPieceKind::kAmmo}};
  std::vector<PresetRowView> rows = {
      PresetRowView{.name = "PvM", .pieces = pieces},
      PresetRowView{.name = "MvP", .with_costumes = true, .hotkey = "Ctrl+F5"}};

  PresetProbe() {
    state.open = true;
    state.tab = CharacterTab::kPresets;
    state.tab_request = CharacterTab::kPresets;
  }

  CharacterWindowAction Frame() {
    const ro::ui::PresetTabView view{.presets = rows, .capacity = kCapacity};
    ui.BeginFrame(kViewport, kFrameSeconds);
    const CharacterWindowAction action = ro::ui::DrawCharacterWindow(
        state, {}, header, slots, stats, view, titles, guild, {}, ro::ui::SkillPointTally{}, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

// L'enfant de l'onglet, que la fiche nomme « presets ».
[[nodiscard]] ImGuiWindow* PresetPane() {
  const ImGuiWindow* sheet = ImGui::FindWindowByName("###character");
  if (sheet == nullptr) {
    return nullptr;
  }
  for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    if (window->ParentWindow == sheet &&
        std::string_view(window->Name).find("/presets_") != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// L'identifiant d'un bouton de l'onglet : sur une ligne — sous son rang —, ou
// hors des lignes.
[[nodiscard]] ImGuiID ButtonId(const ImGuiWindow& pane, std::string_view label,
                               std::optional<int> row = std::nullopt) {
  ImGuiID seed = pane.ID;
  if (row.has_value()) {
    const int rank = *row;
    seed = ImHashData(&rank, sizeof(rank), seed);
  }
  return ImHashStr(label.data(), label.size(), seed);
}

// Balaie l'onglet jusqu'à survoler cet identifiant.
[[nodiscard]] bool Hover(PresetProbe& probe, ImGuiID wanted) {
  const ImGuiWindow* pane = PresetPane();
  REQUIRE(pane != nullptr);
  ImGuiIO& io = ImGui::GetIO();
  for (float y = pane->Pos.y; y < pane->Pos.y + pane->Size.y; y += 3.0F) {
    for (float x = pane->Pos.x; x < pane->Pos.x + pane->Size.x; x += 6.0F) {
      io.AddMousePosEvent(x, y);
      static_cast<void>(probe.Frame());
      if (ImGui::GetHoveredID() == wanted) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] bool HoverButton(PresetProbe& probe, std::string_view label,
                               std::optional<int> row = std::nullopt) {
  const ImGuiWindow* pane = PresetPane();
  REQUIRE(pane != nullptr);
  return Hover(probe, ButtonId(*pane, label, row));
}

// Un clic entier — enfoncé puis relâché — sur ce que la souris survole.
[[nodiscard]] CharacterWindowAction Click(PresetProbe& probe) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  return probe.Frame();
}

// Ce que le joueur a tapé dans le champ du nom.
void Type(PresetProbe& probe, std::string_view text) {
  std::snprintf(probe.state.presets.name.data(), probe.state.presets.name.size(), "%.*s",
                static_cast<int>(text.size()), text.data());
}

[[nodiscard]] CharacterWindowAction Press(PresetProbe& probe, ImGuiKey key) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(key, true);
  const CharacterWindowAction pressed = probe.Frame();
  io.AddKeyEvent(key, false);
  static_cast<void>(probe.Frame());
  return pressed;
}

}  // namespace

TEST_CASE("Presets : l'onglet se montre dans son propre cadre", "[ui][fiche][presets]") {
  PresetProbe probe;
  probe.Settle();
  CHECK(probe.state.tab == CharacterTab::kPresets);
  CHECK(PresetPane() != nullptr);
}

TEST_CASE("Presets : « Charger » et « Suppr » rendent le rang de LEUR ligne",
          "[ui][fiche][presets]") {
  PresetProbe probe;
  probe.Settle();
  REQUIRE(HoverButton(probe, "Charger", 1));
  const CharacterWindowAction loaded = Click(probe);
  CHECK(loaded.presets.load == std::optional<std::size_t>(1));
  CHECK_FALSE(loaded.presets.remove.has_value());

  REQUIRE(HoverButton(probe, "Suppr", 0));
  const CharacterWindowAction removed = Click(probe);
  CHECK(removed.presets.remove == std::optional<std::size_t>(0));
  CHECK_FALSE(removed.presets.load.has_value());
}

TEST_CASE("Presets : les trois boutons du haut rendent chacun le sien", "[ui][fiche][presets]") {
  PresetProbe probe;
  probe.Settle();
  REQUIRE(HoverButton(probe, "Tout déséquiper"));
  CHECK(Click(probe).presets.undress == std::optional<bool>(false));
  REQUIRE(HoverButton(probe, "+ costumes"));
  CHECK(Click(probe).presets.undress == std::optional<bool>(true));
  REQUIRE(HoverButton(probe, "Mon style"));
  const CharacterWindowAction style = Click(probe);
  CHECK(style.presets.toggle_style);
  CHECK_FALSE(style.presets.undress.has_value());
}

TEST_CASE("Presets : la case des costumes rend sa ligne et son nouvel état",
          "[ui][fiche][presets]") {
  PresetProbe probe;
  probe.Settle();
  REQUIRE(HoverButton(probe, "Piloter aussi les costumes", 1));
  const CharacterWindowAction toggled = Click(probe);
  CHECK(toggled.presets.costumes == std::optional<std::size_t>(1));
  // Cochée sur cette ligne-là : le clic demande de la décocher.
  CHECK_FALSE(toggled.presets.with_costumes);
}

TEST_CASE("Presets : « Effacer » n'existe que pour un preset qui a une touche",
          "[ui][fiche][presets]") {
  PresetProbe probe;
  probe.Settle();
  CHECK_FALSE(HoverButton(probe, "Effacer", 0));
  REQUIRE(HoverButton(probe, "Effacer", 1));
  CHECK(Click(probe).presets.clear_hotkey == std::optional<std::size_t>(1));
}

TEST_CASE("Presets : « Sauver l'actuel » rend le nom tapé, et vide le champ",
          "[ui][fiche][presets]") {
  PresetProbe probe;
  Type(probe, "Farm");
  probe.Settle();
  REQUIRE(HoverButton(probe, "Sauver l'actuel"));
  const CharacterWindowAction saved = Click(probe);
  CHECK(saved.presets.save_as == "Farm");
  CHECK(probe.state.presets.name[0] == '\0');
}

TEST_CASE("Presets : au plafond, seul un nom déjà pris s'enregistre", "[ui][fiche][presets]") {
  PresetProbe probe;
  probe.rows = {PresetRowView{.name = "A"}, PresetRowView{.name = "B"}, PresetRowView{.name = "C"},
                PresetRowView{.name = "D"}, PresetRowView{.name = "E"}};

  SECTION("un nouveau nom : le bouton reste gris") {
    Type(probe, "F");
    probe.Settle();
    REQUIRE(HoverButton(probe, "Sauver l'actuel"));
    CHECK(Click(probe).presets.save_as.empty());
  }
  SECTION("un nom déjà pris : il remplace son preset") {
    Type(probe, "C");
    probe.Settle();
    REQUIRE(HoverButton(probe, "Sauver l'actuel"));
    CHECK(Click(probe).presets.save_as == "C");
  }
}

TEST_CASE("Presets : « Définir » attend une touche, et Échap l'annule en le disant",
          "[ui][fiche][presets]") {
  PresetProbe probe;
  probe.Settle();
  REQUIRE(HoverButton(probe, "Définir", 1));
  static_cast<void>(Click(probe));
  CHECK(probe.state.presets.capturing == 1);

  const CharacterWindowAction cancelled = Press(probe, ImGuiKey_Escape);
  CHECK(probe.state.presets.capturing == -1);
  // Le mot qui empêche la même frappe de refermer la fiche.
  CHECK(cancelled.presets.escape_spent);
}

TEST_CASE("Presets : l'onglet tient en largeur, au cran le plus étroit qu'il permet",
          "[ui][fiche][presets]") {
  // Au cran étroit du mannequin, les trois boutons du haut débordaient.
  PresetProbe probe;
  probe.Settle();
  const ImGuiWindow* sheet = ImGui::FindWindowByName("###character");
  REQUIRE(sheet != nullptr);
  ImGui::SetWindowSize("###character", ImVec2(1.0F, sheet->Size.y));
  probe.Settle();
  const ImGuiWindow* pane = PresetPane();
  REQUIRE(pane != nullptr);
  INFO("contenu : " << pane->ContentSize.x << " px, place : " << pane->InnerRect.GetWidth()
                    << " px");
  CHECK(pane->ScrollMax.x == 0.0F);
}

TEST_CASE("Presets : l'attente d'une touche s'annule quand sa ligne défile hors de vue",
          "[ui][fiche][presets]") {
  PresetProbe probe;
  probe.rows.assign(8, PresetRowView{.name = "PvM", .pieces = probe.pieces});
  probe.Settle();
  ImGuiWindow* pane = PresetPane();
  REQUIRE(pane != nullptr);
  REQUIRE(pane->ScrollMax.y > 0.0F);
  probe.state.presets.capturing = 0;
  static_cast<void>(probe.Frame());
  REQUIRE(probe.state.presets.capturing == 0);

  ImGui::SetScrollY(pane, pane->ScrollMax.y);
  probe.Settle();
  CHECK(probe.state.presets.capturing == -1);
}

TEST_CASE("Presets : l'attente d'une touche ne survit pas à l'onglet", "[ui][fiche][presets]") {
  PresetProbe probe;
  probe.Settle();
  probe.state.presets.capturing = 0;
  static_cast<void>(probe.Frame());
  REQUIRE(probe.state.presets.capturing == 0);

  SECTION("un autre onglet") {
    probe.state.tab = CharacterTab::kEquipment;
    probe.state.tab_request = CharacterTab::kEquipment;
    probe.Settle();
    CHECK(probe.state.presets.capturing == -1);
  }
  SECTION("la fiche fermée") {
    probe.state.open = false;
    static_cast<void>(probe.Frame());
    CHECK(probe.state.presets.capturing == -1);
  }
  SECTION("une ligne qui n'existe plus") {
    probe.state.presets.capturing = 7;
    static_cast<void>(probe.Frame());
    CHECK(probe.state.presets.capturing == -1);
  }
}
