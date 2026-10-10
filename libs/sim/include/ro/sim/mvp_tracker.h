#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ro::sim {

// LE CARNET DE CHASSE MVP : ce que le serveur dit des créneaux de MVP, et ce
// que le GROUPE de chasse en a observé.
//
// 🔴 LA RÈGLE DE NON-TRICHE EST AU SERVEUR : un créneau que le groupe n'a pas
// observé n'a pas d'observation, et rien ne s'en déduit. Ce livre ne fait que
// garder ce qui arrive et en tirer les règles d'échéance.
//
// ⚠ TOUT EST EN SECONDES UNIX DU SERVEUR. Ce livre n'a pas d'horloge : l'heure
// locale lui est passée, et il garde l'écart que chaque paquet d'état révèle.

// « Aucun créneau » : le serveur numérote ses créneaux par leur rang.
inline constexpr std::uint16_t kMvpNoSlot = 0xFFFF;
// « Position de tombe inconnue » : la cellule (0, 0) est une cellule valable.
inline constexpr std::int16_t kMvpNoTomb = -1;

// La nature d'un créneau. Valeurs du fil, jamais renumérotées ; une valeur
// inconnue se garde telle quelle, en brut.
enum class MvpSlotKind : std::uint8_t {
  kBossSpawn = 0,
  kScriptTimer = 1,
  kScriptInvasion = 2,
  kSummonLock = 3,
};

// D'OÙ VIENT UNE OBSERVATION. ⚠ L'ORDRE EST L'ORDRE DE PRÉCISION : c'est par
// lui que le serveur arbitre entre deux observations d'un même créneau.
enum class MvpSource : std::uint8_t { kManual = 0, kTomb = 1, kKill = 2, kMirror = 3 };

// Le résultat d'une commande de groupe. Valeurs du fil.
enum class MvpGroupResult : std::uint8_t {
  kOk = 0,
  kNoAccount,
  kAlreadyMember,
  kNotMember,
  kNotOwner,
  kNoSuchUser,
  kSelf,
  kFull,
  kBadName,
  kNoInvite,
  kTargetInGroup,
  kSql,
  kTargetSameGroup,
  kNotInvitable,
  // LES CODES QUI SUIVENT NE VIENNENT QUE D'UN SERVEUR QUI SAIT NOS QUEUES
  // (`kUiCapMvpTrackerExt`) : sans elles, il garde l'ancien code.
  // Une saisie sans effet : le groupe tient déjà une heure au moins aussi
  // précise. ⚠ CE N'EST PAS UN REFUS.
  kObservationKept,
  kUnknownSlot,
  kTargetNotMember,
  kNameTooLong,
};

// D'OÙ VIENT UNE INVITATION, quand la trame le dit. Valeurs du fil ; une valeur
// inconnue se lit `kUnknown`.
enum class MvpInviteOrigin : std::uint8_t {
  kUnknown = 0,
  // La réponse à un instantané : la même invitation, renvoyée.
  kSnapshot,
  // Rendue par la poignée de main : la même, renvoyée.
  kHandshake,
  // Poussée à l'instant où son groupe l'émet : un geste nouveau.
  kPushed,
};

// À QUELLE COMMANDE RÉPOND UN RÉSULTAT, quand la trame le dit : l'identifiant
// que la commande portait, et l'écho de son numéro.
struct MvpResultTag {
  std::uint32_t request_id = 0;
  std::uint8_t command = 0;
};

// UN CRÉNEAU DU CATALOGUE : où et à quel rythme un MVP revient.
struct MvpSlot {
  std::uint16_t slot_id = kMvpNoSlot;
  // Zéro : créneau scripté, que sa carte identifie.
  std::uint16_t mob_id = 0;
  // `MvpSlotKind`, valeur brute.
  std::uint8_t kind = 0;
  // Le retour au plus tôt après la mort, et l'amplitude du tirage.
  std::uint32_t delay1_ms = 0;
  std::uint32_t delay2_ms = 0;
  // La taille de la carte en cellules, celle de son .gat.
  std::uint16_t map_cells_x = 0;
  std::uint16_t map_cells_y = 0;
  // Le nom interne de la carte, ASCII, 15 octets au plus.
  std::string map;
  // Le nom du MVP en UTF-8 ; vide pour un créneau scripté.
  std::string name;
};

// CE QUE LE GROUPE A VU D'UN CRÉNEAU.
struct MvpObservation {
  // ⚠ LA VALEUR BRUTE EST GARDÉE dans `source_raw` : une source inconnue vaut
  // ici `kManual`, mais l'interface la montre « ? ».
  MvpSource source = MvpSource::kManual;
  std::uint8_t source_raw = 0;
  // Le monstre réellement tombé.
  std::uint16_t mob_id = 0;
  // Zéro : inconnue.
  std::int64_t kill_time_s = 0;
  // Zéro : l'instant n'a pas été mérité, une fenêtre se déduit du délai.
  std::int64_t exact_respawn_s = 0;
  std::int16_t tomb_x = kMvpNoTomb;
  std::int16_t tomb_y = kMvpNoTomb;
  std::uint32_t by_user_id = 0;
  std::int64_t reported_at_s = 0;
  // En UTF-8 ; vide pour une saisie au clavier.
  std::string by_name;
};

// UN MEMBRE DU GROUPE DE CHASSE : une ligne par personnage, et un compte peut
// en avoir plusieurs, à la suite.
struct MvpMember {
  std::uint32_t user_id = 0;
  std::int16_t level = 0;
  bool online = false;
  // En UTF-8 ; vide : compte sans personnage.
  std::string name;
};

struct MvpGroup {
  // Zéro : dans aucun groupe.
  std::uint32_t group_id = 0;
  std::uint32_t owner_user_id = 0;
  // En UTF-8.
  std::string name;
  std::vector<MvpMember> members;
  // LE COMPTE DU DESTINATAIRE, quand la trame le dit : nos lignes sont celles
  // de ce compte, et nous possédons le groupe s'il en est le propriétaire.
  // Zéro : compte de jeu non rattaché. Vide : le serveur ne le dit pas.
  std::optional<std::uint32_t> self_user_id;
};

// LA FENÊTRE DE RETOUR d'un créneau, en secondes UNIX du serveur.
struct MvpWindow {
  std::int64_t from_s = 0;
  std::int64_t to_s = 0;
  // Un instant mérité au Convex Mirror : `from_s == to_s`.
  bool exact = false;
};

// Une tombe à montrer sur la minimap de la carte courante.
struct MvpTomb {
  int cell_x = 0;
  int cell_y = 0;
  bool exact = false;
  // Le créneau de la tombe. En dernier : les initialisations désignées s'en
  // passent.
  std::uint16_t slot_id = kMvpNoSlot;
};

class MvpTrackerBook {
 public:
  // ── CE QUE LE RÉSEAU POSE ──

  // Recale l'horloge sur celle du serveur. Ignoré si `server_time_s <= 0`. À
  // CHAQUE paquet d'état, kind inconnu compris.
  void NoteServerTime(std::int64_t server_time_s, std::int64_t local_now_s) noexcept;
  // Remplace le catalogue : `catalog_known()` devient vrai, `catalog_serial()`
  // grandit.
  void ReceiveCatalogue(std::vector<MvpSlot> slots);
  // Un instantané (`replace_all`) vide d'abord ; un delta ajoute ou remplace,
  // SANS condition — l'arbitrage est au serveur.
  void ReceiveObservations(std::vector<std::pair<std::uint16_t, MvpObservation>> entries,
                           bool replace_all);
  void ReceiveFavorites(std::vector<std::uint16_t> slot_ids);
  // Remplace le groupe. `members_complete` faux : la queue était tronquée, le
  // groupe reste sans membres et les observations NE sont PAS vidées. Complet
  // et `group_id == 0` : les observations sont vidées.
  void ReceiveGroup(MvpGroup group, bool members_complete);
  // L'invitation en attente, sans test sur `group_id` : zéro l'efface de fait.
  // `origin` vide : la trame ne dit pas d'où elle vient.
  void ReceiveInvitation(std::uint32_t group_id, std::string group_name,
                         std::optional<MvpInviteOrigin> origin = std::nullopt);
  // `tag` vide : la trame ne dit pas à quelle commande elle répond.
  void ReceiveResult(std::uint8_t result_code, std::uint32_t now_ms,
                     std::optional<MvpResultTag> tag = std::nullopt) noexcept;
  void ClearInvitation() noexcept;
  // Tout, sauf le dernier résultat, son étiquette, son heure et le compte des
  // résultats.
  void Clear() noexcept;

  // ── LECTURE ──

  // L'heure du serveur ; l'heure locale tant qu'aucun paquet n'est arrivé.
  [[nodiscard]] std::int64_t ServerNow(std::int64_t local_now_s) const noexcept;
  [[nodiscard]] bool clock_known() const noexcept { return clock_known_; }
  [[nodiscard]] bool catalog_known() const noexcept { return catalog_known_; }
  [[nodiscard]] std::uint32_t catalog_serial() const noexcept { return catalog_serial_; }
  [[nodiscard]] std::span<const MvpSlot> slots() const noexcept { return slots_; }
  [[nodiscard]] const MvpSlot* FindSlot(std::uint16_t slot_id) const noexcept;
  // Le créneau de (`mob_id`, carte), à défaut le créneau scripté (0, carte) ;
  // la carte se compare SANS la casse. Carte vide : nul.
  [[nodiscard]] const MvpSlot* FindSlotFor(std::uint16_t mob_id,
                                           std::string_view map) const noexcept;
  [[nodiscard]] const MvpObservation* FindObservation(std::uint16_t slot_id) const noexcept;
  // ⚠ ORDONNÉE : le dessin qui la parcourt est déterministe.
  [[nodiscard]] const std::map<std::uint16_t, MvpObservation>& observations() const noexcept {
    return observations_;
  }
  [[nodiscard]] std::span<const std::uint16_t> favorites() const noexcept { return favorites_; }
  [[nodiscard]] bool IsFavorite(std::uint16_t slot_id) const noexcept;
  [[nodiscard]] const MvpGroup& group() const noexcept { return group_; }
  [[nodiscard]] std::uint32_t pending_invite() const noexcept { return invite_group_id_; }
  [[nodiscard]] std::string_view pending_invite_name() const noexcept { return invite_name_; }
  // L'origine de la dernière trame d'invitation ; `kUnknown` si elle ne la
  // disait pas.
  [[nodiscard]] MvpInviteOrigin invite_origin() const noexcept { return invite_origin_; }
  [[nodiscard]] std::uint8_t last_result() const noexcept { return last_result_; }
  // L'étiquette du dernier résultat ; vide s'il n'en portait pas.
  [[nodiscard]] const std::optional<MvpResultTag>& last_result_tag() const noexcept {
    return last_result_tag_;
  }
  // 🔴 LE SERVEUR PARLE LA FORME ÉTENDUE À CETTE SESSION : une queue de groupe
  // ou d'invitation a été lue depuis `Clear()`. Il pousse alors aussi la
  // présence des membres. L'étiquette d'un résultat n'en est pas la preuve.
  [[nodiscard]] bool extensions_seen() const noexcept { return extensions_seen_; }
  [[nodiscard]] std::uint32_t last_result_ms() const noexcept { return last_result_ms_; }
  // Grandit à chaque résultat reçu, même identique : c'est ce qui les compte.
  [[nodiscard]] std::uint32_t result_serial() const noexcept { return result_serial_; }
  // LA SEULE RÈGLE D'ÉCHÉANCE. Sans observation : rien. Un instant exact : la
  // fenêtre de cet instant, même sans créneau au catalogue. Sinon, sans
  // créneau ou sans heure de mort : rien ; avec, la mort plus les délais, en
  // secondes entières.
  [[nodiscard]] std::optional<MvpWindow> WindowOf(std::uint16_t slot_id) const noexcept;

 private:
  std::vector<MvpSlot> slots_;
  std::map<std::uint16_t, MvpObservation> observations_;
  std::vector<std::uint16_t> favorites_;
  MvpGroup group_;
  bool catalog_known_ = false;
  std::uint32_t catalog_serial_ = 0;
  bool clock_known_ = false;
  std::int64_t clock_offset_s_ = 0;
  std::uint32_t invite_group_id_ = 0;
  std::string invite_name_;
  MvpInviteOrigin invite_origin_ = MvpInviteOrigin::kUnknown;
  bool extensions_seen_ = false;
  std::optional<MvpResultTag> last_result_tag_;
  std::uint8_t last_result_ = 0;
  std::uint32_t last_result_ms_ = 0;
  std::uint32_t result_serial_ = 0;
};

// ── RÈGLES DÉRIVÉES, pures ──

// LES TOMBES DE CETTE CARTE (nom comparé sans casse) : une tombe connue, un
// créneau au catalogue, une fenêtre connue, et pas plus vieille qu'une heure
// après sa fin — instant exact compris.
inline constexpr std::int64_t kMvpTombLifetimeS = 3600;
[[nodiscard]] std::vector<MvpTomb> MvpTombsOn(const MvpTrackerBook& book, std::string_view map,
                                              std::int64_t server_now_s);

// L'ICÔNE DU MENU S'ALLUME quand un FAVORI est dans sa fenêtre ou à moins de
// `lead_min` minutes d'y entrer, et que cette fenêtre est encore fraîche :
// l'instant exact plus un quart d'heure, ou la fin de la fenêtre plus une
// demi-heure.
inline constexpr std::int64_t kMvpBadgeExactFreshS = 900;
inline constexpr std::int64_t kMvpBadgeWindowFreshS = 1800;
[[nodiscard]] bool MvpBadgeLit(const MvpTrackerBook& book, std::int64_t server_now_s, int lead_min);

}  // namespace ro::sim
