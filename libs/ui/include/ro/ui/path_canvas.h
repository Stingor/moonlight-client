#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <imgui.h>

#include "ro/ui/damage_texts.h"
#include "ro/ui/entity_sample.h"
#include "ro/ui/options_window.h"

namespace ro::ui {

// ── LE CADRE DES TRAJECTOIRES ───────────────────────────────────────────────
//
// Une cible, des nombres qui en sortent sans cesse, et les ANCRES de la
// trajectoire de leur type — départ, arrivée, poignées, dispersion — qu'on tire à la
// souris. On règle un chemin en le regardant, au lieu de deviner ce que
// « −40 px » donnera.
//
// ⚠ `ui` NE SAIT RIEN DES DÉGÂTS : elle reçoit des quads et des textes déjà
// composés, posés en pixels de sprite autour des pieds de la cible, et des
// ancres qu'elle rend déplacées. C'est `app` qui sait qu'une ancre est un
// départ, et ce que son déplacement change.

// UN QUAD DE SPRITE, en pixels de sprite depuis les pieds de la cible, y
// DESCENDANT — la forme d'un quad du jeu.
struct PathSampleQuad {
  ImTextureID texture = 0;
  // Haut-gauche, haut-droit, bas-droit, bas-gauche.
  std::array<ImVec2, 4> corners{};
  // Le miroir échange les abscisses de texture.
  bool mirrored = false;
  ImU32 tint = IM_COL32_WHITE;
};

// CE QUE LE CADRE MONTRE, composé par `app` à chaque image.
struct PathSample {
  // La cible, en vignette à l'échelle du jeu.
  EntitySample target;
  // Pixels d'écran par pixel de sprite : ceux du jeu au zoom courant, pour que
  // nombres et cible gardent leur rapport de taille.
  float sprite_scale = 1.0F;
  // Les nombres en sprite, du plus ancien au plus récent.
  std::vector<PathSampleQuad> quads;
  // Et ceux écrits en police : milieu, hauteur, écart et cerne en PIXELS DE
  // SPRITE depuis les pieds, y descendant. La police les écrit par-dessus les
  // quads.
  std::vector<DamageTextView> texts;
  ImFont* font = nullptr;
  // OÙ UN NOMBRE DE CE TYPE NAÎT CHEZ LE CLIENT, avant tout décalage — le milieu
  // de son dessin à sa naissance —, en pixels de sprite depuis les pieds, y
  // MONTANT. C'est l'origine des ancres.
  ImVec2 birth{0.0F, 0.0F};
  // LE CHEMIN QUE L'ALLURE SEULE FAIT FAIRE AU NOMBRE, dérive comprise, depuis
  // sa naissance : `kAllureSamples` points à parts égales de sa vie, y montant.
  // Vide sans nombre. La trajectoire S'AJOUTE à ce chemin — c'est lui qui fait
  // tomber le tracé et les ancres là où les nombres passent vraiment.
  std::vector<ImVec2> allure;
};

// Le nombre de points du chemin de l'allure.
inline constexpr std::size_t kAllureSamples = 33;

// Le chemin de l'allure à cette part de la vie, entre deux points relevés ;
// zéro sans relevé.
[[nodiscard]] ImVec2 AllureAt(const PathSample& sample, float progress) noexcept;

// CE QU'UNE ANCRE DÉPLACE, pour son dessin.
enum class PathAnchorKind : std::uint8_t {
  kStart,
  kArrival,
  kBend,
  kRadius,
};

// UNE ANCRE. Tout en pixels de sprite depuis la NAISSANCE, y MONTANT : où elle
// est, dans quelle boîte on peut la tirer, et où le clic droit la remet.
//
// ⚠ UNE BORNE ÉGALE SUR UN AXE LE VERROUILLE : un rayon ne se tire que sur son
// axe.
struct PathAnchor {
  int id = -1;
  PathAnchorKind kind = PathAnchorKind::kStart;
  ImVec2 at{0.0F, 0.0F};
  ImVec2 lowest{0.0F, 0.0F};
  ImVec2 highest{0.0F, 0.0F};
  ImVec2 rest{0.0F, 0.0F};
  // Ce que l'infobulle en dit. ⚠ DÉJÀ TRADUIT.
  const char* label = "";
  // LE RÉGLAGE QU'ELLE PORTE, tel que l'infobulle le chiffre — et non sa place :
  // l'arrivée se lit par rapport au départ, une poignée par rapport au chemin
  // droit. Un seul nombre quand `single` est vrai.
  //
  // UNE POIGNÉE se relie par un trait au point qu'elle tire : `tied`.
  ImVec2 value{0.0F, 0.0F};
  bool single = false;
  std::optional<ImVec2> tied;
};

// LA TRAJECTOIRE DESSINÉE, depuis la naissance, y montant : son départ, son
// arrivée — relative au départ —, l'écart de ses deux poignées au tiers puis
// aux deux tiers du chemin droit, et l'ellipse de sa dispersion quand elle est
// allumée. Voir `ro::render::ArrivalDriftAt`.
struct PathTrace {
  ImVec2 start{0.0F, 0.0F};
  ImVec2 arrival{0.0F, 0.0F};
  ImVec2 bend_one{0.0F, 0.0F};
  ImVec2 bend_two{0.0F, 0.0F};
  bool scatter = false;
  ImVec2 scatter_radii{0.0F, 0.0F};
};

// Ce que le cadre laisse sous les pieds de la cible, par défaut.
inline constexpr float kPathCanvasFloorPixels = 40.0F;

struct PathCanvasView {
  const PathSample* sample = nullptr;
  PathTrace trace;
  std::span<const PathAnchor> anchors;
  // FAUX : le cadre ne fait que MONTRER les nombres, sans tracé ni point de
  // naissance — sur les onglets où l'on règle leur allure ou leurs couleurs.
  bool traced = true;
  // LA PLACE GARDÉE SOUS LES PIEDS, en pixels d'écran : plus grande, elle
  // remonte la cible, et une trajectoire qui descend reste dans le cadre.
  float floor_pixels = kPathCanvasFloorPixels;
  // LE PAS DE LA GRILLE, en pixels de sprite depuis la naissance : les ancres
  // s'y aimantent, sauf sous Maj. Zéro : ni grille ni aimant.
  float grid_pixels = 0.0F;
  // OÙ LE CLIC DROIT PEUT COPIER : des cibles de l'appelant, ⚠ DÉJÀ TRADUITES,
  // rendues par leur rang dans `OptionCanvasMove::copy_to`. Celle de rang
  // `copy_skipped` — celle qu'on règle — ne s'offre pas.
  std::span<const char* const> copy_targets;
  int copy_skipped = -1;
  // L'identifiant que rend la copie de TOUTE la trajectoire, ou moins un pour ne
  // pas l'offrir.
  int whole_path = -1;
};

// Ce point aimanté à la grille de ce pas ; tel quel sans grille.
[[nodiscard]] ImVec2 SnappedToGrid(ImVec2 at, float grid_pixels) noexcept;

// Le rayon, en pixels d'écran, dans lequel une ancre se saisit.
inline constexpr float kAnchorGrabPixels = 10.0F;

// Ce qu'un geste vaut sous Maj : un pixel d'écran n'y déplace l'ancre que d'un
// quart — de quoi viser au pixel de sprite près.
inline constexpr float kFineDragShare = 0.25F;

// Le point d'écran d'un point de trajectoire, pour des pieds posés en `feet`.
[[nodiscard]] ImVec2 PathCanvasPoint(ImVec2 feet, const PathSample& sample, ImVec2 at) noexcept;

// L'ancre tirée de `screen_delta` pixels d'écran, bornée à sa boîte. `fine`
// ralentit le geste — voir `kFineDragShare`.
[[nodiscard]] ImVec2 DraggedAnchor(const PathAnchor& anchor, ImVec2 screen_delta,
                                   float sprite_scale, bool fine) noexcept;

// L'ancre sous ce point d'écran — la plus proche dans `kAnchorGrabPixels` —, ou
// rien.
[[nodiscard]] std::optional<std::size_t> AnchorUnder(ImVec2 feet, const PathSample& sample,
                                                     std::span<const PathAnchor> anchors,
                                                     ImVec2 mouse) noexcept;

// LE CADRE ENTIER : le fond, la grille, le sol, la cible, les nombres, la
// trajectoire et ses ancres. Rend l'ancre déplacée à cette image — tirée, ou
// remise par le menu du clic droit —, la copie que ce menu demande, ou rien.
//
// ⚠ À APPELER JUSTE APRÈS LE BOUTON INVISIBLE QUI COUVRE LE CADRE : c'est lui
// qu'elle interroge pour la souris. Voir `OptionRowView::edit`.
[[nodiscard]] std::optional<OptionCanvasMove> DrawPathCanvas(ImDrawList& into, ImVec2 min,
                                                             ImVec2 max,
                                                             const PathCanvasView& view);

}  // namespace ro::ui
