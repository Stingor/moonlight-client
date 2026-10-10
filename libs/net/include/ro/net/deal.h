#pragma once

#include <cstdint>
#include <span>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/deal.h"
#include "ro/sim/inventory.h"

namespace ro::net {

// L'ÉCHANGE ENTRE JOUEURS : huit paquets reçus, six émis. Voir `ro/sim/deal.h`.
//
// 🔴 L'ACCUSÉ D'UN DÉPÔT RETIRE L'ITEM DU SAC, dans la même trame : un renvoi
// d'annulation qui suit dans la même fournée doit trouver le sac déjà amputé.
// D'où le sac en paramètre.
//
// Ce traducteur lit aussi, SANS LES CONSOMMER, les deux changements de carte :
// le serveur a annulé l'échange avant de déplacer le joueur.
[[nodiscard]] FrameOutcome ApplyDealFrame(const Frame& frame, sim::Deal& deal, sim::Inventory& bag);

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// Chacun REFUSE, sans rien envoyer, ce que le serveur refuserait, jetterait
// sans réponse ou tiendrait pour de la triche : les contrôles sont ceux de
// `sim::Deal`, que la fenêtre appelle d'abord pour dire pourquoi.

// « Je propose un échange à ce joueur. » ⚠ LE SERVEUR ANNULE la session en
// cours du proposant : refusé pendant un échange ouvert, qu'il faut d'abord
// annuler soi-même.
[[nodiscard]] core::Result<void> ProposeDeal(Connection& link, sim::Deal& deal,
                                             sim::DealPartner partner, std::uint32_t now_ms);

// « J'accepte » ou « je refuse » la proposition reçue.
[[nodiscard]] core::Result<void> AnswerDealProposal(Connection& link, sim::Deal& deal, bool accept);

// « Je dépose cette quantité de cet item. » `bag_item` est la case du sac TELLE
// QU'ELLE EST, déjà amputée de ce qui est accusé.
[[nodiscard]] core::Result<void> OfferDealItem(Connection& link, sim::Deal& deal,
                                               const sim::InventoryItem& bag_item,
                                               std::int32_t amount, std::uint32_t now_ms);

// « J'offre ce montant », ABSOLU. `purse` est ce que possède le joueur.
[[nodiscard]] core::Result<void> OfferDealZeny(Connection& link, sim::Deal& deal,
                                               std::int64_t amount, std::int64_t purse);

// « Mon offre est prête. » Refusé tant qu'un dépôt est en route : ce que le
// joueur verrouillerait n'est pas encore ce que le serveur tient.
[[nodiscard]] core::Result<void> LockDeal(Connection& link, sim::Deal& deal);

// « J'échange », pour l'offre du partenaire au rang `confirmed_partner_revision`
// — celui du récapitulatif que le joueur a confirmé.
[[nodiscard]] core::Result<void> ExecuteDeal(Connection& link, sim::Deal& deal,
                                             std::uint32_t confirmed_partner_revision,
                                             std::int64_t purse);

// « J'annule » : l'échange ouvert, ma proposition pendante, ou celle reçue.
[[nodiscard]] core::Result<void> CancelDeal(Connection& link, sim::Deal& deal);

// ── LE DERNIER REMPART ──────────────────────────────────────────────────────
//
// Ce qu'une trame émise ferait à un échange ouvert, quel que soit le geste qui
// l'a demandée. Le juge d'émission de la liaison (`Connection::set_send_gate`)
// s'en sert : une trame refusée ne part pas.
enum class DealFrameRefusal : std::uint8_t {
  kNone,
  // Utiliser, équiper ou ôter un item, quel qu'il soit : le client d'origine
  // les refuse tous pendant un échange. Un item offert puis équipé compterait
  // pour zéro au recompte du serveur ; l'utiliser ferait fondre sa pile.
  kItemGesture,
  // Une compétence, tant que j'ai quelque chose en jeu : elle peut consommer
  // l'item ou le zeny offert.
  kSkillWithStake,
  // Un achat chez un marchand ou un dépôt en banque, tant que j'offre du zeny.
  kZenyWithStake,
};

[[nodiscard]] DealFrameRefusal DealRefusesFrame(const sim::Deal& deal,
                                                std::span<const std::uint8_t> frame) noexcept;

}  // namespace ro::net
