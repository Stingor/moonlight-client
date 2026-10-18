// LA PORTE MOONLIGHT DE L'ÉCRAN DE CONNEXION : ce qu'elle montre, ce qu'elle
// retient, et ce qu'elle fait de ce qu'on lui demande.
//
// 🔴 MÊME CLASSE, AUTRE UNITÉ DE TRADUCTION. `lobby_screen.cpp` a franchi les
// 800 lignes en accueillant ce bloc, et la coupure qui tombe juste est
// celle-ci : tout ce qui suit parle au SITE — un compte de forum, un jeton,
// un coffre, une page de navigateur —, et rien n'en sort qui ne soit un
// couple identifiant/mot de passe ordinaire. Le reste de l'écran peut ignorer
// que ce parcours existe, et ce fichier le rend visible.
//
// ⚠ C'est le montage déjà employé trois fois dans la partie
// (`game_menu_route.cpp`, `game_npc_links.cpp`, `game_chat_links.cpp`).

#include <string>

#include <SDL3/SDL.h>

#include "ro/core/log.h"
#include "ro/render/game_images.h"
#include "ro/ui/web_door.h"

#include "game/skin_image_of.h"
#include "shell/lobby_screen.h"
#include "web/web_credentials.h"

namespace moonclient {

namespace {

// CE QUE LE PARCOURS WEB DIT À L'ÉCRAN.
//
// ⚠ UNE RECOPIE, ET NON UN POINTEUR : `ui` ne connaît pas `moonclient::web`,
// et c'est ce qui l'empêche d'apprendre un jour à faire une requête.
[[nodiscard]] ro::ui::WebDoorStage DoorStageOf(web::WebStage stage) {
  switch (stage) {
    case web::WebStage::kIdle:
      return ro::ui::WebDoorStage::kForm;
    case web::WebStage::kSigningIn:
    case web::WebStage::kSelecting:
      return ro::ui::WebDoorStage::kWaiting;
    case web::WebStage::kWaitingDiscord:
      return ro::ui::WebDoorStage::kInBrowser;
    case web::WebStage::kAccounts:
      return ro::ui::WebDoorStage::kAccounts;
    case web::WebStage::kReady:
      // ⚠ « PRÊT » NE SE MONTRE JAMAIS : l'écran s'en sert dans la même image
      // pour lancer la connexion, et la porte est déjà repartie.
      return ro::ui::WebDoorStage::kWaiting;
    case web::WebStage::kFailed:
      return ro::ui::WebDoorStage::kFailed;
  }
  return ro::ui::WebDoorStage::kForm;
}

}  // namespace

ro::ui::WebDoorView LobbyScreen::ComposeWebDoor() {
  ro::ui::WebDoorView view;
  view.stage = DoorStageOf(web_.stage());
  // ⚠ LE SITE A FINI ET LE JEU RÉPOND : la porte attend, elle ne se remet pas
  // à demander un mot de passe. Voir `WebDoorStage::kEnteringGame`.
  if (web_.stage() == web::WebStage::kIdle && flow_.screen() != lobby::LobbyScreenKind::kForm) {
    view.stage = ro::ui::WebDoorStage::kEnteringGame;
  }
  view.message = web_.message();
  view.ticket_alive = !web_.accounts().empty();
  // LE LOGO DE LA PORTE DISCORD. ⚠ SON ABSENCE N'EST PAS UNE PANNE : l'image
  // vit dans l'archive du serveur, et le bouton se rabat sur son libellé.
  if (shell_.pieces().game_images.has_value()) {
    ro::render::GameImages& images = *shell_.pieces().game_images;
    view.discord = SkinImageOf(images.GetOurs(ro::ui::kDiscordMarkPath));
    view.discord_shape = SkinImageOf(images.GetOursInkShape(ro::ui::kDiscordMarkPath));
  }
  for (const web::WebAccount& account : web_.accounts()) {
    view.accounts.push_back(ro::ui::WebAccountRow{.label = account.label,
                                                  .characters = account.characters,
                                                  .group_id = account.group_id,
                                                  .last_login = account.last_login,
                                                  .banned = account.banned,
                                                  .online = account.online,
                                                  .autotrade = account.autotrade});
  }
  return view;
}

void LobbyScreen::SaveWebMemory() {
  const web::WebCredentials wanted{.member = std::string(form_.web.member.data()),
                                   .password = std::string(form_.web.password.data()),
                                   .remember_member = form_.web.remember_member,
                                   .remember_password = form_.web.remember_password};
  // ⚠ L'ÉCHEC EST DÉJÀ DIT AU JOURNAL par la fonction elle-même, et il n'y a
  // rien à en faire ici : une mémoire qui ne s'écrit pas coûte une frappe au
  // prochain lancement, pas une connexion.
  static_cast<void>(web::SaveWebCredentials(shell_.client_root(), web_vault_, wanted));
}

// EFFACER LE MOT DE PASSE DU SITE DU FORMULAIRE, sauf s'il est retenu.
//
// 🔴 IL S'EFFACE À L'INSTANT OÙ IL PART, comme celui du jeu : il ne sert plus,
// et le laisser dans un tampon qui vit toute la session en ferait une copie de
// plus à oublier.
//
// ⚠ SAUF QUAND LE JOUEUR A DEMANDÉ QU'ON LE RETIENNE. Il est alors déjà sur le
// disque, scellé par le système : le vider du champ n'enlèverait aucune copie
// et obligerait à le retaper au premier refus — précisément ce que la case
// promettait d'éviter.
void LobbyScreen::ForgetWebPassword() {
  if (!form_.web.remember_password) {
    form_.web.password.fill('\0');
  }
}

void LobbyScreen::RouteWebDoor(const ro::ui::WebDoorAction& asked) {
  // ⚠ AVANT LE VERBE : une case a pu basculer dans la même image qu'un clic
  // sur « Se connecter », et l'écriture doit alors porter le nouvel état.
  if (asked.memory_changed) {
    SaveWebMemory();
  }
  switch (asked.verb) {
    case ro::ui::WebDoorVerb::kNothing:
      break;
    case ro::ui::WebDoorVerb::kSignIn:
      web_.SignIn(std::string(form_.web.member.data()), std::string(form_.web.password.data()));
      // ⚠ ENREGISTRER AVANT D'EFFACER : c'est le dernier instant où le
      // formulaire tient encore le mot de passe : lu après qu'on l'a vidé, le
      // champ ne laisserait rien à « Se souvenir du mot de passe ».
      SaveWebMemory();
      ForgetWebPassword();
      break;
    case ro::ui::WebDoorVerb::kDiscord:
      // ⚠ LE NAVIGATEUR NE S'OUVRE PAS ICI : l'adresse n'existe qu'après la
      // réponse du site. Voir `OpenDiscordPage`.
      web_.StartDiscord();
      break;
    case ro::ui::WebDoorVerb::kChoose:
      if (asked.account >= 0 && asked.account < static_cast<int>(web_.accounts().size())) {
        web_.ChooseAccount(web_.accounts()[static_cast<std::size_t>(asked.account)].account_id);
      }
      break;
    case ro::ui::WebDoorVerb::kRestart:
      web_.Forget();
      ForgetWebPassword();
      form_.web.chosen = -1;
      break;
    case ro::ui::WebDoorVerb::kBackToAccounts:
      // ⚠ RIEN À RENVOYER AU SITE : la liste est encore là, et le jeton avec.
      // C'est tout l'intérêt de ce bouton — voir `web_door.h`.
      break;
    case ro::ui::WebDoorVerb::kMergeAccounts:
      OpenMergePage();
      break;
    case ro::ui::WebDoorVerb::kUseClassic:
      // ⚠ CONSOMMÉ PAR LA FENÊTRE, qui possède le choix de la porte : ce verbe
      // n'arrive jamais jusqu'ici. Le cas existe pour que le compilateur
      // réclame une décision le jour où cela changerait.
      break;
  }
}

// LA PAGE DE FUSION DE COMPTES DU SITE, dans le navigateur.
//
// 🔴 L'ADRESSE SE COMPOSE SUR L'ORIGINE À LAQUELLE ON PARLE, jamais sur un
// hôte écrit en dur : un client réglé sur un autre site enverrait sinon son
// joueur regrouper des comptes ailleurs.
//
// ⚠ ET L'ON VÉRIFIE LE SCHÉMA EN TOUTES LETTRES avant de tendre quoi que ce
// soit au système, exactement comme pour une adresse trouvée dans la
// description d'un objet : ce qui s'ouvre ici ouvre un PROGRAMME.
void LobbyScreen::OpenMergePage() {
  const std::string address = web_.origin() + std::string(web::kMergePagePath);
  if (!address.starts_with("https://")) {
    ro::core::LogWarning("fusion de comptes : adresse refusée — {}", address);
    return;
  }
  if (!SDL_OpenURL(address.c_str())) {
    ro::core::LogWarning("fusion de comptes : {} n'a pas pu s'ouvrir", address);
  }
}

// LA PAGE D'AUTORISATION DISCORD, dans le navigateur, UNE fois par demande.
//
// 🔴 `WebLogin` COMPOSE L'ADRESSE, ET C'EST ICI QU'ELLE SE TEND AU SYSTÈME.
// Sans cela, l'écran annoncerait « termine la connexion dans ton navigateur »
// devant un navigateur qui n'a rien reçu.
//
// ⚠ C'EST LE SYSTÈME QUI CHOISIT LE NAVIGATEUR : le client n'a pas à savoir
// lequel, ni où il est installé. Et le schéma se vérifie en toutes lettres,
// comme pour la page de fusion : ce qui s'ouvre ici ouvre un PROGRAMME.
//
// ⚠ L'ADRESSE NE VA PAS AU JOURNAL : elle porte la session de connexion.
void LobbyScreen::OpenDiscordPage() {
  const std::string& address = web_.browser_url();
  if (address.empty() || address == opened_browser_url_) {
    return;
  }
  opened_browser_url_ = address;
  if (!address.starts_with("https://")) {
    ro::core::LogWarning("connexion Discord : adresse refusée, elle n'est pas en https");
    return;
  }
  if (!SDL_OpenURL(address.c_str())) {
    ro::core::LogWarning("connexion Discord : le navigateur n'a pas pu s'ouvrir ({})",
                         SDL_GetError());
  }
}

void LobbyScreen::EnterWithWebToken() {
  // 🔴 CE QUI SORT DU PARCOURS WEB EST UN COUPLE ORDINAIRE, et c'est ce qui
  // fait que rien en aval ne connaît ce parcours : un identifiant, et un mot
  // de passe qui se trouve être à usage unique.
  NoteConnectingServer();
  flow_.Submit(web_.userid(), web_.otp());
  // 🔴 ET L'OTP S'EFFACE AUSSITÔT : il vaut un mot de passe, il a servi, et le
  // site en posera un neuf à la prochaine demande.
  web_.Forget();
  // ⚠ LE MOT DE PASSE DU SITE, LUI, SUIT SA CASE : retenu, il reste dans le
  // champ. Le vider ici sans condition laisserait « Revenir au login » sur un
  // champ vide pour tout le reste de la session, alors que le disque le
  // garde — il ne se relit qu'au lancement.
  ForgetWebPassword();
  form_.web.chosen = -1;
}

}  // namespace moonclient
