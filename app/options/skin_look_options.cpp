// L'habillage du skin. Voir l'en-tête.

#include "options/skin_look_options.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_look_sample.h"

#include "options/skin_look_code.h"
#include "options/skin_look_colors.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::SkinLook;

// Les clés du fichier. ⚠ GRAVÉES : les changer rend à tous les joueurs
// l'habillage d'origine, sans que rien ne le dise. Le fond des cartes garde la
// clé du papier de l'album, qui est la même couleur.
constexpr const char* kArtBrightnessKey = "album_image_brightness";
constexpr const char* kOpacityKey = "opacity";
constexpr const char* kRoundingKey = "rounding";
constexpr const char* kProfileKey = "profile";
// LE THÈME DE RÉFÉRENCE, rang dans `kPresets`. Absent des fichiers d'avant les
// teintes.
constexpr const char* kThemeKey = "theme";
constexpr int kUnknownTheme = -1;
// LA VERSION DE L'HABILLAGE QU'UN FICHIER PORTE : voir `skin_look_colors.h`.
//
// ⚠ UN FICHIER NE LA DIT QUE DEPUIS LES TEXTES. Avant, son thème la trahit :
// présent depuis les teintes, absent avant. Voir `Settle`.
constexpr const char* kVersionKey = "version";
constexpr int kUnknownVersion = -1;
using skin_look::kCurrentVersion;
using skin_look::kDarkPaperVersion;
using skin_look::kOriginalVersion;
using skin_look::kTintsVersion;

// Les rangs des thèmes.
constexpr int kClassicRank = 0;
constexpr int kDarkRank = 1;
constexpr int kSepiaRank = 2;

// LE PAPIER CLAIR QU'AVAIT LE THÈME SOMBRE. Un fichier plus ancien qui le porte
// encore ne l'a pas choisi — il s'y est écrit de lui-même — et prend le papier
// du thème. ⚠ LE FOND ET SON TEXTE ENSEMBLE, ou aucun : un texte retouché pour
// le papier clair ne se lirait plus sur le sombre.
constexpr ImU32 kDarkThemeLightPaper = IM_COL32(236, 238, 244, 255);
constexpr ImU32 kDarkThemeLightPaperText = IM_COL32(20, 22, 28, 255);

// Les bornes. En dessous de la luminosité ou de l'opacité la plus basse, une
// image ou une fenêtre ne se lit plus ; au-delà de l'arrondi le plus grand, le
// coin mange le contenu.
constexpr float kLeastBrightness = 0.3F;
constexpr float kMostBrightness = 1.0F;
constexpr float kLeastOpacity = 0.3F;
constexpr float kMostOpacity = 1.0F;
constexpr float kLeastRounding = 0.0F;
constexpr float kMostRounding = 8.0F;

// LES COULEURS DE LA PALETTE : voir `skin_look_colors.h`.
using skin_look::Absent;
using skin_look::ColorRow;
using skin_look::kColorRows;

// La couleur que règle une ligne, ou nul.
[[nodiscard]] const ColorRow* FindColorRow(SkinLookRow row) {
  const auto found = std::ranges::find(kColorRows, row, &ColorRow::row);
  return found == kColorRows.end() ? nullptr : &*found;
}

// Le libellé d'une couleur.
[[nodiscard]] const char* LabelOf(SkinLookRow row) {
  switch (row) {
    case SkinLookRow::kBody:
      return Tr("Corps");
    case SkinLookRow::kBorder:
      return Tr("Bordure");
    case SkinLookRow::kTitleText:
      return Tr("Texte du titre");
    case SkinLookRow::kText:
      return Tr("Texte du corps");
    case SkinLookRow::kActiveTab:
      return Tr("Onglet actif");
    case SkinLookRow::kIdleTab:
      return Tr("Onglet inactif");
    case SkinLookRow::kInput:
      return Tr("Champ de saisie");
    case SkinLookRow::kTableHeader:
      return Tr("En-tête de tableau");
    case SkinLookRow::kPopup:
      return Tr("Menus et listes");
    case SkinLookRow::kSelection:
      return Tr("Sélection");
    case SkinLookRow::kArtTint:
      return Tr("Teinte des images");
    case SkinLookRow::kFaintText:
      return Tr("Texte secondaire");
    case SkinLookRow::kLink:
      return Tr("Liens");
    case SkinLookRow::kGoodText:
      return Tr("Réussite");
    case SkinLookRow::kBadText:
      return Tr("Manque ou échec");
    case SkinLookRow::kWarnText:
      return Tr("Mise en garde");
    case SkinLookRow::kInfoText:
      return Tr("Information");
    case SkinLookRow::kPaper:
      return Tr("Fond des bulles et descriptions");
    case SkinLookRow::kPaperText:
      return Tr("Texte des bulles et descriptions");
    case SkinLookRow::kButtonTint:
      return Tr("Teinte des boutons");
    case SkinLookRow::kButtonHoverTint:
      return Tr("Teinte du survol");
    case SkinLookRow::kButtonText:
      return Tr("Texte des boutons");
    case SkinLookRow::kCheckTint:
      return Tr("Teinte des cases à cocher");
    case SkinLookRow::kSliderTint:
      return Tr("Teinte des curseurs");
    case SkinLookRow::kItemTabTint:
      return Tr("Teinte des onglets");
    case SkinLookRow::kItemGridTint:
      return Tr("Teinte de la grille");
    case SkinLookRow::kSlot:
      return Tr("Fond des cases");
    case SkinLookRow::kDoll:
      return Tr("Fond du mannequin");
    case SkinLookRow::kCard:
      return Tr("Fond d'une carte");
    case SkinLookRow::kCardBand:
      return Tr("Bandeau d'une carte");
    case SkinLookRow::kCardBandText:
      return Tr("Texte du bandeau");
    default:
      return "";
  }
}

// L'aide d'une couleur, ou nul. ⚠ UNE TEINTE DIT CE QUE FAIT SON BLANC : c'est
// ce qu'on ne devine pas devant une pastille blanche.
[[nodiscard]] const char* HelpOf(SkinLookRow row) {
  switch (row) {
    case SkinLookRow::kPopup:
      return Tr(
          "Le fond des menus et des listes déroulantes ouverts dans les fenêtres. Ceux du HUD "
          "et de la conversation gardent celui du jeu.");
    case SkinLookRow::kSelection:
      return Tr(
          "La ligne choisie d'une liste ou d'un menu, transparence comprise. Son survol s'en "
          "déduit.");
    case SkinLookRow::kFaintText:
      return Tr(
          "Les « (?) », les notes, les compteurs, les titres de groupe, et ce qui est grisé.");
    case SkinLookRow::kLink:
      return Tr("Ce qui se clique pour ouvrir autre chose : un item, un monstre, une adresse.");
    case SkinLookRow::kGoodText:
      return Tr("Ce qui est réuni ou réussi : un matériau en main, un raffinement réussi.");
    case SkinLookRow::kBadText:
      return Tr(
          "Ce qui manque, échoue ou est refusé : un matériau absent, une arme détruite, un texte "
          "trop long.");
    case SkinLookRow::kWarnText:
      return Tr("Ce qui est à surveiller : une chance moyenne, une demande en attente.");
    case SkinLookRow::kInfoText:
      return Tr("Ce qui informe : le niveau d'un raffinement, une chaîne qui avance.");
    case SkinLookRow::kPaper:
      return Tr(
          "Les bulles qui s'ouvrent au survol, les descriptions d'items et de skills, et le "
          "journal des quêtes. Les bulles du HUD gardent le leur.");
    case SkinLookRow::kPaperText:
      return Tr(
          "Une description garde ses propres couleurs, choisies pour un fond clair : sur un fond "
          "sombre, celles qui ne se liraient plus s'éclaircissent d'elles-mêmes.");
    case SkinLookRow::kArtTint:
      return Tr(
          "La barre de titre et ses boutons, le cadre, l'ascenseur et le pied des fenêtres. "
          "Blanc : l'image du skin telle quelle.");
    case SkinLookRow::kButtonTint:
      return Tr(
          "Les boutons au repos, et la flèche des listes déroulantes. Blanc : l'image du skin "
          "telle quelle.");
    case SkinLookRow::kButtonHoverTint:
      return Tr(
          "Un bouton sous la souris, enfoncé ou retenu. Le skin lui donne une autre image, "
          "bleue. Blanc : l'image du skin telle quelle.");
    case SkinLookRow::kButtonText:
      return Tr(
          "Il est posé sur l'image du bouton, et non sur le corps : c'est pour elle qu'il "
          "se choisit.");
    case SkinLookRow::kCheckTint:
      return Tr("Les cases à cocher et les boutons radio. Blanc : l'image du skin telle quelle.");
    case SkinLookRow::kSliderTint:
      return Tr(
          "La piste, les flèches et la poignée des curseurs. Blanc : l'image du skin telle "
          "quelle.");
    case SkinLookRow::kItemTabTint:
      return Tr(
          "Les onglets de l'Inventory et du Cart, sur le côté. Blanc : l'image du skin "
          "telle quelle.");
    case SkinLookRow::kItemGridTint:
      return Tr(
          "Les cases de l'Inventory et du Cart, sous les items. Blanc : l'image du skin "
          "telle quelle.");
    default:
      return nullptr;
  }
}

// Les intertitres, posés au-dessus de la première ligne de leur groupe.
[[nodiscard]] const char* HeadingOf(SkinLookRow row) {
  switch (row) {
    case SkinLookRow::kBody:
      return Tr("Couleurs des fenêtres");
    case SkinLookRow::kTitleText:
      return Tr("Textes");
    case SkinLookRow::kGoodText:
      return Tr("Couleurs d'état");
    case SkinLookRow::kPaper:
      return Tr("Infobulles et descriptions");
    case SkinLookRow::kButtonTint:
      return Tr("Boutons et commandes");
    case SkinLookRow::kItemTabTint:
      return Tr("Inventory et Cart");
    case SkinLookRow::kSlot:
      return Tr("Fiche du personnage");
    case SkinLookRow::kCard:
      return Tr("Cartes et album");
    default:
      return nullptr;
  }
}

// Le nom de la liste quand l'habillage s'écarte de son thème.
[[nodiscard]] const char* RetouchedNameOf(int theme_rank) {
  switch (theme_rank) {
    case kDarkRank:
      return Tr("Sombre (retouché)");
    case kSepiaRank:
      return Tr("Sépia (retouché)");
    default:
      return Tr("Classique (retouché)");
  }
}

// La bulle du bouton qui ramène un réglage à son thème.
[[nodiscard]] const char* RevertHelpOf(int theme_rank) {
  switch (theme_rank) {
    case kDarkRank:
      return Tr("Reprendre le réglage du thème Sombre.");
    case kSepiaRank:
      return Tr("Reprendre le réglage du thème Sépia.");
    default:
      return Tr("Reprendre le réglage du thème Classique.");
  }
}

// LES THÈMES. Le classique est la palette du jeu ; un menu prend la couleur du
// corps.
//
// ⚠ LE SOMBRE FONCE AUSSI LES IMAGES, et c'est ce qui le rend lisible : les
// boutons, la grille et la barre de titre du skin sont presque blancs, et un
// texte clair choisi pour le corps sombre y disparaît. Son survol reste PLUS
// CLAIR que le repos, et celui du sépia chaud : l'image survolée du skin est
// bleue.
//
// SON PAPIER EST SOMBRE AUSSI : les couleurs d'une description, choisies pour
// un fond clair, s'y éclaircissent quand elles ne s'y liraient plus.
constexpr std::array<SkinLook, kSkinLookPresetCount> kPresets = {{
    SkinLook{},
    SkinLook{.art_brightness = 0.9F,
             .body = IM_COL32(44, 46, 54, 255),
             .border = IM_COL32(90, 94, 110, 255),
             .title_text = IM_COL32(255, 255, 255, 255),
             .text = IM_COL32(226, 228, 235, 255),
             .active_tab = IM_COL32(90, 120, 190, 255),
             .idle_tab = IM_COL32(70, 74, 86, 255),
             .input = IM_COL32(64, 66, 76, 255),
             .table_header = IM_COL32(58, 60, 70, 255),
             .popup = IM_COL32(44, 46, 54, 255),
             .slot = IM_COL32(64, 66, 76, 255),
             .doll = IM_COL32(54, 56, 66, 255),
             .card = IM_COL32(54, 56, 66, 255),
             .card_band = IM_COL32(34, 36, 44, 255),
             .card_band_text = IM_COL32(226, 228, 235, 255),
             .art_tint = IM_COL32(120, 128, 150, 255),
             .button_tint = IM_COL32(104, 110, 128, 255),
             .button_hover_tint = IM_COL32(140, 150, 180, 255),
             .button_text = IM_COL32(232, 234, 240, 255),
             .check_tint = IM_COL32(150, 156, 172, 255),
             .slider_tint = IM_COL32(150, 156, 176, 255),
             .item_tab_tint = IM_COL32(130, 136, 156, 255),
             .item_grid_tint = IM_COL32(84, 88, 102, 255),
             .faint_text = IM_COL32(150, 154, 166, 255),
             .link = IM_COL32(110, 170, 255, 255),
             .selection = IM_COL32(90, 120, 190, 140),
             .paper = IM_COL32(40, 42, 50, 255),
             .paper_text = IM_COL32(226, 228, 235, 255),
             .good_text = IM_COL32(110, 200, 120, 255),
             .bad_text = IM_COL32(240, 110, 110, 255),
             .warn_text = IM_COL32(235, 175, 80, 255),
             .info_text = IM_COL32(120, 170, 255, 255)},
    SkinLook{.body = IM_COL32(244, 236, 218, 255),
             .border = IM_COL32(176, 150, 110, 255),
             .title_text = IM_COL32(40, 28, 12, 255),
             .text = IM_COL32(60, 44, 24, 255),
             .active_tab = IM_COL32(196, 166, 120, 255),
             .idle_tab = IM_COL32(226, 214, 190, 255),
             .input = IM_COL32(232, 222, 200, 255),
             .table_header = IM_COL32(224, 210, 184, 255),
             .popup = IM_COL32(244, 236, 218, 255),
             .slot = IM_COL32(232, 222, 200, 255),
             .doll = IM_COL32(240, 232, 214, 255),
             .card = IM_COL32(250, 244, 228, 255),
             .card_band = IM_COL32(150, 120, 80, 255),
             .card_band_text = IM_COL32(250, 244, 230, 255),
             .art_tint = IM_COL32(245, 230, 200, 255),
             .button_tint = IM_COL32(240, 228, 204, 255),
             .button_hover_tint = IM_COL32(255, 222, 170, 255),
             .button_text = IM_COL32(60, 44, 24, 255),
             .check_tint = IM_COL32(240, 228, 204, 255),
             .slider_tint = IM_COL32(240, 228, 204, 255),
             .item_tab_tint = IM_COL32(245, 236, 215, 255),
             .item_grid_tint = IM_COL32(250, 242, 225, 255),
             .faint_text = IM_COL32(130, 112, 90, 255),
             .link = IM_COL32(40, 96, 170, 255),
             .selection = IM_COL32(196, 166, 120, 160),
             .paper = IM_COL32(252, 248, 236, 255),
             .paper_text = IM_COL32(40, 28, 12, 255),
             .good_text = IM_COL32(40, 110, 40, 255),
             .bad_text = IM_COL32(160, 40, 30, 255),
             .warn_text = IM_COL32(150, 90, 10, 255),
             .info_text = IM_COL32(40, 90, 160, 255)},
}};

[[nodiscard]] int RowId(SkinLookRow row) { return kSkinLookRowBase + static_cast<int>(row); }

// Deux habillages sont le même thème s'ils ne diffèrent que par l'arrondi.
[[nodiscard]] bool SameTheme(SkinLook left, const SkinLook& right) {
  left.rounding = right.rounding;
  return left == right;
}

// LE THÈME LE PLUS PROCHE d'un habillage : celui dont il garde le plus de
// réglages, parmi ceux qu'un fichier de cette version porte. À égalité, le
// premier.
[[nodiscard]] int NearestPreset(const SkinLook& look, int version) {
  int nearest = kClassicRank;
  int most_shared = -1;
  for (std::size_t rank = 0; rank < kPresets.size(); ++rank) {
    const SkinLook& theme = kPresets[rank];
    int shared = (look.art_brightness == theme.art_brightness ? 1 : 0) +
                 (look.opacity == theme.opacity ? 1 : 0);
    for (const ColorRow& color : kColorRows) {
      if (color.since <= version && look.*color.member == theme.*color.member) {
        ++shared;
      }
    }
    if (shared > most_shared) {
      most_shared = shared;
      nearest = static_cast<int>(rank);
    }
  }
  return nearest;
}

}  // namespace

const std::array<SkinLook, kSkinLookPresetCount>& SkinLookPresets() { return kPresets; }

void SkinLookOptions::Open(const std::filesystem::path& skin_folder,
                           const std::filesystem::path& profile_path) {
  Flush();
  profiles_.reset();
  file_.reset();
  look_ = SkinLook{};
  set_aside_.reset();
  theme_rank_ = kUnknownTheme;
  version_ = kUnknownVersion;
  profile_name_.clear();
  file_.emplace(skin_folder / std::string(kSkinLookFileName));
  file_->Section("L'habillage du skin");
  file_->Declare(kArtBrightnessKey, &look_.art_brightness);
  file_->Declare(kOpacityKey, &look_.opacity);
  file_->Declare(kRoundingKey, &look_.rounding);
  for (const ColorRow& color : kColorRows) {
    file_->Declare(color.key, &(look_.*color.member));
  }
  // ⚠ DANS LES PROFILS : un profil posé ramène aussi son thème, et dit ce qu'il
  // connaît.
  file_->Declare(kThemeKey, &theme_rank_);
  file_->Declare(kVersionKey, &version_);
  {
    // ⚠ DE LA MACHINE : un profil qui retiendrait son propre nom le
    // reposerait en se posant.
    const SettingsFile::MachineScope machine(*file_);
    file_->Declare(kProfileKey, &profile_name_);
  }
  file_->Load();
  profiles_.emplace(profile_path, *file_, profile_name_);
  Settle();
}

void SkinLookOptions::Settle() {
  look_.art_brightness = std::clamp(look_.art_brightness, kLeastBrightness, kMostBrightness);
  look_.opacity = std::clamp(look_.opacity, kLeastOpacity, kMostOpacity);
  look_.rounding = std::clamp(look_.rounding, kLeastRounding, kMostRounding);
  if (version_ < kCurrentVersion) {
    // UN FICHIER — OU UN PROFIL — PLUS ANCIEN ne porte pas les couleurs venues
    // depuis. Elles prennent celles de son thème, ou du plus proche s'il ne le
    // dit pas, plutôt que le défaut classique : un habillage sombre garderait
    // sinon des boutons blancs sous un libellé clair.
    const int version = version_ >= kOriginalVersion ? version_
                        : theme_rank_ >= 0           ? kTintsVersion
                                                     : kOriginalVersion;
    if (theme_rank_ < 0) {
      theme_rank_ = NearestPreset(look_, version);
    }
    theme_rank_ = std::min(theme_rank_, static_cast<int>(kPresets.size()) - 1);
    const SkinLook& theme = Theme();
    for (const ColorRow& color : kColorRows) {
      if (color.since > version && color.absent == Absent::kTakeTheme) {
        look_.*color.member = theme.*color.member;
      }
    }
    if (version < kDarkPaperVersion && theme_rank_ == kDarkRank &&
        look_.paper == kDarkThemeLightPaper && look_.paper_text == kDarkThemeLightPaperText) {
      look_.paper = theme.paper;
      look_.paper_text = theme.paper_text;
    }
    version_ = kCurrentVersion;
    if (file_.has_value()) {
      file_->Touch();
    }
  }
  theme_rank_ = std::min(theme_rank_, static_cast<int>(kPresets.size()) - 1);
  FollowTheme();
}

void SkinLookOptions::Tick(std::uint32_t now_ms) {
  if (profiles_.has_value()) {
    profiles_->Tick(now_ms);
    Settle();
    if (profiles_->active() != profile_name_) {
      profile_name_ = profiles_->active();
      file_->Touch();
    }
  }
  if (file_.has_value()) {
    file_->Tick(now_ms);
  }
}

void SkinLookOptions::Flush() {
  if (profiles_.has_value()) {
    profiles_->Flush();
  }
  if (file_.has_value()) {
    file_->Flush();
  }
}

void SkinLookOptions::AskProfiles(ro::ui::ProfilesPageAction asked) {
  if (profiles_.has_value()) {
    profiles_->Ask(std::move(asked));
  }
}

ro::ui::OptionSectionBody SkinLookOptions::ProfilesBody() {
  return profiles_.has_value() ? profiles_->Body() : ro::ui::OptionSectionBody{};
}

int SkinLookOptions::PresetRank() const {
  for (std::size_t rank = 0; rank < kPresets.size(); ++rank) {
    if (SameTheme(look_, kPresets[rank])) {
      return static_cast<int>(rank);
    }
  }
  return static_cast<int>(kPresets.size());
}

const SkinLook& SkinLookOptions::Theme() const {
  return kPresets[static_cast<std::size_t>(
      std::clamp(theme_rank_, kClassicRank, static_cast<int>(kPresets.size()) - 1))];
}

void SkinLookOptions::FollowTheme() {
  // ⚠ LA LISTE MONTRE CE THÈME : un bouton de retour vers un autre la
  // contredirait.
  if (const int matched = PresetRank(); matched < static_cast<int>(kPresets.size())) {
    theme_rank_ = matched;
  }
}

void SkinLookOptions::SetAsideRetouch() {
  if (PresetRank() == static_cast<int>(kPresets.size())) {
    set_aside_ = look_;
    set_aside_rank_ = theme_rank_;
  }
}

ImU32* SkinLookOptions::ColorOf(SkinLookRow row) {
  const ColorRow* color = FindColorRow(row);
  return color == nullptr ? nullptr : &(look_.*color->member);
}

void SkinLookOptions::ComposeSample(std::vector<ro::ui::OptionRowView>& rows) const {
  if (!file_.has_value()) {
    return;
  }
  rows.push_back(
      ro::ui::OptionRowView{.id = RowId(SkinLookRow::kSample), .kind = ro::ui::OptionKind::kPanel});
  rows.back().panel = ro::ui::DrawSkinLookSample;
}

void SkinLookOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) {
  if (!file_.has_value()) {
    return;
  }
  const SkinLook& theme = Theme();
  const char* const revert = Tr("Rétablir");
  const char* const revert_help = RevertHelpOf(theme_rank_);
  // ⚠ L'ENTRÉE « RETOUCHÉ » N'EST LÀ QUE S'IL Y A UNE RETOUCHE à montrer ou à
  // reprendre : présente sans rien derrière, elle ne se choisirait pas.
  preset_names_ = {Tr("Classique"), Tr("Sombre"), Tr("Sépia")};
  if (PresetRank() == static_cast<int>(kPresets.size())) {
    preset_names_.push_back(RetouchedNameOf(theme_rank_));
  } else if (set_aside_.has_value()) {
    preset_names_.push_back(RetouchedNameOf(set_aside_rank_));
  }
  rows.push_back(ro::ui::OptionRowView{
      .id = RowId(SkinLookRow::kPreset),
      .label = Tr("Thème"),
      .help = Tr("Toutes les couleurs et les teintes d'un coup, et la luminosité des images. "
                 "L'arrondi reste le tien. Un réglage retouché se rétablit seul, par le bouton "
                 "qui le suit. Rangé avec le skin : chaque skin garde le sien."),
      .kind = ro::ui::OptionKind::kChoice,
      .chosen = PresetRank(),
      .choices = preset_names_});
  rows.back().heading = Tr("Habillage des fenêtres");
  rows.push_back(ro::ui::OptionRowView{
      .id = RowId(SkinLookRow::kArtBrightness),
      .label = Tr("Luminosité des images"),
      .help = Tr("La barre de titre, ses boutons, l'ascenseur, le pied des fenêtres et les "
                 "images de l'album. Ni le texte ni les fonds : leurs couleurs se règlent "
                 "plus bas."),
      .kind = ro::ui::OptionKind::kSlider,
      .value = look_.art_brightness,
      .lowest = kLeastBrightness,
      .highest = kMostBrightness,
      .format = "%.2f",
      .revert = revert,
      .revert_help = revert_help,
      .at_reference = look_.art_brightness == theme.art_brightness});
  rows.push_back(ro::ui::OptionRowView{.id = RowId(SkinLookRow::kOpacity),
                                       .label = Tr("Opacité"),
                                       .help = Tr("Celle de toute la fenêtre, contenu compris."),
                                       .kind = ro::ui::OptionKind::kSlider,
                                       .value = look_.opacity,
                                       .lowest = kLeastOpacity,
                                       .highest = kMostOpacity,
                                       .format = "%.2f",
                                       .revert = revert,
                                       .revert_help = revert_help,
                                       .at_reference = look_.opacity == theme.opacity});
  rows.push_back(ro::ui::OptionRowView{.id = RowId(SkinLookRow::kRounding),
                                       .label = Tr("Arrondi"),
                                       .help = Tr("Celui du bas des fenêtres et des champs."),
                                       .kind = ro::ui::OptionKind::kSlider,
                                       .value = look_.rounding,
                                       .lowest = kLeastRounding,
                                       .highest = kMostRounding,
                                       .format = "%.0f"});
  for (const ColorRow& color : kColorRows) {
    rows.push_back(ro::ui::OptionRowView{.id = RowId(color.row),
                                         .label = LabelOf(color.row),
                                         .help = HelpOf(color.row),
                                         .kind = ro::ui::OptionKind::kColor,
                                         .color = look_.*color.member,
                                         .revert = revert,
                                         .revert_help = revert_help,
                                         .at_reference = look_.*color.member == theme.*color.member,
                                         .heading = HeadingOf(color.row)});
  }
  rows.push_back(ro::ui::OptionRowView{.id = RowId(SkinLookRow::kReset),
                                       .label = Tr("Réinitialiser l'habillage"),
                                       .kind = ro::ui::OptionKind::kAction});
  // LE PARTAGE : une ligne de texte, par le presse-papiers.
  rows.push_back(ro::ui::OptionRowView{
      .id = RowId(SkinLookRow::kCopyCode),
      .label = Tr("Copier le thème"),
      .help = Tr("Copie une ligne de texte qui porte toutes les couleurs, les teintes et la "
                 "luminosité des images de cet habillage. Un autre joueur la colle dans sa "
                 "page Skin."),
      .kind = ro::ui::OptionKind::kAction});
  rows.push_back(ro::ui::OptionRowView{
      .id = RowId(SkinLookRow::kPasteCode),
      .label = Tr("Coller un thème"),
      .help = Tr("Pose l'habillage dont le presse-papiers porte la ligne. L'arrondi reste le "
                 "tien."),
      .kind = ro::ui::OptionKind::kAction,
      .same_line = true});
  if (paste_refused_) {
    rows.push_back(ro::ui::OptionRowView{.id = RowId(SkinLookRow::kPasteRefused),
                                         .label = Tr("Le presse-papiers ne porte pas de thème."),
                                         .kind = ro::ui::OptionKind::kNote});
  }
}

bool SkinLookOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kSkinLookRowBase || asked.changed >= RowId(SkinLookRow::kCount)) {
    return false;
  }
  const auto row = static_cast<SkinLookRow>(asked.changed - kSkinLookRowBase);
  paste_refused_ = false;
  if (asked.revert) {
    Revert(row);
  } else {
    Apply(row, asked);
  }
  FollowTheme();
  if (file_.has_value()) {
    file_->Touch();
  }
  return true;
}

void SkinLookOptions::Apply(SkinLookRow row, const ro::ui::OptionsWindowAction& asked) {
  if (ImU32* color = ColorOf(row); color != nullptr) {
    *color = asked.color;
    return;
  }
  switch (row) {
    case SkinLookRow::kPreset: {
      const float rounding = look_.rounding;
      const int retouched = static_cast<int>(kPresets.size());
      if (asked.chosen >= 0 && asked.chosen < retouched) {
        SetAsideRetouch();
        look_ = kPresets[static_cast<std::size_t>(asked.chosen)];
        theme_rank_ = asked.chosen;
      } else if (asked.chosen == retouched && set_aside_.has_value() && PresetRank() < retouched) {
        // LA RETOUCHE MISE DE CÔTÉ REVIENT, sous l'arrondi du moment.
        look_ = *set_aside_;
        theme_rank_ = set_aside_rank_;
        set_aside_.reset();
      }
      look_.rounding = rounding;
      break;
    }
    case SkinLookRow::kArtBrightness:
      look_.art_brightness = std::clamp(asked.value, kLeastBrightness, kMostBrightness);
      break;
    case SkinLookRow::kOpacity:
      look_.opacity = std::clamp(asked.value, kLeastOpacity, kMostOpacity);
      break;
    case SkinLookRow::kRounding:
      look_.rounding = std::clamp(asked.value, kLeastRounding, kMostRounding);
      break;
    case SkinLookRow::kReset:
      RestoreDefaults();
      break;
    // ⚠ PAR LE PRESSE-PAPIERS D'IMGUI, donc jamais sans son contexte.
    case SkinLookRow::kCopyCode:
      if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::SetClipboardText(SkinLookCode(look_, theme_rank_).c_str());
      }
      break;
    case SkinLookRow::kPasteCode: {
      const char* const clipboard =
          ImGui::GetCurrentContext() != nullptr ? ImGui::GetClipboardText() : nullptr;
      paste_refused_ = !PasteCode(clipboard != nullptr ? clipboard : "");
      break;
    }
    default:
      break;
  }
}

bool SkinLookOptions::PasteCode(std::string_view code) {
  const std::optional<SharedSkinLook> shared = ReadSkinLookCode(code);
  if (!shared.has_value()) {
    return false;
  }
  SetAsideRetouch();
  const float rounding = look_.rounding;
  look_ = shared->look;
  look_.rounding = rounding;
  look_.art_brightness = std::clamp(look_.art_brightness, kLeastBrightness, kMostBrightness);
  look_.opacity = std::clamp(look_.opacity, kLeastOpacity, kMostOpacity);
  theme_rank_ = shared->theme_rank;
  FollowTheme();
  if (file_.has_value()) {
    file_->Touch();
  }
  return true;
}

void SkinLookOptions::RestoreDefaults() {
  look_ = SkinLook{};
  theme_rank_ = kClassicRank;
  if (file_.has_value()) {
    file_->Touch();
  }
}

void SkinLookOptions::Revert(SkinLookRow row) {
  const SkinLook& theme = Theme();
  if (const ColorRow* color = FindColorRow(row); color != nullptr) {
    look_.*color->member = theme.*color->member;
    return;
  }
  if (row == SkinLookRow::kArtBrightness) {
    look_.art_brightness = theme.art_brightness;
  } else if (row == SkinLookRow::kOpacity) {
    look_.opacity = theme.opacity;
  }
}

}  // namespace moonclient
