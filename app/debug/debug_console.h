#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ro/net/socket.h"

#include "debug/debug_queries.h"

namespace moonclient {

// ── LA CONSOLE DE MISE AU POINT ─────────────────────────────────────────────
//
// Une écoute sur 127.0.0.1, servie À CHAQUE IMAGE par la boucle de jeu : un
// outil de la même machine — le pont MCP de `tools/moonclient_mcp` — y pose des
// questions sur l'état du client et lit les réponses. Voir `debug_queries.h`
// pour ce qu'elle sait dire, et l'ADR 0095 pour le pourquoi.
//
// 🔴 ÉTEINTE PAR DÉFAUT, ET À CHAQUE LANCEMENT : c'est une case des outils du
// staff qui l'allume, et elle n'est pas retenue. Un joueur ne l'a jamais
// ouverte sans le vouloir.
//
// ⚠ UN SEUL INTERLOCUTEUR À LA FOIS : un second chasse le premier. Le pont est
// seul à parler, et deux voix entremêlées rendraient les réponses illisibles.
//
// ⚠ LA BOUCLE NE L'ATTEND JAMAIS : tout est non bloquant, et une image ne sert
// qu'un nombre borné de requêtes.

// Le port d'usine de la console.
inline constexpr std::uint16_t kDebugConsolePort = 47611;

class DebugConsole {
 public:
  // Une image : ouvre ou ferme l'écoute selon `enabled`, accueille, lit, répond.
  void Serve(bool enabled, const DebugSources& sources);

  // Le port écouté, ou rien quand la console est éteinte.
  [[nodiscard]] std::optional<std::uint16_t> port() const noexcept;
  // Un interlocuteur est-il branché ?
  [[nodiscard]] bool connected() const noexcept { return client_.is_open(); }
  // La dernière panne de l'écoute, vide sans panne : la case la montre.
  [[nodiscard]] const std::string& failure() const noexcept { return failure_; }

 private:
  void Open();
  void Shut();
  void Read();
  void Answer(const DebugSources& sources);
  void Flush();

  ro::net::TcpListener listener_;
  ro::net::TcpSocket client_;
  std::string received_;
  std::string pending_;
  std::string failure_;
  // L'écoute a échoué : on ne la retente qu'après avoir été éteint puis rallumé,
  // sans quoi chaque image redemanderait un port pris.
  bool gave_up_ = false;
};

}  // namespace moonclient
