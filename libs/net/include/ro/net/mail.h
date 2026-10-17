#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/inventory.h"
#include "ro/sim/mailbox.h"

namespace ro::net {

// LE COURRIER (RODEX) : treize paquets reçus, douze émis. Voir `ro/sim/mailbox.h`.
//
// L'ORDRE D'OUVRIR OU DE FERMER LA BOÎTE (`ZC_MAIL_WINDOWS`) vient de l'ancien
// courrier, mais c'est celui que le serveur envoie pour `@mail` : il commande
// la boîte RODEX. Son type suit le serveur, seul à l'émettre — zéro ouvre, un
// ferme ; toute autre valeur rend la trame indécodable, sans ordre.
//
// 🔴 LE TITRE, LE CORPS ET L'EXPÉDITEUR VIENNENT D'AUTRES JOUEURS. Chaque
// longueur annoncée est confrontée à la trame avant toute lecture ; une trame
// tronquée ou incohérente est rejetée ENTIÈRE, sans rien changer à la boîte.
// Au-delà de `sim::kMailMostListed` courriers ou `sim::kMailMostItems` items,
// le reste est ignoré. Les octets nuls internes d'un corps sont des sauts de
// ligne.
//
// Les chaînes reçues passent par `core::Utf8FromWire`, comme celles des autres
// traducteurs : la boîte tient de l'UTF-8. Seuls les NOMS — l'expéditeur, le
// destinataire de l'écriture accordée — gardent aussi leurs octets du fil, pour
// repartir tels quels.
//
// Ce traducteur lit aussi, SANS LES CONSOMMER, les deux changements de carte :
// le serveur a remis l'écriture à zéro avant de déplacer le joueur.
//
// `now_unix_s` fixe l'instant d'expiration des courriers listés, que le serveur
// annonce en secondes restantes.
[[nodiscard]] FrameOutcome ApplyMailFrame(const Frame& frame, sim::Mailbox& mailbox,
                                          std::int64_t now_unix_s);

// ── LES CHAMPS D'UN ENVOI, ENCODÉS POUR LE FIL ──────────────────────────────
//
// Le destinataire est un IDENTIFIANT, cherché octet par octet dans la base du
// serveur : il part en 1252 strict, et un caractère que 1252 ne porte pas le
// rend inexprimable. Le titre et le corps sont des PHRASES
// (`core::WireFromUtf8Text`).
//
// ⚠ CE SONT CES OCTETS-LÀ QUE `sim::Mailbox::CheckSend` JUGE, puis que
// `RequestSendMail` émet : les bornes du serveur portent sur eux, pas sur le
// texte saisi.
struct MailWireFields {
  std::string recipient;
  std::string title;
  std::string body;
};

// Un nom saisi, tel qu'il partirait : 1252 strict, ou rien s'il n'y est pas
// exprimable. ⚠ UN NOM REÇU DU FIL SE RENVOIE PAR SES OCTETS, pas par ceux-ci :
// l'aller-retour par l'UTF-8 ne les rend pas toujours à l'identique.
[[nodiscard]] std::optional<std::string> MailRecipientWire(std::string_view recipient_utf8);

[[nodiscard]] core::Result<MailWireFields> EncodeMailFields(std::string_view recipient_utf8,
                                                            std::string_view title_utf8,
                                                            std::string_view body_utf8);

// ── LES TRAMES QUI ENGAGENT LE BIEN DU JOUEUR ───────────────────────────────
//
// Construites ici, et REFUSÉES ici, quel que soit l'appelant : elles sont le
// dernier rempart, et leurs essais les vérifient octet par octet.

// L'envoi. 🔴 Refuse un titre vide ou porteur d'un octet nul — les pièces
// jointes seraient détruites —, un destinataire vide, toute longueur hors des
// bornes, un zeny négatif ou au-delà de `sim::kMailMostZeny`. Les longueurs
// émises comptent le zéro final, et la trame n'en porte pas un octet de plus.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeSendMail(const MailWireFields& fields,
                                                                     std::int64_t zeny);

// L'ajout d'une pièce jointe. Refuse une case avant `sim::kMailFirstBagSlot` —
// zéro est le chemin du zeny côté serveur — et une quantité nulle, négative ou
// au-delà de `sim::kMailMostAttachAmount`.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeAttachItem(std::uint16_t client_slot,
                                                                       std::int32_t amount);

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// Chacun REFUSE, sans rien envoyer, ce que le serveur refuserait en silence ou
// ferait payer au joueur. Ceux qui visent un courrier n'acceptent qu'un
// identifiant que la boîte a reçu : le serveur journalise un avertissement pour
// tout autre.

// « Ouvre la boîte » ou « rafraîchis-la » : les plus récents identifiants de
// chaque boîte, zéro pour une boîte vide.
[[nodiscard]] core::Result<void> RequestMailList(Connection& link, std::int64_t newest_normal,
                                                 std::int64_t newest_returned,
                                                 std::int64_t newest_account);

// Refusée si la même lecture est déjà en route ; notée dans la boîte, qui la
// tient pour perdue au silence.
[[nodiscard]] core::Result<void> RequestReadMail(Connection& link, sim::Mailbox& mailbox,
                                                 std::int64_t mail_id, std::uint32_t now_ms);

// Refusés si le courrier ne porte plus cette pièce jointe, ou si la même
// récupération est déjà en vol. Notent la récupération dans la boîte.
[[nodiscard]] core::Result<void> RequestClaimItems(Connection& link, sim::Mailbox& mailbox,
                                                   std::int64_t mail_id, std::uint32_t now_ms);
[[nodiscard]] core::Result<void> RequestClaimZeny(Connection& link, sim::Mailbox& mailbox,
                                                  std::int64_t mail_id, std::uint32_t now_ms);

// Refusé tant qu'il reste du zeny ou des items : le serveur le refuserait en
// silence.
[[nodiscard]] core::Result<void> RequestDeleteMail(Connection& link, const sim::Mailbox& mailbox,
                                                   std::int64_t mail_id);

// Boîte normale seulement, pas un courrier de PNJ, et pas deux fois en route.
[[nodiscard]] core::Result<void> RequestReturnMail(Connection& link, sim::Mailbox& mailbox,
                                                   std::int64_t mail_id);

// « Je veux écrire », destinataire pré-rempli ou vide, en OCTETS DU FIL — ceux
// de `MailRecipientWire`, ou ceux qu'un courrier reçu a portés.
[[nodiscard]] core::Result<void> RequestOpenWrite(Connection& link,
                                                  std::string_view recipient_wire);
// « J'abandonne l'écriture » : le brouillon ferme.
[[nodiscard]] core::Result<void> RequestCancelWrite(Connection& link, sim::Mailbox& mailbox);

// « Je joins cette quantité de cette case. » `bag_item` est la case du sac du
// monde, NON amputée de ce qui est joint.
[[nodiscard]] core::Result<void> RequestAttachItem(Connection& link, sim::Mailbox& mailbox,
                                                   const sim::InventoryItem& bag_item,
                                                   std::int32_t amount, std::uint32_t now_ms);
// « Je retire cette quantité de cette case », au plus ce qui est joint, et
// jamais pendant un envoi (`sim::Mailbox::CanDetach`).
[[nodiscard]] core::Result<void> RequestDetachItem(Connection& link, const sim::Mailbox& mailbox,
                                                   std::uint16_t client_slot, std::int32_t amount);

// « Ce nom existe-t-il ? », en octets du fil : la boîte garde le nom interrogé,
// et la réponse ne vaut que pour lui.
[[nodiscard]] core::Result<void> RequestCheckRecipient(Connection& link, sim::Mailbox& mailbox,
                                                       std::string_view recipient_wire,
                                                       std::uint32_t now_ms);

// « J'envoie. » `fields` vient d'`EncodeMailFields` ; `purse` est la bourse, si
// le serveur l'a dite. Revalide tout — `CheckSend` puis `EncodeSendMail` — et
// note l'envoi en vol.
[[nodiscard]] core::Result<void> RequestSendMail(Connection& link, sim::Mailbox& mailbox,
                                                 const MailWireFields& fields, std::int64_t zeny,
                                                 std::optional<std::int64_t> purse,
                                                 std::uint32_t now_ms);

}  // namespace ro::net
