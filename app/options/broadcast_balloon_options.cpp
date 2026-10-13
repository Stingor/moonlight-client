#include "options/broadcast_balloon_options.h"

#include <algorithm>

#include "ro/core/i18n.h"
#include "ro/ui/item_obtain_toast.h"

#include "options/bubble_samples.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : en changer une rend à tous les joueurs le
// réglage d'origine, sans que rien ne le dise.
constexpr const char* kShownKey = "broadcast_balloons_shown";
constexpr const char* kDurationKey = "broadcast_balloons_duration_ms";
constexpr const char* kMostKey = "broadcast_balloons_most_per_family";
constexpr const char* kFadeKey = "broadcast_balloons_fade";
constexpr const char* kFadeMsKey = "broadcast_balloons_fade_ms";
constexpr const char* kFontScaleKey = "broadcast_balloons_font_scale";
constexpr const char* kWrapKey = "broadcast_balloons_wrap_px";
constexpr const char* kBackgroundKey = "broadcast_balloons_background";
constexpr const char* kBorderKey = "broadcast_balloons_border";
constexpr const char* kTopLockedKey = "broadcast_balloons_locked";
constexpr const char* kTopCenterKey = "broadcast_balloons_center_x";
constexpr const char* kTopYKey = "broadcast_balloons_top_y";
constexpr const char* kHuntLockedKey = "hunt_balloons_locked";
constexpr const char* kHuntCenterKey = "hunt_balloons_center_x";
constexpr const char* kHuntYKey = "hunt_balloons_top_y";

[[nodiscard]] constexpr int RowId(BalloonRow row) {
  return kBalloonRowBase + static_cast<int>(row);
}

[[nodiscard]] OptionRowView Toggle(BalloonRow row, const char* label, const char* help, bool on) {
  return OptionRowView{.id = RowId(row), .label = label, .help = help, .on = on};
}

[[nodiscard]] OptionRowView Steps(BalloonRow row, const char* label, const char* help, int value,
                                  int lowest, int highest, const char* format) {
  return OptionRowView{.id = RowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kSlider,
                       .value = static_cast<float>(value),
                       .lowest = static_cast<float>(lowest),
                       .highest = static_cast<float>(highest),
                       .format = format};
}

[[nodiscard]] OptionRowView Swatch(BalloonRow row, const char* label, ImU32 color) {
  return OptionRowView{
      .id = RowId(row), .label = label, .kind = OptionKind::kColor, .color = color};
}

[[nodiscard]] OptionRowView Button(BalloonRow row, const char* label, const char* help) {
  return OptionRowView{.id = RowId(row), .label = label, .help = help, .kind = OptionKind::kAction};
}

// Une valeur de curseur, entière et bornée.
[[nodiscard]] int Stepped(float value, int lowest, int highest) {
  return std::clamp(static_cast<int>(value + 0.5F), lowest, highest);
}

}  // namespace

void BroadcastBalloonOptions::Declare(SettingsFile& into) {
  into.Declare(kShownKey, &look_.shown);
  into.Declare(kDurationKey, &look_.duration_ms);
  into.Declare(kMostKey, &look_.most_per_family);
  into.Declare(kFadeKey, &look_.fade);
  into.Declare(kFadeMsKey, &look_.fade_ms);
  into.Declare(kFontScaleKey, &look_.font_scale_percent);
  into.Declare(kWrapKey, &look_.wrap_px);
  into.Declare(kBackgroundKey, &look_.background);
  into.Declare(kBorderKey, &look_.border);
  into.Declare(kTopLockedKey, &look_.announcements.locked);
  into.Declare(kTopCenterKey, &look_.announcements.center_x);
  into.Declare(kTopYKey, &look_.announcements.top_y);
  into.Declare(kHuntLockedKey, &look_.hunts.locked);
  into.Declare(kHuntCenterKey, &look_.hunts.center_x);
  into.Declare(kHuntYKey, &look_.hunts.top_y);
}

void BroadcastBalloonOptions::Compose() {
  pinned_rows_.clear();
  pinned_rows_.push_back(BroadcastBalloonSampleRow(look_));
  std::vector<OptionRowView>& rows = rows_;
  rows.clear();
  rows.push_back(Toggle(BalloonRow::kShown, Tr("Bulles d'annonce en haut de l'écran"),
                        Tr("Les annonces du serveur, l'avancée des chasses de quête et ce que "
                           "l'équipe chuchote se posent aussi en haut de l'écran, comme dans le "
                           "jeu. Éteintes, elles ne vivent plus que dans la conversation."),
                        look_.shown));
  const bool off = !look_.shown;
  const auto added = [&rows, off](OptionRowView row) {
    row.disabled = off;
    rows.push_back(row);
  };
  added(Steps(BalloonRow::kDurationMs, Tr("Durée (ms)"),
              Tr("Le jeu tient une bulle 20000 ms, sans réglage possible."), look_.duration_ms,
              ro::ui::kShortestBalloonMs, ro::ui::kLongestBalloonMs, "%.0f"));
  added(Steps(BalloonRow::kMostPerFamily, Tr("Bulles à la fois"),
              Tr("Au-delà, la plus ancienne part pour la nouvelle. Le jeu en garde trois. "
                 "L'avancée des chasses n'en montre toujours qu'une : chacune remplace la "
                 "précédente."),
              look_.most_per_family, ro::ui::kLeastBalloonsPerFamily,
              ro::ui::kMostBalloonsPerFamily, "%.0f"));
  added(Toggle(BalloonRow::kFade, Tr("Fondu de sortie"),
               Tr("La bulle s'efface en fin de vie au lieu de disparaître d'un coup comme "
                  "dans le jeu."),
               look_.fade));
  OptionRowView fade_ms = Steps(BalloonRow::kFadeMs, Tr("Durée du fondu (ms)"), nullptr,
                                look_.fade_ms, 0, ro::ui::kLongestBalloonFadeMs, "%.0f");
  fade_ms.indented = true;
  added(fade_ms);
  rows.back().disabled = off || !look_.fade;
  added(Steps(BalloonRow::kFontScale, Tr("Taille police"),
              Tr("En pourcentage du corps du jeu : douze pixels pour une annonce, quatorze "
                 "pour l'avis d'étoile, et ce que l'annonce colorée demande."),
              look_.font_scale_percent, ro::ui::kLeastBalloonFontPercent,
              ro::ui::kMostBalloonFontPercent, "%.0f %%"));
  added(Steps(BalloonRow::kWrapPx, Tr("Largeur avant retour à la ligne"),
              Tr("Une annonce simple plus large se replie. Le jeu la coupe tous les 72 "
                 "octets, ce qui coupe un caractère accentué en deux ; ce client replie à la "
                 "largeur, et 380 pixels valent ses 72 caractères."),
              look_.wrap_px, ro::ui::kLeastBalloonWrapPixels, ro::ui::kMostBalloonWrapPixels,
              "%.0f px"));
  added(Swatch(BalloonRow::kBackgroundColor, Tr("Couleur du fond"), look_.background));
  added(Steps(BalloonRow::kBackgroundAlpha, Tr("Opacité du fond"),
              Tr("Le jeu pose un voile noir au quart d'opacité, qui laisse le texte se perdre "
                 "sur un sol clair. Plus dense, il se lit partout."),
              ro::ui::ColorAlphaPercent(look_.background), 0, ro::ui::kWholeAlphaPercent,
              "%.0f %%"));
  added(Swatch(BalloonRow::kBorderColor, Tr("Couleur de la bordure"), look_.border));

  // ── LES DEUX BANDEAUX, chacun sa poignée ─────────────────────────────────
  //
  // ⚠ PAS DE CURSEUR X/Y : la place se prend à la souris. Tirer de côté
  // décentre ; le bouton rend la place du jeu, centre compris.
  added(Toggle(BalloonRow::kAnnouncementsUnlocked,
               Tr("Déverrouiller les annonces (glisser pour déplacer)"),
               Tr("Déverrouillé, une poignée apparaît en haut du bandeau des annonces : "
                  "glisse-la où tu veux. Un exemple s'affiche tant qu'aucune annonce n'est "
                  "là.\nUn déplacement vertical garde le centrage."),
               !look_.announcements.locked));
  rows.back().heading = Tr("Position");
  added(Button(BalloonRow::kAnnouncementsHome, Tr("Rendre aux annonces la place du jeu"), nullptr));
  added(Toggle(BalloonRow::kHuntsUnlocked, Tr("Déverrouiller les chasses (glisser pour déplacer)"),
               Tr("Le bandeau qui montre l'avancée des chasses de quête. Le jeu le pose plus "
                  "bas que les annonces, à une hauteur qui suit celle de l'écran."),
               !look_.hunts.locked));
  added(Button(BalloonRow::kHuntsHome, Tr("Rendre aux chasses la place du jeu"), nullptr));
}

bool BroadcastBalloonOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kBalloonRowBase ||
      asked.changed >= kBalloonRowBase + static_cast<int>(BalloonRow::kCount)) {
    return false;
  }
  switch (static_cast<BalloonRow>(asked.changed - kBalloonRowBase)) {
    case BalloonRow::kShown:
      look_.shown = asked.on;
      break;
    case BalloonRow::kDurationMs:
      look_.duration_ms =
          Stepped(asked.value, ro::ui::kShortestBalloonMs, ro::ui::kLongestBalloonMs);
      break;
    case BalloonRow::kMostPerFamily:
      look_.most_per_family =
          Stepped(asked.value, ro::ui::kLeastBalloonsPerFamily, ro::ui::kMostBalloonsPerFamily);
      break;
    case BalloonRow::kFade:
      look_.fade = asked.on;
      break;
    case BalloonRow::kFadeMs:
      look_.fade_ms = Stepped(asked.value, 0, ro::ui::kLongestBalloonFadeMs);
      break;
    case BalloonRow::kFontScale:
      look_.font_scale_percent =
          Stepped(asked.value, ro::ui::kLeastBalloonFontPercent, ro::ui::kMostBalloonFontPercent);
      break;
    case BalloonRow::kWrapPx:
      look_.wrap_px =
          Stepped(asked.value, ro::ui::kLeastBalloonWrapPixels, ro::ui::kMostBalloonWrapPixels);
      break;
    case BalloonRow::kBackgroundColor:
      look_.background = asked.color;
      break;
    case BalloonRow::kBackgroundAlpha:
      look_.background = ro::ui::WithAlphaPercent(
          look_.background, Stepped(asked.value, 0, ro::ui::kWholeAlphaPercent));
      break;
    case BalloonRow::kBorderColor:
      look_.border = asked.color;
      break;
    case BalloonRow::kAnnouncementsUnlocked:
      look_.announcements.locked = !asked.on;
      break;
    case BalloonRow::kAnnouncementsHome:
      look_.announcements.center_x = -1;
      look_.announcements.top_y = -1;
      break;
    case BalloonRow::kHuntsUnlocked:
      look_.hunts.locked = !asked.on;
      break;
    case BalloonRow::kHuntsHome:
      look_.hunts.center_x = -1;
      look_.hunts.top_y = -1;
      break;
    case BalloonRow::kCount:
      break;
  }
  return true;
}

void BroadcastBalloonOptions::NoteAnchor(bool hunts,
                                         const ro::ui::BalloonAnchorAsk& asked) noexcept {
  ro::ui::BalloonAnchor& anchor = hunts ? look_.hunts : look_.announcements;
  anchor.center_x = asked.center_x;
  anchor.top_y = asked.top_y;
}

}  // namespace moonclient
