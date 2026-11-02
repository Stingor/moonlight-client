#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace ro::net {

// ── OÙ JOINDRE LE SERVEUR VOIX — `ZC_MOONLIGHT_VOICE_SERVICE 0x0F55` ─────────
//
// Le serveur de jeu le dit à l'apparition du bit `kUiCapVoice`, et à
// l'acceptation de la poignée de main si le bit l'a précédée. Il ne le dit pas
// quand la voix est fermée chez lui : son silence est la seule réponse.
//
// Disposition : [op:2][len:2][port:2][hôte:64, terminé par NUL]. Fixe 70.
//
// L'authentification auprès du serveur voix ne passe pas par ici : elle se
// fait par le compte, le personnage et `login_id1`, que le serveur de jeu
// garantit au serveur voix de son côté.
inline constexpr std::uint16_t kVoiceServiceOpcode = 0x0F55;
inline constexpr std::size_t kVoiceServiceSize = 70;

// L'EFFET DE CHAPEAU que le serveur pose sur qui parle, pour les clients qui ne
// savent pas qui parle. Ce client le sait par la voix et dessine son icône : il
// ignore cet effet, que sa table des effets de chapeau attribue à un costume.
inline constexpr std::uint16_t kVoiceSpeakingHatEffect = 267;

struct VoiceService {
  std::string host;
  std::uint16_t port = 0;
};

// Le service, ou rien si la trame n'est pas celle-là, n'a pas la bonne taille,
// ou ne nomme ni hôte ni port.
[[nodiscard]] std::optional<VoiceService> DecodeVoiceService(std::span<const std::uint8_t> frame);

}  // namespace ro::net
