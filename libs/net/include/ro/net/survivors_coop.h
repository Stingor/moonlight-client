#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ro::net {

// ── LA COOP DU MODE SURVIVORS SUR LE FIL ───────────────────────────────────
//
// Le groupe tient un salon sur le serveur de carte ; chaque membre y dépose sa
// part, opaque pour le serveur. Quand tous sont prêts, le serveur tire la graine
// et redonne les parts dans l'ordre des places. En partie, il relaie les trames
// d'entrées. Voir le cahier `docs/survivors/lot-14-coop.md`.
//
//   CZ 0x0F58 LOBBY [op:2][len:2][action:1][part...]
//   ZC 0x0F59 LOBBY [op:2][len:2][state:1][refusals:4][count:1]
//                   puis count × [char_id:4][ready:1][name:24]
//   ZC 0x0F5A START [op:2][len:2][seed:8][input_delay_ticks:1][count:1]
//                   puis count × [char_id:4][part_len:2][part]
//   CZ 0x0F5B FRAME [op:2][len:2][payload:4..256]
//   ZC 0x0F5C FRAME [op:2][len:2][char_id:4][payload]
//   ZC 0x0F5D LEFT  [op:2][len:2][char_id:4][through_tick:4]                  12
//
// Tout est petit-boutiste ; `len` compte la trame entière, en-tête compris.
//
// 🔴 MUET TANT QUE LE SERVEUR N'A RIEN ANNONCÉ. Le client n'annonce
// `kUiCapSurvivorsCoop` que si la table des longueurs connaît les quatre ZC, et
// n'émet qu'après un `0x0F59` : un serveur sans la coop ne reçoit jamais un octet.
// AUCUNE fonction de ce fichier n'émet : elles rendent des octets.

inline constexpr std::uint16_t kSurvivorsCoopLobbyAskOpcode = 0x0F58;
inline constexpr std::uint16_t kSurvivorsCoopLobbyOpcode = 0x0F59;
inline constexpr std::uint16_t kSurvivorsCoopStartOpcode = 0x0F5A;
inline constexpr std::uint16_t kSurvivorsCoopFrameAskOpcode = 0x0F5B;
inline constexpr std::uint16_t kSurvivorsCoopFrameOpcode = 0x0F5C;
inline constexpr std::uint16_t kSurvivorsCoopLeftOpcode = 0x0F5D;
inline constexpr std::size_t kSurvivorsCoopLeftSize = 12;
inline constexpr std::size_t kSurvivorsCoopMostPartBytes = 512;
inline constexpr std::size_t kSurvivorsCoopLeastPayloadBytes = 4;
inline constexpr std::size_t kSurvivorsCoopMostPayloadBytes = 256;
inline constexpr std::size_t kSurvivorsCoopNameBytes = 24;

enum class SurvivorsCoopAction : std::uint8_t { kEnter = 0, kReady = 1, kLeave = 2 };
enum class SurvivorsCoopState : std::uint8_t { kClosed = 0, kLobby = 1, kPlaying = 2 };

// Ce qui empêche de partir, un bit par raison.
inline constexpr std::uint32_t kSurvivorsCoopTooMany = 0x01;
inline constexpr std::uint32_t kSurvivorsCoopTooFew = 0x02;
inline constexpr std::uint32_t kSurvivorsCoopOffline = 0x04;
inline constexpr std::uint32_t kSurvivorsCoopOutOfTown = 0x08;
inline constexpr std::uint32_t kSurvivorsCoopNoClient = 0x10;
inline constexpr std::uint32_t kSurvivorsCoopNotInLobby = 0x20;

struct SurvivorsCoopMember {
  std::uint32_t char_id = 0;
  bool ready = false;
  std::string name;
  [[nodiscard]] bool operator==(const SurvivorsCoopMember&) const = default;
};
struct SurvivorsCoopLobby {
  SurvivorsCoopState state = SurvivorsCoopState::kClosed;
  std::uint32_t refusals = 0;
  std::vector<SurvivorsCoopMember> members;
  [[nodiscard]] bool operator==(const SurvivorsCoopLobby&) const = default;
};
struct SurvivorsCoopPlayer {
  std::uint32_t char_id = 0;
  std::vector<std::uint8_t> part;
  [[nodiscard]] bool operator==(const SurvivorsCoopPlayer&) const = default;
};
struct SurvivorsCoopStart {
  std::uint64_t seed = 0;
  std::uint8_t input_delay_ticks = 0;
  std::vector<SurvivorsCoopPlayer> players;  // dans l'ordre des places
  [[nodiscard]] bool operator==(const SurvivorsCoopStart&) const = default;
};
struct SurvivorsCoopFrame {
  std::uint32_t char_id = 0;
  std::vector<std::uint8_t> payload;
  [[nodiscard]] bool operator==(const SurvivorsCoopFrame&) const = default;
};
struct SurvivorsCoopLeft {
  std::uint32_t char_id = 0;
  std::uint32_t through_tick = 0;
  [[nodiscard]] bool operator==(const SurvivorsCoopLeft&) const = default;
};

// Une part de plus de kSurvivorsCoopMostPartBytes se refuse : le serveur la jetterait.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> EncodeSurvivorsCoopLobbyAsk(
    SurvivorsCoopAction action, std::span<const std::uint8_t> part = {});
// Un payload hors de ses bornes se refuse.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> EncodeSurvivorsCoopFrameAsk(
    std::span<const std::uint8_t> payload);

// Rien pour une trame mal formée : opcode, longueur, état ou nombre.
[[nodiscard]] std::optional<SurvivorsCoopLobby> DecodeSurvivorsCoopLobby(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::optional<SurvivorsCoopStart> DecodeSurvivorsCoopStart(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::optional<SurvivorsCoopFrame> DecodeSurvivorsCoopFrame(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::optional<SurvivorsCoopLeft> DecodeSurvivorsCoopLeft(
    std::span<const std::uint8_t> frame);

}  // namespace ro::net
