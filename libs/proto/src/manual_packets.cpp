#include "ro/proto/manual_packets.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/chat_limits.h"
#include "ro/proto/packet_lengths.h"
#include "ro/proto/parsed_packets.h"
#include "ro/proto/skill_text_packets.h"

#include "layout_holds.h"
#include "named_chat.h"
#include "opcode_only.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

// Le gestionnaire du serveur qui lit chaque paquet. C'est LUI qui l'identifie,
// pas son opcode : le serveur réattribue les opcodes d'une version à l'autre.
constexpr std::string_view kEnterMapHandler = "clif_parse_WantToConnection";
constexpr std::string_view kMapLoadedHandler = "clif_parse_LoadEndAck";
constexpr std::string_view kRestartHandler = "clif_parse_Restart";
// 🔴 LE NOM DU SERVEUR EST « AutoRevive », pas « Resurrect » : l'autre nom est
// celui de la compétence, que ce paquet n'emploie pas.
constexpr std::string_view kAutoReviveHandler = "clif_parse_AutoRevive";
constexpr std::string_view kRememberWarpPointHandler = "clif_parse_RequestMemo";
// Le type de réapparition, à l'unique décalage que le serveur déclare.
constexpr std::array<std::uint16_t, 1> kRestartOffsets = {2};
constexpr std::string_view kWalkHandler = "clif_parse_WalkToXY";
constexpr std::string_view kActionHandler = "clif_parse_ActionRequest";
constexpr std::string_view kPickupHandler = "clif_parse_TakeItem";
constexpr std::string_view kNameHandler = "clif_parse_GetCharNameRequest";
constexpr std::string_view kCreatorNameHandler = "clif_parse_SolveCharName";
// Le serveur ne déclare aucun champ pour lui : il lit sa structure à la main.
constexpr std::string_view kBugReportHandler = "clif_parse_moonlight_bug_report";
constexpr std::string_view kTurnHandler = "clif_parse_ChangeDir";
constexpr std::string_view kUseItemHandler = "clif_parse_UseItem";
constexpr std::string_view kUnequipHandler = "clif_parse_UnequipItem";
constexpr std::string_view kDropItemHandler = "clif_parse_DropItem";
// 🔴 PAS `clif_parse_traitstatus_up`, qui lit 0x0B24 et monte les
// caractéristiques de métier. Le nom du gestionnaire est ce qui sépare les deux.
constexpr std::string_view kStatUpHandler = "clif_parse_StatusUp";
constexpr std::string_view kStorageStoreHandler = "clif_parse_MoveToKafra";
constexpr std::string_view kStorageTakeHandler = "clif_parse_MoveFromKafra";
constexpr std::string_view kCloseStorageHandler = "clif_parse_CloseKafra";
constexpr std::string_view kNpcNextHandler = "clif_parse_NpcNextClicked";
constexpr std::string_view kPartyChatHandler = "clif_parse_PartyMessage";
constexpr std::string_view kGuildChatHandler = "clif_parse_GuildMessage";
constexpr std::string_view kClanChatHandler = "clif_parse_clan_chat";
constexpr std::string_view kWhisperHandler = "clif_parse_WisMessage";
constexpr std::string_view kNpcMenuHandler = "clif_parse_NpcSelectMenu";
constexpr std::string_view kChatHandler = "clif_parse_GlobalMessage";
constexpr std::string_view kEmotionHandler = "clif_parse_cash_emotion_use";
constexpr std::string_view kSkillUpHandler = "clif_parse_SkillUp";
constexpr std::array<std::uint16_t, 1> kSkillUpOffsets = {2};

// Les décalages auxquels le serveur lit l'entrée en jeu, recopiés de la ligne
// qui l'enregistre. Ils sont recoupés avec elle avant tout envoi.
constexpr std::array<std::uint16_t, 5> kEnterMapOffsets = {2, 6, 10, 14, 18};

// La marche n'a qu'un champ : les trois octets de destination, juste après
// l'opcode. Le recoupement porte donc surtout sur ce décalage, parce qu'un des
// opcodes acceptés le place ailleurs.
constexpr std::array<std::uint16_t, 1> kWalkOffsets = {2};

// L'action : la cible, puis l'action elle-même, un octet.
constexpr std::array<std::uint16_t, 2> kActionOffsets = {2, 6};
// Le ramassage n'a que l'objet.
constexpr std::array<std::uint16_t, 1> kPickupOffsets = {2};
constexpr std::array<std::uint16_t, 1> kNameOffsets = {2};
// Le regard, puis le corps — et QUATRE octets de bourrage entre les deux.
constexpr std::array<std::uint16_t, 2> kTurnOffsets = {4, 9};
constexpr std::array<std::uint16_t, 2> kUseItemOffsets = {2, 4};
constexpr std::array<std::uint16_t, 1> kUnequipOffsets = {2};
constexpr std::array<std::uint16_t, 2> kDropItemOffsets = {2, 4};
constexpr std::array<std::uint16_t, 2> kStatUpOffsets = {2, 4};
// Les deux transferts de coffre lisent pareil : la case, puis la quantité sur
// quatre octets. ⚠ CE RECOUPEMENT NE TRANCHE PAS LE NUMÉRO — le serveur
// déclare une douzaine d'opcodes vers ces deux gestionnaires, tous à ces
// mêmes décalages. Ce qui tranche est le dernier bloc que PACKETVER traverse,
// recoupé avec la table de longueurs du client.
constexpr std::array<std::uint16_t, 2> kStorageMoveOffsets = {2, 4};
constexpr std::array<std::uint16_t, 1> kNpcNextOffsets = {2};
// Longueur en 2, texte en 4 — comme le message public.
constexpr std::array<std::uint16_t, 2> kPartyChatOffsets = {2, 4};
constexpr std::array<std::uint16_t, 2> kGuildChatOffsets = {2, 4};
// Le clan lit aux mêmes places : `RFIFOCP(fd, 4)` sur `RFIFOW(fd, 2) - 4`.
constexpr std::array<std::uint16_t, 2> kClanChatOffsets = {2, 4};
// Longueur en 2, destinataire en 4, texte en 28 : le nom occupe vingt-quatre
// octets entre les deux.
constexpr std::array<std::uint16_t, 3> kWhisperOffsets = {2, 4, 28};
constexpr std::array<std::uint16_t, 2> kNpcMenuOffsets = {2, 6};

constexpr std::string_view kTickHandler = "clif_parse_TickSend";
constexpr std::array<std::uint16_t, 1> kTickOffsets = {2};

// Les compétences : le NIVEAU d'abord, puis l'identifiant, puis ce qui est
// visé. Le recoupement porte surtout sur ces deux premiers décalages, que
// l'ordre des champs contredit.
constexpr std::string_view kSkillOnTargetHandler = "clif_parse_UseSkillToId";
constexpr std::array<std::uint16_t, 3> kSkillOnTargetOffsets = {2, 4, 6};
constexpr std::string_view kSkillOnGroundHandler = "clif_parse_UseSkillToPos";
constexpr std::array<std::uint16_t, 5> kSkillOnGroundOffsets = {2, 4, 6, 8, 10};

// La demande au sol accompagnée d'un texte : les mêmes quatre champs, puis le
// texte au cinquième décalage.
constexpr std::string_view kSkillOnGroundWithTextHandler = "clif_parse_UseSkillToPosMoreInfo";

// Le onzième octet de la demande au sol : le serveur ne le lit pas.
constexpr std::uint8_t kGroundSkillUnread = 0;

// La parole : la LONGUEUR d'abord, puis le texte. Le serveur lit les deux, ce
// qui n'arrive à aucun autre paquet écrit ici — les autres ont une taille fixe
// que leur table déclare.
constexpr std::array<std::uint16_t, 2> kChatOffsets = {2, 4};

core::Error LayoutDrift() {
  return core::Error{ErrorCode::kUnsupportedVersion,
                     "la disposition écrite pour ce paquet ne correspond plus à ce que le "
                     "serveur déclare lire"};
}

}  // namespace

bool MatchesServerLayout(std::uint16_t opcode, std::string_view handler,
                         std::span<const std::uint16_t> field_offsets) {
  const auto declared = ParsedPacketFor(opcode);
  if (!declared.has_value() || declared->handler != handler) {
    return false;
  }
  return std::ranges::equal(declared->field_offsets, field_offsets);
}

core::Result<std::vector<std::uint8_t>> Encode(const EnterMapRequest& request) {
  if (!FixedLayoutHolds<EnterMapRequest>(kEnterMapHandler, kEnterMapOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(EnterMapRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(EnterMapRequest::kOpcode);
  writer.U32(request.account_id);
  writer.U32(request.character_id);
  writer.U32(request.login_id1);
  writer.U32(request.client_tick);
  writer.U8(request.sex);
  return frame;
}

// La parole nommée : voir `named_chat.h`.
core::Result<std::vector<std::uint8_t>> EncodeNamedChat(
    std::uint16_t opcode, std::string_view handler, std::span<const std::uint16_t> offsets,
    std::string_view speaker, std::string_view text, std::optional<std::size_t> body_max) {
  if (!MatchesServerLayout(opcode, handler, offsets)) {
    return std::unexpected(LayoutDrift());
  }
  const auto declared = PacketLength(opcode);
  if (!declared.has_value() || *declared != kVariableLength) {
    return std::unexpected(LayoutDrift());
  }

  std::string body;
  body.reserve(speaker.size() + ChatRequest::kNameSeparator.size() + text.size());
  body += speaker;
  body += ChatRequest::kNameSeparator;
  body += text;
  if (text.size() > kChatTextMax || (body_max.has_value() && body.size() > *body_max)) {
    return std::unexpected(core::Error{ErrorCode::kInvalidArgument,
                                       "message trop long pour ce que le serveur accepte"});
  }

  const std::size_t length = ChatRequest::kHeaderSize + body.size() + 1;
  std::vector<std::uint8_t> frame;
  frame.reserve(length);
  ByteWriter writer(frame);
  writer.U16(opcode);
  writer.U16(static_cast<std::uint16_t>(length));
  for (const char letter : body) {
    writer.U8(static_cast<std::uint8_t>(letter));
  }
  writer.U8(0);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const ChatRequest& request) {
  // ⚠ UNE COMMANDE AUSSI : le serveur la diffuse quand le groupe n'y a pas droit,
  // et l'encodeur ne sait pas s'il y a droit (`chat_limits.h`).
  return EncodeNamedChat(ChatRequest::kOpcode, kChatHandler, kChatOffsets, request.speaker,
                         request.text, kChatNamedBodyMax);
}

core::Result<std::vector<std::uint8_t>> Encode(const SkillUpRequest& request) {
  if (!FixedLayoutHolds<SkillUpRequest>(kSkillUpHandler, kSkillUpOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(SkillUpRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(SkillUpRequest::kOpcode);
  writer.U16(request.skill_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const EmotionRequest& request) {
  // Aucun décalage déclaré : le gestionnaire du serveur lit ses deux champs en
  // dur. Seuls le gestionnaire et la longueur peuvent donc être recoupés.
  if (!FixedLayoutHolds<EmotionRequest>(kEmotionHandler, std::array<std::uint16_t, 0>{})) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(EmotionRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(EmotionRequest::kOpcode);
  writer.U16(request.pack_id);
  writer.U16(request.emotion_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const MapLoadedNotice& /*notice*/) {
  // Aucun champ, donc aucun décalage à recouper : seule la longueur peut mentir.
  if (!FixedLayoutHolds<MapLoadedNotice>(kMapLoadedHandler, std::array<std::uint16_t, 0>{})) {
    return std::unexpected(LayoutDrift());
  }
  return OpcodeOnlyFrame<MapLoadedNotice>();
}

core::Result<std::vector<std::uint8_t>> Encode(const RestartRequest& request) {
  if (!FixedLayoutHolds<RestartRequest>(kRestartHandler, kRestartOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(RestartRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(RestartRequest::kOpcode);
  writer.U8(request.what);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const AutoReviveRequest& /*request*/) {
  // Aucun champ, donc aucun décalage à recouper : seule la longueur peut mentir.
  if (!FixedLayoutHolds<AutoReviveRequest>(kAutoReviveHandler, std::array<std::uint16_t, 0>{})) {
    return std::unexpected(LayoutDrift());
  }
  return OpcodeOnlyFrame<AutoReviveRequest>();
}

core::Result<std::vector<std::uint8_t>> Encode(const RememberWarpPointRequest& /*request*/) {
  // Aucun champ, donc aucun décalage à recouper : seule la longueur peut mentir.
  if (!FixedLayoutHolds<RememberWarpPointRequest>(kRememberWarpPointHandler,
                                                  std::array<std::uint16_t, 0>{})) {
    return std::unexpected(LayoutDrift());
  }
  return OpcodeOnlyFrame<RememberWarpPointRequest>();
}

core::Result<std::vector<std::uint8_t>> Encode(const WalkRequest& request) {
  if (!FixedLayoutHolds<WalkRequest>(kWalkHandler, kWalkOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(WalkRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(WalkRequest::kOpcode);
  for (const std::uint8_t byte : request.packed_destination) {
    writer.U8(byte);
  }
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const ActionRequest& request) {
  if (!FixedLayoutHolds<ActionRequest>(kActionHandler, kActionOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(ActionRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ActionRequest::kOpcode);
  writer.U32(request.target_id);
  writer.U8(request.action);
  return frame;
}

bool Decode(std::span<const std::uint8_t> frame, SpecialEffectNotice& packet) {
  if (frame.size() != SpecialEffectNotice::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != SpecialEffectNotice::kOpcode) {
    return false;
  }
  const std::uint32_t actor_id = reader.U32();
  const std::uint32_t effect_id = reader.U32();
  if (!reader.ok()) {
    return false;
  }
  packet.actor_id = actor_id;
  packet.effect_id = effect_id;
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, ValuedEffectNotice& packet) {
  if (frame.size() != ValuedEffectNotice::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ValuedEffectNotice::kOpcode) {
    return false;
  }
  const std::uint32_t actor_id = reader.U32();
  const std::uint32_t effect_id = reader.U32();
  const std::int32_t value = reader.I32();
  if (!reader.ok()) {
    return false;
  }
  packet.actor_id = actor_id;
  packet.effect_id = effect_id;
  packet.value = value;
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, DevotionListNotice& packet) {
  if (frame.size() != DevotionListNotice::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != DevotionListNotice::kOpcode) {
    return false;
  }
  DevotionListNotice read;
  read.devoter_id = reader.U32();
  for (std::uint32_t& devotee_id : read.devotee_ids) {
    devotee_id = reader.U32();
  }
  read.range = reader.I16();
  if (!reader.ok()) {
    return false;
  }
  packet = read;
  return true;
}

core::Result<std::vector<std::uint8_t>> Encode(const PickupRequest& request) {
  if (!FixedLayoutHolds<PickupRequest>(kPickupHandler, kPickupOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(PickupRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(PickupRequest::kOpcode);
  writer.U32(request.item_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const TurnRequest& request) {
  if (!FixedLayoutHolds<TurnRequest>(kTurnHandler, kTurnOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  // ⚠ ÉCRIT AUX DÉCALAGES, ET NON À LA SUITE. Les deux champs ne sont pas
  // voisins : partir d'une trame nulle et les poser à leur place est la seule
  // écriture où le bourrage ne se compte pas à la main.
  std::vector<std::uint8_t> frame;
  frame.reserve(TurnRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(TurnRequest::kOpcode);
  frame.resize(TurnRequest::kWireSize, std::uint8_t{0});
  frame[kTurnOffsets[0]] = request.head_direction;
  frame[kTurnOffsets[1]] = request.direction;
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const NameRequest& request) {
  if (!FixedLayoutHolds<NameRequest>(kNameHandler, kNameOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(NameRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(NameRequest::kOpcode);
  writer.U32(request.actor_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const CreatorNameRequest& request) {
  if (!FixedLayoutHolds<CreatorNameRequest>(kCreatorNameHandler, kNameOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(CreatorNameRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(CreatorNameRequest::kOpcode);
  writer.U32(request.character_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const BugReportRequest& request) {
  if (!VariableLayoutHolds<BugReportRequest>(kBugReportHandler, std::array<std::uint16_t, 0>{})) {
    return std::unexpected(LayoutDrift());
  }
  const std::size_t length =
      BugReportRequest::kHeaderSize + request.context.size() + request.message.size();
  if (length > std::numeric_limits<std::uint16_t>::max()) {
    return std::unexpected(
        core::Error{ErrorCode::kInvalidArgument, "rapport de bug trop long pour une trame"});
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(length);
  ByteWriter writer(frame);
  writer.U16(BugReportRequest::kOpcode);
  writer.U16(static_cast<std::uint16_t>(length));
  writer.U8(request.category);
  writer.U16(static_cast<std::uint16_t>(request.context.size()));
  for (const char letter : request.context) {
    writer.U8(static_cast<std::uint8_t>(letter));
  }
  for (const char letter : request.message) {
    writer.U8(static_cast<std::uint8_t>(letter));
  }
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const UseItemRequest& request) {
  if (!FixedLayoutHolds<UseItemRequest>(kUseItemHandler, kUseItemOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(UseItemRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(UseItemRequest::kOpcode);
  writer.U16(request.client_slot);
  writer.U32(request.account_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const UnequipItemRequest& request) {
  if (!FixedLayoutHolds<UnequipItemRequest>(kUnequipHandler, kUnequipOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(UnequipItemRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(UnequipItemRequest::kOpcode);
  writer.U16(request.client_slot);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const DropItemRequest& request) {
  if (!FixedLayoutHolds<DropItemRequest>(kDropItemHandler, kDropItemOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(DropItemRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(DropItemRequest::kOpcode);
  writer.U16(request.client_slot);
  writer.U16(request.amount);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const StatUpRequest& request) {
  if (!FixedLayoutHolds<StatUpRequest>(kStatUpHandler, kStatUpOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(StatUpRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(StatUpRequest::kOpcode);
  writer.U16(request.parameter);
  // ⚠ UN OCTET, et c'est ce que le serveur lit — `RFIFOB`, pas `RFIFOW`.
  writer.U8(request.amount);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const PartyChatRequest& request) {
  return EncodeNamedChat(PartyChatRequest::kOpcode, kPartyChatHandler, kPartyChatOffsets,
                         request.speaker, request.text, kChatNamedBodyMax);
}

core::Result<std::vector<std::uint8_t>> Encode(const GuildChatRequest& request) {
  // En champ de bataille, le relais du champ garde la ligne entière.
  return EncodeNamedChat(GuildChatRequest::kOpcode, kGuildChatHandler, kGuildChatOffsets,
                         request.speaker, request.text,
                         request.battlefield ? kChatNamedBodyMax : kChatGuildBodyMax);
}

core::Result<std::vector<std::uint8_t>> Encode(const ClanChatRequest& request) {
  // Le relais du clan ne coupe pas à cette taille : seul le rejet du texte joue.
  return EncodeNamedChat(ClanChatRequest::kOpcode, kClanChatHandler, kClanChatOffsets,
                         request.speaker, request.text, std::nullopt);
}

core::Result<std::vector<std::uint8_t>> Encode(const WhisperRequest& request) {
  if (!VariableLayoutHolds<WhisperRequest>(kWhisperHandler, kWhisperOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  if (request.recipient.empty()) {
    return std::unexpected(
        core::Error{ErrorCode::kInvalidArgument, "un chuchotement sans destinataire"});
  }
  // ⚠ LE NOM TIENT SUR VINGT-QUATRE OCTETS, ZÉRO COMPRIS. Un nom plus long
  // déborderait sur le texte, et le serveur lirait un destinataire tronqué suivi
  // d'un message ampute.
  if (request.recipient.size() >= WhisperRequest::kNameSize) {
    return std::unexpected(
        core::Error{ErrorCode::kInvalidArgument, "nom de destinataire trop long"});
  }
  if (request.text.size() > kChatTextMax) {
    return std::unexpected(core::Error{ErrorCode::kInvalidArgument,
                                       "message trop long pour ce que le serveur accepte"});
  }

  const std::size_t length = WhisperRequest::kHeaderSize + request.text.size() + 1;
  std::vector<std::uint8_t> frame;
  frame.reserve(length);
  ByteWriter writer(frame);
  writer.U16(WhisperRequest::kOpcode);
  writer.U16(static_cast<std::uint16_t>(length));
  for (std::size_t at = 0; at < WhisperRequest::kNameSize; ++at) {
    writer.U8(at < request.recipient.size() ? static_cast<std::uint8_t>(request.recipient[at]) : 0);
  }
  for (const char letter : request.text) {
    writer.U8(static_cast<std::uint8_t>(letter));
  }
  writer.U8(0);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const NpcNextRequest& request) {
  if (!FixedLayoutHolds<NpcNextRequest>(kNpcNextHandler, kNpcNextOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(NpcNextRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(NpcNextRequest::kOpcode);
  writer.U32(request.npc_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const NpcMenuChoice& request) {
  if (!FixedLayoutHolds<NpcMenuChoice>(kNpcMenuHandler, kNpcMenuOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  // ⚠ LE RANG NUL NE PART PAS D'ICI. Voir NpcMenuChoice : il fait éjecter le
  // joueur, et l'encodeur est le dernier endroit où l'on puisse encore refuser
  // sans rien casser.
  if (request.choice == 0) {
    return std::unexpected(
        core::Error{ErrorCode::kInvalidArgument,
                    "rang de menu nul : le serveur y verrait une manipulation du protocole"});
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(NpcMenuChoice::kWireSize);
  ByteWriter writer(frame);
  writer.U16(NpcMenuChoice::kOpcode);
  writer.U32(request.npc_id);
  writer.U8(request.choice);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const StorageStoreRequest& request) {
  if (!FixedLayoutHolds<StorageStoreRequest>(kStorageStoreHandler, kStorageMoveOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(StorageStoreRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(StorageStoreRequest::kOpcode);
  writer.U16(request.client_slot);
  writer.U32(static_cast<std::uint32_t>(request.amount));
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const StorageTakeRequest& request) {
  if (!FixedLayoutHolds<StorageTakeRequest>(kStorageTakeHandler, kStorageMoveOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(StorageTakeRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(StorageTakeRequest::kOpcode);
  writer.U16(request.client_slot);
  writer.U32(static_cast<std::uint32_t>(request.amount));
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const CloseStorageRequest& request) {
  static_cast<void>(request);
  // Aucun champ : le recoupement ne porte que sur le gestionnaire et la taille.
  if (!FixedLayoutHolds<CloseStorageRequest>(kCloseStorageHandler,
                                             std::array<std::uint16_t, 0>{})) {
    return std::unexpected(LayoutDrift());
  }
  return OpcodeOnlyFrame<CloseStorageRequest>();
}

core::Result<std::vector<std::uint8_t>> Encode(const TickRequest& request) {
  if (!FixedLayoutHolds<TickRequest>(kTickHandler, kTickOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(TickRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(TickRequest::kOpcode);
  writer.U32(request.client_tick);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const SkillOnTargetRequest& request) {
  if (!FixedLayoutHolds<SkillOnTargetRequest>(kSkillOnTargetHandler, kSkillOnTargetOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(SkillOnTargetRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(SkillOnTargetRequest::kOpcode);
  writer.U16(request.skill_level);
  writer.U16(request.skill_id);
  writer.U32(request.target_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const SkillOnGroundRequest& request) {
  if (!FixedLayoutHolds<SkillOnGroundRequest>(kSkillOnGroundHandler, kSkillOnGroundOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(SkillOnGroundRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(SkillOnGroundRequest::kOpcode);
  writer.U16(request.skill_level);
  writer.U16(request.skill_id);
  writer.U16(request.cell_x);
  writer.U16(request.cell_y);
  writer.U8(kGroundSkillUnread);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const SkillOnGroundWithTextRequest& request) {
  if (!FixedLayoutHolds<SkillOnGroundWithTextRequest>(kSkillOnGroundWithTextHandler,
                                                      kSkillOnGroundOffsets)) {
    return std::unexpected(LayoutDrift());
  }

  std::vector<std::uint8_t> frame;
  frame.reserve(SkillOnGroundWithTextRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(SkillOnGroundWithTextRequest::kOpcode);
  writer.U16(request.skill_level);
  writer.U16(request.skill_id);
  writer.U16(request.cell_x);
  writer.U16(request.cell_y);
  // ⚠ COUPÉ AVANT LE DERNIER OCTET, qui reste le zéro : le serveur lit une
  // chaîne C, et un texte qui remplirait tout déborderait sur ce qui suit.
  const std::size_t kept =
      std::min(request.text.size(), SkillOnGroundWithTextRequest::kTextSize - 1);
  for (std::size_t at = 0; at < SkillOnGroundWithTextRequest::kTextSize; ++at) {
    writer.U8(at < kept ? static_cast<std::uint8_t>(request.text[at]) : 0);
  }
  return frame;
}

// Le texte qui occupe la fin d'une trame, sans son zéro terminal s'il en a un.
//
// ⚠ LE ZÉRO EST DEDANS OU NON SELON LE PAQUET, et le garder ferait paraître un
// caractère invisible à la fin de chaque ligne — assez pour fausser une mesure
// de largeur sans jamais se voir.
[[nodiscard]] std::string TailText(std::span<const std::uint8_t> bytes) {
  std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  const std::size_t end = text.find('\0');
  return end == std::string::npos ? text : text.substr(0, end);
}

bool Decode(std::span<const std::uint8_t> frame, WhisperNotice& packet) {
  if (frame.size() < WhisperNotice::kHeaderSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != WhisperNotice::kOpcode) {
    return false;
  }
  const std::uint16_t declared = reader.U16();
  if (declared != frame.size()) {
    return false;
  }
  WhisperNotice read;
  read.sender_id = reader.U32();
  const std::span<const std::uint8_t> name = reader.Bytes(WhisperNotice::kNameSize);
  read.from_staff = reader.U8() != 0;
  if (!reader.ok()) {
    return false;
  }
  read.sender = TailText(name);
  read.message = TailText(frame.subspan(WhisperNotice::kHeaderSize));
  packet = std::move(read);
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, ColoredNotice& packet) {
  if (frame.size() < ColoredNotice::kHeaderSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ColoredNotice::kOpcode) {
    return false;
  }
  const std::uint16_t declared = reader.U16();
  if (declared != frame.size()) {
    return false;
  }
  ColoredNotice read;
  read.speaker_id = reader.U32();
  read.color = reader.U32();
  if (!reader.ok()) {
    return false;
  }
  read.message = TailText(frame.subspan(ColoredNotice::kHeaderSize));
  packet = std::move(read);
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, SkillUnitNotice& packet) {
  if (frame.size() != SkillUnitNotice::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != SkillUnitNotice::kOpcode) {
    return false;
  }
  // La longueur, que le serveur recopie en tête à l'imitation d'un paquet de
  // taille variable : la table la donne déjà, et une trame d'une autre taille a
  // été refusée plus haut.
  reader.Skip(sizeof(std::uint16_t));
  SkillUnitNotice read;
  read.unit_actor_id = reader.U32();
  read.creator_id = reader.U32();
  read.cell_x = reader.U16();
  read.cell_y = reader.U16();
  read.unit_id = reader.U32();
  read.range = reader.U8();
  read.visible = reader.U8();
  read.skill_level = reader.U8();
  if (!reader.ok()) {
    return false;
  }
  packet = read;
  return true;
}

}  // namespace ro::proto
