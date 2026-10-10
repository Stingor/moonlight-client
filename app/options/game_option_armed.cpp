// LE SORT EN MAIN : ce qu'il montre tant qu'il attend sa cible.
//
// 🔴 RANGÉ AVEC LES BARRES DE RACCOURCIS, et ce n'est pas un pis-aller : c'est
// d'une case de barre qu'on arme neuf fois sur dix, et c'est donc là qu'on
// cherche « pourquoi ce chiffre à côté de mon curseur ». La zone au sol, elle,
// reste dans le jeu — elle décrit ce que le SORT fait, pas ce que l'interface
// montre.
//
// ⚠ COMMUNES AUX HUIT BARRES, donc au-dessus des onglets : voir
// `shortcut_shared_rows_`. Posées dessous, elles se liraient comme les réglages
// de la barre ouverte, et rien dans leur libellé ne pourrait détromper.

#include "ro/render/armed_skill.h"

#include "options/game_option_ids.h"
#include "options/game_options.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

}  // namespace

void GameOptions::ComposeArmedSkillRows() {
  shortcut_shared_rows_.push_back(
      OptionRowView{.id = kArmedLevelShownRow,
                    .label = Tr("Montrer le rang du sort en main, au curseur"),
                    .help = Tr("Les chiffres que le client d'origine colle à droite du "
                               "curseur dès qu'un sort attend sa cible. C'est le rang qui "
                               "partira, et la molette le règle."),
                    .kind = OptionKind::kToggle,
                    .on = armed_skill_.level_shown});
  shortcut_shared_rows_.back().heading = Tr("Sort en main");

  shortcut_shared_rows_.push_back(OptionRowView{.id = kArmedLevelScaleRow,
                                                .label = Tr("Taille des chiffres"),
                                                .kind = OptionKind::kSlider,
                                                .value = armed_skill_.level_scale,
                                                .lowest = ro::render::kSmallestArmedLevelScale,
                                                .highest = ro::render::kLargestArmedLevelScale,
                                                .format = "×%.1f",
                                                .indented = true,
                                                .disabled = !armed_skill_.level_shown});
  shortcut_shared_rows_.push_back(
      OptionRowView{.id = kArmedLevelOffsetRow,
                    .label = Tr("Écart au curseur"),
                    .help = Tr("Vers la droite, en pixels. Négatif, les chiffres passent "
                               "à gauche de la pointe."),
                    .kind = OptionKind::kSlider,
                    .value = armed_skill_.level_offset_x_pixels,
                    .lowest = ro::render::kSmallestArmedLevelOffsetPixels,
                    .highest = ro::render::kLargestArmedLevelOffsetPixels,
                    .format = "%.0f px",
                    .indented = true,
                    .disabled = !armed_skill_.level_shown});

  shortcut_shared_rows_.push_back(
      OptionRowView{.id = kArmedBannerShownRow,
                    .label = Tr("Montrer l'étiquette du sort en main"),
                    .help = Tr("Le nom du sort, en haut de l'écran, tant qu'il attend sa "
                               "cible. Le client d'origine la montre aussi."),
                    .kind = OptionKind::kToggle,
                    .on = armed_skill_.banner_shown});
  shortcut_shared_rows_.push_back(
      OptionRowView{.id = kArmedBannerLevelRow,
                    .label = Tr("Y écrire aussi le rang"),
                    .help = Tr("Le client d'origine ne met que le nom : son rang ne se lit "
                               "qu'au curseur. À décocher pour ne pas l'avoir deux fois."),
                    .kind = OptionKind::kToggle,
                    .on = armed_skill_.banner_level,
                    .indented = true,
                    .disabled = !armed_skill_.banner_shown});
  shortcut_shared_rows_.push_back(
      OptionRowView{.id = kArmedBannerHeightRow,
                    .label = Tr("Hauteur de l'étiquette"),
                    .help = Tr("En pourcentage de la hauteur de la fenêtre, depuis le haut. "
                               "Le client d'origine pose la sienne à un peu moins de dix."),
                    .kind = OptionKind::kSlider,
                    .value = armed_skill_.banner_height_percent,
                    .lowest = ro::render::kSmallestArmedBannerHeightPercent,
                    .highest = ro::render::kLargestArmedBannerHeightPercent,
                    .format = "%.1f %%",
                    .indented = true,
                    .disabled = !armed_skill_.banner_shown});
  shortcut_shared_rows_.push_back(
      OptionRowView{.id = kArmedBannerColorRow,
                    .label = Tr("Couleur de l'étiquette"),
                    .help = Tr("L'opacité est le curseur « A » du sélecteur. À zéro, plus "
                               "rien ne se voit — c'est l'interrupteur du dessus qu'il "
                               "vaut mieux décocher."),
                    .kind = OptionKind::kColor,
                    .color = armed_skill_.banner_color,
                    .indented = true,
                    .disabled = !armed_skill_.banner_shown});
}

bool GameOptions::ChangeArmedSkill(const ro::ui::OptionsWindowAction& asked) {
  switch (asked.changed) {
    case kArmedLevelShownRow:
      armed_skill_.level_shown = asked.on;
      return true;
    case kArmedLevelScaleRow:
      armed_skill_.level_scale = asked.value;
      return true;
    case kArmedLevelOffsetRow:
      armed_skill_.level_offset_x_pixels = asked.value;
      return true;
    case kArmedBannerShownRow:
      armed_skill_.banner_shown = asked.on;
      return true;
    case kArmedBannerLevelRow:
      armed_skill_.banner_level = asked.on;
      return true;
    case kArmedBannerHeightRow:
      armed_skill_.banner_height_percent = asked.value;
      return true;
    case kArmedBannerColorRow:
      armed_skill_.banner_color = asked.color;
      return true;
    default:
      return false;
  }
}

void GameOptions::DeclareArmedSkill(SettingsFile& into) {
  into.Declare("spell_in_hand_rank", &armed_skill_.level_shown);
  into.Declare("spell_in_hand_rank_size", &armed_skill_.level_scale);
  into.Declare("spell_in_hand_rank_gap", &armed_skill_.level_offset_x_pixels);
  into.Declare("spell_in_hand_label", &armed_skill_.banner_shown);
  into.Declare("spell_in_hand_label_rank", &armed_skill_.banner_level);
  into.Declare("spell_in_hand_label_height", &armed_skill_.banner_height_percent);
  into.Declare("spell_in_hand_label_color", &armed_skill_.banner_color);
}

}  // namespace moonclient
