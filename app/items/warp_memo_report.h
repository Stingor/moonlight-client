#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/vfs/warp_memo_words.h"

namespace moonclient {

// CE QUE LE CLIENT ÉCRIT DANS LA CONVERSATION QUAND LE SERVEUR RÉPOND à la
// mémorisation d'un point de Warp, ou refuse pour une raison de carte.
//
// 🔴 C'EST ICI QUE VIT LA CORRESPONDANCE code -> phrase, et nulle part ailleurs :
// `net` rend le code du fil, `vfs` porte les textes de l'installation, et aucun
// des deux ne connaît l'autre. Un code rangé sous la mauvaise phrase en dit une
// plausible qui contredit ce qui s'est passé — « point enregistré » sur un
// refus —, et rien à l'écran ne le trahit.
//
// ⚠ TOUT CODE INCONNU EST SILENCIEUX, comme chez le client : il n'écrit rien
// plutôt que d'inventer une raison. Rendre « rien » est donc un choix.

// Les codes de la réponse à la demande, tels que `e_ack_remember_warppoint_result`
// (moonlight, src/map/clif.hpp) les numérote. Le quatrième, le client le
// connaît et ce serveur ne l'émet pas.
enum class WarpMemoAck : std::uint8_t {
  kSaved = 0,
  kSkillTooLow = 1,
  kSkillNotLearned = 2,
  kCannotSaveHere = 3,
};

// Les codes du refus tenant à la carte, tels que `e_notify_mapinfo_result` les
// numérote. Le premier et le dernier ne concernent pas la mémorisation, mais
// c'est le même paquet et le client les écrit au même endroit.
enum class MapInfoNotice : std::int16_t {
  kTeleportForbidden = 0,
  kMemoForbidden = 1,
  kSkillForbidden = 2,
  kItemForbidden = 3,
};

// Les deux couleurs du client. La réponse à la demande est cyan au succès et
// rouge à l'échec ; dans le paquet de refus de carte, seule la téléportation
// interdite est rouge et les trois autres sont cyan.
inline constexpr std::uint32_t kWarpMemoCyan = 0x00FFFF;
inline constexpr std::uint32_t kWarpMemoRed = 0xFF0000;

// Une ligne de conversation : le sens, et la couleur qui l'accompagne.
struct WarpMemoLine {
  ro::vfs::WarpMemoMessage message = ro::vfs::WarpMemoMessage::kSaved;
  std::uint32_t color = kWarpMemoCyan;
};

// LA COMMANDE QUI DEMANDE LA MÉMORISATION : `/memo`, et ses deux autres noms —
// `/rldjr`, le même mot de la Corée tapé sur un clavier latin, et `/기억`.
//
// Seul le premier mot compte, et la casse latine ne compte pas : ce qui suit est
// ignoré, comme chez le client.
[[nodiscard]] bool IsWarpMemoCommand(std::string_view line);

// La ligne qu'écrit un code de réponse à la demande, ou rien.
[[nodiscard]] std::optional<WarpMemoLine> WarpMemoLineOfAck(std::uint8_t code);

// La ligne qu'écrit un code de refus de carte, ou rien.
[[nodiscard]] std::optional<WarpMemoLine> WarpMemoLineOfMapInfo(std::int16_t code);

}  // namespace moonclient
