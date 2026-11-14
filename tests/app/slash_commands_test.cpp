// LES COMMANDES QUE LE CHAT PROPOSE : la table « / » d'accord avec les lecteurs
// de commandes dans les deux sens, et la table « @ » filtrée par le groupe.

#include "chat/slash_commands.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/ascii.h"
#include "ro/core/case_fold.h"
#include "ro/net/chat_target.h"

#include "chat/announce_command.h"
#include "chat/chat_command_catalogue.h"
#include "chat/client_command_words.h"
#include "chat/emotion_commands.h"
#include "chat/native_slash_commands.h"
#include "chat/navigation_command.h"
#include "chat/opening_commands.h"
#include "chat/player_command.h"
#include "chat/rank_command.h"
#include "chat/server_atcommands.h"
#include "chat/setting_toggle_command.h"
#include "chat/show_equipment_command.h"
#include "chat/stat_raise_command.h"
#include "chat/talk_filter_words.h"
#include "chat/whisper_block_command.h"
#include "game/database_compose.h"
#include "game/market_route.h"
#include "items/warp_memo_report.h"
#include "world/follow_rules.h"

namespace {

using moonclient::ChatCommandCatalogue;
using ro::ui::ChatCommandHint;

// Les mots que `SubmitChat` compare lui-même, à l'égalité exacte.
constexpr auto kExactWords = std::to_array<std::string_view>({
    moonclient::kFixedCameraCommand,    moonclient::kExpandedZoomCommand,
    moonclient::kLightmapCommand,       moonclient::kFogCommand,
    moonclient::kCashShopButtonCommand, moonclient::kNoShiftCommand,
    moonclient::kNoShiftShortCommand,   moonclient::kNoCtrlCommand,
    moonclient::kNoCtrlShortCommand,    moonclient::kShoppingCommand,
    moonclient::kShoppingShortCommand,  moonclient::kCallCommand,
    moonclient::kChatRoomCommand,       moonclient::kChatRoomHangulCommand,
    moonclient::kChatRoomLeaveCommand,  moonclient::kShowShopCommand,
    moonclient::kBattleModeCommand,     moonclient::kBattleModeCommandLong,
    moonclient::kMusicCommand,          moonclient::kSoundCommand,
    moonclient::kSoundCommandLong,      moonclient::kEffectsShownCommand,
    moonclient::kMinEffectCommand,      moonclient::kMinEffectAltCommand,
    moonclient::kSkyEffectsCommand,     moonclient::kMacroWindowCommand,
    moonclient::kSaveChatCommand,
});

// L'ORACLE : la ligne est-elle prise par un lecteur de commande du client ?
//
// 🔴 C'EST LE MIROIR DE `SubmitChat`, lecteur par lecteur, dans son ordre. Un
// lecteur ajouté là s'ajoute ici : c'est le contrat que l'en-tête de
// `slash_commands.h` annonce. Les lecteurs qui vivent dans l'exécutable sont
// relus par leurs mots, exposés en en-tête.
[[nodiscard]] bool ClientReadsLine(const std::string& line) {
  if (line.empty() || line.front() != '/') {
    return false;
  }
  if (ro::net::IsSpokenLine(line) || moonclient::EmotionCommand(line).has_value() ||
      moonclient::ReadPlayerCommand(line).kind != moonclient::PlayerCommandKind::kNone ||
      moonclient::ReadSettingToggle(line).has_value() ||
      ro::core::AsciiEqualsFolded(line, moonclient::kBattleChatCommand) ||
      std::ranges::find(kExactWords, line) != kExactWords.end() ||
      moonclient::IsShowEquipmentCommand(line) || line.starts_with(moonclient::kScaleCommand) ||
      line.starts_with(moonclient::kScaleAltCommand)) {
    return true;
  }
  const std::string_view first = std::string_view(line).substr(0, line.find_first_of(" \t"));
  if (std::ranges::any_of(
          moonclient::kTalkFilterWords,
          [&](std::string_view word) { return ro::core::EqualsFolded(line, word); }) ||
      std::ranges::any_of(moonclient::kCraftAtlasCommands, [&](std::string_view word) {
        return ro::core::AsciiEqualsFolded(first, word);
      })) {
    return true;
  }
  return moonclient::ReadNavigationCommand(line).kind != moonclient::NavigationCommandKind::kNone ||
         moonclient::ReadAnnounceCommand(line).kind != moonclient::AnnounceCommandKind::kNone ||
         moonclient::ReadFollowCommand(line).has_value() ||
         moonclient::ReadWhisperBlockCommand(line).kind !=
             moonclient::WhisperBlockCommandKind::kNone ||
         moonclient::IsRankingBoardCommand(line) || moonclient::ReadRankCommand(line).has_value() ||
         moonclient::IsMarketCommand(line) || moonclient::ReadDatabaseCommand(line).has_value() ||
         moonclient::IsBlockListCommand(line) || moonclient::IsWarpMemoCommand(line) ||
         moonclient::ReadStatRaiseCommand(line).kind != moonclient::StatRaiseKind::kNone;
}

// Le nom, seul ou suivi d'un argument : une commande qui en attend un ne se lit
// qu'avec lui.
[[nodiscard]] bool ClientReads(std::string_view name) {
  const std::string bare(name);
  return ClientReadsLine(bare) || ClientReadsLine(bare + " Bob 1") || ClientReadsLine(bare + " 1");
}

// Toutes les graphies « / » que la liste propose, barre oblique comprise.
[[nodiscard]] std::vector<std::string> OfferedSpellings(std::span<const ChatCommandHint> hints) {
  std::vector<std::string> spelled;
  for (const ChatCommandHint& hint : hints) {
    if (hint.sigil != '/') {
      continue;
    }
    spelled.push_back("/" + std::string(hint.name));
    std::string_view aliases = hint.aliases;
    while (!aliases.empty()) {
      const std::size_t end = aliases.find(' ');
      spelled.push_back("/" + std::string(aliases.substr(0, end)));
      aliases.remove_prefix(end == std::string_view::npos ? aliases.size() : end + 1);
    }
  }
  return spelled;
}

[[nodiscard]] bool Offered(const std::vector<std::string>& spelled, std::string_view name) {
  return std::ranges::any_of(
      spelled, [&](const std::string& one) { return ro::core::AsciiEqualsFolded(one, name); });
}

// Le catalogue le plus large : celui d'un administrateur.
[[nodiscard]] const ChatCommandCatalogue& AdminCatalogue() {
  static ChatCommandCatalogue catalogue = [] {
    ChatCommandCatalogue made;
    made.Refresh(99, {});
    return made;
  }();
  return catalogue;
}

}  // namespace

TEST_CASE("Chaque commande « / » proposée est lue par le client") {
  for (const std::string& name : OfferedSpellings(AdminCatalogue().hints())) {
    INFO(name);
    CHECK(ClientReads(name));
  }
}

TEST_CASE("Chaque commande « / » que le client lit est proposée") {
  std::vector<std::string_view> candidates(moonclient::NativeSlashCommandNames().begin(),
                                           moonclient::NativeSlashCommandNames().end());
  // Les mots à nous, que le client d'origine ne connaît pas.
  candidates.insert(candidates.end(), kExactWords.begin(), kExactWords.end());
  candidates.insert(candidates.end(), moonclient::kCraftAtlasCommands.begin(),
                    moonclient::kCraftAtlasCommands.end());
  candidates.insert(candidates.end(), moonclient::kBlockListCommands.begin(),
                    moonclient::kBlockListCommands.end());
  for (const std::string_view word : moonclient::kTalkFilterWords) {
    candidates.push_back(word);
  }
  for (const moonclient::EmotionCommandRow& row : moonclient::EmotionCommandRows()) {
    candidates.push_back(row.name);
  }
  candidates.insert(
      candidates.end(),
      {moonclient::kBattleChatCommand, moonclient::kScaleCommand, moonclient::kScaleAltCommand,
       moonclient::kFollowCommand, moonclient::kRankingBoardCommand, moonclient::kMarketCommand,
       moonclient::kDatabaseCommand, ro::net::kGuildCommand, ro::net::kGuildCommandLong,
       ro::net::kClanCommand, ro::net::kClanCommandLong, "/bc", "/w", "/whisper", "/e"});
  const std::vector<std::string> offered = OfferedSpellings(AdminCatalogue().hints());
  for (const std::string_view name : candidates) {
    if (!ClientReads(name)) {
      continue;
    }
    INFO(name);
    CHECK(Offered(offered, name));
  }
}

TEST_CASE("Un joueur ne se voit proposer que les commandes de son groupe") {
  ChatCommandCatalogue player;
  player.Refresh(0, {});
  const auto has = [](const ChatCommandCatalogue& catalogue, char sigil, std::string_view name) {
    return std::ranges::any_of(catalogue.hints(), [&](const ChatCommandHint& hint) {
      return hint.sigil == sigil && hint.name == name;
    });
  };
  for (const moonclient::ServerAtCommand& command : moonclient::ServerAtCommands()) {
    INFO(command.name);
    CHECK(has(player, '@', command.name) == (command.min_group_level == 0));
    CHECK(has(AdminCatalogue(), '@', command.name) ==
          (command.min_group_level != moonclient::kNoGroupLevel));
  }
  // Les annonces du staff ne se proposent pas à un joueur.
  CHECK_FALSE(has(player, '/', "b"));
  CHECK(has(AdminCatalogue(), '/', "b"));
  CHECK(has(player, '@', "autoloot"));

  // Un niveau intermédiaire : ce qu'il atteint, et rien au-dessus.
  ChatCommandCatalogue guard;
  guard.Refresh(40, {});
  for (const moonclient::ServerAtCommand& command : moonclient::ServerAtCommands()) {
    INFO(command.name);
    CHECK(has(guard, '@', command.name) == moonclient::ServerCommandOffered(command, 40));
  }
}

TEST_CASE("Le serveur donne ses alias et ses formes") {
  const auto commands = moonclient::ServerAtCommands();
  REQUIRE(std::ranges::is_sorted(commands, {}, &moonclient::ServerAtCommand::name));
  const auto mapmove =
      std::ranges::find(commands, std::string_view("mapmove"), &moonclient::ServerAtCommand::name);
  REQUIRE(mapmove != commands.end());
  CHECK(mapmove->aliases.find("warp") != std::string_view::npos);
  CHECK_FALSE(mapmove->params.empty());
}

TEST_CASE("Le menu des macros puise dans la même liste") {
  ChatCommandCatalogue player;
  player.Refresh(0, {});
  const auto groups = player.macro_groups();
  REQUIRE(groups.size() >= 2);
  std::size_t items = 0;
  std::size_t server = 0;
  for (const ro::ui::ChatMacroCommandGroup& group : groups) {
    CHECK_FALSE(group.items.empty());
    for (const ro::ui::ChatMacroCommand& item : group.items) {
      ++items;
      if (item.text.starts_with('@')) {
        ++server;
      }
      CHECK(item.help.size() <= moonclient::kMacroHelpBytes + std::string_view("…").size());
    }
  }
  const auto zero = std::ranges::count_if(moonclient::ServerAtCommands(), [](const auto& command) {
    return command.min_group_level == 0;
  });
  CHECK(server == static_cast<std::size_t>(zero));
  // Les commandes du client, sans les émoticônes, qui ont leur grille.
  const auto client =
      std::ranges::count_if(moonclient::SlashCommands(), [](const auto& c) { return !c.staff; });
  CHECK(items == static_cast<std::size_t>(client) + server);
  // Une commande qui attend un argument se pose avec son espace.
  bool deal = false;
  for (const auto& group : groups) {
    for (const auto& item : group.items) {
      deal = deal || item.text == "/deal ";
    }
  }
  CHECK(deal);
}
