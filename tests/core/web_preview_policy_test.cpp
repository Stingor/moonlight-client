// CE QUE L'APERÇU D'UNE ADRESSE A LE DROIT DE TÉLÉCHARGER : la liste blanche, la
// boucle des sauts sur un transport simulé, la page et son og:image, et le
// registre des aperçus.

#include "ro/core/web_preview_policy.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::core::FetchPreviewBytes;
using ro::core::PreviewAllowList;
using ro::core::PreviewBody;
using ro::core::PreviewBook;
using ro::core::PreviewFetched;
using ro::core::PreviewReply;
using ro::core::PreviewState;

namespace {

constexpr int kHttpFound = 302;
constexpr int kHttpNotFound = 404;
constexpr std::string_view kPng = "image/png";
constexpr std::string_view kHtml = "text/html; charset=utf-8";

// UN SERVEUR SIMULÉ : une réponse par adresse, et le relevé de ce qui a été
// demandé. Une adresse inconnue répond une erreur de transport.
class FakeWeb {
 public:
  void Serve(std::string url, PreviewReply reply) { replies_[std::move(url)] = std::move(reply); }
  void Picture(std::string url) {
    Serve(std::move(url),
          PreviewReply{
              .status = ro::core::kHttpOk, .content_type = std::string(kPng), .body = "PNG"});
  }
  void Redirect(std::string url, std::string location) {
    Serve(std::move(url), PreviewReply{.status = kHttpFound, .location = std::move(location)});
  }
  void Page(std::string url, std::string html) {
    Serve(std::move(url), PreviewReply{.status = ro::core::kHttpOk,
                                       .content_type = std::string(kHtml),
                                       .body = std::move(html)});
  }

  [[nodiscard]] ro::core::PreviewGet Transport() {
    return [this](std::string_view url, std::size_t /*most_bytes*/) {
      asked_.emplace_back(url);
      const auto found = replies_.find(std::string(url));
      if (found == replies_.end()) {
        return PreviewReply{.error = "inconnue"};
      }
      return found->second;
    };
  }
  [[nodiscard]] const std::vector<std::string>& asked() const { return asked_; }

 private:
  std::map<std::string, PreviewReply> replies_;
  std::vector<std::string> asked_;
};

[[nodiscard]] std::string OgPage(std::string_view picture) {
  return "<html><head><meta property=\"og:image:width\" content=\"1200\">"
         "<META content='" +
         std::string(picture) + "' Property=\"og:image\"></head></html>";
}

}  // namespace

TEST_CASE("Aperçu : la liste blanche d'usine et celle du joueur", "[core][web]") {
  const PreviewAllowList factory;
  CHECK(factory.AllowsHost("cdn.discordapp.com"));
  CHECK(factory.AllowsHost("media.discordapp.net"));
  CHECK(factory.AllowsHost("imgur.com"));
  CHECK(factory.AllowsHost("i.imgur.com"));
  CHECK(factory.AllowsHost("moonlight-destiny.fr"));
  CHECK_FALSE(factory.AllowsHost("discordapp.com.evil.io"));
  CHECK_FALSE(factory.AllowsHost("klipy.com"));
  CHECK_FALSE(factory.AllowsHost(""));

  const std::vector<std::string> mine = {"Klipy.com ", "x@y.fr", ".tenor.com"};
  const PreviewAllowList player(mine);
  CHECK(player.AllowsHost("klipy.com"));
  CHECK_FALSE(player.AllowsHost("cdn.klipy.com"));
  CHECK(player.AllowsHost("media.tenor.com"));
  CHECK_FALSE(player.AllowsHost("y.fr"));

  CHECK(factory.AllowsUrl("https://cdn.discordapp.com/a.png"));
  // HTTPS seulement, et jamais d'identité devant l'hôte.
  CHECK_FALSE(factory.AllowsUrl("http://cdn.discordapp.com/a.png"));
  CHECK_FALSE(factory.AllowsUrl("https://cdn.discordapp.com@evil.tld/a.png"));
  CHECK_FALSE(factory.AllowsUrl("https://x@cdn.discordapp.com/a.png"));
  CHECK_FALSE(factory.AllowsUrl("www.imgur.com"));
}

TEST_CASE("Aperçu : un accord pour cette fois ne vaut que pour l'adresse", "[core][web]") {
  PreviewAllowList allow;
  constexpr std::string_view kOther = "https://klipy.com/gifs/disco";
  CHECK_FALSE(allow.Previewable(kOther));
  allow.AllowOnce(kOther);
  CHECK(allow.AllowedOnce(kOther));
  CHECK(allow.Previewable(kOther));
  CHECK_FALSE(allow.Previewable("https://klipy.com/gifs/autre"));
  CHECK_FALSE(allow.AllowsHost("klipy.com"));
}

TEST_CASE("Aperçu : le miroir des images relayées", "[core][web]") {
  CHECK(ro::core::IsMirrorPicture("https://moonlight-destiny.fr/images/relay/a.png"));
  CHECK_FALSE(ro::core::IsMirrorPicture("https://moonlight-destiny.fr/images/relay/"));
  CHECK_FALSE(ro::core::IsMirrorPicture("https://moonlight-destiny.fr/images/a.png"));
  CHECK_FALSE(ro::core::IsMirrorPicture("http://moonlight-destiny.fr/images/relay/a.png"));
}

TEST_CASE("Aperçu : le type annoncé par le serveur", "[core][web]") {
  using ro::core::ClassifyContentType;
  CHECK(ClassifyContentType("image/png") == PreviewBody::kImage);
  CHECK(ClassifyContentType(" Image/GIF; charset=binary") == PreviewBody::kImage);
  CHECK(ClassifyContentType("text/html; charset=utf-8") == PreviewBody::kHtml);
  CHECK(ClassifyContentType("TEXT/HTML") == PreviewBody::kHtml);
  CHECK(ClassifyContentType("application/octet-stream") == PreviewBody::kRefused);
  CHECK(ClassifyContentType("image/") == PreviewBody::kRefused);
  CHECK(ClassifyContentType("") == PreviewBody::kRefused);
}

TEST_CASE("Aperçu : une destination relative se résout sur l'adresse", "[core][web]") {
  using ro::core::ResolveLocation;
  constexpr std::string_view kBase = "https://a.fr/dossier/page.html?q=1#f";
  CHECK(ResolveLocation(kBase, "https://b.fr/x") == "https://b.fr/x");
  CHECK(ResolveLocation(kBase, "//b.fr/x") == "https://b.fr/x");
  CHECK(ResolveLocation(kBase, "/x.png") == "https://a.fr/x.png");
  CHECK(ResolveLocation(kBase, "x.png") == "https://a.fr/dossier/x.png");
  CHECK(ResolveLocation(kBase, "../x.png?v=2") == "https://a.fr/x.png?v=2");
  CHECK(ResolveLocation(kBase, "./sous/") == "https://a.fr/dossier/sous/");
  CHECK(ResolveLocation(kBase, "?v=2") == "https://a.fr/dossier/page.html?v=2");
  CHECK(ResolveLocation("https://a.fr", "x.png") == "https://a.fr/x.png");
  CHECK(ResolveLocation(kBase, " /x ") == "https://a.fr/x");
}

TEST_CASE("Aperçu : l'og:image d'une page", "[core][web]") {
  using ro::core::FindOgImage;
  constexpr std::string_view kPage = "https://klipy.com/gifs/disco";
  const auto found = FindOgImage(OgPage("https://static.klipy.com/a.gif?x=1&amp;Y=2"), kPage);
  REQUIRE(found.has_value());
  // La casse de l'adresse est gardée : un jeton de signature y tient.
  CHECK(*found == "https://static.klipy.com/a.gif?x=1&Y=2");

  const auto relative = FindOgImage("<meta property=\"og:image\" content=\"/a.gif\">", kPage);
  REQUIRE(relative.has_value());
  CHECK(*relative == "https://klipy.com/a.gif");

  const auto secure =
      FindOgImage("<meta property='og:image:secure_url' content='https://k.com/s.gif'>", kPage);
  REQUIRE(secure.has_value());
  CHECK(*secure == "https://k.com/s.gif");

  CHECK_FALSE(FindOgImage("<html><head><title>og:image</title></head></html>", kPage).has_value());
  CHECK_FALSE(
      FindOgImage("<meta property=\"og:image:width\" content=\"1200\">", kPage).has_value());
  CHECK_FALSE(FindOgImage("", kPage).has_value());

  // Au-delà de la tête de page, la balise n'est plus cherchée.
  const std::string late = std::string(ro::core::kPreviewHtmlScanBytes, ' ') +
                           "<meta property=\"og:image\" content=\"https://k.com/a.gif\">";
  CHECK_FALSE(FindOgImage(late, kPage).has_value());
}

TEST_CASE("Aperçu : une image admise se télécharge", "[core][web]") {
  FakeWeb web;
  web.Picture("https://cdn.discordapp.com/a.png");
  const PreviewFetched fetched =
      FetchPreviewBytes("https://cdn.discordapp.com/a.png", PreviewAllowList(), web.Transport());
  REQUIRE(fetched.ok);
  CHECK(fetched.body == "PNG");
  CHECK(fetched.final_url == "https://cdn.discordapp.com/a.png");
}

TEST_CASE("Aperçu : l'hôte se revérifie à chaque saut", "[core][web]") {
  FakeWeb web;
  web.Redirect("https://cdn.discordapp.com/a.png", "https://evil.tld/a.png");
  web.Picture("https://evil.tld/a.png");
  const PreviewFetched fetched =
      FetchPreviewBytes("https://cdn.discordapp.com/a.png", PreviewAllowList(), web.Transport());
  CHECK_FALSE(fetched.ok);
  CHECK_FALSE(fetched.failure.empty());
  // 🔴 L'hôte hors liste n'a JAMAIS été contacté.
  CHECK(web.asked() == std::vector<std::string>{"https://cdn.discordapp.com/a.png"});

  // Un hôte qu'on n'a pas admis n'est même pas contacté au premier saut.
  FakeWeb other;
  other.Picture("https://evil.tld/a.png");
  CHECK_FALSE(
      FetchPreviewBytes("https://evil.tld/a.png", PreviewAllowList(), other.Transport()).ok);
  CHECK(other.asked().empty());
}

TEST_CASE("Aperçu : trois redirections au plus", "[core][web]") {
  FakeWeb web;
  web.Redirect("https://imgur.com/0", "https://imgur.com/1");
  web.Redirect("https://imgur.com/1", "/2");
  web.Redirect("https://imgur.com/2", "https://i.imgur.com/3");
  web.Picture("https://i.imgur.com/3");
  const PreviewFetched three =
      FetchPreviewBytes("https://imgur.com/0", PreviewAllowList(), web.Transport());
  REQUIRE(three.ok);
  CHECK(three.final_url == "https://i.imgur.com/3");

  // Un quatrième saut est refusé, et la quatrième destination jamais demandée.
  web.Redirect("https://i.imgur.com/3", "https://i.imgur.com/4");
  web.Picture("https://i.imgur.com/4");
  const PreviewFetched four =
      FetchPreviewBytes("https://imgur.com/0", PreviewAllowList(), web.Transport());
  CHECK_FALSE(four.ok);
  CHECK(web.asked().back() == "https://i.imgur.com/3");
}

TEST_CASE("Aperçu : une redirection vers HTTP est refusée", "[core][web]") {
  FakeWeb web;
  web.Redirect("https://imgur.com/a", "http://imgur.com/a");
  web.Picture("http://imgur.com/a");
  CHECK_FALSE(FetchPreviewBytes("https://imgur.com/a", PreviewAllowList(), web.Transport()).ok);
  CHECK(web.asked().size() == 1);
}

TEST_CASE("Aperçu : seul le statut 200 est une réussite", "[core][web]") {
  FakeWeb web;
  web.Serve(
      "https://imgur.com/a",
      PreviewReply{.status = kHttpNotFound, .content_type = std::string(kPng), .body = "PNG"});
  CHECK_FALSE(FetchPreviewBytes("https://imgur.com/a", PreviewAllowList(), web.Transport()).ok);

  web.Serve("https://imgur.com/b", PreviewReply{.status = kHttpFound});
  CHECK_FALSE(FetchPreviewBytes("https://imgur.com/b", PreviewAllowList(), web.Transport()).ok);

  CHECK_FALSE(
      FetchPreviewBytes("https://imgur.com/absente", PreviewAllowList(), web.Transport()).ok);
}

TEST_CASE("Aperçu : une page se suit par son og:image, une fois", "[core][web]") {
  constexpr std::string_view kPage = "https://imgur.com/gallery/x";
  FakeWeb web;
  web.Page(std::string(kPage), OgPage("https://i.imgur.com/x.gif"));
  web.Picture("https://i.imgur.com/x.gif");
  const PreviewFetched fetched = FetchPreviewBytes(kPage, PreviewAllowList(), web.Transport());
  REQUIRE(fetched.ok);
  CHECK(fetched.final_url == "https://i.imgur.com/x.gif");

  // L'og:image hors liste est refusée sans être contactée.
  FakeWeb outside;
  outside.Page(std::string(kPage), OgPage("https://tracker.tld/x.gif"));
  outside.Picture("https://tracker.tld/x.gif");
  CHECK_FALSE(FetchPreviewBytes(kPage, PreviewAllowList(), outside.Transport()).ok);
  CHECK(outside.asked().size() == 1);

  // Une og:image qui rend encore une page : jamais de page en page.
  FakeWeb chained;
  chained.Page(std::string(kPage), OgPage("https://i.imgur.com/page"));
  chained.Page("https://i.imgur.com/page", OgPage("https://i.imgur.com/x.gif"));
  chained.Picture("https://i.imgur.com/x.gif");
  CHECK_FALSE(FetchPreviewBytes(kPage, PreviewAllowList(), chained.Transport()).ok);
  CHECK(chained.asked().size() == 2);

  // Une page sans balise, ou trop lourde, ne rend rien.
  FakeWeb empty;
  empty.Page(std::string(kPage), "<html></html>");
  CHECK_FALSE(FetchPreviewBytes(kPage, PreviewAllowList(), empty.Transport()).ok);
  FakeWeb heavy;
  heavy.Page(std::string(kPage), OgPage("https://i.imgur.com/x.gif") +
                                     std::string(ro::core::kPreviewMostHtmlBytes, ' '));
  heavy.Picture("https://i.imgur.com/x.gif");
  CHECK_FALSE(FetchPreviewBytes(kPage, PreviewAllowList(), heavy.Transport()).ok);
}

TEST_CASE("Aperçu : un accord vaut pour l'hôte de l'adresse accordée", "[core][web]") {
  constexpr std::string_view kPage = "https://klipy.com/gifs/disco";
  FakeWeb web;
  web.Page(std::string(kPage), OgPage("https://klipy.com/disco.gif"));
  web.Picture("https://klipy.com/disco.gif");
  PreviewAllowList allow;
  CHECK_FALSE(FetchPreviewBytes(kPage, allow, web.Transport()).ok);
  CHECK(web.asked().empty());
  allow.AllowOnce(kPage);
  CHECK(FetchPreviewBytes(kPage, allow, web.Transport()).ok);

  // 🔴 Un autre hôte, même voisin, n'est pas couvert par l'accord : il n'est
  // jamais contacté.
  FakeWeb elsewhere;
  elsewhere.Page(std::string(kPage), OgPage("https://static.klipy.com/disco.gif"));
  elsewhere.Picture("https://static.klipy.com/disco.gif");
  CHECK_FALSE(FetchPreviewBytes(kPage, allow, elsewhere.Transport()).ok);
  CHECK(elsewhere.asked().size() == 1);

  // Un hôte de la liste, lui, reste joignable.
  FakeWeb listed;
  listed.Redirect(std::string(kPage), "https://i.imgur.com/disco.gif");
  listed.Picture("https://i.imgur.com/disco.gif");
  CHECK(FetchPreviewBytes(kPage, allow, listed.Transport()).ok);

  // Mais jamais en clair, accord ou pas.
  FakeWeb plain;
  plain.Picture("http://klipy.com/a.gif");
  PreviewAllowList plain_allow;
  plain_allow.AllowOnce("http://klipy.com/a.gif");
  CHECK_FALSE(FetchPreviewBytes("http://klipy.com/a.gif", plain_allow, plain.Transport()).ok);
  CHECK(plain.asked().empty());
}

TEST_CASE("Aperçu : une réponse trop lourde est refusée", "[core][web]") {
  FakeWeb web;
  web.Serve("https://imgur.com/a",
            PreviewReply{.status = ro::core::kHttpOk,
                         .content_type = std::string(kPng),
                         .declared_length = ro::core::kPreviewMostBytes + 1});
  CHECK_FALSE(FetchPreviewBytes("https://imgur.com/a", PreviewAllowList(), web.Transport()).ok);

  web.Serve("https://imgur.com/b", PreviewReply{.status = ro::core::kHttpOk,
                                                .content_type = std::string(kPng),
                                                .body = "PNG",
                                                .declared_length = ro::core::kPreviewMostBytes});
  CHECK(FetchPreviewBytes("https://imgur.com/b", PreviewAllowList(), web.Transport()).ok);
}

TEST_CASE("Aperçu : le type refusé ne se décode pas", "[core][web]") {
  FakeWeb web;
  web.Serve("https://imgur.com/a", PreviewReply{.status = ro::core::kHttpOk,
                                                .content_type = "application/javascript",
                                                .body = "x"});
  CHECK_FALSE(FetchPreviewBytes("https://imgur.com/a", PreviewAllowList(), web.Transport()).ok);
}

TEST_CASE("Aperçu : le registre tient trois téléchargements en vol", "[core][web]") {
  PreviewBook book;
  CHECK(book.Start("a"));
  CHECK(book.Start("b"));
  CHECK(book.Start("c"));
  CHECK(book.in_flight() == 3);
  CHECK_FALSE(book.Start("d"));
  // Rien n'est noté pour une demande refusée : elle retentera.
  CHECK(book.State("d") == PreviewState::kUnknown);
  CHECK_FALSE(book.Start("a"));
  CHECK(book.State("a") == PreviewState::kPending);

  book.Ready("a");
  CHECK(book.State("a") == PreviewState::kReady);
  CHECK(book.in_flight() == 2);
  CHECK(book.Start("d"));
}

TEST_CASE("Aperçu : un échec reste, et ne se relance pas", "[core][web]") {
  PreviewBook book;
  REQUIRE(book.Start("a"));
  book.Failed("a");
  CHECK(book.State("a") == PreviewState::kFailed);
  CHECK(book.in_flight() == 0);
  CHECK_FALSE(book.Start("a"));
  CHECK(book.State("a") == PreviewState::kFailed);
  // Un second rendu ne change rien.
  book.Ready("a");
  CHECK(book.State("a") == PreviewState::kFailed);

  // Un accord neuf l'oublie, et elle repart.
  book.Forget("a");
  CHECK(book.State("a") == PreviewState::kUnknown);
  CHECK(book.Start("a"));
  // Une entrée en vol ne s'oublie pas.
  book.Forget("a");
  CHECK(book.State("a") == PreviewState::kPending);
}

TEST_CASE("Aperçu : l'éviction suit l'ordre d'usage, jamais une entrée en vol", "[core][web]") {
  PreviewBook book;
  const auto fill = [&](const std::string& key) {
    REQUIRE(book.Start(key));
    book.Ready(key);
  };
  for (std::size_t index = 0; index < ro::core::kPreviewCacheEntries; ++index) {
    fill("k" + std::to_string(index));
  }
  CHECK(book.Evict().empty());

  // La plus ancienne est servie : c'est la suivante qui part.
  book.Touch("k0");
  fill("k-neuve");
  const std::vector<std::string> evicted = book.Evict();
  REQUIRE(evicted.size() == 1);
  CHECK(evicted.front() == "k1");
  CHECK(book.State("k0") == PreviewState::kReady);
  CHECK(book.State("k1") == PreviewState::kUnknown);

  // Une entrée en vol n'est jamais évincée, même la plus ancienne.
  PreviewBook small(1);
  REQUIRE(small.Start("vol"));
  REQUIRE(small.Start("faite"));
  small.Ready("faite");
  const std::vector<std::string> gone = small.Evict();
  REQUIRE(gone.size() == 1);
  CHECK(gone.front() == "faite");
  CHECK(small.State("vol") == PreviewState::kPending);
  CHECK(small.Evict().empty());
}

TEST_CASE("Aperçu : jamais un hôte local, numérique ou d'une seule étiquette", "[core][web]") {
  // Même admis par le joueur, même accordés : l'aperçu ne sonde pas sa machine.
  const std::vector<std::string> mine = {"127.0.0.1", "nas.lan", "box.local", "x.internal",
                                         "printer.home"};
  PreviewAllowList allow(mine);
  for (const std::string_view url :
       {"https://127.0.0.1/a.png", "https://[::1]/a.png", "https://localhost/a.png",
        "https://intranet/a.png", "https://nas.lan/a.png", "https://box.local/a.png",
        "https://x.internal/a.png", "https://printer.home/a.png", "https://10.0.0.1/a.png",
        "https://1.0x7f/a.png"}) {
    INFO(url);
    allow.AllowOnce(url);
    CHECK_FALSE(allow.AllowsUrl(url));
    FakeWeb web;
    web.Picture(std::string(url));
    CHECK_FALSE(FetchPreviewBytes(url, allow, web.Transport()).ok);
    CHECK(web.asked().empty());
  }
  // Un nom public dont une étiquette est numérique reste un nom public.
  CHECK(PreviewAllowList().AllowsUrl("https://127.0.0.1.discordapp.com/a.png"));

  // Une redirection vers la machine du joueur est refusée sans être suivie.
  FakeWeb web;
  web.Redirect("https://imgur.com/a", "https://127.0.0.1:8080/admin");
  web.Picture("https://127.0.0.1:8080/admin");
  CHECK_FALSE(FetchPreviewBytes("https://imgur.com/a", PreviewAllowList(), web.Transport()).ok);
  CHECK(web.asked().size() == 1);
}

TEST_CASE("Aperçu : une règle trop large ne s'admet pas", "[core][web]") {
  const std::vector<std::string> mine = {".com", ".", "..discordapp.com", "com"};
  const PreviewAllowList allow(mine);
  CHECK_FALSE(allow.AllowsHost("evil.com"));
  CHECK_FALSE(allow.AllowsUrl("https://evil.com/x"));
  CHECK_FALSE(allow.AllowsUrl("https://evil.tld./x"));
}

TEST_CASE("Aperçu : un hôte trompeur ou un octet de contrôle est refusé", "[core][web]") {
  const std::vector<std::string> mine = {"xn--bcher-kva.example"};
  const PreviewAllowList allow(mine);
  CHECK_FALSE(allow.AllowsUrl("https://xn--bcher-kva.example/a.png"));
  CHECK_FALSE(allow.AllowsUrl("https://\xC3\xA9.imgur.com/a.png"));
  CHECK_FALSE(allow.AllowsUrl("https://imgur.com/a b.png"));
  CHECK_FALSE(allow.AllowsUrl("https://imgur.com/a\x7F.png"));
  CHECK(allow.AllowsUrl("https://imgur.com/a%20b.png"));

  // 🔴 Une destination qui glisse un en-tête de plus n'est jamais demandée.
  FakeWeb web;
  web.Redirect("https://imgur.com/a", "https://i.imgur.com/b\r\nX-Injecte: oui");
  CHECK_FALSE(FetchPreviewBytes("https://imgur.com/a", allow, web.Transport()).ok);
  CHECK(web.asked().size() == 1);
}

TEST_CASE("Aperçu : une page annoncée trop lourde est refusée", "[core][web]") {
  FakeWeb web;
  web.Serve("https://imgur.com/page",
            PreviewReply{.status = ro::core::kHttpOk,
                         .content_type = std::string(kHtml),
                         .body = OgPage("https://i.imgur.com/x.gif"),
                         .declared_length = ro::core::kPreviewMostHtmlBytes + 1});
  web.Picture("https://i.imgur.com/x.gif");
  CHECK_FALSE(FetchPreviewBytes("https://imgur.com/page", PreviewAllowList(), web.Transport()).ok);
}
