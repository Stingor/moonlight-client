// La poignée de main d'intégrité : ce que le client déclare de lui-même, et ce
// que le serveur en fait. Voir l'en-tête pour ce qui est déclaré et pourquoi.

#include "ro/net/moonclient_handshake.h"

#include <algorithm>
#include <limits>

#include "ro/proto/map_packets.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace ro::net {

std::string ReadMachineGuid() {
#if defined(_WIN32)
  HKEY key = nullptr;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0,
                    KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
    return {};
  }
  char value[kMachineGuidChars + 1] = {};
  DWORD size = sizeof(value);
  const LSTATUS status = RegQueryValueExA(key, "MachineGuid", nullptr, nullptr,
                                          reinterpret_cast<LPBYTE>(value), &size);
  RegCloseKey(key);
  if (status != ERROR_SUCCESS || size < kMachineGuidChars) {
    return {};
  }
  // ⚠ CONSTRUITE SUR SA LONGUEUR, pas sur son zéro terminal : le champ du paquet
  // n'en porte pas, et la valeur du registre en porte un que `size` compte.
  return std::string(value, kMachineGuidChars);
#else
  return {};
#endif
}

ClientIdentity ClientIdentityOf(std::optional<std::uint64_t> last_applied_patch) {
  ClientIdentity identity;
  identity.machine_guid = ReadMachineGuid();
  if (last_applied_patch.has_value()) {
    constexpr auto kLargestIndex =
        static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    identity.patch_index = static_cast<std::int32_t>(std::min(*last_applied_patch, kLargestIndex));
  }
  return identity;
}

void MoonclientHandshake::Pump(Connection& link, std::uint32_t now_ms) {
  if (state_ == MoonclientHandshakeState::kVerified ||
      state_ == MoonclientHandshakeState::kRefused ||
      state_ == MoonclientHandshakeState::kGivenUp) {
    return;
  }
  if (state_ == MoonclientHandshakeState::kAwaiting) {
    if (now_ms - last_send_ms_ < kHandshakeRetryMs) {
      return;
    }
    if (attempts_ >= kHandshakeMaxAttempts) {
      state_ = MoonclientHandshakeState::kGivenUp;
      return;
    }
  }

  proto::CzMoonclientIntegrity declaration;
  declaration.machine_guid = identity_.machine_guid;
  declaration.patch_index = identity_.patch_index;
  link.Send(proto::Encode(declaration));

  last_send_ms_ = now_ms;
  ++attempts_;
  state_ = MoonclientHandshakeState::kAwaiting;
}

void MoonclientHandshake::NoteFrame(std::uint16_t opcode) {
  if (state_ == MoonclientHandshakeState::kVerified ||
      state_ == MoonclientHandshakeState::kRefused) {
    return;
  }
  if (opcode == proto::ZcMoonlightSettings::kOpcode) {
    state_ = MoonclientHandshakeState::kVerified;
    return;
  }
  if (opcode == proto::ZcMoonlightKickNotice::kOpcode) {
    state_ = MoonclientHandshakeState::kRefused;
  }
}

std::string_view DescribeHandshake(MoonclientHandshakeState state) {
  switch (state) {
    case MoonclientHandshakeState::kUnsent:
      return "poignée de main d'intégrité : pas encore émise";
    case MoonclientHandshakeState::kAwaiting:
      return "poignée de main d'intégrité émise, sans réponse pour l'instant";
    case MoonclientHandshakeState::kVerified:
      return "poignée de main d'intégrité acceptée : le serveur nous a reconnus";
    case MoonclientHandshakeState::kRefused:
      return "poignée de main d'intégrité REFUSÉE : le serveur coupe la session dans cinq secondes";
    case MoonclientHandshakeState::kGivenUp:
      return "poignée de main d'intégrité sans réponse après cinq envois : la session sera coupée";
  }
  return {};
}

}  // namespace ro::net
