// LE SERTISSAGE DES CARTES, DANS LA BOUCLE DE LA PARTIE.
//
// ⚠ À PART DE LA BOUCLE : la fenêtre, le sous-menu du sac et la ligne de
// conversation forment un tout, et ce qui les relie — la session du monde — ne
// regarde qu'eux.

#include <cstdint>

#include "game/game.h"

namespace moonclient {

void Game::StageCardInsert() {
  card_insert_->Refresh(world_);
  // ⚠ CE N'EST PAS LE JOUEUR QUI L'OUVRE : la fenêtre recopie la session,
  // comme la banque.
  windows_.card_insert.open = world_.composition().open();
  windows_.card_insert_view = card_insert_->view();
  // L'APERÇU AU SURVOL SUIT LE RÉGLAGE DU SAC : les deux fenêtres montrent
  // les mêmes objets, et un seul réglage les gouverne.
  windows_.card_insert.hover_preview = windows_.inventory.hover_preview;

  // Le sous-menu rapide du sac : la réponse du serveur, et sur quelle pièce.
  const ro::sim::CompatibleCards& compatible = world_.composition().compatible();
  windows_.inventory_view.quick_insert_equip = compatible.equip_slot;
  windows_.inventory_view.quick_insert_answered = compatible.answered;
  windows_.inventory_view.quick_insert_cards = card_insert_->quick_cards();

  // LE VERDICT DU GLISSER, le même pour les deux cibles : il ne vaut que pour
  // la paire qu'il nomme.
  windows_.inventory_view.card_drop = card_insert_->card_drop_hint();
  windows_.character_view.card_drop = card_insert_->card_drop_hint();
}

void Game::ApplyCardDrop(const UiStage::Answers& answers, std::uint32_t now_ms) {
  const bool dealing = pieces_.trade.deal().deal().open();
  const CardDropStep step = card_insert_->StepCardDrop(
      answers.inventory.card_drop, answers.character.card_drop, world_, dealing, now_ms);
  CardInsertView::AskCompatible(step.ask_compatible, link_, world_);
  // 🔴 LE MÊME CHEMIN QUE LE SERTISSAGE RAPIDE DU SAC : même verbe, même
  // `Apply`, donc même demande, même file d'accusés, même ligne de conversation.
  if (step.order.aimed()) {
    bag_->Apply(CardDropAsQuickInsert(step.order), link_, world_);
  }
  CardInsertView::NoteCardDropRefusal(step.refusal, world_, now_ms);
}

void Game::ApplyCardInsert(const UiStage::Answers& answers, std::uint32_t now_ms) {
  CardInsertView::AskCompatible(answers.inventory.compatible_asked, link_, world_);
  card_insert_->Apply(answers.card_insert, windows_.card_insert, link_, world_);
  // ⚠ AVANT LA LIGNE DES ACCUSÉS : un dépôt accepté retient sa demande, et un
  // refus local s'écrit dans la même image que le geste.
  ApplyCardDrop(answers, now_ms);
  // ⚠ À CHAQUE IMAGE, fenêtre ouverte ou non : le sertissage rapide n'en ouvre
  // aucune, et son accusé doit s'écrire quand même.
  card_insert_->NoteVerdict(world_, now_ms);
  // LA PIÈCE SURVOLÉE, si aucune autre fenêtre ne l'a été. ⚠ ELLE EST DANS LE
  // SAC : c'est lui qui donne au nom ses cartes et son raffinement.
  if (hovered_item_ == 0 && windows_.inventory.hover_preview) {
    hovered_item_ = answers.card_insert.hovered_id;
    hovered_slot_ = answers.card_insert.hovered_slot;
    hovered_holder_ = ro::ui::ItemHolder::kInventory;
  }
}

}  // namespace moonclient
