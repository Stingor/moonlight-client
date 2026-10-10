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

// ── LE GROUPE DU JOUEUR, TEL QUE LE SERVEUR LE DÉCRIT ───────────────────────
//
// Il vit à part du monde, comme la guilde : c'est une famille à elle, et
// `world.h` touche son plafond de lignes.
//
// Le serveur pousse la liste entière à l'entrée en jeu et à chaque changement
// de carte ou de connexion d'un membre, et un membre seul quand il rejoint le
// groupe ou qu'on le redécrit. ⚠ LE JOUEUR Y FIGURE LUI-MÊME : ce qui l'en
// distingue est à l'appelant, qui connaît son compte.

// La taille maximale d'un groupe sur ce serveur.
inline constexpr std::size_t kMaxPartyMembers = 24;

// Au-delà, une réponse de PS n'est plus crue : un membre qui s'éloigne cesse de
// répondre, et montrer son dernier PS indéfiniment serait pire que rien.
inline constexpr std::uint32_t kPartyMagicStaleMs = 3000;

// Une jauge dont le serveur a donné les deux bouts.
struct PartyGauge {
  std::int32_t current = 0;
  std::int32_t max = 0;
};

// Le PS d'un membre, et l'instant de SA réponse, sur l'horloge de l'appelant.
struct PartyMagic {
  PartyGauge gauge;
  std::uint32_t received_ms = 0;
};

// Le point d'un membre sur la carte, en cellules.
struct PartyPosition {
  std::int16_t x = 0;
  std::int16_t y = 0;
};

struct PartyMember {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  // En UTF-8, déjà converti depuis le fil.
  std::string name;
  // Le nom BRUT de sa carte, extension comprise (« prontera.gat »).
  std::string map;
  bool leader = false;
  // Vrai quand le membre n'est pas connecté : il reste dans le groupe.
  bool offline = false;
  bool dead = false;
  std::uint16_t job = 0;
  std::uint16_t base_level = 0;
  // ⚠ RIEN N'EST PAS ZÉRO. Le serveur ne dit la position qu'aux membres de la
  // même carte, et la vie qu'à ceux qui sont à portée de vue : ce qu'on n'a pas
  // reçu reste vide. Un changement de carte ou une déconnexion les oublie.
  std::optional<PartyPosition> position;
  std::optional<PartyGauge> health;
  std::optional<PartyMagic> magic;
};

// Les règles du groupe telles que le serveur les annonce.
struct PartyOptions {
  bool exp_shared = false;
  bool pickup_shared = false;
  bool loot_shared = false;

  friend bool operator==(const PartyOptions&, const PartyOptions&) = default;
};

// Les raisons, venues du serveur, qui écartent un membre du partage d'EXP.
inline constexpr std::uint8_t kShareIdle = 0x01;
inline constexpr std::uint8_t kShareDead = 0x02;
inline constexpr std::uint8_t kShareBusy = 0x04;
// Celle que le client ajoute lui-même : le membre n'est pas sur ma carte. Le
// serveur ne peut pas l'annoncer, la carte qui compte étant celle du monstre.
inline constexpr std::uint8_t kShareOtherMap = 0x80;

struct PartyShareRow {
  std::uint32_t account_id = 0;
  std::uint8_t flags = 0;
};

// Une invitation reçue, en attente de réponse.
struct PartyInvitation {
  std::uint32_t party_id = 0;
  // Le nom du GROUPE, pas celui de qui invite.
  std::string party_name;
};

// CE QUE LE SERVEUR RÉPOND, à relayer dans la conversation.
enum class PartyNoticeKind : std::uint8_t {
  // La création.
  kCreated,
  kNameTaken,
  kAlreadyInParty,
  kCreationMapRefused,
  // La réponse à une invitation qu'on a envoyée ; `name` est l'invité.
  kInviteeInOtherParty,
  kInviteRefused,
  kInviteAccepted,
  kPartyFull,
  kSameAccountJoined,
  kInvitationsBlocked,
  kInviteeNotFound,
  kInviteeMapRefused,
  kJoinMapRefused,
  kMemorialDungeon,
  kInviteeLevelRefused,
  kInviteeLacksBasicSkill,
  // Le départ.
  kYouLeft,
  kLeaveRefused,
  kExpelRefused,
  // `name` est le nouveau chef.
  kLeaderChanged,
  // `options` porte les règles nouvelles.
  kOptionsChanged,
  kExpShareRefused,
  // Un refus du client lui-même : un chef ne quitte qu'après avoir délégué.
  kLeaveNeedsHandOver,
};

struct PartyNotice {
  PartyNoticeKind kind = PartyNoticeKind::kCreated;
  std::string name;
  PartyOptions options;
};

class PartyRoster {
 public:
  // ── LES MEMBRES ─────────────────────────────────────────────────────────
  //
  // La liste entière REMPLACE la précédente. Un membre qui y reste sur la même
  // carte et en ligne garde ce que la liste ne porte pas — position, vie, PS.
  void Replace(std::string party_name, std::vector<PartyMember> members);
  // Un membre ajouté ou redécrit, repéré par son compte ; même règle.
  void Upsert(PartyMember member);
  // Un membre parti. Rend faux si ce compte n'en était pas.
  bool Remove(std::uint32_t account_id);
  // Plus de groupe : le joueur l'a quitté ou en a été exclu. Les règles et le
  // partage s'en vont avec lui ; l'invitation et les avis restent.
  void Clear();

  [[nodiscard]] bool in_party() const noexcept { return !members_.empty(); }
  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  void TellName(std::string party_name) { name_ = std::move(party_name); }
  [[nodiscard]] std::span<const PartyMember> members() const noexcept { return members_; }
  [[nodiscard]] const PartyMember* Find(std::uint32_t account_id) const;
  // ⚠ COMPARAISON EXACTE, en UTF-8 : c'est par le nom qu'une invitation voyage.
  [[nodiscard]] const PartyMember* FindByName(std::string_view name) const;
  // VRAI POUR UN MEMBRE CONNECTÉ, et pour lui seul : c'est la question que le
  // client pose avant d'écrire ce qui arrive à son groupe.
  [[nodiscard]] bool HasOnlineMember(std::uint32_t account_id) const;
  [[nodiscard]] bool IsLeader(std::uint32_t account_id) const;
  [[nodiscard]] bool AmILeader(std::uint32_t own_account_id) const {
    return IsLeader(own_account_id);
  }

  // ── CE QUE LE SERVEUR REDIT D'UN MEMBRE ─────────────────────────────────
  // Rendent faux pour un compte qui n'est pas du groupe.
  bool TellPosition(std::uint32_t account_id, std::optional<PartyPosition> position);
  bool TellHealth(std::uint32_t account_id, PartyGauge health);
  bool TellMagic(std::uint32_t account_id, PartyGauge magic, std::uint32_t now_ms);
  bool ForgetMagic(std::uint32_t account_id);
  bool TellDead(std::uint32_t account_id, bool dead);
  bool TellJobAndLevel(std::uint32_t account_id, std::uint16_t job, std::uint16_t base_level);
  // Le chef change. Rend faux si le nouveau chef n'est pas dans la liste, et
  // rien ne bouge alors.
  bool TellLeader(std::uint32_t previous_account_id, std::uint32_t new_account_id);

  // Le PS d'un membre s'il est connu et frais, sinon rien.
  [[nodiscard]] std::optional<PartyGauge> FreshMagic(std::uint32_t account_id,
                                                     std::uint32_t now_ms) const;

  // ── LES RÈGLES ──────────────────────────────────────────────────────────
  [[nodiscard]] const std::optional<PartyOptions>& options() const noexcept { return options_; }
  // Les règles complètes. Un CHANGEMENT de règles connues, dans un groupe,
  // s'annonce ; la toute première lecture ne s'annonce pas.
  void TellOptions(PartyOptions options);
  // Les deux règles d'objets seules, redites avec un membre : elles ne
  // s'annoncent pas, et l'EXP reste ce qu'on en savait.
  void TellItemOptions(bool pickup_shared, bool loot_shared);
  // Le joueur refuse-t-il toute invitation ? Rien tant que le serveur ne l'a
  // pas dit.
  [[nodiscard]] std::optional<bool> refuses_invitations() const noexcept {
    return refuses_invitations_;
  }
  void TellRefusesInvitations(bool refuses) { refuses_invitations_ = refuses; }

  // ── LE PARTAGE D'EXP ────────────────────────────────────────────────────
  //
  // ⚠ L'ÉTAT COMPLET REMPLACE le précédent : un membre absent de la liste
  // reçue n'est pas « inconnu », il reçoit sa part.
  void TellShare(std::uint16_t idle_s, std::vector<PartyShareRow> rows);
  [[nodiscard]] bool share_known() const noexcept { return share_known_; }
  // Le seuil d'inactivité du serveur ; zéro veut dire que la règle est éteinte.
  [[nodiscard]] std::uint16_t share_idle_s() const noexcept { return share_idle_s_; }
  // Les raisons qui écartent ce membre de MES gains, ou zéro. Ajoute
  // `kShareOtherMap` quand sa carte n'est pas la mienne ; un membre hors ligne,
  // ou un état encore inconnu, rend zéro.
  [[nodiscard]] std::uint8_t ShareFlagsFor(std::uint32_t account_id,
                                           std::uint32_t own_account_id) const;

  // ── L'INVITATION REÇUE ──────────────────────────────────────────────────
  // Une nouvelle invitation remplace la précédente.
  [[nodiscard]] const std::optional<PartyInvitation>& invitation() const noexcept {
    return invitation_;
  }
  void TellInvitation(PartyInvitation invitation) { invitation_ = std::move(invitation); }
  void ForgetInvitation() noexcept { invitation_.reset(); }

  // ── LES AVIS À RELAYER ──────────────────────────────────────────────────
  //
  // Dans l'ordre d'arrivée ; les prendre les retire.
  void Notify(PartyNotice notice) { notices_.push_back(std::move(notice)); }
  [[nodiscard]] std::vector<PartyNotice> TakeNotices();

 private:
  PartyMember* FindMutable(std::uint32_t account_id);

  std::string name_;
  std::vector<PartyMember> members_;
  std::optional<PartyOptions> options_;
  std::optional<bool> refuses_invitations_;
  std::vector<PartyShareRow> share_;
  std::uint16_t share_idle_s_ = 0;
  bool share_known_ = false;
  std::optional<PartyInvitation> invitation_;
  std::vector<PartyNotice> notices_;
};

// ── QUITTER LE GROUPE ───────────────────────────────────────────────────────
//
// Un chef ne laisse pas le groupe sans tête : avant de partir, il passe la
// direction au premier autre membre EN LIGNE qui se trouve sur SA carte.
//
//   · kLeave          — pas chef, ou seul : partir tout de suite ;
//   · kHandOverThenLeave — passer la direction à `successor`, puis partir ;
//   · kNeedsHandOver  — des membres sont en ligne, aucun sur ma carte : ne PAS
//                       partir, et le dire (`kLeaveNeedsHandOver`) ;
//   · kConfirmDisband — personne n'est en ligne : demander confirmation, le
//                       groupe restant sans personne pour le mener.
enum class LeaveStep : std::uint8_t {
  kLeave,
  kHandOverThenLeave,
  kNeedsHandOver,
  kConfirmDisband,
};
struct LeavePlan {
  LeaveStep step = LeaveStep::kLeave;
  std::uint32_t successor_account_id = 0;
};
[[nodiscard]] LeavePlan PlanLeave(const PartyRoster& party, std::uint32_t own_account_id);

}  // namespace ro::sim
