#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/escape_stack.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/web_door.h"

namespace ro::ui {

// L'ÉCRAN DE CONNEXION : deux champs, une case, un bouton.
//
// 🔴 CE N'EST PAS LE PANNEAU PEINT DU CLIENT D'ORIGINE, et c'est un choix, pas
// un manque. `win_login.bmp` est le formulaire entier cuit dans un bitmap de
// 280 × 120 — barre de titre comprise, et « ID » et « Password » écrits DEDANS,
// en anglais. Le natif y pose deux contrôles EDIT de Windows par-dessus les
// deux rectangles peints. Rien de tout cela ne se traduit, ne se redimensionne,
// ni ne s'habille : c'est une image.
//
// Le formulaire ne l'emploie donc pas : c'est une fenêtre RO ordinaire, au
// texte traduit.
//
// ⚠ ET LES CHAMPS NE SONT PAS HABILLÉS, parce que le jeu N'A PAS de champ
// habillé à porter : ses saisies sont des contrôles du système. Les quatre
// champs que ce client possède déjà — banque, charrette, filtre du staff,
// chatbox — sont ceux d'ImGui, et celui-ci leur ressemble.

// LA TAILLE DES DEUX CHAMPS, terminateur compris.
//
// 🔴 ELLE VIENT DU FIL : `CA_LOGIN` porte le compte et le mot de passe sur
// VINGT-QUATRE octets chacun, et ce qui dépasse est coupé à l'encodage. Laisser
// taper davantage promettrait un mot de passe que le serveur ne recevrait pas —
// et l'échec ressemblerait alors à « mot de passe incorrect ».
//
// ⚠ DÉCLARÉE ICI ET RECOUPÉE DANS `app` : `ui` ne connaît pas le protocole, et
// c'est une propriété qu'on tient. Même montage que `kBankZenyMax`.
inline constexpr std::size_t kLoginFieldBytes = 24;

// Le code PIN fait quatre chiffres, et le serveur coupe la liaison sans un mot
// sur tout ce qui n'a pas cette forme.
inline constexpr std::size_t kPincodeDigits = 4;

// LA LARGEUR PLANCHER DES CONTRÔLES, en pixels.
//
// La fenêtre se règle sur son élément le
// plus large ; ce plancher évite que champs et boutons ne s'effondrent quand
// rien de plus large ne s'affiche.
inline constexpr float kLoginFormMinWidth = 340.0F;

// 🔴 LE TITRE ET L'IDENTITÉ SE SÉPARENT, ET CE N'EST PAS COSMÉTIQUE. ImGui range
// l'état d'une fenêtre — sa position — sous son TITRE. Traduit, le titre change,
// et avec lui l'identité : un joueur qui place son formulaire puis passe le
// client en anglais le retrouverait au centre, sans rien pour l'expliquer.
// L'identifiant, lui, ne se traduit pas.
inline constexpr const char* kLoginWindowTitle = core::TrText("Connexion");
// LE MÊME ÉCRAN, VU PAR LA PORTE MOONLIGHT. ⚠ MÊME IDENTITÉ, DONC MÊME
// FENÊTRE : changer de porte change le titre écrit sur la barre, et rien
// d'autre — ni la place, ni la taille, que le joueur a pu régler.
inline constexpr const char* kMoonlightWindowTitle = core::TrText("Connexion Moonlight");
inline constexpr const char* kLoginWindowId = "login";

// L'ILLUSTRATION DE L'ÉCRAN DE CONNEXION DU CLIENT D'ORIGINE.
//
// 🔴 C'EST CE QUE LE NATIF MONTRE, ET LE DÉCOR VIVANT PREND SA PLACE. L'ADR
// 0057 le dit : la ville en direct est le second écart VOULU au natif. Quand le
// joueur éteint cette ville, il n'y a pas de raison d'inventer un fond — il y
// en a un, celui que le jeu a toujours eu.
//
// ⚠ UN JPEG, ET C'EST LE SEUL DE L'INTERFACE. Il a fallu un décodeur pour lui ;
// voir `ro/formats/jpeg.h`, et la clé magenta qu'on ne lui applique pas.
//
// ⚠ IL VIT UN NIVEAU AU-DESSUS DE `login_interface\`, et non dedans : les 78
// pièces de ce dossier sont 77 BMP et 1 TGA, zéro JPEG. L'illustration, elle,
// est à la racine des textures d'interface.
inline constexpr std::string_view kLoginArtPath = "data\\texture\\유저인터페이스\\t_login.jpg";

// OÙ EN EST LA CONNEXION, du point de vue de l'écran.
enum class LoginStage : std::uint8_t {
  // Le formulaire répond.
  kForm,
  // L'attente : les champs restent à l'écran, inertes.
  kConnecting,
  // Le serveur réclame le code PIN. ⚠ En production il ne le fait jamais.
  kPincode,
};

// UNE LANGUE PROPOSÉE SUR LE PREMIER ÉCRAN.
struct LoginLanguage {
  // Le nom dans SA propre langue — « Français », « English », « Español ».
  // ⚠ IL NE SE TRADUIT PAS : un nom de langue se lit par qui la parle, et le
  // traduire le rendrait illisible à celui qui le cherche.
  std::string name;
  // ⚠ UNE LANGUE SANS CATALOGUE RESTE VISIBLE MAIS INERTE. La masquer
  // laisserait croire que le client ne la connaît pas, et le joueur
  // chercherait un fichier qu'il a peut-être simplement effacé.
  bool available = true;
};

// LES TROIS RÉGLAGES DU PREMIER ÉCRAN, et pourquoi ils sont là plutôt que dans
// le panneau du jeu.
//
// 🔴 LE PANNEAU N'EST ATTEIGNABLE QU'UNE FOIS EN JEU. Un joueur anglophone qui
// lance le client pour la première fois devrait donc traverser un formulaire
// français, une tablée française et une entrée en jeu avant de pouvoir
// demander l'anglais. C'est ICI qu'il faut le lui proposer — sur le premier
// écran qu'il voit —, et la police va avec : c'est aussi le premier écran où
// l'on lit.
//
// 🔴 ET LE TROISIÈME GOUVERNE CE QU'ON A SOUS LES YEUX. Le décor vivant n'a
// pas d'autre interrupteur : en jeu, ce réglage ne peut plus rien faire.
struct LoginFirstScreenView {
  std::vector<LoginLanguage> languages;
  // Le rang de la langue en cours dans `languages`.
  int language = 0;
  std::vector<std::string> fonts;
  int font = 0;
  bool backdrop_wanted = true;
  // ⚠ VRAI SEULEMENT QUAND UNE VILLE EST EN PLACE : sans session à rouvrir,
  // « Changer de vue » n'aurait rien à relancer, et le bouton disparaît.
  bool backdrop_live = false;
};

// CE QUE LA FENÊTRE SAIT, recopié par l'appelant.
struct LoginWindowView {
  // LA PORTE MOONLIGHT EST-ELLE OUVERTE DU TOUT ?
  //
  // ⚠ ELLE DEMANDE UN SITE, et un client bâti sans son transport — ou lancé
  // contre un serveur qui n'en a pas — n'a rien à en montrer. Un sélecteur de
  // porte qui n'aurait qu'une porte est un sélecteur qui ment.
  bool web_door_offered = false;
  WebDoorView web;
  LoginStage stage = LoginStage::kForm;
  // Le motif du dernier refus, EN CLAIR et déjà traduit, ou vide.
  //
  // ⚠ IL VIENT DE `ro::net` : c'est la couche qui parle au serveur qui sait
  // lire ses codes, et `ui` n'a pas à connaître deux tables de refus.
  std::string message;
  // Vrai quand le serveur vient de rejeter le dernier code PIN : « recommence »
  // et « donne ton code » sont le même paquet.
  bool pincode_refused = false;
  LoginFirstScreenView first_screen;
  // CE QU'ON ATTEND, QUAND LA BOUCLE VA SE FIGER POUR L'OBTENIR.
  //
  // 🔴 NON VIDE VEUT DIRE « VOILE L'ÉCRAN ET ÉCRIS CECI ». Monter une carte
  // bloque plus d'une seconde, et AUCUNE image ne se dessine pendant ce
  // temps-là : ce voile est donc la DERNIÈRE image peinte avant le gel, celle
  // que le joueur regarde en attendant. Sans lui, il regarde un formulaire qui
  // a l'air de ne rien faire.
  //
  // 🔴 ET SEULEMENT POUR CE QUI BLOQUE. La ville du SERVEUR, elle, arrive sans
  // rien figer : la voiler volerait au joueur la seconde pendant laquelle il
  // pouvait taper son compte. Ce champ ne se remplit donc pas à chaque attente,
  // mais à celles qui prennent la main.
  //
  // ⚠ DÉJÀ TRADUIT : `ui` ne traduit que ce qu'elle écrit elle-même.
  std::string busy;
  // CE QUI SE PEINT DERRIÈRE, quand il n'y a pas de ville à regarder.
  //
  // ⚠ INUTILISABLE LE RESTE DU TEMPS : le décor vivant, lui, n'est pas dessiné
  // par cette fenêtre — il l'est avant elle, par la coquille, en trois
  // dimensions. Les deux ne se superposent jamais.
  SkinImage backdrop;
};

// CE QUE LE JOUEUR A TAPÉ. Il appartient à l'APPELANT, comme tout état dans
// cette couche.
//
// 🔴 LE MOT DE PASSE NE SE RETIENT NULLE PART. Il ne passe ni par les réglages
// ni par aucun fichier : `interface.ini` est en clair, à côté du jeu. Le COMPTE,
// lui, se retient : c'est ce que la case demande.
struct LoginWindowState {
  std::array<char, kLoginFieldBytes> account = {};
  std::array<char, kLoginFieldBytes> password = {};
  std::array<char, kPincodeDigits + 1> pincode = {};
  bool remember_account = false;
  // QUELLE PORTE LE JOUEUR EMPRUNTE.
  //
  // 🔴 MOONLIGHT PAR DÉFAUT, ET LE CLASSIQUE EST LE REPLI. C'est la voie que
  // le serveur veut voir prise — un compte du site, un jeton à usage unique.
  // Le classique ne sert qu'à qui n'a
  // pas de compte du site, ou qui trouve le site en panne : il s'atteint par
  // un petit bouton en bas de l'écran, pas par un onglet en haut.
  //
  // ⚠ ET SANS SITE, C'EST LE CLASSIQUE QUI S'AFFICHE : voir
  // `LoginWindowView::web_door_offered`, qui commande ce champ.
  //
  // ⚠ ELLE NE SE RETIENT PAS D'UN LANCEMENT À L'AUTRE, contrairement au
  // compte : un repli n'est pas une préférence.
  LoginDoor door = LoginDoor::kMoonlight;
  WebDoorState web;
};

// CE QUE LA FENÊTRE A DEMANDÉ.
//
// ⚠ ELLE N'ENVOIE RIEN ELLE-MÊME : elle rend un verbe, et c'est `app` qui parle
// au serveur. Même règle que ses voisines.
//
// ⚠ QUITTER EST UN BOUTON NOMMÉ, PAS UNE CROIX. Une croix promet de fermer la
// fenêtre ; ici, sans fenêtre derrière, elle quitterait le jeu sans l'avoir
// dit. « Quitter le jeu », en bas du formulaire, dit ce qu'il fait.
enum class LoginVerb : std::uint8_t {
  kNothing,
  // Se connecter avec ce que portent les champs.
  kSubmit,
  // Répondre au code PIN.
  kSubmitPincode,
  // Fermer le client, par la même sortie que la sélection de personnage.
  kQuit,
};

// CE QUE LES TROIS RÉGLAGES DU PREMIER ÉCRAN ONT DEMANDÉ.
struct LoginFirstScreenAction {
  // Le rang de la langue choisie, ou -1 si personne n'a touché à rien.
  int language = -1;
  // Le rang de la police choisie, ou -1.
  int font = -1;
  // La case a basculé, et voici sa nouvelle valeur. ⚠ DEUX CHAMPS et non un :
  // « on n'a rien demandé » et « on demande faux » ne sont pas la même chose.
  bool backdrop_toggled = false;
  bool backdrop_wanted = false;
  // Le joueur veut une autre ville.
  bool backdrop_reroll = false;
};

struct LoginWindowAction {
  LoginVerb verb = LoginVerb::kNothing;
  // LE BOUTON « RÉGLAGES » DE LA BARRE DE TITRE a été cliqué : voir
  // `DrawLoginSettingsWindow`.
  bool open_settings = false;
  // Ce que la porte Moonlight a demandé, quand c'est elle qui répond.
  WebDoorAction web;
  LoginFirstScreenAction first_screen;
};

// DESSINE L'ÉCRAN DE CONNEXION.
//
// ⚠ CENTRÉ À LA PREMIÈRE OUVERTURE SEULEMENT, puis DÉPLAÇABLE, et sa place se
// retient. Deux raisons :
//   · reposer la position à chaque image annule le déplacement — la fenêtre
//     revient au centre sous le curseur de qui essaie de la bouger, et rien
//     n'indique pourquoi ;
//   · le décor donne enfin une raison de la déplacer : regarder la ville
//     derrière.
[[nodiscard]] LoginWindowAction DrawLoginWindow(LoginWindowState& state,
                                                const LoginWindowView& view, const SkinArt& art);

// L'identité de la confirmation « Quitter le jeu ? ».
inline constexpr const char* kLoginQuitConfirmId = "##login_quit";

// ÉCHAP SUR L'ÉCRAN DE CONNEXION. Une fois par image, APRÈS le formulaire et
// ses réglages ; rend vrai quand le joueur a confirmé qu'il quitte.
//
// ⚠ LES RÉGLAGES D'ABORD : ouverts, Échap les ferme et rien d'autre. Un Échap
// que personne ne prend — ni fenêtre, ni surgissant, ni champ en saisie —
// demande « Quitter le jeu ? ». Échap de nouveau vaut « non », Entrée « oui ».
//
// ⚠ UNE QUESTION, ET NON UNE SORTIE SÈCHE : Échap est aussi le geste qu'on fait
// pour refermer ce qu'on croit encore ouvert.
[[nodiscard]] bool FollowLoginEscape(EscapeStack& escape, bool* settings_open, const SkinArt& art);

}  // namespace ro::ui
