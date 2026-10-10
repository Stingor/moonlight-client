// LES RANGÉES DE L'ENREGISTREUR DE ZONE.
//
// ⚠ À PART DU CATALOGUE : celui-ci touche déjà les sept cents lignes, et le
// plafond du dépôt est à huit cents. La section a en plus deux textes composés
// à la volée, que les lignes ordinaires n'ont pas.
//
// 🔴 CE QUE LA SECTION DOIT DIRE, ET QU'AUCUN CURSEUR NE DIT TOUT SEUL : la
// CONSÉQUENCE. « Largeur max : 640 » et « Durée : 6 s » ne renseignent ni sur le
// nombre d'images, ni sur ce que cela pèsera en mémoire — et l'infobulle de l'un
// ne peut pas y répondre, puisque la réponse dépend de l'autre. D'où la ligne de
// note, qui les additionne.

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/zone_capture.h"

#include "options/game_options.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

constexpr double kBytesPerMegabyte = 1024.0 * 1024.0;

// La coupure entre deux paragraphes d'une infobulle.
constexpr const char* kParagraph = "\n\n";

[[nodiscard]] OptionRowView Slider(GameOption which, const char* label, int value, int lowest,
                                   int highest, const char* format) {
  return OptionRowView{.id = static_cast<int>(which),
                       .label = label,
                       .kind = OptionKind::kSlider,
                       .value = static_cast<float>(value),
                       .lowest = static_cast<float>(lowest),
                       .highest = static_cast<float>(highest),
                       .format = format};
}

}  // namespace

void GameOptions::BuildCaptureRows() {
  const ro::ui::ZoneCaptureSettings how = capture();
  capture_rows_.clear();

  std::array<char, 160> text = {};
  if (how.zone.empty()) {
    capture_zone_label_ = Tr("Définir la zone");
  } else {
    // ⚠ LE RECTANGLE EST DIT EN TOUTES LETTRES, place comprise : deux zones de
    // même taille à deux endroits différents sont deux zones, et c'est
    // justement ce qu'on revient vérifier.
    std::snprintf(text.data(), text.size(), Tr("Redéfinir la zone  (%d x %d en %d, %d)"),
                  how.zone.width, how.zone.height, how.zone.left, how.zone.top);
    capture_zone_label_ = text.data();
  }
  capture_rows_.push_back(OptionRowView{
      .id = static_cast<int>(GameOption::kCaptureZone),
      .label = capture_zone_label_.c_str(),
      .help = Tr("Trace un rectangle à la souris, comme l'outil de capture du système. "
                 "La zone est retenue d'une partie à l'autre : ensuite, les touches "
                 "suffisent."),
      .kind = OptionKind::kAction});

  // 🔴 LES PARAGRAPHES SONT DES ENTRÉES SÉPARÉES, RECOLLÉES ICI. Le catalogue de
  // traduction range UNE entrée par LIGNE : un saut de ligne à l'intérieur d'une
  // clé la couperait en deux, et la moitié du bas ne serait jamais traduite.
  capture_record_help_ =
      Tr("L'image enregistrée est celle que tu vois — le monde ET les "
         "fenêtres. Le cadre et le témoin, eux, se tiennent en dehors de "
         "la zone : ils n'entrent jamais dans le GIF.");
  capture_record_help_ += kParagraph;
  capture_record_help_ +=
      Tr("À la fin, le GIF part dans le presse-papier : un Ctrl+V le "
         "poste, animation comprise. Ce que tu y avais mis est perdu.");
  capture_record_help_ += kParagraph;
  capture_record_help_ += Tr("Un second appui arrête avant la fin.");
  capture_picture_help_ =
      Tr("La MÊME zone, en une seule image prise à l'instant du clic : "
         "ni décompte, ni durée, rien à arrêter.");
  capture_picture_help_ += kParagraph;
  capture_picture_help_ +=
      Tr("Elle sort à la taille réelle de la zone — la largeur maximale "
         "ci-dessous ne concerne que le GIF, où elle sert à contenir le "
         "poids d'une animation.");

  capture_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kCaptureRecord),
                    .label = Tr("Filmer"),
                    // ⚠ « FILMER » ET NON « ENREGISTRER » : ce dernier mot est celui des
                    // boutons qui SAUVEGARDENT, partout ailleurs dans ce client. Sur un
                    // bouton qui lance une capture, il dirait le contraire de ce qu'il fait.
                    .help = capture_record_help_.c_str(),
                    .kind = OptionKind::kAction,
                    .disabled = how.zone.empty()});

  capture_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kCapturePicture),
                                        .label = Tr("Capturer une image"),
                                        .help = capture_picture_help_.c_str(),
                                        .kind = OptionKind::kAction,
                                        .disabled = how.zone.empty()});

  capture_rows_.push_back(Slider(GameOption::kCaptureFramesPerSecond, Tr("Images par seconde"),
                                 how.frames_per_second, ro::ui::kSlowestCapture,
                                 ro::ui::kFastestCapture, "%.0f"));
  capture_rows_.push_back(Slider(GameOption::kCaptureSeconds, Tr("Durée"), how.seconds,
                                 ro::ui::kShortestCapture, ro::ui::kLongestCapture, "%.0f s"));
  capture_rows_.push_back(Slider(GameOption::kCaptureWidth, Tr("Largeur maximale"),
                                 how.widest_output, ro::ui::kNarrowestOutput, ro::ui::kWidestOutput,
                                 "%.0f px"));
  capture_rows_.back().help =
      Tr("La zone est réduite à cette largeur, proportions gardées, avant d'entrer "
         "dans le GIF. C'est le réglage qui pèse le plus lourd : la mémoire et la "
         "taille du fichier montent avec le CARRÉ de la largeur.");
  capture_rows_.push_back(Slider(GameOption::kCaptureDelay, Tr("Délai avant départ"),
                                 how.start_delay_seconds, 0, ro::ui::kLongestDelay, "%.0f s"));
  capture_rows_.back().help =
      Tr("Le temps de refermer ce panneau et de te placer avant que "
         "ça tourne.");

  const ro::ui::ZoneRect out = ro::ui::OutputSize(how.zone, how.widest_output);
  const std::size_t bytes = ro::ui::CaptureBytes(how);
  std::snprintf(text.data(), text.size(), Tr("%d images en %d x %d — %.0f Mo en mémoire"),
                how.frames_per_second * how.seconds, out.width, out.height,
                static_cast<double>(bytes) / kBytesPerMegabyte);
  capture_note_ = text.data();
  if (bytes > ro::ui::kLargestCaptureBytes) {
    // ⚠ DIT AVANT LE DÉPART, ET NON APRÈS : les images sont réservées d'un bloc,
    // et un refus qui n'arriverait qu'au moment de filmer laisserait croire à
    // une panne du bouton.
    capture_note_ += "  ";
    capture_note_ += Tr("— au-delà du budget : réduis la durée ou la largeur.");
  }
  capture_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kCaptureZone),
                                        .label = capture_note_.c_str(),
                                        .kind = OptionKind::kNote});
}

}  // namespace moonclient
