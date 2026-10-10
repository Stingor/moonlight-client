#include "ro/net/whisper_blocks.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/social_packets.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

using core::Tr;
using sim::WhisperBlockResult;
using sim::WhisperBlockWay;

// Les couleurs du client d'origine : cyan pour la réponse à une demande, jaune
// pour la liste redite.
constexpr std::uint32_t kAnswerColor = 0x00FFFF;
constexpr std::uint32_t kListColor = 0xFFFF00;

// Le sens tel que le fil le code, dans les deux directions.
[[nodiscard]] WhisperBlockWay WayOf(std::uint8_t type) {
  return type == proto::WhisperBlockRequest::kBlock ? WhisperBlockWay::kBlock
                                                    : WhisperBlockWay::kAllow;
}
[[nodiscard]] std::uint8_t TypeOf(WhisperBlockWay way) {
  return way == WhisperBlockWay::kBlock ? proto::WhisperBlockRequest::kBlock
                                        : proto::WhisperBlockRequest::kAllow;
}

void Say(sim::World& world, std::string line, std::uint32_t color, std::uint32_t now_ms) {
  world.RecordChat(sim::ChatKind::kServer, 0, std::move(line), now_ms, color);
}

// LA PHRASE D'UNE RÉPONSE PAR NOM, ou rien : le blocage réussi se tait.
[[nodiscard]] std::optional<std::string> AnswerLine(const sim::WhisperBlockAnswer& answer) {
  const char* name = answer.name.c_str();
  const auto code = static_cast<WhisperBlockResult>(answer.result);
  if (answer.way == WhisperBlockWay::kBlock) {
    switch (code) {
      case WhisperBlockResult::kDone:
        return std::nullopt;
      case WhisperBlockResult::kListFull:
        return core::Printed(
            Tr("%s : échec du blocage des chuchotements. La liste de blocage est pleine."), name);
      case WhisperBlockResult::kFailed:
      default:
        return core::Printed(Tr("%s : échec du blocage des chuchotements."), name);
    }
  }
  switch (code) {
    case WhisperBlockResult::kDone:
      return core::Printed(Tr("%s : chuchotements acceptés."), name);
    case WhisperBlockResult::kFailed:
      return core::Printed(Tr("%s : échec du déblocage, ce nom n'était pas bloqué."), name);
    case WhisperBlockResult::kListFull:
    default:
      return core::Printed(Tr("%s : échec du déblocage."), name);
  }
}

[[nodiscard]] std::string EveryoneLine(WhisperBlockWay way, bool failed) {
  if (way == WhisperBlockWay::kBlock) {
    return failed ? Tr("Échec du blocage de tous les chuchotements : ils l'étaient déjà.")
                  : Tr("Les chuchotements de tous les personnages sont bloqués.");
  }
  return failed ? Tr("Échec : aucun chuchotement n'était bloqué.")
                : Tr("Les chuchotements de tous les personnages sont autorisés.");
}

core::Result<void> SendEncoded(Connection& link, core::Result<std::vector<std::uint8_t>> frame) {
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace

FrameOutcome ApplyWhisperBlockFrame(const Frame& frame, sim::WhisperBlockList& blocks,
                                    sim::World& world, std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ZcSettingWhisperPc::kOpcode:
      return Decoded<proto::ZcSettingWhisperPc>(frame, [&](const auto& packet) {
        const sim::WhisperBlockAnswer answer = blocks.Settle(WayOf(packet.type), packet.result);
        // UNE RÉPONSE SANS DEMANDE ne sait pas de qui elle parle : elle se tait.
        if (answer.name.empty()) {
          return FrameOutcome::kIgnored;
        }
        if (std::optional<std::string> line = AnswerLine(answer); line.has_value()) {
          Say(world, std::move(*line), kAnswerColor, now_ms);
        }
        return FrameOutcome::kApplied;
      });

    case proto::ZcSettingWhisperState::kOpcode:
      return Decoded<proto::ZcSettingWhisperState>(frame, [&](const auto& packet) {
        const WhisperBlockWay way = WayOf(packet.type);
        const bool failed = packet.result != 0;
        blocks.SettleEveryone(way, failed);
        Say(world, EveryoneLine(way, failed), kAnswerColor, now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ZcWhisperList::kOpcode:
      return Decoded<proto::ZcWhisperList>(frame, [&](const auto& packet) {
        std::vector<std::string> names;
        names.reserve(packet.names.size());
        for (const proto::ZcWhisperListSub& entry : packet.names) {
          names.push_back(core::Utf8FromCp1252(entry.name));
        }
        blocks.Replace(std::move(names));
        if (blocks.names().empty()) {
          Say(world, Tr("La liste de blocage est vide."), kListColor, now_ms);
          return FrameOutcome::kApplied;
        }
        Say(world, Tr("-Liste de blocage-"), kListColor, now_ms);
        for (const std::string& name : blocks.names()) {
          Say(world, name, kListColor, now_ms);
        }
        return FrameOutcome::kApplied;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestWhisperBlock(Connection& link, sim::WhisperBlockList& blocks,
                                       std::string_view name, sim::WhisperBlockWay way) {
  const std::optional<std::string> wire = core::ExactCp1252FromUtf8(name);
  // ⚠ LE DERNIER OCTET DU CHAMP EST LE ZÉRO FINAL : le serveur l'y pose
  // d'office, et un nom qui le remplit perdrait sa dernière lettre.
  if (name.empty() || !wire.has_value() || wire->size() >= proto::WhisperBlockRequest::kNameSize) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "nom de personnage impossible à bloquer tel quel");
  }
  proto::WhisperBlockRequest request;
  request.name = *wire;
  request.type = TypeOf(way);
  if (core::Result<void> sent = SendEncoded(link, proto::Encode(request)); !sent) {
    return sent;
  }
  blocks.Expect(std::string(name), way);
  return {};
}

core::Result<void> RequestWhisperBlockEveryone(Connection& link, sim::WhisperBlockWay way) {
  proto::CzSettingWhisperState request;
  request.type = static_cast<std::int8_t>(TypeOf(way));
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestWhisperBlockList(Connection& link) {
  return SendEncoded(link, proto::Encode(proto::WhisperBlockListRequest{}));
}

}  // namespace ro::net
