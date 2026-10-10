#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"

namespace ro::net {

// LE RAPPORT DE BUG, sur le fil — voir l'ADR 0052.
//
//   CZ 0x0F13 [op][len][category:1][ctx_len:2][contexte][message]
//   ZC 0x0F14 [op][len][status:1]   0 enregistré · 1 trop rapide · 2 refusé
//
// 🔴 LE CLIENT N'ENVOIE AUCUNE IDENTITÉ : compte, personnage, carte, position,
// le serveur les prend de la session. Un client bricolé pourrait sinon signer
// au nom d'un autre, et le relais Discord le publierait sous ce nom.
//
// 🔴 TOUT PART EN CP1252, LE CONTEXTE AUSSI. Le tuyau du serveur — base,
// relais — est latin1 ; nos noms d'items et de PNJ sortent de nos tables, en
// UTF-8. Les recopier tels quels doublerait leurs accents, et seulement les
// leurs. Le serveur borne lui-même : contexte à 1024 octets, message à 500.

// Les catégories, miroir de `e_bug_report_category` du serveur. Une catégorie
// qu'il ne connaît pas retombe chez lui sur « generic » : un rangement perdu,
// jamais un rapport.
enum class BugReportCategory : std::uint8_t {
  kGeneric = 0,
  kItem = 1,
  kSkill = 2,
  kNpc = 3,
  kQuest = 4,
  kStyle = 5,
};

// Ce que le serveur garde du message.
inline constexpr std::size_t kBugReportMessageMostBytes = 500;

// Envoie le rapport ; le contexte et le message arrivent en UTF-8.
[[nodiscard]] core::Result<void> SendBugReport(Connection& link, std::uint8_t category,
                                               std::string_view context_utf8,
                                               std::string_view message_utf8);

// L'accusé, s'il s'agit de lui ; rien sinon, ou une trame qui ne se lit pas.
[[nodiscard]] std::optional<std::uint8_t> ReadBugReportAck(const Frame& frame);

}  // namespace ro::net
