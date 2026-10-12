#include "ro/sim/world_npc_dialog.h"

#include <string>
#include <utility>
#include <vector>

// CE QUE LE MONDE FAIT D'UN DIALOGUE DE PNJ : voir `WorldNpcDialog`. Ces
// méthodes ne touchent qu'à `npc_dialog_`.

namespace ro::sim {

void WorldNpcDialog::SayNpcDialog(std::uint32_t npc_id, std::string line, std::uint32_t now_ms) {
  if (!npc_dialog_.has_value() || npc_dialog_->npc_id != npc_id) {
    npc_dialog_ = NpcDialog{};
    npc_dialog_->npc_id = npc_id;
  }
  NpcDialog& dialog = *npc_dialog_;
  dialog.last_packet_ms = now_ms;
  dialog.holds_world = true;
  // SAUT DE PAGE : la page en réception REMPLACERA l'affichée à sa publication,
  // au lieu de s'y ajouter. L'effacement lui-même n'a pas lieu ici — la page
  // courante reste lisible tant que la suivante n'est pas complète.
  if (dialog.clear_on_next_line) {
    dialog.pending_lines.clear();
    dialog.pending_replaces = true;
    dialog.clear_on_next_line = false;
  }
  dialog.PushLine(line);
  // Une ligne qui arrive retire le BOUTON de la page précédente : le serveur en
  // redira un quand il aura fini de parler. La liste de choix et le champ de
  // saisie, eux, restent à l'écran jusqu'à la publication — c'est tout l'objet
  // de la mise en attente.
  if (dialog.prompt == NpcPrompt::kNext || dialog.prompt == NpcPrompt::kClose) {
    dialog.prompt = NpcPrompt::kNone;
  }
}

bool WorldNpcDialog::PromptNpcDialog(std::uint32_t npc_id, NpcPrompt prompt, std::uint32_t now_ms) {
  if (!npc_dialog_.has_value() || npc_dialog_->npc_id != npc_id) {
    return false;
  }
  NpcDialog& dialog = *npc_dialog_;
  dialog.last_packet_ms = now_ms;
  dialog.CommitPage();
  dialog.prompt = prompt;
  // Un « fermer » termine la conversation : ce que le PNJ dira ensuite ouvre une
  // page neuve, et le monde se clique de nouveau.
  if (prompt == NpcPrompt::kClose) {
    dialog.clear_on_next_line = true;
    dialog.holds_world = false;
  } else if (prompt == NpcPrompt::kNext) {
    dialog.holds_world = true;
  }
  return true;
}

void WorldNpcDialog::OfferNpcMenu(std::uint32_t npc_id, std::vector<NpcMenuOption> options,
                                  std::uint32_t now_ms) {
  if (!npc_dialog_.has_value() || npc_dialog_->npc_id != npc_id) {
    npc_dialog_ = NpcDialog{};
    npc_dialog_->npc_id = npc_id;
  }
  NpcDialog& dialog = *npc_dialog_;
  dialog.last_packet_ms = now_ms;
  dialog.CommitPage();
  dialog.options = std::move(options);
  dialog.prompt = NpcPrompt::kMenu;
  dialog.holds_world = true;
  // Une liste neuve rouvre le droit de répondre : voir `NpcDialog::menu`.
  ++dialog.menu;
  dialog.menu_answered = false;
}

bool WorldNpcDialog::ClearNpcDialogText(std::uint32_t npc_id, std::uint32_t now_ms) {
  if (!npc_dialog_.has_value() || npc_dialog_->npc_id != npc_id) {
    return false;
  }
  npc_dialog_->last_packet_ms = now_ms;
  npc_dialog_->clear_on_next_line = true;
  return true;
}

bool WorldNpcDialog::CommitNpcPageIfIdle(std::uint32_t now_ms) {
  if (!npc_dialog_.has_value() || npc_dialog_->pending_lines.empty()) {
    return false;
  }
  if (now_ms - npc_dialog_->last_packet_ms < kNpcPageIdleMs) {
    return false;
  }
  return npc_dialog_->CommitPage();
}

void WorldNpcDialog::AdvanceNpcDialog() {
  if (!npc_dialog_.has_value()) {
    return;
  }
  npc_dialog_->clear_on_next_line = true;
  npc_dialog_->awaiting_reply = true;
}

void WorldNpcDialog::AnswerNpcMenu() {
  if (!npc_dialog_.has_value()) {
    return;
  }
  npc_dialog_->menu_answered = true;
  npc_dialog_->awaiting_reply = true;
  // Un choix est un saut de PAGE, comme le « suivant » : ce que le script écrit
  // ensuite remplace la question.
  npc_dialog_->clear_on_next_line = true;
}

void WorldNpcDialog::SubmitNpcInput() {
  if (!npc_dialog_.has_value()) {
    return;
  }
  npc_dialog_->awaiting_reply = true;
}

void WorldNpcDialog::CloseNpcDialog() { npc_dialog_.reset(); }

}  // namespace ro::sim
