#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace ro::proto {

// UNE PAROLE POSÉE AU-DESSUS D'UN ACTEUR PAR UN SCRIPT (`ZC_SHOWSCRIPT`,
// `showscript`), écrite à la main par le serveur (clif_showscript) :
//
//     08B3 <longueur>.W <acteur>.L <message>.?B
//
// Une bulle comme celle d'un message de chat, mais SANS ligne dans le fil, ni
// nom devant, ni masque. Le joueur peut en être le porteur.
struct ShowScriptNotice {
  static constexpr std::uint16_t kOpcode = 0x08B3;
  static constexpr std::size_t kHeaderSize = 2 + 2 + 4;

  std::uint32_t actor_id = 0;
  std::string message;
};

// Faux si la trame n'a ni le bon opcode ni la longueur qu'elle déclare.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ShowScriptNotice& packet);

}  // namespace ro::proto
