// LE SERVICE WEB DES EMBLÈMES, sur un transport qui répond ce qu'on veut.
//
// 🔴 CE BANC PORTE TROIS CHOSES :
//
//   · le service ne lit que des PARTIES de formulaire, sous des noms exacts ;
//     une requête qui lui présenterait ses champs autrement serait refusée ;
//   · un dépôt passe DEVANT les lectures en attente ;
//   · un emblème qui ne vient pas se REPOSE une minute, il ne se relance pas
//     à chaque image.

#include "web/emblem_web.h"

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "web/web_transport.h"

using moonclient::web::BuildMultipartBody;
using moonclient::web::EmblemIdentity;
using moonclient::web::EmblemWeb;
using moonclient::web::kEmblemRetryMs;
using moonclient::web::WebPart;
using moonclient::web::WebReply;
using moonclient::web::WebRequest;
using moonclient::web::WebTransport;

namespace {

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

[[nodiscard]] EmblemIdentity Me() {
  return EmblemIdentity{.account_id = 2000000, .token = "abcdef", .world_name = "Moonlight"};
}

}  // namespace

TEST_CASE("Emblème web : une lecture porte l'identité en parties", "[app][emblem_web]") {
  FakeTransport wire;
  EmblemWeb web(wire);
  web.SetIdentity(Me());
  web.Want(42, 3, 0);
  web.Advance(0);
  REQUIRE(wire.sent.size() == 1);
  const WebRequest& request = wire.sent.front();
  CHECK(request.path == "/emblem/download");
  CHECK(request.fields.empty());
  CHECK(PartOf(request, "AID") == "2000000");
  CHECK(PartOf(request, "AuthToken") == "abcdef");
  CHECK(PartOf(request, "WorldName") == "Moonlight");
  CHECK(PartOf(request, "GDID") == "42");
  CHECK(PartOf(request, "Version") == "3");

  wire.queued.push_back(WebReply{.status = 200, .body = "BMimage", .error = {}});
  web.Advance(10);
  const auto downloads = web.TakeDownloads();
  REQUIRE(downloads.size() == 1);
  CHECK(downloads.front().guild_id == 42);
  CHECK(downloads.front().image == "BMimage");
}

TEST_CASE("Emblème web : un échec se repose une minute", "[app][emblem_web]") {
  FakeTransport wire;
  EmblemWeb web(wire);
  web.SetIdentity(Me());
  web.Want(42, 3, 0);
  web.Advance(0);
  wire.queued.push_back(WebReply{.status = 404, .body = "Error", .error = {}});
  web.Advance(10);
  CHECK(web.TakeDownloads().empty());

  web.Want(42, 3, 20);
  web.Advance(20);
  CHECK(wire.sent.size() == 1);

  web.Want(42, 3, 10 + kEmblemRetryMs);
  web.Advance(10 + kEmblemRetryMs);
  CHECK(wire.sent.size() == 2);
}

TEST_CASE("Emblème web : le dépôt passe devant et rend sa version", "[app][emblem_web]") {
  FakeTransport wire;
  EmblemWeb web(wire);
  web.SetIdentity(Me());
  web.Want(7, 1, 0);
  REQUIRE(web.Upload(42, "BMdessin"));
  CHECK_FALSE(web.Upload(42, "BMautre"));
  web.Advance(0);
  REQUIRE(wire.sent.size() == 1);
  const WebRequest& upload = wire.sent.front();
  CHECK(upload.path == "/emblem/upload");
  CHECK(PartOf(upload, "ImgType") == "BMP");
  CHECK(PartOf(upload, "Img") == "BMdessin");

  wire.queued.push_back(WebReply{.status = 200, .body = R"({"Type":1,"version":5})", .error = {}});
  web.Advance(10);
  const auto outcome = web.TakeUploadOutcome();
  REQUIRE(outcome.has_value());
  CHECK(outcome->accepted());
  CHECK(outcome->guild_id == 42);
  CHECK(outcome->version == 5);
  // La lecture en attente part ensuite.
  CHECK(wire.sent.size() == 2);
  CHECK(wire.sent.back().path == "/emblem/download");
}

TEST_CASE("Emblème web : un refus du service se dit", "[app][emblem_web]") {
  FakeTransport wire;
  EmblemWeb web(wire);
  web.SetIdentity(Me());
  REQUIRE(web.Upload(42, "BMdessin"));
  web.Advance(0);
  wire.queued.push_back(WebReply{.status = 400, .body = "Error", .error = {}});
  web.Advance(10);
  const auto outcome = web.TakeUploadOutcome();
  REQUIRE(outcome.has_value());
  CHECK_FALSE(outcome->accepted());
  CHECK_FALSE(outcome->refusal.empty());
  CHECK_FALSE(web.uploading());
}

TEST_CASE("Emblème web : sans jeton, rien ne part", "[app][emblem_web]") {
  FakeTransport wire;
  EmblemWeb web(wire);
  web.Want(42, 3, 0);
  web.Advance(0);
  CHECK(wire.sent.empty());
}

TEST_CASE("Le corps en plusieurs morceaux évite sa frontière", "[app][emblem_web]") {
  const std::vector<WebPart> parts = {WebPart{.name = "AID", .content = "12"},
                                      WebPart{.name = "Img",
                                              .content = "----moonclient-part",
                                              .file_name = "emblem.bmp",
                                              .content_type = "image/bmp"}};
  const auto built = BuildMultipartBody(parts);
  CHECK(built.boundary != "----moonclient-part");
  CHECK(built.body.find("Content-Disposition: form-data; name=\"AID\"\r\n\r\n12\r\n") !=
        std::string::npos);
  CHECK(built.body.find("name=\"Img\"; filename=\"emblem.bmp\"\r\nContent-Type: image/bmp") !=
        std::string::npos);
  CHECK(built.body.ends_with("--" + built.boundary + "--\r\n"));
}
