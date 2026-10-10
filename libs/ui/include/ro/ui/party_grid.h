#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/align_grid.h"
#include "ro/ui/hud_frame.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/status_bar.h"
#include "ro/ui/status_cell.h"

namespace ro::ui {

// ── LA GRILLE DE GROUPE ─────────────────────────────────────────────────────
//
// Le groupe en tuiles, à la manière des « raid frames » : la barre de vie EST le
// fond de la tuile, et sa couleur dit l'état du membre d'un coup d'œil.
//
// Le cadre est un HUD libre (`hud_frame.h`), mais sa taille se CALCULE depuis la
// tuile : colonnes, largeur, hauteur et écart commandent. Tirer le cadre par un
// bord change le nombre de colonnes.

// Comment les PV s'écrivent sous le nom. ⚠ PERSISTÉ PAR SON RANG : ne pas
// intercaler.
enum class PartyGridHpText : int {
  kNone = 0,
  kNumbers = 1,
  kPercent = 2,
  kBoth = 3,
};

// Les bornes des réglages, celles des curseurs de la fenêtre d'options. Le
// dessin les applique aussi : la valeur vient d'un fichier que le joueur peut
// écrire à la main.
inline constexpr int kPartyGridFewestColumns = 1;
inline constexpr int kPartyGridMostColumns = 6;
inline constexpr int kPartyGridNarrowestTilePixels = 60;
inline constexpr int kPartyGridWidestTilePixels = 400;
inline constexpr int kPartyGridShortestTilePixels = 18;
inline constexpr int kPartyGridTallestTilePixels = 80;
inline constexpr int kPartyGridWidestGapPixels = 12;
inline constexpr int kPartyGridSmallestTextPixels = 8;
inline constexpr int kPartyGridLargestTextPixels = 28;
inline constexpr int kPartyGridThinnestSpBarPixels = 2;
inline constexpr int kPartyGridThickestSpBarPixels = 14;
inline constexpr int kPartyGridSmallestStatusPixels = 6;
inline constexpr int kPartyGridLargestStatusPixels = 28;
inline constexpr int kPartyGridFewestStatusIcons = 1;
inline constexpr int kPartyGridMostStatusIcons = 24;
inline constexpr int kPartyGridMostStatusRows = 4;
inline constexpr int kPartyGridLowestMidPercent = 20;
inline constexpr int kPartyGridHighestMidPercent = 90;
inline constexpr int kPartyGridLowestLowPercent = 5;
inline constexpr int kPartyGridHighestLowPercent = 50;

// TOUT CE QUI SE PERSISTE, avec ses défauts.
struct PartyGridLook {
  bool shown = false;
  bool locked = false;

  HudRect rect{.x = 40.0F, .y = 200.0F, .width = 190.0F, .height = 120.0F};

  int columns = 1;
  int tile_width_px = 180;
  int tile_height_px = 34;
  int gap_px = 2;

  bool show_self = true;
  bool show_offline = true;
  bool show_job_icon = true;
  bool show_level = false;
  int hp_text = static_cast<int>(PartyGridHpText::kNumbers);
  // Indépendante de la police des fenêtres : une grille se lit en périphérie.
  int text_px = 13;

  // Le sort de soutien armé part sur la tuile survolée ; voir
  // `PartyGridAnswer::cast_account_id`.
  bool cast_on_tile = true;
  // ⚠ CLIQUABLE, LE CADRE PREND LA SOURIS AU JEU sur toute sa surface : d'où le
  // défaut à faux.
  bool clickable = false;
  bool show_sp = true;
  bool show_tooltip = false;
  int sp_bar_px = 6;

  bool show_statuses = true;
  int status_px = 14;
  int status_most = 8;
  int status_rows = 1;
  bool status_countdown = true;
  int status_sweep = static_cast<int>(StatusSweep::kRadial);
  // L'OPACITÉ DES ICÔNES, en pour cent, et le TEXTE DU TEMPS RESTANT : les
  // réglages de la barre d'états du joueur. Taille automatique, le texte fait
  // la moitié de l'icône, avec un plancher ; sinon, la taille réglée.
  int status_opacity_percent = kFullStatusOpacityPercent;
  bool status_countdown_auto = true;
  int status_countdown_px = kDefaultStatusCountdownPixels;
  ImU32 status_countdown_background = kDefaultStatusCountdownBackground;
  ImU32 status_icon_background = kDefaultStatusIconBackground;
  ImU32 status_sweep_color = kDefaultStatusSweepColor;

  ImU32 frame_background = IM_COL32(13, 13, 18, 184);
  ImU32 tile_background = IM_COL32(23, 23, 28, 230);
  ImU32 hp_high = IM_COL32(64, 199, 71, 255);
  ImU32 hp_mid = IM_COL32(214, 191, 61, 255);
  ImU32 hp_low = IM_COL32(207, 66, 56, 255);
  ImU32 sp = IM_COL32(69, 130, 219, 255);
  ImU32 text = IM_COL32(240, 240, 240, 255);
  ImU32 self_border = IM_COL32(255, 217, 120, 219);
  // Deux absences distinctes : déconnecté (gris terne), et en jeu mais trop loin
  // pour qu'on connaisse ses PV (bleu pâle).
  ImU32 offline_text = IM_COL32(115, 115, 122, 255);
  ImU32 far_text = IM_COL32(148, 184, 230, 255);

  int hp_mid_percent = 55;
  int hp_low_percent = 25;
};

// UNE TUILE, composée par `app`. ⚠ LES CHAÎNES APPARTIENNENT À L'APPELANT et
// doivent vivre jusqu'à la fin du dessin.
struct PartyGridTileView {
  std::uint32_t account_id = 0;
  std::string_view name;
  // Déjà traduit ; pour l'infobulle.
  std::string_view job_label;
  // Le nom affiché de la carte, vide s'il n'est pas connu ; pour l'infobulle.
  std::string_view map_label;
  SkinImage job_icon;
  int base_level = 0;

  bool is_me = false;
  bool is_leader = false;
  bool is_target = false;
  bool offline = false;
  // Un acteur le représente à l'écran : il se cible, et son clic droit est le
  // menu d'entité.
  bool in_sight = false;
  // Sur la même carte que moi : le serveur n'accepte de nommer chef que là.
  bool same_map = false;

  bool hp_known = false;
  int hp = 0;
  int max_hp = 0;
  // ⚠ FAUX QUAND LA RÉPONSE EST PÉRIMÉE : une barre figée vaut moins que rien.
  bool sp_known = false;
  int sp = 0;
  int max_sp = 0;

  // Faux quand on ne sait rien de ses états — ce qui n'est pas « aucun état ».
  // ⚠ L'APERÇU DES FAUX ÉTATS est composé par `app`, qui pose alors ce drapeau.
  bool statuses_known = false;
  std::span<const StatusCellView> statuses;
};

struct PartyGridView {
  // ⚠ MOI COMPRIS, même hors groupe : la grille sert aussi de HUD de vitals.
  // `show_self` et `show_offline` filtrent ici, pas chez l'appelant.
  std::span<const PartyGridTileView> tiles;
  bool i_am_leader = false;
};

// Ce qui dure d'une image à l'autre sans se persister.
struct PartyGridState {
  // Le membre du menu ouvert, FIGÉ AU CLIC : il peut se déconnecter pendant que
  // le menu est déplié, et les entrées ne changent pas sous le curseur.
  std::uint32_t menu_account_id = 0;
  std::string menu_name;
  bool menu_is_me = false;
  bool menu_offline = false;
  bool menu_same_map = false;
  bool open_menu = false;
};

enum class PartyGridVerb : std::uint8_t {
  kNone = 0,
  // Le menu d'entité de `app`, pour un membre qu'un acteur représente.
  kEntityMenu,
  kWhisper,
  // La conversation 1:1 à part.
  kWhisperWindow,
  kMakeLeader,
  kKick,
};

struct PartyGridAnswer {
  // La géométrie ou le nombre de colonnes ont changé : à persister.
  bool moved = false;
  // Le compte sous le curseur, zéro pour aucun.
  std::uint32_t hovered_account_id = 0;
  // Le compte qu'un sort de SOUTIEN armé doit viser, zéro pour aucun. ⚠ JAMAIS
  // POUR UN SORT OFFENSIF NI POUR UN SORT AU SOL : une tuile est survolée en
  // permanence pendant qu'on soigne, et un offensif mal armé partirait sur un
  // coéquipier. C'est à `app` de ne le lire que pour le soutien.
  std::uint32_t cast_account_id = 0;
  // Clic gauche : cibler ce compte.
  std::uint32_t target_account_id = 0;
  PartyGridVerb verb = PartyGridVerb::kNone;
  std::uint32_t verb_account_id = 0;
  std::string verb_name;
};

// ── LA GÉOMÉTRIE, SÉPARÉE DU DESSIN ────────────────────────────────────────

struct PartyGridLayout {
  int columns = 1;
  int rows = 0;
  float tile_width = 0.0F;
  float tile_height = 0.0F;
  float gap = 0.0F;
  float padding = 0.0F;
  float frame_width = 0.0F;
  float frame_height = 0.0F;
};

// La grille de `shown_count` tuiles, à l'échelle d'interface `scale`. Le nombre
// de colonnes ne dépasse jamais celui des tuiles.
[[nodiscard]] PartyGridLayout PartyGridLayoutOf(const PartyGridLook& look, std::size_t shown_count,
                                                float scale);

// Le coin haut-gauche de la tuile `rank`, rangée par rangée.
[[nodiscard]] ImVec2 PartyGridTileAt(const PartyGridLayout& layout, ImVec2 frame_top_left,
                                     std::size_t rank);

// Le nombre de colonnes qu'une largeur de cadre tirée à la main représente.
[[nodiscard]] int PartyGridColumnsForWidth(const PartyGridLayout& layout, float frame_width);

// La teinte de la jauge : haute, moyenne, puis basse sous chaque seuil.
[[nodiscard]] ImU32 PartyGridHpInk(const PartyGridLook& look, int hp, int max_hp);

// Le pourcentage affiché, arrondi vers le haut tant qu'il reste un point de vie :
// « 0 % » sur un vivant ferait renoncer à le soigner.
[[nodiscard]] int PartyGridHpPercent(int hp, int max_hp);

// La seconde ligne d'une tuile dont les PV sont connus.
[[nodiscard]] std::string PartyGridHpCaption(PartyGridHpText mode, int hp, int max_hp);

// Le mode lu d'un entier venu du fichier, borné.
[[nodiscard]] PartyGridHpText PartyGridHpTextOf(int stored) noexcept;

// La grille entière. Ne dessine rien quand `look.shown` est faux.
PartyGridAnswer DrawPartyGrid(const PartyGridView& view, PartyGridLook& look, PartyGridState& state,
                              const AlignGrid& grid, HudGrip& grip);

}  // namespace ro::ui
