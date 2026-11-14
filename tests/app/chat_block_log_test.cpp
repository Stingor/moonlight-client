// « JOURNAL DES BLOCAGES DE CHAT » : l'entrée du staff, et ce que le journal
// prépare pour sa fenêtre — la demande, l'appariement de la réponse, l'attente
// qui expire, les lignes écrites.

#include "world/chat_block_log.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/chat_blocks.h"
#include "ro/ui/chat_block_log_window.h"
#include "ro/ui/entity_menu.h"

#include "world/entity_menu.h"

using moonclient::BuildEntityMenu;
using moonclient::ChatBlockLog;
using moonclient::EntityFamily;
using moonclient::EntityMenuFacts;
using moonclient::EntityVerb;
using moonclient::kChatBlockLogWaitMs;
using ro::net::ChatBlock;
using ro::net::ChatBlockKind;
using ro::net::ChatBlocksReply;
using ro::net::ChatBlocksStatus;
using ro::ui::ChatBlockLogPhase;

namespace {

constexpr std::uint32_t kPlayer = 2000002;
constexpr std::uint32_t kOtherPlayer = 2000003;
constexpr std::uint32_t kStartMs = 1000;
constexpr std::uint32_t kMutedAt = 1760190000;
constexpr std::int32_t kMuteMinutes = 60;

[[nodiscard]] bool Offers(EntityFamily family, bool staff) {
  EntityMenuFacts facts;
  facts.family = family;
  facts.id = kPlayer;
  facts.name = "Quelqu'un";
  facts.staff = staff;
  for (const ro::ui::EntityMenuItem& item : BuildEntityMenu(facts, {}).items) {
    if (item.verb == static_cast<int>(EntityVerb::kChatBlockLog)) {
      return item.staff && item.submenu && !item.disabled;
    }
  }
  return false;
}

[[nodiscard]] ChatBlocksReply Found(std::uint32_t aid) {
  ChatBlocksReply reply;
  reply.aid = aid;
  reply.status = ChatBlocksStatus::kFound;
  reply.blocks = {
      ChatBlock{.date_s = kMutedAt + 1, .kind = ChatBlockKind::kUnmute, .author = "Garde"},
      ChatBlock{.date_s = kMutedAt,
                .minutes = kMuteMinutes,
                .kind = ChatBlockKind::kMute,
                .author = "Sentinelle"}};
  return reply;
}

struct Consulting {
  ChatBlockLog log;
  ro::ui::ChatBlockLogWindowState state;

  Consulting() { log.Open(state, kPlayer, "Alice — AID 2000002", kStartMs); }

  void Frame(std::uint32_t now_ms, bool refresh = false) {
    log.Advance(state, ro::ui::ChatBlockLogAnswer{.refresh = refresh}, now_ms);
  }
};

}  // namespace

TEST_CASE("Blocages : l'entrée est au staff, sur un joueur seulement", "[app][chat_block_log]") {
  CHECK(Offers(EntityFamily::kPlayer, /*staff=*/true));
  CHECK_FALSE(Offers(EntityFamily::kPlayer, /*staff=*/false));
  for (const EntityFamily family : {EntityFamily::kSelf, EntityFamily::kMonster, EntityFamily::kNpc,
                                    EntityFamily::kGroundItem, EntityFamily::kOther}) {
    CHECK_FALSE(Offers(family, /*staff=*/true));
  }
}

TEST_CASE("Blocages : l'ouverture demande une fois, et attend", "[app][chat_block_log]") {
  Consulting probe;
  CHECK(probe.state.open);
  CHECK(probe.state.raise);
  CHECK(probe.log.view().subject == "Alice — AID 2000002");
  CHECK(probe.log.view().phase == ChatBlockLogPhase::kAsking);
  CHECK(probe.log.TakeRequest() == std::optional<std::uint32_t>(kPlayer));
  CHECK_FALSE(probe.log.TakeRequest().has_value());
  // Pas d'autre demande tant que la première attend, même si on insiste.
  probe.Frame(kStartMs + 1, /*refresh=*/true);
  CHECK_FALSE(probe.log.TakeRequest().has_value());
}

TEST_CASE("Blocages : la réponse d'un autre joueur est jetée", "[app][chat_block_log]") {
  Consulting probe;
  probe.log.Note(Found(kOtherPlayer));
  CHECK(probe.log.view().phase == ChatBlockLogPhase::kAsking);
  CHECK(probe.log.view().rows.empty());
  probe.log.Note(Found(kPlayer));
  CHECK(probe.log.view().phase == ChatBlockLogPhase::kReady);
  REQUIRE(probe.log.view().rows.size() == 2);
  // L'ordre du serveur, le plus récent en haut.
  CHECK(probe.log.view().rows[0].command == "@unmute");
  CHECK(probe.log.view().rows[0].author == "Garde");
  CHECK(probe.log.view().rows[0].duration.empty());
  CHECK(probe.log.view().rows[1].command == "@mute");
  CHECK(probe.log.view().rows[1].author == "Sentinelle");
  CHECK(probe.log.view().rows[1].duration.find("60") != std::string::npos);
  CHECK_FALSE(probe.log.view().rows[1].when.empty());
}

TEST_CASE("Blocages : chaque statut a sa phase", "[app][chat_block_log]") {
  Consulting probe;
  ChatBlocksReply reply;
  reply.aid = kPlayer;
  for (const auto& [status, phase] :
       {std::pair{ChatBlocksStatus::kDenied, ChatBlockLogPhase::kDenied},
        std::pair{ChatBlocksStatus::kMissing, ChatBlockLogPhase::kMissing},
        std::pair{ChatBlocksStatus::kLogOff, ChatBlockLogPhase::kLogOff},
        std::pair{ChatBlocksStatus::kReadFailed, ChatBlockLogPhase::kReadFailed},
        std::pair{ChatBlocksStatus::kFound, ChatBlockLogPhase::kReady}}) {
    reply.status = status;
    probe.log.Note(reply);
    CHECK(probe.log.view().phase == phase);
    CHECK(probe.log.view().rows.empty());
  }
}

TEST_CASE("Blocages : une attente sans réponse finit par se dire", "[app][chat_block_log]") {
  Consulting probe;
  probe.Frame(kStartMs + kChatBlockLogWaitMs - 1);
  CHECK(probe.log.view().phase == ChatBlockLogPhase::kAsking);
  probe.Frame(kStartMs + kChatBlockLogWaitMs);
  CHECK(probe.log.view().phase == ChatBlockLogPhase::kSilent);
  // « Redemander » repart d'une attente neuve.
  static_cast<void>(probe.log.TakeRequest());
  probe.Frame(kStartMs + kChatBlockLogWaitMs + 1, /*refresh=*/true);
  CHECK(probe.log.view().phase == ChatBlockLogPhase::kAsking);
  CHECK(probe.log.TakeRequest() == std::optional<std::uint32_t>(kPlayer));
}

TEST_CASE("Blocages : « Actualiser » vide la liste et redemande", "[app][chat_block_log]") {
  Consulting probe;
  static_cast<void>(probe.log.TakeRequest());
  probe.log.Note(Found(kPlayer));
  probe.Frame(kStartMs + 1, /*refresh=*/true);
  CHECK(probe.log.view().phase == ChatBlockLogPhase::kAsking);
  CHECK(probe.log.view().rows.empty());
  CHECK(probe.log.TakeRequest() == std::optional<std::uint32_t>(kPlayer));
}

TEST_CASE("Blocages : la fenêtre fermée oublie le joueur", "[app][chat_block_log]") {
  Consulting probe;
  probe.state.open = false;
  probe.Frame(kStartMs + 1);
  CHECK(probe.log.aid() == 0);
  CHECK_FALSE(probe.log.TakeRequest().has_value());
  // Une réponse tardive ne rouvre rien.
  probe.log.Note(Found(kPlayer));
  CHECK(probe.log.view().rows.empty());
}

TEST_CASE("Blocages : une date s'écrit en année, mois, jour, heure et minute",
          "[app][chat_block_log]") {
  const std::string text = moonclient::ChatBlockDateText(kMutedAt);
  // « AAAA-MM-JJ HH:MM » : le fuseau de ce poste décale l'heure, pas la forme.
  constexpr std::size_t kDateLength = 16;
  REQUIRE(text.size() == kDateLength);
  CHECK(text.substr(0, 4) == "2025");
  CHECK(text[4] == '-');
  CHECK(text[10] == ' ');
  CHECK(text[13] == ':');
}
