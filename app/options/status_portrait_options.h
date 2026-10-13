#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "ro/ui/options_window.h"
#include "ro/ui/stat_bars.h"
#include "ro/ui/status_portrait.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LES ONGLETS « PORTRAIT » ET « ÉTIQUETTES » DE LA PAGE « BASIC INFO » ────
//
// Le portrait de statut du joueur : ce qu'il garde, ce que chaque onglet
// montre, ce qu'une ligne change.
//
// « Portrait » règle le personnage et son cadre ; « Étiquettes », le pseudo, la
// classe et le niveau. 🔴 CE QUI VAUT POUR LES QUATRE CADRES — l'interrupteur
// maître, le verrou, l'aimant, la bordure, la réinitialisation — OUVRE LES DEUX
// ONGLETS : le joueur le cherche là où il regarde, et l'interrupteur maître
// grise ce qu'il commande dans l'un comme dans l'autre.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`, comme
// celles du HUD de cible.
inline constexpr int kPortraitRowBase = 9000;
inline constexpr int kPortraitPartRowBase = 9100;
inline constexpr int kPortraitFieldsPerPart = 5;

enum class PortraitRow : int {
  kShown = 0,
  kLocked,
  kSticky,
  kHeadOnly,
  kGarment,
  kZoom,
  kOffsetX,
  kOffsetY,
  kAnimation,
  kDirection,
  kAnimated,
  kLevelFormat,
  kResetLayout,
  kBorder,
  kCount,
};

enum class PortraitPartField : int {
  kShown = 0,
  kBackground,
  kText,
  kRounding,
  kTextScale,
  kCount
};
static_assert(static_cast<int>(PortraitPartField::kCount) == kPortraitFieldsPerPart);
static_assert(kPortraitRowBase + static_cast<int>(PortraitRow::kCount) < kPortraitPartRowBase,
              "les lignes du portrait déborderaient sur celles de ses cadres");

[[nodiscard]] constexpr int PortraitRowId(PortraitRow row) {
  return kPortraitRowBase + static_cast<int>(row);
}
[[nodiscard]] constexpr int PortraitPartRowId(ro::ui::PortraitPart part, PortraitPartField field) {
  return kPortraitPartRowBase + static_cast<int>(part) * kPortraitFieldsPerPart +
         static_cast<int>(field);
}

// LES DEUX ONGLETS du portrait, dans l'ordre de la page.
enum class PortraitTab : int { kPortrait = 0, kLabels, kCount };

// LEUR PLACE dans la page « Basic Info » : après les sept jauges, et les
// derniers. Le filtre des Options compose les onglets jusqu'à ce compte.
inline constexpr std::size_t kPortraitTab = ro::ui::kStatBarCount;
inline constexpr std::size_t kLabelsTab = kPortraitTab + 1;
inline constexpr std::size_t kBasicInfoTabCount =
    kPortraitTab + static_cast<std::size_t>(PortraitTab::kCount);
static_assert(kLabelsTab - kPortraitTab == static_cast<std::size_t>(PortraitTab::kLabels));

// Les bornes du cadrage, celles du panneau ET celles que la vue applique : une
// valeur relue d'un fichier édité à la main ne sort pas d'elles.
inline constexpr float kPortraitZoomLeast = 0.10F;
inline constexpr float kPortraitZoomMost = 2.00F;
inline constexpr float kPortraitOffsetMost = 1.5F;
// Les neuf premiers rangs d'animation d'un personnage, du repos à la mort.
inline constexpr int kPortraitAnimationCount = 9;
inline constexpr int kPortraitDirectionCount = 8;
// La garde : la pose prête au combat.
inline constexpr int kPortraitDefaultAnimation = 4;

// COMMENT LE PERSONNAGE SE MONTRE dans le cadre du portrait. `ui` n'en lit
// rien : c'est `app` qui compose la vignette.
struct PortraitFramingSettings {
  // Cadre la tête et ses coiffes, et masque la cape. Faux : le personnage
  // entier.
  bool head_only = true;
  float zoom = 1.0F;
  // En tailles du cadre, depuis son centre.
  float offset_x = 0.0F;
  float offset_y = 0.0F;
  // Le rang d'animation du .act, de 0 (repos) à 8 (mort).
  int animation = kPortraitDefaultAnimation;
  // La direction du .act : 0 est la face, puis dans le sens des aiguilles
  // d'une montre.
  int direction = 0;
  bool animated = true;
  // La cape, en personnage entier seulement.
  bool garment = true;
};

// Le cadrage ramené dans ses bornes : un fichier édité à la main peut porter
// n'importe quoi, et c'est cette version-là que la vignette lit. La direction
// boucle, le reste se borne.
[[nodiscard]] PortraitFramingSettings BoundedFraming(const PortraitFramingSettings& stored);

// CE QU'UN PROPRIÉTAIRE LIVRE AU PREMIER LANCEMENT : le cadrage et les quatre
// cadres. « Réinitialiser la disposition » repose les places de ce `look`.
struct StatusPortraitSetup {
  PortraitFramingSettings framing;
  ro::ui::StatusPortraitLook look;
};

class StatusPortraitOptions {
 public:
  // Les défauts d'origine, ceux des structures.
  StatusPortraitOptions() = default;
  // ⚠ AVANT `Declare` : c'est ce qui en fait les défauts du fichier, donc ce
  // que « Rétablir cette page » repose.
  explicit StatusPortraitOptions(const StatusPortraitSetup& factory)
      : framing_(factory.framing), look_(factory.look), factory_look_(factory.look) {}

  [[nodiscard]] const PortraitFramingSettings& framing() const noexcept { return framing_; }
  // En écriture : le joueur pose les cadres à la souris.
  [[nodiscard]] ro::ui::StatusPortraitLook& look() noexcept { return look_; }
  [[nodiscard]] const ro::ui::StatusPortraitLook& look() const noexcept { return look_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Refait les lignes de cet onglet, à chaque image où il est sous les yeux.
  // `shared` faux omet les réglages communs aux deux onglets : la recherche
  // ne les compose que sous le premier, pour qu'ils ne ressortent qu'une fois.
  void Compose(PortraitTab tab, bool shared = true);
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  // Ce qui ouvre les deux onglets.
  void ComposeShared();
  // Le personnage : sa pose, son cadrage, son cadre.
  void ComposeFraming();
  // Le pseudo, la classe et le niveau.
  void ComposeLabels();
  // Le cadre d'une partie : un interrupteur, puis le fond, le texte, l'arrondi
  // et la taille du texte — ces deux-là pour une étiquette seulement.
  void ComposePart(ro::ui::PortraitPart part, const char* heading);

  PortraitFramingSettings framing_;
  ro::ui::StatusPortraitLook look_;
  // La disposition livrée, que « Réinitialiser la disposition » repose.
  ro::ui::StatusPortraitLook factory_look_;
  std::vector<ro::ui::OptionRowView> rows_;
  // Refaits à chaque composition : le pointeur d'un texte traduit meurt au
  // changement de langue.
  std::vector<const char*> animation_names_;
  std::vector<const char*> direction_names_;
  std::vector<const char*> level_format_names_;
};

}  // namespace moonclient
