#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/voice_service.h"
#include "ro/net/world_update.h"
#include "ro/ui/voice_overlay.h"
#include "ro/voice/voice_messages.h"

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
  // LES TOUCHES DE LA VOIX, au rang de `VoiceTalkKey`, ou 0 pour une touche
  // liée à rien. ⚠ LUES TENUES sur l'état du clavier, sans leurs
  // modificateurs : on parle en tenant Maj aussi.
  std::array<std::uint32_t, kVoiceTalkKeyCount> talk_keys{};
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
  // Le niveau du micro à la dernière pompe, en pour cent de
  // `kVoiceLevelAtFullScale` ; vide quand la liaison est fermée.
  [[nodiscard]] std::optional<float> input_percent() const noexcept { return input_percent_; }

  // CE QUE LA VOIX SAIT D'UN JOUEUR, par son nom : le nom est le seul lien
  // entre le personnage à l'écran et celui du serveur voix.
  enum class Peer : std::uint8_t {
    kUnknown,  // hors de portée de voix, ou sans voix
    kHeard,    // à portée, ou entendu à l'instant
    kBlocked,  // ce compte ne l'entend plus
  };
  [[nodiscard]] Peer PeerOf(std::string_view name) const;
  // Les joueurs que ce compte n'entend plus et qui sont en ligne : le serveur
  // voix ne nomme pas les autres. Vide quand la liaison est fermée.
  [[nodiscard]] std::vector<std::string> BlockedNames() const;
  // La liaison était ouverte à la dernière pompe.
  [[nodiscard]] bool online() const noexcept { return was_online_; }
  // LA COOP SURVIVORS par l'UDP de la voix : sans liaison, rien ne part et rien
  // n'arrive, et le serveur de carte porte tout.
  void PostCoop(std::vector<std::uint8_t> payload);
  [[nodiscard]] std::vector<ro::voice::CoopRelayed> TakeCoop();
  // Ne plus entendre ce joueur, ou l'entendre de nouveau. Sans effet sur un
  // joueur que la voix ne connaît pas.
  void Block(std::string_view name, bool blocked);

 private:
  // QUI LA LIAISON A PRÉSENTÉ au serveur voix.
  struct VoiceIdentity {
    std::uint32_t account_id = 0;
    std::uint32_t character_id = 0;
    std::uint32_t login_id1 = 0;
    bool operator==(const VoiceIdentity&) const = default;
  };

  void Start(const VoiceFacts& facts);
  void Stop() noexcept;
  // Ferme une liaison ouverte, et dit pourquoi au journal.
  void StopFor(std::string_view why);
  void Compose(const VoiceFacts& facts);
  // La barre de la voix : ses pastilles, ses boutons, son apparence.
  void ComposeBar(const VoiceFacts& facts, bool transmitting);

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
  VoiceIdentity started_as_;
  std::uint32_t online_since_ms_ = 0;
  bool was_online_ = false;
  // LE CANAL OÙ L'ON PARLE : celui de la touche tenue, gardé tant que la
  // parole tient le délai au relâchement — une fin de phrase ne change pas de
  // canal. Le canal par défaut quand rien ne part.
  std::optional<int> talk_channel_;
  // Au rang de `VoiceTalkKey`.
  std::array<VoicePressGate, kVoiceTalkKeyCount> press_gates_{};
  bool transmitting_ = false;
  ro::ui::VoiceOverlayState overlay_;
  std::optional<float> input_percent_;
  // Relevés à la dernière pompe, vides quand la liaison est fermée.
  std::vector<ro::voice::NearbyPlayer> heard_;
  std::vector<ro::voice::NearbyPlayer> blocked_;
};

}  // namespace moonclient
