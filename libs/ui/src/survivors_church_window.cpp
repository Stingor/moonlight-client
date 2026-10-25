#include "ro/ui/survivors_church_window.h"

#include "ro/core/i18n.h"
#include "ro/ui/window_name.h"

#include "survivors_meta_text.h"
#include "survivors_shop_list.h"
#include "survivors_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kChurchTitle = core::TrText("Église de Prontera");

constexpr float kFirstWidth = 460.0F;
constexpr float kFirstHeight = 620.0F;

SurvivorsEffectText BlessingEffect(const SurvivorsShopRow& row, int total) {
  return SurvivorsBlessingEffect(row.key, total);
}

const char* BlessingDescription(const SurvivorsShopRow& row) {
  return SurvivorsText(row.description);
}

}  // namespace

SurvivorsShopAsk DrawSurvivorsChurchWindow(SurvivorsShopState& state, const SurvivorsShopView& view,
                                           const SkinArt& art) {
  if (!state.open) return {};
  static WindowName name(kChurchTitle, kSurvivorsChurchId);
  const SurvivorsShopLook look{.window_name = name.Get(),
                               .buy_label = core::Tr("Bénir"),
                               .first_width = kFirstWidth,
                               .first_height = kFirstHeight,
                               .effect = BlessingEffect,
                               .describe = BlessingDescription};
  return DrawSurvivorsShop(state, view, look, art);
}

}  // namespace ro::ui
