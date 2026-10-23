#pragma once

#include <cstdint>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE MENU D'ÉCHAP — celui que le client appelle `UIEscOptionWnd`.
//
// ⚠ C'EST LE SEUL ÉCRAN QUE TOUT LE MONDE SAIT ATTEINDRE, et c'est ce qui en
// fait la place naturelle de tout ce qui se règle : un joueur qui ne trouve plus
// une fenêtre essaie Échap.
//
// ⚠ IL NE FAIT RIEN LUI-MÊME. Il REND ce que le joueur a choisi ; c'est la
// boucle qui quitte, qui ouvre, qui envoie. Une fenêtre qui quitterait le jeu
// depuis son propre dessin le ferait au milieu d'une image.
//
// 🔴 TROIS DISPOSITIONS, RELUES À CHAQUE IMAGE, jamais mises en cache : le
// joueur peut MOURIR menu ouvert, et un menu qui garderait alors ses boutons de
// vivant proposerait des gestes que le serveur refuse.
//
//   vivant                       -> la liste complète
//   mort                         -> retour au point de sauvegarde, retour au jeu
//   mort + jeton de résurrection -> la résurrection en plus, EN TÊTE

enum class GameMenuChoice : std::uint8_t {
  kNothing,
  kBackToGame,
  // ⛔ « Réglages Moonlight » A DISPARU : les deux panneaux n'en font plus
  // qu'un. Voir `options_window.h`, qui dit pourquoi la séparation ne tenait
  // pas.
  kOptions,
  kShortcuts,
  // LES DIX MACROS DE CHAT. ⚠ ELLES N'ONT AUCUN CHEMIN D'OUVERTURE DANS LE MENU
  // DU CLIENT : elles ne s'atteignent qu'au raccourci. Un joueur qui l'a
  // remappé, ou qui ne l'a jamais su, n'y accédait donc plus du tout.
  kChatMacros,
  // LES COULEURS DU PERSONNAGE. ⚠ AUCUN CHEMIN D'OUVERTURE DANS LE CLIENT
  // D'ORIGINE : c'est un ajout de ce serveur, et son styliste peut aussi
  // l'ouvrir lui-même — voir `ZC_MOONLIGHT_STYLE_OPEN`.
  kStyleEditor,
  // LE LECTEUR DE MUSIQUE. ⚠ UN AJOUT DE CE CLIENT : voir
  // `music_player_window.h`.
  kMusicPlayer,
  // LA SALLE DES MINI-JEUX. ⚠ UN AJOUT DE CE CLIENT : voir `minigames_window.h`.
  kMinigames,
  // L'établi du staff. ⚠ Ce bouton n'apparaît que pour qui y a droit : voir
  // `staff_tools.h`, qui dit d'où vient ce droit et pourquoi il ne se mémorise
  // pas.
  kStaffTools,
  // « RETOUR À MON POINT DE SAUVEGARDE », le personnage VIVANT.
  //
  // 🔴 CE N'EST PAS LE « retour au point de sauvegarde » DU MENU DE MORT. Rien
  // dans le protocole ne permet à un vivant de se téléporter : c'est une
  // commande du SERVEUR, envoyée telle qu'un joueur la taperait — ce qui a
  // l'avantage de faire revenir son refus éventuel dans la conversation, à sa
  // place.
  kSavePointCommand,
  // La réapparition après un décès, elle, est un paquet : voir `kRespawn`.
  kRespawn,
  // REVENIR À LA SÉLECTION DE PERSONNAGE, sans quitter le client. Le serveur
  // l'accorde ou le refuse ; voir `ro/net/character_select_return.h`.
  kCharacterSelect,
  // Se relever sur place. ⚠ Le serveur exige un jeton de Siegfried ou l'état de
  // régénération : le bouton n'apparaît que lorsque l'un des deux est là.
  kResurrect,
  kQuit,
};

struct GameMenuState {
  bool open = false;
  // ── DE QUOI RENDRE CE QU'ON A PRIS ─────────────────────────────────────────
  // Le menu s'ouvre SEUL à la mort (voir `FollowDeath`). Ces deux-là sont ce
  // qu'il faut pour le refermer seul aussi, et seulement quand c'est bien lui
  // qui l'avait ouvert.
  bool opened_by_death = false;
  bool dead_last_frame = false;
};

// CE QUE LA SESSION PERMET AU MENU, relu à chaque image.
//
// ⚠ UN TYPE NOMMÉ PLUTÔT QUE QUATRE `bool` À LA SUITE : ils se confondraient à
// l'appel, et l'inversion de deux d'entre eux ferait proposer la résurrection à
// un vivant sans que rien ne le signale.
struct GameMenuFacts {
  // Le droit d'ouvrir l'établi du staff.
  bool staff = false;
  // Vrai tant que la liaison tient. ⚠ LIAISON PERDUE, LES GESTES QUI PARLENT AU
  // SERVEUR DISPARAISSENT : il n'y a plus personne à qui demander, et un bouton
  // qui ne fait rien se prend pour une panne.
  bool online = false;
  // Le personnage est-il mort ?
  bool dead = false;
  // Peut-il se relever sur place ?
  bool can_resurrect = false;
};

inline constexpr const char* kGameMenuTitle = core::TrText("Options du jeu");
inline constexpr const char* kGameMenuId = "menu";

// L'identité de la question du retour au point de sauvegarde. ⚠ UNE SEULE
// CHAÎNE POUR L'OUVERTURE ET POUR LE DESSIN : ImGui apparie ses boîtes par
// identifiant, et deux libellés différents ne se retrouvent pas — le bouton ne
// ferait alors simplement rien.
inline constexpr const char* kGameMenuRespawnId = "menu_respawn";

// LE MENU DE MORT S'OUVRE DE LUI-MÊME, À L'INSTANT DE LA MORT.
//
// 🔴 MOURIR N'EST PAS UN GESTE DU JOUEUR, et c'est ce qui le distingue de tout
// le reste de ce menu. Un personnage à terre ne peut plus rien faire d'autre
// que les deux choses qui sont ici ; attendre qu'il pense à presser Échap, c'est
// le laisser devant un écran qui ne répond plus, sans dire pourquoi.
//
// ⚠ AU FRANCHISSEMENT, ET NON TANT QU'IL EST MORT : un menu réouvert à chaque
// image ne se refermerait plus, et le joueur qui veut regarder l'endroit où il
// est tombé — c'est ce qu'on fait en attendant un soigneur — ne le pourrait
// plus.
//
// ⚠ ET IL SE REFERME AU RETOUR À LA VIE, mais SEULEMENT s'il s'était ouvert
// tout seul. Ce qu'on n'a pas ouvert ne se referme pas : un joueur ressuscité
// menu ouvert par lui-même le retrouve ouvert. Celui qui vient d'être relevé
// par un soigneur, lui, reprend la main tout de suite — on se fait frapper
// pendant ce temps-là.
void FollowDeath(GameMenuState& state, bool dead);

// ⚠ `DrawGameMenu` APPELLE `FollowDeath` LUI-MÊME, à chaque image et avant toute
// chose : la règle appartient au menu, pas à ses appelants. Un étage qui devrait
// penser à l'appeler l'oublierait, et la mort resterait muette.
[[nodiscard]] GameMenuChoice DrawGameMenu(GameMenuState& state, const SkinArt& art,
                                          const GameMenuFacts& facts);

}  // namespace ro::ui
