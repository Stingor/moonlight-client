#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA CARTE DU MONDE ───────────────────────────────────────────────────────
//
// L'image d'un monde, ajustée à la fenêtre sans être déformée, et par-dessus :
// l'itinéraire de la navigation, les donjons, le visage du groupe sur ses
// cartes, et au survol d'une escale, le circuit de son dirigeable. Survolée, une carte dit son nom
// et son niveau ; cliquée, elle devient la destination de la navigation. `app` compose tout ; la
// fenêtre ne lit rien.

inline constexpr const char* kWorldMapWindowId = "###world_map";

// Un rectangle de l'image du monde, en pixels de l'image.
struct WorldMapRect {
  float left = 0.0F;
  float top = 0.0F;
  float right = 0.0F;
  float bottom = 0.0F;
};

// UN MONSTRE QUI APPARAÎT SUR UNE CARTE, pour son infobulle.
struct WorldMapMonsterView {
  std::string_view name;
  int level = 0;
  int amount = 0;
  bool boss = false;
};

// Au-delà de ce nombre, l'infobulle d'une carte dit combien de monstres restent.
inline constexpr std::size_t kWorldMapMonstersShown = 8;

// UNE CARTE DU MONDE, telle que la fenêtre la montre : un champ, une ville —
// ou l'étage d'un donjon, qui n'a pas de case à lui et se montre avec celle de
// son donjon.
struct WorldMapAreaView {
  WorldMapRect rect;
  std::string_view map_name;
  std::string_view title;
  // Le niveau conseillé — vide pour une ville.
  std::string_view level;
  // Le groupe : les étages d'un donjon partagent celui de sa case.
  int group = 0;
  // Les monstres qui y apparaissent, boss en tête.
  std::span<const WorldMapMonsterView> monsters;
  // LA TEINTE DE LA RECHERCHE de la navigation, quand elle a trouvé quelque
  // chose ici ; zéro sinon. Un étage de donjon la porte aussi : sa case est
  // celle où la recherche le montre.
  ImU32 search_color = 0;
  // Et l'image de son palier, étirée sur la carte par-dessus la teinte.
  SkinImage search_image;
};

// UN DONJON : un point là où il est, et une étiquette — le rectangle de ses
// étages — qui porte son nom, reliés par un trait. Sans étage, l'étiquette est
// posée sur le point. C'est l'étiquette que la souris désigne.
struct WorldMapDungeonView {
  WorldMapRect rect;
  WorldMapRect label;
  std::string_view title;
  std::string_view level;
  int group = 0;
  std::span<const WorldMapAreaView> floors;
};

// UN MEMBRE DU GROUPE, dans la liste de sa carte.
struct WorldMapMemberView {
  std::string_view name;
  // Soi-même : son nom se distingue.
  bool self = false;
};

// UNE CARTE OÙ SE TROUVE LE GROUPE : un visage au milieu, un filet autour, et
// la liste de ceux qui y sont. Hors groupe, la seule carte est la sienne.
struct WorldMapPartyView {
  WorldMapRect rect;
  std::string_view map_name;
  // La carte où l'on est : son visage et son filet sont les siens.
  bool here = false;
  std::span<const WorldMapMemberView> members;
};

// UNE ÉTAPE DE L'ITINÉRAIRE de la navigation : un champ ou une ville — un étage
// de donjon n'en est pas une. L'itinéraire suit le guidage, la destination en
// dernier ; une carte n'y paraît qu'une fois.
struct WorldMapRouteStep {
  WorldMapRect rect;
  // Sur le monde montré. Sinon l'étape ne se peint pas, et ne se relie pas à
  // ses voisines.
  bool shown = false;
};

// UNE ESCALE DE DIRIGEABLE.
struct WorldMapAirStop {
  WorldMapRect rect;
  std::string_view map_name;
  // Le nom affiché de la carte, sur son étiquette.
  std::string_view title;
};

// LE CIRCUIT D'UNE LIGNE DE DIRIGEABLE vu depuis l'une de ses escales : il en
// part et y revient.
struct WorldMapAirTour {
  std::string_view start;
  std::span<const WorldMapAirStop> stops;
};

// UNE LIGNE DE DIRIGEABLE : un circuit par escale. Elle se montre quand la
// souris survole l'une d'elles.
struct WorldMapAirLine {
  std::span<const WorldMapAirTour> tours;
};

// Le nombre de lignes de dirigeable : chacune garde son vol dans l'état de la
// fenêtre.
inline constexpr std::size_t kWorldMapAirLineCount = 2;

// LES QUATRE PALIERS DE TAILLE de la lettre et des visages, selon la largeur de
// l'image à l'écran : jusqu'à 640, 800 et 1024 pixels, puis au-delà.
inline constexpr std::size_t kWorldMapSizeTiers = 4;
[[nodiscard]] std::size_t WorldMapSizeTier(float shown_width_pixels);

// UN TEXTE DE LA CARTE : sa lettre, son ombre, et sa taille à chaque palier.
struct WorldMapText {
  ImU32 letter = IM_COL32_WHITE;
  ImU32 shadow = IM_COL32_BLACK;
  std::array<float, kWorldMapSizeTiers> sizes{};
};

// L'APERÇU DE LA CARTE SURVOLÉE : sa minimap, son nom affiché et son nom
// interne.
struct WorldMapPreview {
  std::string_view map_name;
  std::string_view title;
  SkinImage image;
};

// LE WARP AGENT D'UNE VILLE, montré au survol : sa figure en haut à droite de
// la ville, et son nom à côté.
struct WorldMapWarpAgentView {
  std::string_view town_map;
  WorldMapRect town;
  // L'image de sa figure à cet instant, et ses pieds dans l'image.
  SkinImage sprite;
  ImVec2 sprite_feet{0.0F, 0.0F};
  // La hauteur de la figure au-dessus de ses pieds, en pixels d'écran.
  float sprite_height = 0.0F;
};

struct WorldMapView {
  // Les mondes, et celui qui est montré.
  std::span<const char* const> worlds;
  int world = 0;
  SkinImage image;
  // La taille de l'image en pixels : les rectangles s'y rapportent.
  ImVec2 image_size{1280.0F, 1024.0F};
  // Les champs et les villes ; les étages sont rangés avec leur donjon.
  std::span<const WorldMapAreaView> areas;
  std::span<const WorldMapDungeonView> dungeons;
  ImU32 dungeon_color = IM_COL32(255, 0, 0, 180);
  ImU32 route_color = IM_COL32(85, 85, 255, 102);
  ImU32 select_color = IM_COL32(230, 100, 0, 255);
  // LES DONJONS : le point, l'étiquette, et la lettre de son nom.
  SkinImage dungeon_spot;
  SkinImage dungeon_label;
  WorldMapText dungeon_name;
  // LES NOMS DES CARTES, quand ils se montrent : le filet de chaque carte, son
  // nom et son niveau.
  ImU32 field_edge = IM_COL32(80, 80, 80, 255);
  WorldMapText field_name;
  WorldMapText field_level;
  // LA CARTE SURVOLÉE : son voile, et son aperçu — celui de la carte que
  // l'image précédente désignait.
  ImU32 select_box = IM_COL32(149, 15, 15, 70);
  std::optional<WorldMapPreview> preview;
  // LE WARP AGENT de la ville désignée à l'image précédente.
  std::optional<WorldMapWarpAgentView> warp_agent;
  // LE GROUPE, sur le monde montré.
  std::span<const WorldMapPartyView> party;
  SkinImage party_face;
  SkinImage party_face_here;
  // LA CARTE DE L'ÉTAPE EN COURS du guidage, marquée d'une coche — un champ ou
  // une ville du monde montré.
  std::optional<WorldMapRect> step_mark;
  SkinImage step_mark_image;
  // L'ITINÉRAIRE : chaque étape montrée se teint, un trait la relie à la
  // précédente, et la marque d'arrivée se pose sur la dernière quand c'est la
  // destination du guidage.
  std::span<const WorldMapRouteStep> route;
  bool route_reaches_goal = false;
  SkinImage route_line;
  SkinImage route_goal;
  // LES DIRIGEABLES : le trait du circuit, la trace et l'appareil.
  std::span<const WorldMapAirLine> air_lines;
  SkinImage air_line;
  SkinImage air_trail;
  SkinImage airship;
};

// LE VOL D'UNE LIGNE DE DIRIGEABLE : le circuit choisi en arrivant sur une
// escale, l'étape en cours et le chemin déjà fait.
struct WorldMapAirState {
  // Le circuit, rang dans `WorldMapAirLine::tours` ; moins un : au repos.
  int tour = -1;
  std::size_t leg = 0;
  float traveled = 0.0F;
  float leg_length = 0.0F;
};

struct WorldMapWindowState {
  bool open = false;
  bool raise = false;
  // Le monde choisi dans la liste ; moins un : celui de la carte où l'on est.
  int world = -1;
  std::array<WorldMapAirState, kWorldMapAirLineCount> air;
  // Les noms et niveaux des cartes, que la case de la fenêtre montre.
  bool show_names = false;
  // La carte survolée à la dernière image : son aperçu se compose à la
  // suivante. Vide : aucune.
  std::string hovered_map;
  // Le palier de taille de la dernière image : le Warp Agent s'y compose.
  std::size_t size_tier = 0;
  // Le clic droit du staff sur une carte, posé par `app` selon ses droits, et
  // la carte que son menu désigne — le premier étage pour un donjon.
  bool staff_warp = false;
  std::string staff_menu_map;
};

struct WorldMapAction {
  // Un autre monde, choisi dans la liste.
  std::optional<int> world;
  // La carte à rejoindre, cliquée — le premier étage pour un donjon.
  std::string guide_to;
  // Le staff : `@warp` sur cette carte, où le serveur tire la case.
  std::string warp_to;
};

[[nodiscard]] WorldMapAction DrawWorldMapWindow(WorldMapWindowState& state,
                                                const WorldMapView& view, const SkinArt& art);

// ── LA GÉOMÉTRIE, isolée pour être éprouvée ─────────────────────────────────

// L'image ajustée à `room` sans déformation, centrée : son coin et son échelle.
struct WorldMapFit {
  ImVec2 origin{0.0F, 0.0F};
  float scale = 0.0F;
};
[[nodiscard]] WorldMapFit FitWorldMap(ImVec2 image_size, ImVec2 room_min, ImVec2 room_max);

// La carte sous un point de l'image, la PLUS PETITE qui le contient — une ville
// dans un champ plus large —, ou rien.
[[nodiscard]] std::optional<std::size_t> WorldMapAreaAt(std::span<const WorldMapAreaView> areas,
                                                        ImVec2 image_point);

// L'APERÇU D'UNE CARTE SURVOLÉE, carré, posé à dix pixels à droite de la carte —
// à gauche quand la droite déborde — et dix pixels sous son haut — remonté
// quand le bas déborde. Les coordonnées sont celles de l'écran.
inline constexpr float kWorldMapPreviewSize = 130.0F;
[[nodiscard]] ImVec2 WorldMapPreviewAt(ImVec2 rect_min, ImVec2 rect_max, ImVec2 screen_end);

// LA LISTE DU GROUPE, à côté de l'aperçu : à sa droite, un peu plus bas, quand
// l'aperçu est à droite de la carte du groupe ; à sa gauche sinon ; à gauche de
// la carte quand la place manque ; remontée quand le bas déborde.
[[nodiscard]] ImVec2 WorldMapMemberListAt(ImVec2 preview_at, ImVec2 party_min, ImVec2 party_max,
                                          ImVec2 size, ImVec2 screen_end);

// UN QUADRILATÈRE TEXTURÉ, en pixels d'écran. Ses coins vont dans l'ordre de
// l'image : haut gauche, haut droit, bas gauche, bas droit.
struct WorldMapQuad {
  std::array<ImVec2, 4> corners{};
  ImU32 color = IM_COL32_WHITE;
};

// UN TRAIT TEXTURÉ de `from` à `to` : la largeur de l'image en travers, sa
// hauteur le long du trait. Rien pour un trait de longueur nulle.
[[nodiscard]] std::optional<WorldMapQuad> WorldMapBand(ImVec2 from, ImVec2 to, float half_width,
                                                       ImU32 color);

// L'AVANCE D'UN VOL. Survoler une escale choisit son circuit, que le vol garde
// tant que la souris reste sur l'une des escales de la ligne ; ailleurs, il se
// pose. L'appareil va d'escale en escale à vitesse constante — en pixels
// d'écran, d'où `pixels_per_unit`, l'échelle de l'image —, et repart de la
// première après la dernière. Rend l'avance sur l'étape en cours, de zéro à un
// — un peu plus à l'image où elle s'achève —, ou rien au repos.
[[nodiscard]] std::optional<float> AdvanceWorldMapAir(WorldMapAirState& state,
                                                      const WorldMapAirLine& line,
                                                      std::string_view hovered_map,
                                                      float elapsed_seconds, float pixels_per_unit);

// L'ÉTAPE EN COURS D'UN VOL, de `from` à `to` : la trace laissée derrière
// l'appareil, de plus en plus nette vers lui, puis l'appareil, le nez vers
// l'escale suivante. L'appareil disparaît quand l'étape s'achève.
struct WorldMapFlight {
  std::vector<WorldMapQuad> trail;
  std::optional<WorldMapQuad> airship;
};
[[nodiscard]] WorldMapFlight FlyWorldMapLeg(ImVec2 from, ImVec2 to, float progress);

// LA PLACE DU WARP AGENT, en pixels d'écran, pour une ville dont `town` est le
// rectangle à l'écran : les pieds de la figure sur le coin haut droit de la
// ville — sans monter plus haut que six pixels plus sa hauteur sous le haut de
// l'image, `image_top` —, et son nom à droite des pieds, le milieu de la ligne
// à mi-hauteur de la figure. `sprite_corner` est le coin haut gauche de
// l'image ; `label_at` le haut gauche du texte, haut de `font_pixels`.
struct WorldMapWarpAgentPlace {
  ImVec2 sprite_corner{0.0F, 0.0F};
  ImVec2 label_at{0.0F, 0.0F};
};
[[nodiscard]] WorldMapWarpAgentPlace PlaceWorldMapWarpAgent(const WorldMapWarpAgentView& agent,
                                                            const WorldMapRect& town,
                                                            float image_top, float font_pixels);

}  // namespace ro::ui
