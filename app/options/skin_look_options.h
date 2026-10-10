#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/options_window.h"
#include "ro/ui/skin_look.h"

#include "game/settings_file.h"
#include "options/skin_look_profiles.h"
#include "options/skin_options.h"

namespace moonclient {

// ── L'HABILLAGE DU SKIN, dans la section Skin ───────────────────────────────
//
// La luminosité des images, l'opacité et l'arrondi des fenêtres, chaque couleur
// de leur palette — corps, textes, liens, menus, sélection, infobulles —, et la
// teinte de leurs images — celles de la fenêtre, des boutons, des cases, des
// curseurs, des onglets et de la grille de l'Inventory —, avec trois thèmes,
// classique, sombre et sépia.
//
// 🔴 RANGÉ AVEC LE SKIN, et non dans `settings.yml` : `skin\<actif>\look.yaml`.
// Un skin sombre veut un habillage sombre ; le partager, c'est partager son
// dossier. Le fichier a son propre `SettingsFile`, donc la même écriture
// différée que les réglages.
//
// UN THÈME DE RÉFÉRENCE : le dernier posé. Chaque réglage qui s'en écarte porte
// un bouton qui l'y ramène, et la liste dit « retouché » plutôt qu'un
// « personnalisé » qui ne dirait plus d'où l'on vient.
//
// ⚠ IL S'APPLIQUE EN JEU, à l'image suivante : la palette n'est faite que de
// couleurs, rien n'est à recharger.
inline constexpr int kSkinLookRowBase = 8300;

// ⚠ DANS L'ORDRE DE LA PAGE. Les identifiants ne se gardent nulle part : les
// rangs peuvent bouger d'une version à l'autre.
enum class SkinLookRow : int {
  // L'aperçu, épinglé en tête de la page.
  kSample = 0,
  kPreset,
  kArtBrightness,
  kOpacity,
  kRounding,
  kBody,
  kBorder,
  kActiveTab,
  kIdleTab,
  kInput,
  kTableHeader,
  kPopup,
  kSelection,
  kArtTint,
  kTitleText,
  kText,
  kFaintText,
  kLink,
  kGoodText,
  kBadText,
  kWarnText,
  kInfoText,
  kPaper,
  kPaperText,
  kButtonTint,
  kButtonHoverTint,
  kButtonText,
  kCheckTint,
  kSliderTint,
  kItemTabTint,
  kItemGridTint,
  kSlot,
  kDoll,
  kCard,
  kCardBand,
  kCardBandText,
  kReset,
  // Le code d'un thème, à partager : voir `skin_look_code.h`.
  kCopyCode,
  kPasteCode,
  kPasteRefused,
  kCount,
};

static_assert(kSkinRowBase + static_cast<int>(SkinRow::kCount) <= kSkinLookRowBase,
              "la plage de l'habillage chevauche celle du skin");

// Le nom du fichier, dans le dossier du skin.
inline constexpr std::string_view kSkinLookFileName = "look.yaml";

// LES THÈMES : la palette du jeu, un sombre et un sépia. ⚠ L'ARRONDI N'EN FAIT
// PAS PARTIE : appliquer un thème ne reprend pas l'arrondi que le joueur a
// choisi.
inline constexpr std::size_t kSkinLookPresetCount = 3;
[[nodiscard]] const std::array<ro::ui::SkinLook, kSkinLookPresetCount>& SkinLookPresets();

class SkinLookOptions {
 public:
  // Ouvre `look.yaml` du skin porté, et le lit, puis les profils
  // d'habillage. Au démarrage, puis à chaque skin posé en jeu : ce qui était
  // ouvert s'écrit d'abord.
  void Open(const std::filesystem::path& skin_folder, const std::filesystem::path& profile_path);

  // Les fichiers : l'écriture différée, et l'écriture tout de suite.
  void Tick(std::uint32_t now_ms);
  void Flush();

  // La page des profils, sous les lignes de la section ; vide tant que le
  // fichier n'est pas ouvert.
  [[nodiscard]] ro::ui::OptionSectionBody ProfilesBody();
  // Un geste de cette page, appliqué au prochain `Tick`.
  void AskProfiles(ro::ui::ProfilesPageAction asked);

  [[nodiscard]] const ro::ui::SkinLook& look() const noexcept { return look_; }
  // Le rang du thème de référence dans `SkinLookPresets`.
  [[nodiscard]] int theme_rank() const noexcept { return theme_rank_; }

  // Ajoute ses lignes à celles d'une section ; rien tant que le fichier n'est
  // pas ouvert.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows);
  // L'APERÇU, à épingler en tête de la page : voir `ro::ui::DrawSkinLookSample`.
  void ComposeSample(std::vector<ro::ui::OptionRowView>& rows) const;

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // L'HABILLAGE D'ORIGINE, comme le rend sa ligne « Réinitialiser » : la page du
  // skin le rétablit avec ses réglages.
  void RestoreDefaults();

  // POSE LE THÈME QU'UN CODE PORTE, l'arrondi du joueur gardé : vrai quand la
  // ligne en était un. Voir `skin_look_code.h`.
  [[nodiscard]] bool PasteCode(std::string_view code);

 private:
  // Le rang du thème que l'habillage reproduit, ou celui de « retouché ».
  [[nodiscard]] int PresetRank() const;
  // Le thème de référence.
  [[nodiscard]] const ro::ui::SkinLook& Theme() const;
  // La couleur que règle une ligne, ou nul.
  [[nodiscard]] ImU32* ColorOf(SkinLookRow row);
  // Pose ce que la ligne demande, ou la ramène à son thème.
  void Apply(SkinLookRow row, const ro::ui::OptionsWindowAction& asked);
  void Revert(SkinLookRow row);
  // L'habillage qui reproduit un thème le prend pour référence.
  void FollowTheme();
  // Met de côté l'habillage qu'un autre va remplacer, s'il est retouché.
  void SetAsideRetouch();
  // Après une lecture — du fichier ou d'un profil : ramène l'habillage dans
  // ses bornes, qu'un fichier écrit à la main peut quitter, et donne son thème
  // à un fichier qui n'en dit pas.
  void Settle();

  // ⚠ L'HABILLAGE AVANT LE FICHIER, LE FICHIER AVANT SES PROFILS : chacun lit
  // le précédent, et doit mourir avant lui.
  ro::ui::SkinLook look_;
  // Le thème de référence ; négatif tant qu'un fichier ne l'a pas dit.
  int theme_rank_ = 0;
  // LA DERNIÈRE RETOUCHE QU'UN THÈME A REMPLACÉE, et le thème dont elle part :
  // la liste la propose, pour revenir d'un essai. ⚠ DE LA SESSION ET DU SKIN
  // SEULEMENT — le fichier ne garde que l'habillage porté.
  std::optional<ro::ui::SkinLook> set_aside_;
  int set_aside_rank_ = 0;
  // Les couleurs que le fichier connaît ; négatif tant qu'il ne l'a pas dit.
  int version_ = 0;
  // LE DERNIER PROFIL POSÉ, gardé au fichier mais hors de tout profil.
  std::string profile_name_;
  std::optional<SettingsFile> file_;
  std::optional<SkinLookProfiles> profiles_;
  // Les libellés de la liste. ⚠ ICI ET NON SUR LA PILE : la ligne n'en garde
  // qu'une vue, lue après la composition.
  std::vector<const char*> preset_names_;
  // LE DERNIER COLLAGE N'ÉTAIT PAS UN THÈME : la page le dit jusqu'au geste
  // suivant.
  bool paste_refused_ = false;
};

}  // namespace moonclient
