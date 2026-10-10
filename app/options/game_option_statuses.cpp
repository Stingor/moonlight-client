// LES RANGEES DE LA BARRE D'ETATS.
//
// ⚠ À PART DU CATALOGUE, et c'est le plafond de huit cents lignes qui l'a
// decide — mais la couture suit la frontiere que `app/options` emploie deja
// pour les jauges, les barres de raccourcis, les degats et les objets : une
// FAMILLE par fichier.
//
// 🔴 QUINZE REGLAGES EN COLONNE NE SE LISENT PAS. Trois intertitres, les
// dependants indentes sous leur maitre, et le reste GRISE quand la barre est
// eteinte : c'est ce qui separe un formulaire d'une liste.

#include <array>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/status_bar.h"

#include "options/game_options.h"
#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// LES TROIS LISTES DE LA BARRE D'ÉTATS, REFAITES À CHAQUE IMAGE.
//
// 🔴 ET SURTOUT PAS DANS UN `static`. Le pointeur que `Tr` rend appartient au
// catalogue et MEURT au changement de langue — `ro/core/i18n.h` le dit à la
// ligne où il le rend. Une liste gardée une fois pour toutes montrerait donc de
// la mémoire libérée dès que le joueur change de langue, et ce n'est pas un
// affichage fautif : c'est une lecture après libération.
//
// ⚠ ET LEUR ORDRE EST CELUI DES ÉNUMÉRATIONS DE `ro::ui`, qui est celui du
// fichier de réglages : un libellé intercalé décalerait tout ce qu'un joueur a
// déjà choisi.
void FillStatusChoices(std::vector<const char*>& anchors, std::vector<const char*>& sorts,
                       std::vector<const char*>& sweeps) {
  anchors.clear();
  anchors.push_back(Tr("Haut gauche"));
  anchors.push_back(Tr("Haut droite"));
  anchors.push_back(Tr("Bas gauche"));
  anchors.push_back(Tr("Bas droite"));

  sorts.clear();
  sorts.push_back(Tr("Ordre d'arrivée"));
  sorts.push_back(Tr("Bientôt fini d'abord"));
  sorts.push_back(Tr("Plus long d'abord"));

  sweeps.clear();
  sweeps.push_back(Tr("Aucun"));
  sweeps.push_back(Tr("Balayage horaire"));
  sweeps.push_back(Tr("Voile descendant"));
}

}  // namespace

void GameOptions::ComposeStatusBarRows() {
  // ── LA BARRE D'ÉTATS ──────────────────────────────────────────────────────
  //
  // ⚠ SA PROPRE SECTION, contre celle des barres de raccourcis : c'est l'autre
  // rangée posée SUR LE MONDE, et les deux se règlent l'une en regardant
  // l'autre.
  //
  // 🔴 QUINZE RÉGLAGES EN COLONNE NE SE LISENT PAS. Trois intertitres, les
  // dépendants indentés sous leur maître, et le reste GRISÉ quand la barre est
  // éteinte : c'est ce qui sépare un formulaire d'une liste.
  FillStatusChoices(status_anchor_names_, status_sort_names_, status_sweep_names_);
  const bool bar_off = !status_bar_.shown;
  status_rows_.clear();
  status_rows_.push_back(
      Toggle(GameOption::kStatusBarShown, Tr("Montrer la barre d'états"),
             Tr("Les buffs et les altérations en cours, avec leur durée. Éteinte, "
                "rien ne les montre : ce client n'a pas d'autre barre."),
             nullptr, status_bar_.shown));
  // ⚠ L'APERÇU NE SE GARDE PAS d'une session à l'autre : c'est un outil de
  // réglage, pas un mode de jeu.
  status_rows_.push_back(Toggle(GameOption::kStatusBarPreview, Tr("Aperçu (faux états)"),
                                Tr("Remplit la barre d'états réels de l'installation, aux durées "
                                   "étagées, le temps de la régler — sans attendre d'en avoir de "
                                   "vrais.\n\nNe se garde pas d'une session à l'autre."),
                                nullptr, status_preview_));
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(
      Toggle(GameOption::kStatusBarLocked, Tr("Verrouiller la position"),
             Tr("Verrouillée, la barre est figée et laisse passer les clics au jeu "
                "en dessous.\n\nMaintenez MAJ au-dessus d'elle pour la saisir sans "
                "la déverrouiller. Son emprise entière apparaît alors, bien plus "
                "large que ses icônes."),
             nullptr, status_bar_.locked));
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(
      Toggle(GameOption::kStatusBarBorder, Tr("Filet"), nullptr, nullptr, status_bar_.border));
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kStatusBarBackground),
                                       .label = Tr("Fond"),
                                       .help = Tr("Transparence comprise : un fond éteint laisse "
                                                  "les icônes seules sur le monde."),
                                       .kind = OptionKind::kColor,
                                       .color = status_bar_.background_color,
                                       .disabled = bar_off});

  status_rows_.push_back(Measure(GameOption::kStatusBarSide, Tr("Taille des icônes"), nullptr,
                                 status_bar_.side_px, ro::ui::kSmallestStatusSidePixels,
                                 ro::ui::kLargestStatusSidePixels, "%.0f px"));
  status_rows_.back().heading = Tr("Icônes");
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(Measure(GameOption::kStatusBarGap, Tr("Espacement"), nullptr,
                                 status_bar_.gap_px, 0.0F, ro::ui::kLargestStatusGapPixels,
                                 "%.0f px"));
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(
      Measure(GameOption::kStatusBarMost, Tr("Nombre maximum"),
              Tr("Au-delà, les états en trop sont écartés — ce sont les DERNIERS du "
                 "rangement ci-dessous, qui décide donc de ce qu'on perd."),
              static_cast<float>(status_bar_.most), static_cast<float>(ro::ui::kFewestStatusIcons),
              static_cast<float>(ro::ui::kMostStatusIcons), "%.0f"));
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(Measure(GameOption::kStatusBarRows, Tr("Lignes"),
                                 Tr("Le plafond ci-dessus réparti sur autant de lignes : une barre "
                                    "haute et étroite plutôt que longue et plate."),
                                 static_cast<float>(status_bar_.rows), 1.0F,
                                 static_cast<float>(ro::ui::kMostStatusRows), "%.0f"));
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(
      Choice(GameOption::kStatusBarAnchor, Tr("Ancrage"),
             Tr("Le coin d'où les icônes se rangent, et donc le sens dans lequel la "
                "barre grandit."),
             status_anchor_names_, status_bar_.anchor));
  status_rows_.back().help_warning =
      Tr("Ancrée du mauvais côté, la rangée RECULE quand un état tombe et toutes les icônes "
         "glissent. Une barre posée à droite de l'écran veut pousser vers la gauche, celle du bas "
         "vers le haut.");
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(
      Choice(GameOption::kStatusBarSort, Tr("Rangement"),
             Tr("« Bientôt fini d'abord » met en tête ce qu'il faudra relancer."),
             status_sort_names_, status_bar_.sort));
  status_rows_.back().help_warning =
      Tr("Un état sans durée n'a pas de place dans un tri par durée : il passe en dernier sous "
         "« bientôt fini », et en premier sous « plus long » — rien ne dure plus que ce qui ne "
         "finit pas.");
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(Measure(GameOption::kStatusBarOpacity, Tr("Opacité des icônes"), nullptr,
                                 status_bar_.opacity * kPercent,
                                 ro::ui::kFaintestStatusOpacity * kPercent, kPercent, "%.0f %%"));
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(OptionRowView{
      .id = static_cast<int>(GameOption::kStatusBarIconBackground),
      .label = Tr("Fond des icônes"),
      .help =
          Tr("Posé derrière chaque icône, transparence comprise. Les icônes d'état sont en partie "
             "translucides : noir et opaque, elles restent lisibles sur tout "
             "décor ; transparent, le décor passe à travers."),
      .kind = OptionKind::kColor,
      .color = status_bar_.icon_background,
      .disabled = bar_off});

  status_rows_.push_back(Toggle(GameOption::kStatusBarCountdown, Tr("Temps restant sous l'icône"),
                                nullptr, nullptr, status_bar_.show_countdown));
  status_rows_.back().heading = Tr("Écoulement");
  status_rows_.back().disabled = bar_off;
  status_rows_.push_back(Measure(
      GameOption::kStatusBarCountdownSize, Tr("Taille du texte"), nullptr, status_bar_.countdown_px,
      ro::ui::kSmallestStatusCountdownPixels, ro::ui::kLargestStatusCountdownPixels, "%.0f px"));
  status_rows_.back().indented = true;
  status_rows_.back().disabled = bar_off || !status_bar_.show_countdown;
  // ⚠ SON FOND EST UN RÉGLAGE À PART ENTIÈRE : sous l'icône, les chiffres
  // tombent sur le MONDE — une herbe claire, un mur blanc — et le cerné ne
  // suffit pas toujours.
  status_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kStatusBarCountdownBackground),
                    .label = Tr("Fond du texte"),
                    .help = Tr("Transparence comprise. Sous l'icône, le temps restant se pose "
                               "sur le décor : un fond discret le rend lisible partout."),
                    .kind = OptionKind::kColor,
                    .color = status_bar_.countdown_background,
                    .indented = true,
                    .disabled = bar_off || !status_bar_.show_countdown});
  status_rows_.push_back(
      Choice(GameOption::kStatusBarSweep, Tr("Grisage de la case"),
             Tr("La case s'assombrit à mesure que l'état s'écoule : on voit d'un "
                "coup d'œil ce qui est près de tomber."),
             status_sweep_names_, status_bar_.sweep));
  status_rows_.back().help_warning =
      Tr("Un état sans échéance n'est jamais voilé — il n'a pas de part écoulée, et le griser à "
         "moitié le ferait croire à mi-course.");
  status_rows_.back().disabled = bar_off;
  // Le nuancier EN DERNIER : son sélecteur s'ouvre vers le bas.
  status_rows_.push_back(OptionRowView{
      .id = static_cast<int>(GameOption::kStatusBarSweepColor),
      .label = Tr("Couleur du voile"),
      .kind = OptionKind::kColor,
      .color = status_bar_.sweep_color,
      .indented = true,
      .disabled =
          bar_off || ro::ui::StatusSweepOf(status_bar_.sweep) == ro::ui::StatusSweep::kNone});
}

}  // namespace moonclient
