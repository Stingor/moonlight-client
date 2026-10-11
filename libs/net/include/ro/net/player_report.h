#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// ── LE SIGNALEMENT D'UN JOUEUR : la fenêtre « Report a User » ───────────────
//
//   CZ 0x0BE2 [op][auteur:4][cible:4][nom de la cible:24][motif:2][texte:101]
//   ZC 0x0BE3 [op][cible:4][nom de la cible:24][verdict:4]
//
// Le paquet du client d'origine, tel quel : le serveur l'enregistre en base, où
// le staff le consulte. Il prend l'auteur de la session et relit la cible en
// ligne ; l'auteur et le nom écrits ici ne sont que ce que le client croit.
//
// 🔴 TOUT PART EN CP1252, le texte aussi : la base du serveur est latin1, comme
// pour le rapport de bug.

// Le motif, dans l'ordre des deux boutons de la fenêtre.
enum class PlayerReportReason : std::int16_t {
  kIllegalProgram = 0,
  kBugAbuse = 1,
};

// Le verdict, tel que le fil le porte. Le serveur n'en envoie que quatre — enregistré,
// déjà signalé il y a peu, cumul du jour atteint, cible introuvable —, plus
// l'échec d'écriture qu'il ajoute ; les autres sont ceux du client d'origine.
enum class PlayerReportResult : std::int32_t {
  kCountLimit = 0,
  kRecorded = 1,
  kCoolTime = 2,
  kTargetProtected = 3,
  kDailyMaxReached = 4,
  kReportedByOther = 5,
  kAreaForbidden = 6,
  kTargetNotFound = 7,
  kFailed = 8,
};

// Le texte libre, en octets CP1252 : cent caractères. Le champ du fil en a un
// de plus, pour le zéro final.
inline constexpr std::size_t kPlayerReportMessageMostBytes = 100;
// Le nom de la cible, zéro final réservé.
inline constexpr std::size_t kPlayerReportNameMostBytes = 23;

// L'opcode de l'accusé, pour qui doit reconnaître une trame qui ne se décode pas.
inline constexpr std::uint16_t kPlayerReportAckOpcode = 0x0BE3;

struct PlayerReport {
  // L'AID de l'auteur, tel que le client le connaît.
  std::uint32_t reporter_aid = 0;
  std::uint32_t target_aid = 0;
  // En UTF-8.
  std::string target_name;
  PlayerReportReason reason = PlayerReportReason::kIllegalProgram;
  // En UTF-8 ; coupé au-delà de ce que le champ porte.
  std::string message;
};

struct PlayerReportAck {
  std::uint32_t target_aid = 0;
  // En UTF-8 ; vide quand la cible n'a pas été trouvée.
  std::string target_name;
  // Le numéro brut : un verdict inconnu ne se montre pas.
  std::int32_t result = 0;
};

[[nodiscard]] core::Result<void> SendPlayerReport(Connection& link, const PlayerReport& report);

// L'accusé, ou rien quand la trame n'en est pas un ou ne se lit pas.
[[nodiscard]] std::optional<PlayerReportAck> DecodePlayerReportAck(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::optional<PlayerReportAck> ReadPlayerReportAck(const Frame& frame);

}  // namespace ro::net
