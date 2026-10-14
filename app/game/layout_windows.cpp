#include "game/layout_windows.h"

#include <string>

#include "ro/ui/achievement_window.h"
#include "ro/ui/cart_window.h"
#include "ro/ui/character_window.h"
#include "ro/ui/chat_macros.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/companion_ai_window.h"
#include "ro/ui/companion_tactic_window.h"
#include "ro/ui/craft_atlas_window.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/log_window.h"
#include "ro/ui/mvp_tracker_window.h"
#include "ro/ui/navigation_window.h"
#include "ro/ui/packet_window.h"
#include "ro/ui/party_friend_window.h"
#include "ro/ui/quest_window.h"
#include "ro/ui/staff_tools.h"
#include "ro/ui/status_window.h"
#include "ro/ui/world_map_window.h"

namespace moonclient {
namespace {

// Ce qui distingue, dans le profil, l'épingle d'une fenêtre de son ouverture.
constexpr const char* kPinSuffix = ".pinned";
// Le repli des Skills de l'homoncule et du mercenaire.
constexpr const char* kHomunculusSkillsKey = "character.homunculus.skills";
constexpr const char* kMercenarySkillsKey = "character.mercenary.skills";

[[nodiscard]] std::string PinKey(const char* window_id) {
  return std::string(window_id) + kPinSuffix;
}

}  // namespace

void RememberLayoutWindows(ro::ui::UiContext& ui, UiStage::Windows& windows) {
  // ── LES OUVERTURES ─────────────────────────────────────────────────────
  ui.RememberWindow(ro::ui::kLogWindowId, &windows.log.open);
  ui.RememberWindow(ro::ui::kChatWindowId, &windows.chat.main.open);
  ui.RememberWindow(ro::ui::kPacketWindowId, &windows.packets.open);
  // ⚠ UN `true` HÉRITÉ N'OUVRE PAS L'ÉTABLI SANS LE DROIT : le dessin le
  // revérifie.
  ui.RememberWindow(ro::ui::kStaffToolsId, &windows.staff_tools.open);
  ui.RememberWindow(ro::ui::kChatMacroId, &windows.macros.open);
  ui.RememberWindow(ro::ui::kCraftAtlasWindowId, &windows.craft_atlas.state.open);
  ui.RememberWindow(ro::ui::kNavigationWindowId, &windows.navigation.state.open);
  ui.RememberWindow(ro::ui::kWorldMapWindowId, &windows.navigation.world_map.open);
  ui.RememberWindow(ro::ui::kPartyFriendWindowId, &windows.party.window.open);
  // Le sac et la fiche montrent ce que le monde porte, et le suivent quand il
  // arrive après l'entrée.
  ui.RememberWindow(ro::ui::kInventoryWindowId, &windows.inventory.open);
  ui.RememberWindow(ro::ui::kCharacterWindowId, &windows.character.open);
  ui.RememberWindow(ro::ui::kStatusWindowId, &windows.status.state.open);
  // ⚠ LA CHARRETTE NE SE MONTRE QUE PORTÉE : ouverte par le profil d'un
  // personnage qui n'en a pas, elle attend sans paraître.
  ui.RememberWindow(ro::ui::kCartWindowId, &windows.cart.open);
  // Le journal et les succès se composent du livre du personnage, à chaque
  // image : arrivé après l'entrée, il s'y montre aussitôt.
  ui.RememberWindow(ro::ui::kQuestWindowId, &windows.quests.window.open);
  ui.RememberWindow(ro::ui::kAchievementWindowId, &windows.achievements.window.open);
  // ⚠ LE CARNET ÉTEINT SE REFERME de lui-même : seul l'interrupteur des
  // réglages l'allume.
  ui.RememberWindow(ro::ui::kMvpTrackerWindowId, &windows.mvp_tracker.window.open);

  // ── LES ÉPINGLES ───────────────────────────────────────────────────────
  ui.RememberWindow(PinKey(ro::ui::kInventoryWindowId), &windows.inventory.pinned);
  ui.RememberWindow(PinKey(ro::ui::kCartWindowId), &windows.cart.pinned);
  ui.RememberWindow(PinKey(ro::ui::kCharacterWindowId), &windows.character.pinned);
  ui.RememberWindow(PinKey(ro::ui::kPartyFriendWindowId), &windows.party.window.pinned);
  ui.RememberWindow(PinKey(ro::ui::kCompanionAiWindowId), &windows.companion_ai.pinned);
  ui.RememberWindow(PinKey(ro::ui::kCompanionTacticWindowId), &windows.companion_tactic.pinned);
  ui.RememberWindow(PinKey(ro::ui::kMvpTrackerWindowId), &windows.mvp_tracker.window.pinned);

  // ── LES REPLIS ET LE RANGEMENT ─────────────────────────────────────────
  ui.RememberWindow(kHomunculusSkillsKey, &windows.character.homunculus.skills_open);
  ui.RememberWindow(kMercenarySkillsKey, &windows.character.mercenary.skills_open);
  // La partie en fait la vérité des onglets : voir `ChatTabsView::FollowMeterDock`.
  ui.RememberWindow(std::string(kDamageMeterDockedKey), &windows.damage_meter.docked);
}

}  // namespace moonclient
