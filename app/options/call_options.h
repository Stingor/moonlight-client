#pragma once

#include <optional>
#include <vector>

#include "ro/ui/options_window.h"

namespace moonclient {

// ── L'APPEL D'URGENCE, DANS « GAMEPLAY » ────────────────────────────────────
//
// Une case : le chef de guilde peut-il nous appeler auprès de lui ? C'est
// l'option « /call » des réglages de jeu du client d'origine, cochée par
// défaut.
//
// 🔴 LE SERVEUR TIENT CE RÉGLAGE, PAS LE FICHIER. La case montre ce qu'il a dit
// à la connexion, et un clic ne change rien sur place : il DEMANDE, et la case
// suit l'écho — c'est l'écho qui écrit aussi la ligne dans la conversation.
// L'écrire aussi dans le fichier ferait deux sources, dont l'une mentirait.
//
// ⚠ GRISÉE TANT QUE LE SERVEUR N'A RIEN DIT : un clic écraserait alors le vrai
// réglage par un défaut.
inline constexpr int kCallRowBase = 7500;

enum class CallRow : int {
  kCallable = 0,
  kCount,
};

class CallOptions {
 public:
  // Ce que le serveur a dit, relu à chaque image : il le dit à la connexion, et
  // le redit à chaque écho.
  void NoteCallable(const std::optional<bool>& callable) noexcept { callable_ = callable; }

  // Ajoute sa ligne à celles d'une section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const;

  // Rend vrai quand la ligne était la sienne. ⚠ RIEN À ÉCRIRE DANS LE FICHIER :
  // le geste devient une demande, que la partie envoie.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // `/call` : demande l'inverse de ce que le serveur a dit. Rend faux, sans
  // rien demander, tant qu'il n'a rien dit — basculer un réglage inconnu
  // reviendrait à en choisir un au hasard.
  [[nodiscard]] bool RequestToggle() noexcept;

  // La demande en attente, rendue une seule fois.
  [[nodiscard]] std::optional<bool> TakeRequest() noexcept;

 private:
  std::optional<bool> callable_;
  std::optional<bool> requested_;
};

}  // namespace moonclient
