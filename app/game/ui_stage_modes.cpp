// LES JUMELLES DE `UiStage::Draw` : une image d'interface qui ne montre QUE la
// veille, l'écran de chargement, celui de mise à jour ou une partie de
// survivors.

#include "ro/ui/loading_screen.h"
#include "ro/ui/survivors_church_window.h"
#include "ro/ui/survivors_ranking_window.h"

#include "game/ui_stage.h"

namespace moonclient {

void UiStage::DrawAfkOnly(const ro::ui::AfkClockView& clock, const OverWorldPaint& over_world) {
  // ⚠ L'IMAGE S'OUVRE ET SE FERME QUAND MÊME : ImGui tient son état d'entrée
  // d'une image à l'autre, et lui en sauter une lui ferait perdre le compte de
  // ce que la souris survole — donc le laisserait croire, au réveil, qu'un
  // bouton est encore enfoncé.
  backend_.BeginFrame(*ui_);
  // LE VOILE RESTE PENDANT LA VEILLE : il appartient au monde, que la veille continue de montrer.
  PaintOverWorld(over_world);
  ro::ui::DrawAfkClock(clock);
  ui_->EndFrame();
  backend_.Draw();
}

void UiStage::DrawLoadingOnly(const ro::ui::LoadingScreenView& view) {
  // ⚠ MÊME RAISON QUE LA VEILLE : l'image s'ouvre et se ferme, pour qu'ImGui
  // garde le compte de ce que la souris et le clavier font pendant l'attente.
  backend_.BeginFrame(*ui_);
  ro::ui::DrawLoadingScreen(view);
  ui_->EndFrame();
  backend_.Draw();
}

SurvivorsAsk UiStage::DrawSurvivorsOnly(const SurvivorsFrameView& view) {
  // ⚠ MÊME RAISON QUE LA VEILLE : l'image d'ImGui s'ouvre et se ferme, pour
  // qu'elle garde le compte de la souris et du clavier pendant la partie.
  backend_.BeginFrame(*ui_);
  SurvivorsAsk ask;
  // LE HUD SOUS TOUT : il peint sur la liste de fond et n'ouvre aucune fenêtre.
  if (view.hud != nullptr) ro::ui::DrawSurvivorsHud(*view.hud);
  if (view.map_sign != nullptr) ro::ui::DrawMapSign(*view.map_sign, ui_->MapSignFont());
  if (view.offer != nullptr && view.offer_state != nullptr) {
    ask.offer = ro::ui::DrawSurvivorsOfferWindow(*view.offer_state, *view.offer, skin_);
  }
  if (view.chest != nullptr) {
    ask.chest_proceed = ro::ui::DrawSurvivorsChestWindow(*view.chest, skin_);
  }
  // LE CODEX RECOUVRE LA PAUSE : elle ne se dessine pas sous lui, si bien
  // qu'aucun clic ne reprend la partie tant qu'il est ouvert, et le fermer y
  // ramène.
  const bool codex_shown = view.codex != nullptr && view.codex_state != nullptr &&
                           view.album != nullptr && view.album_state != nullptr;
  if (codex_shown) {
    ask.codex = ro::ui::DrawSurvivorsCodexWindow(*view.codex_state, *view.codex, *view.album_state,
                                                 *view.album, skin_);
  } else if (view.pause != nullptr && view.pause_state != nullptr) {
    ask.pause = ro::ui::DrawSurvivorsPauseWindow(*view.pause_state, *view.pause, skin_);
  }
  // LES OPTIONS DU JEU APRÈS LA PAUSE, donc PAR-DESSUS : c'est d'elle qu'on les ouvre.
  if (view.options_state != nullptr) {
    ask.options = ro::ui::DrawOptionsWindowAside(*view.options_state, view.options, skin_);
  }
  if (view.summary != nullptr) {
    ask.summary = ro::ui::DrawSurvivorsSummaryWindow(*view.summary, skin_);
  }
  // L'ÉGLISE PAR-DESSUS L'ÉCRAN DE FIN : Échap passe par la partie, qui la ferme.
  if (view.church != nullptr && view.church_state != nullptr) {
    ask.church = ro::ui::DrawSurvivorsChurchWindow(*view.church_state, *view.church, skin_);
  }
  // LE TABLEAU MONDIAL PAR-DESSUS L'ÉCRAN DE FIN, au-dessus de l'Église.
  if (view.ranking != nullptr && view.ranking_state != nullptr) {
    ask.ranking = ro::ui::DrawSurvivorsRankingWindow(*view.ranking_state, *view.ranking, skin_);
  }
  ui_->EndFrame();
  backend_.Draw();
  return ask;
}

ro::ui::PatchScreenAnswers UiStage::DrawPatchOnly(const ro::ui::PatchScreenView& view) {
  backend_.BeginFrame(*ui_);
  const ro::ui::PatchScreenAnswers answers = ro::ui::DrawPatchScreen(view);
  ui_->EndFrame();
  backend_.Draw();
  return answers;
}

}  // namespace moonclient
