// LE CORPS D'UN JOUEUR MONTÉ : sur une monture de trésorerie, et dans la
// combinaison de Madogear.
//
// Le métier montré (`ro/render/shown_job.h`) a déjà choisi le Peco, le dragon,
// le loup ou la machine. Il reste deux cas que le métier ne dit pas : la
// monture de trésorerie, un état qui prête au joueur la classe montée de son
// métier, et la combinaison, qui habille le corps du Madogear d'une armure.

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "ro/render/sprite_catalog.h"

#include "client_tables.h"
#include "sprite_catalog_internal.h"

namespace ro::render::catalog_internal {
namespace {

// La machine, et l'armure qui la remplace sous la combinaison.
constexpr std::string_view kMadogearMachine = "마도기어";
constexpr std::string_view kMadogearArmor = "마도아머";
constexpr std::string_view kMeisterMachine = "MEISTER_MADOGEAR1";
constexpr std::string_view kMeisterArmor = "MEISTER_MADOGEAR2";

}  // namespace

std::uint32_t BodyJob(const ActorLook& look) {
  if (!look.cash_mount) {
    return look.job;
  }
  const auto& mounts = client_tables::kCashMountClasses;
  const auto found =
      std::ranges::lower_bound(mounts, look.job, {}, &client_tables::CashMountClass::job);
  return found == mounts.end() || found->job != look.job ? look.job : found->mounted;
}

std::string_view MadogearSuitName(std::string_view machine_name, bool suit) {
  if (!suit) {
    return machine_name;
  }
  if (machine_name == kMadogearMachine) {
    return kMadogearArmor;
  }
  if (machine_name == kMeisterMachine) {
    return kMeisterArmor;
  }
  return machine_name;
}

std::string_view ShownBodyName(const ActorLook& look) {
  return MadogearSuitName(PlayerBodyName(BodyJob(look)), look.madogear_suit);
}

}  // namespace ro::render::catalog_internal
