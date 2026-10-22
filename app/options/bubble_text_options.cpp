#include "options/bubble_text_options.h"

#include <cmath>

#include "ro/core/i18n.h"

namespace moonclient {

using ro::core::Tr;

void DeclareBubbleTextScale(SettingsFile& into, ro::ui::ChatBubbleLook& look) {
  into.Declare(kBubbleTextScaleKey, &look.text_scale_percent);
}

ro::ui::OptionRowView BubbleTextScaleRow(int row_id, const ro::ui::ChatBubbleLook& look) {
  return ro::ui::OptionRowView{
      .id = row_id,
      .label = Tr("Taille du texte des bulles"),
      .help = Tr("La bulle entière suit : cadre, marges, largeur avant le retour à la "
                 "ligne et hauteur au-dessus de la tête."),
      .kind = ro::ui::OptionKind::kSlider,
      .value = static_cast<float>(ro::ui::ChatBubbleTextPercent(look.text_scale_percent)),
      .lowest = static_cast<float>(ro::ui::kLeastBubbleTextPercent),
      .highest = static_cast<float>(ro::ui::kMostBubbleTextPercent),
      .format = "%.0f %%"};
}

void ChangeBubbleTextScale(ro::ui::ChatBubbleLook& look, float asked_percent) {
  look.text_scale_percent =
      ro::ui::ChatBubbleTextPercent(static_cast<int>(std::lround(asked_percent)));
}

}  // namespace moonclient
