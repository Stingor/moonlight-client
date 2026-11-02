#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "ro/net/connection.h"
#include "ro/net/voice_service.h"
#include "ro/net/world_update.h"
#include "ro/ui/voice_overlay.h"

#include "options/voice_options.h"

namespace ro::audio {
class VoiceDevices;
}
namespace ro::voice {
class VoiceEngine;
}

namespace moonclient {

// ── LA VOIX DANS LA PARTIE ──────────────────────────────────────────────────
//
// Elle lit où joindre le serveur voix (`ZC 0x0F55`), ouvre la liaison quand la
// poignée de main est reconnue et que le joueur veut la voix, la ferme sinon,
// et lui redit à chaque image ce que le joueur demande. Le reste vit sur le fil
// de `ro::voice::VoiceEngine`. Voir l'ADR 0135.
//
// 🔴 LE BIT `kUiCapVoice` N'ENTRE DANS `0x0F24` QUE SI LA TABLE DES LONGUEURS
// CONNAÎT `0x0F55`, vérifié une fois, à la construction : un ZC inconnu de la
// table arrête la liaison.

// La longueur qu'une table donne à un opcode, comme `ro::proto::PacketLength`.
using VoiceLengthOf = std::function<std::optional<std::int16_t>(std::uint16_t)>;

// CE QUE LA PARTIE DONNE À LA VOIX, à chaque image.
struct VoiceFacts {
  bool verified = false;  // la poignée de main d'intégrité est reconnue
  std::uint32_t account_id = 0;
  std::uint32_t character_id = 0;
  std::uint32_t login_id1 = 0;
  VoiceSettings settings;
  bool spatial = true;
  // LA TOUCHE « PARLER », ou 0 quand elle n'est liée à rien. ⚠ LUE TENUE sur
  // l'état du clavier, sans ses modificateurs : on parle en tenant Maj aussi.
  std::uint32_t talk_key = 0;
  std::uint32_t now_ms = 0;
};

class VoiceMode {
 public:
  // `length_of` vide : la table des longueurs du client.
  explicit VoiceMode(const VoiceLengthOf& length_of = {});
  ~VoiceMode();

  VoiceMode(const VoiceMode&) = delete;
  VoiceMode& operator=(const VoiceMode&) = delete;

  // Le bit de la voix à ajouter au masque de `0x0F24`, ou 0.
  [[nodiscard]] std::uint32_t caps() const noexcept { return caps_; }

  // `0x0F55` se lit ici ; toute autre trame reste non traitée.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame);

  void Pump(const VoiceFacts& facts);

  // Le témoin à dessiner, composé à la dernière pompe.
  [[nodiscard]] const ro::ui::VoiceOverlayState& overlay() const noexcept { return overlay_; }

 private:
  void Start(const VoiceFacts& facts);
  void Stop() noexcept;
  void Compose(const VoiceFacts& facts, bool talking);

  std::uint32_t caps_ = 0;
  std::optional<ro::net::VoiceService> service_;
  // ⚠ LES APPAREILS AVANT LE MOTEUR, qui les emprunte depuis son fil : il se
  // détruit le premier, et son fil s'arrête avant qu'eux ne se ferment.
  std::unique_ptr<ro::audio::VoiceDevices> devices_;
  std::unique_ptr<ro::voice::VoiceEngine> engine_;
  // Les appareils n'ont pas pu s'ouvrir : on ne réessaie qu'au prochain
  // changement de réglage, pour ne pas écrire l'échec à chaque image.
  bool devices_failed_ = false;
  // LES APPAREILS DEMANDÉS à la dernière pompe : un autre choix rouvre la
  // liaison sur les nouveaux.
  std::string microphone_;
  std::string speakers_;
  std::uint32_t online_since_ms_ = 0;
  bool was_online_ = false;
  ro::ui::VoiceOverlayState overlay_;
};

}  // namespace moonclient
