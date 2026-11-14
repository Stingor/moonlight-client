// CE QUE LE JOUEUR PEUT DANS SA GUILDE.
//
// 🔴 DEUX RÈGLES MUETTES EN JEU : un faux « pas maître » ou un faux « pas le
// droit » ne produit aucune erreur, seulement des boutons qui manquent. On ne
// le verrait qu'en étant maître, ou en tenant le bon poste.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/guild.h"

using ro::sim::GuildBelonging;
using ro::sim::GuildBook;
using ro::sim::GuildMember;
using ro::sim::GuildRights;
using ro::sim::GuildSummary;

namespace {

constexpr std::int32_t kGuildId = 42;
constexpr std::uint32_t kMe = 150001;
constexpr std::uint32_t kOther = 150002;
constexpr std::int32_t kOfficerPosition = 3;

GuildBook BookWith(bool master_flag, std::int32_t my_position) {
  GuildBook book;
  book.TellBelonging(GuildBelonging{.guild_id = kGuildId, .master = master_flag, .name = "Lune"});
  std::vector<GuildMember> members;
  members.push_back(GuildMember{.char_id = kMe, .name = "Selene", .position = my_position});
  members.push_back(GuildMember{.char_id = kOther, .name = "Helios", .position = 1});
  book.ReplaceMembers(std::move(members));
  return book;
}

}  // namespace

TEST_CASE("Guilde : hors de toute guilde, personne n'est maître", "[sim][guild]") {
  const GuildBook book;
  CHECK_FALSE(book.IsMaster(kMe));
  const GuildRights rights = book.RightsOf(kMe);
  CHECK_FALSE(rights.known);
  CHECK_FALSE(rights.may_invite);
  CHECK_FALSE(rights.may_expel);
}

TEST_CASE("Guilde : l'identifiant de l'en-tête prime sur le nom et le drapeau", "[sim][guild]") {
  GuildBook book = BookWith(/*master_flag=*/true, 1);
  // Le nom désignerait le joueur, le drapeau aussi : seul l'identifiant compte.
  book.TellSummary(GuildSummary{.guild_id = kGuildId,
                                .name = "Lune",
                                .master_char_id = static_cast<std::int32_t>(kOther),
                                .master_name = "Selene"});
  CHECK_FALSE(book.IsMaster(kMe));
  CHECK(book.IsMaster(kOther));
}

TEST_CASE("Guilde : sans identifiant, le nom du maître désigne, casse ignorée", "[sim][guild]") {
  GuildBook book = BookWith(/*master_flag=*/false, 1);
  book.TellSummary(GuildSummary{.guild_id = kGuildId, .name = "Lune", .master_name = "SELENE"});
  CHECK(book.IsMaster(kMe));
  CHECK_FALSE(book.IsMaster(kOther));
}

TEST_CASE("Guilde : sans en-tête, le drapeau de l'appartenance tranche", "[sim][guild]") {
  CHECK(BookWith(/*master_flag=*/true, 0).IsMaster(kMe));
  CHECK_FALSE(BookWith(/*master_flag=*/false, 0).IsMaster(kMe));
}

TEST_CASE("Guilde : des droits inconnus retombent sur « maître », le storage sur non",
          "[sim][guild]") {
  const GuildBook master = BookWith(/*master_flag=*/true, 0);
  const GuildRights granted = master.RightsOf(kMe);
  CHECK_FALSE(granted.known);
  CHECK(granted.may_invite);
  CHECK(granted.may_expel);
  CHECK_FALSE(granted.may_storage);

  const GuildRights refused = BookWith(/*master_flag=*/false, 1).RightsOf(kMe);
  CHECK_FALSE(refused.known);
  CHECK_FALSE(refused.may_invite);
  CHECK_FALSE(refused.may_expel);
}

TEST_CASE("Guilde : des droits connus viennent du POSTE, pas du drapeau de maître",
          "[sim][guild]") {
  GuildBook officer = BookWith(/*master_flag=*/false, kOfficerPosition);
  officer.TellPositionRights(kOfficerPosition, ro::sim::kGuildMayInvite | ro::sim::kGuildMayStorage,
                             0);
  const GuildRights granted = officer.RightsOf(kMe);
  CHECK(granted.known);
  CHECK(granted.may_invite);
  CHECK_FALSE(granted.may_expel);
  CHECK(granted.may_storage);

  // Un maître dont le poste ne permet pas d'expulser ne le peut pas : c'est
  // le masque que le serveur consulte.
  GuildBook master = BookWith(/*master_flag=*/true, kOfficerPosition);
  master.TellPositionRights(kOfficerPosition, ro::sim::kGuildMayInvite, 0);
  CHECK(master.IsMaster(kMe));
  CHECK_FALSE(master.RightsOf(kMe).may_expel);
}

TEST_CASE("Guilde : seul le maître lance les compétences, sauf le cri de charge", "[sim][guild]") {
  constexpr std::uint16_t kEmergencyCall = 10013;
  const GuildBook master = BookWith(/*master_flag=*/true, 0);
  CHECK(master.MayWield(kEmergencyCall, kMe));
  const GuildBook member = BookWith(/*master_flag=*/false, 1);
  CHECK_FALSE(member.MayWield(kEmergencyCall, kMe));
  // Le cri de charge, lui, est à tout membre — mais pas à qui n'a pas de guilde.
  CHECK(member.MayWield(ro::sim::kMemberGuildSkill, kMe));
  CHECK_FALSE(GuildBook().MayWield(ro::sim::kMemberGuildSkill, kMe));
}

TEST_CASE("Guilde : un membre absent de la liste n'a que le repli", "[sim][guild]") {
  GuildBook book = BookWith(/*master_flag=*/false, kOfficerPosition);
  book.TellPositionRights(kOfficerPosition, ro::sim::kGuildRightsMask, 0);
  constexpr std::uint32_t kStranger = 150099;
  const GuildRights rights = book.RightsOf(kStranger);
  CHECK_FALSE(rights.known);
  CHECK_FALSE(rights.may_invite);
}
