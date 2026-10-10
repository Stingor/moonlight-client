#include "items/bank_view.h"

#include <string>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/bank.h"
#include "ro/sim/bank.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// 🔴 LE PLAFOND EST DÉCLARÉ DES DEUX CÔTÉS, et voici l'endroit qui les voit
// tous les deux. `ui` ne connaît pas `sim` — le graphe des bibliothèques
// l'interdit —, si bien que la borne existe dans les deux en-têtes. Leur
// divergence est donc une erreur de COMPILATION, pas une remarque en revue.
static_assert(ro::ui::kBankZenyMax == ro::sim::kBankZenyMax,
              "le plafond de la banque doit être le même dans ui et dans sim");

// LA CADENCE MINIMALE ENTRE DEUX DEMANDES D'OUVERTURE, en millisecondes.
//
// ⚠ ELLE EXISTE PARCE QUE LA RÉPONSE BASCULE : deux demandes coup sur coup
// ouvrent puis referment, et le joueur n'a vu qu'un clignotement.
constexpr std::uint32_t kToggleQuietMs = 400;

// CE QUE LE SERVEUR A RÉPONDU, EN FRANÇAIS.
//
// ⚠ CE SONT LES PHRASES DU CLIENT D'ORIGINE, traduites comme le reste de
// l'interface. Le SENS et le MOMENT sont les siens, mot pour mot.
[[nodiscard]] const char* VerdictSaid(ro::sim::BankVerdict verdict) {
  switch (verdict) {
    case ro::sim::BankVerdict::kSystemBusy:
      return Tr("Une opération est en cours, réessayez.");
    case ro::sim::BankVerdict::kSystemError:
      return Tr("La banque est indisponible. Réessayez dans quelques minutes.");
    case ro::sim::BankVerdict::kDepositNoMoney:
      return Tr("Vous n'avez pas assez de zeny.");
    case ro::sim::BankVerdict::kDepositOverflow:
      return Tr("Le montant maximum est de 2 147 483 647 zeny.");
    case ro::sim::BankVerdict::kWithdrawNoMoney:
      return Tr("Le solde de la banque ne suffit pas.");
    case ro::sim::BankVerdict::kProhibited:
      return Tr("Une fenêtre d'échange ou de stockage est ouverte : transfert impossible.");
    case ro::sim::BankVerdict::kDone:
      break;
  }
  return nullptr;
}

// LA POCHE, ou zéro tant que le serveur ne l'a pas dite.
[[nodiscard]] std::int64_t PocketOf(const ro::sim::World& world) {
  const std::int64_t zeny = world.player_status().zeny;
  return zeny == ro::sim::kUnknownStat ? 0 : zeny;
}

}  // namespace

ro::ui::BankWindowView BankView::Of(const ro::sim::World& world) {
  const std::int64_t zeny = world.player_status().zeny;
  return ro::ui::BankWindowView{
      .vault = world.bank().vault(),
      .zeny = zeny == ro::sim::kUnknownStat ? 0 : zeny,
      .zeny_known = zeny != ro::sim::kUnknownStat,
  };
}

void BankView::Apply(const ro::ui::BankWindowAction& asked, ro::ui::BankWindowState& state,
                     ro::net::Connection& link, ro::sim::World& world) {
  // ⚠ FERMER NE PARLE À PERSONNE : le serveur ne tient aucun état de banque
  // ouverte, et le client d'origine ne lui envoie jamais sa fermeture.
  if (asked.verb == ro::ui::BankVerb::kClose) {
    world.bank().Close();
    return;
  }
  const bool deposit = asked.verb == ro::ui::BankVerb::kDeposit;
  if (!deposit && asked.verb != ro::ui::BankVerb::kWithdraw) {
    return;
  }

  // 🔴 ON REVALIDE DANS L'ORDRE DU CLIENT D'ORIGINE, et cet ordre décide de la
  // phrase : un montant nul alors que la poche est vide se refuse sur le
  // MONTANT, pas sur la poche. Changer l'ordre changerait le message sans rien
  // changer au résultat — et c'est exactement le genre d'écart qu'on ne voit
  // qu'en jouant.
  const std::int64_t vault = world.bank().vault();
  const std::int64_t pocket = PocketOf(world);
  const char* refused = nullptr;
  if (asked.amount <= 0) {
    refused = Tr("Saisissez au moins 1 zeny.");
  } else if (asked.amount > ro::sim::kBankZenyMax) {
    refused = Tr("Saisissez un montant inférieur à 2 147 483 647 zeny.");
  } else if (deposit) {
    if (pocket <= 0) {
      refused = Tr("Vous n'avez aucun zeny.");
    } else if (asked.amount > pocket) {
      refused = Tr("Vous n'avez pas assez de zeny.");
    } else if (vault + asked.amount > ro::sim::kBankZenyMax) {
      refused = Tr("Vous ne pouvez pas détenir plus de 2 147 483 647 zeny.");
    }
  } else {
    if (vault <= 0) {
      refused = Tr("Vous n'avez aucun zeny.");
    } else if (asked.amount > vault) {
      refused = Tr("Le solde de la banque ne suffit pas.");
    } else if (pocket + asked.amount > ro::sim::kBankZenyMax) {
      refused = Tr("Vous ne pouvez pas détenir plus de 2 147 483 647 zeny.");
    }
  }
  if (refused != nullptr) {
    // ⚠ LA BANDE, ET NON LA CONVERSATION : un refus que la fenêtre prononce
    // toute seule reste dans la fenêtre. C'est le partage du client d'origine.
    ro::ui::SetBankStatus(state, refused, true);
    return;
  }

  // ⚠ LE MONTANT TIENT DANS UN `int32` : les deux gardes ci-dessus l'assurent,
  // et le champ du fil n'est pas plus large.
  const auto amount = static_cast<std::int32_t>(asked.amount);
  const ro::core::Result<void> sent =
      deposit ? ro::net::RequestBankDeposit(link, world.player_id(), amount)
              : ro::net::RequestBankWithdraw(link, world.player_id(), amount);
  if (!sent) {
    ro::core::LogWarning("banque : transfert non émis — {}", ro::core::Describe(sent.error()));
  }
}

void BankView::ToggleSession(ro::net::Connection& link, ro::sim::World& world,
                             std::uint32_t now_ms) {
  // Ouverte : on ferme chez nous, sans un octet — comme le raccourci du client
  // d'origine quand sa fenêtre est là.
  if (world.bank().open()) {
    world.bank().Close();
    return;
  }
  if (ever_asked_ && now_ms - asked_ms_ < kToggleQuietMs) {
    return;
  }
  ever_asked_ = true;
  asked_ms_ = now_ms;
  const ro::core::Result<void> sent = ro::net::RequestBankCheck(link, world.player_id());
  if (!sent) {
    ro::core::LogWarning("banque : demande non émise — {}", ro::core::Describe(sent.error()));
  }
}

void BankView::NoteVerdict(ro::sim::World& world, std::uint32_t now_ms) {
  const std::uint64_t rank = world.bank().verdict_seq();
  if (rank == said_verdict_) {
    return;
  }
  said_verdict_ = rank;
  const char* said = VerdictSaid(world.bank().verdict());
  if (said == nullptr) {
    return;
  }
  world.RecordChat(ro::sim::ChatKind::kServer, 0, std::string(said), now_ms);
}

}  // namespace moonclient
