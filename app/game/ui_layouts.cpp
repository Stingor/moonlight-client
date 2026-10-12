#include "game/ui_layouts.h"

#include <filesystem>
#include <system_error>
#include <utility>

#include "ro/core/i18n.h"

#include "game/ui_stage.h"

namespace moonclient {

std::uint64_t LayoutPort::Revision() {
  std::map<std::string, bool> toggles = ui_->LayoutToggles();
  if (toggles != seen_toggles_) {
    seen_toggles_ = std::move(toggles);
    ++toggle_revision_;
  }
  return ui_->layout_revision() + toggle_revision_ + minimap_.Revision();
}

ro::ui::LayoutProfile LayoutPort::Capture() {
  ro::ui::LayoutProfile layout = ui_->CurrentLayout();
  minimap_.Capture(layout);
  return layout;
}

void LayoutPort::Pose(const ro::ui::LayoutProfile& profile) {
  ui_->UseLayout(profile);
  minimap_.Pose(profile);
}

bool LayoutPort::Load(UiLayoutBook& into) {
  std::error_code failed;
  const bool fresh = !std::filesystem::exists(UiLayoutPath(client_root_), failed);
  if (!LoadUiLayouts(client_root_, into)) {
    return false;
  }
  if (fresh && into.Find(kDefaultProfileName) == nullptr) {
    // La minimap d'un joueur qui la réglait déjà avant les profils est dans
    // `navigation.yml` : le premier « Default » en hérite.
    ro::ui::LayoutProfile first = FactoryLayout();
    first.minimap.reset();
    into.Put(kDefaultProfileName, first);
  }
  return true;
}

void LayoutPort::Save(const UiLayoutBook& from) {
  static_cast<void>(SaveUiLayouts(client_root_, from));
}

ProfileFamilyText UiLayouts::Text() {
  return ProfileFamilyText{
      .title = ro::core::Tr("Profils de disposition"),
      .help = ro::core::Tr(
          "Un profil retient la place, la taille et l'ouverture de chaque fenêtre. Chaque "
          "personnage garde le sien. Une fenêtre suit le bord de l'écran dont elle est la plus "
          "proche : changer de résolution ne la perd pas."),
      .id = "layout_profiles"};
}

}  // namespace moonclient
