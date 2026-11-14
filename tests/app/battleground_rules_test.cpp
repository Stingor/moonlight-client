// LES RÈGLES PURES DU CHAMP DE BATAILLE : l'inscription refusée sans rien
// envoyer, les avis, la condition d'entrée, les sons.

#include "game/battleground_rules.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::ApplyFacts;
using moonclient::ApplyRefusal;
using moonclient::BattlegroundField;
using moonclient::ConditionText;
using moonclient::HasApplyDelay;
using moonclient::InterfaceSoundPlace;
using moonclient::NoticeText;
using moonclient::OnlinePartyMembers;
using moonclient::SoundFileOf;
using moonclient::TeamText;
using ro::sim::ActiveStatus;
using ro::sim::EntryQueueApplyKind;
using ro::sim::EntryQueueNoticeKind;
using ro::sim::EntryQueueSound;
using ro::sim::PartyMember;

namespace {

// Un joueur prêt à s'inscrire seul : liaison prête, hors champ de bataille.
[[nodiscard]] ApplyFacts Ready() { return ApplyFacts{.link_ready = true}; }

// La raison, ou « part » quand il n'y en a pas.
[[nodiscard]] std::string Said(const char* refusal) {
  return refusal != nullptr ? std::string(refusal) : std::string("part");
}

}  // namespace

TEST_CASE("Règles du champ : seul, l'inscription part dès que la liaison est prête",
          "[app][battleground]") {
  CHECK(ApplyRefusal(EntryQueueApplyKind::kSolo, Ready()) == nullptr);
  CHECK(ApplyRefusal(EntryQueueApplyKind::kSolo, ApplyFacts{}) != nullptr);
}

TEST_CASE("Règles du champ : on ne s'inscrit pas depuis un champ de bataille",
          "[app][battleground]") {
  ApplyFacts facts = Ready();
  facts.on_battlefield = true;
  facts.in_party = true;
  facts.party_leader = true;
  facts.in_guild = true;
  facts.guild_master = true;
  for (const EntryQueueApplyKind kind :
       {EntryQueueApplyKind::kSolo, EntryQueueApplyKind::kParty, EntryQueueApplyKind::kGuild}) {
    CHECK(ApplyRefusal(kind, facts) != nullptr);
  }
}

TEST_CASE("Règles du champ : la liaison se juge avant tout le reste", "[app][battleground]") {
  ApplyFacts facts;
  facts.on_battlefield = true;
  ApplyFacts on_field = Ready();
  on_field.on_battlefield = true;
  CHECK(Said(ApplyRefusal(EntryQueueApplyKind::kSolo, facts)) !=
        Said(ApplyRefusal(EntryQueueApplyKind::kSolo, on_field)));
}

TEST_CASE("Règles du champ : le délai d'une file quittée refuse toute inscription",
          "[app][battleground]") {
  ApplyFacts facts = Ready();
  facts.apply_delayed = true;
  facts.in_party = true;
  facts.party_leader = true;
  facts.in_guild = true;
  facts.guild_master = true;
  const std::string delayed = Said(ApplyRefusal(EntryQueueApplyKind::kSolo, facts));
  CHECK(delayed != "part");
  CHECK(Said(ApplyRefusal(EntryQueueApplyKind::kParty, facts)) == delayed);
  CHECK(Said(ApplyRefusal(EntryQueueApplyKind::kGuild, facts)) == delayed);
  // LA CARTE SE JUGE AVANT LE DÉLAI.
  facts.on_battlefield = true;
  CHECK(Said(ApplyRefusal(EntryQueueApplyKind::kSolo, facts)) != delayed);
}

TEST_CASE("Règles du champ : le délai se lit à l'état que le serveur pose sur le joueur",
          "[app][battleground]") {
  std::vector<ActiveStatus> statuses = {ActiveStatus{.effect = ro::sim::kEfstNight}};
  CHECK_FALSE(HasApplyDelay(statuses));
  statuses.push_back(ActiveStatus{.effect = ro::sim::kEfstEntryQueueApplyDelay});
  CHECK(HasApplyDelay(statuses));
  CHECK_FALSE(HasApplyDelay({}));
}

TEST_CASE(
    "Règles du champ : en party, le chef seul inscrit, et ses connectés tiennent dans un camp",
    "[app][battleground]") {
  ApplyFacts facts = Ready();
  CHECK(ApplyRefusal(EntryQueueApplyKind::kParty, facts) != nullptr);
  facts.in_party = true;
  const std::string member = Said(ApplyRefusal(EntryQueueApplyKind::kParty, facts));
  CHECK(member != "part");
  CHECK(member != Said(ApplyRefusal(EntryQueueApplyKind::kParty, Ready())));
  facts.party_leader = true;
  CHECK(ApplyRefusal(EntryQueueApplyKind::kParty, facts) == nullptr);
  facts.team_most = 3;
  facts.party_online = 3;
  CHECK(ApplyRefusal(EntryQueueApplyKind::kParty, facts) == nullptr);
  facts.party_online = 4;
  const std::string crowded = Said(ApplyRefusal(EntryQueueApplyKind::kParty, facts));
  CHECK(crowded != "part");
  CHECK(crowded != member);
  // Un terrain qui ne dit pas sa taille ne refuse rien.
  facts.team_most = 0;
  CHECK(ApplyRefusal(EntryQueueApplyKind::kParty, facts) == nullptr);
}

TEST_CASE("Règles du champ : les connectés d'une party se comptent, le joueur compris",
          "[app][battleground]") {
  std::vector<PartyMember> members(3);
  members[1].offline = true;
  CHECK(OnlinePartyMembers(members) == 2);
  CHECK(OnlinePartyMembers({}) == 0);
}

TEST_CASE("Règles du champ : en guilde, le maître inscrit, et ses connectés tiennent dans un camp",
          "[app][battleground]") {
  ApplyFacts facts = Ready();
  CHECK(ApplyRefusal(EntryQueueApplyKind::kGuild, facts) != nullptr);
  facts.in_guild = true;
  CHECK(ApplyRefusal(EntryQueueApplyKind::kGuild, facts) != nullptr);
  facts.guild_master = true;
  CHECK(ApplyRefusal(EntryQueueApplyKind::kGuild, facts) == nullptr);
  facts.team_most = 15;
  facts.guild_online = 16;
  CHECK(ApplyRefusal(EntryQueueApplyKind::kGuild, facts) != nullptr);
  // Les connectés de la party ne comptent pas pour la guilde.
  facts.guild_online = 15;
  facts.party_online = 40;
  CHECK(ApplyRefusal(EntryQueueApplyKind::kGuild, facts) == nullptr);
  // Être chef de party n'ouvre pas l'inscription de guilde.
  ApplyFacts leader = Ready();
  leader.in_party = true;
  leader.party_leader = true;
  leader.in_guild = true;
  CHECK(ApplyRefusal(EntryQueueApplyKind::kGuild, leader) != nullptr);
}

TEST_CASE("Règles du champ : chaque avis a sa phrase, et deux avis distincts deux phrases",
          "[app][battleground]") {
  const std::array kinds = {
      EntryQueueNoticeKind::kQueued,        EntryQueueNoticeKind::kRefusedElsewhere,
      EntryQueueNoticeKind::kQueueFinished, EntryQueueNoticeKind::kUnknownName,
      EntryQueueNoticeKind::kUnknownType,   EntryQueueNoticeKind::kTooMany,
      EntryQueueNoticeKind::kLevel,         EntryQueueNoticeKind::kDuplicate,
      EntryQueueNoticeKind::kReconnect,     EntryQueueNoticeKind::kLeaderOnly,
      EntryQueueNoticeKind::kJobRefused,    EntryQueueNoticeKind::kCancelled,
      EntryQueueNoticeKind::kCancelRefused, EntryQueueNoticeKind::kDeclined,
      EntryQueueNoticeKind::kAdmitted,      EntryQueueNoticeKind::kNoAnswer};
  for (std::size_t first = 0; first < kinds.size(); ++first) {
    const std::string said = NoticeText(kinds[first]);
    CHECK_FALSE(said.empty());
    for (std::size_t second = first + 1; second < kinds.size(); ++second) {
      CHECK(said != NoticeText(kinds[second]));
    }
  }
}

TEST_CASE("Règles du champ : la condition d'entrée dit ses bornes", "[app][battleground]") {
  BattlegroundField field;
  CHECK(ConditionText(field).empty());
  field.level_min = 80;
  const std::string at_least = ConditionText(field);
  CHECK(at_least.find("80") != std::string::npos);
  field.level_min = 0;
  field.level_max = 80;
  const std::string at_most = ConditionText(field);
  CHECK(at_most.find("80") != std::string::npos);
  CHECK(at_most != at_least);
  field.level_min = 60;
  field.level_max = 79;
  const std::string between = ConditionText(field);
  CHECK(between.find("60") != std::string::npos);
  CHECK(between.find("79") != std::string::npos);
}

TEST_CASE("Règles du champ : l'effectif dit un nombre fixe ou une fourchette",
          "[app][battleground]") {
  BattlegroundField field;
  CHECK(TeamText(field).empty());
  field.team_min = 3;
  field.team_max = 3;
  const std::string fixed = TeamText(field);
  CHECK(fixed.find('3') != std::string::npos);
  field.team_max = 15;
  const std::string range = TeamText(field);
  CHECK(range.find("15") != std::string::npos);
  CHECK(range != fixed);
  field.team_max = 0;
  const std::string at_least = TeamText(field);
  CHECK(at_least.find('3') != std::string::npos);
  CHECK(at_least != range);
}

TEST_CASE("Règles du champ : les trois sons viennent des archives du jeu", "[app][battleground]") {
  CHECK(SoundFileOf(EntryQueueSound::kRequest).path == "se_btg_request.wav");
  CHECK(SoundFileOf(EntryQueueSound::kReady).path == "se_btg_ready.wav");
  CHECK(SoundFileOf(EntryQueueSound::kForward).path == "se_btg_forward.wav");
  for (const EntryQueueSound sound :
       {EntryQueueSound::kRequest, EntryQueueSound::kReady, EntryQueueSound::kForward}) {
    CHECK(SoundFileOf(sound).place == InterfaceSoundPlace::kSoundArchive);
  }
}
