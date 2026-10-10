#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/net/connection.h"

namespace ro::net {

// LA POIGNÉE DE MAIN QUI DÉCIDE DE LA SESSION.
//
// 🔴 ELLE N'OUVRE PAS UNE FONCTIONNALITÉ : ELLE GARDE LA SESSION ELLE-MÊME.
// Quinze secondes après l'entrée en carte, un serveur qui n'a pas reçu ce
// paquet affiche l'avis de patch, puis coupe la session cinq secondes plus
// tard. Sans elle, ce client ne tient pas vingt secondes sur une carte.
//
// Le serveur ne répond aux paquets personnalisés `0x0F00+`, dans un sens comme
// dans l'autre, qu'à une session qui l'a donnée.

// ── CE QUE NOUS DÉCLARONS ───────────────────────────────────────────────────
//
// `CZ_MOONCLIENT_INTEGRITY`, qui nomme ce client au serveur. Le serveur
// l'accepte tant que sa configuration porte `allow_moonclient: 1` (ADR 0105).
//
// Deux champs, tous deux LUS sur cette machine :
//   - le MachineGuid, qui dit la MACHINE et laisse à l'exploitant sa détection
//     de multi-comptes ;
//   - le dernier patch appliqué par le patcheur intégré, que le serveur compare
//     à `moonclient_min_patch_index`.

// Le MachineGuid tient sur 36 caractères et n'est PAS terminé par un zéro :
// c'est ce que le serveur lit, et un octet de moins décalerait le champ suivant.
inline constexpr std::size_t kMachineGuidChars = 36;

// Ce que le serveur lit d'une installation qui n'a jamais été patchée.
inline constexpr std::int32_t kNeverPatched = -1;

// Ce que le client dit de lui-même au serveur.
struct ClientIdentity {
  // Vide quand la machine ne sait pas le dire — le serveur tolère un champ nul
  // et se contente alors de ne pas faire sa détection de multi-comptes.
  std::string machine_guid;
  std::int32_t patch_index = kNeverPatched;
};

// Le MachineGuid de cette machine, ou une chaîne vide.
//
// ⚠ IL FAUT LA VUE 64 BITS de la base de registre (`KEY_WOW64_64KEY`) : un
// processus 32 bits est redirigé vers `Wow6432Node`, où cette valeur n'existe
// pas. Rend toujours vide hors Windows, où la valeur n'existe pas du tout.
[[nodiscard]] std::string ReadMachineGuid();

// L'identité complète : le GUID relevé, et le dernier patch appliqué tel que le
// patcheur l'a consigné — rien quand il n'a jamais tourné. Un index trop grand
// pour le champ du paquet s'y écrit plafonné, jamais tronqué en négatif.
[[nodiscard]] ClientIdentity ClientIdentityOf(std::optional<std::uint64_t> last_applied_patch);

// Où en est la poignée de main. C'est `app` qui en parle au journal : cette
// couche transporte des octets et ne dit rien.
enum class MoonclientHandshakeState {
  kUnsent,
  // Partie, et sans réponse pour l'instant.
  kAwaiting,
  // Le serveur a répondu par ses réglages : le client est reconnu.
  kVerified,
  // Le serveur a répondu par son avis de coupure. La déconnexion suit à cinq
  // secondes, et rien ne la rattrape.
  kRefused,
  // Cinq envois sans un mot. Le serveur ne nous voit pas ; la coupure viendra
  // de son minuteur.
  kGivenUp,
};

// Le délai de renvoi et le nombre d'envois.
//
// 🔴 « ENVOYÉ » N'EST PAS « RÉPONDU », et c'est toute la raison de ce renvoi :
// le premier paquet part pendant la transition personnage → carte et peut mourir
// en route sans que l'émission le sache. Tant que le serveur n'a répondu ni par
// ses réglages ni par son avis de coupure, on rejoue.
inline constexpr std::uint32_t kHandshakeRetryMs = 3000;
inline constexpr int kHandshakeMaxAttempts = 5;

// Le serveur n'acquitte pas la poignée de main : il envoie les réglages du
// joueur (`0x0F05`), et c'est ce premier paquet personnalisé qui vaut accusé.
class MoonclientHandshake {
 public:
  explicit MoonclientHandshake(ClientIdentity identity) : identity_(std::move(identity)) {}

  // Émet la poignée de main, et la rejoue tant que rien n'a répondu.
  void Pump(Connection& link, std::uint32_t now_ms);

  // À nourrir de CHAQUE trame reçue : c'est ainsi que la poignée de main apprend
  // qu'elle est acceptée ou refusée.
  void NoteFrame(std::uint16_t opcode);

  [[nodiscard]] MoonclientHandshakeState state() const noexcept { return state_; }
  [[nodiscard]] int attempts() const noexcept { return attempts_; }
  [[nodiscard]] const ClientIdentity& identity() const noexcept { return identity_; }

 private:
  ClientIdentity identity_;
  MoonclientHandshakeState state_ = MoonclientHandshakeState::kUnsent;
  int attempts_ = 0;
  std::uint32_t last_send_ms_ = 0;
};

// Ce que le journal en dit. Une phrase par état, parce qu'une session coupée
// vingt secondes après l'entrée ne laisse rien d'autre à relire.
[[nodiscard]] std::string_view DescribeHandshake(MoonclientHandshakeState state);

}  // namespace ro::net
