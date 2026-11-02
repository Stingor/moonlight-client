#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>

#include "ro/ui/block_list_window.h"

namespace moonclient {

// ── LA FENÊTRE DES JOUEURS BLOQUÉS : CE QU'ELLE MONTRE ──────────────────────
//
// Deux sources pour les chuchotements, une pour la voix, fondues en une ligne
// par nom. Voir `chat/block_list_route.h` pour ce qui les lit dans la partie.
//
// 🔴 LES CHUCHOTEMENTS SE LISENT DANS LE REFLET DU SERVEUR ET DANS LA LISTE
// RANGÉE. Le reflet se vide quand un autre serveur de zone prend la main, la
// liste rangée non ; elle se rejoue à l'entrée en jeu. Un nom de l'une ou de
// l'autre est bloqué, ou le sera : le débloquer retire des deux.
//
// ⚠ LES NOMS SE COMPARENT OCTET POUR OCTET, comme chez le serveur : « bob » et
// « Bob » font deux lignes.
struct BlockListFacts {
  std::span<const std::string> whispers;
  std::span<const std::string> kept_whispers;
  bool whispers_kept = false;
  bool whispers_from_everyone = false;
  // Ceux dont la voix est coupée, en ligne.
  std::span<const std::string> voice_blocked;
  // La voix entend-elle ce joueur, et peut-elle donc le couper ? Vide : non.
  std::function<bool(std::string_view)> voice_hears;
  bool connected = false;
  bool voice_online = false;
};

[[nodiscard]] ro::ui::BlockListView ComposeBlockList(const BlockListFacts& facts);

}  // namespace moonclient
