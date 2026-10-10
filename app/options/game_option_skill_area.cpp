// LA SECTION « ZONE DES SORTS » : ce que les sorts de zone dessinent au sol.
//
// Aucun client natif ne dessine rien sous nous, et deux choses en découlent :
//
//   · tout est à dessiner, la zone des MONSTRES comprise : l'interrupteur de
//     l'incantation couvre tout le monde, et il est ALLUMÉ, sans quoi on ne
//     verrait jamais ce que le client d'origine montre nativement ;
//   · le joint n'a de sens que sur le carreau plein, et nous le disons par une
//     indentation plutôt que par une infobulle.

#include "options/game_options.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

}  // namespace

void GameOptions::BuildSkillAreaRows() {
  // LES TROIS DESSINS D'UNE CASE. ⚠ UN MEMBRE ET NON UN TABLEAU LOCAL : la
  // rangée n'en garde qu'une VUE, et un local serait mort avant le dessin.
  // C'est aussi ce qui les fait passer par `Tr` — des littéraux figés ne
  // seraient jamais traduits, et rien ne le dirait.
  //
  // ⚠ ET L'ORDRE EST CELUI DE `SkillAreaPattern` : une liste déroulante rend un
  // RANG, et deux ordres qui divergent feraient choisir un motif pour un autre.
  skill_area_pattern_names_.clear();
  skill_area_pattern_names_.push_back(Tr("Équerres (coins de case)"));
  skill_area_pattern_names_.push_back(Tr("Carrelage (cadre de case)"));
  skill_area_pattern_names_.push_back(Tr("Carreau plein"));

  game_rows_.push_back(OptionRowView{
      .id = static_cast<int>(GameOption::kSkillAreaCastShown),
      .label = Tr("Montrer la zone des sorts en cours d'incantation"),
      .help = Tr("Celle que le serveur annonce, la tienne comprise. Tous les sorts n'ont "
                 "pas de zone déclarée : c'est le serveur qui dit lesquels en ont une, et "
                 "de quelle taille."),
      .kind = OptionKind::kToggle,
      .on = skill_area_.cast_shown});
  // ⚠ SOUS UN TITRE, DANS « GAMEPLAY », et non dans une section à elle : c'est
  // un réglage de JEU — ce que les sorts montrent —, pas une surface d'interface
  // qu'on range.
  game_rows_.back().heading = Tr("Zone des sorts");

  game_rows_.push_back(OptionRowView{
      .id = static_cast<int>(GameOption::kSkillAreaArmedShown),
      .label = Tr("Montrer la zone du sort que je tiens en main"),
      .help = Tr("Dès qu'un sort de zone attend son clic au sol, sa zone suit le curseur. "
                 "Personne d'autre ne la voit, et le client d'origine ne sait pas la "
                 "montrer."),
      .kind = OptionKind::kToggle,
      .on = skill_area_.armed_shown});

  game_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kSkillAreaPattern),
                                     .label = Tr("Dessin"),
                                     .kind = OptionKind::kChoice,
                                     .chosen = static_cast<int>(skill_area_.pattern),
                                     .choices = skill_area_pattern_names_});

  // ⚠ SOUS LE DESSIN, ET INDENTÉ : le joint ne veut rien dire tout seul, et il
  // ne fait rien sur les deux motifs qui dessinent un contour — les creuser les
  // rapetisserait au lieu de les espacer.
  game_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kSkillAreaGap),
                    .label = Tr("Joint"),
                    .help = Tr("La part du côté d'une case laissée vide tout autour, en "
                               "pourcentage. Sans effet sur les deux autres dessins."),
                    .kind = OptionKind::kSlider,
                    .value = skill_area_.gap_percent,
                    .lowest = ro::render::kSmallestSkillAreaGapPercent,
                    .highest = ro::render::kLargestSkillAreaGapPercent,
                    .format = "%.0f %%",
                    // ⚠ GRISÉ SUR LES DEUX AUTRES MOTIFS, et non simplement sans
                    // effet : un curseur qui bouge sans rien changer se lit comme
                    // une panne. Ceux-là dessinent un contour, que creuser
                    // rapetisserait au lieu d'espacer.
                    .indented = true,
                    .disabled = skill_area_.pattern != ro::render::SkillAreaPattern::kSolid});

  game_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kSkillAreaColor),
                    .label = Tr("Couleur et opacité"),
                    .help = Tr("L'opacité est le curseur « A » du sélecteur. À zéro, plus "
                               "rien ne se voit — c'est l'interrupteur du dessus qu'il "
                               "vaut mieux décocher."),
                    .kind = OptionKind::kColor,
                    .color = skill_area_.color});
}

}  // namespace moonclient
