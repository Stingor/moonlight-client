#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/staff_homunculus.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {

// LE GRIMOIRE : LA GRILLE D'ICÔNES DU CLIENT, DANS LA FICHE DE PERSONNAGE.
//
// ⚠ ELLE NE CLASSE RIEN ET NE COMPTE RIEN. Une case lui arrive avec son rang,
// son rang visé, son icône et sa place déjà décidés ; l'onglet où elle tombe est
// décidé ailleurs aussi. `ui` ne connaît ni l'arbre du client ni le livre du
// serveur — elle dessine ce qu'on lui donne et rend ce que le joueur a demandé.
//
// ⚠ ET ELLE N'ENVOIE RIEN. Un clic REND une intention ; c'est la boucle qui
// réserve, et « Appliquer » qui parle au serveur.

// Un prérequis, tel que la grille en a besoin : de quoi savoir quelles cases
// s'allument autour de celle qu'on survole.
struct SkillNeedView {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;
};

// Une case de la grille.
struct SkillCellView {
  std::uint16_t skill_id = 0;
  // Le rang de case, sur une grille de sept colonnes. DÉJÀ DÉCALÉ quand deux
  // arbres sont fusionnés dans le même onglet.
  int position = 0;
  std::string_view name;
  // L'icône, vide quand l'installation ne l'a pas. La case reste alors lisible :
  // le rang s'écrit dessous de toute façon.
  SkinImage icon;
  int learned = 0;
  // Le rang VISÉ par la réservation, zéro quand rien n'est réservé.
  int reserved = 0;
  int max_level = 0;
  // Un rang de plus est payable tout de suite.
  bool raisable = false;
  std::span<const SkillNeedView> needs;

  // ── LE TEMPS D'ATTENTE ────────────────────────────────────────────────────
  //
  // ⚠ LES DEUX, ET CE N'EST PAS UN DOUBLON — c'est la convention de la barre de
  // raccourcis, reprise à la lettre pour qu'un joueur qui connaît l'une lise
  // l'autre sans rien réapprendre : la PART dessine le voile qui monte, les
  // MILLISECONDES écrivent le décompte. Le voile seul ne dit rien d'une attente
  // de trois minutes, il avance d'un pixel par seconde.
  float cooldown = 0.0F;
  std::uint32_t cooldown_ms = 0;

  // LE RANG AUQUEL ON LA LANCE, qui n'est pas toujours celui qu'on a appris.
  //
  // 🔴 LANCER PLUS BAS A UN SENS : un Heal de rang 1 coûte dix points de magie
  // là où le rang 10 en coûte quarante, et sur une égratignure c'est le rang 1
  // qu'on veut. Le client garde ce choix par compétence, et nous aussi.
  int use_level = 0;
  // Son effet dépend-il du rang lancé ? ⚠ SEULES CELLES-LÀ SE LANCENT EN
  // DESSOUS : proposer le choix sur les autres offrirait un réglage sans effet.
  bool level_matters = false;
  // Peut-on la lancer du tout ? ⚠ `ui` NE SAIT PAS CE QU'EST UNE PASSIVE : c'est
  // `app` qui a lu les drapeaux de ciblage du serveur, et qui tranche.
  bool castable = false;
  // ── CE QUE LA VUE EN LISTE MONTRE EN COLONNES ─────────────────────────────
  //
  // ⚠ LA GRILLE N'EN A QUE FAIRE : une case de soixante-douze pixels ne porte
  // qu'une icône et un rang. C'est la liste qui a la place de les écrire, et
  // c'est pour cela qu'on la garde.
  //
  // Ce que lancer coûte, au rang appris. Zéro pour ce qu'on n'a pas appris.
  int sp_cost = 0;
  // La portée, en cases. Zéro pour ce qui n'en a pas.
  int range_cells = 0;
  // 🔴 PASSIVE : rien à lancer, jamais. ⚠ DISTINCT DE `castable`, qui est faux
  // AUSSI pour une compétence qu'on n'a pas encore : la colonne du coût écrit
  // « passif » dans un cas et « - » dans l'autre, et les confondre ferait passer
  // une compétence qu'on peut apprendre pour une qu'on ne lancera jamais.
  bool passive = false;

  [[nodiscard]] int effective_level() const noexcept {
    return learned > reserved ? learned : reserved;
  }
};

// LA FRONTIÈRE ENTRE DEUX ARBRES FUSIONNÉS : la ligne où le suivant commence.
//
// ⚠ SANS CE REPÈRE, les compétences de deux classes se lisent comme un seul
// arbre continu. Le libellé est vide quand la frontière sépare deux métiers d'un
// MÊME palier — le répéter à chaque maillon de la chaîne ne dirait rien.
struct SkillSplitView {
  int row = 0;
  std::string_view label;
};

// Un onglet du grimoire. Il peut fusionner PLUSIEURS arbres : la chaîne d'un
// Seigneur Chevalier en compte trois au même palier.
struct SkillGroupView {
  std::string_view label;
  std::span<const SkillCellView> cells;
  // Rangées par ligne croissante : le calcul d'ordonnée les parcourt dans
  // l'ordre et s'arrête au premier trait qui vient après la ligne cherchée.
  std::span<const SkillSplitView> splits;
};

// ── CE QUE LA LIGNE DU STAFF DEMANDE ────────────────────────────────────────
//
// 🔴 UNE INTENTION, ET NON LA COMMANDE. Ces quatre gestes passent par des
// atcommands, et le vocabulaire du serveur n'a rien à faire dans une fenêtre :
// c'est `app` qui compose « @resetskill », comme il compose déjà « @storage »
// pour l'ouverture du coffre.
//
// ⚠ ET ELLES COURT-CIRCUITENT LA RÉSERVATION : elles agissent TOUT DE SUITE, là
// où un clic sur une case attend « Appliquer ». Un point réservé sur une
// compétence que « Reset » vient d'effacer se ferait jeter au tri de l'image
// suivante — mais après avoir compté pour rien dans l'en-tête. La réservation se
// vide donc avec elles.
enum class StaffSkillOrder : std::uint8_t {
  kNone,
  // Oublie toutes les compétences et rend les points.
  kForgetAll,
  // Apprend toutes les compétences de la classe.
  kLearnAll,
  // Remet à zéro tous les temps d'attente — homoncule et mercenaire compris.
  kClearCooldowns,
  // Donne le nombre de points de compétence saisi.
  kGivePoints,
  // Crée l'homoncule de la classe `staff_homunculus_job`.
  kMakeHomunculus,
};

// CE QUE LA LIGNE DU STAFF SAIT DU MONDE, posé par `app` à chaque image.
struct GrimoireStaffView {
  // Les homoncules que le serveur sait créer.
  std::span<const StaffHomunculusChoice> homunculi;
  // ⚠ LE SERVEUR REFUSE D'EN CRÉER UN SECOND : le bouton se grise quand le
  // client SAIT que le personnage en a un. Faux ne prouve rien : un homoncule
  // déjà au repos ou mort à la connexion n'est pas annoncé au client.
  bool has_homunculus = false;
};

// Les bornes du champ de points, et sa valeur d'ouverture. ⚠ UNE POIGNÉE ET NON
// DES CENTAINES : ce sont des points de COMPÉTENCE, dont un arbre entier ne
// demande que quelques dizaines.
inline constexpr int kStaffSkillPointsFirst = 10;
inline constexpr int kStaffSkillPointsLeast = 1;
inline constexpr int kStaffSkillPointsMost = 9999;

// Ce que l'ONGLET garde d'une image à l'autre.
//
// ⚠ IL N'A PLUS D'INTERRUPTEUR : c'est la fiche qui s'ouvre et se ferme, et
// le grimoire est l'un de ses onglets. Deux interrupteurs pour une fenêtre
// auraient fini par se contredire.
struct SkillWindowState {
  // L'onglet de métier ouvert. Ramené à zéro quand il disparaît — un changement
  // de métier peut retirer un onglet.
  int group = 0;
  // Le filtre par nom, tel que le joueur le tape.
  char filter[64] = {};
  // ⚠ LA CASE SURVOLÉE À L'IMAGE PRÉCÉDENTE, et elle vit ICI plutôt que dans une
  // variable de fichier : `ui` ne possède aucun état, et deux fiches ouvertes
  // sur deux personnages ne doivent pas se voler leur survol.
  //
  // La retenir d'une image à l'autre est ce qui permet d'allumer les cases
  // voisines SANS second parcours : le foyer est connu avant qu'on dessine.
  std::uint16_t hovered_skill = 0;

  // ── LE CLIC QUI ATTEND DE SAVOIR CE QU'IL EST ─────────────────────────────
  //
  // 🔴 CLIC SIMPLE ET DOUBLE-CLIC COMMENCENT PAREIL, et il faut donc ATTENDRE.
  // Sans cela, un double-clic pour lancer dépense d'abord un point au passage.
  // L'action du clic simple est différée du temps d'un double-clic.
  //
  // La case où l'appui est parti, zéro si aucun.
  std::uint16_t pressed_skill = 0;
  // 🔴 LE VERDICT « RELÂCHÉ SANS AVOIR GLISSÉ », PRIS AU RELÂCHÉ ET RETENU.
  // `GetMouseDragDelta` ne rend plus rien passé l'image du relâché : le relire
  // trois dixièmes plus tard donnerait « n'a pas glissé » pour tout le monde, et
  // un glisser vers la barre réserverait un point au passage.
  bool pressed_moved = false;
  // La case dont le menu contextuel est ouvert, zéro si aucun.
  std::uint16_t menu_skill = 0;
  // LISSER LES ICÔNES DE LA GRILLE, plutôt que de les garder nettes.
  //
  // ⚠ NE SE VOIT QU'EN GRILLE, et la case n'y paraît donc que là : en liste, une
  // icône fait une hauteur de ligne — plus PETITE que son bitmap —, et il n'y a
  // pas d'agrandissement dont le mode se discute.
  //
  // 🔴 ET LE MODE NET S'IMPOSE AU DESSIN, il ne se déduit pas de la texture. Le
  // moteur de rendu d'ImGui lie un objet SAMPLER avant chaque lot de commandes,
  // et un sampler lié écrase le filtrage de la texture : le `GL_NEAREST` du
  // téléversement ne décide de rien. Le sampler ambiant est le LINÉAIRE — donc
  // ne rien faire, c'est lisser. Voir `ImposeIconSampler`.
  bool smooth_icons = false;

  // ── LE MODE CLAVIER ───────────────────────────────────────────────────────
  //
  // Une touche TENUE sur une compétence la pose dans la case de barre liée à
  // cette touche. Persisté par `app`, comme `smooth_icons` ; c'est `app` qui
  // écoute le clavier et mesure le maintien.
  bool keyboard_layout = false;
  // CE QUE LA CASE SURVOLÉE POSERAIT sur une barre, rang compris — le même que
  // le glisser. Un `keyboard_skill` nul : rien de posable sous la souris.
  // ⚠ RELEVÉ À CHAQUE IMAGE DU GRIMOIRE : il ne vaut que tant que l'onglet est
  // à l'écran.
  std::uint16_t keyboard_skill = 0;
  std::uint16_t keyboard_level = 0;
  // LE MESSAGE DISCRET, posé par `app`, qui suit la souris le temps de se
  // lire. `keyboard_notice_fresh` demande de relancer son horloge.
  std::string keyboard_notice;
  bool keyboard_notice_fresh = false;
  double keyboard_notice_until_s = 0.0;

  // ── LA LIGNE DU STAFF ─────────────────────────────────────────────────────
  //
  // Ce qu'elle garde d'une image à l'autre. Son droit, lui, lui descend à
  // chaque image par `DrawGrimoireTab`.
  //
  // Le nombre de points que le bouton « Points » demandera.
  int staff_points = kStaffSkillPointsFirst;
  // Le rang de l'homoncule choisi dans la liste du bouton « Créer ».
  int staff_homunculus = 0;
  // 🔴 CTRL AU MOMENT DU CLIC, PAS TROIS DIXIÈMES APRÈS. L'action du clic simple
  // est différée ; relire la touche au moment où elle part donnerait « jusqu'au
  // maximum » à qui a relâché Ctrl entre-temps, et l'inverse à qui l'a pressé
  // après coup.
  bool pressed_to_max = false;

  // ── LE PORTAIL DE LA MOLETTE ──────────────────────────────────────────────
  //
  // 🔴 PARCOURIR LE GRIMOIRE NE DOIT RIEN RÉGLER. La molette pose le rang de
  // lancement d'une compétence ; sans ces quatre valeurs, descendre la grille
  // changerait au passage le rang de chaque case effleurée — et rien à l'écran
  // ne dirait pourquoi.
  //
  // Trois gardes, chacune nécessaire : un DÉLAI DE POSE (la case ne prend la
  // molette qu'après un court survol immobile), un VERROU DE SALVE (un
  // défilement en cours garde la main jusqu'à son terme) et une SALVE DE
  // RÉGLAGE (une case qu'on règle ne se fait pas reprendre la molette au milieu
  // du geste).
  //
  // ⚠ ET LE SURVOL EST NEUF AUSSI QUAND L'IMAGE PRÉCÉDENTE NE SURVOLAIT PAS :
  // sans ce test, revenir sur une case quittée depuis longtemps hériterait de
  // son ancien chrono et prendrait la molette aussitôt.
  WheelGateState wheel;

  // 🔴 LA GRILLE OU LA LISTE ? Le même arbre, lu autrement — les icônes à leur
  // place du client, ou les lignes indentées avec leur coût, leur portée et leurs
  // coudes de liaison. La bascule vit dans la fenêtre plutôt que dans les
  // réglages : on la change en consultant, pas en réglant.
  bool grid = true;
};

// Ce que le joueur a demandé pendant l'image.
struct SkillWindowAction {
  // La compétence cliquée, zéro si aucune.
  std::uint16_t reserve = 0;
  // Ctrl enfoncé : jusqu'au rang maximal plutôt qu'un seul rang.
  bool to_max = false;
  bool apply = false;
  bool cancel = false;
  // LA COMPÉTENCE QU'ON VIENT DE LANCER, zéro si aucune. Double-clic, ou l'entrée
  // du menu contextuel.
  //
  // ⚠ ELLE PART AU RANG D'UTILISATION, pas au rang appris : c'est tout l'objet du
  // réglage ci-dessous, et `app` le relit au moment d'envoyer.
  std::uint16_t cast = 0;
  // LE RANG D'UTILISATION QU'ON VIENT DE CHANGER : la compétence, et le rang visé.
  // ⚠ LES DEUX ENSEMBLE : un rang sans compétence ne désigne rien, et zéro est un
  // rang que le menu propose de ne jamais atteindre.
  std::uint16_t use_level_for = 0;
  int use_level = 0;
  // LA COMPÉTENCE DONT ON VEUT LA DESCRIPTION, zéro si aucune.
  //
  // 🔴 CTRL + CLIC DROIT, ET C'EST LE GESTE DÉJÀ EN PLACE : le sac, le coffre et
  // la charrette ouvrent une description ainsi, et le grimoire fait de même — un
  // joueur n'a qu'un geste à retenir pour « qu'est-ce que c'est ? ».
  //
  // ⚠ ET LE CLIC DROIT SEUL NE FAIT RIEN ENCORE : c'est le menu contextuel, qui
  // n'existe pas. Le poser à moitié ferait un menu qui ne propose rien.
  std::uint16_t describe = 0;
  // CE QUE LA LIGNE DU STAFF VIENT DE DEMANDER. Voir `StaffSkillOrder`.
  StaffSkillOrder staff_order = StaffSkillOrder::kNone;
  // ⚠ N'A DE SENS QUE POUR `kGivePoints` : ailleurs, la commande ne prend rien.
  int staff_points = 0;
  // ⚠ N'A DE SENS QUE POUR `kMakeHomunculus`.
  std::uint16_t staff_homunculus_job = 0;
};

// Les nombres de l'en-tête. Groupés plutôt qu'en quatre paramètres voisins : ils
// se lisent ensemble et l'ordre de quatre entiers devient vite un piège.
struct SkillPointTally {
  int left = 0;
  int reserved = 0;
};

// L'ARÊTE « ce prérequis -> ce qu'il débloque » EST-ELLE REDONDANTE ?
//
// 🔴 LA LISTE DE PRÉREQUIS DU CLIENT PORTE DES RACCOURCIS : Bowling Bash réclame
// Bash ET Magnum Break, alors que Magnum Break réclame déjà Bash. Cent
// trente-six arêtes redondantes ont été comptées dans `skillinfolist`. Tracées
// telles quelles, elles doubleraient le même chemin — et une liste indentée les
// montrerait deux fois.
//
// ⚠ CES RACCOURCIS NE FONT PAS DE LA LISTE UNE FERMETURE TRANSITIVE, et c'est le
// contraire qu'on croirait : elle est aussi INCOMPLÈTE. Monter une chaîne demande
// donc de la remonter soi-même — ce que fait `sim`.
//
// ⚠ PUBLIQUE PARCE QUE C'EST UNE RÈGLE DE L'ARBRE, pas un trait : les flèches de
// la grille et les coudes de la liste l'appliquent toutes deux, et elle se
// vérifie sans fenêtre.
[[nodiscard]] bool SkillEdgeIsRedundant(std::span<const SkillCellView> cells,
                                        const SkillCellView& dependent, std::uint16_t prereq_id);

// LA LARGEUR QUE LA GRILLE DEMANDE, chrome non compris : sept colonnes, à
// l'échelle d'interface courante.
//
// 🔴 ELLE SORT D'ICI PARCE QU'ELLE CONTRAINT LA FENÊTRE QUI L'ACCUEILLE. Les
// icônes se dessinent au double exact de leur bitmap, et la grille est donc plus
// large que « mannequin + nombres » : la fiche de personnage doit relever son
// cran large quand c'est le grimoire qu'elle montre, faute de quoi la septième
// colonne tomberait hors du cadre. La recopier là-bas en dur ferait deux nombres
// à garder d'accord, et un seul serait mis à jour.
[[nodiscard]] float SkillGridWidth();

// L'ARBRE, DANS L'ONGLET QU'ON VIENT D'OUVRIR.
//
// ⚠ SANS COQUE : l'appelant a déjà ouvert la fenêtre et l'onglet. Le titre et
// l'identité de la fenêtre vivent dans `ro/ui/character_window.h`, parce que
// la fenêtre est la fiche entière, dont le grimoire n'est qu'un onglet.
//
// 🔴 `staff_allowed` MONTRE LA LIGNE DU STAFF, et c'est le droit de la fiche,
// relu à chaque image : un droit retiré en cours de session doit faire
// disparaître la ligne sans attendre une reconnexion. Voir
// `StaffToolsAllowed`. `staff` ne sert que sous ce droit.
[[nodiscard]] SkillWindowAction DrawGrimoireTab(SkillWindowState& state,
                                                std::span<const SkillGroupView> groups,
                                                SkillPointTally points, bool staff_allowed,
                                                const SkinArt& art,
                                                const GrimoireStaffView& staff = {});

}  // namespace ro::ui
