#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/doll_overlay.h"
#include "ro/ui/hud_frame.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LE PORTRAIT DE STATUT ───────────────────────────────────────────────────
//
// Le personnage du joueur, d'un coup d'œil : son portrait, son pseudo, sa
// classe et son niveau, en quatre cadres libres que le joueur pose, retaille
// et colore chacun à son goût.
//
// DEUX GROUPES : le portrait seul, et les trois étiquettes. Chacun s'affiche,
// s'aimante et se borde pour lui. Le verrou n'est à aucun des deux : c'est
// celui de la page « Basic Info », que les jauges portent — voir
// `StatusPortraitView::locked`.

// Les quatre cadres. ⚠ L'ORDRE PART DANS LES RÉGLAGES ET DANS LE FICHIER.
enum class PortraitPart : std::uint8_t {
  kHead = 0,
  kName = 1,
  kClass = 2,
  kLevel = 3,
};
inline constexpr std::size_t kPortraitPartCount = 4;

// Les deux groupes de cadres, dans l'ordre des onglets des réglages.
enum class PortraitGroup : std::uint8_t {
  // Le portrait seul.
  kHead = 0,
  // Le pseudo, la classe et le niveau.
  kLabels = 1,
};
inline constexpr std::size_t kPortraitGroupCount = 2;
[[nodiscard]] PortraitGroup PortraitGroupOf(PortraitPart part) noexcept;

// Comment le niveau s'écrit. ⚠ MÊME RÈGLE D'ORDRE : le rang est gravé dans le
// fichier.
enum class LevelFormat : std::uint8_t {
  // « 99/70 »
  kSlashed = 0,
  // « Base 99 · Job 70 »
  kLabelled = 1,
  // « 99 »
  kBaseOnly = 2,
};
// Relu d'un fichier : une valeur hors bornes retombe sur « 99/70 ».
[[nodiscard]] LevelFormat LevelFormatOf(int stored) noexcept;

// Le texte du niveau. Une part inconnue ne s'écrit pas : ni zéro, ni tiret.
// Vide quand rien n'est connu de ce qu'il faut écrire.
[[nodiscard]] std::string PortraitLevelText(std::optional<std::int32_t> base_level,
                                            std::optional<std::int32_t> job_level,
                                            LevelFormat format);

// Les bornes appliquées AU DESSIN : une valeur relue d'un fichier édité à la
// main ne doit donner ni une police géante, ni un arrondi qui avale le cadre.
inline constexpr float kPortraitTextScaleLeast = 0.5F;
inline constexpr float kPortraitTextScaleMost = 4.0F;
inline constexpr float kPortraitRoundingMost = 16.0F;

// Un cadre : ce que le joueur en a fait. Le portrait n'a pas de texte, mais le
// champ reste, pour que les quatre se rangent de la même façon.
struct PortraitPartLook {
  bool shown = true;
  HudRect rect;
  ImU32 background = IM_COL32(13, 13, 18, 199);
  ImU32 text = IM_COL32_WHITE;
  float rounding = 4.0F;
  // × la taille de la police de l'interface.
  float text_scale = 1.0F;
};

// La disposition d'origine, que « Réinitialiser la disposition » repose.
[[nodiscard]] HudRect DefaultPortraitPartRect(PortraitPart part);
[[nodiscard]] std::array<PortraitPartLook, kPortraitPartCount> DefaultPortraitParts();

// Ce qu'un groupe règle pour lui seul.
struct PortraitGroupLook {
  // ⚠ ÉTEINT PAR DÉFAUT : des cadres de plus ne s'imposent pas à l'écran.
  bool shown = false;
  // Les bords d'un cadre du groupe qu'on déplace s'alignent sur ceux des
  // autres cadres visibles ; un bloc CTRL n'emporte que ceux de son groupe.
  bool sticky = true;
  bool border = false;
};

struct StatusPortraitLook {
  std::array<PortraitGroupLook, kPortraitGroupCount> groups{};
  // Un `LevelFormat`, gardé en entier pour le fichier : voir `LevelFormatOf`.
  int level_format = static_cast<int>(LevelFormat::kSlashed);
  std::array<PortraitPartLook, kPortraitPartCount> parts = DefaultPortraitParts();

  [[nodiscard]] PortraitGroupLook& group(PortraitGroup which) noexcept {
    return groups[static_cast<std::size_t>(which)];
  }
  [[nodiscard]] const PortraitGroupLook& group(PortraitGroup which) const noexcept {
    return groups[static_cast<std::size_t>(which)];
  }
  // Un groupe au moins est allumé.
  [[nodiscard]] bool AnyShown() const noexcept;
  // Le cadre paraît : son groupe ET lui sont allumés.
  [[nodiscard]] bool PartShown(PortraitPart part) const noexcept;
};

// Le joueur, en lecture seule. `ui` ne connaît pas `sim` : c'est `app` qui lit
// le monde et compose le portrait.
struct StatusPortraitView {
  // Faux sans personnage : rien ne se dessine.
  bool present = false;
  // Vide tant que le nom n'est pas connu : le cadre écrit alors « ? ».
  std::string_view name;
  std::string_view job;
  std::optional<std::int32_t> base_level;
  std::optional<std::int32_t> job_level;
  // Le portrait, déjà rendu pour l'INTÉRIEUR du cadre : un pixel de moins de
  // chaque côté. Inutilisable pour une classe dessinée en modèle 3D, que
  // `model` signale.
  SkinImage portrait;
  bool model = false;
  // LES EFFETS DE CHAPEAU QUE LE JOUEUR PORTE, peints par `app` avec le
  // portrait, au même cadrage et découpés à l'intérieur du cadre ; vide sans
  // effet. Voir `DollOverlay`.
  DollOverlay overlay;
  // UN EFFET CHANGE LE CORPS — le rapetisse, l'éclaire, le double — : le
  // rappel le repeint, et l'image composée ne se pose pas.
  bool body_overlaid = false;
  // LES CADRES D'UN AUTRE HUD, visibles, sur lesquels les quatre s'aimantent
  // aussi — les jauges de la même page. Ils ne bougent jamais d'ici : un bloc
  // CTRL ne les emporte pas.
  std::span<const HudRect> neighbours;
  // LE VERROU DE LA PAGE « BASIC INFO », celui des jauges : le portrait ne le
  // possède pas, il le lit. Verrouillés, les cadres se figent et laissent
  // passer les clics au jeu.
  bool locked = false;
};

// L'écart entre le portrait rendu et le bord de son cadre, de chaque côté.
inline constexpr float kPortraitImageInset = 1.0F;

struct StatusPortraitAnswer {
  // Un cadre a bougé : l'appelant persiste, une fois.
  bool moved = false;
};

// Dessine les cadres allumés, groupe par groupe. `look` est modifié quand le
// joueur en déplace un.
void DrawStatusPortrait(const StatusPortraitView& view, const AlignGrid& grid,
                        StatusPortraitLook& look, HudGrip& grip, StatusPortraitAnswer& answer);

}  // namespace ro::ui
