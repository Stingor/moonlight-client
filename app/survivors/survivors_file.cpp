#include "survivors/survivors_file.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

constexpr std::string_view kEndingSignature = "MSVE";
// La version 1 s'arrête aux Zeny ; elle se lit encore, en partie libre, sans
// carte tombée ni glaçage.
constexpr std::uint16_t kFirstEndingVersion = 1;
constexpr std::uint16_t kEndingVersion = 2;
// La taille écrite d'une entrée de récapitulatif (id u16, valeur u64), d'une
// carte portée (id u16, niveau u8) et d'une carte tombée (id u16) : une liste
// annoncée plus longue que les octets restants est refusée, sans rien
// réserver.
constexpr std::size_t kCountBytes = sizeof(std::uint16_t) + sizeof(std::uint64_t);
constexpr std::size_t kCardBytes = sizeof(std::uint16_t) + sizeof(std::uint8_t);
constexpr std::size_t kFoundCardBytes = sizeof(std::uint16_t);

void WriteCounts(ro::core::ByteWriter& out, const std::vector<ro::sim::SurvivorsCount>& counts) {
  out.U16(static_cast<std::uint16_t>(counts.size()));
  for (const ro::sim::SurvivorsCount& count : counts) {
    out.U16(count.id);
    out.U64(count.value);
  }
}

[[nodiscard]] bool ReadCounts(ro::core::ByteReader& in,
                              std::vector<ro::sim::SurvivorsCount>& counts) {
  const std::size_t announced = in.U16();
  if (announced * kCountBytes > in.remaining()) return false;
  counts.reserve(announced);
  for (std::size_t index = 0; index < announced; ++index) {
    const std::uint16_t id = in.U16();
    counts.push_back(ro::sim::SurvivorsCount{.id = id, .value = in.U64()});
  }
  return in.ok();
}

[[nodiscard]] ro::core::Result<std::vector<std::uint8_t>> ReadBytes(
    const std::filesystem::path& path) {
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return ro::core::Fail(ro::core::ErrorCode::kNotFound, ro::core::Utf8FromPath(path));
  }
  // Le flux s'ouvre par le chemin large : un dossier hors de la page ANSI
  // resterait introuvable par un nom étroit.
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return ro::core::Fail(ro::core::ErrorCode::kIoFailure, ro::core::Utf8FromPath(path));
  return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(stream),
                                   std::istreambuf_iterator<char>()};
}

[[nodiscard]] ro::core::Result<void> WriteBytes(const std::filesystem::path& path,
                                                const std::vector<std::uint8_t>& bytes) {
  return ro::core::WriteFileAtomically(
      path, std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

}  // namespace

std::filesystem::path SurvivorsSavePath(const std::filesystem::path& client_root,
                                        std::string_view file_name) {
  return client_root / ro::core::PathFromUtf8(kSaveFolder) /
         ro::core::PathFromUtf8(kSurvivorsSaveFolder) / ro::core::PathFromUtf8(file_name);
}

std::vector<std::uint8_t> EncodeSurvivorsEnding(const SurvivorsEnding& ending) {
  std::vector<std::uint8_t> bytes;
  ro::core::ByteWriter out(bytes);
  out.Text(kEndingSignature);
  out.U16(kEndingVersion);
  out.U16(ending.class_id);
  const ro::sim::SurvivorsTally& tally = ending.tally;
  out.U32(tally.survived_steps);
  out.U32(tally.kills);
  out.U16(tally.boss_kills);
  out.U32(tally.kill_points);
  out.U16(tally.level);
  out.U32(tally.exp_total);
  out.I32(tally.hp_left);
  out.U64(tally.damage_dealt);
  out.U32(tally.gems_gathered);
  out.U16(tally.foes_alive);
  out.U8(static_cast<std::uint8_t>(tally.outcome));
  out.U16(tally.elite_kills);
  out.U16(tally.chests_opened);
  out.U16(tally.evolutions);
  WriteCounts(out, ending.recap.kills_by_foe);
  WriteCounts(out, ending.recap.damage_by_skill);
  out.U8(static_cast<std::uint8_t>(ending.cards.size()));
  for (const ro::sim::OwnedCard& card : ending.cards) {
    out.U16(card.card_item_id);
    out.U8(card.level);
  }
  out.U32(ending.zeny);
  out.U8(static_cast<std::uint8_t>(ending.board.kind));
  out.I32(ending.board.week.year);
  out.U8(ending.board.week.week);
  out.U16(static_cast<std::uint16_t>(ending.recap.cards_found.size()));
  for (const std::uint16_t card_item_id : ending.recap.cards_found) out.U16(card_item_id);
  out.U64(ending.recap.chills);
  return bytes;
}

ro::core::Result<SurvivorsEnding> DecodeSurvivorsEnding(std::span<const std::uint8_t> bytes) {
  using ro::core::ErrorCode;
  using ro::core::Fail;
  ro::core::ByteReader in(bytes);
  if (in.FixedString(kEndingSignature.size()) != kEndingSignature) {
    return in.ok() ? Fail(ErrorCode::kBadSignature, "fin de partie : signature")
                   : Fail(ErrorCode::kUnexpectedEnd, "fin de partie : en-tête");
  }
  const std::uint16_t version = in.U16();
  if (version != kFirstEndingVersion && version != kEndingVersion) {
    return in.ok() ? Fail(ErrorCode::kUnsupportedVersion, "fin de partie : version")
                   : Fail(ErrorCode::kUnexpectedEnd, "fin de partie : version");
  }
  SurvivorsEnding ending;
  ending.class_id = in.U16();
  ro::sim::SurvivorsTally& tally = ending.tally;
  tally.survived_steps = in.U32();
  tally.kills = in.U32();
  tally.boss_kills = in.U16();
  tally.kill_points = in.U32();
  tally.level = in.U16();
  tally.exp_total = in.U32();
  tally.hp_left = in.I32();
  tally.damage_dealt = in.U64();
  tally.gems_gathered = in.U32();
  tally.foes_alive = in.U16();
  const std::uint8_t outcome = in.U8();
  tally.elite_kills = in.U16();
  tally.chests_opened = in.U16();
  tally.evolutions = in.U16();
  if (!ReadCounts(in, ending.recap.kills_by_foe) || !ReadCounts(in, ending.recap.damage_by_skill)) {
    return Fail(ErrorCode::kUnexpectedEnd, "fin de partie : récapitulatif");
  }
  const std::size_t card_count = in.U8();
  if (card_count * kCardBytes > in.remaining()) {
    return Fail(ErrorCode::kUnexpectedEnd, "fin de partie : cartes");
  }
  for (std::size_t index = 0; index < card_count; ++index) {
    const std::uint16_t card_item_id = in.U16();
    ending.cards.push_back(ro::sim::OwnedCard{.card_item_id = card_item_id, .level = in.U8()});
  }
  ending.zeny = in.U32();
  std::uint8_t board_kind = 0;
  if (version == kEndingVersion) {
    board_kind = in.U8();
    ending.board.week.year = in.I32();
    ending.board.week.week = in.U8();
    const std::size_t found_count = in.U16();
    if (found_count * kFoundCardBytes > in.remaining()) {
      return Fail(ErrorCode::kUnexpectedEnd, "fin de partie : cartes tombées");
    }
    ending.recap.cards_found.reserve(found_count);
    for (std::size_t index = 0; index < found_count; ++index) {
      ending.recap.cards_found.push_back(in.U16());
    }
    ending.recap.chills = in.U64();
  }
  if (!in.ok()) return Fail(ErrorCode::kUnexpectedEnd, "fin de partie : tronquée");
  if (board_kind > static_cast<std::uint8_t>(SurvivorsBoardKind::kRanked)) {
    return Fail(ErrorCode::kCorruptData, "fin de partie : tableau inconnu");
  }
  ending.board.kind = static_cast<SurvivorsBoardKind>(board_kind);
  if (outcome > static_cast<std::uint8_t>(ro::sim::SurvivorsOutcome::kAbandoned)) {
    return Fail(ErrorCode::kCorruptData, "fin de partie : issue inconnue");
  }
  tally.outcome = static_cast<ro::sim::SurvivorsOutcome>(outcome);
  if (in.remaining() != 0) return Fail(ErrorCode::kCorruptData, "fin de partie : octets en trop");
  return ending;
}

ro::core::Result<void> SaveLastRun(const std::filesystem::path& client_root,
                                   const ro::sim::SurvivorsJournal& journal) {
  return WriteBytes(SurvivorsSavePath(client_root, kLastRunFileName),
                    ro::sim::EncodeSurvivorsJournal(journal));
}

ro::core::Result<void> SaveInterruptedRun(const std::filesystem::path& client_root,
                                          const ro::sim::SurvivorsJournal& journal,
                                          const SurvivorsEnding& ending) {
  if (auto saved = SaveLastRun(client_root, journal); !saved) return saved;
  return WriteBytes(SurvivorsSavePath(client_root, kInterruptedEndingFileName),
                    EncodeSurvivorsEnding(ending));
}

std::optional<SurvivorsEnding> TakeInterruptedEnding(const std::filesystem::path& client_root) {
  const std::filesystem::path path = SurvivorsSavePath(client_root, kInterruptedEndingFileName);
  ro::core::Result<std::vector<std::uint8_t>> bytes = ReadBytes(path);
  if (!bytes) {
    if (bytes.error().code != ro::core::ErrorCode::kNotFound) {
      ro::core::LogWarning("partie interrompue : {}", ro::core::Describe(bytes.error()));
    }
    return std::nullopt;
  }
  std::error_code removed;
  std::filesystem::remove(path, removed);
  ro::core::Result<SurvivorsEnding> ending = DecodeSurvivorsEnding(*bytes);
  if (!ending) {
    ro::core::LogWarning("partie interrompue : {} illisible ({})", ro::core::Utf8FromPath(path),
                         ro::core::Describe(ending.error()));
    return std::nullopt;
  }
  return std::move(*ending);
}

ro::core::Result<ro::sim::SurvivorsJournal> ReadJournalFile(const std::filesystem::path& path) {
  ro::core::Result<std::vector<std::uint8_t>> bytes = ReadBytes(path);
  if (!bytes) return std::unexpected(std::move(bytes.error()));
  return ro::sim::DecodeSurvivorsJournal(*bytes);
}

}  // namespace moonclient
