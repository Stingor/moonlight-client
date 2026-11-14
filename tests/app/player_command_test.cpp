// LES COMMANDES DU JOUEUR : ce que chaque ligne demande, sous ses noms et ses
// doubles, et les formes que le client du joueur refuse.

#include "chat/player_command.h"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "chat/emotion_commands.h"
#include "chat/native_slash_commands.h"

using moonclient::PlayerCommand;
using moonclient::PlayerCommandKind;
using moonclient::ReadPlayerCommand;

namespace {

[[nodiscard]] PlayerCommandKind KindOf(std::string_view line) {
  return ReadPlayerCommand(line).kind;
}

}  // namespace

TEST_CASE("Commandes du joueur : s'asseoir et se lever basculent", "[app][chat][player_command]") {
  for (const std::string_view line : {"/sit", "/stand", "/SIT", "/dkswrl", "/tjrl", "/sit plus"}) {
    INFO(line);
    CHECK(KindOf(line) == PlayerCommandKind::kSitOrStand);
  }
  CHECK(KindOf("/sitting") == PlayerCommandKind::kNone);
}

TEST_CASE("Commandes du joueur : un nom sans espace, ni plus ni moins",
          "[app][chat][player_command]") {
  const PlayerCommand deal = ReadPlayerCommand("/deal Aurélie");
  CHECK(deal.kind == PlayerCommandKind::kDeal);
  CHECK(deal.name == "Aurélie");
  CHECK_FALSE(deal.malformed);
  CHECK(ReadPlayerCommand("/rjfo Bob").name == "Bob");
  CHECK(ReadPlayerCommand("/ryghks   Bob").name == "Bob");
  // Sans nom, ou avec un nom à espace : la forme est fausse, et se dit.
  CHECK(ReadPlayerCommand("/deal").malformed);
  CHECK(ReadPlayerCommand("/deal Bob Leponge").malformed);

  CHECK(ReadPlayerCommand("/organize Lune").kind == PlayerCommandKind::kOrganizeParty);
  CHECK(ReadPlayerCommand("/ruftjd Lune").name == "Lune");
  CHECK(ReadPlayerCommand("/expel Bob").kind == PlayerCommandKind::kExpelFromParty);
  CHECK(ReadPlayerCommand("/wpaud Bob").name == "Bob");
  CHECK(ReadPlayerCommand("/guild Lune").kind == PlayerCommandKind::kCreateGuild);
  CHECK(ReadPlayerCommand("/rlfem Lune").name == "Lune");
  CHECK(ReadPlayerCommand("/breakguild Lune").kind == PlayerCommandKind::kBreakGuild);
  CHECK(ReadPlayerCommand("/gocp Lune").name == "Lune");
  for (const std::string_view line : {"/organize", "/expel", "/guild", "/breakguild a b"}) {
    INFO(line);
    CHECK(ReadPlayerCommand(line).malformed);
  }
}

TEST_CASE("Commandes du joueur : une invitation prend toute la suite de la ligne",
          "[app][chat][player_command]") {
  const PlayerCommand party = ReadPlayerCommand("/invite Bob Leponge");
  CHECK(party.kind == PlayerCommandKind::kInviteToParty);
  CHECK(party.name == "Bob Leponge");
  CHECK(ReadPlayerCommand("/cheo Bob").name == "Bob");
  const PlayerCommand guild = ReadPlayerCommand("/guildinvite Bob Leponge");
  CHECK(guild.kind == PlayerCommandKind::kInviteToGuild);
  CHECK(guild.name == "Bob Leponge");
  CHECK(ReadPlayerCommand("/rlfemcheo Bob").name == "Bob");
  CHECK(ReadPlayerCommand("/invite").malformed);
  CHECK(ReadPlayerCommand("/guildinvite").malformed);
}

TEST_CASE("Commandes du joueur : groupe, réglages et questions sans argument",
          "[app][chat][player_command]") {
  CHECK(KindOf("/leave") == PlayerCommandKind::kLeaveParty);
  CHECK(KindOf("/xkfxhl") == PlayerCommandKind::kLeaveParty);
  CHECK(KindOf("/accept") == PlayerCommandKind::kAllowPartyInvites);
  CHECK(KindOf("/refuse") == PlayerCommandKind::kRefusePartyInvites);
  CHECK(KindOf("/notrade") == PlayerCommandKind::kToggleDealRefusal);
  CHECK(KindOf("/nt") == PlayerCommandKind::kToggleDealRefusal);
  CHECK(KindOf("/who") == PlayerCommandKind::kUserCount);
  CHECK(KindOf("/wjqthrwk") == PlayerCommandKind::kUserCount);
  CHECK(KindOf("/snrn") == PlayerCommandKind::kUserCount);
  CHECK(KindOf("/w") == PlayerCommandKind::kUserCount);
  // « /w nom » est un chuchotement, jamais une question.
  CHECK(KindOf("/w Bob") == PlayerCommandKind::kNone);
  CHECK(KindOf("/help") == PlayerCommandKind::kHelp);
  CHECK(KindOf("/h") == PlayerCommandKind::kHelp);
  CHECK(KindOf("/emotion") == PlayerCommandKind::kEmotionList);
}

TEST_CASE("Commandes du joueur : les volumes, lus comme atoi et bornés",
          "[app][chat][player_command]") {
  const PlayerCommand sound = ReadPlayerCommand("/v 64");
  CHECK(sound.kind == PlayerCommandKind::kSoundVolume);
  REQUIRE(sound.volume.has_value());
  CHECK(*sound.volume == 64);
  const PlayerCommand music = ReadPlayerCommand("/bv 100");
  CHECK(music.kind == PlayerCommandKind::kMusicVolume);
  CHECK(music.volume == 100);
  // Les bornes du client du joueur : de 0 à 127.
  CHECK(ReadPlayerCommand("/v 0").volume == 0);
  CHECK(ReadPlayerCommand("/v 127").volume == 127);
  CHECK(ReadPlayerCommand("/v 128").volume == 127);
  CHECK(ReadPlayerCommand("/v 99999999999999").volume == 127);
  CHECK(ReadPlayerCommand("/v -5").volume == 0);
  // Ce qu'atoi lit : les chiffres de tête, et zéro sans chiffre.
  CHECK(ReadPlayerCommand("/v 42abc").volume == 42);
  CHECK(ReadPlayerCommand("/v abc").volume == 0);
  CHECK(ReadPlayerCommand("/bv +7").volume == 7);
  // Sans nombre, la commande dit le volume en cours.
  CHECK_FALSE(ReadPlayerCommand("/v").volume.has_value());
  CHECK(ReadPlayerCommand("/bv").kind == PlayerCommandKind::kMusicVolume);
}

TEST_CASE("Commandes du joueur : l'échelle des volumes et la fraction des options",
          "[app][chat][player_command]") {
  CHECK(moonclient::VolumeFractionOf(0) == 0.0F);
  CHECK(moonclient::VolumeFractionOf(127) == 1.0F);
  CHECK(moonclient::VolumeFractionOf(500) == 1.0F);
  CHECK(moonclient::CommandVolumeOf(1.0F) == 127);
  CHECK(moonclient::CommandVolumeOf(0.5F) == 64);
  CHECK(moonclient::CommandVolumeOf(-1.0F) == 0);
  for (int volume = 0; volume <= moonclient::kLoudestCommandVolume; ++volume) {
    INFO(volume);
    CHECK(moonclient::CommandVolumeOf(moonclient::VolumeFractionOf(volume)) == volume);
  }
}

TEST_CASE("Commandes du joueur : chacune est un nom que le client du joueur connaît",
          "[app][chat][player_command]") {
  // Une commande reprise ne doit pas ouvrir de fuite le jour où sa prise
  // recule : la barrière des noms connus la tient encore.
  for (const std::string_view line :
       {"/sit",   "/stand",       "/deal",       "/organize", "/invite", "/leave",   "/expel",
        "/guild", "/guildinvite", "/breakguild", "/accept",   "/refuse", "/notrade", "/nt",
        "/who",   "/help",        "/h",          "/emotion",  "/v",      "/bv"}) {
    INFO(line);
    CHECK(moonclient::NativeKnowsSlashCommand(line));
    CHECK(moonclient::PlayerCommandWord(KindOf(line)).size() > 0);
  }
}

TEST_CASE("Émoticônes : les commandes nommées et numérotées du client du joueur",
          "[app][chat][player_command]") {
  using moonclient::EmotionCommand;
  CHECK(EmotionCommand("/sorry") == 17);
  CHECK(EmotionCommand("/smile") == 18);
  CHECK(EmotionCommand("/e1") == 36);
  CHECK(EmotionCommand("/e10") == 45);
  CHECK(EmotionCommand("/e13") == 52);
  CHECK(EmotionCommand("/e18") == 57);
  CHECK(EmotionCommand("/e20") == 65);
  CHECK(EmotionCommand("/hum") == 74);
  CHECK(EmotionCommand("/e27") == 74);
  CHECK(EmotionCommand("/oops") == 76);
  CHECK(EmotionCommand("/whisp") == 80);
  CHECK(EmotionCommand("/e33") == 80);
  // Le premier dé : le serveur tire la face.
  CHECK(EmotionCommand("/dice") == 58);
  CHECK(EmotionCommand("/e19") == 58);
  // Les doubles du clavier coréen.
  CHECK(EmotionCommand("/rlQma") == 2);
  CHECK(EmotionCommand("/qh") == 12);
  // Les trous de la numérotation du client restent des trous.
  CHECK_FALSE(EmotionCommand("/e21").has_value());
  CHECK_FALSE(EmotionCommand("/e34").has_value());
  // La commande numérotée maison ne change pas.
  CHECK(EmotionCommand("/e 5") == 5);
  // La forme lisible l'emporte sur le numéro, et les anciennes ne bougent pas.
  CHECK(moonclient::EmotionCommandOf(37) == "/rice");
  CHECK(moonclient::EmotionCommandOf(17) == "/sry");
  CHECK(moonclient::EmotionListLines().size() == 38);
  CHECK(moonclient::EmotionListLines().front() == "/lv2");
}
