#pragma once

#include <string>
#include <string_view>

namespace moonclient {

// ── `/ex`, `/in`, `/exall`, `/inall` : BLOQUER LES CHUCHOTEMENTS ────────────
//
//     /ex <nom>     bloque les chuchotements de ce personnage
//     /ex           redemande la liste des noms bloqués, et l'écrit
//     /in <nom>     les autorise de nouveau
//     /exall        bloque ceux de tout le monde
//     /inall        les autorise tous de nouveau
//
// Elles mettent à jour la même liste que le menu d'entité : c'est la réponse
// du serveur qui la change, d'où que vienne la demande.

enum class WhisperBlockCommandKind {
  kNone,
  kBlock,
  kAllow,
  kList,
  kBlockEveryone,
  kAllowEveryone,
  // `/in` sans nom : il n'y a rien à autoriser, la ligne d'aide se dit.
  kAllowWithoutName,
};

struct WhisperBlockCommand {
  WhisperBlockCommandKind kind = WhisperBlockCommandKind::kNone;
  // Le nom visé, sans les espaces qui l'entourent : un nom de personnage n'en
  // porte ni en tête ni en queue. Vide hors de `/ex <nom>` et `/in <nom>`.
  std::string name;
};

// Le mot de commande se compare sans tenir compte de la casse, comme le client.
// `/exit` n'est pas `/ex` : la commande s'arrête à l'espace.
[[nodiscard]] WhisperBlockCommand ReadWhisperBlockCommand(std::string_view line);

}  // namespace moonclient
