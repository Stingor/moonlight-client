#include "game/hud_lock.h"

#include <algorithm>
#include <vector>

#include "ro/core/i18n.h"

#include "options/damage_meter_options.h"
#include "options/game_option_ids.h"
#include "options/menu_icons_options.h"
#include "options/target_options.h"
#include "party/party_options.h"

namespace moonclient {

void ListUnlockedHudShown(GameOptions& options, const UiStage::Windows& windows,
                          std::vector<const char*>& into) {
  into.clear();
  // L'avis éteint par le joueur : la liste reste vide, et l'avis ne paraît pas.
  if (!options.unlock_notice()) {
    return;
  }
  const ro::ui::ShortcutBarsLayout& bars = options.shortcut_bars();
  const bool bar_shown = std::ranges::any_of(
      bars.bars, [](const ro::ui::ShortcutBarLayout& bar) { return bar.shown; });
  if (bar_shown && !bars.locked) {
    into.push_back(ro::core::Tr("Barres de raccourcis"));
  }
  if (options.status_bar().shown && !options.status_bar().locked) {
    into.push_back(ro::core::Tr("États"));
  }
  if (options.stat_bars().shown && !options.stat_bars().locked) {
    into.push_back(ro::core::Tr("Jauges"));
  }
  // Déverrouillés, les deux cadres PvP s'ouvrent partout, hors PvP compris.
  if (!options.pvp_hud().locked) {
    into.push_back(ro::core::Tr("Cadres PvP"));
  }
  // Déverrouillé, le bandeau d'obtention montre sa poignée en permanence.
  if (!options.item_obtain().locked) {
    into.push_back(ro::core::Tr("Bandeau d'obtention"));
  }
  if (options.frame_meter() && !options.frame_meter_locked()) {
    into.push_back(ro::core::Tr("Compteur d'images"));
  }
  OptionPages& pages = options.pages();
  // La grille d'icônes de menu est toujours à l'écran.
  if (!pages.menu_icons.look().locked) {
    into.push_back(ro::core::Tr("Icônes de menu"));
  }
  const ro::ui::DamageMeterLook meter = pages.damage_meter.look();
  if (meter.shown && !meter.locked) {
    into.push_back(ro::core::Tr("Compteur de dégâts"));
  }
  // LE HUD DE CIBLE ET LA GRILLE DE GROUPE n'ont de vue que lorsqu'ils se
  // dessinent : sous le mode Ciblage, dans un groupe.
  if (windows.target_hud.view != nullptr && !pages.targeting.look().locked) {
    into.push_back(ro::core::Tr("Fenêtre de cible"));
  }
  if (windows.party.grid_view != nullptr && pages.party.grid().shown &&
      !pages.party.grid().locked) {
    into.push_back(ro::core::Tr("Grille de groupe"));
  }
}

void LockWholeHud(GameSettings& settings, const OptionTargets& to, std::uint32_t now_ms) {
  GameOptions& options = settings.options();
  // ⚠ SEULS CEUX QUI SONT OUVERTS : chaque bascule écrit le fichier de
  // réglages, et rebasculer un verrou fermé n'apprendrait rien à personne.
  if (!options.shortcut_bars().locked) {
    settings.SetRowSwitch(kShortcutLockedRow, true, to, now_ms);
  }
  if (!options.status_bar().locked) {
    settings.SetSwitch(GameOption::kStatusBarLocked, true, to, now_ms);
  }
  if (!options.stat_bars().locked) {
    settings.SetSwitch(GameOption::kStatBarsLocked, true, to, now_ms);
  }
  if (!options.pvp_hud().locked) {
    settings.SetSwitch(GameOption::kPvpHudLocked, true, to, now_ms);
  }
  // ⚠ SA CASE DIT « DÉVERROUILLÉ » : la décocher verrouille.
  if (!options.item_obtain().locked) {
    settings.SetSwitch(GameOption::kItemObtainUnlocked, false, to, now_ms);
  }
  if (!options.frame_meter_locked()) {
    settings.SetSwitch(GameOption::kFrameMeterLocked, true, to, now_ms);
  }
  OptionPages& pages = options.pages();
  if (!pages.menu_icons.look().locked) {
    settings.SetRowSwitch(MenuIconsRowId(MenuIconsRow::kLocked), true, to, now_ms);
  }
  if (!pages.damage_meter.look().locked) {
    settings.SetRowSwitch(DamageMeterRowId(DamageMeterRow::kLocked), true, to, now_ms);
  }
  if (!pages.targeting.look().locked) {
    settings.SetRowSwitch(TargetRowId(TargetRow::kLocked), true, to, now_ms);
  }
  if (!pages.party.grid().locked) {
    settings.SetRowSwitch(PartyGridRowId(PartyGridRow::kLocked), true, to, now_ms);
  }
}

}  // namespace moonclient
