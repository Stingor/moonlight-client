#pragma once

#include <cstdint>
#include <vector>

#include "game/game_settings.h"
#include "game/ui_stage.h"
#include "options/game_options.h"
#include "options/option_targets.h"

namespace moonclient {

// LES VERROUS DE L'INTERFACE, TOUS ENSEMBLE — ce que l'avis de pose regarde et
// ce que sa case referme. Voir `ro/ui/unlock_notice.h`.
//
// ⚠ CHAQUE ÉLÉMENT GARDE SON VERROU, et rien ici ne les fusionne : le panneau
// d'options continue de les régler un par un. Ce fichier ne fait que les
// parcourir.

// LES NOMS DES ÉLÉMENTS DÉVERROUILLÉS À L'ÉCRAN, déjà traduits, dans `into`
// vidé d'abord ; vide quand tout est verrouillé. L'avis les cite, pour que le
// joueur sache ce qu'il a ouvert.
//
// 🔴 À L'ÉCRAN, ET PAS SEULEMENT DÉVERROUILLÉ. Plusieurs verrous sont ouverts
// par défaut sur des éléments éteints par défaut (jauges, compteur de dégâts,
// grille de groupe) : l'avis paraîtrait chez tout le monde pour des cadres que
// personne ne voit.
void ListUnlockedHudShown(GameOptions& options, const UiStage::Windows& windows,
                          std::vector<const char*>& into);

// Verrouille tous les éléments, montrés ou non, par le même chemin que le
// panneau : le fichier de réglages le retient, et le panneau le montre.
void LockWholeHud(GameSettings& settings, const OptionTargets& to, std::uint32_t now_ms);

}  // namespace moonclient
