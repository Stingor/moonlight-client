// LES CHOIX D'ARGUMENT DES COMMANDES : quelle commande en déclare, et ce que la
// liste des métiers de « @jobchange » propose.

#include "chat/chat_argument_choices.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "character/staff_jobs.h"
#include "chat/chat_command_catalogue.h"
#include "chat/server_jobchange.h"

using moonclient::ChatArgumentChoices;
using moonclient::ChatArgumentSource;
using moonclient::ServerCommandArgumentSource;
using moonclient::ServerJobChangeTarget;
using moonclient::ServerJobChangeTargets;
using ro::ui::ChatArgumentChoice;

namespace {

[[nodiscard]] const ChatArgumentChoice* ChoiceOf(const ChatArgumentChoices& choices,
                                                 std::string_view value) {
  const auto jobs = choices.Of(ChatArgumentSource::kServerJobChange);
  const auto found = std::ranges::find(jobs, value, &ChatArgumentChoice::value);
  return found == jobs.end() ? nullptr : &*found;
}

}  // namespace

TEST_CASE("Seule « @jobchange » déclare une source de choix") {
  CHECK(ServerCommandArgumentSource("jobchange") == ChatArgumentSource::kServerJobChange);
  // L'alias y mène par la liste, pas par une seconde déclaration.
  CHECK(ServerCommandArgumentSource("job") == ChatArgumentSource::kNone);
  CHECK(ServerCommandArgumentSource("warp") == ChatArgumentSource::kNone);
  CHECK(ServerCommandArgumentSource("joblevel") == ChatArgumentSource::kNone);
  CHECK(ChatArgumentChoices().Of(ChatArgumentSource::kNone).empty());
}

TEST_CASE("Les métiers proposés sont ceux que le serveur accepte, nommés par le client") {
  ro::vfs::JobNames names;
  names.by_id[4008] = "Lord Knight";
  names.by_id[4011] = "White Smith";
  ChatArgumentChoices choices;
  choices.Rebuild(names);
  const auto jobs = choices.Of(ChatArgumentSource::kServerJobChange);
  REQUIRE(jobs.size() == ServerJobChangeTargets().size());
  for (std::size_t rank = 0; rank < jobs.size(); ++rank) {
    CHECK(jobs[rank].value == std::to_string(ServerJobChangeTargets()[rank].job_id));
  }

  // Même nom au client et au serveur : pas de parenthèses.
  const ChatArgumentChoice* lord = ChoiceOf(choices, "4008");
  REQUIRE(lord != nullptr);
  CHECK(lord->label == "Lord Knight");
  CHECK(lord->detail.empty());
  // Un nom qui diffère : celui du serveur suit, c'est lui que la commande lit.
  const ChatArgumentChoice* smith = ChoiceOf(choices, "4011");
  REQUIRE(smith != nullptr);
  CHECK(smith->label == "White Smith");
  CHECK(smith->detail == "Whitesmith");
  CHECK(smith->search_key.find("whitesmith") != std::string_view::npos);
  // Un métier que le client ne nomme pas prend le nom du serveur seul.
  const ChatArgumentChoice* novice = ChoiceOf(choices, "0");
  REQUIRE(novice != nullptr);
  CHECK(novice->label == "Novice");
  CHECK(novice->detail.empty());
}

TEST_CASE("Les métiers fantômes que la commande refuse ne sont pas proposés") {
  // Montures, tenues de mariage, de Noël, d'été, hanbok, Oktoberfest.
  constexpr auto kRefused = std::to_array<std::uint16_t>(
      {13, 21, 22, 26, 27, 28, 29, 30, 4014, 4022, 4036, 4044, 4048, 4080, 4087, 4109, 4112});
  for (const std::uint16_t refused : kRefused) {
    INFO(refused);
    CHECK(std::ranges::find(ServerJobChangeTargets(), refused, &ServerJobChangeTarget::job_id) ==
          ServerJobChangeTargets().end());
  }
  CHECK(std::ranges::is_sorted(ServerJobChangeTargets(), {}, &ServerJobChangeTarget::job_id));
}

TEST_CASE("Le menu des classes du staff propose exactement les métiers du serveur") {
  ro::vfs::JobNames names;
  names.by_id[4011] = "White Smith";
  names.by_id[4008] = "Lord Knight";
  moonclient::StaffJobs staff;
  const auto rows = staff.Rebuild(&names);
  std::vector<std::uint32_t> shown;
  for (const ro::ui::StaffJobView& row : rows) {
    shown.push_back(row.id);
  }
  std::vector<std::uint32_t> accepted;
  for (const ServerJobChangeTarget& target : ServerJobChangeTargets()) {
    accepted.push_back(target.job_id);
  }
  std::ranges::sort(shown);
  CHECK(std::ranges::adjacent_find(shown) == shown.end());
  CHECK(shown == accepted);

  const auto row_of = [&](std::uint32_t job_id) {
    return std::ranges::find(rows, job_id, &ro::ui::StaffJobView::id);
  };
  REQUIRE(row_of(4011) != rows.end());
  CHECK(row_of(4011)->label == "White Smith");
  CHECK(row_of(4011)->detail == "Whitesmith");
  CHECK(row_of(4008)->detail.empty());
  // Sans nom au client, le nom du serveur seul.
  CHECK(row_of(0)->label == "Novice");
  CHECK(row_of(0)->detail.empty());
  // L'ordre de présentation par famille tient : les novices d'abord.
  CHECK(rows.front().family == ro::ui::StaffJobFamily::kNovice);
}

TEST_CASE("Le catalogue porte les choix sur « @jobchange » seulement") {
  moonclient::ChatCommandCatalogue catalogue;
  catalogue.Refresh(99, {});
  bool found = false;
  for (const ro::ui::ChatCommandHint& hint : catalogue.hints()) {
    INFO(hint.name);
    const bool jobchange = hint.sigil == '@' && hint.name == "jobchange";
    CHECK(hint.argument_choices.empty() == !jobchange);
    if (jobchange) {
      found = true;
      CHECK(hint.aliases.find("job") != std::string_view::npos);
      CHECK(hint.argument_choices.size() == ServerJobChangeTargets().size());
    }
  }
  CHECK(found);
}
