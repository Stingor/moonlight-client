// La fenêtre des joueurs bloqués, dans la partie. Voir l'en-tête.

#include "chat/block_list_route.h"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/whisper_blocks.h"

#include "chat/block_list_view.h"
#include "chat/whisper_block_memory.h"
#include "game/whisper_stage.h"
#include "voice/voice_mode.h"
#include "world/click_action.h"

namespace moonclient {
namespace {

// Le nom est-il bloqué chez le serveur, ou rangé pour l'être ?
[[nodiscard]] bool WhispersBlocked(const WhisperStage& whispers, std::string_view name) {
  const WhisperBlockMemory* const memory = whispers.block_memory();
  return whispers.blocks().Blocks(name) ||
         (memory != nullptr && std::ranges::find(memory->names(), name) != memory->names().end());
}

}  // namespace

ro::ui::BlockListView ComposeBlockList(const WhisperStage& whispers, const VoiceMode& voice,
                                       bool connected) {
  const WhisperBlockMemory* const memory = whispers.block_memory();
  const std::vector<std::string> voice_blocked = voice.BlockedNames();
  return ComposeBlockList(BlockListFacts{
      .whispers = whispers.blocks().names(),
      .kept_whispers = memory != nullptr ? memory->names() : std::span<const std::string>(),
      .whispers_kept = memory != nullptr && memory->kept(),
      .whispers_from_everyone = whispers.blocks().blocks_everyone(),
      .voice_blocked = voice_blocked,
      .voice_hears =
          [&voice](std::string_view name) { return voice.PeerOf(name) == VoiceMode::Peer::kHeard; },
      .connected = connected,
      .voice_online = voice.online()});
}

void FollowBlockListAnswer(const ro::ui::BlockListAnswer& answer, ro::net::Connection& link,
                           WhisperStage& whispers, VoiceMode& voice) {
  using ro::sim::WhisperBlockWay;
  using Verb = ro::ui::BlockListVerb;
  const auto whisper = [&](WhisperBlockWay way) {
    if (link.is_open()) {
      ReportRefusal(ro::net::RequestWhisperBlock(link, whispers.blocks(), answer.name, way));
    }
  };
  const auto everyone = [&](WhisperBlockWay way) {
    if (link.is_open()) {
      ReportRefusal(ro::net::RequestWhisperBlockEveryone(link, way));
    }
  };
  switch (answer.verb) {
    case Verb::kNothing:
      return;
    case Verb::kBlock:
      whisper(WhisperBlockWay::kBlock);
      voice.Block(answer.name, true);
      return;
    case Verb::kBlockWhispers:
      whisper(WhisperBlockWay::kBlock);
      return;
    case Verb::kAllowWhispers:
      whisper(WhisperBlockWay::kAllow);
      return;
    case Verb::kBlockVoice:
      voice.Block(answer.name, true);
      return;
    case Verb::kAllowVoice:
      voice.Block(answer.name, false);
      return;
    case Verb::kAllowBoth:
      if (WhispersBlocked(whispers, answer.name)) {
        whisper(WhisperBlockWay::kAllow);
      }
      voice.Block(answer.name, false);
      return;
    case Verb::kBlockEveryone:
      everyone(WhisperBlockWay::kBlock);
      return;
    case Verb::kAllowEveryone:
      everyone(WhisperBlockWay::kAllow);
      return;
  }
}

}  // namespace moonclient
