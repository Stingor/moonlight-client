// LA LISTE DE BLOCAGE RANGÉE AU SERVICE WEB, sur un transport qui répond ce
// qu'on veut.
//
// 🔴 CE BANC PORTE QUATRE CHOSES :
//
//   · le document est celui du client d'origine : {"data":{"WhisperBlockList":
//     [noms]}}, et une liste vide part en tableau vide ;
//   · la liste se rejoue UNE fois, et pas avant que la carte soit annoncée ;
//   · chaque changement fait par une réponse du serveur se sauvegarde, et ceux
//     faits pendant la relecture attendent qu'elle aboutisse ;
//   · sans service, ou s'il ne répond pas, la liste vit la session et rien ne
//     casse.

#include "chat/whisper_block_memory.h"

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/whisper_blocks.h"

#include "web/user_config_web.h"
#include "web/web_transport.h"

using moonclient::WhisperBlockMemory;
using moonclient::web::UserConfigIdentity;
using moonclient::web::UserConfigOfWhisperBlocks;
using moonclient::web::UserConfigWeb;
using moonclient::web::WebPart;
using moonclient::web::WebReply;
using moonclient::web::WebRequest;
using moonclient::web::WebTransport;
using moonclient::web::WhisperBlocksFromUserConfig;
using ro::sim::WhisperBlockChange;
using ro::sim::WhisperBlockChangeKind;

namespace {

constexpr int kHttpOk = 200;
constexpr int kHttpBadRequest = 400;

class FakeTransport final : public WebTransport {
 public:
  void Begin(const WebRequest& request) override {
    if (in_flight_) {
      return;
    }
    sent.push_back(request);
    in_flight_ = true;
  }

  [[nodiscard]] std::optional<WebReply> Take() override {
    if (!in_flight_ || queued.empty()) {
      return std::nullopt;
    }
    in_flight_ = false;
    WebReply reply = queued.front();
    queued.pop_front();
    return reply;
  }

  [[nodiscard]] bool busy() const override { return in_flight_; }
  void Abandon() override { in_flight_ = false; }

  void Answer(std::string body) {
    queued.push_back(WebReply{.status = kHttpOk, .body = std::move(body)});
  }

  std::deque<WebReply> queued;
  std::vector<WebRequest> sent;

 private:
  bool in_flight_ = false;
};

[[nodiscard]] std::string PartOf(const WebRequest& request, std::string_view name) {
  for (const WebPart& part : request.parts) {
    if (part.name == name) {
      return part.content;
    }
  }
  return {};
}

[[nodiscard]] bool HasPart(const WebRequest& request, std::string_view name) {
  for (const WebPart& part : request.parts) {
    if (part.name == name) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] UserConfigIdentity Me() {
  return UserConfigIdentity{.account_id = 2000000, .token = "abcdef", .world_name = "Moonlight"};
}

[[nodiscard]] WhisperBlockChange Added(std::string name) {
  return WhisperBlockChange{.kind = WhisperBlockChangeKind::kAdded, .name = std::move(name)};
}

[[nodiscard]] WhisperBlockChange Removed(std::string name) {
  return WhisperBlockChange{.kind = WhisperBlockChangeKind::kRemoved, .name = std::move(name)};
}

// La liste qu'une sauvegarde a envoyée, relue comme le service la rendrait.
[[nodiscard]] std::vector<std::string> SavedNames(const WebRequest& request) {
  std::string document = PartOf(request, "data");
  // Le service range `data` et répond le document fusionné, `Type` en plus.
  document.insert(1, R"("Type":1,)");
  return WhisperBlocksFromUserConfig(document).value();
}

}  // namespace

// ── LE DOCUMENT ────────────────────────────────────────────────────────────

TEST_CASE("Données de compte : la liste se lit sous data.WhisperBlockList",
          "[app][whisper_block_memory]") {
  const auto names = WhisperBlocksFromUserConfig(
      R"({"Type":1,"data":{"EmotionHotkey":["/!"],"WhisperBlockList":["Bob","Aurélie","Bob",3,""]}})");
  REQUIRE(names.has_value());
  // Doublon, nombre et nom vide écartés ; l'ordre du document gardé.
  CHECK(*names == std::vector<std::string>{"Bob", "Aurélie"});
}

TEST_CASE("Données de compte : un compte neuf n'a pas de liste", "[app][whisper_block_memory]") {
  CHECK(WhisperBlocksFromUserConfig(R"({"Type": 1})").value().empty());
  CHECK(WhisperBlocksFromUserConfig(R"({"Type":1,"data":{}})").value().empty());
  CHECK(WhisperBlocksFromUserConfig(R"({"Type":2})").value().empty());
  // Ni réponse servie, ni JSON.
  CHECK_FALSE(WhisperBlocksFromUserConfig(R"({"Type":0})").has_value());
  CHECK_FALSE(WhisperBlocksFromUserConfig("Error").has_value());
}

TEST_CASE("Données de compte : le document envoyé se relit à l'identique",
          "[app][whisper_block_memory]") {
  const std::vector<std::string> names{"Bob", R"(Le "Grand")", "Barre\\Oblique", "Aurélie"};
  const std::string document = UserConfigOfWhisperBlocks(names);
  CHECK(document.starts_with(R"({"data":{"WhisperBlockList":[)"));
  std::string served = document;
  served.insert(1, R"("Type":1,)");
  CHECK(WhisperBlocksFromUserConfig(served).value() == names);
  // 🔴 UNE LISTE VIDE PART EN TABLEAU VIDE : le service fusionne, et une clé
  // absente laisserait l'ancienne liste en place.
  CHECK(UserConfigOfWhisperBlocks({}) == R"({"data":{"WhisperBlockList":[]}})");
}

TEST_CASE("Données de compte : le jeton connu part à la relecture et à la sauvegarde",
          "[app][whisper_block_memory]") {
  FakeTransport transport;
  UserConfigWeb web(transport, Me());
  web.Load();
  web.Save("{}");
  web.Advance();
  REQUIRE(transport.sent.size() == 1);
  // ⚠ LA RELECTURE PASSE DEVANT, et porte le compte, le jeton et le monde.
  CHECK(transport.sent[0].path == "/userconfig/load");
  CHECK(PartOf(transport.sent[0], "AID") == "2000000");
  CHECK(PartOf(transport.sent[0], "AuthToken") == "abcdef");
  CHECK(PartOf(transport.sent[0], "WorldName") == "Moonlight");
  transport.Answer(R"({"Type":1})");
  web.Advance();
  REQUIRE(web.TakeLoaded().has_value());
  CHECK_FALSE(web.TakeLoaded().has_value());
  REQUIRE(transport.sent.size() == 2);
  CHECK(transport.sent[1].path == "/userconfig/save");
  CHECK(PartOf(transport.sent[1], "AuthToken") == "abcdef");
  CHECK(PartOf(transport.sent[1], "WorldName") == "Moonlight");
  CHECK(PartOf(transport.sent[1], "data") == "{}");
  transport.queued.push_back(WebReply{.status = kHttpBadRequest, .body = "Error"});
  web.Advance();
  const auto saved = web.TakeSaved();
  REQUIRE(saved.has_value());
  CHECK_FALSE(saved->served());
  CHECK(web.idle());
}

TEST_CASE("Données de compte : sans jeton, la relecture part sans le champ",
          "[app][whisper_block_memory]") {
  FakeTransport transport;
  UserConfigIdentity tokenless = Me();
  tokenless.token.clear();
  UserConfigWeb web(transport, tokenless);
  web.Load();
  web.Save("{}");
  web.Advance();
  REQUIRE(transport.sent.size() == 1);
  CHECK(transport.sent[0].path == "/userconfig/load");
  CHECK(PartOf(transport.sent[0], "AID") == "2000000");
  CHECK(PartOf(transport.sent[0], "WorldName") == "Moonlight");
  CHECK_FALSE(HasPart(transport.sent[0], "AuthToken"));
  // La sauvegarde, elle, exige le jeton : elle ne part pas.
  transport.Answer(R"({"Type":1})");
  web.Advance();
  REQUIRE(web.TakeLoaded().has_value());
  CHECK(transport.sent.size() == 1);
}

// ── LA MÉMOIRE ─────────────────────────────────────────────────────────────

TEST_CASE("Blocage rangé : la liste relue se rejoue une fois, après la carte",
          "[app][whisper_block_memory]") {
  FakeTransport transport;
  WhisperBlockMemory memory(&transport, Me());
  CHECK(memory.Advance({}).empty());
  REQUIRE(transport.sent.size() == 1);
  transport.Answer(R"({"Type":1,"data":{"WhisperBlockList":["Bob","Carl"]}})");
  // ⚠ RELUE, MAIS LA CARTE N'EST PAS ANNONCÉE : le serveur jetterait tout.
  CHECK(memory.Advance({}).empty());
  CHECK(memory.kept());
  memory.NoteMapEntered();
  CHECK(memory.Advance({}) == std::vector<std::string>{"Bob", "Carl"});
  CHECK(memory.Advance({}).empty());
  // Les réponses du rejeu ne changent rien à la liste rangée : pas de sauvegarde.
  const std::vector<WhisperBlockChange> replayed{Added("Bob"), Added("Carl")};
  CHECK(memory.Advance(replayed).empty());
  CHECK(transport.sent.size() == 1);
}

TEST_CASE("Blocage rangé : la carte peut précéder la relecture", "[app][whisper_block_memory]") {
  FakeTransport transport;
  WhisperBlockMemory memory(&transport, Me());
  memory.NoteMapEntered();
  CHECK(memory.Advance({}).empty());
  transport.Answer(R"({"Type":1,"data":{"WhisperBlockList":["Bob"]}})");
  CHECK(memory.Advance({}) == std::vector<std::string>{"Bob"});
}

TEST_CASE("Blocage rangé : chaque changement se sauvegarde, la liste entière",
          "[app][whisper_block_memory]") {
  FakeTransport transport;
  WhisperBlockMemory memory(&transport, Me());
  static_cast<void>(memory.Advance({}));
  transport.Answer(R"({"Type":1,"data":{"WhisperBlockList":["Bob"]}})");
  static_cast<void>(memory.Advance({}));

  const std::vector<WhisperBlockChange> blocked{Added("Aurélie")};
  static_cast<void>(memory.Advance(blocked));
  REQUIRE(transport.sent.size() == 2);
  CHECK(transport.sent[1].path == "/userconfig/save");
  CHECK(SavedNames(transport.sent[1]) == std::vector<std::string>{"Bob", "Aurélie"});

  transport.Answer(R"({"Type":1})");
  const std::vector<WhisperBlockChange> allowed{Removed("Bob")};
  static_cast<void>(memory.Advance(allowed));
  REQUIRE(transport.sent.size() == 3);
  CHECK(SavedNames(transport.sent[2]) == std::vector<std::string>{"Aurélie"});

  // « TOUT AUTORISER » VIDE, et le vide se sauvegarde.
  transport.Answer(R"({"Type":1})");
  const std::vector<WhisperBlockChange> emptied{
      WhisperBlockChange{.kind = WhisperBlockChangeKind::kEmptied}};
  static_cast<void>(memory.Advance(emptied));
  REQUIRE(transport.sent.size() == 4);
  CHECK(PartOf(transport.sent[3], "data") == R"({"data":{"WhisperBlockList":[]}})");
  CHECK(memory.names().empty());
}

TEST_CASE("Blocage rangé : un changement pendant la relecture attend qu'elle aboutisse",
          "[app][whisper_block_memory]") {
  FakeTransport transport;
  WhisperBlockMemory memory(&transport, Me());
  const std::vector<WhisperBlockChange> early{Added("Aurélie"), Removed("Bob")};
  static_cast<void>(memory.Advance(early));
  // 🔴 RIEN NE PART AVANT LA RELECTURE : la liste rangée serait écrasée.
  REQUIRE(transport.sent.size() == 1);
  transport.Answer(R"({"Type":1,"data":{"WhisperBlockList":["Bob","Carl"]}})");
  static_cast<void>(memory.Advance({}));
  CHECK(std::vector<std::string>(memory.names().begin(), memory.names().end()) ==
        std::vector<std::string>{"Carl", "Aurélie"});
  static_cast<void>(memory.Advance({}));
  REQUIRE(transport.sent.size() == 2);
  CHECK(SavedNames(transport.sent[1]) == std::vector<std::string>{"Carl", "Aurélie"});
}

TEST_CASE("Blocage rangé : sans service, la liste vit la session", "[app][whisper_block_memory]") {
  WhisperBlockMemory without_origin(nullptr, Me());
  without_origin.NoteMapEntered();
  const std::vector<WhisperBlockChange> blocked{Added("Bob")};
  CHECK(without_origin.Advance(blocked).empty());
  CHECK_FALSE(without_origin.kept());

  // Sans jeton, rien ne part.
  FakeTransport tokenless;
  WhisperBlockMemory without_token(&tokenless, UserConfigIdentity{.account_id = 2000000});
  CHECK(without_token.Advance(blocked).empty());
  CHECK(tokenless.sent.empty());

  // Un service qui ne répond pas : la session continue, rien ne se sauvegarde.
  FakeTransport silent;
  WhisperBlockMemory unreachable(&silent, Me());
  unreachable.NoteMapEntered();
  static_cast<void>(unreachable.Advance({}));
  silent.queued.push_back(WebReply{.error = "délai dépassé"});
  CHECK(unreachable.Advance({}).empty());
  CHECK_FALSE(unreachable.kept());
  CHECK(unreachable.Advance(blocked).empty());
  CHECK(silent.sent.size() == 1);
}

TEST_CASE("Blocage rangé : le rejeu s'arrête où le reflet sait apparier",
          "[app][whisper_block_memory]") {
  FakeTransport transport;
  WhisperBlockMemory memory(&transport, Me());
  std::vector<std::string> many;
  for (std::size_t index = 0; index <= ro::sim::kMostPendingWhisperBlocks; ++index) {
    many.push_back("Nom" + std::to_string(index));
  }
  std::string served = UserConfigOfWhisperBlocks(many);
  served.insert(1, R"("Type":1,)");
  memory.NoteMapEntered();
  static_cast<void>(memory.Advance({}));
  transport.Answer(served);
  CHECK(memory.Advance({}).size() == ro::sim::kMostPendingWhisperBlocks);
  CHECK(memory.names().size() == many.size());
}
