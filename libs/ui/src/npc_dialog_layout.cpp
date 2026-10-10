#include <utility>

#include "ro/ui/npc_dialog_text.h"

namespace ro::ui {
namespace {

// Ce qui sépare un média de ce qui le suit, et une icône de son mot.
constexpr float kMediaGapPixels = 4.0F;
constexpr float kIconGapPixels = 3.0F;

// De combien une icône dépasse la hauteur des glyphes : elle se cale sur la
// ligne, pas sur la lettre, sans quoi elle paraît écrasée entre deux mots.
constexpr float kIconOverGlyphPixels = 3.0F;

// LE PLACEUR D'UNE PAGE.
//
// ⚠ IL FERME CHAQUE LIGNE EN CONNAISSANT SA HAUTEUR, et c'est ce que la hauteur
// variable exige : un sprite fait plusieurs lignes de haut, et le texte posé à
// côté doit rester lisible. Sans média, la hauteur d'une ligne reste celle du
// texte et le décalage vaut zéro — une page ordinaire ne bouge pas d'un pixel.
class Placer {
 public:
  Placer(const DialogMetrics& metrics, DialogLayout* into)
      : metrics_(metrics),
        into_(into),
        wrap_(metrics.wrap_width > 1.0F ? metrics.wrap_width : 1.0F),
        space_width_(metrics.measure(" ", metrics.measure_owner)),
        line_height_(metrics.line_height) {
    // ⚠ POSÉE ICI, PAS À LA DÉCLARATION : la hauteur d'une ligne vide est celle
    // du texte, et un zéro ferait se chevaucher les lignes d'une page sans média.
    line_tallest_ = line_height_;
  }

  void Run(std::span<const std::string> lines) {
    std::vector<DialogRun> runs;
    for (const std::string& line : lines) {
      runs = ParseDialogLine(line, metrics_.words);
      for (const DialogRun& run : runs) {
        Place(run);
      }
      EndLine();
    }
    into_->height = pen_y_;
  }

 private:
  void EndLine() {
    for (std::size_t rank = line_start_; rank < into_->fragments.size(); ++rank) {
      DialogFragment& fragment = into_->fragments[rank];
      // Le texte se centre sur la bande de TEXTE — sa hauteur nominale est celle
      // d'une ligne, pas celle du glyphe — et un média sur la sienne.
      const float own = fragment.media != DialogMedia::kNone ? fragment.height : line_height_;
      fragment.top += (line_tallest_ - own) * 0.5F;
    }
    pen_y_ += line_tallest_;
    line_start_ = into_->fragments.size();
    line_tallest_ = line_height_;
    pen_x_ = 0.0F;
  }

  // Mémorise une cible et rend son index, ou moins un.
  [[nodiscard]] int Remember(const DialogTarget& target) {
    if (!target.valid()) {
      return -1;
    }
    into_->targets.push_back(target);
    return static_cast<int>(into_->targets.size()) - 1;
  }

  void Place(const DialogRun& run) {
    if (run.media != DialogMedia::kNone) {
      PlaceMedia(run);
      return;
    }
    const int target = Remember(run.target);
    if (run.icon_item != 0) {
      PlaceIcon(run, target);
      return;
    }
    PlaceWords(run, target);
  }

  void PlaceMedia(const DialogRun& run) {
    DialogMediaSize box{};
    if (metrics_.media_measure != nullptr) {
      box = metrics_.media_measure(run, wrap_, metrics_.media_owner);
    }
    // ⚠ NOTÉE MÊME NULLE : c'est justement l'image absente qui arrivera.
    into_->media_probes.push_back(
        DialogMediaProbe{.run = run, .width = box.width, .height = box.height});
    if (box.width <= 0.0F || box.height <= 0.0F) {
      return;
    }
    // Jamais plus large que le texte — et à l'échelle, sans déformer : le
    // rapport d'un sprite ou d'une image est une information.
    if (box.width > wrap_) {
      const float shrink = wrap_ / box.width;
      box.width *= shrink;
      box.height *= shrink;
    }
    if (pen_x_ > 0.0F && pen_x_ + box.width > wrap_) {
      EndLine();
    }
    DialogFragment fragment;
    fragment.left = pen_x_;
    fragment.top = pen_y_;
    fragment.width = box.width;
    fragment.height = box.height;
    fragment.media = run.media;
    fragment.monster_id = run.monster_id;
    fragment.target = Remember(run.target);
    if (run.media == DialogMedia::kImage) {
      into_->sources.push_back(run.source);
      fragment.source = static_cast<int>(into_->sources.size()) - 1;
    }
    into_->fragments.push_back(std::move(fragment));
    pen_x_ += box.width + kMediaGapPixels;
    if (box.height > line_tallest_) {
      line_tallest_ = box.height;
    }
  }

  void PlaceIcon(const DialogRun& run, int target) {
    const float aspect = metrics_.icon_aspect != nullptr
                             ? metrics_.icon_aspect(run.icon_item, metrics_.icon_owner)
                             : 0.0F;
    into_->icon_probes.push_back(DialogIconProbe{.item_id = run.icon_item, .aspect = aspect});
    if (aspect <= 0.0F) {
      // Pas d'icône à montrer : on ne réserve rien. Une place vide au milieu
      // d'une phrase se lit comme une faute de frappe.
      return;
    }
    const float height = metrics_.font_size + kIconOverGlyphPixels;
    const float width = height * aspect;
    if (pen_x_ > 0.0F && pen_x_ + width > wrap_) {
      EndLine();
    }
    DialogFragment fragment;
    fragment.left = pen_x_;
    fragment.top = pen_y_;
    fragment.width = width;
    fragment.height = height;
    fragment.icon_item = run.icon_item;
    fragment.target = target;
    into_->fragments.push_back(std::move(fragment));
    pen_x_ += width + kIconGapPixels;
  }

  void PlaceWords(const DialogRun& run, int target) {
    const std::string& text = run.text;
    std::size_t at = 0;
    while (at < text.size()) {
      // Où commençaient les espaces de tête : c'est de là que part le
      // soulignement d'un lien, pour qu'il ne se coupe pas entre deux mots.
      const float gap_left = pen_x_;
      const float gap_top = pen_y_;
      // ⚠ DÉCOUPER SUR L'ESPACE EST SÛR EN UTF-8 : un octet de continuation
      // (0x80-0xBF) ne vaut jamais 0x20.
      while (at < text.size() && text[at] == ' ') {
        pen_x_ += space_width_;
        ++at;
      }
      std::size_t stop = at;
      while (stop < text.size() && text[stop] != ' ') {
        ++stop;
      }
      if (stop == at) {
        break;
      }
      const std::string_view word(text.data() + at, stop - at);
      const float width = metrics_.measure(word, metrics_.measure_owner);
      bool wrapped = false;
      if (pen_x_ > 0.0F && pen_x_ + width > wrap_) {
        EndLine();
        wrapped = true;
      }
      DialogFragment fragment;
      fragment.left = pen_x_;
      fragment.top = pen_y_;
      fragment.width = width;
      fragment.height = metrics_.font_size;
      fragment.text = std::string(word);
      fragment.color = run.color;
      fragment.bold = run.bold;
      fragment.link = run.link;
      fragment.target = target;
      if (run.link != DialogLink::kNone || target >= 0) {
        fragment.payload = run.payload;
        // Le soulignement remonte aux espaces de tête, mais seulement sur la
        // MÊME ligne : replié, il repartirait de la marge gauche.
        fragment.underline_left = (!wrapped && gap_top == pen_y_) ? gap_left : pen_x_;
      }
      into_->fragments.push_back(std::move(fragment));
      pen_x_ += width;
      at = stop;
    }
  }

  const DialogMetrics& metrics_;
  DialogLayout* into_;
  float wrap_ = 1.0F;
  float space_width_ = 0.0F;
  float line_height_ = 0.0F;

  float pen_x_ = 0.0F;
  float pen_y_ = 0.0F;
  // Le premier fragment de la ligne visuelle en cours, et sa hauteur.
  std::size_t line_start_ = 0;
  float line_tallest_ = 0.0F;
};

}  // namespace

DialogLayout LayOutDialog(std::span<const std::string> lines, const DialogMetrics& metrics) {
  DialogLayout laid;
  laid.wrap_width = metrics.wrap_width;
  laid.font_size = metrics.font_size;
  if (metrics.measure == nullptr || metrics.line_height <= 0.0F) {
    return laid;
  }
  Placer placer(metrics, &laid);
  placer.Run(lines);
  return laid;
}

bool DialogMeasuresChanged(const DialogLayout& laid, const DialogMetrics& metrics) {
  // ⚠ LA MÊME LARGEUR QUE LE PLACEUR : une mesure d'image dépend de la place
  // offerte, et la reposer autrement la verrait changer à chaque image.
  const float wrap = metrics.wrap_width > 1.0F ? metrics.wrap_width : 1.0F;
  if (metrics.media_measure != nullptr) {
    for (const DialogMediaProbe& probe : laid.media_probes) {
      const DialogMediaSize now = metrics.media_measure(probe.run, wrap, metrics.media_owner);
      if (now.width != probe.width || now.height != probe.height) {
        return true;
      }
    }
  }
  if (metrics.icon_aspect != nullptr) {
    for (const DialogIconProbe& probe : laid.icon_probes) {
      if (metrics.icon_aspect(probe.item_id, metrics.icon_owner) != probe.aspect) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace ro::ui
