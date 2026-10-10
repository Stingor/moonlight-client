#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/chat_line.h"

namespace ro::sim {

// LE SALON DE DISCUSSION DU JOUEUR : celui qu'il crée, celui où il entre, et ce
// qui s'y dit.
//
// 🔴 LE CRÉATEUR NE REÇOIT JAMAIS L'ENSEIGNE DE SON PROPRE SALON : le serveur
// la diffuse à la zone en excluant sa source. Ce que sa fenêtre montre — titre,
// accès, limite — ne peut donc venir que de SA demande, retenue au départ.
//
// 🔴 ENTRER CHEZ UN AUTRE NE DIT NI TITRE, NI LIMITE, NI ACCÈS : la réponse ne
// porte que l'identifiant et les membres. Le reste se lit sur l'enseigne que la
// zone a déjà montrée ; sans elle, le client d'origine n'ouvre rien, et celui-ci
// non plus.
//
// ⚠ LE SERVEUR NE RENVOIE JAMAIS LE MOT DE PASSE. Le seul endroit où il existe
// encore côté client est ce que le chef a lui-même envoyé.

// Les plafonds du SERVEUR, que le client d'origine reprend à l'octet près :
// les dépasser serait promettre ce qui sera coupé.
inline constexpr std::size_t kChatRoomTitleMostBytes = 36;
inline constexpr std::size_t kChatRoomPasswordMostBytes = 8;
// Le plancher d'un mot de passe, exigé du seul salon PRIVÉ.
inline constexpr std::size_t kChatRoomPasswordFewestBytes = 4;
inline constexpr std::uint16_t kChatRoomFewestSeats = 2;
inline constexpr std::uint16_t kChatRoomMostSeats = 20;

// L'écart minimal entre deux demandes d'entrée : une seconde, comme le client
// d'origine, qui libère aussi la demande dès que le serveur y a répondu.
inline constexpr std::uint32_t kChatRoomEnterSpacingMs = 1000;
// Le silence au bout duquel une création sans réponse est tenue pour perdue :
// la fenêtre rend alors son bouton.
inline constexpr std::uint32_t kChatRoomCreateSilenceMs = 5000;
// Le temps laissé à une expulsion pour aboutir. Le serveur ne répond qu'en
// faisant sortir le membre ; un membre encore là après ce délai a été protégé.
inline constexpr std::uint32_t kChatRoomExpelSilenceMs = 1500;

// Les lignes qu'un salon garde : celles d'un genre de la conversation.
inline constexpr std::size_t kChatRoomLinesKept = static_cast<std::size_t>(kChatLinesKeptPerKind);

// ── LA RÉPONSE À UNE CRÉATION (`e_create_chatroom`) ─────────────────────────
inline constexpr std::uint8_t kChatRoomCreated = 0;
inline constexpr std::uint8_t kChatRoomTooMany = 1;
inline constexpr std::uint8_t kChatRoomSameTitle = 2;

// ── LES REFUS D'ENTRÉE (`e_refuse_enter_room`) ──────────────────────────────
inline constexpr std::uint8_t kChatRoomRefusedFull = 0;
inline constexpr std::uint8_t kChatRoomRefusedPassword = 1;
inline constexpr std::uint8_t kChatRoomRefusedExpelled = 2;
// Trois, c'est l'entrée accordée : elle ne passe pas par le refus.
inline constexpr std::uint8_t kChatRoomRefusedZeny = 4;
inline constexpr std::uint8_t kChatRoomRefusedLevelLow = 5;
inline constexpr std::uint8_t kChatRoomRefusedLevelHigh = 6;
inline constexpr std::uint8_t kChatRoomRefusedJob = 7;

// Le rôle que porte un membre sur le fil : zéro est le chef.
inline constexpr std::uint32_t kChatRoomOwnerRole = 0;

// Ce qu'on demande au serveur : à la création comme au changement de réglages.
struct ChatRoomSettings {
  // En UTF-8 ; c'est le traducteur qui l'écrit dans l'encodage du serveur.
  std::string title;
  std::string password;
  bool is_public = true;
  std::uint16_t limit = kChatRoomMostSeats;
};

struct ChatRoomMember {
  std::string name;
  bool owner = false;
};

// LE SALON OÙ L'ON SE TROUVE.
struct ChatRoomInside {
  std::uint32_t room_id = 0;
  std::string title;
  // L'état du serveur : zéro privé, un public, deux et trois des salles de PNJ
  // (voir `kRoomPrivate`). Seul zéro s'écrit « privé ».
  std::uint8_t status = kRoomPublic;
  std::uint16_t users = 0;
  std::uint16_t limit = 0;
  // Ce que le chef a envoyé, seule trace du mot de passe. Vide pour qui n'a
  // fait qu'entrer.
  std::string password;
  std::vector<ChatRoomMember> members;
  // Le joueur porte-t-il les droits de chef ?
  bool owned = false;

  [[nodiscard]] bool is_public() const noexcept { return status != kRoomPrivate; }
};

// CE QUE LE SALON A À DIRE, que l'appelant écrit avec les phrases du client.
enum class ChatRoomNewsKind : std::uint8_t {
  // Les trois réponses à une création.
  kCreated,
  kTooManyRooms,
  kSameTitle,
  // Le serveur n'a pas répondu à la création.
  kCreateSilent,
  // L'entrée refusée ; la raison est dans `reason`.
  kEnterRefused,
  // Un membre arrive, part, est expulsé ; son nom est dans `name`.
  kMemberJoined,
  kMemberLeft,
  kMemberExpelled,
  // Une expulsion demandée n'a rien fait : le serveur protège ce membre, sans
  // le dire. Son nom est dans `name`.
  kExpelRefused,
};

struct ChatRoomNews {
  ChatRoomNewsKind kind = ChatRoomNewsKind::kCreated;
  std::uint8_t reason = 0;
  std::string name;
};

class ChatRoom {
 public:
  // ── LA CRÉATION ───────────────────────────────────────────────────────────
  // La demande attend sa réponse : elle ne repart pas.
  [[nodiscard]] bool creating() const noexcept { return creating_.has_value(); }
  void NoteCreateSent(ChatRoomSettings settings, std::uint32_t now_ms);
  // La réponse du serveur. Acceptée, le joueur est dans son salon, seul et chef.
  //
  // 🔴 UNE ACCEPTATION TARDIVE OUVRE QUAND MÊME LE SALON : le serveur y a mis le
  // joueur, qui ne peut plus marcher et dont la parole y part. La demande
  // relâchée par le silence se garde donc jusqu'à la réponse suivante.
  void TellCreateAnswer(std::uint8_t status, std::string_view self_name);
  // Libère une création restée sans réponse : le bouton se rend.
  void ExpireSilentCreate(std::uint32_t now_ms);

  // ── L'ENTRÉE ──────────────────────────────────────────────────────────────
  // Une demande d'entrée peut-elle partir ? Ni depuis un salon, ni tant que la
  // précédente attend — au plus une seconde.
  [[nodiscard]] bool EnterMayLeave(std::uint32_t now_ms) const noexcept;
  void NoteEnterSent(std::uint32_t now_ms);
  void TellEnterRefused(std::uint8_t reason);
  // Le joueur est entré. Titre, accès et limite viennent de l'enseigne.
  void TellEntered(ChatRoomInside room, std::string_view self_name);
  // LE SERVEUR NOUS A FAIT ENTRER, mais l'enseigne qui dit le salon manque : rien
  // ne peut s'ouvrir. On en ressort plutôt que de rester occupant d'un salon
  // qu'aucune fenêtre ne montre — et qui empêcherait de marcher.
  void TellBlindEntry();
  // Vrai UNE fois après une entrée aveugle : la sortie est à dire au serveur.
  [[nodiscard]] bool TakeLeaveOwed() noexcept { return std::exchange(leave_owed_, false); }

  // ── LA VIE DU SALON ───────────────────────────────────────────────────────
  [[nodiscard]] const std::optional<ChatRoomInside>& inside() const noexcept { return inside_; }
  [[nodiscard]] bool in_room() const noexcept { return inside_.has_value(); }
  void TellMemberJoined(std::uint16_t users, std::string name);
  // Un membre part ; le joueur lui-même, et le salon se ferme.
  void TellMemberLeft(std::uint16_t users, std::string_view name, bool expelled,
                      std::string_view self_name);
  void TellSettings(std::uint16_t limit, std::uint16_t users, std::uint8_t status,
                    std::string title);
  void TellRole(std::uint32_t role, std::string_view name, std::string_view self_name);
  // Le salon a disparu : s'il était le nôtre, on en sort.
  void TellRoomGone(std::uint32_t room_id);
  // UNE EXPULSION ENVOYÉE. Le serveur n'accuse rien : il fait sortir le
  // membre, ou se tait quand ce membre est protégé (permission du staff). Le
  // silence se constate donc à la montre.
  void NoteExpelSent(std::string name, std::uint32_t now_ms);
  // Annonce, une fois, chaque expulsion restée sans effet après le délai.
  void ExpireSilentExpels(std::uint32_t now_ms);
  // Retient le mot de passe d'un changement de réglages envoyé.
  void NoteSettingsSent(const ChatRoomSettings& settings);
  // On sort, quelle qu'en soit la raison : demandée, ou carte quittée.
  void Leave();

  // ── CE QUI S'Y DIT ────────────────────────────────────────────────────────
  void RecordLine(ChatKind kind, std::uint32_t speaker_id, std::string text, std::uint32_t color,
                  std::uint32_t said_ms, std::optional<ChatAuthor> author = std::nullopt);
  [[nodiscard]] std::span<const ChatLine> lines() const noexcept { return lines_; }
  // Change à chaque salon ouvert : la fenêtre s'y remet à neuf.
  [[nodiscard]] std::uint32_t room_seq() const noexcept { return room_seq_; }

  // Les nouvelles, dans l'ordre, une seule fois.
  [[nodiscard]] std::vector<ChatRoomNews> TakeNews() { return std::exchange(news_, {}); }

  // Une partie commence : rien de la précédente ne survit.
  void Reset();

 private:
  struct PendingCreate {
    ChatRoomSettings settings;
    std::uint32_t sent_ms = 0;
  };
  struct PendingExpel {
    std::string name;
    std::uint32_t sent_ms = 0;
  };

  void Open(ChatRoomInside room);
  void Say(ChatRoomNewsKind kind, std::uint8_t reason = 0, std::string name = {});

  std::optional<PendingCreate> creating_;
  // La demande que le silence a relâchée, gardée pour une réponse tardive.
  std::optional<ChatRoomSettings> late_create_;
  bool leave_owed_ = false;
  std::optional<std::uint32_t> enter_sent_ms_;
  std::optional<ChatRoomInside> inside_;
  std::vector<PendingExpel> expels_;
  std::vector<ChatLine> lines_;
  std::uint64_t lines_seen_ = 0;
  std::uint32_t room_seq_ = 0;
  std::vector<ChatRoomNews> news_;
};

}  // namespace ro::sim
