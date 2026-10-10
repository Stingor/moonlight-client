#include "options/quick_cast_options.h"

#include <cmath>

namespace moonclient {
namespace {

using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kGroundKey = "quickcast_ground";
constexpr const char* kTargetKey = "quickcast_target";
constexpr const char* kRepeatKey = "quickcast_rate";
constexpr const char* kItemKey = "quickcast_item";
constexpr const char* kItemRepeatKey = "quickcast_item_rate";

[[nodiscard]] int Rounded(float value) { return static_cast<int>(std::lround(value)); }

}  // namespace

void QuickCastOptions::Declare(SettingsFile& into) {
  into.Declare(kGroundKey, &look_.ground);
  into.Declare(kTargetKey, &look_.target);
  into.Declare(kRepeatKey, &look_.repeat_ms);
  into.Declare(kItemKey, &look_.item);
  into.Declare(kItemRepeatKey, &look_.item_repeat_ms);
}

void QuickCastOptions::ComposeInto(std::vector<OptionRowView>& rows) {
  const ro::ui::QuickCastTexts texts = ro::ui::QuickCastTextsFor(staff_);
  rows.push_back(OptionRowView{.id = QuickCastRowId(QuickCastRow::kGround),
                               .label = texts.ground_label,
                               .help = texts.ground_help,
                               .kind = OptionKind::kToggle,
                               .on = look_.ground});
  rows.back().heading = texts.title;
  rows.push_back(OptionRowView{.id = QuickCastRowId(QuickCastRow::kTarget),
                               .label = texts.target_label,
                               .help = texts.target_help,
                               .kind = OptionKind::kToggle,
                               .on = look_.target});
  // ⚠ GRISÉE ET NON RETIRÉE quand aucun des deux n'est coché : c'est la règle
  // du panneau, où une ligne qui disparaît fait sauter la mise en page.
  rows.push_back(OptionRowView{.id = QuickCastRowId(QuickCastRow::kRepeat),
                               .label = texts.repeat_label,
                               .help = texts.repeat_help,
                               .kind = OptionKind::kSlider,
                               .value = static_cast<float>(look_.repeat_ms),
                               .lowest = static_cast<float>(ro::ui::kQuickCastRepeatLowestMs),
                               .highest = static_cast<float>(ro::ui::kQuickCastRepeatHighestMs),
                               .format = "%.0f",
                               .indented = true,
                               .disabled = !look_.ground && !look_.target});
  rows.push_back(OptionRowView{.id = QuickCastRowId(QuickCastRow::kItem),
                               .label = texts.item_label,
                               .help = texts.item_help,
                               .kind = OptionKind::kToggle,
                               .on = look_.item});
  // ⚠ LA BORNE BASSE SUIT LE DROIT. Le curseur ne montre jamais moins que le
  // plancher, un fichier édité à la main compris ; ce qui part, lui, est borné
  // plus loin, par `QuickCastItemPeriodMs`.
  const int floor_ms = ro::ui::QuickCastItemFloorMs(staff_);
  rows.push_back(OptionRowView{
      .id = QuickCastRowId(QuickCastRow::kItemRepeat),
      .label = texts.item_repeat_label,
      .help = texts.item_repeat_help,
      .kind = OptionKind::kSlider,
      .value = static_cast<float>(ro::ui::QuickCastItemPeriodMs(look_.item_repeat_ms, staff_)),
      .lowest = static_cast<float>(floor_ms),
      .highest = static_cast<float>(ro::ui::kQuickCastItemHighestMs),
      .format = "%.0f",
      .indented = true,
      .disabled = !look_.item});
}

bool QuickCastOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kQuickCastRowBase ||
      asked.changed >= kQuickCastRowBase + static_cast<int>(QuickCastRow::kCount)) {
    return false;
  }
  switch (static_cast<QuickCastRow>(asked.changed - kQuickCastRowBase)) {
    case QuickCastRow::kGround:
      look_.ground = asked.on;
      break;
    case QuickCastRow::kTarget:
      look_.target = asked.on;
      break;
    case QuickCastRow::kRepeat:
      look_.repeat_ms = Rounded(asked.value);
      break;
    case QuickCastRow::kItem:
      look_.item = asked.on;
      break;
    case QuickCastRow::kItemRepeat:
      look_.item_repeat_ms = Rounded(asked.value);
      break;
    case QuickCastRow::kCount:
      break;
  }
  return true;
}

bool QuickCastOptions::Note(const ro::ui::QuickCastLook& seen) noexcept {
  if (seen == look_) {
    return false;
  }
  look_ = seen;
  return true;
}

}  // namespace moonclient
