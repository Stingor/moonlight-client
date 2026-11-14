// LA LIAISON CONTRE UN FAUX SERVEUR VOIX, sur la boucle locale et sans UDP :
// l'authentification qui part, l'acceptation qui met en ligne, une voix
// entendue par le TCP, et le micro qui part quand la touche est tenue — après
// avoir dit au serveur qu'on parle —, qui tient le délai au relâchement, et qui
// part à la voix au-dessus du seuil.

#include "ro/voice/voice_link.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/socket.h"
#include "ro/voice/opus_codec.h"
#include "ro/voice/voice_engine.h"

using ro::net::ReceiveState;
using ro::net::TcpListener;
using ro::net::TcpSocket;
using ro::voice::FrameKind;
using ro::voice::FrameReader;
using ro::voice::StreamFrame;
using ro::voice::VoiceChannel;
using ro::voice::VoiceLink;
using ro::voice::VoiceLinkState;

namespace {

// Assez de pas pour qu'une liaison locale aboutisse, sans jamais dormir.
constexpr int kTurns = 20000;

struct FakeServer {
  TcpListener listener;
  std::optional<TcpSocket> client;
  FrameReader reader;
  std::vector<StreamFrame> received;

  void Pump() {
    if (!client.has_value()) {
      auto accepted = listener.Accept();
      REQUIRE(accepted.has_value());
      if (accepted->has_value()) client = std::move(**accepted);
      return;
    }
    std::array<std::uint8_t, 4096> chunk = {};
    const auto read = client->Receive(chunk);
    REQUIRE(read.has_value());
    if (read->state == ReceiveState::kBytes) {
      reader.Feed(std::span<const std::uint8_t>(chunk.data(), read->bytes));
    }
    for (;;) {
      auto next = reader.Next();
      REQUIRE(next.has_value());
      if (!next->has_value()) break;
      received.push_back(std::move(**next));
    }
  }

  void Say(FrameKind kind, const std::vector<std::uint8_t>& payload) {
    REQUIRE(client.has_value());
    const std::vector<std::uint8_t> frame = ro::voice::EncodeFrame(kind, payload);
    std::size_t sent = 0;
    while (sent < frame.size()) {
      const auto wrote = client->Send(std::span(frame).subspan(sent));
      REQUIRE(wrote.has_value());
      sent += *wrote;
    }
  }

  void SayText(const std::string& json) {
    Say(FrameKind::kText, std::vector<std::uint8_t>(json.begin(), json.end()));
  }

  [[nodiscard]] bool HeardText(const std::string& piece) const {
    for (const StreamFrame& frame : received) {
      if (frame.kind != FrameKind::kText) continue;
      if (std::string(frame.payload.begin(), frame.payload.end()).find(piece) !=
          std::string::npos) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] std::size_t BinaryCount() const {
    std::size_t count = 0;
    for (const StreamFrame& frame : received) {
      if (frame.kind == FrameKind::kBinary) ++count;
    }
    return count;
  }
};

// L'audio d'un autre, tel que le serveur l'écrit, autour d'un paquet Opus.
[[nodiscard]] std::vector<std::uint8_t> Heard(std::uint16_t sequence,
                                              const std::vector<std::uint8_t>& opus) {
  std::vector<std::uint8_t> bytes = {0,    2,    0x49, 0xF1,  // char_id 150001
                                     0x3F, 0x80, 0,    0,     // 1.0F
                                     0,    0,    0,    10,   0, 0, 0, 20};
  const char name[] = "Ami";
  for (std::size_t at = 0; at < ro::voice::kHeardNameBytes; ++at) {
    bytes.push_back(at < sizeof(name) - 1 ? static_cast<std::uint8_t>(name[at]) : 0);
  }
  bytes.push_back(static_cast<std::uint8_t>(sequence >> 8));
  bytes.push_back(static_cast<std::uint8_t>(sequence));
  bytes.insert(bytes.end(), opus.begin(), opus.end());
  return bytes;
}

}  // namespace

TEST_CASE("Voix : la liaison s'authentifie, entend, puis parle", "[voice][link]") {
  auto listening = TcpListener::ListenLocal(0);
  REQUIRE(listening.has_value());
  FakeServer server{.listener = std::move(*listening)};

  // UN MICRO QUI REND DU SILENCE, une trame par pas, et une sortie qui joue
  // tout ce qu'on lui donne d'un pas à l'autre.
  std::size_t queued = 0;
  std::size_t played = 0;
  ro::voice::VoiceDevices devices{.read_capture =
                                      [](std::span<std::int16_t> into) {
                                        const std::size_t samples =
                                            std::min(into.size(), ro::voice::kVoiceFrameSamples);
                                        std::fill_n(into.begin(), samples, std::int16_t{100});
                                        return samples;
                                      },
                                  .write_playback =
                                      [&](std::span<const std::int16_t> stereo) {
                                        queued += stereo.size();
                                        played += stereo.size();
                                      },
                                  .queued_playback = [&] { return queued; }};
  VoiceLink link(ro::voice::VoiceTarget{.host = "127.0.0.1",
                                        .port = server.listener.port(),
                                        .account_id = 2000001,
                                        .character_id = 150001,
                                        .login_id1 = 77},
                 std::move(devices));

  std::uint32_t now_ms = 0;
  const auto step = [&] {
    link.Step(now_ms);
    now_ms += 5;
    queued = 0;
    server.Pump();
  };

  for (int turn = 0; turn < kTurns && !server.HeardText("\"type\":\"auth\""); ++turn) step();
  REQUIRE(server.HeardText("\"login_id1\":77"));
  CHECK(link.status(now_ms).state == VoiceLinkState::kAuthenticating);
  CHECK(played > 0);

  server.SayText("{\"type\":\"auth_ok\",\"udp_port\":0,\"udp_token\":1}");
  for (int turn = 0; turn < kTurns && link.status(now_ms).state != VoiceLinkState::kOnline;
       ++turn) {
    step();
  }
  REQUIRE(link.status(now_ms).state == VoiceLinkState::kOnline);
  CHECK_FALSE(link.status(now_ms).udp);

  auto encoder = ro::voice::OpusVoiceEncoder::Create();
  REQUIRE(encoder.has_value());
  const std::vector<std::int16_t> silence(ro::voice::kVoiceFrameSamples);
  for (std::uint16_t sequence = 0; sequence < 3; ++sequence) {
    auto packet = encoder->Encode(silence);
    REQUIRE(packet.has_value());
    server.Say(FrameKind::kBinary, Heard(sequence, *packet));
  }
  for (int turn = 0; turn < kTurns && link.status(now_ms).speaking.empty(); ++turn) step();
  REQUIRE(link.status(now_ms).speaking.size() == 1);
  CHECK(link.status(now_ms).speaking[0].name == "Ami");

  // SE TAIRE : rien ne part, mais le serveur sait qu'on se tait.
  for (int turn = 0; turn < 10; ++turn) step();
  CHECK(server.HeardText("\"type\":\"set_channel\",\"channel\":0"));
  CHECK(server.HeardText("\"type\":\"ptt\",\"value\":false"));
  CHECK(server.BinaryCount() == 0);

  link.Steer(ro::voice::VoiceControls{.talking = true});
  for (int turn = 0; turn < kTurns && server.BinaryCount() < 3; ++turn) step();
  CHECK(server.HeardText("\"type\":\"ptt\",\"value\":true"));
  CHECK(server.BinaryCount() >= 3);
  CHECK(link.status(now_ms).transmitting);

  // LA TOUCHE LÂCHÉE, LA PAROLE TIENT LE DÉLAI, puis se tait.
  constexpr std::uint32_t kReleaseMs = 100;
  constexpr std::uint32_t kStepMs = 5;
  const std::uint32_t released_at_ms = now_ms;
  link.Steer(ro::voice::VoiceControls{.release_ms = kReleaseMs});
  step();
  CHECK(link.status(now_ms).transmitting);
  while (now_ms - released_at_ms <= kReleaseMs + kStepMs) step();
  CHECK_FALSE(link.status(now_ms).transmitting);

  // À LA VOIX : un micro au-dessus du seuil parle sans touche — le banc rend
  // un niveau d'environ 0,003 —, et sous le seuil il se tait, une fois tenu
  // le plus court délai.
  link.Steer(ro::voice::VoiceControls{
      .voice_activated = true, .activation_level = 0.001F, .release_ms = 0});
  step();
  CHECK(link.status(now_ms).transmitting);
  CHECK(link.status(now_ms).input_level > 0.001F);
  link.Steer(
      ro::voice::VoiceControls{.voice_activated = true, .activation_level = 0.5F, .release_ms = 0});
  step();
  CHECK(link.status(now_ms).transmitting);
  const std::uint32_t quiet_at_ms = now_ms;
  while (now_ms - quiet_at_ms <= ro::voice::kVoiceActivationLeastHoldMs + kStepMs) step();
  CHECK_FALSE(link.status(now_ms).transmitting);

  // LE MICRO COUPÉ ET LA SOURDINE : la touche tenue ne fait rien partir.
  link.Steer(ro::voice::VoiceControls{.talking = true, .self_muted = true});
  step();
  CHECK_FALSE(link.status(now_ms).transmitting);
  link.Steer(ro::voice::VoiceControls{.talking = true, .deafened = true});
  step();
  CHECK_FALSE(link.status(now_ms).transmitting);

  // PLUSIEURS CANAUX ÉCOUTÉS, celui où l'on parle compris. Le serveur qui le
  // confirme n'entend plus parler de canal unique.
  CHECK(server.HeardText("\"type\":\"set_listen\",\"mask\":1"));
  link.Steer(ro::voice::VoiceControls{.channel = VoiceChannel::kGuild,
                                      .listened = ro::voice::ListenBit(VoiceChannel::kNearby)});
  for (int turn = 0; turn < 10; ++turn) step();
  CHECK(server.HeardText("\"type\":\"set_listen\",\"mask\":5"));
  CHECK(server.HeardText("\"type\":\"set_channel\",\"channel\":2"));
  server.SayText("{\"type\":\"listen_ack\",\"mask\":5}");
  for (int turn = 0; turn < 10; ++turn) step();
  link.Steer(ro::voice::VoiceControls{.channel = VoiceChannel::kParty,
                                      .listened = ro::voice::ListenBit(VoiceChannel::kNearby)});
  for (int turn = 0; turn < 10; ++turn) step();
  CHECK(server.HeardText("\"type\":\"set_listen\",\"mask\":3"));
  CHECK_FALSE(server.HeardText("\"type\":\"set_channel\",\"channel\":1"));
}

TEST_CASE("Voix : un refus levé disparaît sans que la liaison tombe", "[voice][link]") {
  auto listening = TcpListener::ListenLocal(0);
  REQUIRE(listening.has_value());
  FakeServer server{.listener = std::move(*listening)};
  VoiceLink link(ro::voice::VoiceTarget{.host = "127.0.0.1", .port = server.listener.port()},
                 ro::voice::VoiceDevices{});
  std::uint32_t now_ms = 0;
  const auto step = [&] {
    link.Step(now_ms);
    now_ms += 5;
    server.Pump();
  };
  for (int turn = 0; turn < kTurns && !server.HeardText("\"type\":\"auth\""); ++turn) step();
  server.SayText("{\"type\":\"auth_ok\",\"udp_port\":0,\"udp_token\":1}");
  for (int turn = 0; turn < kTurns && link.status(now_ms).state != VoiceLinkState::kOnline;
       ++turn) {
    step();
  }
  REQUIRE(link.status(now_ms).state == VoiceLinkState::kOnline);

  server.SayText("{\"type\":\"admin_banned\"}");
  for (int turn = 0; turn < kTurns && !link.status(now_ms).refusal.has_value(); ++turn) step();
  REQUIRE(link.status(now_ms).refusal == ro::voice::RefusalKind::kBanned);

  server.SayText("{\"type\":\"admin_unbanned\"}");
  for (int turn = 0; turn < kTurns && link.status(now_ms).refusal.has_value(); ++turn) step();
  CHECK_FALSE(link.status(now_ms).refusal.has_value());
}

TEST_CASE("Voix : une boucle de jeu figée referme le micro", "[voice][link]") {
  constexpr auto kPatience = std::chrono::seconds(10);
  auto listening = TcpListener::ListenLocal(0);
  REQUIRE(listening.has_value());
  FakeServer server{.listener = std::move(*listening)};
  ro::voice::VoiceEngine engine(
      ro::voice::VoiceTarget{.host = "127.0.0.1", .port = server.listener.port()},
      ro::voice::VoiceDevices{});

  const auto wait_for = [&](const auto& reached) {
    const auto deadline = std::chrono::steady_clock::now() + kPatience;
    while (!reached() && std::chrono::steady_clock::now() < deadline) {
      server.Pump();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return reached();
  };
  REQUIRE(wait_for([&] { return server.HeardText("\"type\":\"auth\""); }));
  server.SayText("{\"type\":\"auth_ok\",\"udp_port\":0,\"udp_token\":1}");
  REQUIRE(wait_for([&] { return engine.status().state == VoiceLinkState::kOnline; }));

  engine.Steer(ro::voice::VoiceControls{.talking = true});
  REQUIRE(wait_for([&] { return server.HeardText("\"type\":\"ptt\",\"value\":true"); }));

  // Plus aucun `Steer` : la partie ne sait plus lire la touche.
  server.received.clear();
  CHECK(wait_for([&] { return server.HeardText("\"type\":\"ptt\",\"value\":false"); }));
}

namespace {

// Une liaison qui a dit qui elle est, face au faux serveur.
struct AuthenticatingLink {
  FakeServer server;
  VoiceLink link;
  std::uint32_t now_ms = 0;

  explicit AuthenticatingLink(TcpListener listening)
      : server{.listener = std::move(listening)},
        link(ro::voice::VoiceTarget{.host = "127.0.0.1", .port = server.listener.port()},
             ro::voice::VoiceDevices{}) {
    for (int turn = 0; turn < kTurns && !server.HeardText("\"type\":\"auth\""); ++turn) Step();
    REQUIRE(server.HeardText("\"type\":\"auth\""));
  }

  void Step() {
    link.Step(now_ms);
    now_ms += 5;
    server.Pump();
  }

  void GoOnline() {
    server.SayText("{\"type\":\"auth_ok\",\"udp_port\":0,\"udp_token\":1}");
    for (int turn = 0; turn < kTurns && link.status(now_ms).state != VoiceLinkState::kOnline;
         ++turn) {
      Step();
    }
    REQUIRE(link.status(now_ms).state == VoiceLinkState::kOnline);
  }
};

[[nodiscard]] TcpListener ListenAnywhere() {
  auto listening = TcpListener::ListenLocal(0);
  REQUIRE(listening.has_value());
  return std::move(*listening);
}

// Le temps qu'une fermeture traverse la boucle locale.
constexpr auto kLoopbackSettling = std::chrono::milliseconds(50);

}  // namespace

TEST_CASE("Voix : un refus suivi de la fermeture se lit avant de tomber", "[voice][link]") {
  AuthenticatingLink bench(ListenAnywhere());

  // LE REFUS ET LA FIN DU FLUX ARRIVENT AU MÊME PAS.
  bench.server.SayText("{\"type\":\"flood_banned\",\"duration_ms\":300000}");
  bench.server.client->Close();
  std::this_thread::sleep_for(kLoopbackSettling);
  bench.link.Step(bench.now_ms);

  const ro::voice::VoiceStatus status = bench.link.status(bench.now_ms);
  CHECK(status.state == VoiceLinkState::kLost);
  CHECK(status.refusal == ro::voice::RefusalKind::kFlooded);
}

TEST_CASE("Voix : un serveur qui n'accorde jamais rien fait retenter", "[voice][link]") {
  AuthenticatingLink bench(ListenAnywhere());
  const std::uint32_t opened_ms = bench.now_ms;

  // ⚠ LE SERVEUR PEUT TENIR L'ACCORD 30 S : jusque-là, on attend.
  constexpr std::uint32_t kServerHoldsMs = 30'000;
  STATIC_CHECK(ro::voice::kVoiceAuthTimeoutMs > kServerHoldsMs);
  bench.link.Step(opened_ms + kServerHoldsMs);
  CHECK(bench.link.status(opened_ms).state == VoiceLinkState::kAuthenticating);

  bench.link.Step(opened_ms + ro::voice::kVoiceAuthTimeoutMs);
  CHECK(bench.link.status(opened_ms).state == VoiceLinkState::kLost);
}

TEST_CASE("Voix : un serveur muet fait tomber la liaison, pas un serveur qui répond",
          "[voice][link]") {
  AuthenticatingLink bench(ListenAnywhere());
  bench.GoOnline();

  // LE PONG COMPTE : au rythme des battements, la liaison tient bien plus longtemps
  // que le silence admis.
  constexpr int kBeats = 4;
  STATIC_CHECK(ro::voice::kVoicePingMs < ro::voice::kVoiceSilentMs);
  std::uint32_t last_heard_ms = 0;
  for (int beat = 0; beat < kBeats; ++beat) {
    bench.now_ms += ro::voice::kVoicePingMs;
    last_heard_ms = bench.now_ms;
    bench.server.SayText("{\"type\":\"pong\",\"t\":0}");
    std::this_thread::sleep_for(kLoopbackSettling);
    bench.Step();
    REQUIRE(bench.link.status(bench.now_ms).state == VoiceLinkState::kOnline);
  }

  bench.link.Step(last_heard_ms + ro::voice::kVoiceSilentMs - 1);
  CHECK(bench.link.status(last_heard_ms).state == VoiceLinkState::kOnline);
  bench.link.Step(last_heard_ms + ro::voice::kVoiceSilentMs);
  CHECK(bench.link.status(last_heard_ms).state == VoiceLinkState::kLost);
}

TEST_CASE("Voix : le repos d'une connexion ratée se compte depuis sa fin", "[voice][link]") {
  // Un port où plus personne n'écoute : la connexion échoue, quel qu'en soit le temps.
  std::uint16_t closed_port = 0;
  {
    const TcpListener listening = ListenAnywhere();
    closed_port = listening.port();
  }
  VoiceLink link(ro::voice::VoiceTarget{.host = "127.0.0.1", .port = closed_port},
                 ro::voice::VoiceDevices{});
  link.Step(0);
  REQUIRE(link.status(0).state == VoiceLinkState::kLost);

  // L'attente de la connexion a pris tout ce temps ; c'est le pas suivant qui le voit.
  constexpr std::uint32_t kConnectTookMs = 2000;
  link.Step(kConnectTookMs);
  CHECK(link.status(0).state == VoiceLinkState::kLost);
  // Compté depuis l'entrée de la tentative, le repos serait déjà fini, et cet essai
  // remonterait la liaison sur un écouteur rouvert.
  auto reopened = TcpListener::ListenLocal(closed_port);
  REQUIRE(reopened.has_value());
  link.Step(ro::voice::kVoiceRetryMs);
  CHECK(link.status(0).state == VoiceLinkState::kLost);
  link.Step(kConnectTookMs + ro::voice::kVoiceRetryMs);
  CHECK(link.status(0).state != VoiceLinkState::kLost);
}

TEST_CASE("Voix : fermer n'attend pas une connexion sans réponse", "[voice][link]") {
  // Une adresse réservée à la documentation : personne n'y répond. Une machine sans
  // route la refuse aussitôt, et l'essai ne dit alors rien de plus.
  constexpr const char* kUnansweringHost = "192.0.2.1";
  constexpr auto kConnectUnderWay = std::chrono::milliseconds(200);
  constexpr auto kMostJoinWait = std::chrono::milliseconds(ro::voice::kVoiceConnectTimeoutMs / 2);

  std::optional<ro::voice::VoiceEngine> engine;
  engine.emplace(ro::voice::VoiceTarget{.host = kUnansweringHost, .port = 9},
                 ro::voice::VoiceDevices{});
  std::this_thread::sleep_for(kConnectUnderWay);
  const auto asked = std::chrono::steady_clock::now();
  engine.reset();
  CHECK(std::chrono::steady_clock::now() - asked < kMostJoinWait);
}
