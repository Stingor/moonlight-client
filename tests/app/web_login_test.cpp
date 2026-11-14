// LE PARCOURS DU LOGIN WEB, sur un transport qui répond ce qu'on veut.
//
// 🔴 CE QUI SE VÉRIFIE ICI N'EST PAS « SAIT-IL SE CONNECTER ». C'est ce que le
// parcours fait de ce qui va mal, et il y a de quoi : un site injoignable, une
// page d'erreur HTML à la place du JSON, un jeton qui vieillit pendant que le
// joueur hésite, une session Discord qu'on interroge indéfiniment. Aucun de ces
// cas ne se provoque à la main contre le vrai site.

#include "web/web_login.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

using moonclient::web::kDiscordPollFloorMs;
using moonclient::web::kWebTicketLifeMs;
using moonclient::web::WebLogin;
using moonclient::web::WebReply;
using moonclient::web::WebRequest;
using moonclient::web::WebStage;
using moonclient::web::WebTransport;

namespace {

// UN TRANSPORT QUI DIT CE QU'ON LUI A DIT DE DIRE, et qui RETIENT ce qu'on lui
// a demandé — c'est la moitié du banc : ce qui part sur le fil compte autant
// que ce qui revient.
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

  // Ce que l'on rendra, dans l'ordre.
  std::deque<WebReply> queued;
  // Ce qui est parti, dans l'ordre.
  std::vector<WebRequest> sent;

 private:
  bool in_flight_ = false;
};

[[nodiscard]] WebReply Answered(std::string body) {
  return WebReply{.status = 200, .body = std::move(body), .error = {}};
}

// La réponse d'`action=auth` : un jeton, deux comptes.
[[nodiscard]] std::string AccountsBody() {
  return R"({"ok":true,"web_ticket":"1.2.abc","accounts":[)"
         R"({"account_id":2000000,"userid":"stingor","label":"stingor","char_count":3,)"
         R"("group_id":99,"last_login":"2026-09-20","banned":false,"online":false,"autotrade":false},)"
         R"({"account_id":2000001,"userid":"second","label":"second","char_count":0,)"
         R"("last_login":"","banned":true,"online":true,"autotrade":true}]})";
}

// Ce qu'un champ de requête porte, ou vide.
[[nodiscard]] std::string FieldOf(const WebRequest& request, std::string_view name) {
  for (const auto& field : request.fields) {
    if (field.first == name) {
      return field.second;
    }
  }
  return {};
}

constexpr std::string_view kOrigin = "https://exemple.test";

// UNE MAISONNÉE DE COMPTES, dans le DÉSORDRE que le site rend : un banni en
// tête, une boutique, un connecté, et deux jouables dont le plus récent est
// le dernier de la liste.
[[nodiscard]] std::string MixedAccountsBody() {
  return R"({"ok":true,"web_ticket":"1.2.abc","accounts":[)"
         R"({"account_id":1,"userid":"banni","label":"banni","char_count":1,)"
         R"("last_login":"2026-09-21","banned":true,"online":false,"autotrade":false},)"
         R"({"account_id":2,"userid":"echoppe","label":"echoppe","char_count":2,)"
         R"("last_login":"2026-09-21","banned":false,"online":true,"autotrade":true},)"
         R"({"account_id":3,"userid":"connecte","label":"connecte","char_count":3,)"
         R"("last_login":"2026-09-21","banned":false,"online":true,"autotrade":false},)"
         R"({"account_id":4,"userid":"vieux","label":"vieux","char_count":4,)"
         R"("last_login":"2026-01-02","banned":false,"online":false,"autotrade":false},)"
         R"({"account_id":5,"userid":"hier","label":"hier","char_count":5,)"
         R"("last_login":"2026-09-20","banned":false,"online":false,"autotrade":false}]})";
}

// Mène le parcours jusqu'à la liste, avec le corps donné.
void SignInWith(WebLogin& login, FakeTransport& wire, std::string body) {
  login.SignIn("membre", "secret");
  wire.queued.push_back(Answered(std::move(body)));
  login.Advance(1000);
}

}  // namespace

TEST_CASE("Login web : ce qui se joue remonte, ce qui ne se joue pas descend", "[app][weblogin]") {
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  SignInWith(login, wire, MixedAccountsBody());

  REQUIRE(login.stage() == WebStage::kAccounts);
  REQUIRE(login.accounts().size() == 5);

  // 🔴 QUATRE RANGS, ET ILS DISENT CE QUE COÛTE UN CLIC : rien, une boutique,
  // une session, un refus. L'ordre du site ne dit rien d'utile.
  CHECK(login.accounts()[0].userid == "vieux");
  CHECK(login.accounts()[1].userid == "hier");
  CHECK(login.accounts()[2].userid == "echoppe");
  CHECK(login.accounts()[3].userid == "connecte");
  CHECK(login.accounts()[4].userid == "banni");
}

TEST_CASE("Login web : Entrée joue le dernier compte joué, jamais un occupé", "[app][weblogin]") {
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  SignInWith(login, wire, MixedAccountsBody());

  // 🔴 « hier » ET NON « echoppe » NI « banni », qui portent pourtant la date
  // la plus récente. Présélectionner un compte occupé mettrait sa conséquence
  // — fermer une boutique, faire tomber une session — sous une touche Entrée
  // appuyée par réflexe.
  const int preferred = login.preferred_account();
  REQUIRE(preferred >= 0);
  CHECK(login.accounts()[static_cast<std::size_t>(preferred)].userid == "hier");
}

TEST_CASE("Login web : sans aucun compte libre, la désignation reste valide", "[app][weblogin]") {
  // ⚠ ELLE NE REND PAS -1 POUR AUTANT : l'écran a besoin d'une ligne à
  // montrer, et c'est son bouton qui se grise. Rendre « rien » ferait une
  // liste sans surbrillance devant un joueur qui cherche quoi cliquer.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  SignInWith(login, wire,
             R"({"ok":true,"web_ticket":"1.2.abc","accounts":[)"
             R"({"account_id":1,"userid":"a","label":"a","char_count":1,)"
             R"("last_login":"2026-01-01","banned":false,"online":true,"autotrade":false},)"
             R"({"account_id":2,"userid":"b","label":"b","char_count":1,)"
             R"("last_login":"2026-02-02","banned":true,"online":false,"autotrade":false}]})");

  REQUIRE(login.accounts().size() == 2);
  const int preferred = login.preferred_account();
  REQUIRE(preferred >= 0);
  CHECK(preferred < static_cast<int>(login.accounts().size()));
}

TEST_CASE("Login web : du compte du forum à l'OTP", "[app][weblogin]") {
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  CHECK(login.stage() == WebStage::kIdle);

  login.SignIn("membre", "secret");
  REQUIRE(wire.sent.size() == 1);
  CHECK(FieldOf(wire.sent[0], "action") == "auth");
  CHECK(FieldOf(wire.sent[0], "username") == "membre");
  CHECK(FieldOf(wire.sent[0], "password") == "secret");
  CHECK(login.stage() == WebStage::kSigningIn);

  wire.queued.push_back(Answered(AccountsBody()));
  login.Advance(1000);
  REQUIRE(login.stage() == WebStage::kAccounts);
  REQUIRE(login.accounts().size() == 2);
  CHECK(login.accounts()[0].userid == "stingor");
  CHECK(login.accounts()[0].characters == 3);
  CHECK(login.accounts()[0].group_id == 99);
  // Un site qui ne dit pas le groupe laisse un joueur ordinaire.
  CHECK(login.accounts()[1].group_id == 0);
  CHECK_FALSE(login.accounts()[0].banned);
  // ⚠ CES TROIS-LÀ SE MONTRENT AVANT LE CLIC : un compte banni ne se
  // connectera pas, un compte en ligne fera tomber l'autre session, et un
  // compte en vente automatique est en ligne pour une raison.
  CHECK(login.accounts()[1].banned);
  CHECK(login.accounts()[1].online);
  CHECK(login.accounts()[1].autotrade);

  login.ChooseAccount(2'000'000);
  REQUIRE(wire.sent.size() == 2);
  CHECK(FieldOf(wire.sent[1], "action") == "select");
  CHECK(FieldOf(wire.sent[1], "web_ticket") == "1.2.abc");
  CHECK(FieldOf(wire.sent[1], "account_id") == "2000000");

  wire.queued.push_back(Answered(R"({"ok":true,"userid":"stingor","otp":"0123456789abcdef"})"));
  login.Advance(2000);
  REQUIRE(login.stage() == WebStage::kReady);
  CHECK(login.userid() == "stingor");
  CHECK(login.otp() == "0123456789abcdef");
  // 🔴 L'OTP TIENT DANS LE CHAMP DE `CA_LOGIN`, et ce n'est pas un hasard : le
  // site le fait court — seize caractères — PARCE QUE le mot de passe du
  // client est borné à vingt-quatre octets. Un OTP plus long serait tronqué,
  // et son refus se lirait « mot de passe incorrect ».
  CHECK(login.otp().size() <= 24);

  // ⚠ L'OUBLI EFFACE L'OTP : il vaut un mot de passe, et il a servi.
  login.Forget();
  CHECK(login.otp().empty());
  CHECK(login.stage() == WebStage::kIdle);
}

TEST_CASE("Login web : le motif du refus vient du SITE", "[app][weblogin]") {
  // Le site écrit ses refus en français et les connaît mieux que nous :
  // compte inconnu, mot de passe faux, trop d'essais. Les réécrire ici
  // donnerait deux vérités, dont une périmée.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  login.SignIn("membre", "faux");
  wire.queued.push_back(
      WebReply{.status = 429, .body = R"({"ok":false,"error":"Trop de tentatives."})"});
  login.Advance(1000);

  CHECK(login.stage() == WebStage::kFailed);
  CHECK(login.message() == "Trop de tentatives.");
  CHECK(login.accounts().empty());
}

TEST_CASE("Login web : une page HTML n'est pas un message", "[app][weblogin]") {
  // 🔴 C'EST CE QU'UN SERVEUR EN PANNE ENVOIE : une notice PHP, une page
  // d'erreur du serveur frontal, une porte captive. Recopier deux kilo-octets
  // de HTML dans le bandeau donnerait du charabia là où le joueur cherche une
  // raison.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  login.SignIn("membre", "secret");
  wire.queued.push_back(Answered("<html><body>503 Service Unavailable</body></html>"));
  login.Advance(1000);

  CHECK(login.stage() == WebStage::kFailed);
  CHECK(login.message().find("illisible") != std::string::npos);
  CHECK(login.message().find("<html>") == std::string::npos);
}

TEST_CASE("Login web : un site injoignable se dit autrement qu'un refus", "[app][weblogin]") {
  // ⚠ UN TRANSPORT QUI ÉCHOUE N'EST PAS UN SERVEUR QUI REFUSE. Le premier veut
  // dire « réessaie, ou regarde ta connexion » ; le second « ton mot de passe
  // est faux ». Les confondre envoie le joueur chercher au mauvais endroit.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  login.SignIn("membre", "secret");
  wire.queued.push_back(WebReply{.status = 0, .body = {}, .error = "nom d'hôte introuvable"});
  login.Advance(1000);

  CHECK(login.stage() == WebStage::kFailed);
  CHECK(login.message() == "nom d'hôte introuvable");
}

TEST_CASE("Login web : le jeton vieillit pendant que le joueur choisit", "[app][weblogin]") {
  // 🔴 DEUX MINUTES, ET LE COMPTE À REBOURS PART À L'ARRIVÉE DE LA LISTE. Un
  // joueur qui va chercher un café dépasse ce délai sans rien faire de mal, et
  // le site refuse alors son choix avec un message qui parle de signature.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  login.SignIn("membre", "secret");
  wire.queued.push_back(Answered(AccountsBody()));
  login.Advance(1000);
  REQUIRE(login.stage() == WebStage::kAccounts);

  // Juste avant l'échéance, rien ne change.
  login.Advance(1000 + kWebTicketLifeMs - 1);
  CHECK(login.stage() == WebStage::kAccounts);

  login.Advance(1000 + kWebTicketLifeMs);
  CHECK(login.stage() == WebStage::kFailed);
  CHECK(login.message().find("expir") != std::string::npos);
  // ⚠ ET LE CHOIX NE PART PLUS : un jeton mort n'obtient rien, et l'envoyer
  // ferait répondre au site une phrase que le joueur ne peut pas interpréter.
  const std::size_t before = wire.sent.size();
  login.ChooseAccount(2'000'000);
  CHECK(wire.sent.size() == before);
}

TEST_CASE("Login web : la porte Discord s'ouvre sur NOTRE origine", "[app][weblogin]") {
  // 🔴 LE SITE NE REND QU'UN CHEMIN, et son commentaire dit pourquoi : son
  // propre `generate_board_url` pointe sur le forum, alors que la page
  // d'autorisation est à la racine du domaine. La seule origine juste est
  // celle par laquelle nous l'appelons.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  login.StartDiscord();
  REQUIRE(wire.sent.size() == 1);
  CHECK(FieldOf(wire.sent[0], "action") == "discord_start");

  wire.queued.push_back(Answered(
      R"({"ok":true,"game_session":"deadbeef","authorize_path":"/oauth_discord.php?game=deadbeef",)"
      R"("poll_interval":2,"ttl":600})"));
  login.Advance(1000);

  CHECK(login.stage() == WebStage::kWaitingDiscord);
  CHECK(login.browser_url() == "https://exemple.test/oauth_discord.php?game=deadbeef");
}

TEST_CASE("Login web : Discord s'interroge au RYTHME du site, jamais plus vite",
          "[app][weblogin]") {
  // ⚠ UN SITE QUI RÉPONDRAIT ZÉRO NOUS FERAIT L'INTERROGER À CHAQUE IMAGE,
  // soixante fois par seconde : un déni de service qu'on s'inflige, sur une
  // porte qui attend un humain dans son navigateur.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  login.StartDiscord();
  wire.queued.push_back(Answered(
      R"({"ok":true,"game_session":"deadbeef","authorize_path":"/x","poll_interval":0,"ttl":600})"));
  login.Advance(1000);
  REQUIRE(wire.sent.size() == 1);

  // Avant le plancher, rien ne part.
  login.Advance(1000 + kDiscordPollFloorMs - 1);
  CHECK(wire.sent.size() == 1);

  login.Advance(1000 + kDiscordPollFloorMs);
  REQUIRE(wire.sent.size() == 2);
  CHECK(FieldOf(wire.sent[1], "action") == "discord_poll");
  CHECK(FieldOf(wire.sent[1], "game_session") == "deadbeef");

  // « Pas encore » ne change rien à l'écran : le joueur est dans son
  // navigateur, et l'on repasse.
  wire.queued.push_back(Answered(R"({"ok":true,"pending":true})"));
  login.Advance(1000 + kDiscordPollFloorMs + 1);
  CHECK(login.stage() == WebStage::kWaitingDiscord);

  // Puis elle se résout, et rend la MÊME chose qu'`auth`.
  login.Advance(1000 + 2 * kDiscordPollFloorMs + 1);
  wire.queued.push_back(Answered(AccountsBody()));
  login.Advance(1000 + 3 * kDiscordPollFloorMs);
  CHECK(login.stage() == WebStage::kAccounts);
  CHECK(login.accounts().size() == 2);
}

TEST_CASE("Login web : une session Discord expirée s'arrête", "[app][weblogin]") {
  // Sans cette borne, le client interrogerait le site toutes les deux secondes
  // pour l'éternité, pour une session que personne ne résoudra jamais.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  login.StartDiscord();
  wire.queued.push_back(Answered(
      R"({"ok":true,"game_session":"deadbeef","authorize_path":"/x","poll_interval":2,"ttl":5})"));
  login.Advance(1000);
  REQUIRE(login.stage() == WebStage::kWaitingDiscord);

  login.Advance(1000 + 5 * 1000);
  CHECK(login.stage() == WebStage::kFailed);
  CHECK(login.message().find("expir") != std::string::npos);
  // Et plus rien ne part.
  const std::size_t before = wire.sent.size();
  login.Advance(1000 + 60 * 1000);
  CHECK(wire.sent.size() == before);
}

TEST_CASE("Login web : une session Discord sans durée de vie n'expire pas", "[app][weblogin]") {
  // 🔴 `ttl` ABSENT VALAIT « EXPIRÉE » : l'échéance tombait sur l'instant même,
  // et la connexion échouait au tour suivant sans que le joueur ait rien fait.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  login.StartDiscord();
  wire.queued.push_back(
      Answered(R"({"ok":true,"game_session":"deadbeef","authorize_path":"/x","poll_interval":2})"));
  login.Advance(1000);
  REQUIRE(login.stage() == WebStage::kWaitingDiscord);

  login.Advance(1000 + 1);
  CHECK(login.stage() == WebStage::kWaitingDiscord);
}

TEST_CASE("Login web : un compte web sans compte de jeu se DIT", "[app][weblogin]") {
  // ⚠ LE COMPTE EXISTE, il n'a simplement rien de lié. « Choisis un compte »
  // devant une liste vide est pire qu'une phrase, et le joueur chercherait ce
  // qu'il a mal tapé.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  login.SignIn("membre", "secret");
  wire.queued.push_back(Answered(R"({"ok":true,"web_ticket":"1.2.abc","accounts":[]})"));
  login.Advance(1000);

  CHECK(login.stage() == WebStage::kFailed);
  CHECK(login.message().find("aucun compte de jeu") != std::string::npos);
}

TEST_CASE("Login web : deux demandes ne se chevauchent pas", "[app][weblogin]") {
  // Le parcours est une séquence. Une seconde identification lancée par-dessus
  // la première — un double clic, une touche Entrée répétée — laisserait deux
  // réponses arriver pour un seul écran.
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  login.SignIn("membre", "secret");
  login.SignIn("membre", "secret");
  login.StartDiscord();
  CHECK(wire.sent.size() == 1);
}

TEST_CASE("Login web : changer de site abandonne le parcours et vise la nouvelle origine",
          "[app][weblogin]") {
  FakeTransport wire;
  WebLogin login(wire, std::string(kOrigin));
  SignInWith(login, wire, AccountsBody());
  REQUIRE(login.stage() == WebStage::kAccounts);

  // 🔴 LE JETON VALAIT POUR L'AUTRE SITE, donc pour la base d'un autre serveur.
  login.Retarget("https://essai.local:8443");
  CHECK(login.origin() == "https://essai.local:8443");
  CHECK(login.stage() == WebStage::kIdle);
  CHECK(login.accounts().empty());
  CHECK_FALSE(wire.busy());
}
