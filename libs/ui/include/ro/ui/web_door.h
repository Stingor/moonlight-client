#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE LOGO DE LA PORTE DISCORD, dans l'installation.
//
// ⚠ LE CHEMIN ENTIER : notre VFS n'ajoute aucun préfixe, là où le gestionnaire
// de textures du client part de `data\texture\`. Même règle que le décor du
// banquet, et payée au même endroit.
inline constexpr std::string_view kDiscordMarkPath = "data\\texture\\discord.bmp";

// LA PORTE MOONLIGHT : se connecter avec son compte du SITE, puis choisir
// lequel de ses comptes de jeu employer.
//
// 🔴 ELLE NE REMPLACE PAS LE FORMULAIRE CLASSIQUE, ELLE S'AJOUTE À CÔTÉ. Ce
// qu'elle produit est un couple identifiant / mot de passe ordinaire — le mot
// de passe étant un jeton à usage unique que le site vient de poser —, et la
// suite du démarrage ne fait pas la différence. Le classique reste le repli
// quand le site est en panne et que le jeu, lui, répond.
//
// ⚠ TROIS ÉTATS QUI SE SUIVENT, ET NON TROIS ONGLETS : on s'identifie, on
// choisit, on joue. Cette forme tient dans la même petite fenêtre que le
// formulaire classique.

// LA TAILLE DES CHAMPS DU SITE, terminateur compris.
//
// ⚠ PLUS LARGE QUE CEUX DU JEU, et ce n'est pas une incohérence : `CA_LOGIN`
// borne les siens à vingt-quatre octets, le site ne borne rien. Un pseudonyme
// de forum et un mot de passe web n'ont pas à tenir dans la contrainte d'un
// protocole qu'ils ne traversent pas.
inline constexpr std::size_t kWebFieldBytes = 64;

// QUELLE PORTE LE JOUEUR EMPRUNTE.
//
// ⚠ DEUX ET NON TROIS : Discord n'est pas une porte, c'est un BOUTON de la
// porte Moonlight. Il mène au même endroit — la liste des comptes du site —, et
// lui donner un rang ici aurait laissé croire à un troisième écran.
enum class LoginDoor : std::uint8_t { kClassic, kMoonlight };

// OÙ EN EST LA PORTE, du point de vue de l'écran. ⚠ C'EST UNE RECOPIE de ce
// que la machine à états dit : `ui` ne connaît pas `moonclient::web`, et ne
// doit pas.
enum class WebDoorStage : std::uint8_t {
  // Le formulaire du site répond.
  kForm,
  // On attend le site.
  kWaiting,
  // Le joueur doit finir dans son navigateur.
  kInBrowser,
  // La liste est là : à lui de choisir.
  kAccounts,
  // LE SITE A FINI, ET C'EST LE JEU QUI RÉPOND MAINTENANT.
  //
  // 🔴 CET ÉTAT-LÀ N'EST PAS AU SITE, et c'est pour cela qu'il existe. Le
  // jeton est devenu un mot de passe ordinaire et le serveur de jeu le
  // vérifie ; la porte, elle, n'a plus rien à faire. Sans cet état l'écran
  // retomberait à cet instant sur le formulaire CLASSIQUE, vide et grisé — ce
  // qui a tout l'air d'un échec qui redemande tout.
  kEnteringGame,
  // `message` dit pourquoi.
  kFailed,
};

// UNE LIGNE DE LA LISTE DES COMPTES.
//
// ⚠ DES DONNÉES PURES, remplies par `app` : `ui` ne lit ni HTTP ni JSON.
struct WebAccountRow {
  std::string label;
  int characters = 0;
  // 0 pour un joueur ; sinon le niveau du groupe staff, que la ligne affiche.
  int group_id = 0;
  // Ce que le site rend, ou vide pour un compte qui n'a jamais servi.
  std::string last_login;
  // 🔴 CES TROIS-LÀ NE SONT PAS DES ORNEMENTS, et l'écran les dit AVANT le
  // clic. Un compte banni ne se connectera pas ; un compte déjà en ligne fera
  // tomber l'autre session ; un compte en vente automatique est en ligne POUR
  // UNE RAISON, et l'en déloger ferme une boutique. Les apprendre après le
  // refus, c'est les apprendre trop tard.
  bool banned = false;
  bool online = false;
  bool autotrade = false;

  // Un compte banni ne sert à rien ; les deux autres drapeaux avertissent
  // sans interdire, parce que se déloger soi-même est un geste légitime.
  [[nodiscard]] bool usable() const noexcept { return !banned; }
};

// CE QUE LA PORTE MONTRE.
struct WebDoorView {
  WebDoorStage stage = WebDoorStage::kForm;
  std::vector<WebAccountRow> accounts;
  // Le motif du dernier échec, déjà traduit — et écrit par le SITE la plupart
  // du temps, qui connaît ses refus mieux que nous.
  std::string message;
  // ⚠ LE JETON VIT-IL ENCORE ? C'est ce qui décide si un refus propose de
  // revenir au CHOIX ou à l'identification : après « ce compte est déjà
  // connecté », rien ne justifie de retaper son mot de passe.
  bool ticket_alive = false;
  // LE LOGO DE LA PORTE DISCORD, ou une image inutilisable.
  //
  // ⚠ UNE ABSENCE N'EST PAS UNE PANNE : `data\texture\discord.bmp` vit dans
  // l'archive du serveur, et un joueur peut avoir une installation qui ne la
  // porte pas. Le bouton se rabat alors sur son libellé.
  //
  // ⚠ ET C'EST LA MARQUE D'UN TIERS, donc la seule chose de cet écran qui ne
  // se dessine pas aux couleurs du jeu : un bouton Discord qui ne ressemble
  // pas à Discord ne se reconnaît pas.
  SkinImage discord;
  // LE MÊME LOGO RÉDUIT À SA FORME, en blanc, ou une image inutilisable.
  //
  // 🔴 POUR UNE FACE DE BOUTON SOMBRE, où le bleu de Discord se perd : sa
  // clarté est celle d'un gris moyen, et le logo, lissé contre du blanc, s'y
  // cerne d'un halo pâle. La marque s'y peint alors de l'encre du libellé — en
  // blanc, comme Discord la veut sur un fond sombre.
  SkinImage discord_shape;
};

// CE QUE LE JOUEUR A TAPÉ. Il appartient à l'APPELANT.
struct WebDoorState {
  std::array<char, kWebFieldBytes> member = {};
  std::array<char, kWebFieldBytes> password = {};
  // ── CE QUE LE CLIENT RETIENT D'UN LANCEMENT À L'AUTRE ─────────────────
  //
  // 🔴 DEUX CASES ET NON UNE, ET LA SECONDE COÛTE PLUS CHER. L'identifiant du
  // site n'est qu'un nom ; le mot de passe, lui, ouvre le compte. Les fondre
  // ferait retenir le second à qui ne demandait que le premier.
  //
  // ⚠ COCHER LA SECONDE COCHE LA PREMIÈRE : un mot de passe retenu sans son
  // identifiant ne remplirait qu'un champ sur deux. La règle se voit à l'écran.
  //
  // ⚠ CE QUE « RETENIR » VEUT DIRE N'EST PAS L'AFFAIRE DE `ui` : elle montre
  // deux cases et dit quand elles bougent. Où cela s'écrit, et sous quelle
  // protection, appartient à `app` — voir `web_credentials.h`.
  bool remember_member = false;
  bool remember_password = false;
  // La ligne désignée, ou -1.
  int chosen = -1;
  // ⚠ UN SEUL TOUR : posé quand les flèches déplacent le choix, consommé par la
  // liste qui le fait défiler jusqu'à lui. Le garder ferait défiler à chaque
  // image, et l'ascenseur n'obéirait plus à la souris.
  bool scroll_to_chosen = false;
};

// CE QUE LA PORTE A DEMANDÉ.
enum class WebDoorVerb : std::uint8_t {
  kNothing,
  // S'identifier avec ce que portent les champs.
  kSignIn,
  // Ouvrir la porte Discord.
  kDiscord,
  // Jouer avec la ligne `chosen`.
  kChoose,
  // Revenir à l'identification, en oubliant la liste.
  kRestart,
  // Revenir au CHOIX sans se réidentifier — le jeton vit encore.
  kBackToAccounts,
  // Ouvrir la page de fusion de comptes du site, dans le navigateur.
  kMergeAccounts,
  // Passer au formulaire classique. ⚠ CONSOMMÉ PAR LA FENÊTRE DE CONNEXION,
  // qui possède le choix de la porte : il n'atteint jamais `app`.
  kUseClassic,
};

struct WebDoorAction {
  WebDoorVerb verb = WebDoorVerb::kNothing;
  int account = -1;
  // UNE DES DEUX CASES DE MÉMOIRE VIENT DE BASCULER.
  //
  // ⚠ UN ÉVÉNEMENT ET NON UN ÉTAT, parce que ce qu'il déclenche est une
  // ÉCRITURE : lire les deux drapeaux à chaque image et réconcilier le disque
  // écrirait un fichier soixante fois par seconde. Même montage que la case du
  // décor sur le premier écran.
  bool memory_changed = false;
};

// DESSINE LA PORTE, DANS LA FENÊTRE DÉJÀ OUVERTE.
//
// ⚠ ELLE N'OUVRE PAS DE FENÊTRE : c'est le formulaire de connexion qui la
// possède, et les deux portes se partagent son cadre — sans quoi changer de
// porte ferait sauter une fenêtre et en ouvrir une autre à sa place.
//
// `width` est la largeur des contrôles, celle que le formulaire calcule.
[[nodiscard]] WebDoorAction DrawWebDoor(WebDoorState& state, const WebDoorView& view,
                                        const SkinArt& art, float width);

}  // namespace ro::ui
