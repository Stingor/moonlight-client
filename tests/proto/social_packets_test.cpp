// Les paquets de groupe et d'amis écrits à la main : chaque disposition est
// recoupée avec la table du serveur, octet par octet.

#include "ro/proto/social_packets.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/wire_bytes.h"

using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

void PutText(std::vector<std::uint8_t>& into, const std::string& text, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index) {
    into.push_back(index < text.size() ? static_cast<std::uint8_t>(text[index]) : 0);
  }
}

}  // namespace

TEST_CASE("Social : les cinq demandes partent aux octets près", "[party][friend]") {
  ro::proto::FriendAddRequest add;
  add.name = "Alice";
  const auto add_bytes = ro::proto::Encode(add);
  REQUIRE(add_bytes.has_value());
  std::vector<std::uint8_t> expected_add;
  PutU16(expected_add, 0x0202);
  PutText(expected_add, "Alice", 24);
  CHECK(*add_bytes == expected_add);

  ro::proto::FriendRemoveRequest remove;
  remove.account_id = 2000001;
  remove.char_id = 150001;
  std::vector<std::uint8_t> expected_remove;
  PutU16(expected_remove, 0x0203);
  PutU32(expected_remove, 2000001);
  PutU32(expected_remove, 150001);
  CHECK(ro::proto::Encode(remove).value() == expected_remove);

  ro::proto::FriendAnswerRequest answer;
  answer.account_id = 7;
  answer.char_id = 8;
  answer.answer = ro::proto::FriendAnswerRequest::kAccepted;
  std::vector<std::uint8_t> expected_answer;
  PutU16(expected_answer, 0x0208);
  PutU32(expected_answer, 7);
  PutU32(expected_answer, 8);
  PutU32(expected_answer, 1);
  CHECK(ro::proto::Encode(answer).value() == expected_answer);

  ro::proto::PartyLeaderRequest leader;
  leader.account_id = 2000002;
  std::vector<std::uint8_t> expected_leader;
  PutU16(expected_leader, 0x07DA);
  PutU32(expected_leader, 2000002);
  CHECK(ro::proto::Encode(leader).value() == expected_leader);

  ro::proto::PartyOptionsRequest options;
  options.exp_share = 1;
  options.item_pickup = 0;
  options.item_share = 1;
  const std::vector<std::uint8_t> expected_options = {0xD7, 0x07, 1, 0, 0, 0, 0, 1};
  CHECK(ro::proto::Encode(options).value() == expected_options);
}

TEST_CASE("Social : les avis reçus se relisent", "[party][friend]") {
  std::vector<std::uint8_t> request;
  PutU16(request, 0x0207);
  PutU32(request, 11);
  PutU32(request, 12);
  PutText(request, "Bob", 24);
  const auto asked = ro::proto::DecodeFriendRequestNotice(request);
  REQUIRE(asked.has_value());
  CHECK(asked->account_id == 11);
  CHECK(asked->char_id == 12);
  CHECK(asked->name == "Bob");

  std::vector<std::uint8_t> result;
  PutU16(result, 0x0209);
  PutU16(result, 3);
  PutU32(result, 21);
  PutU32(result, 22);
  PutText(result, "Carole", 24);
  const auto answered = ro::proto::DecodeFriendAddResult(result);
  REQUIRE(answered.has_value());
  CHECK(answered->result == 3);
  CHECK(answered->account_id == 21);
  CHECK(answered->char_id == 22);
  CHECK(answered->name == "Carole");

  std::vector<std::uint8_t> removed;
  PutU16(removed, 0x020A);
  PutU32(removed, 31);
  PutU32(removed, 32);
  const auto gone = ro::proto::DecodeFriendRemovedNotice(removed);
  REQUIRE(gone.has_value());
  CHECK(gone->account_id == 31);
  CHECK(gone->char_id == 32);

  const std::vector<std::uint8_t> options = {0xD8, 0x07, 2, 0, 0, 0, 1, 0};
  const auto told = ro::proto::DecodePartyOptionsNotice(options);
  REQUIRE(told.has_value());
  CHECK(told->exp_share == ro::proto::PartyOptionsNotice::kExpChangeRefused);
  CHECK(told->item_pickup == 1);
  CHECK(told->item_share == 0);

  // Une trame d'une longueur que la table ne déclare pas se refuse.
  removed.pop_back();
  CHECK_FALSE(ro::proto::DecodeFriendRemovedNotice(removed).has_value());
}

TEST_CASE("Social : l'état du partage se lit jusqu'où la trame le porte", "[party]") {
  std::vector<std::uint8_t> frame;
  PutU16(frame, 0x0F35);
  PutU16(frame, 0);
  PutU16(frame, 15);
  // Trois entrées annoncées, deux portées, dont une de compte nul.
  PutU16(frame, 3);
  PutU32(frame, 2000001);
  frame.push_back(0x01);
  PutU32(frame, 0);
  frame.push_back(0x02);
  frame[2] = static_cast<std::uint8_t>(frame.size());
  const auto state = ro::proto::DecodePartyShareState(frame);
  REQUIRE(state.has_value());
  CHECK(state->idle_s == 15);
  REQUIRE(state->entries.size() == 1);
  CHECK(state->entries[0].account_id == 2000001);
  CHECK(state->entries[0].flags == 0x01);

  // Une longueur annoncée au-delà de la trame est une trame tronquée.
  frame[2] = static_cast<std::uint8_t>(frame.size() + 5);
  CHECK_FALSE(ro::proto::DecodePartyShareState(frame).has_value());
}

TEST_CASE("Social : les demandes de blocage partent aux octets près", "[whisper_block]") {
  ro::proto::WhisperBlockRequest block;
  block.name = "Alice";
  block.type = ro::proto::WhisperBlockRequest::kAllow;
  std::vector<std::uint8_t> expected_block;
  PutU16(expected_block, 0x00CF);
  PutText(expected_block, "Alice", 24);
  expected_block.push_back(ro::proto::WhisperBlockRequest::kAllow);
  CHECK(ro::proto::Encode(block).value() == expected_block);

  std::vector<std::uint8_t> expected_list;
  PutU16(expected_list, 0x00D3);
  CHECK(ro::proto::Encode(ro::proto::WhisperBlockListRequest{}).value() == expected_list);
}
