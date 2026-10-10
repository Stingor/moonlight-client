#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA FENÊTRE DES DESTINATIONS : Teleport à partir du niveau 2, Warp Portal,
// Odin's Recall. Le serveur liste les lieux, le joueur en choisit un.
//
// C'est la fenêtre du client d'origine, reproduite à ses mesures : un cadre en
// neuf morceaux sans barre de titre, une ligne de titre, une liste blanche de
// lignes de seize pixels, et deux boutons en bas à droite.
//
// 🔴 ELLE NE DÉCIDE DE RIEN. Les lignes arrivent déjà écrites ; elle rend le
// RANG de la ligne choisie, et c'est `app` qui retrouve le nom à renvoyer.
//
// ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : c'est la liste du serveur qui
// l'ouvre, et l'état recopie la question du monde, comme le sertissage.

inline constexpr const char* kWarpListWindowId = "destinations";

// ── LES MESURES DU CLIENT, en pixels d'interface ────────────────────────────
inline constexpr ImVec2 kWarpListSizePixels = ImVec2(280.0F, 120.0F);
// La ligne de titre.
inline constexpr ImVec2 kWarpListTitlePixels = ImVec2(12.0F, 9.0F);
// La liste : sa place, et ce qu'elle laisse au reste de la fenêtre — sa largeur
// est celle de la fenêtre moins 24, sa hauteur celle de la fenêtre moins 55.
inline constexpr ImVec2 kWarpListRowsPixels = ImVec2(12.0F, 26.0F);
inline constexpr ImVec2 kWarpListRowsMarginPixels = ImVec2(24.0F, 55.0F);
// Une ligne, et la bande de sélection, d'un pixel plus basse que la ligne.
inline constexpr float kWarpListRowPixels = 16.0F;
inline constexpr float kWarpListSelectionPixels = 15.0F;
// Le texte d'une ligne commence à trois pixels du bord de la liste.
inline constexpr float kWarpListTextInsetPixels = 3.0F;
// Les deux boutons, de la taille des bitmaps du client, comptés depuis le coin
// bas-droit de la fenêtre : « OK » 92 pixels à gauche du bord, « Annuler » 46,
// tous deux 24 pixels au-dessus du bas.
inline constexpr ImVec2 kWarpListButtonPixels = ImVec2(42.0F, 20.0F);
inline constexpr float kWarpListOkFromRightPixels = 92.0F;
inline constexpr float kWarpListCancelFromRightPixels = 46.0F;
inline constexpr float kWarpListButtonsFromBottomPixels = 24.0F;
// Où elle naît : 120 pixels à gauche du milieu de l'écran, et aux cinq
// huitièmes de sa hauteur.
inline constexpr float kWarpListSpawnLeftOfCenterPixels = 120.0F;
inline constexpr float kWarpListSpawnHeightShare = 0.625F;

// CE QU'UN CANAL DEVIENT À L'ÉCRAN DU CLIENT. Il peint ses fenêtres sur une
// surface de seize bits : un canal n'y garde que ses cinq bits de poids fort,
// et l'affichage les étend en recopiant les trois plus hauts dans les trois
// plus bas. Le gris 240 qu'il demande pour la liste s'y montre 247.
inline constexpr unsigned kFiveBitKeptMask = 0xF8U;
inline constexpr unsigned kFiveBitRefillShift = 5U;
[[nodiscard]] constexpr unsigned ShownOnFiveBits(unsigned channel) noexcept {
  return (channel & kFiveBitKeptMask) | (channel >> kFiveBitRefillShift);
}

// LES COULEURS DE LA LISTE : un fond gris très clair — RGB(240, 240, 240)
// demandé —, la ligne choisie en bleu pâle — RGB(202, 223, 251), la case de la
// palette du client —, toutes deux telles que son écran de seize bits les
// montre. Le texte, titre et lignes, est noir.
inline constexpr ImU32 kWarpListRowsColor =
    IM_COL32(ShownOnFiveBits(240), ShownOnFiveBits(240), ShownOnFiveBits(240), 255);
inline constexpr ImU32 kWarpListSelectionColor =
    IM_COL32(ShownOnFiveBits(202), ShownOnFiveBits(223), ShownOnFiveBits(251), 255);
inline constexpr ImU32 kWarpListTextColor = IM_COL32(0, 0, 0, 255);

// Aucune ligne choisie : un clic sous la dernière ligne désélectionne, et « OK »
// ne fait alors rien.
inline constexpr int kWarpListNoRow = -1;

// ── LA BARRE DE DÉFILEMENT, en pixels d'interface ───────────────────────────
//
// 🔴 LA LISTE NE GRANDIT PAS : elle montre autant de lignes ENTIÈRES que sa
// hauteur en contient — quatre —, et les suivantes se déroulent. La barre
// n'apparaît que lorsque les lignes dépassent ; sinon, rien n'est peint à sa
// place et le clic y choisit une ligne comme ailleurs.
//
// Elle se tient contre le bord droit de la liste, sur toute sa hauteur, et se
// peint PAR-DESSUS les lignes : la bande de sélection passe dessous.
inline constexpr int kWarpListBarPixels = 13;
// Une flèche, en haut et en bas : c'est aussi ce que la piste laisse à chaque
// bout. La flèche du haut se pose un pixel sous le bord, celle du bas un pixel
// au-dessus de la place qui lui revient.
inline constexpr int kWarpListArrowPixels = 13;
inline constexpr int kWarpListArrowInsetPixels = 1;
// La piste se peint par carreaux de sa hauteur, la poignée par carreaux de
// quatre pixels : un bout, le milieu répété, l'autre bout.
inline constexpr int kWarpListTrackTilePixels = 13;
inline constexpr int kWarpListGrabTilePixels = 4;
// La poignée ne descend pas sous huit pixels, et jamais sous deux.
inline constexpr int kWarpListGrabLeastPixels = 8;
inline constexpr int kWarpListGrabFloorPixels = 2;

// Ce qu'un clic sur la barre désigne, de haut en bas.
enum class WarpListBarZone : std::uint8_t {
  kUpArrow,
  kPageUp,
  kGrab,
  kPageDown,
  kDownArrow,
};

// La poignée, comptée depuis le haut de la barre.
struct WarpListGrab {
  int top = 0;
  int length = 0;
};

// CE QUE LA FENÊTRE SAIT DU MONDE, recomposé par l'appelant à chaque image.
struct WarpListView {
  // La ligne au-dessus de la liste.
  std::string_view title;
  // TOUTES les lignes, l'annulation comprise en dernier.
  std::span<const std::string> rows;
  // Le rang de la liste montrée : une liste nouvelle remet la sélection sur la
  // première ligne et reprend le premier plan.
  std::uint64_t offer_seq = 0;
};

enum class WarpListVerb : std::uint8_t {
  kNothing,
  // Partir vers la ligne `row` — y compris la ligne d'annulation.
  kChoose,
  // Le bouton « Annuler », ou Échap.
  kCancel,
};

struct WarpListWindowAction {
  WarpListVerb verb = WarpListVerb::kNothing;
  int row = kWarpListNoRow;
  // Échap a servi ici : rien derrière ne doit se fermer sur la même touche.
  bool escape_spent = false;
};

struct WarpListWindowState {
  // ⚠ CE N'EST PAS LE JOUEUR QUI LE POSE : il recopie la question du monde.
  bool open = false;
  // La ligne choisie. ⚠ LA PREMIÈRE À L'OUVERTURE, comme chez le client.
  int selected = 0;
  // La première ligne montrée : la liste défile par lignes entières.
  int first_row = 0;
  std::uint64_t seen_offer_seq = 0;
  // Le glisser de la poignée : où il a commencé, en pixels d'interface depuis
  // le haut de la barre, et la première ligne d'alors.
  bool dragging_grab = false;
  int drag_start_pixels = 0;
  int drag_start_first_row = 0;
};

// LA LIGNE SOUS UN POINT DE LA LISTE, ou `kWarpListNoRow` au-delà de la
// dernière. `y_in_rows` se compte depuis le haut du contenu, défilement compris.
[[nodiscard]] int WarpListRowAt(float y_in_rows, float row_height, int row_count);

// Le nombre de lignes ENTIÈRES qu'une liste de cette hauteur montre.
[[nodiscard]] int WarpListVisibleRows(int list_height_pixels);

// LA PREMIÈRE LIGNE APRÈS UN DÉPLACEMENT de `delta` lignes : jamais avant la
// première, jamais au point de laisser du vide sous la dernière.
[[nodiscard]] int WarpListScrolled(int first_row, int delta, int row_count, int visible_rows);

// LA POIGNÉE d'une barre de cette hauteur. Sa longueur est la part des lignes
// montrées, sa place celle de la première ligne dans la course.
//
// ⚠ Sans ligne cachée, il n'y a pas de barre : la longueur est nulle.
[[nodiscard]] WarpListGrab WarpListGrabOf(int bar_height_pixels, int row_count, int visible_rows,
                                          int first_row);

// Ce qu'un clic à `y_pixels` du haut de la barre désigne.
[[nodiscard]] WarpListBarZone WarpListBarZoneAt(int y_pixels, int bar_height_pixels,
                                                const WarpListGrab& grab);

// LA PREMIÈRE LIGNE PENDANT UN GLISSER DE LA POIGNÉE, `moved_pixels` plus bas
// que là où il a commencé. La course de la poignée couvre une ligne de plus que
// ce qui se déroule, et la division tronque vers zéro.
[[nodiscard]] int WarpListDraggedFirstRow(int start_first_row, int moved_pixels,
                                          int bar_height_pixels, int row_count, int visible_rows);

// LA SÉLECTION APRÈS UNE FLÈCHE. Vers le haut, elle ne passe pas sous la
// première ligne ; vers le bas, pas au-delà de la dernière. Sans ligne choisie,
// les deux flèches reprennent à la première.
[[nodiscard]] int WarpListStep(int selected, int delta, int row_count);

// Où la fenêtre naît, pour un écran de cette taille et cette échelle.
[[nodiscard]] ImVec2 WarpListSpawn(ImVec2 screen, float scale);

[[nodiscard]] WarpListWindowAction DrawWarpListWindow(WarpListWindowState& state,
                                                      const WarpListView& view, const SkinArt& art);

}  // namespace ro::ui
