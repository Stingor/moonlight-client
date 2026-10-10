// La banque de zeny. Voir l'en-tête pour la bascule et pour les dispositions
// qui diffèrent d'un paquet à l'autre.

#include "ro/net/bank.h"

#include "ro/proto/map_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// ── LES RAISONS, TELLES QUE LE SERVEUR LES NUMÉROTE ─────────────────────────
//
// ⚠ TROIS ÉNUMÉRATIONS DISTINCTES, et elles se recouvrent : la valeur 2 existe
// dans les trois et n'y veut jamais dire la même chose. C'est pourquoi chacune
// a ses constantes, plutôt qu'un `2` écrit trois fois.

// `ZC_BANKING_CHECK`. rAthena n'émet que zéro ; les deux autres viennent du
// client d'origine, qui sait les afficher, et un fork peut s'en servir.
constexpr std::int16_t kCheckOk = 0;
constexpr std::int16_t kCheckBusy = 2;

// `e_BANKING_DEPOSIT_ACK`, dans `src/map/clif.hpp`.
constexpr std::int16_t kDepositSuccess = 0;
constexpr std::int16_t kDepositNoMoney = 2;
constexpr std::int16_t kDepositOverflow = 3;
// ⚠ AUCUN SERVEUR NE L'ENVOIE : le code n'est dans aucune des deux
// énumérations de rAthena, mais le client d'origine a un cas pour lui.
constexpr std::int16_t kDepositProhibited = 4;

// `e_BANKING_WITHDRAW_ACK`. ⚠ `BWA_UNKNOWN_ERROR` vaut DEUX et non un : le
// retrait numérote ses raisons à l'envers du dépôt.
constexpr std::int16_t kWithdrawSuccess = 0;
constexpr std::int16_t kWithdrawNoMoney = 1;
constexpr std::int16_t kWithdrawProhibited = 3;

// LE VERDICT D'UN ACCUSÉ DE DÉPÔT. ⚠ TOUT CE QUI N'EST PAS NOMMÉ TOMBE DANS
// L'ERREUR DE SYSTÈME, `BDA_ERROR` compris : le client d'origine n'a pas de cas
// pour lui et le laisse échouer dans son défaut.
[[nodiscard]] sim::BankVerdict DepositVerdict(std::int16_t reason) {
  switch (reason) {
    case kDepositNoMoney:
      return sim::BankVerdict::kDepositNoMoney;
    case kDepositOverflow:
      return sim::BankVerdict::kDepositOverflow;
    case kDepositProhibited:
      return sim::BankVerdict::kProhibited;
    default:
      return sim::BankVerdict::kSystemError;
  }
}

// Le verdict d'un accusé de retrait. Même règle pour le défaut, et il attrape
// ici `BWA_UNKNOWN_ERROR` — que le serveur emploie, lui, pour « montant nul »
// comme pour « la poche déborderait ».
[[nodiscard]] sim::BankVerdict WithdrawVerdict(std::int16_t reason) {
  switch (reason) {
    case kWithdrawNoMoney:
      return sim::BankVerdict::kWithdrawNoMoney;
    case kWithdrawProhibited:
      return sim::BankVerdict::kProhibited;
    default:
      return sim::BankVerdict::kSystemError;
  }
}

// CE QU'UN ACCUSÉ RÉUSSI POSE. Les deux en portent autant : le nouveau solde de
// la banque, et celui de la poche.
//
// ⚠ LA POCHE ARRIVE AUSSI PAR `ZC_PAR_CHANGE`, que le serveur envoie dans le
// même souffle (`pc_payzeny` / `pc_getzeny`). La reprendre ici n'est donc pas
// indispensable — mais c'est ce que fait le client d'origine, et l'ordre des
// deux trames n'est garanti par rien.
void TellTransfer(sim::World& world, std::int64_t money, std::int32_t zeny) {
  world.bank().TellVault(money);
  // Le verdict de la pose ne nous apprend rien : le numéro est écrit ici,
  // donc connu, et la bourse ne dépend d'aucun acteur.
  static_cast<void>(
      world.SetPlayerParameter(static_cast<std::uint16_t>(sim::ServerParameter::kZeny), zeny));
}

}  // namespace

FrameOutcome ApplyBankFrame(const Frame& frame, sim::World& world) {
  switch (frame.opcode) {
    case proto::ZcBankingCheck::kOpcode: {
      proto::ZcBankingCheck packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // 🔴 LE SOLDE SE POSE D'ABORD, ET SANS CONDITION DE RAISON. C'est ce que
      // fait le client d'origine, et il a raison : un solde à jour ne dépend pas
      // de ce qu'on décide d'afficher, et d'autres écrans le lisent.
      world.bank().TellVault(packet.money);
      if (packet.reason != kCheckOk) {
        world.bank().TellVerdict(packet.reason == kCheckBusy ? sim::BankVerdict::kSystemBusy
                                                             : sim::BankVerdict::kSystemError);
        return FrameOutcome::kApplied;
      }
      // ⚠ IL BASCULE. Le client d'origine referme la fenêtre si elle était là,
      // et ne la crée que sinon — c'est ce qui rend le raccourci réversible
      // sans second paquet.
      if (world.bank().open()) {
        world.bank().Close();
      } else {
        world.bank().Open();
      }
      world.bank().TellVerdict(sim::BankVerdict::kDone);
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckBankingDeposit::kOpcode: {
      proto::ZcAckBankingDeposit packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      if (packet.reason != kDepositSuccess) {
        world.bank().TellVerdict(DepositVerdict(packet.reason));
        return FrameOutcome::kApplied;
      }
      TellTransfer(world, packet.money, packet.zeny);
      world.bank().TellVerdict(sim::BankVerdict::kDone);
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckBankingWithdraw::kOpcode: {
      proto::ZcAckBankingWithdraw packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      if (packet.reason != kWithdrawSuccess) {
        world.bank().TellVerdict(WithdrawVerdict(packet.reason));
        return FrameOutcome::kApplied;
      }
      TellTransfer(world, packet.money, packet.zeny);
      world.bank().TellVerdict(sim::BankVerdict::kDone);
      return FrameOutcome::kApplied;
    }

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestBankCheck(Connection& link, std::uint32_t account_id) {
  proto::CzReqBankingCheck request;
  request.AID = account_id;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestBankDeposit(Connection& link, std::uint32_t account_id,
                                      std::int32_t zeny) {
  proto::CzReqBankingDeposit request;
  request.AID = account_id;
  request.zeny = zeny;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestBankWithdraw(Connection& link, std::uint32_t account_id,
                                       std::int32_t zeny) {
  proto::CzReqBankingWithdraw request;
  request.AID = account_id;
  request.zeny = zeny;
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net
