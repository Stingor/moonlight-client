#pragma once

#include "game/game_pieces.h"
#include "game/ui_stage.h"
#include "options/setting_links.h"

namespace moonclient {

// CE QU'UN LIEN DE RÉGLAGE OUVRE, d'où qu'il vienne : la conversation, une
// bulle, un dialogue de PNJ. Une seule fonction, pour que deux surfaces ne
// finissent pas par ouvrir deux choses différentes sous la même clé.
//
// 🔴 UN LIEN OUVRE, IL NE REFERME JAMAIS. La touche qui mène au même onglet
// bascule — un second appui referme —, mais un lien se suit pour VOIR : le
// suivre deux fois ne doit pas cacher ce qu'il montre.
//
// ⚠ LA DESTINATION A ÉTÉ JUGÉE OFFERTE À L'ANALYSE : on n'y revient pas ici.
void OpenSettingDestination(const SettingDestination& destination, UiStage::Windows& windows);

// LA DESTINATION DONT UN MAJ + CLIC DE CETTE IMAGE DEMANDE LE LIEN — une entrée
// ou un onglet du panneau, un onglet de la fiche, des succès, de Groupe / Amis
// ou des raccourcis —, ou nul. ⚠ UNE SEULE : le joueur n'a qu'une souris.
//
// ⚠ À LIRE AVANT QUE LES RÉPONSES DES SUCCÈS ET DU GROUPE NE SOIENT CONSOMMÉES :
// elles vivent dans `windows`, et leur étage les vide en les appliquant.
[[nodiscard]] const SettingDestination* AskedSettingLink(const UiStage::Answers& answers,
                                                         const UiStage::Windows& windows);

// CE QUE LE LECTEUR A À CETTE IMAGE : ses compagnons, et le catalogue des
// succès.
[[nodiscard]] SettingReach SettingReachOf(const GamePieces& pieces);

}  // namespace moonclient
