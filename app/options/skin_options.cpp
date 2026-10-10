// Le choix du skin. Voir l'en-tête.

#include "options/skin_options.h"

#include <algorithm>
#include <utility>

#include "ro/core/i18n.h"

#include "shell/skin_folder.h"

namespace moonclient {
namespace {

// La clé du fichier. ⚠ GRAVÉE : la changer rend à tous les joueurs le skin
// d'origine, sans que rien ne le dise.
constexpr const char* kSkinNameKey = "skin_name";

}  // namespace

void SkinOptions::Declare(SettingsFile& into) {
  if (name_.empty()) {
    name_ = std::string(kDefaultSkinName);
  }
  into.Declare(kSkinNameKey, &name_);
}

void SkinOptions::SeeInstalled(std::vector<std::string> names) { installed_ = std::move(names); }

std::string_view SkinOptions::active() const {
  if (installed_.empty() || std::ranges::find(installed_, name_) != installed_.end()) {
    return name_.empty() ? kDefaultSkinName : std::string_view(name_);
  }
  return kDefaultSkinName;
}

void SkinOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) {
  if (installed_.empty()) {
    return;
  }
  choices_.clear();
  for (const std::string& name : installed_) {
    choices_.push_back(name.c_str());
  }
  const auto chosen = std::ranges::find(installed_, active());
  rows.push_back(ro::ui::OptionRowView{
      .id = kSkinRowBase + static_cast<int>(SkinRow::kChoice),
      .label = ro::core::Tr("Skin"),
      .help = ro::core::Tr("Les dossiers de skin\\ de l'installation. Un skin ne porte que les "
                           "pièces qu'il change ; les autres viennent des archives. Le choix "
                           "s'applique tout de suite."),
      .kind = ro::ui::OptionKind::kChoice,
      .chosen = static_cast<int>(chosen - installed_.begin()),
      .choices = choices_});
}

bool SkinOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed != kSkinRowBase + static_cast<int>(SkinRow::kChoice)) {
    return false;
  }
  if (asked.chosen >= 0 && static_cast<std::size_t>(asked.chosen) < installed_.size()) {
    name_ = installed_[static_cast<std::size_t>(asked.chosen)];
  }
  return true;
}

}  // namespace moonclient
