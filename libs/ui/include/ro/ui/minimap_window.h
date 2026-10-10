#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {

// ── LA MINIMAP ──────────────────────────────────────────────────────────────
//
// La carte de la carte courante, le joueur, et tout ce que
// le jeu sait situer dessus — commodités, groupe, guilde, quêtes, repères du
// serveur, boss, marqueurs du joueur et tracé de l'itinéraire.
//
// ── UNE FENÊTRE SANS BARRE DE TITRE ─────────────────────────────────────────
// Un calque de carte n'a rien à faire d'un bandeau : on la déplace en la
// glissant, on la redimensionne aux poignées des deux coins du bas, et la
// molette zoome. Le nom de la carte vit dans la ligne « carte,x,y » qu'on lit
// sous l'image.
//
// ── LA ZONE DE CARTE RESTE CARRÉE ───────────────────────────────────────────
// C'est le carré qu'on garde, pas la fenêtre : celle-ci porte en plus ses marges
// et ses lignes de texte. Côté minimal `kMinimapLeastSide`, plafond à
// `kMinimapScreenShare` de l'écran — une fenêtre plus grande que l'écran met sa
// poignée hors d'atteinte, et l'état se persisterait.
//
// ── LE CHOIX DU JOUEUR, ET CE QUE L'ÉCRAN EN MONTRE ─────────────────────────
// La position et le côté des réglages sont ce que le joueur a CHOISI ; la
// fenêtre en montre, à chaque image, la version bornée à l'écran du moment :
// entièrement dedans, côté rabattu à la part d'écran permise. Le bornage
// n'écrit jamais dans les réglages — seul le geste du joueur, qui tient la
// fenêtre par un glisser ou une poignée, les change. Un écran inconnu ou
// minuscule à la première image, puis un redimensionnement, ne laissent donc
// ni une fenêtre hors de l'écran ni un choix rogné.
//
// ── TRAVERSER LES CLICS ─────────────────────────────────────────────────────
// Un mode où la souris passe au travers, pour qu'on clique le monde derrière.
// Maintenir Maj au-dessus la réveille le temps du geste ; un rappel le dit au
// joueur qui s'attarde.
//
// ── LES RÔLES ───────────────────────────────────────────────────────────────
// `app` lit le monde et remplit `MinimapView` — des cellules, des couleurs et
// des images déjà chargées. Cette couche dessine, répond aux gestes, et rend
// ses demandes dans `MinimapAnswer`. Les réglages (`MinimapSettings`) sont à
// l'appelant : la fenêtre les modifie sous les gestes du joueur et dit quand
// ils méritent d'être enregistrés.

inline constexpr const char* kMinimapWindowId = "minimap";

// Côté minimal de la zone de carte, en pixels : en dessous, la flèche du joueur
// mange l'image et plus rien ne se lit.
inline constexpr float kMinimapLeastSide = 128.0F;
// La part de l'écran que la fenêtre ne dépasse jamais, sur les deux axes.
inline constexpr float kMinimapScreenShare = 0.9F;

// Bornes et pas du zoom, ceux des boutons + et − du radar du jeu : le joueur
// retrouve l'amplitude qu'il connaît.
inline constexpr float kMinimapZoomLeast = 1.0F;
inline constexpr float kMinimapZoomMost = 4.0F;
inline constexpr float kMinimapZoomStep = 4.0F / 3.0F;

// Plafond de marqueurs du joueur par carte : au-delà, la carte et la liste du
// menu deviennent illisibles.
inline constexpr int kMinimapMemoCap = 32;
// Le nom d'un marqueur en saisie, zéro final compris.
inline constexpr std::size_t kMinimapMemoNameBytes = 64;

// Les trois faces du bouton de réglages : normal, survolé, enfoncé.
inline constexpr std::size_t kMinimapButtonFaceCount = 3;

// La position d'une minimap que le joueur n'a jamais glissée.
inline constexpr int kMinimapUnplaced = -1;
// Ce dont la minimap jamais glissée se décolle des bords haut et droit.
inline constexpr float kMinimapScreenMargin = 8.0F;

// LES RÉGLAGES PERSISTÉS. Des scalaires nus, pour que l'appelant les écrive
// sans rien savoir de la fenêtre.
struct MinimapSettings {
  // La position de la fenêtre. Elle n'a pas de réglage numérique : on la glisse,
  // et le glissement se retient ici.
  //
  // ⚠ `kMinimapUnplaced` TANT QUE LE JOUEUR NE L'A PAS GLISSÉE : elle se tient alors
  // au coin haut-droit, calculé à chaque image contre l'écran du moment — le coin
  // haut-gauche est celui des icônes de menu. Un coin écrit dans le fichier
  // serait celui d'UNE taille d'écran. Une place qui ne tient plus dans l'écran
  // s'y montre ramenée, sans être réécrite.
  int pos_x = kMinimapUnplaced;
  int pos_y = kMinimapUnplaced;
  // Le côté de la ZONE DE CARTE, en pixels — pas la taille de la fenêtre.
  // Réglé à la poignée, jamais par un curseur. Un écran trop petit le montre
  // rabattu, sans le réécrire.
  int side_px = 220;
  // La souris traverse la fenêtre ; elle cesse alors aussi d'être déplaçable et
  // redimensionnable — trois faces du même choix.
  bool click_through = false;
  // 1 = la carte entière ; au-delà on ne voit que 1/zoom, centré sur le joueur.
  float zoom = kMinimapZoomLeast;
  int backdrop_percent = 70;
  int map_percent = 100;
  // Filtrage LINÉAIRE de l'image de carte, qui est très agrandie.
  bool smooth = true;
  // La demi-étendue de la flèche du joueur, en pixels, et sa teinte — blanc
  // laisse l'art tel quel.
  int arrow_half_px = 9;
  ImU32 arrow_tint = IM_COL32(255, 255, 255, 255);
  bool show_coords = true;
  // Le nom LISIBLE du lieu, replié sur la largeur de la carte.
  bool show_map_name = true;
  bool show_party = true;
  bool show_guild = true;
  bool show_quests = true;
  bool show_town = true;
  // Les sorties de la carte, à part des PNJ : on ne cherche pas l'une comme
  // on cherche l'autre.
  bool show_portals = true;
  bool show_viewpoints = true;
  bool show_boss = true;
  bool show_route = true;
};

// Une cellule de la carte.
struct MinimapCell {
  int x = 0;
  int y = 0;
};

// Une commodité ou un PNJ déclaré pour la carte : cliquable, il guide vers SA
// cellule ; survolé, il dit son libellé.
struct MinimapTownIcon {
  MinimapCell cell;
  SkinImage image;
  std::string_view label;
};

// Un membre du groupe ou de la guilde, dans la couleur que le serveur a choisie.
struct MinimapMate {
  MinimapCell cell;
  ImU32 color = IM_COL32(255, 255, 255, 255);
};

// UN COÉQUIPIER DU CHAMP DE BATAILLE, dans la couleur de son camp ; survolé, il
// dit son nom. Distinct des membres du groupe et de la guilde.
struct MinimapBattleMate {
  MinimapCell cell;
  ImU32 color = IM_COL32(255, 255, 255, 255);
  std::string_view name;
};

// Un repère posé par le serveur. Il clignote 500 ms sur 1000, la phase partant
// de sa pose : deux repères posés à des instants différents clignotent décalés.
// ⚠ `app` retire lui-même ceux qui ont expiré.
struct MinimapViewpoint {
  MinimapCell cell;
  ImU32 color = IM_COL32(255, 255, 255, 255);
  std::uint32_t age_ms = 0;
};

// LA MARQUE DE QUÊTE D'UN PNJ, à sa case : l'icône de la grande carte du client
// d'origine, qui dit la sorte de quête — un point d'exclamation, une souris à
// cliquer, un Poring… — dans la couleur que le serveur a choisie, à sa taille ;
// à défaut, la pastille de la couleur. Hors du cadrage, elle se colle au bord
// et cède la place à la flèche de sa direction — voir
// `MinimapView::quest_arrows`.
struct MinimapQuest {
  MinimapCell cell;
  SkinImage image;
  SkinImage kind_image;
};

// Les flèches d'une marque de quête collée au bord, dans cet ordre : gauche,
// droite, haut, bas.
inline constexpr std::size_t kMinimapQuestArrowCount = 4;

// Un marqueur posé par le joueur.
struct MinimapMemo {
  MinimapCell cell;
  std::string_view name;
};

inline constexpr std::uint16_t kMinimapNoTombSlot = 0xFFFF;

// LA TOMBE D'UN MVP CONNU DU GROUPE, sur CETTE carte. La couleur se décide ici :
// dorée si l'instant du retour a été mérité, orangée si l'on n'a qu'une fenêtre.
// Infobulle et clic : ouvrir le carnet sur sa ligne, Ctrl+clic pour y guider.
struct MinimapTomb {
  MinimapCell cell;
  bool exact = false;
  // Le créneau du carnet ; `kMinimapNoTombSlot` : aucun, le clic n'ouvre rien.
  std::uint16_t slot_id = kMinimapNoTombSlot;
  // L'infobulle, DÉJÀ TRADUITE ET DATÉE par `app` ; une ligne vide se saute.
  std::string_view label;
  std::string_view window_line;
  std::string_view age_line;
};

struct MinimapView {
  // Le nom INTERNE de la carte, sans extension ; vide si inconnu.
  std::string_view map_name;
  // Son nom lisible, vide si le jeu n'en connaît pas.
  std::string_view display_name;
  // La taille de la carte en cellules. Zéro : rien ne se situe, la fenêtre ne
  // montre que le fond.
  int cells_width = 0;
  int cells_height = 0;
  // La cellule CONTINUE du joueur — sans arrondi, pour que la flèche glisse — et
  // son orientation en degrés.
  float player_cell_x = 0.0F;
  float player_cell_y = 0.0F;
  float player_facing_deg = 0.0F;

  // L'image de la carte ; inutilisable, un fond dit laquelle manque et les
  // marqueurs restent. Jamais de minimap muette.
  SkinImage map_image;
  // La flèche du joueur ; inutilisable, un point la remplace.
  SkinImage arrow;
  // Normal, survolé, enfoncé. Une face manquante retombe sur la normale.
  std::array<SkinImage, kMinimapButtonFaceCount> settings_button;
  // L'épingle d'un marqueur ; inutilisable, un losange la remplace.
  SkinImage memo_point;
  SkinImage boss_icon;

  std::span<const MinimapTownIcon> town;
  std::span<const MinimapTownIcon> portals;
  std::span<const MinimapMate> party;
  std::span<const MinimapMate> guild;
  // Déjà filtrés par `app` : vides hors d'une carte de champ de bataille.
  std::span<const MinimapBattleMate> battle_mates;
  std::span<const MinimapViewpoint> viewpoints;
  std::span<const MinimapQuest> quests;
  // Gauche, droite, haut, bas : voir `kMinimapQuestArrowCount`.
  std::array<SkinImage, kMinimapQuestArrowCount> quest_arrows;
  // Le boss révélé ; hors du cadrage, son icône se colle au bord pour en donner
  // la direction.
  std::optional<MinimapCell> boss;
  // Les tombes des MVP connus du groupe, déjà filtrées par `app` (carnet
  // allumé, réglage des tombes, âge). Hors du cadrage, une tombe est omise.
  std::span<const MinimapTomb> tombs;
  // L'échoppe désignée dans la recherche d'échoppes, et son icône ; hors du
  // cadrage, l'icône se colle au bord comme celle du boss.
  std::optional<MinimapCell> store_search;
  SkinImage store_search_icon;
  // Les marqueurs du joueur sur CETTE carte, au plus `kMinimapMemoCap`.
  std::span<const MinimapMemo> memos;

  // Le chemin de l'itinéraire sur cette carte, DÉCIMÉ à ses coins et ordonné du
  // départ à l'arrivée.
  std::span<const MinimapCell> route_path;
  // Faute de chemin, le point de sortie de l'étape : un segment depuis le
  // joueur en donne au moins la direction.
  std::optional<MinimapCell> route_exit;
};

// CE QUI NE SE PERSISTE PAS, d'une image à l'autre.
struct MinimapWindowState {
  // Maj tenue au-dessus de la carte, ou l'un des deux menus ouvert.
  bool shift_engaged = false;
  bool menu_open = false;
  // Le temps de survol en mode traverser les clics, pour le rappel.
  float hint_hover_seconds = 0.0F;
  // Le marqueur en cours de pose : sa cellule et son nom.
  MinimapCell memo_cell;
  std::array<char, kMinimapMemoNameBytes> memo_name{};
  // Le geste « Téléporter ici » du staff au même menu, posé par `app` selon
  // ses droits.
  bool staff_warp = false;
  // Un réglage a bougé ; l'annonce attend la fin du geste.
  bool save_pending = false;
  WheelGateState wheel;
};

struct MinimapMemoAdd {
  MinimapCell cell;
  std::string name;
};

struct MinimapAnswer {
  // Guider vers cette cellule de la carte courante.
  std::optional<MinimapCell> guide_to;
  std::optional<MinimapMemoAdd> add_memo;
  // Retirer le marqueur de ce rang dans `MinimapView::memos`.
  std::optional<std::size_t> remove_memo;
  // Le staff : `@warp` sur cette cellule de la carte courante.
  std::optional<MinimapCell> warp_to;
  bool open_world_map = false;
  bool open_navigation = false;
  // Ouvrir le carnet de chasse sur ce créneau (clic sur une tombe).
  std::optional<std::uint16_t> open_mvp_slot;
  // Les réglages ont changé et le geste est fini : c'est le moment d'écrire.
  bool settings_changed = false;
};

// Dessine la minimap. `app` décide si elle est affichée — hors du monde, elle
// ne s'appelle pas.
[[nodiscard]] MinimapAnswer DrawMinimapWindow(MinimapSettings& settings, MinimapWindowState& state,
                                              const MinimapView& view, const SkinArt& art);

// LE CONTENU DES RÉGLAGES, le même au sous-menu du bouton et dans un panneau
// d'options : appelé, pas recopié — deux listes divergent au premier ajout.
// Rend vrai quand un réglage a changé.
[[nodiscard]] bool DrawMinimapSettings(MinimapSettings& settings, const SkinArt& art);

// ── LA GÉOMÉTRIE, isolée pour être éprouvée ─────────────────────────────────

// Une fenêtre [0,1] de largeur `span` centrée sur `center`, ramenée dans les
// bornes SANS rétrécir : près d'un bord, la vue glisse le long du bord.
struct MinimapSpan {
  float low = 0.0F;
  float high = 1.0F;
};
[[nodiscard]] MinimapSpan CenteredMinimapSpan(float center, float span);

// Le zoom après `notches` crans de molette, borné. Les fractions d'un pavé
// tactile comptent : un compte entier les arrondirait à zéro.
[[nodiscard]] float StepMinimapZoom(float zoom, float notches);

// Ce que la contrainte de taille sait de l'habillage : les marges, la hauteur
// non-carte, et le côté maximal.
struct MinimapSquareFit {
  float pad_x = 0.0F;
  float pad_y = 0.0F;
  float extra_height = 0.0F;
  float most_side = 0.0F;
};
// La taille de fenêtre qui garde la zone de carte carrée, depuis celle que le
// geste demande. Le côté est la MOYENNE des deux axes : tirer un bord seul
// répond à demi-vitesse, au lieu de rester inerte.
[[nodiscard]] ImVec2 FitMinimapSquare(ImVec2 desired, const MinimapSquareFit& fit);

}  // namespace ro::ui
