// LES DOCUMENTS D'ÉDITION DE CARTE EN MORCEAUX — `CZ 0x0F45`, `ZC 0x0F47` : le
// découpage, la lecture, et le réassemblage (ADR 0124).

#include "ro/proto/map_edit_packets.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::DecodeMapEditDoc;
using ro::proto::DecodeMapEditPut;
using ro::proto::EncodeMapEditDoc;
using ro::proto::EncodeMapEditPut;
using ro::proto::IsMapEditName;
using ro::proto::kMapEditDocChunkBytes;
using ro::proto::kMapEditMostTextBytes;
using ro::proto::kMapEditMostZippedBytes;
using ro::proto::kMapEditPutChunkBytes;
using ro::proto::MapEditAssembler;
using ro::proto::MapEditChunk;

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint32_t kTextBytes = 9000;
constexpr std::uint32_t kRevision = 7;
// La tête de `CZ 0x0F45` et celle de `ZC 0x0F47`, en octets.
constexpr std::size_t kPutHeadBytes = 36;
constexpr std::size_t kDocHeadBytes = 60;
// La période du motif d'un flux d'essai : un nombre premier, qu'aucune taille de
// morceau ne divise.
constexpr std::size_t kPatternPeriod = 251;

// Un flux dont chaque octet dit son rang : un morceau déplacé se voit.
Bytes Stream(std::size_t size) {
  Bytes bytes(size);
  for (std::size_t index = 0; index < size; ++index) {
    bytes[index] = static_cast<std::uint8_t>(index % kPatternPeriod);
  }
  return bytes;
}

std::vector<MapEditChunk> DocChunks(const Bytes& zipped) {
  std::vector<MapEditChunk> chunks;
  for (const Bytes& frame :
       EncodeMapEditDoc("gon_fild01", kRevision, "Stingor", kTextBytes, zipped)) {
    auto chunk = DecodeMapEditDoc(frame);
    REQUIRE(chunk.has_value());
    chunks.push_back(std::move(*chunk));
  }
  return chunks;
}

}  // namespace

TEST_CASE("Éditions de carte : un nom de carte doit pouvoir nommer un fichier",
          "[proto][map_edit]") {
  CHECK(IsMapEditName("prontera"));
  CHECK(IsMapEditName("gon_fild01"));
  CHECK(IsMapEditName("1@tower"));
  CHECK_FALSE(IsMapEditName(""));
  CHECK_FALSE(IsMapEditName("../prontera"));
  CHECK_FALSE(IsMapEditName("pront\\era"));
  CHECK_FALSE(IsMapEditName("prontera.gat"));
  // Seize octets ne laissent pas la place du zéro final.
  CHECK_FALSE(IsMapEditName("abcdefghijklmnop"));
  CHECK(IsMapEditName("abcdefghijklmno"));
}

TEST_CASE("Éditions de carte : l'envoi se coupe en morceaux d'un kilo-octet, dans l'ordre",
          "[proto][map_edit]") {
  const Bytes zipped = Stream(2 * kMapEditPutChunkBytes + 10);
  const std::vector<Bytes> frames = EncodeMapEditPut("prontera", kRevision, kTextBytes, zipped);
  REQUIRE(frames.size() == 3);
  Bytes joined;
  for (std::size_t rank = 0; rank < frames.size(); ++rank) {
    // 🔴 LE TAMPON DE RÉCEPTION DU SERVEUR vaut 2 Kio : aucune trame ne l'atteint.
    CHECK(frames[rank].size() <= kPutHeadBytes + kMapEditPutChunkBytes);
    CHECK(frames[rank][0] == 0x45);
    CHECK(frames[rank][1] == 0x0F);
    const auto chunk = DecodeMapEditPut(frames[rank]);
    REQUIRE(chunk.has_value());
    CHECK(chunk->map == "prontera");
    CHECK(chunk->revision == kRevision);
    CHECK(chunk->zipped_size == zipped.size());
    CHECK(chunk->text_size == kTextBytes);
    CHECK(chunk->offset == joined.size());
    joined.insert(joined.end(), chunk->bytes.begin(), chunk->bytes.end());
  }
  CHECK(joined == zipped);
}

TEST_CASE("Éditions de carte : rien ne part hors des plafonds ou sous un faux nom",
          "[proto][map_edit]") {
  const Bytes small = Stream(10);
  CHECK(EncodeMapEditPut("../x", 0, kTextBytes, small).empty());
  CHECK(EncodeMapEditPut("prontera", 0, kTextBytes, {}).empty());
  CHECK(EncodeMapEditPut("prontera", 0, 0, small).empty());
  CHECK(EncodeMapEditPut("prontera", 0, kMapEditMostTextBytes + 1, small).empty());
  CHECK(EncodeMapEditPut("prontera", 0, kTextBytes, Stream(kMapEditMostZippedBytes + 1)).empty());
}

TEST_CASE("Éditions de carte : une trame dont la longueur ment ne se lit pas",
          "[proto][map_edit]") {
  std::vector<Bytes> frames = EncodeMapEditDoc("prontera", 1, "", kTextBytes, Stream(40));
  REQUIRE(frames.size() == 1);
  CHECK(frames[0].size() == kDocHeadBytes + 40);
  REQUIRE(DecodeMapEditDoc(frames[0]).has_value());
  Bytes cut = frames[0];
  cut.pop_back();
  CHECK_FALSE(DecodeMapEditDoc(cut).has_value());
  // Un envoi n'est pas un document servi.
  CHECK_FALSE(
      DecodeMapEditDoc(EncodeMapEditPut("prontera", 1, kTextBytes, Stream(40))[0]).has_value());
}

TEST_CASE("Éditions de carte : le document servi se réassemble", "[proto][map_edit]") {
  const Bytes zipped = Stream(2 * kMapEditDocChunkBytes + 300);
  std::vector<MapEditChunk> chunks = DocChunks(zipped);
  REQUIRE(chunks.size() == 3);
  MapEditAssembler assembler;
  CHECK(assembler.Feed(chunks[0]) == MapEditAssembler::Step::kPending);
  CHECK(assembler.Feed(chunks[1]) == MapEditAssembler::Step::kPending);
  REQUIRE(assembler.Feed(chunks[2]) == MapEditAssembler::Step::kComplete);
  const ro::proto::AssembledMapEdit document = assembler.Take();
  CHECK(document.map == "gon_fild01");
  CHECK(document.revision == kRevision);
  CHECK(document.author == "Stingor");
  CHECK(document.text_size == kTextBytes);
  CHECK(document.zipped == zipped);
}

TEST_CASE("Éditions de carte : un morceau hors d'ordre jette le transfert", "[proto][map_edit]") {
  std::vector<MapEditChunk> chunks = DocChunks(Stream(3 * kMapEditDocChunkBytes));
  REQUIRE(chunks.size() == 3);
  MapEditAssembler assembler;
  REQUIRE(assembler.Feed(chunks[0]) == MapEditAssembler::Step::kPending);
  CHECK(assembler.Feed(chunks[2]) == MapEditAssembler::Step::kRefused);
  CHECK_FALSE(assembler.refusal().empty());
  // Le transfert est jeté : la suite ne le reprend pas.
  CHECK(assembler.Feed(chunks[1]) == MapEditAssembler::Step::kRefused);
  // Un nouveau décalage 0 en ouvre un autre.
  CHECK(assembler.Feed(chunks[0]) == MapEditAssembler::Step::kPending);
  CHECK(assembler.Feed(chunks[1]) == MapEditAssembler::Step::kPending);
  CHECK(assembler.Feed(chunks[2]) == MapEditAssembler::Step::kComplete);
}

TEST_CASE("Éditions de carte : un morceau d'une autre révision ne se greffe pas",
          "[proto][map_edit]") {
  std::vector<MapEditChunk> chunks = DocChunks(Stream(2 * kMapEditDocChunkBytes));
  MapEditAssembler assembler;
  REQUIRE(assembler.Feed(chunks[0]) == MapEditAssembler::Step::kPending);
  chunks[1].revision = kRevision + 1;
  CHECK(assembler.Feed(chunks[1]) == MapEditAssembler::Step::kRefused);
}

TEST_CASE("Éditions de carte : un morceau qui déborde de son total est refusé",
          "[proto][map_edit]") {
  MapEditChunk chunk{.map = "prontera",
                     .revision = 1,
                     .zipped_size = 10,
                     .text_size = kTextBytes,
                     .offset = 5,
                     .author = {},
                     .bytes = Stream(6)};
  MapEditAssembler assembler;
  CHECK(assembler.Feed(chunk) == MapEditAssembler::Step::kRefused);
  chunk.offset = 0;
  chunk.zipped_size = kMapEditMostZippedBytes + 1;
  CHECK(assembler.Feed(chunk) == MapEditAssembler::Step::kRefused);
}
