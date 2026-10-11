// Ce que le serveur de carte dit du personnage. Voir l'en-tête.

#include "ro/net/character_notice.h"

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/proto/ledger_packets.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// Le paramètre que le gain touche : SP_BASEEXP et SP_JOBEXP, dans
// src/map/map.hpp du serveur.
constexpr std::uint16_t kBaseExpVar = 1;
constexpr std::uint16_t kJobExpVar = 2;
// Le genre de gain qui vient d'une quête.
constexpr std::uint16_t kQuestExpType = 1;

// La teinte de la ligne : jaune pour un gain ordinaire, violette pour celui
// d'une quête.
constexpr std::uint32_t kExpColor = 0xFFFF00;
constexpr std::uint32_t kQuestExpColor = 0xBE58BE;

constexpr std::size_t kExpLineCapacity = 128;

// Ce qui précède la fin de phrase quand le paramètre n'a pas de début.
constexpr std::string_view kLoneEndPrefix = " ";

// La phrase d'un gain, selon le paramètre et le sens : le nombre et le
// paramètre, puis « obtenus » ou « perdus ».
//
// ⚠ CHAQUE PHRASE PASSE EN LITTÉRAL à `snprintf` : un format choisi à part ne
// se vérifie plus contre ses arguments.
[[nodiscard]] std::string ExpLine(std::uint16_t var_id, std::int64_t amount) {
  // ⚠ LE NOMBRE GARDE SON SIGNE : une perte s'écrit « '-1200' … perdus », comme
  // le client d'origine l'écrit.
  const auto shown = static_cast<long long>(amount);
  const bool lost = amount < 0;
  std::array<char, kExpLineCapacity> line{};
  if (var_id == kBaseExpVar && lost) {
    std::snprintf(line.data(), line.size(), core::Tr("'%lld' d'expérience de base perdus."), shown);
  } else if (var_id == kBaseExpVar) {
    std::snprintf(line.data(), line.size(), core::Tr("'%lld' d'expérience de base obtenus."),
                  shown);
  } else if (var_id == kJobExpVar && lost) {
    std::snprintf(line.data(), line.size(), core::Tr("'%lld' d'expérience de job perdus."), shown);
  } else if (var_id == kJobExpVar) {
    std::snprintf(line.data(), line.size(), core::Tr("'%lld' d'expérience de job obtenus."), shown);
  } else {
    // Un autre paramètre n'a pas de début de phrase : seule la fin s'écrit,
    // derrière son espace.
    return std::string(kLoneEndPrefix) + (lost ? core::Tr("perdus.") : core::Tr("obtenus."));
  }
  return std::string(line.data());
}

[[nodiscard]] FrameOutcome AppliedExp(const proto::ExpNotice& notice, sim::World& world,
                                      std::uint32_t now_ms) {
  if (notice.account_id != world.player_id() || notice.amount == 0) {
    return FrameOutcome::kIgnored;
  }
  world.RecordChat(sim::ChatKind::kExp, 0, ExpLine(notice.var_id, notice.amount), now_ms,
                   notice.exp_type == kQuestExpType ? kQuestExpColor : kExpColor);
  return FrameOutcome::kApplied;
}

[[nodiscard]] FrameOutcome CheckedAccountId(const proto::AccountIdNotice& notice,
                                            const sim::World& world) {
  if (notice.account_id != world.player_id()) {
    core::LogWarning("le serveur de carte annonce le compte {} ; la connexion a donné {}",
                     notice.account_id, world.player_id());
    return FrameOutcome::kIgnored;
  }
  return FrameOutcome::kApplied;
}

}  // namespace

FrameOutcome ApplyCharacterNoticeFrame(const Frame& frame, sim::World& world,
                                       std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ExpNotice::kOpcode: {
      const core::Result<proto::ExpNotice> notice = proto::DecodeExpNotice(frame.bytes);
      return notice ? AppliedExp(*notice, world, now_ms) : FrameOutcome::kUndecodable;
    }
    case proto::AccountIdNotice::kOpcode: {
      const core::Result<proto::AccountIdNotice> notice = proto::DecodeAccountIdNotice(frame.bytes);
      return notice ? CheckedAccountId(*notice, world) : FrameOutcome::kUndecodable;
    }
    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace ro::net
