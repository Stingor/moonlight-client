#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/sim/inventory.h"

namespace ro::sim {

// LE COURRIER (RODEX) : une boîte qu'on liste, des courriers qu'on lit et dont
// on récupère les pièces jointes, et une écriture — le brouillon — qu'on remplit
// puis qu'on envoie.
//
// 🔴 UN TITRE VIDE DÉTRUIT LES PIÈCES JOINTES. À l'envoi, le serveur retire du
// sac les items et le zeny joints, PUIS refuse en silence un titre vide : rien
// n'est rendu, aucune réponse ne vient. `CheckSend` rend cette émission
// impossible, sur les octets TELS QU'ILS PARTIRONT.
//
// 🔴 LE SERVEUR NE BORNE PAS LES LONGUEURS DE L'ENVOI : il lit le corps à la
// suite du titre sur la longueur annoncée, sans la confronter à la trame. Les
// longueurs émises doivent être exactes, et les bornes de `CheckSend` tenues.
//
// ⚠ LES ITEMS JOINTS RESTENT DANS LE SAC DU SERVEUR jusqu'à l'envoi réussi, qui
// les retire par les paquets ordinaires. Le sac du monde n'y touche donc pas ;
// le sac MONTRÉ retranche ce que l'accusé a joint (`AttachedAmountAt`).
//
// ⚠ PLUSIEURS GESTES RESTENT SANS RÉPONSE : un ajout que le serveur refuse par
// un simple message, un envoi pendant un échange, une récupération d'un id qu'il
// ne connaît plus, une lecture sur une carte sans courrier. Chacun se relâche au
// silence (`ExpireSilent`).
//
// 🔴 APRÈS UN ENVOI, LE BROUILLON N'EST PLUS FIABLE TEL QUEL (`MailDraftStanding`).
// Réussi, le serveur garde l'écriture ouverte — marche, usage d'item et échange
// refusés en silence — jusqu'à l'annulation : elle est DUE. Refusé, le fil ne
// dit pas ce que sont devenues les pièces jointes. Resté sans réponse, le
// courrier a pu partir. Dans ces deux derniers cas, le brouillon oublie ses
// pièces jointes et refuse tout ajout et tout envoi jusqu'à la prochaine
// ouverture accordée : seule celle-ci remet l'état du serveur à zéro.
//
// Les chaînes sont en UTF-8 : `net` les convertit à la réception. Seuls les
// verdicts d'envoi lisent des octets déjà encodés pour le fil, parce que c'est
// sur eux que portent les bornes du serveur.

// ── LES BORNES DU SERVEUR ───────────────────────────────────────────────────

// Les courriers que la boîte tient au plus (`MAIL_MAX_INBOX`).
inline constexpr std::size_t kMailMostListed = 30;
// Les items distincts qu'un courrier porte au plus (`MAIL_MAX_ITEM`).
inline constexpr std::size_t kMailMostItems = 5;
// Les octets utiles d'un titre, d'un corps, d'un nom : le champ du serveur moins
// son zéro final (`MAIL_TITLE_LENGTH`, `MAIL_BODY_LENGTH`, `NAME_LENGTH`).
inline constexpr std::size_t kMailMostTitleBytes = 39;
inline constexpr std::size_t kMailMostBodyBytes = 499;
inline constexpr std::size_t kMailMostNameBytes = 23;
// Ce que la bourse peut tenir (`MAX_ZENY`).
inline constexpr std::int64_t kMailMostZeny = 2147483647;
// La plus grande quantité d'un ajout : la plus grande pile du serveur
// (`MAX_AMOUNT`). Le champ du fil, lui, tient seize bits non signés.
inline constexpr std::int32_t kMailMostAttachAmount = 30000;
// La première case du sac vue du client. ⚠ LA CASE ZÉRO EST LE CHEMIN DU ZENY
// côté serveur : un ajout qui la viserait joindrait de l'argent.
inline constexpr std::uint16_t kMailFirstBagSlot = 2;

// ── LES FRAIS D'ENVOI ───────────────────────────────────────────────────────
//
// Réglages par défaut du serveur (`mail_attachment_price`, `mail_zeny_fee`) :
// chaque pièce jointe coûte un prix fixe, et le zeny envoyé un pourcentage,
// arrondi vers le bas.
inline constexpr std::int64_t kMailAttachmentFeeZeny = 2500;
inline constexpr std::int64_t kMailZenyFeePercent = 2;
inline constexpr std::int64_t kMailPercentBase = 100;

// Les frais d'un envoi : `attachment_count` pièces, `zeny` envoyés.
[[nodiscard]] std::int64_t MailFeeFor(std::int64_t zeny, std::size_t attachment_count) noexcept;
// Ce que l'envoi débite de la bourse : le zeny et ses frais.
[[nodiscard]] std::int64_t MailSendTotal(std::int64_t zeny, std::size_t attachment_count) noexcept;

// ── LES DÉLAIS ──────────────────────────────────────────────────────────────

// Une liste demandée et pas encore venue « charge » pendant ce temps ; au-delà,
// une boîte sans liste est une boîte vide.
inline constexpr std::uint32_t kMailListLoadingMs = 3000;
// Le silence au bout duquel un ajout, un envoi, une récupération, une
// vérification de nom, une lecture sont tenus pour restés sans réponse.
inline constexpr std::uint32_t kMailAttachSilenceMs = 3000;
inline constexpr std::uint32_t kMailSendSilenceMs = 10000;
inline constexpr std::uint32_t kMailClaimSilenceMs = 5000;
inline constexpr std::uint32_t kMailCheckSilenceMs = 5000;
inline constexpr std::uint32_t kMailReadSilenceMs = 5000;

// LES SECONDES RESTANTES D'UN COURRIER QUI N'EXPIRE PAS : le serveur annonce
// alors exactement un an, et non une échéance.
inline constexpr std::uint32_t kMailNeverExpiresSeconds = 365U * 24U * 60U * 60U;

// ── LES CODES DU SERVEUR ────────────────────────────────────────────────────

// Le résultat d'un envoi.
inline constexpr std::uint8_t kMailSendSucceeded = 0;
inline constexpr std::uint8_t kMailSendFailed = 1;
inline constexpr std::uint8_t kMailSendDailyQuota = 2;
inline constexpr std::uint8_t kMailSendItemFailed = 3;
inline constexpr std::uint8_t kMailSendNoRecipient = 4;
inline constexpr std::uint8_t kMailSendBlocked = 5;

// L'accusé d'un ajout de pièce jointe.
inline constexpr std::uint8_t kMailAttachSucceeded = 0;
inline constexpr std::uint8_t kMailAttachOverWeight = 1;
inline constexpr std::uint8_t kMailAttachFailed = 2;
inline constexpr std::uint8_t kMailAttachNoRoom = 3;
inline constexpr std::uint8_t kMailAttachNotTradable = 4;

// Le résultat d'une récupération.
inline constexpr std::uint8_t kMailClaimSucceeded = 0;
inline constexpr std::uint8_t kMailClaimFailed = 1;
inline constexpr std::uint8_t kMailClaimOverWeight = 2;

// Le résultat d'un retour à l'expéditeur.
inline constexpr std::uint32_t kMailReturnSucceeded = 0;

// Le nôtre, jamais sur le fil : le serveur n'a rien répondu.
inline constexpr std::uint8_t kMailSilent = 0xFF;

// ── LES DONNÉES ─────────────────────────────────────────────────────────────

enum class MailBox : std::uint8_t {
  kNormal,
  kAccount,
  kReturned,
};

// UN COURRIER TEL QUE LA LISTE LE DÉCRIT.
struct MailHeader {
  std::int64_t mail_id = 0;
  MailBox box = MailBox::kNormal;
  bool read = false;
  bool has_zeny = false;
  bool has_items = false;
  // Envoyé par un PNJ ou un script, et non par un joueur.
  bool from_npc = false;
  std::string sender;
  // L'expéditeur TEL QUE LE FIL L'A PORTÉ, sans zéro final : c'est lui que
  // « Répondre » renvoie, parce qu'un nom est cherché octet par octet et que
  // l'aller-retour par l'UTF-8 ne le rend pas toujours à l'identique.
  std::string sender_wire;
  std::string title;
  // L'instant ABSOLU de l'expiration, en secondes Unix. Le serveur envoie des
  // secondes RESTANTES ; l'instant se fixe à la réception.
  std::int64_t expires_at_unix_s = 0;
  // Le serveur a annoncé `kMailNeverExpiresSeconds` : aucune échéance.
  bool never_expires = false;

  [[nodiscard]] bool has_attachments() const noexcept { return has_zeny || has_items; }
};

// Les secondes qui restent avant l'expiration, à `now_unix_s` ; zéro au-delà.
[[nodiscard]] std::int64_t MailSecondsLeft(const MailHeader& header,
                                           std::int64_t now_unix_s) noexcept;

// CE QU'UNE LECTURE A RÉVÉLÉ. N'existe que pour un courrier lu pendant la
// session.
struct MailContent {
  std::int64_t mail_id = 0;
  MailBox box = MailBox::kNormal;
  std::string body;
  std::int64_t zeny = 0;
  // `client_slot` n'y désigne rien : ces items ne sont pas dans le sac.
  std::vector<InventoryItem> items;
};

enum class MailClaim : std::uint8_t {
  kZeny,
  kItems,
};

// UNE PIÈCE JOINTE AU BROUILLON : l'item tel que l'accusé le décrit, `amount`
// étant la quantité jointe et non la pile.
struct MailAttachment {
  InventoryItem item;
};

// Un ajout parti et pas encore accusé.
struct MailPendingAttach {
  InventoryItem item;
  std::int16_t amount = 0;
  std::uint32_t sent_ms = 0;
};

enum class MailRecipientState : std::uint8_t {
  kUnchecked,
  kPending,
  // Le serveur a répondu « aucun personnage de ce nom ».
  kNotFound,
  kFound,
  // Pas de réponse : carte sans courrier, serveur de personnages injoignable.
  // ⚠ CE N'EST PAS « INEXISTANT ».
  kSilent,
};

// LA VÉRIFICATION D'UN NOM vaut pour le nom INTERROGÉ, et pour lui seul : une
// saisie dont les octets du fil diffèrent de `queried_wire` n'est pas vérifiée.
struct MailRecipient {
  MailRecipientState state = MailRecipientState::kUnchecked;
  // Les octets du fil interrogés, sans zéro final.
  std::string queried_wire;
  std::uint32_t char_id = 0;
  std::uint16_t job = 0;
  std::uint16_t base_level = 0;
  // Le nom tel que le serveur le réémet.
  std::string name;
};

// CE QUE VAUT LE BROUILLON depuis le dernier envoi.
enum class MailDraftStanding : std::uint8_t {
  // Aucun envoi n'a eu de suite depuis l'ouverture.
  kUsable,
  // L'envoi a réussi : le brouillon est fermé, mais le serveur tient
  // l'écriture ouverte tant que l'annulation ne part pas.
  kCancelDue,
  // L'envoi a été refusé : l'état des pièces jointes n'est pas dit.
  kReopenDue,
  // L'envoi est resté sans réponse : le courrier a pu partir.
  kOutcomeUnknown,
};

// CE QUE LE SERVEUR ORDONNE À LA BOÎTE, de lui-même — la commande `@mail`, un
// script de PNJ. L'ouverture est celle de l'icône du menu : la liste est
// demandée.
enum class MailWindowOrder : std::uint8_t {
  kOpen,
  kClose,
};

// CE QUE LA FENÊTRE ET LA CONVERSATION DOIVENT DIRE, une fois chacun.
enum class MailNewsKind : std::uint8_t {
  // `code` : le résultat de l'envoi, `kMailSilent` s'il n'est pas venu.
  kSendResult,
  // `code` : le résultat d'un envoi déjà tenu pour resté sans réponse. Il ne
  // touche plus au brouillon ; il dit seulement si le courrier est parti.
  kSendResultLate,
  // `code` : l'accusé, `kMailSilent` s'il n'est pas venu ; `item` : l'ajout.
  kAttachRefused,
  // L'écriture a été refusée : coffre ouvert, échange en cours.
  kWriteRefused,
  // `claim`, `mail_id` ; `code` : le résultat.
  kClaimed,
  kClaimRefused,
  kDeleted,
  kReturned,
  kReturnRefused,
};

struct MailNews {
  std::uint32_t seq = 0;
  MailNewsKind kind = MailNewsKind::kSendResult;
  std::uint8_t code = 0;
  std::int64_t mail_id = 0;
  MailClaim claim = MailClaim::kItems;
  InventoryItem item;
};

// Les nouvelles gardées : plus qu'une session n'en produit entre deux images.
inline constexpr std::size_t kMailNewsKept = 32;

// ── LES VERDICTS ────────────────────────────────────────────────────────────

// POURQUOI UN AJOUT NE PARTIRAIT PAS.
enum class MailAttachFault : std::uint8_t {
  kNone,
  kNotWriting,
  kSendInFlight,
  // Le brouillon est à rouvrir (`MailDraftStanding`).
  kReopenDue,
  // Un ajout attend son accusé : un seul à la fois.
  kAttachInFlight,
  // Case du zeny, ou aucune case du sac.
  kBadSlot,
  // Le serveur refuse ce qui est porté.
  kEquipped,
  kRented,
  // Toute la pile est déjà jointe ou en route.
  kAllAttached,
  // Quantité nulle, négative, au-delà de ce qui reste non joint ou du plafond.
  kBadAmount,
  // Cinq items distincts sont déjà joints ou en route.
  kAttachmentsFull,
};

// POURQUOI UN ENVOI NE PARTIRAIT PAS.
enum class MailSendFault : std::uint8_t {
  kNone,
  kNotWriting,
  kSendInFlight,
  // Le brouillon est à rouvrir (`MailDraftStanding`) : le courrier précédent a
  // pu partir, ou ses pièces jointes ne sont plus sues.
  kReopenDue,
  // Un ajout n'est pas accusé : ce qui partirait n'est pas ce qui est montré.
  kAttachInFlight,
  kRecipientEmpty,
  kRecipientTooLong,
  // 🔴 Le serveur détruirait les pièces jointes.
  kTitleEmpty,
  kTitleTooLong,
  kBodyTooLong,
  // Un octet nul tronquerait le champ chez le serveur — un titre réduit à rien.
  kEmbeddedNul,
  kZenyNegative,
  kZenyAboveMost,
  // La bourse n'est pas connue : rien ne garantit les frais.
  kPurseUnknown,
  // Le zeny et ses frais dépassent la bourse : échec, et quota quand même
  // décompté.
  kPurseShort,
};

class Mailbox {
 public:
  // ── LA BOÎTE ─────────────────────────────────────────────────────────────

  [[nodiscard]] bool open() const noexcept { return open_; }
  // La boîte s'ouvre, et sa liste est demandée.
  void NoteOpened(std::uint32_t now_ms);
  void Close() noexcept { open_ = false; }

  // L'ordre du serveur sur la boîte, gardé jusqu'à ce que la boucle le prenne.
  // Un ordre plus récent remplace celui qui n'a pas encore été pris.
  void TellWindowOrder(MailWindowOrder order) noexcept { window_order_ = order; }
  [[nodiscard]] std::optional<MailWindowOrder> TakeWindowOrder() noexcept {
    return std::exchange(window_order_, std::nullopt);
  }

  // Une liste est demandée — ouverture ou rafraîchissement.
  void NoteListRequested(std::uint32_t now_ms) noexcept;
  // La liste attendue n'est pas venue, depuis moins de `kMailListLoadingMs`.
  [[nodiscard]] bool ListLoading(std::uint32_t now_ms) const noexcept;
  // Une liste est arrivée depuis la session.
  [[nodiscard]] bool list_received() const noexcept { return list_received_; }

  // LA LISTE REMPLACE LA PRÉCÉDENTE : le serveur envoie toute la boîte. Le
  // contenu déjà lu des courriers encore présents est gardé ; au-delà de
  // `kMailMostListed`, les entrées sont ignorées.
  void TellList(std::vector<MailHeader> headers);

  // Du plus récent au plus ancien.
  [[nodiscard]] std::span<const MailHeader> headers() const noexcept { return headers_; }
  [[nodiscard]] const MailHeader* FindHeader(std::int64_t mail_id) const noexcept;
  [[nodiscard]] std::size_t UnreadCount() const noexcept;
  // Le plus récent identifiant de cette boîte, zéro si elle est vide.
  [[nodiscard]] std::int64_t NewestIdIn(MailBox box) const noexcept;

  // La pastille « non lu » que le serveur pose sur l'icône du menu.
  void TellUnreadBadge(bool unread) noexcept { unread_badge_ = unread; }
  [[nodiscard]] bool unread_badge() const noexcept { return unread_badge_; }

  // ── LE CONTENU ───────────────────────────────────────────────────────────

  // Une lecture demandée. ⚠ ELLE PEUT RESTER SANS RÉPONSE — carte sans
  // courrier, identifiant que le serveur ne connaît plus — : au-delà de
  // `kMailReadSilenceMs`, elle est tenue pour perdue, et peut se redemander.
  void NoteReadSent(std::int64_t mail_id, std::uint32_t now_ms);
  [[nodiscard]] bool ReadInFlight(std::int64_t mail_id) const noexcept;
  // La dernière lecture de ce courrier n'a pas eu de réponse.
  [[nodiscard]] bool ReadUnanswered(std::int64_t mail_id) const noexcept;

  // Une lecture : le contenu est gardé, l'en-tête marqué lu.
  void TellContent(MailContent content);
  [[nodiscard]] const MailContent* FindContent(std::int64_t mail_id) const noexcept;

  // ── LES RÉCUPÉRATIONS ────────────────────────────────────────────────────
  //
  // Plusieurs peuvent être en vol à la fois — toute une boîte — : le serveur
  // cumule ce qu'il réserve. Chacune se reconnaît à son courrier et à sa sorte.

  void NoteClaimSent(std::int64_t mail_id, MailClaim claim, std::uint32_t now_ms);
  [[nodiscard]] bool ClaimInFlight(std::int64_t mail_id, MailClaim claim) const noexcept;
  // Au succès, le contenu perd ce qui est récupéré et l'en-tête le bit qui le
  // signalait.
  void TellClaimResult(std::int64_t mail_id, MailClaim claim, std::uint8_t code);
  // Les courriers de cette boîte qui portent encore une pièce jointe dont la
  // récupération n'est pas déjà en vol, du plus récent au plus ancien.
  [[nodiscard]] std::vector<std::int64_t> ClaimableIn(MailBox box) const;

  // ── SUPPRESSION ET RETOUR ────────────────────────────────────────────────

  // N'arrive que sur succès : en-tête et contenu disparaissent.
  void TellDeleted(std::int64_t mail_id);
  // Plusieurs retours peuvent attendre leur réponse, comme les récupérations.
  void NoteReturnSent(std::int64_t mail_id);
  [[nodiscard]] bool ReturnInFlight(std::int64_t mail_id) const noexcept;
  // ⚠ LE SERVEUR NE RENVOIE QUE LES 32 BITS BAS DE L'IDENTIFIANT, et répond
  // aussi par un échec à la lecture d'un courrier qu'il ne connaît pas : un
  // échec ne se dit que si un retour était demandé ; sinon il clôt la lecture
  // en attente de ce courrier.
  void TellReturnResult(std::uint32_t mail_id_low, std::uint32_t result);

  // ── L'ÉCRITURE ───────────────────────────────────────────────────────────

  [[nodiscard]] bool writing() const noexcept { return writing_; }
  // Le rang de l'ouverture : une neuve remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t write_seq() const noexcept { return write_seq_; }
  // Le destinataire que le serveur a renvoyé avec son accord, en UTF-8 et tel
  // que le fil l'a porté.
  [[nodiscard]] const std::string& write_recipient() const noexcept { return write_recipient_; }
  [[nodiscard]] const std::string& write_recipient_wire() const noexcept {
    return write_recipient_wire_;
  }

  // L'accord ou le refus du serveur. L'accord ouvre un brouillon vierge, et
  // remet le brouillon à `kUsable`.
  void TellWriteOpened(bool granted, std::string recipient, std::string recipient_wire = {});
  // L'annulation part : le brouillon ferme, et l'annulation n'est plus due.
  void NoteWriteCancelled();
  // Un changement de carte : le serveur a remis l'écriture à zéro en silence.
  void CloseWriteForMapChange();

  [[nodiscard]] MailDraftStanding draft_standing() const noexcept { return standing_; }
  // 🔴 L'ANNULATION EST DUE : le serveur tient l'écriture ouverte, et retient
  // le joueur, tant qu'elle ne part pas.
  [[nodiscard]] bool cancel_due() const noexcept {
    return standing_ == MailDraftStanding::kCancelDue;
  }
  // Le brouillon refuse ajouts et envois jusqu'à la prochaine ouverture
  // accordée.
  [[nodiscard]] bool reopen_due() const noexcept {
    return standing_ == MailDraftStanding::kReopenDue ||
           standing_ == MailDraftStanding::kOutcomeUnknown;
  }

  // Combien de cette case est joint, accusé.
  [[nodiscard]] std::int32_t AttachedAmountAt(std::uint16_t client_slot) const noexcept;
  // Combien de cette case est en route.
  [[nodiscard]] std::int32_t InFlightAt(std::uint16_t client_slot) const noexcept;
  // Les pièces jointes distinctes, accusées : c'est ce que les frais comptent.
  [[nodiscard]] std::size_t attachment_count() const noexcept { return attachments_.size(); }
  [[nodiscard]] std::span<const MailAttachment> attachments() const noexcept {
    return attachments_;
  }
  [[nodiscard]] std::span<const MailPendingAttach> pending_attaches() const noexcept {
    return pending_attaches_;
  }
  // Le poids total joint, tel que le serveur le dit.
  [[nodiscard]] std::int32_t attached_weight() const noexcept { return attached_weight_; }
  // Ce que le sac MONTRÉ retranche de cette case : joint et en route. Zéro
  // hors écriture.
  [[nodiscard]] std::int32_t WithheldAt(std::uint16_t client_slot) const noexcept;

  // `bag_item` : la case du sac du monde, NON amputée de ce qui est joint.
  //
  // 🔴 UN SEUL AJOUT EN ROUTE À LA FOIS. Un échec ne porte pas la case, et un
  // refus par simple message ne répond pas du tout : avec deux ajouts en
  // route, un échec s'attribuerait au mauvais item. Le suivant attend donc
  // l'accusé, ou le silence, du précédent.
  [[nodiscard]] MailAttachFault CheckAttach(const InventoryItem& bag_item,
                                            std::int32_t amount) const noexcept;
  void NoteAttachSent(const InventoryItem& bag_item, std::int16_t amount, std::uint32_t now_ms);
  // L'accusé d'un ajout. `count` est la quantité AJOUTÉE par ce geste : le
  // serveur cumule sur une case déjà jointe. Un échec répond à l'ajout en
  // route.
  void TellAttachResult(std::uint8_t code, const InventoryItem& item, std::int16_t count,
                        std::int32_t weight);
  // Un retrait est-il permis ? Au plus ce qui est joint, brouillon utilisable,
  // aucun envoi en vol.
  [[nodiscard]] bool CanDetach(std::uint16_t client_slot, std::int32_t amount) const noexcept;
  // Un retrait accusé. Arrive aussi, une fois par pièce, quand un envoi échoue
  // après validation : le serveur les détache toutes. Ne touche qu'une pièce
  // jointe connue d'une écriture ouverte.
  void TellDetached(std::uint16_t client_slot, std::int32_t amount, std::int32_t weight);

  [[nodiscard]] bool send_in_flight() const noexcept { return send_in_flight_; }
  // `recipient`, `title`, `body` : les octets TELS QU'ILS PARTIRONT, sans zéro
  // final. `purse` : la bourse, si le serveur l'a dite.
  [[nodiscard]] MailSendFault CheckSend(std::string_view recipient, std::string_view title,
                                        std::string_view body, std::int64_t zeny,
                                        std::optional<std::int64_t> purse) const noexcept;
  void NoteSendSent(std::uint32_t now_ms) noexcept;
  // Le succès ferme le brouillon et rend l'annulation due ; un échec le laisse
  // ouvert mais à rouvrir, sans pièces jointes. Un résultat qui suit un silence
  // ne se dit que comme `kSendResultLate` ; sans envoi attendu, il est ignoré.
  void TellSendResult(std::uint8_t code);

  [[nodiscard]] const MailRecipient& recipient() const noexcept { return recipient_; }
  // `queried_wire` : le nom interrogé, en octets du fil.
  void NoteRecipientCheckSent(std::uint32_t now_ms, std::string queried_wire);
  // `char_id` nul : aucun personnage de ce nom. Ignorée hors vérification en
  // attente : une réponse tardive ne vise plus la saisie courante.
  void TellRecipient(std::uint32_t char_id, std::uint16_t job, std::uint16_t base_level,
                     std::string name);

  // ── LES SILENCES ET LES NOUVELLES ────────────────────────────────────────

  // Relâche ce qui attend depuis trop longtemps une réponse qui ne viendra pas.
  void ExpireSilent(std::uint32_t now_ms);

  [[nodiscard]] std::span<const MailNews> news() const noexcept { return news_; }
  [[nodiscard]] std::uint32_t last_news_seq() const noexcept { return news_seq_; }

 private:
  struct PendingClaim {
    std::int64_t mail_id = 0;
    MailClaim claim = MailClaim::kItems;
    std::uint32_t sent_ms = 0;
  };

  struct PendingRead {
    std::int64_t mail_id = 0;
    std::uint32_t sent_ms = 0;
  };

  void ResetWrite();
  void ForgetAttachments() noexcept;
  void MarkReadUnanswered(std::int64_t mail_id);
  void Tell(MailNewsKind kind, std::uint8_t code = 0, std::int64_t mail_id = 0,
            MailClaim claim = MailClaim::kItems, const InventoryItem* item = nullptr);
  void Forget(std::int64_t mail_id);
  [[nodiscard]] MailHeader* FindHeaderMutable(std::int64_t mail_id) noexcept;
  [[nodiscard]] MailContent* FindContentMutable(std::int64_t mail_id) noexcept;
  [[nodiscard]] std::size_t DistinctEngaged() const noexcept;
  [[nodiscard]] bool Engages(std::uint16_t client_slot) const noexcept;

  bool open_ = false;
  std::optional<MailWindowOrder> window_order_;
  bool list_requested_ = false;
  bool list_received_ = false;
  std::uint32_t list_requested_ms_ = 0;
  bool unread_badge_ = false;
  std::vector<MailHeader> headers_;
  std::vector<MailContent> contents_;
  std::vector<PendingClaim> pending_claims_;
  std::vector<std::int64_t> pending_returns_;
  std::vector<PendingRead> pending_reads_;
  std::vector<std::int64_t> unanswered_reads_;

  bool writing_ = false;
  std::uint32_t write_seq_ = 0;
  std::string write_recipient_;
  std::string write_recipient_wire_;
  MailDraftStanding standing_ = MailDraftStanding::kUsable;
  // Un envoi a été tenu pour resté sans réponse, et son résultat peut encore
  // venir.
  bool send_silenced_ = false;
  std::vector<MailAttachment> attachments_;
  std::vector<MailPendingAttach> pending_attaches_;
  std::int32_t attached_weight_ = 0;
  bool send_in_flight_ = false;
  std::uint32_t send_sent_ms_ = 0;
  MailRecipient recipient_;
  std::uint32_t recipient_sent_ms_ = 0;

  std::vector<MailNews> news_;
  std::uint32_t news_seq_ = 0;
};

}  // namespace ro::sim
