#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/survivors_presence.h"
#include "ro/net/world_update.h"

#include "game/survivors_coop_mode.h"
#include "game/survivors_presence_labels.h"
#include "game/survivors_ranking_mode.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// ── LA PRÉSENCE EN ARÈNE ────────────────────────────────────────────────────
//
// Ce que le joueur fait de survivors, dit au serveur, et ce que le serveur dit
// des autres. Voir `ro/net/survivors_presence.h` et le cahier
// `docs/survivors/lot-13-presence.md`.
//
// 🔴 MUETTE TANT QUE LE SERVEUR N'A RIEN ANNONCÉ : le bit n'entre au masque que
// si la table des longueurs connaît `ZC 0x0F57`, et rien ne part avant qu'un
// `0x0F57` à `account_id` nul ait annoncé le service. Contre un serveur qui ne
// la connaît pas, elle ne change pas un octet.

// Au plus un `0x0F56` par cette durée : une pause qu'on prend et qu'on lâche
// aussitôt ne fait qu'un envoi, celui de l'état qui tient à la fin.
inline constexpr std::int64_t kSurvivorsPresenceSendEveryMs = 1000;

// CE QUE LE JOUEUR FAIT DE SURVIVORS à cette image, d'après l'étage : en
// partie de l'arrivée au coffre, en pause dans la pause, dehors autrement —
// l'accueil, le chargement, la fin de partie et le bilan compris.
[[nodiscard]] ro::net::SurvivorsPresenceAsk PresenceOfStage(const SurvivorsStage& stage);

// Ce que la partie sait au moment de l'envoi.
struct SurvivorsPresenceFacts {
  bool link_open = false;
  bool verified = false;  // la poignée de main moonclient
  // Le réglage « Montrer ma partie aux autres » : éteint, le joueur est
  // dehors pour les autres, quoi qu'il fasse.
  bool shown = true;
  ro::net::SurvivorsPresenceAsk own;
  std::int64_t now_ms = 0;  // horloge monotone
};

class SurvivorsPresenceMode {
 public:
  // `length_of` vide : la table des longueurs du client.
  explicit SurvivorsPresenceMode(const PacketLengthOf& length_of = {});

  // Le bit de la présence à ajouter au masque de `0x0F24`, ou 0.
  [[nodiscard]] std::uint32_t caps() const noexcept { return caps_; }

  // UNE TRAME REÇUE à l'instant monotone `received_at_ms` : `ZC 0x0F57` se lit
  // ici, ou se dit indéchiffrable ; toute autre trame reste non traitée.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame,
                                                 std::int64_t received_at_ms);

  // LES OCTETS À ENVOYER À CETTE IMAGE, ou rien : service annoncé, état changé
  // depuis le dernier envoi, et cadence passée. Liaison fermée ou poignée de
  // main défaite, tout s'oublie — l'annonce, l'envoi, les autres — : la
  // prochaine liaison les redira.
  [[nodiscard]] std::optional<std::vector<std::uint8_t>> Due(const SurvivorsPresenceFacts& facts);
  // `Due`, et l'envoi.
  void Pump(ro::net::Connection& link, const SurvivorsPresenceFacts& facts);

  // Le serveur a annoncé la présence sur cette liaison.
  [[nodiscard]] bool announced() const noexcept { return announced_; }
  // Ce qu'il sait d'un joueur, par `account_id` ; rien s'il est dehors.
  [[nodiscard]] const SurvivorsPresenceSeen* Of(std::uint32_t account_id) const noexcept;
  // Tous les joueurs en arène, par `account_id` croissant.
  [[nodiscard]] std::span<const SurvivorsPresenceSeen> seen() const noexcept { return seen_; }

 private:
  void Forget();

  std::uint32_t caps_ = 0;
  bool announced_ = false;
  // Ce que le serveur sait de nous : dehors à l'annonce.
  ro::net::SurvivorsPresenceAsk sent_;
  std::optional<std::int64_t> sent_at_ms_;
  std::vector<SurvivorsPresenceSeen> seen_;
};

// CE QUE `SurvivorsPresenceSlot` TIENT : le fil de la présence, et ce que
// l'interface écrit de ceux qui sont en arène, refait à chaque image.
struct SurvivorsPresence {
  SurvivorsPresenceMode mode;
  SurvivorsPresenceLabels labels;
  // La coop du mode : elle vit ici, à côté de la présence, sur le même fil.
  SurvivorsCoopMode coop;
};

}  // namespace moonclient
