// LE PARCOURS DU LOGIN WEB : du compte du forum à un OTP que `CA_LOGIN` avale.

#include "web/web_login.h"

#include <algorithm>
#include <utility>

#include "ro/core/log.h"

#include "web/json_value.h"

namespace moonclient::web {
namespace {

// CE QUE COÛTE UN CLIC SUR CE COMPTE, en quatre rangs. Le tri s'en sert, et
// l'ordre dit l'intention : ce qui ne coûte rien d'abord, ce qui ne se joue
// pas en dernier.
// CE QU'ON PEUT PRÉSÉLECTIONNER SANS RIEN CASSER.
//
// 🔴 PLUS ÉTROIT QUE « JOUABLE », et les confondre coûterait la session d'à
// côté. Un compte en ligne SE JOUE — l'écran le propose, avec sa conséquence
// écrite au-dessus du bouton. Mais le PRÉSÉLECTIONNER mettrait cette
// conséquence sous une touche Entrée appuyée par réflexe, sans que personne
// ait rien lu. Même chose pour une boutique, qui coûte davantage encore.
[[nodiscard]] bool SafeToPrefer(const WebAccount& account) {
  return !account.banned && !account.online && !account.autotrade;
}

[[nodiscard]] int AccountRank(const WebAccount& account) {
  if (account.banned) {
    return 3;
  }
  if (account.autotrade) {
    return 1;
  }
  return account.online ? 2 : 0;
}

// LE CODE QUE LE SITE REND QUAND SA SESSION DISCORD A EXPIRÉ. Il porte aussi
// son motif dans le corps ; le code dit seulement qu'il ne sert à rien
// d'interroger encore.
constexpr int kHttpGone = 410;

// CE QU'ON MONTRE QUAND LE SITE N'A RIEN D'INTELLIGIBLE À DIRE.
//
// ⚠ IL NE RECOPIE PAS LE CORPS : une page d'erreur HTML de deux kilo-octets
// n'est pas un message, et la coller dans un bandeau donnerait du charabia là
// où le joueur cherche une raison.
constexpr std::string_view kUnreadable = "le site a répondu quelque chose d'illisible";

}  // namespace

void WebLogin::Fail(std::string why) {
  stage_ = WebStage::kFailed;
  awaiting_ = Awaiting::kNothing;
  message_ = std::move(why);
  // ⚠ LE JETON ET L'OTP PARTENT AVEC L'ÉCHEC : ce qui reste après un refus
  // n'est plus valable, et le garder ferait réessayer avec un jeton mort.
  ticket_.clear();
  otp_.clear();
  discord_session_.clear();
}

void WebLogin::Send(const WebRequest& request, Awaiting awaiting, std::uint32_t now_ms) {
  transport_->Begin(request);
  awaiting_ = awaiting;
  discord_next_ms_ = now_ms;
}

void WebLogin::SignIn(std::string_view username, std::string_view password) {
  if (stage_ == WebStage::kSigningIn || stage_ == WebStage::kSelecting ||
      stage_ == WebStage::kWaitingDiscord) {
    return;
  }
  Forget();
  stage_ = WebStage::kSigningIn;
  awaiting_ = Awaiting::kAccounts;
  // ⚠ LE MOT DE PASSE NE VA NULLE PART AILLEURS QUE DANS CETTE REQUÊTE : il
  // n'est ni retenu par cette classe ni journalisé, et c'est le seul endroit du
  // parcours où il paraît.
  transport_->Begin(WebRequest{.path = std::string(kWebLoginPath),
                               .fields = {{"action", "auth"},
                                          {"username", std::string(username)},
                                          {"password", std::string(password)}}});
}

void WebLogin::StartDiscord() {
  if (stage_ == WebStage::kSigningIn || stage_ == WebStage::kSelecting ||
      stage_ == WebStage::kWaitingDiscord) {
    return;
  }
  Forget();
  stage_ = WebStage::kWaitingDiscord;
  awaiting_ = Awaiting::kDiscordStart;
  transport_->Begin(
      WebRequest{.path = std::string(kWebLoginPath), .fields = {{"action", "discord_start"}}});
}

void WebLogin::ChooseAccount(std::int64_t account_id) {
  if (stage_ != WebStage::kAccounts) {
    return;
  }
  stage_ = WebStage::kSelecting;
  awaiting_ = Awaiting::kOtp;
  transport_->Begin(WebRequest{.path = std::string(kWebLoginPath),
                               .fields = {{"action", "select"},
                                          {"web_ticket", ticket_},
                                          {"account_id", std::to_string(account_id)}}});
}

void WebLogin::Forget() noexcept {
  transport_->Abandon();
  stage_ = WebStage::kIdle;
  awaiting_ = Awaiting::kNothing;
  message_.clear();
  accounts_.clear();
  ticket_.clear();
  ticket_since_ms_ = 0;
  userid_.clear();
  // 🔴 L'OTP VAUT UN MOT DE PASSE : il s'efface, il ne se laisse pas expirer.
  otp_.clear();
  browser_url_.clear();
  discord_session_.clear();
  discord_until_ms_ = 0;
}

void WebLogin::TakeAccounts(const JsonValue& body, std::uint32_t now_ms) {
  ticket_ = std::string(body.TextAt("web_ticket"));
  if (ticket_.empty()) {
    Fail(std::string(kUnreadable));
    return;
  }
  ticket_since_ms_ = now_ms;
  accounts_.clear();
  const JsonValue* listed = body.Field("accounts");
  if (listed != nullptr) {
    for (const JsonValue& entry : listed->Items()) {
      WebAccount account;
      account.account_id = entry.WholeAt("account_id");
      account.userid = std::string(entry.TextAt("userid"));
      account.label = std::string(entry.TextAt("label"));
      account.characters = static_cast<int>(entry.WholeAt("char_count"));
      account.group_id = static_cast<int>(entry.WholeAt("group_id"));
      account.last_login = std::string(entry.TextAt("last_login"));
      account.banned = entry.TruthAt("banned");
      account.online = entry.TruthAt("online");
      account.autotrade = entry.TruthAt("autotrade");
      // ⚠ UN COMPTE SANS NOM NE SERT À RIEN : c'est lui que `CA_LOGIN`
      // portera, et une ligne vide dans la liste est un clic qui échoue.
      if (!account.userid.empty()) {
        accounts_.push_back(std::move(account));
      }
    }
  }
  // ⚠ UNE LISTE VIDE EST UNE RÉPONSE, et une réponse qu'il faut dire : le
  // compte web existe, il n'a simplement aucun compte de jeu lié. L'écran ne
  // pourrait rien en faire, et « choisis un compte » devant rien est pire
  // qu'une phrase.
  if (accounts_.empty()) {
    Fail("ce compte Moonlight n'a aucun compte de jeu lié");
    return;
  }

  // 🔴 CE QU'ON PEUT JOUER REMONTE, et l'ordre du site ne dit rien d'utile. Les
  // quatre rangs disent ce que coûte un clic : rien, une boutique, une session,
  // ou un refus. Un tri STABLE, pour que deux comptes de même rang gardent
  // l'ordre dans lequel le site les a donnés.
  std::stable_sort(accounts_.begin(), accounts_.end(),
                   [](const WebAccount& left, const WebAccount& right) {
                     return AccountRank(left) < AccountRank(right);
                   });

  stage_ = WebStage::kAccounts;
  awaiting_ = Awaiting::kNothing;
  message_.clear();
}

int WebLogin::preferred_account() const noexcept {
  if (accounts_.empty()) {
    return -1;
  }
  int best = 0;
  for (int rank = 0; rank < static_cast<int>(accounts_.size()); ++rank) {
    const WebAccount& candidate = accounts_[static_cast<std::size_t>(rank)];
    if (!SafeToPrefer(candidate)) {
      continue;
    }
    // ⚠ LE PREMIER JOUABLE L'EMPORTE SUR UN INJOUABLE, quelle que soit la date :
    // sans cette moitié, un compte banni resté en tête garderait la
    // présélection pour peu qu'il porte la date la plus récente.
    const WebAccount& held = accounts_[static_cast<std::size_t>(best)];
    if (!SafeToPrefer(held) || candidate.last_login > held.last_login) {
      best = rank;
    }
  }
  return best;
}

void WebLogin::Digest(const WebReply& reply, std::uint32_t now_ms) {
  if (!reply.arrived()) {
    Fail(reply.error.empty() ? "le site est injoignable" : reply.error);
    return;
  }
  const ro::core::Result<JsonValue> body = ParseJson(reply.body);
  if (!body.has_value()) {
    // ⚠ ON DIT QUE C'EST ILLISIBLE, PAS CE QUI EST ILLISIBLE : le motif du
    // lecteur nomme une position dans des octets que le joueur n'a pas.
    ro::core::LogWarning("login web : réponse illisible ({}) — {} octet(s), code {}",
                         ro::core::Describe(body.error()), reply.body.size(), reply.status);
    Fail(std::string(kUnreadable));
    return;
  }

  if (!body->TruthAt("ok")) {
    // LE MOTIF VIENT DU SITE, qui l'écrit en français et le sait mieux que
    // nous : compte inconnu, mot de passe faux, trop d'essais.
    const std::string_view why = body->TextAt("error");
    Fail(why.empty() ? std::string(kUnreadable) : std::string(why));
    return;
  }

  switch (awaiting_) {
    case Awaiting::kNothing:
      return;
    case Awaiting::kAccounts:
      TakeAccounts(*body, now_ms);
      return;
    case Awaiting::kDiscordStart: {
      discord_session_ = std::string(body->TextAt("game_session"));
      const std::string_view path = body->TextAt("authorize_path");
      if (discord_session_.empty() || path.empty()) {
        Fail(std::string(kUnreadable));
        return;
      }
      // 🔴 L'ORIGINE EST LA NÔTRE, ET LE SITE LE VEUT AINSI : il ne rend qu'un
      // CHEMIN, parce que son propre `generate_board_url` pointe sur le forum
      // alors que la page d'autorisation est à la racine. La seule origine
      // juste est celle par laquelle nous l'appelons.
      browser_url_ = origin_ + std::string(path);
      // ⚠ LES DEUX DURÉES DU SITE SE BORNENT AVANT D'ÊTRE MULTIPLIÉES : une
      // valeur négative ou immense débordait les 32 bits en millisecondes.
      const std::int64_t every_s = body->WholeAt("poll_interval", kDiscordPollMs / kWebMsPerSecond);
      discord_every_ms_ = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
          every_s * kWebMsPerSecond, kDiscordPollFloorMs, kDiscordPollCeilingMs));
      // 🔴 UNE DURÉE DE VIE ABSENTE OU NULLE VEUT DIRE « SANS LIMITE », et non
      // « expirée » : `until = now` faisait échouer la connexion au tour suivant.
      const std::int64_t life_s = body->WholeAt("ttl", 0);
      discord_until_ms_ = 0;
      if (life_s > 0) {
        discord_until_ms_ = now_ms + static_cast<std::uint32_t>(std::min<std::int64_t>(
                                         life_s * kWebMsPerSecond, kDiscordLifeCeilingMs));
      }
      awaiting_ = Awaiting::kNothing;
      discord_next_ms_ = now_ms + discord_every_ms_;
      return;
    }
    case Awaiting::kDiscordPoll:
      awaiting_ = Awaiting::kNothing;
      // ⚠ « PAS ENCORE » N'EST PAS UNE RÉPONSE FINALE : on repasse, sans rien
      // changer à l'écran — le joueur est dans son navigateur.
      if (body->TruthAt("pending")) {
        discord_next_ms_ = now_ms + discord_every_ms_;
        return;
      }
      TakeAccounts(*body, now_ms);
      return;
    case Awaiting::kOtp: {
      const std::string_view name = body->TextAt("userid");
      const std::string_view secret = body->TextAt("otp");
      if (name.empty() || secret.empty()) {
        Fail(std::string(kUnreadable));
        return;
      }
      userid_ = std::string(name);
      otp_ = std::string(secret);
      stage_ = WebStage::kReady;
      awaiting_ = Awaiting::kNothing;
      // ⚠ LE JETON NE SERT PLUS, et il vaut un droit : le site le signe pour
      // deux minutes, et il ouvre le choix d'un compte.
      ticket_.clear();
      return;
    }
  }
}

void WebLogin::Advance(std::uint32_t now_ms) {
  if (std::optional<WebReply> reply = transport_->Take(); reply.has_value()) {
    if (awaiting_ == Awaiting::kDiscordPoll && reply->status == kHttpGone) {
      Fail("la connexion Discord a expiré — recommence");
      return;
    }
    Digest(*reply, now_ms);
    return;
  }
  if (transport_->busy()) {
    return;
  }

  // ── LA PORTE DISCORD : interroger, tant qu'il reste du temps ─────────────
  if (stage_ == WebStage::kWaitingDiscord && !discord_session_.empty()) {
    if (discord_until_ms_ != 0 && now_ms >= discord_until_ms_) {
      Fail("la connexion Discord a expiré — recommence");
      return;
    }
    if (now_ms >= discord_next_ms_) {
      Send(WebRequest{.path = std::string(kWebLoginPath),
                      .fields = {{"action", "discord_poll"}, {"game_session", discord_session_}}},
           Awaiting::kDiscordPoll, now_ms);
    }
    return;
  }

  // ── LE JETON VIEILLIT PENDANT QUE LE JOUEUR CHOISIT ──────────────────────
  //
  // 🔴 DEUX MINUTES, ET LE COMPTE À REBOURS PART À L'ARRIVÉE DE LA LISTE. Un
  // joueur qui hésite dépasse ce délai sans rien faire de mal, et le site
  // refuse alors son choix avec un message qui parle de signature. On le dit
  // nous-mêmes, dans ses termes à lui, et on le renvoie à l'identification —
  // qui est le seul geste qui règle le problème.
  if (stage_ == WebStage::kAccounts && now_ms - ticket_since_ms_ >= kWebTicketLifeMs) {
    Fail("la session avec le site a expiré — identifie-toi à nouveau");
  }
}

}  // namespace moonclient::web
