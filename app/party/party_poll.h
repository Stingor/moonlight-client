#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

#include "ro/sim/member_statuses.h"
#include "ro/sim/party_roster.h"

namespace moonclient {

// ── LE PS DES MEMBRES, DEMANDÉ AU SERVEUR ───────────────────────────────────
//
// Aucun paquet ne pousse le PS d'un autre joueur : on le demande, membre par
// membre. UNE demande à la fois, au plus toutes les 250 ms, en rotation sur la
// liste — un groupe de vingt-quatre interrogé à chaque image ferait des
// centaines de paquets par seconde pour une valeur qui bouge lentement.
//
// Un tour cherche le prochain membre INTERROGEABLE : ni moi — mon PS est déjà
// connu —, ni un membre hors ligne, que le serveur ne décrirait pas.
inline constexpr std::uint32_t kPartyPollIntervalMs = 250;

class PartyVitalsPoll {
 public:
  // Le compte à interroger à cet instant, ou zéro. `wanted` : une surface
  // montre le PS cette image-ci ; la demande se redit à chaque image.
  //
  // ⚠ RIEN QUAND LA LISTE COMPTE AU PLUS UN MEMBRE : il n'y a que moi.
  [[nodiscard]] std::uint32_t Next(std::span<const ro::sim::PartyMember> members,
                                   std::uint32_t own_account_id, bool wanted, std::uint32_t now_ms);

 private:
  std::size_t cursor_ = 0;
  std::optional<std::uint32_t> last_poll_ms_;
};

// ── LES ÉTATS DES MEMBRES, DEMANDÉS AU SERVEUR ──────────────────────────────
//
// Même patron que le PS : une demande à la fois, au plus toutes les 300 ms, en
// rotation. Un tour saute moi — mes états ont leur propre liste —, un membre
// hors ligne, un membre que le serveur vient de refuser, et un membre qu'aucun
// acteur ne représente : ses états ne s'afficheraient pas.
inline constexpr std::uint32_t kMemberStatusPollIntervalMs = 300;

class MemberStatusPoll {
 public:
  // `present` : un acteur représente ce compte dans la scène.
  [[nodiscard]] std::uint32_t Next(std::span<const ro::sim::PartyMember> members,
                                   std::uint32_t own_account_id, bool wanted,
                                   const ro::sim::MemberStatuses& statuses,
                                   const std::function<bool(std::uint32_t)>& present,
                                   std::uint32_t now_ms);

  // Le monde repart : la rotation aussi.
  void Reset();

 private:
  std::size_t cursor_ = 0;
  std::optional<std::uint32_t> last_poll_ms_;
};

}  // namespace moonclient
