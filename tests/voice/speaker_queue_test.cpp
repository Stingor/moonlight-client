// LA FILE D'UN LOCUTEUR : la marge avant de jouer, l'ordre rétabli, la trame
// perdue reconstruite d'après la suivante, et le silence qui clôt la parole.

#include "ro/voice/speaker_queue.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::voice::SpeakerQueue;
using ro::voice::SpeakerStep;

namespace {

[[nodiscard]] std::vector<std::uint8_t> Packet(std::uint8_t tag) { return {tag}; }

}  // namespace

TEST_CASE("Voix : la file attend sa marge, puis rend dans l'ordre", "[voice][queue]") {
  SpeakerQueue queue;
  queue.Push(11, Packet(11));
  queue.Push(10, Packet(10));
  CHECK(queue.Pop().step == SpeakerStep::kSilent);
  queue.Push(12, Packet(12));

  for (std::uint8_t expected = 10; expected <= 12; ++expected) {
    const ro::voice::SpeakerFrame frame = queue.Pop();
    REQUIRE(frame.step == SpeakerStep::kPacket);
    CHECK(frame.packet == Packet(expected));
  }
  CHECK(queue.playing());
}

TEST_CASE("Voix : une trame perdue se refait d'après la suivante", "[voice][queue]") {
  SpeakerQueue queue;
  queue.Push(1, Packet(1));
  queue.Push(3, Packet(3));
  queue.Push(4, Packet(4));
  CHECK(queue.Pop().packet == Packet(1));
  const ro::voice::SpeakerFrame lost = queue.Pop();
  CHECK(lost.step == SpeakerStep::kLost);
  CHECK(lost.next == Packet(3));
  CHECK(queue.Pop().packet == Packet(3));
  // Arrivé trop tard : sa place est jouée.
  queue.Push(2, Packet(2));
  CHECK(queue.Pop().packet == Packet(4));
}

TEST_CASE("Voix : le numéro survit à son retour à zéro", "[voice][queue]") {
  SpeakerQueue queue;
  queue.Push(65534, Packet(1));
  queue.Push(65535, Packet(2));
  queue.Push(0, Packet(3));
  CHECK(queue.Pop().packet == Packet(1));
  CHECK(queue.Pop().packet == Packet(2));
  CHECK(queue.Pop().packet == Packet(3));
}

TEST_CASE("Voix : un silence clôt la parole, la reprise attend sa marge", "[voice][queue]") {
  SpeakerQueue queue;
  for (std::uint16_t at = 0; at < 3; ++at) queue.Push(at, Packet(1));
  for (int frame = 0; frame < 3; ++frame) static_cast<void>(queue.Pop());
  for (std::size_t frame = 0; frame < ro::voice::kSpeakerMostMissedFrames; ++frame) {
    CHECK(queue.Pop().step == SpeakerStep::kSilent);
  }
  CHECK_FALSE(queue.playing());
  queue.Push(200, Packet(9));
  CHECK(queue.Pop().step == SpeakerStep::kSilent);
}

TEST_CASE("Voix : la reprise après un blanc court se joue entière", "[voice][queue]") {
  // L'émetteur ne numérote que ce qu'il envoie : sa reprise suit le dernier paquet,
  // alors que la lecture a continué d'avancer pendant le blanc.
  constexpr std::uint8_t kFirstResumed = 7;
  constexpr int kBlankFrames = 3;
  SpeakerQueue queue;
  for (std::uint16_t at = 0; at < 3; ++at) queue.Push(at, Packet(1));
  for (int frame = 0; frame < 3; ++frame) static_cast<void>(queue.Pop());
  for (int frame = 0; frame < kBlankFrames; ++frame) {
    CHECK(queue.Pop().step == SpeakerStep::kSilent);
  }
  REQUIRE(queue.playing());

  queue.Push(3, Packet(kFirstResumed));
  queue.Push(4, Packet(8));
  queue.Push(5, Packet(9));
  const ro::voice::SpeakerFrame resumed = queue.Pop();
  REQUIRE(resumed.step == SpeakerStep::kPacket);
  CHECK(resumed.packet == Packet(kFirstResumed));
  CHECK(queue.Pop().packet == Packet(8));
  CHECK(queue.Pop().packet == Packet(9));
}

TEST_CASE("Voix : un paquet déjà joué ne relance pas la lecture", "[voice][queue]") {
  SpeakerQueue queue;
  for (std::uint16_t at = 0; at < 3; ++at) queue.Push(at, Packet(1));
  for (int frame = 0; frame < 3; ++frame) static_cast<void>(queue.Pop());
  CHECK(queue.Pop().step == SpeakerStep::kSilent);

  queue.Push(1, Packet(2));
  CHECK(queue.playing());
  CHECK(queue.Pop().step == SpeakerStep::kSilent);
}
