#include "options/call_options.h"

#include <utility>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;

[[nodiscard]] int IdOf(CallRow row) { return kCallRowBase + static_cast<int>(row); }

}  // namespace

void CallOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const {
  rows.push_back(ro::ui::OptionRowView{
      .id = IdOf(CallRow::kCallable),
      .label = Tr("Accepter l'appel d'urgence"),
      .help = Tr("Le chef de guilde peut vous appeler auprès de lui avec Emergency Call. "
                 "Décoché, son appel ne vous atteint pas.\nC'est le serveur qui retient ce "
                 "réglage, pour ce personnage : il s'active une fois en jeu."),
      .command = "/call",
      .kind = ro::ui::OptionKind::kToggle,
      // Le défaut du client d'origine, montré grisé tant que le serveur se tait.
      .on = callable_.value_or(true),
      .disabled = !callable_.has_value()});
  rows.back().heading = Tr("Guilde");
}

bool CallOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed != IdOf(CallRow::kCallable)) {
    return false;
  }
  if (callable_.has_value()) {
    requested_ = asked.on;
  }
  return true;
}

bool CallOptions::RequestToggle() noexcept {
  if (!callable_.has_value()) {
    return false;
  }
  requested_ = !*callable_;
  return true;
}

std::optional<bool> CallOptions::TakeRequest() noexcept { return std::exchange(requested_, {}); }

}  // namespace moonclient
