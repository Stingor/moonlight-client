#include <array>
#include <string_view>

#include "game/ui_layouts.h"

namespace moonclient {
namespace {

using ro::ui::AnchorSide;

struct FactoryWindow {
  std::string_view key;
  ro::ui::WindowPlace place;
};

[[nodiscard]] constexpr ro::ui::WindowPlace At(AnchorSide across, AnchorSide down, float offset_x,
                                               float offset_y, float width, float height) {
  return ro::ui::WindowPlace{.across = across,
                             .down = down,
                             .offset_x = offset_x,
                             .offset_y = offset_y,
                             .width = width,
                             .height = height};
}

constexpr AnchorSide kStart = AnchorSide::kStart;
constexpr AnchorSide kCenter = AnchorSide::kCenter;
constexpr AnchorSide kEnd = AnchorSide::kEnd;

// LA PLACE DE CHAQUE FENÊTRE, en unités d'interface : voir `WindowPlace`. La
// conversation en bas à gauche, le groupe en bas à droite, la barre d'état et
// le compteur d'images en haut au centre, le sac et les deux barres de
// raccourcis au-dessus du bas de l'écran.
//
// ⚠ LA FENÊTRE DU LOGIN N'Y EST PAS : sa taille suit son contenu, et la poser
// la figerait.
constexpr std::array kFactoryWindows = {
    FactoryWindow{"##frame_meter", At(kCenter, kStart, -112.0F, 0.0F, 88.0F, 32.0F)},
    FactoryWindow{"##unlock_notice", At(kStart, kCenter, 58.0F, -52.5F, 383.0F, 167.0F)},
    FactoryWindow{"cash_shop", At(kEnd, kCenter, 0.0F, -68.5F, 753.0F, 759.0F)},
    FactoryWindow{"character", At(kStart, kCenter, 60.0F, -145.0F, 576.0F, 490.0F)},
    FactoryWindow{"chat", At(kStart, kEnd, 0.0F, 0.0F, 508.0F, 227.0F)},
    FactoryWindow{"craft_atlas", At(kStart, kStart, 60.0F, 60.0F, 810.0F, 420.0F)},
    FactoryWindow{"damage_meter", At(kStart, kStart, 376.0F, 34.0F, 286.0F, 215.0F)},
    FactoryWindow{"entry_queue", At(kCenter, kCenter, 0.0F, -0.5F, 448.0F, 355.0F)},
    FactoryWindow{"hotkeys", At(kStart, kStart, 60.0F, 60.0F, 420.0F, 380.0F)},
    FactoryWindow{"inventory", At(kCenter, kEnd, -72.5F, 85.0F, 359.0F, 263.0F)},
    FactoryWindow{"log", At(kCenter, kStart, 162.5F, 0.0F, 857.0F, 442.0F)},
    FactoryWindow{"login_settings", At(kStart, kCenter, 64.0F, -113.5F, 520.0F, 607.0F)},
    FactoryWindow{"menu", At(kCenter, kCenter, -241.5F, -91.5F, 221.0F, 245.0F)},
    FactoryWindow{"mvp_tracker", At(kStart, kStart, 60.0F, 60.0F, 720.0F, 460.0F)},
    FactoryWindow{"npc_dialog", At(kStart, kCenter, 280.0F, 60.0F, 460.0F, 300.0F)},
    FactoryWindow{"options", At(kEnd, kCenter, 9.0F, 68.5F, 647.0F, 763.0F)},
    FactoryWindow{"party_booking", At(kCenter, kCenter, 0.0F, 0.0F, 560.0F, 520.0F)},
    FactoryWindow{"party_friends", At(kEnd, kEnd, 0.0F, 0.0F, 364.0F, 342.0F)},
    FactoryWindow{"quest_log", At(kCenter, kCenter, 0.0F, 0.0F, 760.0F, 500.0F)},
    FactoryWindow{"shortcut_bar", At(kCenter, kEnd, 1.0F, 164.0F, 406.0F, 32.0F)},
    FactoryWindow{"shortcut_bar2", At(kCenter, kEnd, 1.0F, 129.0F, 406.0F, 32.0F)},
    FactoryWindow{"staff_tools", At(kStart, kStart, 60.0F, 60.0F, 420.0F, 320.0F)},
    // La petite fenêtre de statut, à droite de la fiche et alignée sur son haut.
    FactoryWindow{"status", At(kStart, kCenter, 646.0F, -324.0F, 302.0F, 132.0F)},
    FactoryWindow{"status_bar", At(kCenter, kStart, 174.0F, 16.0F, 638.0F, 84.0F)},
    FactoryWindow{"storage", At(kStart, kStart, 60.0F, 60.0F, 564.0F, 420.0F)},
    FactoryWindow{"world_map", At(kStart, kCenter, 60.0F, -40.0F, 820.0F, 700.0F)},
};

struct FactoryToggle {
  std::string_view key;
  bool on = false;
};

// LES FENÊTRES OUVERTES ET ÉPINGLÉES : la conversation seule est ouverte, et le
// compteur de dégâts flotte hors d'elle.
constexpr std::array kFactoryToggles = {
    FactoryToggle{"###companion_ai.pinned", false},
    FactoryToggle{"###companion_tactic.pinned", false},
    FactoryToggle{"###world_map", false},
    FactoryToggle{"achievements", false},
    FactoryToggle{"cart", false},
    FactoryToggle{"cart.pinned", false},
    FactoryToggle{"character", false},
    FactoryToggle{"character.homunculus.skills", true},
    FactoryToggle{"character.mercenary.skills", true},
    FactoryToggle{"character.pinned", false},
    FactoryToggle{"chat", true},
    FactoryToggle{"chat_macros", false},
    FactoryToggle{"craft_atlas", false},
    FactoryToggle{"damage_meter.docked", false},
    FactoryToggle{"inventory", false},
    FactoryToggle{"inventory.pinned", false},
    FactoryToggle{"log", false},
    FactoryToggle{"mvp_tracker", false},
    FactoryToggle{"mvp_tracker.pinned", false},
    FactoryToggle{"navigation", false},
    FactoryToggle{"packets", false},
    FactoryToggle{"party_friends", false},
    FactoryToggle{"party_friends.pinned", false},
    FactoryToggle{"quest_log", false},
    FactoryToggle{"staff_tools", false},
    FactoryToggle{"status", false},
};

}  // namespace

ro::ui::LayoutProfile FactoryLayout() {
  ro::ui::LayoutProfile profile;
  for (const FactoryWindow& window : kFactoryWindows) {
    profile.places.emplace(window.key, window.place);
  }
  for (const FactoryToggle& toggle : kFactoryToggles) {
    profile.toggles.emplace(toggle.key, toggle.on);
  }
  profile.minimap = ro::ui::MinimapLayout{};
  return profile;
}

}  // namespace moonclient
