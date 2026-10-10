#pragma once

#include <cstdint>
#include <optional>

#include "ro/net/connection.h"
#include "ro/net/session.h"

namespace ro::net {

// CE QU'IL FAUT POUR ENTRER SUR UN SERVEUR DE ZONE : le compte, tel que la
// connexion l'a obtenu, et le personnage choisi.
//
// ⚠ IL SURVIT À L'ENTRÉE, et c'est sa raison d'être : un serveur de zone peut
// en désigner un AUTRE en cours de partie, et y entrer redemande les mêmes
// pièces que la première fois. Personne d'autre ne les garde.
struct ZoneTicket {
  LoginPhase::Account account;
  std::uint32_t character_id = 0;

  // Faux pour une partie dont l'entrée n'a pas été tracée.
  [[nodiscard]] bool usable() const noexcept {
    return account.account_id != 0 && character_id != 0;
  }
};

// LE SERVEUR DE ZONE EN DÉSIGNE UN AUTRE : la carte demandée vit dans un autre
// processus (`ZC_NPCACK_SERVERMOVE`, `clif_changemapserver`). Rend où aller,
// ou rien si la trame n'est pas celle-là ou ne se lit pas.
//
// 🔴 LA LIAISON COURANTE EST MORTE DÈS CETTE TRAME : le serveur quitté ne sert
// plus le personnage, et n'écoutera ni l'avis de carte chargée ni le battement
// d'horloge. Tout se refait sur la nouvelle, avec `MapPhase`.
[[nodiscard]] std::optional<CharacterPhase::Handoff> ServerMoveOf(const Frame& frame,
                                                                  std::uint32_t character_id);

}  // namespace ro::net
