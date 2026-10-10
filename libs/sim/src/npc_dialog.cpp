#include "ro/sim/npc_dialog.h"

#include <utility>

namespace ro::sim {

void NpcDialog::PushLine(std::string_view line) {
  // Un `mes` peut porter plusieurs lignes d'un coup : le serveur y laisse les
  // sauts que le script a écrits, et le client officiel les découpe de même.
  // ⚠ LE RETOUR CHARIOT SE JETTE : gardé, il s'afficherait en carré.
  std::string current;
  for (const char letter : line) {
    if (letter == '\r') {
      continue;
    }
    if (letter == '\n') {
      pending_lines.push_back(std::move(current));
      current.clear();
      continue;
    }
    current.push_back(letter);
  }
  pending_lines.push_back(std::move(current));
}

bool NpcDialog::CommitPage() {
  // La page tenue affichée pendant l'aller-retour meurt ICI, et pas au premier
  // paquet de la réponse : l'effacer plus tôt escamote le menu et laisse le
  // texte se ré-étaler sur toute la fenêtre le temps que la suite arrive — un
  // aller-retour de réarrangement pour rien.
  bool changed = false;
  if (awaiting_reply) {
    awaiting_reply = false;
    // Ce que la page répondue montrait encore s'en va MAINTENANT : la liste de
    // choix et le champ de saisie tenaient l'écran le temps de l'aller-retour, et
    // le paquet qui publie la suite est celui qui les remplace.
    options.clear();
    menu_answered = false;
    if (prompt == NpcPrompt::kMenu || prompt == NpcPrompt::kNumber || prompt == NpcPrompt::kText) {
      prompt = NpcPrompt::kNone;
    }
    changed = true;
  }
  if (pending_lines.empty() && !pending_replaces) {
    return changed;
  }
  if (pending_replaces) {
    lines = std::move(pending_lines);
  } else {
    lines.insert(lines.end(), std::make_move_iterator(pending_lines.begin()),
                 std::make_move_iterator(pending_lines.end()));
  }
  pending_lines.clear();
  pending_replaces = false;
  if (lines.size() > kNpcDialogMaxLines) {
    lines.erase(lines.begin(),
                lines.begin() + static_cast<std::ptrdiff_t>(lines.size() - kNpcDialogMaxLines));
  }
  ++page;
  return true;
}

std::uint8_t NpcDialog::RankAt(std::size_t position) const {
  return position < options.size() ? options[position].rank : 0;
}

std::vector<NpcMenuOption> SplitNpcMenu(std::string_view menu) {
  std::vector<NpcMenuOption> options;
  if (menu.empty()) {
    return options;
  }
  // Le rang suit les options RETENUES, une à une : `menu_countoptions` ne compte
  // que les entrées non vides, et c'est ce compte que le serveur oppose au choix
  // reçu.
  std::size_t retained = 0;
  std::size_t start = 0;
  while (true) {
    const std::size_t separator = menu.find(':', start);
    const std::string_view piece = separator == std::string_view::npos
                                       ? menu.substr(start)
                                       : menu.substr(start, separator - start);
    if (!piece.empty()) {
      ++retained;
      // ⚠ AU-DELÀ DU DERNIER RANG, L'OPTION S'AFFICHE MAIS NE SE CHOISIT PAS.
      // Un octet de choix ne porte pas plus loin, et le serveur le dit
      // lui-même : les entrées en excès « are displayed but cause uint8
      // overflow ». Un rang nul est ce qui empêche l'envoi ; la retirer de la
      // liste changerait ce que le joueur LIT, ce qui est pire.
      const std::uint8_t rank =
          retained <= kNpcMenuLastChoice ? static_cast<std::uint8_t>(retained) : 0;
      options.push_back(NpcMenuOption{.text = std::string(piece), .rank = rank});
    }
    if (separator == std::string_view::npos) {
      return options;
    }
    start = separator + 1;
  }
}

}  // namespace ro::sim
