#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ro::sim {

// ── LA LISTE D'AMIS, TELLE QUE LE SERVEUR LA DÉCRIT ─────────────────────────
//
// Le serveur pousse la liste entière à l'entrée en jeu, puis l'état en ligne de
// chaque ami connecté ; il redit ensuite chaque connexion, chaque ajout et
// chaque retrait. Un ami se repère par son compte ET son personnage : c'est un
// personnage qu'on a pour ami, pas un compte.

// La taille maximale d'une liste d'amis sur ce serveur.
inline constexpr std::size_t kMaxFriends = 40;

struct Friend {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  // En UTF-8, déjà converti depuis le fil.
  std::string name;
  bool online = false;
};

// Une demande d'amitié reçue, en attente de réponse.
struct FriendInvitation {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  // Le nom de qui DEMANDE.
  std::string name;
};

// CE QUE LE SERVEUR RÉPOND, à relayer dans la conversation ; `name` est l'autre
// personnage.
enum class FriendNoticeKind : std::uint8_t {
  kBefriended,
  kDeclined,
  kOwnListFull,
  kTheirListFull,
  kAlreadyRequested,
  kAlreadyFriends,
  kNotFound,
  kFriendOfAnotherCharacter,
};

struct FriendNotice {
  FriendNoticeKind kind = FriendNoticeKind::kBefriended;
  std::string name;
};

class FriendList {
 public:
  // La liste entière : elle REMPLACE la précédente, et tout le monde y part
  // hors ligne jusqu'à ce que le serveur dise le contraire.
  void Replace(std::vector<Friend> friends);
  // Un ami ajouté, ou redécrit s'il y était déjà.
  void Add(Friend added);
  // Rend faux si ce personnage n'était pas dans la liste.
  bool Remove(std::uint32_t account_id, std::uint32_t char_id);
  // Rend faux si ce personnage n'est pas dans la liste : le serveur ne parle
  // que de nos amis, et on n'en invente pas un.
  bool TellOnline(std::uint32_t account_id, std::uint32_t char_id, bool online, std::string name);
  void Clear() { friends_.clear(); }

  [[nodiscard]] std::span<const Friend> friends() const noexcept { return friends_; }
  [[nodiscard]] const Friend* Find(std::uint32_t account_id, std::uint32_t char_id) const;
  [[nodiscard]] const Friend* FindByAccount(std::uint32_t account_id) const;
  // ⚠ COMPARAISON EXACTE, en UTF-8 : c'est par le nom qu'une demande voyage.
  [[nodiscard]] const Friend* FindByName(std::string_view name) const;
  [[nodiscard]] bool HasName(std::string_view name) const { return FindByName(name) != nullptr; }
  [[nodiscard]] bool full() const noexcept { return friends_.size() >= kMaxFriends; }

  // Une nouvelle demande remplace la précédente.
  [[nodiscard]] const std::optional<FriendInvitation>& invitation() const noexcept {
    return invitation_;
  }
  void TellInvitation(FriendInvitation invitation) { invitation_ = std::move(invitation); }
  void ForgetInvitation() noexcept { invitation_.reset(); }

  // Dans l'ordre d'arrivée ; les prendre les retire.
  void Notify(FriendNotice notice) { notices_.push_back(std::move(notice)); }
  [[nodiscard]] std::vector<FriendNotice> TakeNotices();

 private:
  Friend* FindMutable(std::uint32_t account_id, std::uint32_t char_id);

  std::vector<Friend> friends_;
  std::optional<FriendInvitation> invitation_;
  std::vector<FriendNotice> notices_;
};

}  // namespace ro::sim
