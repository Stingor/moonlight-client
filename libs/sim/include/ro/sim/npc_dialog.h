#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ro::sim {

// LE DIALOGUE D'UN PNJ : ce qu'il dit, et ce qu'il attend du joueur.
//
// Le serveur ne décrit pas une fenêtre, il envoie une SUITE. Des lignes
// (ZC_SAY_DIALOG), puis un bouton — « suivant » (ZC_WAIT_DIALOG) ou « fermer »
// (ZC_CLOSE_DIALOG) —, ou une liste de choix (ZC_MENU_LIST), ou encore une
// demande de saisie (ZC_OPEN_EDITDLG / ZC_OPEN_EDITDLGSTR). C'est le client
// qui accumule et qui décide de la forme.

// Ce que le dialogue attend. Tant que rien n'est attendu, le PNJ parle encore
// et il n'y a rien à cliquer.
enum class NpcPrompt : std::uint8_t { kNone, kNext, kClose, kMenu, kNumber, kText };

// Ce que le serveur attend quand un menu est ANNULÉ.
//
// ⚠ DEUX CENT CINQUANTE-CINQ, ET SURTOUT PAS ZÉRO. `clif_parse_NpcSelectMenu`
// traite un rang nul, ou supérieur au nombre d'options, comme une manipulation
// du protocole : il APPELLE `clif_GM_kick` et jette le joueur hors du jeu. Un
// client qui se trompe de convention ne provoque pas un message d'erreur, il
// provoque une déconnexion que rien n'explique.
inline constexpr std::uint8_t kNpcMenuCancelled = 255;

// Le premier rang d'un menu, tel que le serveur les compte.
inline constexpr std::uint8_t kNpcMenuFirstChoice = 1;

// Le dernier rang qu'un octet de choix puisse porter : au-delà vit
// l'annulation, et le serveur le dit lui-même (« client supports only up to 254
// entries », `buildin_menu`).
inline constexpr std::uint8_t kNpcMenuLastChoice = 254;

// UNE OPTION DE MENU, ET SON RANG.
//
// 🔴 LE RANG N'EST PAS LA POSITION, ET LES OPTIONS VIDES SONT CE QUI LES SÉPARE.
// `menu_countoptions` (script.cpp) n'incrémente son compteur que pour une entrée
// NON VIDE : `sd->npc_menu` vaut donc le nombre d'options affichables, et le
// contrôle de `clif_parse_NpcSelectMenu` est `select > sd->npc_menu`. Envoyer la
// position absolue d'une option qui suit une entrée vide dépasse ce compte et
// fait ÉJECTER le joueur.
//
// (Le serveur, lui, retraduit ensuite ce rang en position absolue pour la valeur
// de retour de `select()` — d'où la confusion : `select("a","","b")` REND 3 pour
// « b », et le client ENVOIE 2.)
struct NpcMenuOption {
  // Le libellé, BALISAGE COMPRIS : c'est l'affichage qui le lit.
  std::string text;
  // Le rang à envoyer, compté à partir de un parmi les options non vides.
  std::uint8_t rank = 0;
};

// Le silence après lequel une page inachevée s'affiche quand même.
//
// 🔴 IL SE MESURE EN SILENCE, PAS EN DURÉE TOTALE, ET IL EST LARGE EXPRÈS. Le
// moteur de script du serveur ne rend jamais la main entre deux `mes` : tous les
// paquets d'une page partent ensemble, et seul TCP peut les espacer — de
// quelques images, jamais d'un tiers de seconde. Un `sleep` de script, lui, dure
// au moins une seconde. Un seuil court publiait une DEMI-PAGE au milieu d'une
// rafale : un agent de téléportation écrit sept cents lignes d'affilée, et le
// menu qui les termine arrivait après.
inline constexpr std::uint32_t kNpcPageIdleMs = 400;

// Ce qu'une page publiée ne dépassera pas.
//
// ⚠ C'EST UN GARDE-FOU, PAS UN BUDGET D'AFFICHAGE. Un plafond serré tronque des
// pages légitimes par le HAUT — donc leur en-tête, donc ce qui les explique.
inline constexpr std::size_t kNpcDialogMaxLines = 4000;

struct NpcDialog {
  std::uint32_t npc_id = 0;

  // Ce qui est écrit, ligne à ligne : la page AFFICHÉE.
  std::vector<std::string> lines;

  // 🔴 LA PAGE EN COURS DE RÉCEPTION, ET C'EST TOUTE LA FORME DU DIALOGUE. Le
  // serveur envoie une page en PLUSIEURS paquets : un ZC_SAY par `mes`, puis
  // celui qui la TERMINE (« suivant », « fermer », un menu, une saisie).
  // L'afficher au fil de l'eau fait grandir le texte sur toute la hauteur, puis
  // le RABOTER quand le menu arrive — un réarrangement d'autant plus visible que
  // la page est bavarde. On la tient donc en attente et on la publie ENTIÈRE,
  // d'un coup : la hauteur du menu est connue avant de dimensionner le texte.
  // C'est aussi ce que fait le client officiel, dont la liste de choix est une
  // fenêtre séparée.
  std::vector<std::string> pending_lines;
  // La page en attente REMPLACE l'affichée au lieu de s'y ajouter : un saut de
  // page, armé par `clear_on_next_line`.
  bool pending_replaces = false;

  NpcPrompt prompt = NpcPrompt::kNone;
  // Les options, quand il y a un menu. Les entrées vides n'y sont pas : elles
  // ne s'affichent pas et ne se choisissent pas — elles ne font qu'avancer les
  // rangs, ce dont `NpcMenuOption::rank` se souvient déjà.
  std::vector<NpcMenuOption> options;

  // Vrai après un « suivant », un choix de menu ou un `clear` : la prochaine
  // ligne EFFACE ce qui est écrit. C'est le comportement que le serveur
  // documente au-dessus de ZC_WAIT_DIALOG (« set to clear on next mes »), et il
  // appartient au client.
  bool clear_on_next_line = false;

  // 🔴 UNE RÉPONSE EST PARTIE, CELLE DU SERVEUR N'EST PAS ENCORE LÀ. La page
  // reste alors telle quelle — bouton, liste, champ —, simplement INERTE, au
  // lieu d'être effacée tout de suite. L'effacer faisait sauter le pied de
  // fenêtre : le bouton principal disparaissait, son voisin glissait sous le
  // curseur, et un second clic répondait deux fois.
  bool awaiting_reply = false;

  // LA GÉNÉRATION DE LA PAGE, incrémentée à chaque publication. L'affichage s'en
  // sert pour savoir que sa mise en page ne vaut plus, et qu'une page neuve se
  // lit par le HAUT.
  std::uint32_t page = 0;

  // Celle du menu, incrémentée à chaque liste reçue, et celle qui a DÉJÀ reçu sa
  // réponse.
  //
  // ⚠ SANS CE COUPLE, UN SECOND ENVOI PART. Le premier choix fait avancer le
  // script, et le second vise un menu qui n'a plus la même longueur : « got 14,
  // valid [1..3] », c'est-à-dire l'éjection.
  std::uint32_t menu = 0;
  bool menu_answered = false;

  // Quand le dernier paquet de dialogue est arrivé, en millisecondes de
  // l'horloge de l'appelant. Sert au seul filet de `kNpcPageIdleMs`.
  std::uint32_t last_packet_ms = 0;

  // LE DIALOGUE RETIENT LE MONDE : le clic ne vise plus ni le sol ni une entité.
  // Un texte, un « suivant » ou un menu le lèvent ; « fermer » le baisse, alors
  // même que son bouton reste à l'écran. Une saisie n'y touche pas : demandée
  // sans rien dire d'abord, elle laisse le monde cliquable.
  bool holds_world = false;

  // Ajoute une ligne à la page en RÉCEPTION, en la découpant sur ses sauts de
  // ligne — un `mes` peut en porter plusieurs, et le client officiel les découpe
  // aussi.
  void PushLine(std::string_view line);

  // Publie la page reçue. Rend vrai si l'affichage a changé.
  //
  // ⚠ RIEN DE NEUF NE GARDE LA PAGE TELLE QUELLE : un terminateur sans `mes` —
  // un menu qui suit un choix, un « fermer » sec — ne doit pas vider l'écran.
  bool CommitPage();

  // Le rang à envoyer pour l'option affichée à cette position, ou zéro si la
  // position n'en désigne aucune.
  [[nodiscard]] std::uint8_t RankAt(std::size_t position) const;
};

// Découpe les options d'un ZC_MENU_LIST. Le séparateur est le deux-points.
//
// ⚠ LES ENTRÉES VIDES SONT RETIRÉES, ET LEUR RANG EST CONSERVÉ : voir
// `NpcMenuOption`. Un menu vide ne rend aucune option.
//
// ⚠ UN DEUX-POINTS DANS UN LIBELLÉ SCINDE L'OPTION. C'est une limite du
// protocole, pas de ce découpage : le client officiel fait pareil, et c'est au
// script de ne pas en écrire.
[[nodiscard]] std::vector<NpcMenuOption> SplitNpcMenu(std::string_view menu);

}  // namespace ro::sim
