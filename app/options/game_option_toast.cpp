// LE PANNEAU DU BANDEAU « OBJET OBTENU ».
//
// 🔴 L'ORDRE ET LES LIBELLÉS SONT GRAVÉS, à la ligne près, et ce ne sont pas des
// synonymes : un joueur cherche « Lignes max » et « Cadre RO », pas « Nombre de
// notifications » et « Cadre du client ». Les renommer pour mieux dire lui
// coûterait ce qu'il sait déjà.
//
// ⚠ PAS DE CASE MAÎTRESSE, et c'est voulu : il n'y a aucune fenêtre native à
// masquer. Le bandeau EST la notification de ramassage, et l'éteindre
// laisserait un ramassage muet.
//
// 🔴 ELLES ONT LEUR PROPRE FICHIER pour tenir `game_option_rows.cpp` sous son
// plafond.
//
// 🔴 LES LIGNES SE REFONT À CHAQUE IMAGE, panneau ouvert : c'est ce qui fait
// qu'un changement de langue se voit à l'image suivante.

#include "ro/core/i18n.h"
#include "ro/ui/item_obtain_toast.h"

#include "options/game_options.h"
#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;

}  // namespace

void GameOptions::ComposeItemObtainRows() {
  item_obtain_rows_.clear();

  item_obtain_rows_.push_back(WholeSlider(
      GameOption::kItemObtainLines, Tr("Lignes max"),
      Tr("Le bandeau d'origine est bloqué à UNE ligne : un second ramassage écrase "
         "le premier."),
      item_obtain_.most_lines, ro::ui::kLeastToastLines, ro::ui::kMostToastLines, "%.0f"));
  item_obtain_rows_.push_back(Toggle(GameOption::kItemObtainNewestOnTop,
                                     Tr("La plus récente en haut"), nullptr, nullptr,
                                     item_obtain_.newest_on_top));
  item_obtain_rows_.push_back(
      Toggle(GameOption::kItemObtainMergeSame, Tr("Regrouper le même item"),
             Tr("Additionne les quantités sur une seule ligne tant qu'elle est affichée, "
                "au lieu d'en ouvrir une par ramassage."),
             nullptr, item_obtain_.merge_same));
  // Clé « Durée (ms) » : elle existe déjà au catalogue, et le format ne répète
  // donc pas l'unité.
  item_obtain_rows_.push_back(WholeSlider(
      GameOption::kItemObtainDurationMs, Tr("Durée (ms)"),
      Tr("Le client d'origine tient 5000 ms, sans réglage possible."), item_obtain_.duration_ms,
      ro::ui::kShortestToastSettingMs, ro::ui::kLongestToastSettingMs, "%.0f"));

  // ── PLACEMENT ─────────────────────────────────────────────────────────────
  //
  // ⚠ PAS DE CURSEUR X/Y : le placement se fait à la souris. Deux mécanismes
  // pour le même geste seraient une hésitation gratuite.
  item_obtain_rows_.push_back(
      Toggle(GameOption::kItemObtainUnlocked, Tr("Déverrouiller (glisser pour déplacer)"),
             Tr("Déverrouillé, une poignée apparaît à l'emplacement du bandeau : glisse-la "
                "où tu veux. Un exemple s'affiche tant que rien n'a été ramassé.\n"
                "Verrouillé, le bandeau ne capte plus aucun clic."),
             nullptr, !item_obtain_.locked));
  item_obtain_rows_.back().heading = Tr("Placement");
  item_obtain_rows_.push_back(
      Toggle(GameOption::kItemObtainCentered, Tr("Centrer horizontalement"),
             Tr("Le centrage survit à un déplacement VERTICAL de la poignée ; un "
                "déplacement horizontal en sort."),
             nullptr, item_obtain_.pos_x < 0));

  // ── COMPACITÉ ─────────────────────────────────────────────────────────────
  item_obtain_rows_.push_back(WholeSlider(GameOption::kItemObtainRowGap, Tr("Interligne"),
                                          Tr("L'espace entre deux lignes. À 0 elles se touchent."),
                                          item_obtain_.row_gap_px, 0,
                                          ro::ui::kMostToastRowGapPixels, "%.0f px"));
  item_obtain_rows_.back().heading = Tr("Compacité");
  item_obtain_rows_.push_back(
      WholeSlider(GameOption::kItemObtainPadVertical, Tr("Marge verticale"),
                  Tr("La marge au-dessus et au-dessous du contenu d'une ligne.\n"
                     "Sans effet tant que le cadre RO est affiché : sa hauteur est imposée par "
                     "la taille de ses tuiles."),
                  item_obtain_.pad_vertical_px, 0, ro::ui::kMostToastPadVerticalPixels, "%.0f px"));
  item_obtain_rows_.push_back(WholeSlider(GameOption::kItemObtainFontScale, Tr("Taille police"),
                                          nullptr, item_obtain_.font_scale_percent,
                                          ro::ui::kLeastToastFontScalePercent,
                                          ro::ui::kMostToastFontScalePercent, "%.0f %%"));

  // ── APPARENCE ─────────────────────────────────────────────────────────────
  item_obtain_rows_.push_back(Toggle(GameOption::kItemObtainIcon, Tr("Afficher l'icône"), nullptr,
                                     nullptr, item_obtain_.show_icon));
  item_obtain_rows_.back().heading = Tr("Apparence");
  item_obtain_rows_.push_back(
      Toggle(GameOption::kItemObtainFrame, Tr("Cadre RO"),
             Tr("Le cadre clair du client. Décoché, le fond libre ci-dessous prend sa place."),
             nullptr, item_obtain_.show_frame));

  // Le fond libre ne sert QUE sans le cadre RO : le griser dit pourquoi il ne
  // fait rien, au lieu de laisser croire à un réglage cassé.
  const bool framed = item_obtain_.show_frame;
  item_obtain_rows_.push_back(Toggle(GameOption::kItemObtainBackground, Tr("Fond libre"), nullptr,
                                     nullptr, item_obtain_.background_shown));
  item_obtain_rows_.back().disabled = framed;
  item_obtain_rows_.push_back(Swatch(GameOption::kItemObtainBackgroundColor, Tr("Couleur du fond"),
                                     nullptr, item_obtain_.background));
  item_obtain_rows_.back().disabled = framed;
  item_obtain_rows_.push_back(WholeSlider(GameOption::kItemObtainBackgroundAlpha,
                                          Tr("Opacité du fond"), nullptr,
                                          ro::ui::ColorAlphaPercent(item_obtain_.background), 0,
                                          ro::ui::kWholeAlphaPercent, "%.0f %%"));
  item_obtain_rows_.back().disabled = framed;
  item_obtain_rows_.push_back(WholeSlider(GameOption::kItemObtainRounding, Tr("Arrondi"), nullptr,
                                          item_obtain_.rounding_px, 0,
                                          ro::ui::kMostToastRoundingPixels, "%.0f px"));
  item_obtain_rows_.back().disabled = framed;

  item_obtain_rows_.push_back(Toggle(GameOption::kItemObtainBorder, Tr("Bordure"), nullptr, nullptr,
                                     item_obtain_.border_shown));
  item_obtain_rows_.back().disabled = framed;
  // La bordure grise ce qui la décrit, en plus du cadre : deux raisons de ne
  // rien faire, et le panneau les dit toutes les deux.
  const bool no_border = framed || !item_obtain_.border_shown;
  item_obtain_rows_.push_back(Swatch(GameOption::kItemObtainBorderColor,
                                     Tr("Couleur de la bordure"), nullptr, item_obtain_.border));
  item_obtain_rows_.back().disabled = no_border;
  item_obtain_rows_.push_back(WholeSlider(
      GameOption::kItemObtainBorderAlpha, Tr("Opacité de la bordure"), nullptr,
      ro::ui::ColorAlphaPercent(item_obtain_.border), 0, ro::ui::kWholeAlphaPercent, "%.0f %%"));
  item_obtain_rows_.back().disabled = no_border;
  item_obtain_rows_.push_back(
      WholeSlider(GameOption::kItemObtainBorderThickness, Tr("Épaisseur de la bordure"), nullptr,
                  item_obtain_.border_px, 1, ro::ui::kMostToastBorderPixels, "%.0f px"));
  item_obtain_rows_.back().disabled = no_border;

  // ── COULEURS ──────────────────────────────────────────────────────────────
  item_obtain_rows_.push_back(
      Swatch(GameOption::kItemObtainTextColor, Tr("Couleur du nom"), nullptr, item_obtain_.text));
  item_obtain_rows_.back().heading = Tr("Couleurs");
  item_obtain_rows_.push_back(Swatch(GameOption::kItemObtainQuantityColor,
                                     Tr("Couleur de la quantité"), nullptr, item_obtain_.quantity));
}

void GameOptions::NoteItemObtainAnchor(int pos_x, int pos_y) noexcept {
  item_obtain_.pos_x = pos_x;
  item_obtain_.pos_y = pos_y;
}

}  // namespace moonclient
