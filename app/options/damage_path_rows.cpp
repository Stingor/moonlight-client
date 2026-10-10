// LA TRAJECTOIRE DES DÉGÂTS : le volet de droite, et les rangées qu'elle ajoute
// à l'onglet « Allure ». Ses ancres, et ce qu'elles changent, sont dans
// `damage_path_anchors.cpp`.
//
// ⚠ À PART DES AUTRES RANGÉES DES DÉGÂTS, pour le plafond, et parce qu'elles
// règlent un objet à part : la trajectoire du type montré, et non le réglage
// commun.

#include "options/damage_path_rows.h"

#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/render/number_look.h"

#include "options/damage_path_anchors.h"
#include "options/option_row_kinds.h"
#include "world/number_paths.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les rangées qui ne sont pas des réglages : le cadre, et la note qui le dit.
// ⚠ HORS DE `GameOption`, et négatifs : la fenêtre ne rend jamais leur
// identifiant — le cadre rend celui de l'ancre qu'on tire.
constexpr int kPathCanvasRow = -2;
constexpr int kPathNoteRow = -3;

}  // namespace

void PathPreviewRows(const HitPopupLook& look, const DamagePageNames& names,
                     const ro::ui::PathSample* sample, bool anchored,
                     std::vector<OptionRowView>& side) {
  const DamagePath shown = DamagePathOf(look.path_shown);
  side.push_back(Choice(GameOption::kDamagePathShown, Tr("Type de dégât"),
                        Tr("Le type dont le cadre fait sortir les nombres, et dont on règle "
                           "la trajectoire. Chaque type a la sienne."),
                        names.paths, static_cast<int>(shown)));
  side.push_back(Measure(GameOption::kDamageRehearsalStep,
                         Tr("Intervalle entre deux coups d'essai"),
                         Tr("Le temps entre deux coups factices, dans ce volet comme dans "
                            "l'aperçu en boucle. Plus court, on voit plus vite ce qu'un réglage "
                            "change."),
                         look.rehearsal_interval_ms, kShortestRehearsalIntervalMs,
                         kLongestRehearsalIntervalMs, "%.0f ms"));
  side.push_back(Measure(GameOption::kDamagePreviewFloor, Tr("Hauteur de la cible dans le cadre"),
                         Tr("La place gardée sous ses pieds : de quoi la remonter quand une "
                            "trajectoire descend sous le bas du cadre."),
                         look.preview_floor_pixels, 0.0F, kLargestPreviewFloorPixels, "%.0f px"));
  if (anchored) {
    side.push_back(Measure(GameOption::kDamagePreviewGrid, Tr("Pas de la grille"),
                           Tr("L'écart entre deux lignes de la grille, en pixels de sprite depuis "
                              "le point de naissance : les ancres s'y aimantent, sauf sous Maj. "
                              "À zéro, ni grille ni aimant."),
                           look.preview_grid_pixels, 0.0F, kLargestPreviewGridPixels, "%.0f px"));
  }
  // OÙ LE MENU DU CADRE COPIE : chaque type à son rang, puis tous à la fois —
  // voir `CopyPathPart`.
  std::vector<const char*> copy_targets = names.paths;
  copy_targets.push_back(Tr("Tous les autres types"));
  OptionRowView canvas{.id = kPathCanvasRow, .kind = OptionKind::kCanvas};
  // 🔴 LE CADRE SE LIT PAR SON ADRESSE, au moment de peindre : la partie le
  // recompose à chaque image, après cette page. Les ancres, elles, se copient
  // — elles ne changent qu'avec le réglage.
  //
  // ⚠ LE CHEMIN DE L'ALLURE EST CELUI DE L'IMAGE PRÉCÉDENTE : la page se compose
  // avant le cadre, et une image de retard ne se voit pas.
  AllureShift allure;
  if (sample != nullptr) {
    allure.one = ro::ui::AllureAt(*sample, ro::render::kFirstHandleShare);
    allure.two = ro::ui::AllureAt(*sample, ro::render::kSecondHandleShare);
    allure.end = ro::ui::AllureAt(*sample, 1.0F);
  }
  std::vector<ro::ui::PathAnchor> anchors;
  if (anchored) {
    anchors = PathAnchorsOf(PathOf(look, shown), allure);
  }
  canvas.edit = [sample, trace = PathTraceOf(PathOf(look, shown)), anchors = std::move(anchors),
                 anchored, allure, floor = look.preview_floor_pixels,
                 grid = look.preview_grid_pixels, copy_targets = std::move(copy_targets),
                 shown](ImDrawList& into, ImVec2 min, ImVec2 max) {
    std::optional<ro::ui::OptionCanvasMove> moved = ro::ui::DrawPathCanvas(
        into, min, max,
        ro::ui::PathCanvasView{
            .sample = sample,
            .trace = trace,
            .anchors = anchors,
            .traced = anchored,
            .floor_pixels = floor,
            .grid_pixels = grid,
            .copy_targets = anchored ? std::span<const char* const>(copy_targets)
                                     : std::span<const char* const>(),
            .copy_skipped = static_cast<int>(shown),
            .whole_path = anchored ? static_cast<int>(GameOption::kDamagePathCopy) : -1});
    // L'ANCRE REVIENT À LA TRAJECTOIRE SEULE : ce que l'allure y ajoutait
    // n'est pas un réglage. Une copie, elle, n'a pas de point.
    if (moved.has_value() && moved->copy_to < 0) {
      const ImVec2 shift = AllureShiftOf(static_cast<GameOption>(moved->anchor), allure);
      moved->point = ImVec2(moved->point.x - shift.x, moved->point.y - shift.y);
    }
    return moved;
  };
  side.push_back(std::move(canvas));
}

void PathTypeRows(const HitPopupLook& look, std::vector<OptionRowView>& rows) {
  const DamagePath shown = DamagePathOf(look.path_shown);
  const NumberPath& path = PathOf(look, shown);
  rows.push_back(OptionRowView{
      .id = kPathNoteRow,
      .label = Tr("Tirez les ancres dans le volet de droite : le rond vert est le départ, la croix "
                  "orange l'arrivée, les losanges bleus les poignées de la courbe, les carrés les "
                  "rayons de la dispersion. Le tracé suit le chemin réel des nombres ; le cercle "
                  "pâle marque où le client les fait naître."),
      .kind = OptionKind::kNote});

  // ── SA VIE ────────────────────────────────────────────────────────────────
  // ⚠ LES MOTS N'EN ONT PAS : « Miss » et la bulle vivent ce que leur animation
  // dure.
  if (const std::optional<ro::render::NumberLife> life = PathLifeOf(look, shown);
      life.has_value()) {
    rows.push_back(Measure(GameOption::kDamageBirthScale, Tr("Taille à la naissance"),
                           Tr("En pour cent de sa taille finale : au-dessus de cent, le nombre "
                              "naît grand et rétrécit ; en dessous, il naît petit et grandit. Le "
                              "client fait jaillir ses coups à plus de 400 %, et naître son total "
                              "au sixième de sa taille."),
                           life->birth_scale * kPercent, ro::render::kSmallestBirthScale * kPercent,
                           ro::render::kLargestBirthScale * kPercent, "%.0f %%"));
    rows.back().heading = Tr("Sa vie");
    rows.push_back(Measure(GameOption::kDamageBirthSettle, Tr("Retour à la taille finale"),
                           Tr("Le temps qu'il met à rejoindre sa taille finale, d'un pas régulier. "
                              "À zéro, il naît directement à sa taille finale."),
                           life->birth_settle_ms, 0.0F, ro::render::kLongestBirthSettleMs,
                           "%.0f ms"));
    rows.back().indented = true;
    rows.push_back(Measure(GameOption::kDamageFadeStart, Tr("Début du fondu"),
                           Tr("Jusque-là, le nombre reste opaque ; il s'efface ensuite d'un pas "
                              "régulier jusqu'à sa disparition. Le client fait pâlir ses coups "
                              "dès leur naissance, et son total dans sa dernière demi-seconde."),
                           life->fade_start_ms, 0.0F, ro::render::kLongestNumberLifeMs, "%.0f ms"));
    rows.push_back(Measure(GameOption::kDamageLifeSpan, Tr("Durée de vie"),
                           Tr("Quand le nombre disparaît. Il parcourt sa trajectoire en ce temps, "
                              "et sa dérive de côté avec."),
                           life->life_ms, ro::render::kShortestNumberLifeMs,
                           ro::render::kLongestNumberLifeMs, "%.0f ms"));
  }

  // ── SON MOUVEMENT, hors des ancres ─────────────────────────────────────────
  rows.push_back(
      Toggle(GameOption::kDamageFacingDrift, Tr("Dérive de côté"),
             Tr("Le nombre glisse en diagonale dans le monde, du côté où regarde la cible, en "
                "plus de sa trajectoire. Le client le fait pour les coups qui jaillissent."),
             nullptr, path.facing_drift));
  rows.back().heading = Tr("Son mouvement");
  rows.push_back(
      Toggle(GameOption::kDamageScatter, Tr("Dispersion aléatoire"),
             Tr("Chaque nombre arrive à un point tiré au hasard autour du point d'arrivée, "
                "dans les deux rayons que tirent les carrés du volet : deux coups rapprochés ne "
                "finissent plus au même pixel, et chacun décrit son propre arc."),
             nullptr, path.scatter));
  rows.push_back(OptionRowView{
      .id = static_cast<int>(GameOption::kDamagePathClient),
      .label = Tr("Ce type comme le client"),
      .help = Tr("Rend à ce type de dégât sa trajectoire et sa vie d'origine. Les autres types "
                 "gardent les leurs."),
      .kind = OptionKind::kAction,
      .small_button = true});
}

}  // namespace moonclient
