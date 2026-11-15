// LA FENÊTRE D'ENCHANTEMENT : elle se dessine ouverte, se ferme par sa croix ou par
// l'état, et une ouverture neuve remet ses choix à zéro.

#include "ro/ui/item_enchant_window.h"

#include <array>
#include <string_view>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::ItemEnchantPage;
using ro::ui::ItemEnchantPieceView;
using ro::ui::ItemEnchantSlotView;
using ro::ui::ItemEnchantView;
using ro::ui::ItemEnchantWindowAction;
using ro::ui::ItemEnchantWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr std::uint16_t kArmorSlot = 12;

struct Probe {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ItemEnchantWindowState state;
  std::array<ItemEnchantPieceView, 1> candidates{
      ItemEnchantPieceView{.client_slot = kArmorSlot,
                           .item = {.item_id = 450206, .name = "Snowflower Armor"},
                           .chosen = true}};
  std::array<ItemEnchantSlotView, 2> slots{ItemEnchantSlotView{.slot = 0, .card_slot = true},
                                           ItemEnchantSlotView{.slot = 3, .next = true}};
  std::array<std::string_view, 1> blockers{"Pas assez de zenys."};
  ItemEnchantView view;

  Probe() {
    ro::ui::OpenItemEnchantOn(state, 1);
    view.title = "Enchantement";
    view.instruction = "Choisissez une pièce.";
    view.candidates = candidates;
    view.chosen = candidates[0].item;
    view.slots = slots;
    view.enchant_open = true;
    view.random_open = true;
    view.blockers = blockers;
    view.action_label = "Tenter";
    view.confirmation = "Tenter ?";
  }

  ItemEnchantWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ItemEnchantWindowAction action = ro::ui::DrawItemEnchantWindow(state, view, art);
    ui.EndFrame();
    return action;
  }
};

}  // namespace

TEST_CASE("Fenêtre d'enchantement : une ouverture neuve remet ses choix à zéro",
          "[ui][item_enchant]") {
  ItemEnchantWindowState state;
  state.page = ItemEnchantPage::kReset;
  state.perfect = true;
  state.enchant_item_id = 311092;
  state.upgrade_slot = 3;
  state.asking = true;
  ro::ui::OpenItemEnchantOn(state, 7);
  CHECK(state.open);
  CHECK(state.opening_seen == 7);
  CHECK(state.page == ItemEnchantPage::kEnchant);
  CHECK_FALSE(state.perfect);
  CHECK(state.enchant_item_id == 0);
  CHECK(state.upgrade_slot == ro::ui::kItemEnchantNoSlot);
  CHECK_FALSE(state.asking);
}

TEST_CASE("Fenêtre d'enchantement : ouverte, elle se dessine sans rien demander",
          "[ui][item_enchant]") {
  Probe probe;
  for (int frame = 0; frame < 4; ++frame) {
    const ItemEnchantWindowAction action = probe.Frame();
    CHECK_FALSE(action.close);
    CHECK_FALSE(action.enchant);
  }
  CHECK(probe.state.open);
}

TEST_CASE("Fenêtre d'enchantement : une case sans tirage prend le choix sûr d'office",
          "[ui][item_enchant]") {
  Probe probe;
  probe.view.random_open = false;
  probe.view.perfect_open = true;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.perfect);
  // Et l'inverse : une case sans choix sûr revient au tirage.
  probe.view.random_open = true;
  probe.view.perfect_open = false;
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.perfect);
}

TEST_CASE("Fenêtre d'enchantement : l'état baissé se dit comme une fermeture",
          "[ui][item_enchant]") {
  Probe probe;
  static_cast<void>(probe.Frame());
  probe.state.open = false;
  // Fermée avant l'image : elle ne dessine rien, et ne demande rien.
  const ItemEnchantWindowAction action = probe.Frame();
  CHECK_FALSE(action.enchant);
}
