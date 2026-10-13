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
// et colore chacun à son goût. Un verrou commun les fige et les rend
// transparents aux clics, et l'aimantation les aligne entre eux.

// Les quatre cadres. ⚠ L'ORDRE PART DANS LES RÉGLAGES ET DANS LE FICHIER.
enum class PortraitPart : std::uint8_t {
  kHead = 0,
  kName = 1,
  kClass = 2,
  kLevel = 3,
};
inline constexpr std::size_t kPortraitPartCount = 4;

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

struct StatusPortraitLook {
  // ⚠ ÉTEINT PAR DÉFAUT : quatre cadres de plus ne s'imposent pas à l'écran.
  bool shown = false;
  bool locked = false;
  bool sticky = true;
  bool border = false;
  // Un `LevelFormat`, gardé en entier pour le fichier : voir `LevelFormatOf`.
  int level_format = static_cast<int>(LevelFormat::kSlashed);
  std::array<PortraitPartLook, kPortraitPartCount> parts = DefaultPortraitParts();
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
};

// L'écart entre le portrait rendu et le bord de son cadre, de chaque côté.
inline constexpr float kPortraitImageInset = 1.0F;

struct StatusPortraitAnswer {
  // Un cadre a bougé : l'appelant persiste, une fois.
  bool moved = false;
};

// Dessine les quatre cadres. `look` est modifié quand le joueur en déplace un.
void DrawStatusPortrait(const StatusPortraitView& view, const AlignGrid& grid,
                        StatusPortraitLook& look, HudGrip& grip, StatusPortraitAnswer& answer);

}  // namespace ro::ui
