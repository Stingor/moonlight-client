#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/hud_frame.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/status_cell.h"

namespace ro::ui {

// ── LE HUD DE CIBLE ─────────────────────────────────────────────────────────
//
// Qui est l'entité que le joueur a désignée, et dans quel état elle est : six
// cadres libres — portrait, nom, PV, SP, espèce, états — que le joueur pose
// lui-même. Un verrou commun les fige et les rend transparents aux clics, et
// l'aimantation les aligne entre eux. Les états en font partie : tout ce qui
// parle de la cible se place et se verrouille ensemble.

// Les six cadres. ⚠ L'ORDRE PART DANS LES RÉGLAGES ET DANS LE FICHIER.
enum class TargetPart : std::uint8_t {
  kPortrait = 0,
  kName = 1,
  kHealth = 2,
  kMagic = 3,
  kKind = 4,
  kStatuses = 5,
};
inline constexpr std::size_t kTargetPartCount = 6;

// Ce que le texte d'une jauge dit. ⚠ MÊME RÈGLE D'ORDRE.
enum class GaugeText : std::uint8_t {
  kNothing = 0,
  kPercent = 1,
  kValues = 2,
  kBoth = 3,
};
// Relu d'un fichier : une valeur hors bornes retombe sur les valeurs chiffrées.
[[nodiscard]] GaugeText GaugeTextOf(int stored) noexcept;

// Un cadre : ce que le joueur en a fait. Pour les deux jauges, `foreground` est
// le remplissage ; le texte y reste blanc.
struct TargetPartLook {
  bool shown = true;
  HudRect rect;
  ImU32 background = IM_COL32(13, 13, 18, 199);
  ImU32 foreground = IM_COL32_WHITE;
};

// La disposition d'origine, que « Réinitialiser la disposition » repose.
[[nodiscard]] HudRect DefaultTargetPartRect(TargetPart part);
[[nodiscard]] std::array<TargetPartLook, kTargetPartCount> DefaultTargetParts();

// Les bornes de la taille du texte, appliquées au dessin : une valeur relue d'un
// fichier édité à la main ne doit donner ni une police géante ni une nulle.
inline constexpr float kTargetTextScaleLeast = 0.5F;
inline constexpr float kTargetTextScaleMost = 3.0F;

struct TargetHudLook {
  bool locked = false;
  // Garder les cadres à l'écran sans cible, le temps de les poser.
  bool layout_mode = false;
  bool border = false;
  bool sticky = true;
  float rounding = 4.0F;
  // × la taille de la police de l'interface.
  float text_scale = 1.0F;
  // Un `GaugeText`, gardé en entier pour le fichier : voir `GaugeTextOf`.
  int gauge_text = static_cast<int>(GaugeText::kValues);
  std::array<TargetPartLook, kTargetPartCount> parts = DefaultTargetParts();

  // Le contenu du cadre des états, qui n'a d'équivalent dans aucun autre cadre.
  // Le rangement et l'écoulement sont gardés en entiers, comme ceux de la barre
  // d'états.
  int status_icon_pixels = 24;
  int status_most = 20;
  int status_gap_pixels = 2;
  int status_rows = 1;
  int status_sort = static_cast<int>(StatusSort::kEndingSoon);
  bool status_countdown = true;
  int status_countdown_pixels = 11;
  int status_sweep = static_cast<int>(StatusSweep::kRadial);
  ImU32 status_sweep_color = IM_COL32(0, 0, 0, 140);
  ImU32 status_icon_background = kDefaultStatusIconBackground;
};

// La cible, en lecture seule. `ui` ne connaît pas `sim` : c'est `app` qui croise
// la désignation, le dossier du serveur et l'acteur.
struct TargetView {
  // Faux quand rien n'est désigné. Revalidé à chaque image par l'appelant.
  bool present = false;
  // Vide tant que le nom n'est pas connu : le cadre écrit alors « (nom inconnu) ».
  std::string_view name;
  // La ligne sociale d'un joueur : sa guilde, et rien d'autre.
  std::string_view guild;
  std::optional<std::int16_t> level;

  // Un PNJ confirmé par le serveur : ses jauges ne mesurent rien.
  bool is_npc = false;
  // Un joueur : son espèce ne distingue personne, et ses PV ne se chiffrent que
  // pour un allié.
  bool is_player = false;
  // Le serveur ne livre le SP qu'à soi, son groupe et sa guilde : sa présence
  // est l'autorisation, elle ne se refait pas ici.
  bool ally = false;

  std::optional<std::uint32_t> health;
  std::optional<std::uint32_t> max_health;
  std::optional<std::uint32_t> magic;
  std::optional<std::uint32_t> max_magic;

  // « Demi-Human · Neutral 1 · Medium », composé par `app`.
  std::string_view species;

  // Le portrait, déjà rendu. Inutilisable pour une entité dessinée en modèle 3D,
  // que `model` signale.
  SkinImage portrait;
  bool model = false;

  std::span<const StatusCellView> statuses;

  // Les cadres agissent comme l'entité cette image : ils reprennent la souris.
  bool clickable = false;
};

// Ce que les cadres ont reçu cette image.
struct TargetHudAnswer {
  // Un cadre a bougé : l'appelant persiste, une fois.
  bool moved = false;
  // Le curseur est sur un cadre qui agit.
  bool hovered = false;
  bool left_click = false;
  bool right_click = false;
};

// Le texte d'une jauge, dans le mode demandé. Vide quand il n'y a rien à écrire.
//
// `numbers_allowed` faux ne veut pas dire « rien » : la jauge reste, seuls les
// chiffres tombent.
[[nodiscard]] std::string GaugeCaption(bool known, std::uint32_t current, std::uint32_t most,
                                       GaugeText mode, bool numbers_allowed, bool is_magic);

// La part remplie d'une jauge, entre 0 et 1. Un monstre n'a pas de réserve de
// SP : le serveur n'écrit jamais moins de 1, et la barre reste alors vide.
[[nodiscard]] float GaugeFill(bool known, std::uint32_t current, std::uint32_t most, bool is_magic);

// Les chiffres de PV se taisent devant un joueur qui n'est pas un allié, dès
// qu'ils sont connus.
[[nodiscard]] bool GaugeNumbersAllowed(const TargetView& target, bool is_magic, bool known);

// Ce cadre se dessine-t-il pour cette cible ?
[[nodiscard]] bool TargetPartApplies(TargetPart part, const TargetView& target);

// Vrai tant que le joueur tient l'un des cadres de ce HUD.
[[nodiscard]] bool TargetHudHeld(const HudGrip& grip);

// Dessine le HUD. `look` est modifié quand le joueur déplace un cadre.
void DrawTargetHud(const TargetView& target, const AlignGrid& grid, TargetHudLook& look,
                   HudGrip& grip, TargetHudAnswer& answer);

}  // namespace ro::ui
