#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── LE COURRIER (RODEX) ─────────────────────────────────────────────────────
//
// Deux fenêtres : la BOÎTE — onglets par
// boîte, filtre, liste, bloc de lecture qui agrandit la fenêtre — et
// l'ÉCRITURE — destinataire vérifié, sujet, message, zeny, pièces jointes
// lâchées depuis le sac.
//
// 🔴 LES FENÊTRES NE POSSÈDENT AUCUN COURRIER : la boîte, le contenu lu, les
// pièces jointes, la vérification du nom vivent dans `app`. Les fenêtres
// DEMANDENT — lire, récupérer, supprimer, joindre, envoyer — et la vue
// suivante montre ce que le serveur a accusé.
//
// 🔴 L'EXPÉDITEUR, LE SUJET ET LE CORPS VIENNENT D'AUTRES JOUEURS : ils
// s'affichent en texte BRUT, jamais comme gabarit ni comme libellé de widget.
//
// Les fenêtres portent aussi : un
// récapitulatif avant tout envoi qui emporte quelque chose, « Envoyer » grisé
// quand la bourse ne couvre pas le total, la vérification du nom à la sortie
// du champ, les compteurs d'octets du sujet et du message, l'insigne
// « Système », « Récupérer tout » sur l'onglet, et l'expiration proche en
// rouge.

inline constexpr const char* kMailboxWindowId = "mailbox";
inline constexpr const char* kMailComposeWindowId = "mail_compose";

// Les trois boîtes du serveur, dans l'ordre de leur numéro sur le fil.
enum class MailBox : std::uint8_t { kNormal = 0, kAccount = 1, kReturned = 2 };

// Les onglets : une par boîte, puis la vue de toutes.
enum class MailboxTab : std::uint8_t { kNormal = 0, kAccount = 1, kReturned = 2, kAll = 3 };
inline constexpr std::size_t kMailboxTabCount = 4;

// LE MASQUE DE CE QU'UN COURRIER PORTE ENCORE. Le serveur le remet à jour à
// chaque récupération : c'est « reste à récupérer », pas « contenait ».
inline constexpr std::uint8_t kMailCarriesZeny = 2;
inline constexpr std::uint8_t kMailCarriesItems = 4;
// Un courrier du serveur ou d'un PNJ, et non d'un joueur.
inline constexpr std::uint8_t kMailFromSystem = 8;

// Les pièces jointes qu'une écriture peut porter.
inline constexpr int kMailMaxAttachments = 5;
// Les bornes du fil, en octets ENCODÉS et sans le zéro final.
inline constexpr std::size_t kMailTitleMaxBytes = 39;
inline constexpr std::size_t kMailBodyMaxBytes = 499;

// L'expiration qui s'écrit en rouge, quand il reste des pièces jointes.
inline constexpr std::int64_t kMailExpirySoonSeconds = std::int64_t{3} * 24 * 60 * 60;

// UN COURRIER DÉSIGNÉ : son numéro ET sa boîte — le même numéro peut revenir
// dans une autre boîte.
struct MailRef {
  std::int64_t mail_id = 0;
  MailBox box = MailBox::kNormal;
  [[nodiscard]] bool operator==(const MailRef&) const = default;
};

// ── LA BOÎTE ────────────────────────────────────────────────────────────────

// UNE LIGNE DE LA LISTE.
struct MailRowView {
  MailRef ref;
  // L'état du SERVEUR, qui traverse les sessions : grise le sujet.
  bool read = false;
  // Voir `kMailCarriesZeny` et suivants.
  std::uint8_t carried = 0;
  // Le contenu de ce courrier est déjà dans `app` : le sélectionner ne le
  // redemande pas.
  bool content_ready = false;
  // La dernière lecture demandée n'a pas eu de réponse : le bloc de lecture
  // invite à réessayer au lieu de « charger » sans fin.
  bool content_unanswered = false;
  std::string_view sender;
  std::string_view title;
  // Les secondes qui restent avant l'expiration, comptées à cette image ;
  // vide pour un courrier qui n'expire pas.
  std::optional<std::int64_t> expire_seconds_left;
};

// UNE PIÈCE JOINTE, reçue ou à envoyer.
struct MailItemView {
  // Le nom COMPOSÉ — raffinement, cartes, emplacements.
  TradeItemView item;
  int amount = 0;
};

// LE CONTENU LU du courrier sélectionné.
struct MailContentView {
  MailRef ref;
  std::string_view body;
  std::int64_t zeny = 0;
  std::span<const MailItemView> items;
};

// Les trois images de la colonne « reste à récupérer », lues de l'installation
// par `app`. Une image absente laisse place au repère textuel.
struct MailCarriedIcons {
  SkinImage zeny;
  SkinImage items;
  SkinImage zeny_and_items;
};

struct MailboxView {
  std::span<const MailRowView> rows;
  int unread = 0;
  // Une liste est demandée et pas encore venue, depuis moins que le délai que
  // `app` tient : « Chargement... » plutôt que « Aucun courrier. ».
  bool list_loading = false;
  // Le contenu du courrier sélectionné, s'il est arrivé. Un contenu d'un
  // AUTRE courrier est ignoré.
  const MailContentView* content = nullptr;
  MailCarriedIcons icons;
};

// La question qui attend sa réponse.
enum class MailConfirm : std::uint8_t { kNone, kDelete, kReturn, kClaimAll, kDeleteAll };

// La place du filtre.
inline constexpr std::size_t kMailFilterBytes = 64;

struct MailboxWindowState {
  // Recopié par `app` ; la croix et Échap le baissent, et `app` lit une boîte
  // baissée comme une fermeture.
  bool open = false;
  MailboxTab tab = MailboxTab::kNormal;
  std::array<char, kMailFilterBytes> filter{};
  std::optional<MailRef> selected;
  MailConfirm confirm = MailConfirm::kNone;
  // L'AGRANDISSEMENT EN DELTA : la taille mesurée à l'image précédente, et le
  // bloc de lecture qu'elle montrait.
  bool detail_shown = false;
  float last_width = 0.0F;
  float last_height = 0.0F;
};

struct MailboxAction {
  // La croix. ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  bool refresh = false;
  // « Écrire » : une écriture vide.
  bool compose = false;
  // « Répondre » : une écriture adressée à l'expéditeur de ce courrier.
  std::optional<MailRef> reply;
  // Un courrier sélectionné dont le contenu n'est pas là.
  std::optional<MailRef> read;
  // « Tout récupérer » sur le courrier ouvert.
  std::optional<MailRef> claim;
  // « Récupérer tout » confirmé : les courriers de l'onglet qui portent encore
  // quelque chose.
  std::vector<MailRef> claim_all;
  std::optional<MailRef> return_to_sender;
  std::optional<MailRef> erase;
  // « Supprimer tout » confirmé : les courriers de l'onglet qui ne portent plus
  // rien. Le serveur n'a pas de suppression groupée : une par courrier.
  std::vector<MailRef> erase_all;
  // La pièce jointe du courrier ouvert survolée, pour l'aperçu, et celle qu'on
  // veut décrire — un rang dans `content->items`, -1 sinon.
  int hovered_item = -1;
  int described_item = -1;
};

// LES COMPTES PAR ONGLET, et si l'onglet « Compte » se montre.
//
// ⚠ AUCUN CHEMIN DU SERVEUR NE REMPLIT LA BOÎTE « COMPTE » : il resterait
// éternellement à zéro. Il se masque tant qu'il est vide, et revient dès qu'il
// contient quelque chose — sans quoi un courrier réellement reçu deviendrait
// inaccessible.
struct MailboxTabs {
  std::array<int, kMailboxTabCount> counts{};
  bool show_account = false;
};
[[nodiscard]] MailboxTabs CountMailboxTabs(std::span<const MailRowView> rows);

// L'onglet courant qui s'efface sous le joueur se replie sur « Reçus », et la
// sélection comme la question en cours tombent avec lui.
void SettleMailboxTab(MailboxWindowState& state, const MailboxTabs& tabs);

// La ligne se montre-t-elle dans cet onglet, sous ce filtre ? Le filtre porte
// sur l'expéditeur OU le sujet, sans égard à la casse.
[[nodiscard]] bool MailRowShown(const MailRowView& row, MailboxTab tab, std::string_view filter);

// « J-3 », « 5 h », « expiré », ou rien pour un courrier qui n'expire pas.
[[nodiscard]] std::string MailExpiryLabel(std::optional<std::int64_t> seconds_left);

// L'expiration est-elle proche ALORS qu'il reste quelque chose à récupérer ?
[[nodiscard]] bool MailExpiresSoon(const MailRowView& row);

// « Retourner » vaut-il pour ce courrier ? Celui d'un joueur, dans la boîte de
// réception : un courrier « Système » n'a pas d'expéditeur à qui le rendre.
[[nodiscard]] bool MailCanReturn(const MailRowView& row);

// Ce que « Récupérer tout » viserait : les lignes montrées qui portent encore
// zeny ou items.
[[nodiscard]] std::vector<MailRef> MailClaimTargets(std::span<const MailRowView> rows,
                                                    const MailboxWindowState& state);

// Ce que « Supprimer tout » viserait : les lignes montrées qui ne portent plus
// rien. ⚠ LE SERVEUR REFUSE EN SILENCE de supprimer un courrier qui porte encore
// zeny ou items : ceux-là restent, et la confirmation dit combien.
[[nodiscard]] std::vector<MailRef> MailDeleteTargets(std::span<const MailRowView> rows,
                                                     const MailboxWindowState& state);

[[nodiscard]] MailboxAction DrawMailboxWindow(MailboxWindowState& state, const MailboxView& view,
                                              const SkinArt& art);

// ── L'ÉCRITURE ──────────────────────────────────────────────────────────────

// Les tampons de saisie : ils tiennent l'UTF-8 d'un texte que le fil borne
// plus court. Le destinataire porte vingt-trois caractères 1252, et chacun
// peut prendre jusqu'à trois octets en UTF-8, plus le zéro final.
inline constexpr std::size_t kMailRecipientMostChars = 23;
inline constexpr std::size_t kUtf8MostBytesPerCp1252Char = 3;
inline constexpr std::size_t kMailRecipientBytes =
    kMailRecipientMostChars * kUtf8MostBytesPerCp1252Char + 1;
inline constexpr std::size_t kMailSubjectBytes = 128;
inline constexpr std::size_t kMailMessageBytes = 1400;

// UNE PIÈCE JOINTE DE L'ÉCRITURE, et la case du sac d'où elle vient.
struct MailAttachmentView {
  MailItemView line;
  std::uint16_t client_slot = 0;
};

// UNE CASE DU SAC qu'on peut lâcher sur les pièces jointes : ce qu'elle montre
// encore, quantités déjà jointes retranchées.
struct MailBagView {
  std::uint16_t client_slot = 0;
  int amount = 0;
};

// La réponse du serveur à la vérification du destinataire.
enum class MailRecipientCheck : std::uint8_t { kNone, kPending, kUnknown, kFound };

struct MailComposeView {
  std::span<const MailAttachmentView> attachments;
  std::span<const MailBagView> bag;
  int attachment_capacity = kMailMaxAttachments;

  MailRecipientCheck check = MailRecipientCheck::kNone;
  // Le nom tel que le serveur l'a réémis, le job lisible, le niveau.
  std::string_view checked_name;
  std::string_view checked_job;
  int checked_level = 0;

  // Les frais d'envoi ; zéro tant qu'il n'y a rien à facturer.
  std::int64_t fee = 0;
  // Zeny joint + frais, et la bourse qui ne les couvre pas.
  std::int64_t total_debit = 0;
  bool purse_short = false;

  // Les longueurs ENCODÉES pour le fil de la saisie courante.
  std::size_t subject_wire_bytes = 0;
  std::size_t message_wire_bytes = 0;
  std::size_t subject_max_bytes = kMailTitleMaxBytes;
  std::size_t message_max_bytes = kMailBodyMaxBytes;

  // Le dernier refus — local ou du serveur —, déjà traduit, ou vide.
  std::string_view error;
  // Un envoi est parti et n'a pas reçu sa réponse.
  bool send_in_flight = false;
};

struct MailComposeWindowState {
  // Recopié par `app` ; la croix, Échap et « Annuler » le baissent — c'est une
  // ANNULATION, que `app` dit au serveur.
  bool open = false;
  std::array<char, kMailRecipientBytes> recipient{};
  std::array<char, kMailSubjectBytes> subject{};
  std::array<char, kMailMessageBytes> message{};
  std::int64_t zeny = 0;
  // Le récapitulatif d'un envoi qui emporte quelque chose attend sa réponse.
  bool confirming_send = false;
  // LA QUANTITÉ DEMANDÉE pour une pile lâchée.
  bool asking_amount = false;
  std::uint16_t asking_slot = 0;
  int asking_value = 1;
};

// LA SAISIE, recopiée telle quelle : `app` la juge et l'encode.
struct MailComposeSend {
  std::string recipient;
  std::string subject;
  std::string message;
  std::int64_t zeny = 0;
};

// Une case du sac et une quantité : à joindre, ou à retirer.
struct MailAttachmentMove {
  std::uint16_t client_slot = 0;
  int amount = 0;
};

struct MailComposeAction {
  // La croix, Échap ou « Annuler ». ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool cancel = false;
  // « Vérifier », ou le champ du destinataire quitté après modification.
  bool check = false;
  std::optional<MailComposeSend> send;
  std::optional<MailAttachmentMove> attach;
  std::optional<MailAttachmentMove> remove;
  // La pièce jointe survolée et celle à décrire — un rang dans `attachments`,
  // -1 sinon.
  int hovered_item = -1;
  int described_item = -1;
};

// Une écriture neuve : saisie vide, destinataire pré-rempli (vide ou nul pour
// aucun). Un nom trop long pour le tampon est coupé sur une frontière de
// caractère UTF-8, jamais au milieu.
void OpenMailCompose(MailComposeWindowState& state, std::string_view recipient);

// L'envoi emporte-t-il quelque chose ? Alors il passe par le récapitulatif.
[[nodiscard]] bool MailSendCarries(const MailComposeWindowState& state,
                                   const MailComposeView& view);

// Le texte du récapitulatif : destinataire, items, zeny, frais, total. Déjà
// traduit.
[[nodiscard]] std::string MailSendRecapText(const MailComposeWindowState& state,
                                            const MailComposeView& view);

[[nodiscard]] MailComposeAction DrawMailComposeWindow(MailComposeWindowState& state,
                                                      const MailComposeView& view,
                                                      const SkinArt& art);

}  // namespace ro::ui
