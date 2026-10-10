// Voir l'en-tête.

#include "game/homunculus_staff_words.h"

#include <algorithm>
#include <string_view>

#include "ro/sim/companion_ai.h"

#include "game/companion_ai_names.h"

namespace moonclient {
namespace {

using ro::sim::kHomunculusBasicTypeCount;
using ro::sim::kHomunculusFirstMutatedType;
using ro::sim::kHomunculusJobBase;
using ro::sim::kHomunculusLastType;
using ro::sim::kHomunculusLastUnevolvedType;

[[nodiscard]] int TypeOf(std::uint32_t job) noexcept {
  return static_cast<int>(job) - kHomunculusJobBase;
}

// Les classes de `first_type` à `last_type`, chacune en « classe nom ».
void AppendRange(std::vector<ro::ui::StaffHomunculusChoice>& into, int first_type, int last_type) {
  for (int type = first_type; type <= last_type; ++type) {
    const auto job = static_cast<std::uint16_t>(kHomunculusJobBase + type);
    into.push_back(ro::ui::StaffHomunculusChoice{
        .job = job, .label = std::to_string(job) + " " + CompanionClassName(job)});
  }
}

// Une phrase d'une seule ligne : le serveur lit la commande jusqu'au premier
// saut de ligne.
[[nodiscard]] std::string OneLine(std::string_view text) {
  std::string line(text.substr(0, text.find_first_of("\r\n")));
  return line;
}

}  // namespace

std::vector<ro::ui::StaffHomunculusChoice> CreatableHomunculi() {
  std::vector<ro::ui::StaffHomunculusChoice> choices;
  AppendRange(choices, 1, kHomunculusLastUnevolvedType);
  AppendRange(choices, kHomunculusFirstMutatedType, kHomunculusLastType);
  return choices;
}

std::vector<ro::ui::StaffHomunculusChoice> MutatedHomunculi() {
  std::vector<ro::ui::StaffHomunculusChoice> choices;
  AppendRange(choices, kHomunculusFirstMutatedType, kHomunculusLastType);
  return choices;
}

bool IsMutatedHomunculus(std::uint32_t job) noexcept {
  const int type = TypeOf(job);
  return type >= kHomunculusFirstMutatedType && type <= kHomunculusLastType;
}

std::optional<bool> HomunculusEvolved(std::uint32_t job) noexcept {
  const int type = TypeOf(job);
  if (IsMutatedHomunculus(job)) {
    return false;
  }
  if (type < 1 || type > kHomunculusBasicTypeCount) {
    return std::nullopt;
  }
  return type > kHomunculusLastUnevolvedType;
}

std::string MakeHomunculusCommand(std::uint16_t job) { return "@makehomun " + std::to_string(job); }

std::string HomunculusStaffCommand(const ro::ui::HomunculusTabAction& asked) {
  if (asked.verb != ro::ui::HomunculusVerb::kStaff) {
    return std::string();
  }
  switch (asked.staff_order) {
    case ro::ui::HomunculusStaffOrder::kNone:
      return std::string();
    case ro::ui::HomunculusStaffOrder::kSetHunger:
      return "@homhungry " +
             std::to_string(std::clamp(asked.staff_amount, 0, ro::ui::kHomunculusStaffHungerMost));
    case ro::ui::HomunculusStaffOrder::kSetIntimacy:
      return "@homfriendly " + std::to_string(std::clamp(asked.staff_amount, 0,
                                                         ro::ui::kHomunculusStaffIntimacyMost));
    case ro::ui::HomunculusStaffOrder::kAddLevels:
      // ⚠ LA COMMANDE EST RELATIVE, ET LE SERVEUR REFUSE MOINS D'UN NIVEAU.
      return asked.staff_amount > 0 ? "@homlevel " + std::to_string(asked.staff_amount)
                                    : std::string();
    case ro::ui::HomunculusStaffOrder::kShuffle:
      return "@homshuffle";
    case ro::ui::HomunculusStaffOrder::kStats:
      return "@homstats";
    case ro::ui::HomunculusStaffOrder::kInfo:
      return "@hominfo";
    case ro::ui::HomunculusStaffOrder::kEvolve:
      return "@homevolution";
    case ro::ui::HomunculusStaffOrder::kMutate:
      // ⚠ SANS CLASSE, LE SERVEUR EN TIRERAIT UNE AU SORT : on n'envoie rien.
      return asked.staff_job != 0 ? "@hommutate " + std::to_string(asked.staff_job) : std::string();
    case ro::ui::HomunculusStaffOrder::kTalk: {
      const std::string line = OneLine(asked.staff_text);
      return line.empty() ? std::string() : "@homtalk " + line;
    }
  }
  return std::string();
}

}  // namespace moonclient
