#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "web/web_transport.h"

namespace moonclient::web {

// LE CHEMIN DE L'API SUR LE SITE, et l'origine par défaut.
//
// ⚠ L'ORIGINE EST CELLE DE LA PRODUCTION, et elle se règle. ⚠ EN HTTPS, ET
// L'API L'IMPOSE elle-même — identifiant du forum, mot de passe et OTP de jeu y
// circulent, et les trois valent un compte.
inline constexpr std::string_view kWebLoginOrigin = "https://moonlight-destiny.fr";
inline constexpr std::string_view kWebLoginPath = "/api/game_login.php";

// LA PAGE QUI REGROUPE PLUSIEURS COMPTES DU SITE EN UN SEUL.
//
// ⚠ ELLE N'EST PAS DE L'API : c'est une page du panneau de contrôle du forum,
// que l'on ouvre dans le NAVIGATEUR. Le client n'en lit rien et n'y envoie
// rien — il n'a ni la session du forum ni de raison de l'avoir.
inline constexpr std::string_view kMergePagePath = "/ucp.php?i=moonlight&mode=merge";

// COMBIEN DE TEMPS LE JETON DU SITE VIT, en millisecondes.
//
// 🔴 DEUX MINUTES, ET C'EST COURT POUR UN HUMAIN. `GAME_LOGIN_TICKET_TTL` vaut
// 120 s côté site, et le compte à rebours part à l'instant où la liste arrive —
// donc pendant que le joueur lit ses comptes et se décide. Passé ce délai, le
// site refuse le choix avec un message qui parle de signature ; l'écran, lui,
// sait que le jeton a simplement vieilli, et le dit ainsi.
//
// ⚠ NOTRE COPIE EST DÉLIBÉRÉMENT PLUS COURTE QUE LA SIENNE : mieux vaut
// redemander un jeton qu'on croyait encore bon que d'en envoyer un que le site
// vient de refuser. Le joueur ne voit ni l'un ni l'autre — il voit un écran qui
// se réidentifie tout seul, ou un refus.
inline constexpr std::uint32_t kWebTicketLifeMs = 100'000;

// LE RYTHME D'INTERROGATION DE LA PORTE DISCORD, par défaut et au plus rapide.
//
// ⚠ LE SITE LE DONNE dans sa réponse (`poll_interval`, en secondes) ; ceci est
// le repli et le PLANCHER. Un site qui répondrait zéro nous ferait l'interroger
// soixante fois par seconde, ce qui est un déni de service qu'on s'inflige.
inline constexpr std::uint32_t kDiscordPollMs = 2'000;
inline constexpr std::uint32_t kDiscordPollFloorMs = 1'000;
// Les bornes hautes de ce que le site peut demander : une minute entre deux
// questions, une heure de validité. Au-delà, c'est une réponse abîmée.
inline constexpr std::uint32_t kDiscordPollCeilingMs = 60'000;
inline constexpr std::uint32_t kDiscordLifeCeilingMs = 3'600'000;
inline constexpr std::int64_t kWebMsPerSecond = 1'000;

// UN COMPTE DE JEU LIÉ AU COMPTE WEB.
struct WebAccount {
  std::int64_t account_id = 0;
  // Le nom que `CA_LOGIN` portera.
  std::string userid;
  // Ce que l'écran montre ; le site le distingue de `userid` pour pouvoir un
  // jour l'orner, et aujourd'hui les deux sont égaux.
  std::string label;
  int characters = 0;
  // Le groupe du compte sur le serveur : 0 pour un joueur, le niveau du groupe
  // pour un compte staff. Un site qui ne l'envoie pas laisse 0.
  int group_id = 0;
  // La date du dernier accès, telle que le site la rend (« 2026-09-20 »), ou
  // vide pour un compte qui n'a jamais servi.
  std::string last_login;
  // ⚠ CES TROIS-LÀ NE SONT PAS DES ORNEMENTS. Un compte banni ne se connectera
  // pas ; un compte déjà en ligne fera tomber la session de l'autre ; un compte
  // en vente automatique est en ligne pour une RAISON, et l'en déloger coûte
  // une boutique. L'écran doit les dire avant le clic, pas après le refus.
  bool banned = false;
  bool online = false;
  bool autotrade = false;
};

// OÙ EN EST LE PARCOURS WEB.
enum class WebStage : std::uint8_t {
  // Rien de demandé.
  kIdle,
  // Le compte web est parti ; on attend la liste.
  kSigningIn,
  // La porte Discord est ouverte : le navigateur est chez le joueur, et l'on
  // interroge le site jusqu'à ce qu'il réponde.
  kWaitingDiscord,
  // La liste est là ; à l'appelant de choisir.
  kAccounts,
  // Le choix est parti ; on attend l'OTP.
  kSelecting,
  // `userid()` et `otp()` sont là : il ne reste qu'à faire un `CA_LOGIN`.
  kReady,
  // `message()` dit pourquoi.
  kFailed,
};

// LE PARCOURS DU LOGIN WEB : compte du forum, liste des comptes de jeu, jeton
// à usage unique.
//
// 🔴 IL NE REMPLACE PAS LA VOIE CLASSIQUE, IL S'AJOUTE. Ce qu'il produit est un
// couple identifiant/mot de passe ordinaire — le mot de passe étant un OTP que
// le site vient de poser —, et `CA_LOGIN` ne fait pas la différence. C'est ce
// qui permet à toute la suite (personnages, entrée en jeu) d'ignorer
// complètement l'existence de ce parcours.
//
// ⚠ IL NE CONNAÎT NI HTTP NI FENÊTRE : un transport abstrait d'un côté, des
// intentions de l'autre. C'est ce qui le rend essayable sur un transport qui
// répond ce qu'on veut — y compris rien, ou une page d'erreur HTML.
//
// 🔴 ET L'OTP VAUT UN MOT DE PASSE. Il ne se journalise pas, ne se retient pas,
// et s'efface dès qu'il est parti sur le fil : `Forget`.
class WebLogin {
 public:
  WebLogin(WebTransport& transport, std::string origin)
      : transport_(&transport), origin_(std::move(origin)) {}

  // ── CE QUE LE JOUEUR DEMANDE ────────────────────────────────────────────

  // La porte du forum. Sans effet si le parcours est déjà en route.
  void SignIn(std::string_view username, std::string_view password);

  // La porte Discord : le site ouvre une session, et rend l'adresse à montrer
  // dans le navigateur.
  //
  // ⚠ LE CLIENT NE PEUT PAS FAIRE L'OAUTH LUI-MÊME — il faudrait un navigateur
  // et le secret du site —, d'où ce détour : le joueur va sur le site, et nous
  // interrogeons jusqu'à ce qu'il ait fini.
  void StartDiscord();

  // L'adresse à ouvrir dans le navigateur, une fois `kWaitingDiscord` atteint.
  [[nodiscard]] const std::string& browser_url() const noexcept { return browser_url_; }

  // Le joueur a choisi. Sans effet hors de `kAccounts`.
  void ChooseAccount(std::int64_t account_id);

  // Efface tout — jeton, OTP, liste. ⚠ À LA FERMETURE DE L'ÉCRAN, et après que
  // l'OTP est parti : voir ce que vaut un OTP.
  void Forget() noexcept;

  // Fait avancer le parcours. À appeler une fois par tour de boucle.
  void Advance(std::uint32_t now_ms);

  // ── CE QUE L'ÉCRAN LIT ──────────────────────────────────────────────────

  [[nodiscard]] WebStage stage() const noexcept { return stage_; }
  // L'ORIGINE DU SITE, telle qu'elle a été réglée.
  //
  // ⚠ ELLE SORT D'ICI POUR QU'UNE ADRESSE DU SITE SE COMPOSE SUR ELLE, jamais
  // sur un hôte écrit en dur ailleurs : le lien « Fusionner mes comptes » doit
  // mener au site auquel on vient de parler, pas à celui d'une constante
  // oubliée dans un autre fichier.
  [[nodiscard]] const std::string& origin() const noexcept { return origin_; }
  // Le motif du dernier échec, en clair, ou vide.
  [[nodiscard]] const std::string& message() const noexcept { return message_; }
  [[nodiscard]] const std::vector<WebAccount>& accounts() const noexcept { return accounts_; }

  // LE COMPTE QUE « ENTRÉE » JOUERA, sans que le joueur ait rien désigné.
  //
  // 🔴 LE PLUS RÉCEMMENT JOUÉ PARMI CEUX QUI SE JOUENT, et jamais un autre :
  // un joueur qui revient veut son compte d'hier, et le lui présélectionner
  // réduit la reconnexion à une touche. Un compte banni ou déjà en ligne n'est
  // jamais présélectionné — appuyer sur Entrée par réflexe ne doit pas faire
  // tomber la session qui tourne à côté.
  //
  // ⚠ LA DATE SE COMPARE COMME DU TEXTE, et c'est juste parce que le site la
  // rend en « AAAA-MM-JJ » : cet ordre-là est le même que l'ordre des dates.
  // Une date vide passe donc avant toutes les autres, ce qui est le bon sens —
  // un compte jamais joué n'est pas celui qu'on revient chercher.
  //
  // Rend -1 quand la liste est vide ; sinon toujours un rang valide, même si
  // aucun compte n'est jouable — l'écran grise alors son bouton de lui-même.
  [[nodiscard]] int preferred_account() const noexcept;
  // Ce qu'il faut passer à `CA_LOGIN`, une fois `kReady`.
  [[nodiscard]] const std::string& userid() const noexcept { return userid_; }
  [[nodiscard]] const std::string& otp() const noexcept { return otp_; }

 private:
  // Ce que la requête en vol attend comme réponse.
  enum class Awaiting : std::uint8_t { kNothing, kAccounts, kDiscordStart, kDiscordPoll, kOtp };

  void Send(const WebRequest& request, Awaiting awaiting, std::uint32_t now_ms);
  void Fail(std::string why);
  // Lit une réponse et fait avancer. Rend faux si elle n'était pas lisible.
  void Digest(const WebReply& reply, std::uint32_t now_ms);
  // La réponse commune à `auth` et à `discord_poll` résolue : jeton + comptes.
  void TakeAccounts(const class JsonValue& body, std::uint32_t now_ms);

  WebTransport* transport_;
  std::string origin_;

  WebStage stage_ = WebStage::kIdle;
  Awaiting awaiting_ = Awaiting::kNothing;
  std::string message_;
  std::vector<WebAccount> accounts_;
  std::string ticket_;
  std::uint32_t ticket_since_ms_ = 0;
  std::string userid_;
  std::string otp_;
  std::string browser_url_;
  // La porte Discord : la session à interroger, son rythme, et quand la
  // prochaine interrogation part.
  std::string discord_session_;
  std::uint32_t discord_every_ms_ = kDiscordPollMs;
  std::uint32_t discord_next_ms_ = 0;
  std::uint32_t discord_until_ms_ = 0;
};

}  // namespace moonclient::web
