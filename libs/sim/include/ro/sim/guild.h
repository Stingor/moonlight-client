#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ro::sim {

// ── LA GUILDE, TELLE QUE LE SERVEUR LA DÉCRIT ───────────────────────────────
//
// Elle vit à part du monde, comme la banque et le dialogue d'un PNJ : c'est une
// famille à elle, et `world.h` touche son plafond de lignes.
//
// 🔴 LE SERVEUR NE LA POUSSE PAS EN ENTIER. L'appartenance et l'annonce
// arrivent seules à l'entrée en jeu — et à l'adhésion, l'en-tête de la guilde
// SEUL, qui tient alors lieu d'appartenance ; les membres, les postes, les compétences
// et les expulsions ne viennent que sur DEMANDE — ce que fait la fenêtre de
// guilde à son ouverture. Chaque morceau dit donc s'il est déjà arrivé :
// « aucun membre » et « membres pas encore reçus » ne se montrent pas pareil.
//
// ⚠ LES NOMS SONT EN UTF-8, déjà convertis depuis le fil.

// LES DROITS D'UN POSTE, tels que le serveur les masque.
inline constexpr std::int32_t kGuildMayInvite = 0x001;
inline constexpr std::int32_t kGuildMayExpel = 0x010;
inline constexpr std::int32_t kGuildMayStorage = 0x100;
inline constexpr std::int32_t kGuildRightsMask =
    kGuildMayInvite | kGuildMayExpel | kGuildMayStorage;

// LES ONGLETS DE LA FENÊTRE DE GUILDE que le serveur ouvre au personnage
// (`ZC_ACK_GUILD_MENUINTERFACE`). Le maître reçoit tout, un simple membre tout
// sauf l'annonce. Les informations et les compétences ne se masquent pas.
inline constexpr std::uint32_t kGuildMenuMembers = 0x01;
inline constexpr std::uint32_t kGuildMenuPositions = 0x02;
inline constexpr std::uint32_t kGuildMenuBans = 0x10;
inline constexpr std::uint32_t kGuildMenuNotice = 0x80;

// LE NOMBRE DE POSTES D'UNE GUILDE, et le poste du maître.
inline constexpr int kGuildPositionCount = 20;
inline constexpr std::int32_t kGuildMasterPosition = 0;

// LA PART D'EXPÉRIENCE QU'UN POSTE PEUT CÉDER au plus, en pour cent. C'est le
// défaut du serveur, qui borne de toute façon.
inline constexpr int kGuildPayRateMost = 50;

// LES COMPÉTENCES DE GUILDE commencent à ce numéro : le serveur aiguille une
// montée de niveau vers la guilde dès qu'un identifiant l'atteint.
inline constexpr std::uint16_t kFirstGuildSkill = 10000;

// LA SEULE COMPÉTENCE DE GUILDE QU'UN SIMPLE MEMBRE PEUT LANCER : le cri de
// charge (GD_CHARGESHOUT_BEATING). Le serveur refuse toutes les autres à qui
// n'est pas le maître.
inline constexpr std::uint16_t kMemberGuildSkill = 10018;

struct GuildMember {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::string name;
  std::int16_t hair = 0;
  std::int16_t hair_palette = 0;
  // Zéro pour une femme, un pour un homme — le codage du serveur.
  std::int16_t sex = 0;
  std::int16_t job = 0;
  std::int16_t level = 0;
  std::int32_t contribution = 0;
  bool online = false;
  std::int32_t position = 0;
  // Un instant Unix, en secondes. Zéro quand le serveur ne l'a pas.
  std::uint32_t last_login = 0;
};

// Le point d'un membre de la guilde sur la carte, en cellules.
struct GuildMemberCell {
  std::int16_t x = 0;
  std::int16_t y = 0;
};

struct GuildPosition {
  std::string name;
  std::int32_t rights = 0;
  std::int32_t pay_rate = 0;
  // ⚠ LES DEUX MOITIÉS ARRIVENT PAR DEUX PAQUETS, et une ligne ne s'édite que
  // si ses DROITS sont connus : renvoyer un poste dont on ignore le masque le
  // remettrait à zéro chez le serveur.
  bool has_name = false;
  bool has_rights = false;
};

enum class GuildRelationKind : std::uint8_t { kAlly = 0, kEnemy = 1 };

struct GuildRelation {
  std::int32_t guild_id = 0;
  GuildRelationKind kind = GuildRelationKind::kAlly;
  std::string name;
};

struct GuildSkill {
  std::uint16_t id = 0;
  // Le masque de nature du serveur : zéro pour une passive.
  std::int32_t inf = 0;
  int level = 0;
  int sp = 0;
  int range = 0;
  // Le nom TECHNIQUE (« GD_APPROVAL »). Le libellé se cherche ailleurs.
  std::string name;
  // ⚠ C'EST LE SERVEUR QUI TRANCHE : maître ET niveau maximal non atteint.
  bool upgradable = false;
};

struct GuildBan {
  std::int32_t char_id = 0;
  std::string name;
  std::string reason;
};

// CE QUE L'EN-TÊTE DE LA FENÊTRE MONTRE, envoyé à la demande.
struct GuildSummary {
  std::int32_t guild_id = 0;
  std::int32_t level = 0;
  // ⚠ LES CONNECTÉS, pas les membres : le total se compte sur la liste.
  std::int32_t online_count = 0;
  std::int32_t member_most = 0;
  std::int32_t average_level = 0;
  std::int32_t exp = 0;
  std::int32_t next_exp = 0;
  std::int32_t emblem_version = 0;
  std::string name;
  std::string land;
  std::int32_t master_char_id = 0;
  std::string master_name;
};

// L'APPARTENANCE, que le serveur pousse à l'entrée en jeu et à chaque
// changement d'emblème. ⚠ À L'ADHÉSION, elle se déduit de l'en-tête de la
// guilde, sans masque de droits ni drapeau de maître : ceux-là n'arrivent
// qu'à l'entrée en jeu suivante.
struct GuildBelonging {
  std::int32_t guild_id = 0;
  std::int32_t emblem_version = 0;
  std::int32_t rights = 0;
  bool master = false;
  std::string name;
  std::int32_t master_char_id = 0;
};

// CE QUE LE POSTE D'UN MEMBRE LUI PERMET. `known` est faux tant que le
// serveur n'a pas dit les droits de ce poste.
struct GuildRights {
  bool known = false;
  bool may_invite = false;
  bool may_expel = false;
  bool may_storage = false;
};

// Le changement de poste d'un membre, tel que le serveur le rediffuse.
struct GuildPositionMove {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::int32_t position = 0;
};

// CE QUE LE SERVEUR A RÉPONDU À UNE DEMANDE DE CRÉATION.
enum class GuildCreation : std::uint8_t {
  kCreated = 0,
  kAlreadyInGuild = 1,
  kNameTaken = 2,
  kEmperiumMissing = 3,
};

// L'IMAGE D'UN EMBLÈME QUE LE SERVEUR A POUSSÉE PAR LE FIL : un BMP compressé
// en zlib, que ce module ne sait pas ouvrir — il n'en a pas le droit.
struct PushedEmblem {
  std::int32_t version = 0;
  std::vector<std::uint8_t> compressed;
};

// UNE INVITATION REÇUE, en attente de réponse.
struct GuildInvitation {
  std::int32_t guild_id = 0;
  std::string guild_name;
};

// CE QUE L'INVITÉ A RÉPONDU À NOTRE INVITATION, tel que le serveur le numérote.
enum class GuildInviteAnswer : std::uint8_t {
  kAlreadyInGuild = 0,
  kRefused = 1,
  kAccepted = 2,
  kGuildFull = 3,
};

class GuildBook {
 public:
  // ── L'APPARTENANCE ──────────────────────────────────────────────────────
  [[nodiscard]] bool in_guild() const noexcept { return belonging_.has_value(); }
  [[nodiscard]] const std::optional<GuildBelonging>& belonging() const noexcept {
    return belonging_;
  }
  void TellBelonging(GuildBelonging belonging);
  // ⚠ QUITTER EFFACE TOUT, jusqu'aux postes et aux compétences : une autre
  // guilde n'a rien en commun avec celle qu'on quitte. Les versions d'emblème
  // des guildes d'à côté, elles, restent.
  void Leave();

  [[nodiscard]] const std::optional<GuildSummary>& summary() const noexcept { return summary_; }
  void TellSummary(GuildSummary summary);

  // ── L'ANNONCE ───────────────────────────────────────────────────────────
  [[nodiscard]] const std::string& notice_subject() const noexcept { return notice_subject_; }
  [[nodiscard]] const std::string& notice_body() const noexcept { return notice_body_; }
  void TellNotice(std::string subject, std::string body);

  // ── LES MEMBRES ─────────────────────────────────────────────────────────
  [[nodiscard]] std::span<const GuildMember> members() const noexcept { return members_; }
  [[nodiscard]] bool members_known() const noexcept { return members_known_; }
  void ReplaceMembers(std::vector<GuildMember> members);
  [[nodiscard]] const GuildMember* FindMember(std::uint32_t char_id) const;
  // L'apparence voyage avec la connexion : le serveur la redit à chaque fois.
  void TellMemberOnline(std::uint32_t account_id, std::uint32_t char_id, bool online,
                        std::int16_t sex, std::int16_t hair, std::int16_t hair_palette);
  void TellPositionMoves(std::span<const GuildPositionMove> moves);
  void RemoveMember(std::uint32_t char_id);

  // ── OÙ SONT LES MEMBRES, SUR LA CARTE COURANTE ──────────────────────────
  //
  // ⚠ PAR COMPTE, et non par personnage : le serveur nomme le compte. Rien
  // n'est pas zéro : un membre dont on n'a pas reçu la cellule, ou dont le
  // serveur a retiré la position, n'y figure pas. La table se vide à chaque
  // changement de carte.
  [[nodiscard]] const std::map<std::uint32_t, GuildMemberCell>& member_cells() const noexcept {
    return member_cells_;
  }
  [[nodiscard]] const GuildMemberCell* FindMemberCell(std::uint32_t account_id) const;
  // Une cellule vide retire la position de ce compte.
  void TellMemberCell(std::uint32_t account_id, std::optional<GuildMemberCell> cell);
  void ForgetMemberCells() noexcept { member_cells_.clear(); }

  // ── LES POSTES ──────────────────────────────────────────────────────────
  [[nodiscard]] const std::array<GuildPosition, kGuildPositionCount>& positions() const noexcept {
    return positions_;
  }
  void TellPositionName(std::int32_t position, std::string name);
  void TellPositionRights(std::int32_t position, std::int32_t rights, std::int32_t pay_rate);

  // LE LIBELLÉ D'UN POSTE, ou vide.
  //
  // 🔴 LA LISTE DES MEMBRES REVIENT SANS NOM DE POSTE, et rien ne garantit que
  // les noms arrivent après elle. Le dernier nom vu pour chaque poste est donc
  // retenu, et survit même à un rafraîchissement : sans lui, la colonne
  // « Poste » clignoterait à chaque fois.
  [[nodiscard]] const std::string& PositionLabel(std::int32_t position) const;

  // ── CE QUE LE JOUEUR PEUT ───────────────────────────────────────────────
  // LE MAÎTRE, par le premier témoin disponible : l'identifiant que l'en-tête
  // nomme, sinon son nom (casse ignorée) contre celui du membre, sinon le
  // drapeau de l'appartenance.
  [[nodiscard]] bool IsMaster(std::uint32_t char_id) const;
  // 🔴 LE SERVEUR NE REGARDE PAS LE DRAPEAU DE MAÎTRE pour inviter ou expulser,
  // mais le masque de droits du POSTE occupé. Tant que ces droits ne sont pas
  // connus, inviter et expulser retombent sur « maître », le storage sur non.
  [[nodiscard]] GuildRights RightsOf(std::uint32_t char_id) const;
  // QUI PEUT LANCER — ET DONC POSER DANS UNE BARRE — CETTE COMPÉTENCE.
  // 🔴 LE MAÎTRE SEUL, sauf `kMemberGuildSkill` : le serveur refuse le lancer
  // d'un simple membre sans un mot, et un membre ne fait que consulter l'arbre.
  [[nodiscard]] bool MayWield(std::uint16_t skill_id, std::uint32_t char_id) const;

  // ── LES RELATIONS ───────────────────────────────────────────────────────
  [[nodiscard]] std::span<const GuildRelation> relations() const noexcept { return relations_; }
  void ReplaceRelations(std::vector<GuildRelation> relations);
  void RemoveRelation(std::int32_t guild_id, GuildRelationKind kind);

  // ── LES COMPÉTENCES ─────────────────────────────────────────────────────
  //
  // ⚠ LA LISTE EST DÉJÀ FILTRÉE PAR LE SERVEUR : une compétence dont les
  // prérequis manquent n'y est pas. Elle se remplace donc en entier.
  [[nodiscard]] std::span<const GuildSkill> skills() const noexcept { return skills_; }
  [[nodiscard]] bool skills_known() const noexcept { return skills_known_; }
  [[nodiscard]] int skill_points() const noexcept { return skill_points_; }
  void ReplaceSkills(int points, std::vector<GuildSkill> skills);
  [[nodiscard]] const GuildSkill* FindSkill(std::uint16_t skill_id) const;

  // ── LES ONGLETS QUE LE SERVEUR OUVRE ────────────────────────────────────
  //
  // Rien tant qu'il ne l'a pas dit : il ne le pousse de lui-même qu'au maître,
  // à la connexion, et répond à la demande de quiconque.
  [[nodiscard]] const std::optional<std::uint32_t>& menu_flags() const noexcept {
    return menu_flags_;
  }
  void TellMenuFlags(std::uint32_t flags) noexcept { menu_flags_ = flags; }
  // L'onglet est-il ouvert ? Vrai tant que le serveur n'a rien dit : un onglet
  // fermé d'avance cacherait ce que le joueur a peut-être le droit de voir.
  [[nodiscard]] bool MenuOpens(std::uint32_t flag) const noexcept {
    return !menu_flags_.has_value() || (*menu_flags_ & flag) != 0;
  }

  // ── LES EXPULSIONS ──────────────────────────────────────────────────────
  [[nodiscard]] std::span<const GuildBan> bans() const noexcept { return bans_; }
  [[nodiscard]] bool bans_known() const noexcept { return bans_known_; }
  void ReplaceBans(std::vector<GuildBan> bans);

  // ── LA CRÉATION ─────────────────────────────────────────────────────────
  //
  // ⚠ UN NUMÉRO DE RÉPONSE, pour que deux refus identiques de suite se
  // distinguent : c'est la règle des verdicts de banque.
  [[nodiscard]] std::optional<GuildCreation> creation() const noexcept { return creation_; }
  [[nodiscard]] std::uint64_t creation_seq() const noexcept { return creation_seq_; }
  void TellCreation(GuildCreation result);

  // ── LES EMBLÈMES DE TOUTES LES GUILDES ──────────────────────────────────
  //
  // 🔴 UNE VERSION PAR GUILDE, ET C'EST ELLE QUI DIT QU'UN EMBLÈME A CHANGÉ.
  // Elle arrive par plusieurs chemins — l'appartenance, l'en-tête de guilde, la
  // rediffusion aux alentours — et c'est le seul indicateur commun aux deux
  // façons dont un emblème peut arriver. Zéro : pas d'emblème.
  [[nodiscard]] std::int32_t EmblemVersionOf(std::int32_t guild_id) const;
  void TellEmblemVersion(std::int32_t guild_id, std::int32_t version);
  [[nodiscard]] const std::map<std::int32_t, std::int32_t>& emblem_versions() const noexcept {
    return emblem_versions_;
  }

  // L'image que le serveur a poussée par le fil, pour une guilde, ou nul.
  [[nodiscard]] const PushedEmblem* PushedEmblemOf(std::int32_t guild_id) const;
  void TellPushedEmblem(std::int32_t guild_id, PushedEmblem emblem);

  // ── LES INVITATIONS ─────────────────────────────────────────────────────
  //
  // Celle qu'on REÇOIT attend une réponse ; celle qu'on a ENVOYÉE revient avec
  // la réponse de l'invité, numérotée comme les verdicts de création.
  [[nodiscard]] const std::optional<GuildInvitation>& invitation() const noexcept {
    return invitation_;
  }
  void TellInvitation(GuildInvitation invitation) { invitation_ = std::move(invitation); }
  void ForgetInvitation() noexcept { invitation_.reset(); }
  [[nodiscard]] std::optional<GuildInviteAnswer> invite_answer() const noexcept {
    return invite_answer_;
  }
  [[nodiscard]] std::uint64_t invite_answer_seq() const noexcept { return invite_answer_seq_; }
  void TellInviteAnswer(GuildInviteAnswer answer) {
    invite_answer_ = answer;
    ++invite_answer_seq_;
  }

 private:
  std::optional<GuildBelonging> belonging_;
  std::optional<GuildSummary> summary_;
  std::string notice_subject_;
  std::string notice_body_;
  std::vector<GuildMember> members_;
  bool members_known_ = false;
  std::map<std::uint32_t, GuildMemberCell> member_cells_;
  std::array<GuildPosition, kGuildPositionCount> positions_ = {};
  std::array<std::string, kGuildPositionCount> remembered_labels_ = {};
  std::vector<GuildRelation> relations_;
  std::vector<GuildSkill> skills_;
  bool skills_known_ = false;
  int skill_points_ = 0;
  std::vector<GuildBan> bans_;
  bool bans_known_ = false;
  std::optional<std::uint32_t> menu_flags_;
  std::optional<GuildCreation> creation_;
  std::uint64_t creation_seq_ = 0;
  std::map<std::int32_t, std::int32_t> emblem_versions_;
  std::map<std::int32_t, PushedEmblem> pushed_emblems_;
  std::optional<GuildInvitation> invitation_;
  std::optional<GuildInviteAnswer> invite_answer_;
  std::uint64_t invite_answer_seq_ = 0;
};

}  // namespace ro::sim
