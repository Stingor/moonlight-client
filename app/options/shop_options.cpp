#include "options/shop_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs le réglage
// d'origine, sans que rien ne le dise.
constexpr const char* kOneClickKey = "shops_open_of_a_click";
constexpr const char* kSignboardsShownKey = "shops_signboards_shown";

[[nodiscard]] int IdOf(ShopRow row) { return kShopRowBase + static_cast<int>(row); }

}  // namespace

void ShopOptions::Declare(SettingsFile& into) {
  into.Declare(kOneClickKey, &one_click_);
  into.Declare(kSignboardsShownKey, &signboards_shown_);
}

void ShopOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const {
  rows.push_back(ro::ui::OptionRowView{
      .id = IdOf(ShopRow::kOneClick),
      .label = Tr("Ouvrir une échoppe d'un simple clic"),
      .help = Tr("Un clic gauche sur l'enseigne ouvre l'échoppe, un clic droit sur une enseigne "
                 "referme celle qu'on visitait. Sinon il faut un double-clic pour l'ouvrir."),
      .command = "/shopping",
      .kind = ro::ui::OptionKind::kToggle,
      .on = one_click_});
  rows.push_back(ro::ui::OptionRowView{
      .id = IdOf(ShopRow::kSignboardsShown),
      .label = Tr("Afficher les enseignes des échoppes"),
      .help = Tr("Éteint, les enseignes des vendeurs et des acheteurs disparaissent ; celles des "
                 "salons de discussion restent."),
      .command = "/showshop",
      .kind = ro::ui::OptionKind::kToggle,
      .on = signboards_shown_});
}

bool ShopOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kShopRowBase ||
      asked.changed >= kShopRowBase + static_cast<int>(ShopRow::kCount)) {
    return false;
  }
  switch (static_cast<ShopRow>(asked.changed - kShopRowBase)) {
    case ShopRow::kOneClick:
      one_click_ = asked.on;
      break;
    case ShopRow::kSignboardsShown:
      signboards_shown_ = asked.on;
      break;
    case ShopRow::kCount:
      break;
  }
  return true;
}

bool ShopOptions::Toggle(ShopRow row) {
  switch (row) {
    case ShopRow::kOneClick:
      one_click_ = !one_click_;
      return one_click_;
    case ShopRow::kSignboardsShown:
      signboards_shown_ = !signboards_shown_;
      return signboards_shown_;
    case ShopRow::kCount:
      break;
  }
  return false;
}

}  // namespace moonclient
