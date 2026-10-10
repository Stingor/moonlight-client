#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LE PANNEAU DE SUIVI ─────────────────────────────────────────────────────
//
// Sans fond ni cadre par défaut : seul le texte se voit, sur le jeu. Il porte
// des SECTIONS — les quêtes, les succès —, chacune repliable sous son en-tête,
// et chaque entrée repliable sous son titre. Un clic droit ouvre ses réglages.
//
// Le même moteur dessine un panneau commun ou deux panneaux séparés : `app`
// décide quelles sections vont dans quel panneau, et donne à chacun son
// identité et sa place.
//
// Sa hauteur suit ses entrées : un panneau de hauteur fixe coupait les
// dernières, et un fond posé sur lui couvrirait du vide.

// LE GENRE D'UNE SECTION : il dit à `app` à quel étage renvoyer une action.
enum class TrackerKind : std::uint8_t {
  kQuest = 0,
  kAchievement = 1,
};
inline constexpr std::size_t kTrackerKindCount = 2;

// Les identités des deux panneaux : le commun (ou celui des quêtes une fois
// séparés), puis celui des succès séparés. Non traduites.
inline constexpr const char* kTrackerPanelId = "tracker";
inline constexpr const char* kTrackerSecondPanelId = "tracker_achievements";

enum class TrackerBackground : std::uint8_t {
  kNone = 0,
  kLight = 1,
  kDark = 2,
};
inline constexpr std::size_t kTrackerBackgroundCount = 3;

// LES RÉGLAGES DU SUIVI, communs aux deux panneaux.
struct TrackerLook {
  TrackerBackground background = TrackerBackground::kNone;
  // Le panneau ne se traîne plus.
  bool locked = false;
  // Les clics le traversent et vont au jeu. ⚠ SON MENU NE S'OUVRE PLUS : il ne
  // se règle alors que depuis la puce de la fenêtre des quêtes ou des succès.
  bool click_through = false;
  // Les succès dans leur propre panneau.
  bool split = false;
  // Le plus de quêtes montrées ; au-delà, « +N autres ».
  std::int32_t most_quests = 10;
  // Une quête reçue se suit d'elle-même.
  bool track_new_quests = true;
  // Les quêtes qui se jouent sur la carte courante d'abord.
  bool current_map_first = true;

  [[nodiscard]] bool operator==(const TrackerLook&) const = default;
};
inline constexpr std::int32_t kTrackerFewestQuests = 1;
inline constexpr std::int32_t kTrackerMostQuests = 30;

// Le coin haut-gauche d'un panneau, en pixels d'écran.
struct TrackerPlace {
  float x = 0.0F;
  float y = 0.0F;

  [[nodiscard]] bool operator==(const TrackerPlace&) const = default;
};

// UNE LIGNE SOUS UN TITRE : un objectif, une chasse.
struct TrackerLine {
  std::string_view text;
  // Atteint : en vert.
  bool reached = false;
  // Une barre fine sous le texte, de 0 à 1 ; négative : pas de barre.
  float progress = -1.0F;
};

// UNE ENTRÉE : un succès suivi, une quête suivie.
//
// ⚠ DES VUES dans les tampons de `app`, valables le temps de l'image.
struct TrackerEntry {
  std::uint32_t key = 0;
  std::string_view name;
  // À droite du titre, en petit : le temps qui reste. Vide : rien.
  std::string_view badge;
  // La couleur du badge : orange s'il presse, rouge s'il est échu.
  bool badge_urgent = false;
  bool badge_expired = false;
  // Fait : le titre en vert. Une quête aux objectifs remplis l'est aussi.
  bool completed = false;
  // Un compte vient d'avancer : l'entrée s'éclaire un instant. `app` le pose
  // pendant la durée de l'éclat ; `flash` dit où en est l'éclat, de 1 à 0.
  float flash = 0.0F;
  std::span<const TrackerLine> lines;
  // Un lieu où guider : la flèche « aller » s'affiche.
  bool can_guide = false;
};

struct TrackerSection {
  TrackerKind kind = TrackerKind::kQuest;
  // L'en-tête, déjà traduit : « Quêtes (3) ». Vide : pas d'en-tête (une seule
  // section dans le panneau).
  std::string_view label;
  std::span<const TrackerEntry> entries;
  // « +2 autres », déjà composé ; vide : rien de caché.
  std::string_view more;
};

// L'ÉTAT D'INTERFACE D'UN PANNEAU.
struct TrackerPanelState {
  // Le panneau se montre.
  bool shown = false;
  // Les entrées DÉPLIÉES, par genre et clé. ⚠ UNE ENTRÉE AJOUTÉE EST REPLIÉE,
  // sauf si `app` la déplie.
  std::vector<std::pair<TrackerKind, std::uint32_t>> expanded;
  // Les sections repliées.
  std::vector<TrackerKind> folded_sections;
  // UNE PLACE À PRENDRE, posée par `app` — celle du disque — et consommée par
  // le dessin. Ramenée dans l'écran si elle en sort.
  std::optional<TrackerPlace> move_to;
  // Revenir à la place d'origine, à l'image suivante.
  bool place_reset = false;
  // Là où le panneau s'est dessiné : un écart, souris lâchée, est un
  // déplacement à retenir.
  std::optional<TrackerPlace> drawn_at;
};

struct TrackerTarget {
  TrackerKind kind = TrackerKind::kQuest;
  std::uint32_t key = 0;

  [[nodiscard]] bool operator==(const TrackerTarget&) const = default;
};

struct TrackerPanelAction {
  // Ouvrir la fenêtre sur cette entrée (clic sur la flèche « ▶ »).
  std::optional<TrackerTarget> open;
  // Guider vers le lieu de cette entrée.
  std::optional<TrackerTarget> guide;
  // Ne plus suivre cette entrée (menu de l'entrée).
  std::optional<TrackerTarget> untrack;
  std::optional<TrackerLook> look;
  // Le joueur l'a traîné ici.
  std::optional<TrackerPlace> moved;
  bool reset_place = false;
};

// UN PANNEAU. `window_id` est sa clé ImGui ; `first_top` décale sa place
// d'origine vers le bas, pour que deux panneaux séparés ne naissent pas l'un
// sur l'autre. Rien n'est dessiné sans entrée.
[[nodiscard]] TrackerPanelAction DrawTrackerPanel(const char* window_id, TrackerPanelState& state,
                                                  std::span<const TrackerSection> sections,
                                                  const TrackerLook& look, float first_top,
                                                  const SkinArt& skin);

// La hauteur du panneau pour ces sections, en pixels d'art.
[[nodiscard]] float TrackerPanelHeight(const TrackerPanelState& state,
                                       std::span<const TrackerSection> sections);

// LES RÉGLAGES DU SUIVI, en éléments de menu : le panneau les montre sous son
// clic droit, les fenêtres des quêtes et des succès sous leur puce. Rend le
// réglage changé, ou rien.
struct TrackerSettingsAnswer {
  std::optional<TrackerLook> look;
  bool reset_place = false;
};
[[nodiscard]] TrackerSettingsAnswer DrawTrackerSettingsItems(const TrackerLook& look,
                                                             const SkinArt& skin);

}  // namespace ro::ui
