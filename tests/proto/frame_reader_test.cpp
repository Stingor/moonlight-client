#include "ro/proto/frame_reader.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::FrameReader;
using ro::proto::FrameStatus;

namespace {

// 0x0064, la demande de connexion : 55 octets, longueur fixe.
constexpr std::uint16_t kFixedOpcode = 0x0064;
constexpr std::size_t kFixedSize = 55;

// 0x0069 : longueur variable, portée par les deux octets suivant l'opcode.
constexpr std::uint16_t kVariableOpcode = 0x0069;

std::vector<std::uint8_t> FixedFrame(std::uint8_t filler) {
  std::vector<std::uint8_t> bytes(kFixedSize, filler);
  bytes[0] = static_cast<std::uint8_t>(kFixedOpcode & 0xFF);
  bytes[1] = static_cast<std::uint8_t>(kFixedOpcode >> 8);
  return bytes;
}

std::vector<std::uint8_t> VariableFrame(std::uint16_t announced, std::uint8_t filler) {
  std::vector<std::uint8_t> bytes(announced, filler);
  bytes[0] = static_cast<std::uint8_t>(kVariableOpcode & 0xFF);
  bytes[1] = static_cast<std::uint8_t>(kVariableOpcode >> 8);
  bytes[2] = static_cast<std::uint8_t>(announced & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(announced >> 8);
  return bytes;
}

}  // namespace

TEST_CASE("Lecteur : une trame versée d'un coup ressort entière", "[proto][reader]") {
  FrameReader reader;
  reader.Append(FixedFrame(0x11));

  const auto taken = reader.Take();
  CHECK(taken.status == FrameStatus::kComplete);
  CHECK(taken.opcode == kFixedOpcode);
  CHECK(taken.frame.size() == kFixedSize);
  // La trame rendue porte bien SES octets, pas ceux d'un voisin.
  CHECK(taken.frame[10] == 0x11);
  CHECK(reader.pending() == 0);
}

TEST_CASE("Lecteur : une trame coupée en deux se recolle", "[proto][reader]") {
  // Le cas ordinaire d'un flux TCP : les blocs reçus n'ont aucun rapport avec
  // les limites des paquets.
  const std::vector<std::uint8_t> frame = FixedFrame(0x22);
  FrameReader reader;
  reader.Append(std::span(frame).first(20));
  CHECK(reader.Take().status == FrameStatus::kIncomplete);

  reader.Append(std::span(frame).subspan(20));
  const auto taken = reader.Take();
  CHECK(taken.status == FrameStatus::kComplete);
  CHECK(taken.frame.size() == kFixedSize);
}

TEST_CASE("Lecteur : plusieurs trames dans un même bloc sortent une à une", "[proto][reader]") {
  FrameReader reader;
  std::vector<std::uint8_t> block = FixedFrame(0x33);
  const std::vector<std::uint8_t> second = VariableFrame(30, 0x44);
  block.insert(block.end(), second.begin(), second.end());
  reader.Append(block);

  const auto first = reader.Take();
  REQUIRE(first.status == FrameStatus::kComplete);
  CHECK(first.opcode == kFixedOpcode);
  CHECK(first.frame.size() == kFixedSize);

  const auto next = reader.Take();
  REQUIRE(next.status == FrameStatus::kComplete);
  CHECK(next.opcode == kVariableOpcode);
  CHECK(next.frame.size() == 30);
  // Le second bloc porte SON remplissage : les deux ne se sont pas mélangés.
  CHECK(next.frame[10] == 0x44);

  CHECK(reader.Take().status == FrameStatus::kIncomplete);
  CHECK(reader.pending() == 0);
}

TEST_CASE("Lecteur : un flux versé octet par octet se découpe quand même", "[proto][reader]") {
  const std::vector<std::uint8_t> frame = FixedFrame(0x55);
  FrameReader reader;
  for (std::size_t index = 0; index + 1 < frame.size(); ++index) {
    reader.Append(std::span(frame).subspan(index, 1));
    CHECK(reader.Take().status == FrameStatus::kIncomplete);
  }
  reader.Append(std::span(frame).last(1));
  CHECK(reader.Take().status == FrameStatus::kComplete);
}

TEST_CASE("Lecteur : un opcode inconnu BLOQUE au lieu d'avancer", "[proto][reader]") {
  // Sans sa longueur, avancer d'un nombre arbitraire d'octets reviendrait à
  // chercher un opcode au milieu d'un corps de paquet. Le lecteur doit rendre
  // indéfiniment le même verdict : c'est ce qui force l'appelant à fermer au
  // lieu de se croire reparti.
  FrameReader reader;
  std::vector<std::uint8_t> bytes(100, 0x00);
  bytes[0] = 0xFF;
  bytes[1] = 0xFF;
  reader.Append(bytes);

  for (int attempt = 0; attempt < 3; ++attempt) {
    const auto taken = reader.Take();
    CHECK(taken.status == FrameStatus::kUnknownOpcode);
    CHECK(taken.frame.empty());
  }
  // Rien n'a été consommé.
  CHECK(reader.pending() == 100);
}

TEST_CASE("Lecteur : une trame corrompue bloque aussi", "[proto][reader]") {
  // Une longueur annoncée plus courte que l'en-tête ferait avancer de moins que
  // l'en-tête, ou de rien : le lecteur tournerait en boucle sur les mêmes
  // octets en croyant progresser.
  FrameReader reader;
  std::vector<std::uint8_t> bytes(100, 0x00);
  bytes[0] = static_cast<std::uint8_t>(kVariableOpcode & 0xFF);
  bytes[1] = static_cast<std::uint8_t>(kVariableOpcode >> 8);
  bytes[2] = 2;  // longueur annoncée : 2, impossible
  bytes[3] = 0;
  reader.Append(bytes);

  CHECK(reader.Take().status == FrameStatus::kCorrupt);
  CHECK(reader.pending() == 100);
}

TEST_CASE("Lecteur : le tampon ne grandit pas indéfiniment", "[proto][reader]") {
  // Sans repli, l'indice de lecture n'avancerait que vers l'avant et la mémoire
  // croîtrait sans fin sur une session longue — un client de jeu en tient
  // plusieurs heures. Le défaut ne se voit pas en une minute d'essai.
  FrameReader reader;
  for (int round = 0; round < 5000; ++round) {
    reader.Append(FixedFrame(static_cast<std::uint8_t>(round)));
    REQUIRE(reader.Take().status == FrameStatus::kComplete);
  }
  CHECK(reader.pending() == 0);
}

TEST_CASE("Lecteur : un reste partiel survit au repli du tampon", "[proto][reader]") {
  // Le repli déplace les octets non consommés ; les perdre couperait la trame
  // suivante en deux, et le symptôme n'apparaîtrait qu'après des milliers de
  // paquets.
  FrameReader reader;
  const std::vector<std::uint8_t> frame = FixedFrame(0x66);
  for (int round = 0; round < 2000; ++round) {
    reader.Append(frame);
    // On laisse traîner un début de trame suivante avant de consommer.
    reader.Append(std::span(frame).first(7));
    REQUIRE(reader.Take().status == FrameStatus::kComplete);
    CHECK(reader.pending() == 7);
    // Puis on complète ce début, qui doit ressortir intact.
    reader.Append(std::span(frame).subspan(7));
    const auto taken = reader.Take();
    REQUIRE(taken.status == FrameStatus::kComplete);
    CHECK(taken.frame.size() == kFixedSize);
    CHECK(taken.frame[30] == 0x66);
  }
}

TEST_CASE("Lecteur : les octets annoncés bruts ne sont pas découpés", "[proto][reader]") {
  // Tout le flux n'est pas encadré : le serveur de personnages répond à la
  // demande d'entrée par un identifiant de compte NU. Le découper reviendrait à
  // lire un opcode dans un numéro de compte, puis à consommer un nombre
  // arbitraire d'octets.
  constexpr std::size_t kBareSize = 4;
  FrameReader reader;
  reader.ExpectRaw(kBareSize);

  std::vector<std::uint8_t> stream = {0x7B, 0x80, 0x1E, 0x00};
  const std::vector<std::uint8_t> frame = FixedFrame(0x77);
  stream.insert(stream.end(), frame.begin(), frame.end());
  reader.Append(stream);

  // Tant que le brut n'est pas retiré, il n'y a rien à découper.
  CHECK(reader.Take().status == FrameStatus::kIncomplete);

  const std::vector<std::uint8_t> raw = reader.TakeRaw();
  REQUIRE(raw.size() == kBareSize);
  CHECK(raw[0] == 0x7B);
  CHECK(reader.expected_raw() == 0);

  // La trame qui suivait sort intacte.
  const auto taken = reader.Take();
  CHECK(taken.status == FrameStatus::kComplete);
  CHECK(taken.opcode == kFixedOpcode);
  CHECK(taken.frame.size() == kFixedSize);
}

TEST_CASE("Lecteur : des octets bruts incomplets attendent", "[proto][reader]") {
  // Les rendre amputés donnerait un identifiant de compte tronqué, et laisserait
  // le reste dans le flux à la place d'un opcode.
  constexpr std::size_t kBareSize = 4;
  FrameReader reader;
  reader.ExpectRaw(kBareSize);
  const std::vector<std::uint8_t> partial = {0x7B, 0x80};
  reader.Append(partial);

  CHECK(reader.TakeRaw().empty());
  CHECK(reader.expected_raw() == kBareSize);

  const std::vector<std::uint8_t> rest = {0x1E, 0x00};
  reader.Append(rest);
  CHECK(reader.TakeRaw().size() == kBareSize);
}

TEST_CASE("Lecteur : un lecteur vide attend, il n'invente rien", "[proto][reader]") {
  FrameReader reader;
  const auto taken = reader.Take();
  CHECK(taken.status == FrameStatus::kIncomplete);
  CHECK(taken.frame.empty());
  CHECK(reader.pending() == 0);
}

TEST_CASE("Lecteur : il se souvient des dernières trames découpées", "[proto][reader]") {
  // Un opcode inconnu ne se lit presque jamais comme la faute du paquet qui le
  // porte : c'est le PRÉCÉDENT dont la longueur annoncée n'était pas celle que
  // le serveur a écrite. Sans ce souvenir, rien ne le désigne, et il faut
  // recapturer le flux pour savoir quoi corriger.
  FrameReader reader;
  reader.Append(FixedFrame(0x11));
  reader.Append(VariableFrame(12, 0x22));
  REQUIRE(reader.Take().status == FrameStatus::kComplete);
  REQUIRE(reader.Take().status == FrameStatus::kComplete);

  const std::vector<FrameReader::Recent> recent = reader.recent_frames();
  REQUIRE(recent.size() == 2);
  CHECK(recent[0].opcode == kFixedOpcode);
  CHECK(recent[0].size == kFixedSize);
  CHECK(recent[1].opcode == kVariableOpcode);
  CHECK(recent[1].size == 12);
}

TEST_CASE("Lecteur : le souvenir garde les dernières, dans l'ordre", "[proto][reader]") {
  // Une session de jeu découpe des centaines de milliers de trames ; les garder
  // toutes serait une fuite. Ce qui compte est le VOISINAGE de la rupture.
  FrameReader reader;
  constexpr int kSent = 20;
  for (int index = 0; index < kSent; ++index) {
    reader.Append(VariableFrame(static_cast<std::uint16_t>(10 + index), 0x33));
    REQUIRE(reader.Take().status == FrameStatus::kComplete);
  }

  const std::vector<FrameReader::Recent> recent = reader.recent_frames();
  REQUIRE(!recent.empty());
  CHECK(recent.size() < static_cast<std::size_t>(kSent));
  // La dernière du souvenir est la dernière découpée, et elles se suivent.
  CHECK(recent.back().size == static_cast<std::size_t>(10 + kSent - 1));
  for (std::size_t step = 1; step < recent.size(); ++step) {
    CHECK(recent[step].size == recent[step - 1].size + 1);
  }
}

TEST_CASE("Lecteur : une trame refusée n'entre pas dans le souvenir", "[proto][reader]") {
  // Le souvenir sert à nommer ce qui a MARCHÉ avant la rupture. Y faire figurer
  // la rupture elle-même la ferait passer pour un précédent valide.
  FrameReader reader;
  reader.Append(FixedFrame(0x11));
  REQUIRE(reader.Take().status == FrameStatus::kComplete);
  reader.Append(std::vector<std::uint8_t>{0x0B, 0x06, 0x00, 0x00});
  REQUIRE(reader.Take().status != FrameStatus::kComplete);

  const std::vector<FrameReader::Recent> recent = reader.recent_frames();
  REQUIRE(recent.size() == 1);
  CHECK(recent[0].opcode == kFixedOpcode);
}
