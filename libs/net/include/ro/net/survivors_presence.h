#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ro::net {

// ── LA PRÉSENCE EN ARÈNE SUR LE FIL ─────────────────────────────────────────
//
// Le client dit au serveur qu'il entre en partie de survivors, s'y met en
// pause ou en sort ; le serveur relaie l'état aux sessions qui voient le
// personnage, à ses amis et à sa guilde. Voir le cahier
// `docs/survivors/lot-13-presence.md`.
//
//   CZ 0x0F56 PRESENCE [op][len:2][state:1][arena:2][class:2][flags:1]        10
//   ZC 0x0F57 PRESENCE [op][len:2][account_id:4][char_id:4][state:1]
//                      [arena:2][class:2][elapsed_s:4]                        21
//
// Tout est petit-boutiste ; `len` compte la trame entière, en-tête compris.
// `elapsed_s` est la durée écoulée depuis l'entrée à l'heure de l'envoi : une
// durée et non une heure, les horloges n'ont pas à s'accorder.
//
// 🔴 MUET TANT QUE LE SERVEUR N'A RIEN ANNONCÉ. Le client n'annonce
// `kUiCapSurvivorsPresence` que si la table des longueurs connaît `0x0F57`, et
// n'émet `0x0F56` qu'après un `0x0F57` à `account_id` nul, par lequel le
// serveur annonce la présence. AUCUNE fonction de ce fichier n'émet : elles
// rendent des octets.

inline constexpr std::uint16_t kSurvivorsPresenceAskOpcode = 0x0F56;
inline constexpr std::uint16_t kSurvivorsPresenceOpcode = 0x0F57;
inline constexpr std::size_t kSurvivorsPresenceAskSize = 10;
inline constexpr std::size_t kSurvivorsPresenceSize = 21;

// Où en est un joueur. Un numéro inconnu se lit `kOut` : rien n'est promis.
enum class SurvivorsPresenceState : std::uint8_t {
  kOut = 0,
  kPlaying = 1,
  kPaused = 2,
};

// Le tableau de la partie, en drapeaux : ni l'un ni l'autre pour une partie
// libre.
inline constexpr std::uint8_t kSurvivorsPresenceRanked = 0x01;
inline constexpr std::uint8_t kSurvivorsPresenceChallenge = 0x02;
// Le serveur n'en dit rien à personne : le joueur ne se montre pas, ou lit son bilan.
// Le personnage reste protégé des demandes qu'il ne verrait pas.
inline constexpr std::uint8_t kSurvivorsPresenceHidden = 0x04;

// Ce que le client dit de lui. L'arène, la classe et les drapeaux sont nuls
// hors de l'arène.
struct SurvivorsPresenceAsk {
  SurvivorsPresenceState state = SurvivorsPresenceState::kOut;
  std::uint16_t arena_id = 0;
  // La classe du mode (`ClassRule::class_id`), pas le job d'apparence.
  std::uint16_t class_id = 0;
  std::uint8_t flags = 0;
  [[nodiscard]] bool operator==(const SurvivorsPresenceAsk&) const = default;
};

// Ce que le serveur dit d'un joueur. `account_id` nul : l'annonce du service.
struct SurvivorsPresence {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  SurvivorsPresenceState state = SurvivorsPresenceState::kOut;
  std::uint16_t arena_id = 0;
  std::uint16_t class_id = 0;
  std::uint32_t elapsed_s = 0;
  [[nodiscard]] bool operator==(const SurvivorsPresence&) const = default;
};

[[nodiscard]] std::vector<std::uint8_t> EncodeSurvivorsPresenceAsk(const SurvivorsPresenceAsk& ask);

// Le ZC depuis la trame entière : rien quand l'opcode n'est pas le sien, que
// `len` ne dit pas la taille de la trame, ou que la taille n'est pas la sienne.
[[nodiscard]] std::optional<SurvivorsPresence> DecodeSurvivorsPresence(
    std::span<const std::uint8_t> frame);

}  // namespace ro::net
