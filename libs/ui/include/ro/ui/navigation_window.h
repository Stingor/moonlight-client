#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/ui/chat_link.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA NAVIGATION ───────────────────────────────────────────────────────────
//
// Chercher une map, un NPC ou un monstre, lire ce qu'on en
// sait, et se faire guider.
//
// ── UN SEUL PANNEAU ─────────────────────────────────────────────────────────
// Le jeu éclate cette tâche sur quatre fenêtres — recherche, itinéraire, choix
// de la trace, aide. Ici : la saisie et ses pastilles en haut, la liste à
// gauche, le détail à droite, l'itinéraire en bas. L'aide tient en trois
// phrases, au survol d'un « (?) ».
//
// ── LA RECHERCHE EST INCRÉMENTALE ───────────────────────────────────────────
// Le graphe est en mémoire : le terme part à la frappe, sans attendre Entrée.
// Les pastilles filtrent l'AFFICHAGE et ne relancent rien.
//
// ── LES GESTES D'UNE LIGNE ──────────────────────────────────────────────────
// Le clic gauche SÉLECTIONNE — le volet de détail en dépend. Maj+clic pose le
// lien dans le chat, le clic droit ouvre le menu.
//
// ── LES RÔLES ───────────────────────────────────────────────────────────────
// `app` interroge le graphe, compose les groupes et l'itinéraire, et route les
// intentions. Cette couche dessine, sélectionne et filtre.

inline constexpr const char* kNavigationWindowId = "navigation";

// La saisie, zéro final compris.
inline constexpr std::size_t kNavigationTermBytes = 64;
// Les jeux de traces que le guidage sème au sol.
inline constexpr int kNavigationTrailIconCount = 8;

enum class NavigationKind : std::uint8_t { kMap, kNpc, kMonster };

// Le filtre d'affichage des pastilles.
enum class NavigationFilter : std::uint8_t { kAll, kMaps, kNpcs, kMonsters };

// Ce que le graphe sait d'une entrée. ⚠ LES CHAMPS DÉPENDENT DE LA NATURE : un
// monstre n'a pas de cellule — son spawn couvre la map —, un NPC n'a ni niveau
// ni statistiques.
struct NavigationEntryView {
  NavigationKind kind = NavigationKind::kMap;
  // Le nom affiché : celui de la map, du NPC ou du monstre.
  std::string_view name;
  // La map qui le porte : son nom INTERNE, le seul que le guidage accepte, et
  // son nom lisible.
  std::string_view map_name;
  std::string_view map_label;
  // NPC ou point de map : la cellule. Faux, c'est la map entière.
  bool has_cell = false;
  int x = 0;
  int y = 0;
  // Monstre : son niveau, et le nombre d'exemplaires du spawn.
  int level = 0;
  int amount = 0;
  // Monstre : élément et son niveau, taille, race, en valeurs du jeu
  // (élément 0 neutre à 9 mort-vivant ; taille 0 à 2 ; race 0 à 9).
  bool has_stats = false;
  int element = 0;
  int element_level = 0;
  int size = 0;
  int race = 0;
  bool is_mvp = false;
  bool is_shop = false;
  // Monstre : ce qu'un lien de chat et la fiche désignent.
  std::uint32_t monster_id = 0;
  // L'aperçu : le sprite s'il y en a un, sinon le plan de la map.
  SkinImage sprite;
  // Un sprite est attendu pour cette ligne. ⚠ IL ARRIVE UNE IMAGE APRÈS LE
  // CLIC : la sélection se fait pendant le dessin, et `app` ne compose que la
  // ligne choisie. Le carré reste vide en attendant — le plan qui s'afficherait
  // là serait remplacé sous les yeux du joueur.
  bool sprite_expected = false;
  SkinImage map_image;
};

// Un groupe de résultats : un en-tête et ses lignes.
struct NavigationGroupView {
  std::string_view name;
  NavigationKind kind = NavigationKind::kMap;
  std::span<const NavigationEntryView> entries;
};

// Une étape de l'itinéraire.
struct NavigationRouteStep {
  std::string_view map_label;
  // Comment on la quitte — « Parler à Kafra Employee » pour un NPC de
  // transport. Vide quand on n'en sait rien, ou pour la destination.
  std::string_view passage;
  // L'étape où l'on se trouve.
  bool current = false;
};

struct NavigationRouteView {
  std::span<const NavigationRouteStep> steps;
  // Un guidage est en cours, étapes ou non : une destination sur la map
  // courante n'en a aucune.
  bool guiding = false;
  // La dernière demande n'a trouvé aucun chemin.
  bool no_path = false;
  // La dernière demande visait la map où l'on est déjà.
  bool already_here = false;
  // Une distance estimée en cellules, négative quand on ne l'a pas.
  int distance_cells = -1;
};

struct NavigationView {
  std::span<const NavigationGroupView> groups;
  // Change quand `app` remplace les résultats : la sélection d'avant
  // appartient aux résultats d'avant.
  std::uint32_t results_serial = 0;
  // La map courante, interne et lisible ; vide hors du monde.
  std::string_view current_map;
  std::string_view current_map_label;
  // Le nom lisible de la map de bornage, quand l'état en porte une.
  std::string_view bound_map_label;
  // Les maps du graphe. Zéro : les données ne sont pas chargées.
  int graph_map_count = 0;
  NavigationRouteView route;
  // Les vignettes des traces, de 1 à `kNavigationTrailIconCount`.
  std::array<SkinImage, kNavigationTrailIconCount> trail_icons;
};

// Une ligne de résultat, désignée par ses rangs.
struct NavigationRowRef {
  std::size_t group = 0;
  std::size_t entry = 0;
};

struct NavigationWindowState {
  bool open = false;
  // Passer devant à l'image suivante.
  bool raise = false;
  std::array<char, kNavigationTermBytes> term{};
  NavigationFilter filter = NavigationFilter::kAll;
  // La sélection, -1 pour aucune.
  int selected_group = -1;
  int selected_entry = -1;
  std::uint32_t seen_serial = 0;
  // La trace choisie, de 1 à `kNavigationTrailIconCount`.
  int trail_icon = 1;
  // La map INTERNE à laquelle les résultats sont bornés, vide pour aucune.
  // Posée par un lien de chat : un nom de NPC est un rôle, et le lien désigne
  // celui de SA map.
  std::string bound_map;
  // Le menu d'une ligne, armé au clic droit et ouvert hors de la liste.
  bool menu_armed = false;
  NavigationRowRef menu_row;
  // Le bouton « Signaler un bug » de la barre de titre, posé par `app` selon
  // le réglage.
  bool bug_report = false;
  // Les gestes « Téléporter » du staff, posés par `app` selon ses droits : la
  // commande `@warp`, que le serveur refuse aux autres.
  bool staff_warp = false;
  // Le côté de l'aperçu du détail, en pixels d'écran : `app` y compose le
  // sprite à cette taille.
  float preview_side = 0.0F;
};

// Ouvre la navigation SUR une recherche : la fiche d'un monstre sait QUOI, la
// navigation sait OÙ. `monsters_only` allume la pastille Monstres ; `bound_map`
// borne à une map, vide pour aucune. ⚠ `app` lance lui-même la recherche.
void OpenNavigationSearch(NavigationWindowState& state, std::string_view term, bool monsters_only,
                          std::string_view bound_map = {});

struct NavigationAnswer {
  // Le terme a changé — vide compris : la liste se vide alors.
  std::optional<std::string> term;
  // « Voir ce qu'il y a ici » : les NPC et les monstres de la map courante.
  bool show_here = false;
  // « Rechercher la map actuelle » : `term` porte alors son nom interne.
  bool search_here = false;
  // Le bornage vient d'être levé : relancer la recherche du terme.
  bool unbound = false;
  std::optional<NavigationRowRef> go_to;
  std::optional<NavigationRowRef> share;
  std::optional<NavigationRowRef> open_monster;
  // Menu d'une ligne : ce qu'il y a sur sa map.
  std::optional<NavigationRowRef> show_map_of;
  // Maj+clic ou menu : le lien de cette ligne dans la saisie du chat.
  std::optional<NavigationRowRef> link_to_chat;
  std::optional<NavigationRowRef> copy_name;
  // Le staff : `@warp` sur la case de la ligne — celle du NPC ou du point —,
  // ou sur la map entière, où le serveur tire la case.
  std::optional<NavigationRowRef> warp_to;
  // Le menu d'un MONSTRE, qui est celui de tout lien de monstre.
  //
  // ⚠ SANS JETON : la ligne n'appartient à aucune table de liens. Le geste se
  // joue sur l'identifiant, le nom et le rang posés ici, ceux de la ligne.
  ChatLinkAsk monster_link;
  std::string monster_name;
  bool monster_is_mvp = false;
  bool stop_guidance = false;
  std::optional<int> trail_icon;
  // « Signaler un bug » : `app` compose le contexte — recherche, résultat
  // choisi, guidage et itinéraire.
  bool report_bug = false;
};

// Le nom d'un élément, d'une taille, d'une race, traduit ; « ? » hors table.
[[nodiscard]] const char* NavigationElementName(int element);
[[nodiscard]] const char* NavigationSizeName(int size);
[[nodiscard]] const char* NavigationRaceName(int race);

// L'INFOBULLE D'UN LIEU DE CHAT (`<NAVIL>`) : le nom lisible de la carte, son
// plan, son nom interne et la case. Composée par `app` quand un lien de lieu
// est survolé.
struct NavigationPlaceTip {
  std::string label;
  std::string map_name;
  // Zéro pour la carte entière.
  int x = 0;
  int y = 0;
  SkinImage map_image;
};

void DrawNavigationPlaceTip(const NavigationPlaceTip& tip);

// LE CÔTÉ DE LA SILHOUETTE D'UN PNJ dans l'infobulle d'une recherche de chat,
// en pixels avant mise à l'échelle : la boîte où `app` compose le sprite, et
// celle où il se peint.
inline constexpr int kNavigationSearchTipNpcSidePx = 110;

// L'INFOBULLE D'UNE RECHERCHE DE CHAT (`<NAVS>`) : le terme, la carte de
// contexte, et ce qui répond à « lequel ? » — la silhouette du PNJ à gauche du
// plan de sa carte, ou le plan d'une carte cherchée. Composée par `app` quand
// le lien est survolé.
struct NavigationSearchTip {
  // Le terme montré : le nom lisible d'une carte, le nom d'un PNJ ou d'un
  // monstre.
  std::string term;
  // La carte de contexte, lisible ; vide sans contexte.
  std::string context_label;
  // Le nom INTERNE de la carte dont le plan se montre, celui qu'une commande
  // accepte ; vide quand aucun plan n'a de sens (PNJ ou monstre sans contexte).
  std::string plan_map_name;
  SkinImage map_image;
  // La silhouette du PNJ, quand les données de navigation le connaissent.
  SkinImage npc_image;
};

void DrawNavigationSearchTip(const NavigationSearchTip& tip);

// Dessine la fenêtre si elle est ouverte.
[[nodiscard]] NavigationAnswer DrawNavigationWindow(NavigationWindowState& state,
                                                    const NavigationView& view, const SkinArt& art);

}  // namespace ro::ui
