#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include "ro/net/session.h"
#include "ro/proto/manual_packets.h"

namespace ro::test_support {

// LE BANC DE LA SESSION DE PERSONNAGES : ce qu'il faut pour amener une
// `CharacterPhase` jusqu'au choix, sur un socket de bouclage.
//
// Il vit ICI parce que DEUX fichiers d'essais en ont besoin — la session
// elle-même et la création —, et qu'ils seront trois avec la suppression. Deux
// copies d'un même bâtisseur finiraient par se séparer, et l'essai qui tombe
// accuserait alors le code.

// 127.0.0.1 sur le fil. Le serveur écrit l'adresse en ordre RÉSEAU, donc le
// décodage petit-boutiste pointe les octets du plus faible au plus fort.
inline constexpr std::uint32_t kLoopbackWireIp = 0x0100007F;

// La demande d'entrée sur le serveur de personnages : opcode, compte, deux
// jetons, un remplissage, le sexe.
inline constexpr std::size_t kEnterRequestSize = 17;

// Assez de tours pour que les octets traversent la pile du système, pas assez
// pour qu'un essai bloqué suspende la suite.
inline constexpr int kMaxAttempts = 400;

// Combien de tours laisser tourner APRÈS une trame, sans s'arrêter au premier
// verdict d'étape. Sur la boucle locale les octets traversent en quelques
// millisecondes ; ce qui compte est de ne pas rendre la main avant la trame
// suivante.
inline constexpr int kRoundsPastTheList = 100;

// Combien de temps attendre pour prouver que RIEN ne part. Un délai court : on
// n'attend pas un octet, on attend d'être sûr qu'il n'y en a pas.
inline constexpr int kBriefWaitMs = 50;

// Interroge une phase jusqu'à ce qu'elle se prononce.
template <typename Phase>
[[nodiscard]] net::PhaseState Settle(Phase& phase) {
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const net::PhaseState state = phase.Advance();
    if (state != net::PhaseState::kWaiting) {
      return state;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return net::PhaseState::kWaiting;
}

// Interroge jusqu'à ce que la phase de personnage atteigne l'étape voulue.
[[nodiscard]] inline net::PhaseState SettleUntil(net::CharacterPhase& phase,
                                                 net::CharacterPhase::Step step) {
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const net::PhaseState state = phase.Advance();
    if (state != net::PhaseState::kWaiting || phase.step() == step) {
      return state;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return net::PhaseState::kWaiting;
}

// Fait tourner la phase un moment, SANS s'arrêter à une étape.
//
// ⚠ `SettleUntil` ne convient pas partout : il rend la main dès l'étape
// atteinte, et un défaut qui se produit sur la trame SUIVANTE passerait alors
// inaperçu — l'essai réussirait sans avoir rien lu.
inline void Churn(net::CharacterPhase& phase, int rounds) {
  for (int round = 0; round < rounds; ++round) {
    static_cast<void>(phase.Advance());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

// L'identifiant de compte NU que le serveur de personnages envoie en premier :
// quatre octets sans opcode ni longueur.
[[nodiscard]] inline std::vector<std::uint8_t> BareAccountId(std::uint32_t account_id) {
  return {static_cast<std::uint8_t>(account_id & 0xFF),
          static_cast<std::uint8_t>((account_id >> 8) & 0xFF),
          static_cast<std::uint8_t>((account_id >> 16) & 0xFF),
          static_cast<std::uint8_t>((account_id >> 24) & 0xFF)};
}

// Deux personnages aux emplacements 0 et 1 : le compte ordinaire, sans trou.
[[nodiscard]] inline std::vector<std::uint8_t> CharacterList() {
  proto::HcAcceptEnter list;
  list.total = 2;
  proto::CharacterInfo first;
  first.GID = 150'000;
  first.name = "Alice";
  first.CharNum = 0;
  proto::CharacterInfo second;
  second.GID = 150'001;
  second.name = "Bob";
  second.CharNum = 1;
  list.characters = {first, second};
  return Encode(list);
}

}  // namespace ro::test_support
