#include "ro/ui/survivors_kafra_window.h"

#include "ro/core/i18n.h"
#include "ro/ui/window_name.h"

#include "survivors_meta_text.h"
#include "survivors_shop_list.h"
#include "survivors_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kKafraTitle = core::TrText("Kafra des Survivants");

constexpr float kFirstWidth = 420.0F;
constexpr float kFirstHeight = 300.0F;

SurvivorsEffectText KafraEffect(const SurvivorsShopRow& /*row*/, int total) {
  return SurvivorsKafraEffect(total);
}

// La description passée par la vue l'emporte ; à défaut, celle de la clé.
const char* KafraDescription(const SurvivorsShopRow& row) {
  if (row.description != nullptr && row.description[0] != '\0') {
    return SurvivorsText(row.description);
  }
  return SurvivorsKafraDescription(row.key);
}

}  // namespace

SurvivorsShopAsk DrawSurvivorsKafraWindow(SurvivorsShopState& state, const SurvivorsShopView& view,
                                          const SkinArt& art) {
  if (!state.open) return {};
  static WindowName name(kKafraTitle, kSurvivorsKafraId);
  const SurvivorsShopLook look{.window_name = name.Get(),
                               .buy_label = core::Tr("Acheter"),
                               .first_width = kFirstWidth,
                               .first_height = kFirstHeight,
                               .effect = KafraEffect,
                               .describe = KafraDescription};
  return DrawSurvivorsShop(state, view, look, art);
}

}  // namespace ro::ui
