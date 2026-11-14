// LES TROIS PAQUETS DES TITRES, écrits à la main.
//
// 🔴 CE BANC PORTE CE QU'UNE SESSION NE MONTRERAIT PAS :
//
//   · la demande de changement se recoupe avec le serveur, qui l'enregistre sans
//     déclarer de champ — une disposition qui dériverait refuserait l'envoi ;
//   · la liste des succès se refuse dès que son compte ne tombe pas sur sa
//     longueur : lue à moitié, elle donnerait des titres tirés du bourrage.

#include "ro/proto/title_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::AchievementEntry;
using ro::proto::AchievementList;
using ro::proto::ChangeTitleAck;
using ro::proto::ChangeTitleRequest;

namespace {

[[nodiscard]] std::uint32_t U32At(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  return static_cast<std::uint32_t>(bytes[at]) | (static_cast<std::uint32_t>(bytes[at + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[at + 2]) << 16) |
         (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
}

[[nodiscard]] AchievementList TwoAchievements() {
  AchievementList list;
  list.total_points = 150;
  list.rank = 3;
  list.current_rank_points = 20;
  list.next_rank_points = 90;
  AchievementEntry claimed;
  claimed.achievement_id = 900007;
  claimed.completed = true;
  claimed.objectives[0] = 200;
  claimed.completed_at = 1758800000;
  claimed.rewarded = true;
  AchievementEntry running;
  running.achievement_id = 128001;
  running.objectives[9] = 7;
  list.entries = {claimed, running};
  return list;
}

}  // namespace

TEST_CASE("Titres : la demande de changement porte le titre après l'opcode", "[proto][titres]") {
  const auto frame = ro::proto::Encode(ChangeTitleRequest{.title_id = 1103});
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == ChangeTitleRequest::kWireSize);
  CHECK(((*frame)[0] | ((*frame)[1] << 8)) == ChangeTitleRequest::kOpcode);
  CHECK(U32At(*frame, 2) == 1103);
}

TEST_CASE("Succès : la réclamation porte le numéro du succès après l'opcode", "[proto][titres]") {
  using ro::proto::AchievementRewardRequest;
  const auto frame = ro::proto::Encode(AchievementRewardRequest{.achievement_id = 900007});
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == AchievementRewardRequest::kWireSize);
  CHECK(((*frame)[0] | ((*frame)[1] << 8)) == AchievementRewardRequest::kOpcode);
  CHECK(U32At(*frame, 2) == 900007);
}

TEST_CASE("Titres : l'accusé se relit tel qu'il est écrit", "[proto][titres]") {
  const auto ack = ro::proto::DecodeChangeTitleAck(
      ro::proto::EncodeChangeTitleAck(ChangeTitleAck{.result = 1, .title_id = 1046}));
  REQUIRE(ack.has_value());
  CHECK(ack->result == 1);
  CHECK(ack->title_id == 1046);
}

TEST_CASE("Titres : un accusé d'une autre longueur se refuse", "[proto][titres]") {
  std::vector<std::uint8_t> frame = ro::proto::EncodeChangeTitleAck(ChangeTitleAck{});
  frame.pop_back();
  CHECK_FALSE(ro::proto::DecodeChangeTitleAck(frame).has_value());
}

TEST_CASE("Titres : la liste des succès se relit entrée par entrée", "[proto][titres]") {
  const auto list =
      ro::proto::DecodeAchievementList(ro::proto::EncodeAchievementList(TwoAchievements()));
  REQUIRE(list.has_value());
  CHECK(list->total_points == 150);
  CHECK(list->rank == 3);
  CHECK(list->next_rank_points == 90);
  REQUIRE(list->entries.size() == 2);
  CHECK(list->entries[0].achievement_id == 900007);
  CHECK(list->entries[0].completed);
  CHECK(list->entries[0].rewarded);
  CHECK(list->entries[0].objectives[0] == 200);
  CHECK(list->entries[0].completed_at == 1758800000);
  CHECK(list->entries[1].achievement_id == 128001);
  CHECK_FALSE(list->entries[1].completed);
  CHECK_FALSE(list->entries[1].rewarded);
  CHECK(list->entries[1].objectives[9] == 7);
}

TEST_CASE("Titres : une liste vide est une liste, pas une panne", "[proto][titres]") {
  const auto list = ro::proto::DecodeAchievementList(ro::proto::EncodeAchievementList({}));
  REQUIRE(list.has_value());
  CHECK(list->entries.empty());
}

TEST_CASE("Titres : un compte qui ne tombe pas sur la longueur refuse la liste",
          "[proto][titres]") {
  std::vector<std::uint8_t> frame = ro::proto::EncodeAchievementList(TwoAchievements());
  // Le compte, juste après l'opcode et la longueur, annonce une entrée de plus.
  frame[4] = 3;
  CHECK_FALSE(ro::proto::DecodeAchievementList(frame).has_value());
}

TEST_CASE("Titres : une liste coupée se refuse", "[proto][titres]") {
  std::vector<std::uint8_t> frame = ro::proto::EncodeAchievementList(TwoAchievements());
  frame.resize(frame.size() - 1);
  CHECK_FALSE(ro::proto::DecodeAchievementList(frame).has_value());
}
