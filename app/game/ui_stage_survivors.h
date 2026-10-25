#pragma once

#include <optional>
#include <span>

#include "ro/ui/map_sign_banner.h"
#include "ro/ui/options_window.h"
#include "ro/ui/survivors_album_window.h"
#include "ro/ui/survivors_chest_window.h"
#include "ro/ui/survivors_hud.h"
#include "ro/ui/survivors_offer_window.h"
#include "ro/ui/survivors_pause_window.h"
#include "ro/ui/survivors_ranking_window.h"
#include "ro/ui/survivors_records_window.h"
#include "ro/ui/survivors_shop.h"
#include "ro/ui/survivors_summary_window.h"

namespace moonclient {

// UNE IMAGE D'INTERFACE PENDANT UNE PARTIE DE SURVIVORS : le HUD, la fenêtre
// de l'écran en cours et le bandeau du nom de l'arène, rien d'autre. Les
// fenêtres du jeu ne se dessinent pas — elles ne sont ni fermées ni
// déplacées, et l'image qui suit la partie les redessine telles qu'avant.
//
// ⚠ LES VUES POINTENT DANS CE QU'APP A COMPOSÉ POUR L'IMAGE, et les états des
// fenêtres tiennent d'une image à l'autre : app les possède.
struct SurvivorsFrameView {
  // Le HUD, de l'arrivée au voile de la mort ; nul au bilan.
  const ro::ui::SurvivorsHudView* hud = nullptr;
  const ro::ui::SurvivorsOfferView* offer = nullptr;
  ro::ui::SurvivorsOfferState* offer_state = nullptr;
  const ro::ui::SurvivorsChestView* chest = nullptr;
  const ro::ui::SurvivorsPauseView* pause = nullptr;
  ro::ui::SurvivorsPauseState* pause_state = nullptr;
  const ro::ui::SurvivorsSummaryView* summary = nullptr;
  // L'Église ouverte par-dessus l'écran de fin.
  const ro::ui::SurvivorsShopView* church = nullptr;
  ro::ui::SurvivorsShopState* church_state = nullptr;
  // Le bandeau « Prontera Field » pendant la descente.
  const ro::ui::MapSignView* map_sign = nullptr;
  // Le tableau mondial ouvert par-dessus l'écran de fin d'une partie classée.
  const ro::ui::SurvivorsRankingView* ranking = nullptr;
  ro::ui::SurvivorsRankingState* ranking_state = nullptr;
  // LES OPTIONS DU JEU par-dessus la pause : la fenêtre du menu Échap, sur ses
  // pages. Nul quand elles ne s'y montrent pas.
  ro::ui::OptionsWindowState* options_state = nullptr;
  std::span<const ro::ui::OptionSectionView> options;
};

// CE QUE LES FENÊTRES DE L'IMAGE ONT DEMANDÉ : app en fait l'intention de
// l'image suivante.
struct SurvivorsAsk {
  std::optional<ro::ui::SurvivorsOfferAsk> offer;
  bool chest_proceed = false;
  ro::ui::SurvivorsPauseAsk pause;
  // Ce que les options du jeu ont changé : appliqué tout de suite, par le même
  // chemin que le panneau du jeu.
  ro::ui::OptionsWindowAction options;
  ro::ui::SurvivorsSummaryAsk summary;
  ro::ui::SurvivorsShopAsk church;
  ro::ui::SurvivorsRankingAsk ranking;
};

// LES FENÊTRES DE LA MÉTA (Q1) : l'Église, la Kafra, l'Album et les records,
// que l'accueil ouvre par-dessus lui, dans l'image du jeu. L'Église s'ouvre
// aussi par-dessus l'écran de fin, avec le même état. Les vues sont composées
// par le mode, qui lit et vide les réponses.
//
// LE TABLEAU MONDIAL DU CLASSEMENT (Q4) les rejoint : l'accueil l'ouvre par
// « Classement », l'écran de fin d'une partie classée par le sien. Il n'existe
// que service ouvert ; le mode le ferme sinon.
struct SurvivorsMetaWindows {
  ro::ui::SurvivorsShopState church;
  ro::ui::SurvivorsShopView church_view;
  ro::ui::SurvivorsShopAsk church_answer;
  ro::ui::SurvivorsShopState kafra;
  ro::ui::SurvivorsShopView kafra_view;
  ro::ui::SurvivorsShopAsk kafra_answer;
  ro::ui::SurvivorsAlbumState album;
  ro::ui::SurvivorsAlbumView album_view;
  ro::ui::SurvivorsRecordsState records;
  ro::ui::SurvivorsRecordsView records_view;
  ro::ui::SurvivorsRankingState ranking;
  ro::ui::SurvivorsRankingView ranking_view;
  ro::ui::SurvivorsRankingAsk ranking_answer;

  // Toutes fermées : une partie commence, ou l'accueil s'est fermé.
  void CloseAll() noexcept {
    church.open = false;
    church.confirming_refund = false;
    kafra.open = false;
    kafra.confirming_refund = false;
    album.open = false;
    records.open = false;
    ranking.open = false;
  }
  // Échap par-dessus l'écran de fin : la fenêtre du dessus se ferme, le
  // tableau mondial avant l'Église.
  void CloseTopOverSummary() noexcept {
    if (ranking.open) {
      ranking.open = false;
      return;
    }
    church.open = false;
  }
};

}  // namespace moonclient
