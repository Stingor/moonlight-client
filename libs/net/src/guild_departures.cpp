// Le départ et l'expulsion d'un membre, écrits dans la conversation. Voir
// l'en-tête pour l'ordre des lignes et ce que le joueur perd quand c'est lui.

#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/net/guild.h"
#include "ro/sim/actor.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

using core::Tr;

// LES COULEURS, en `0xRRGGBB` : le jaune d'une nouvelle de la guilde, le rouge
// du constat qui nous concerne.
constexpr std::uint32_t kNewsColor = 0xFFFF00;
constexpr std::uint32_t kOwnDepartureColor = 0xFF0000;

// De quoi composer une phrase à trou : un nom de vingt-quatre octets et un
// motif de quarante y tiennent largement, même convertis.
constexpr std::size_t kComposedSize = 256;

void Say(sim::World& world, std::string text, std::uint32_t color, std::uint32_t now_ms) {
  world.RecordChat(sim::ChatKind::kServer, sim::ChatType::kGuildChat, 0, std::move(text), now_ms,
                   color);
}

// LE PERSONNAGE JOUÉ N'A PLUS DE GUILDE, sans attendre que le serveur le
// rediffuse aux alentours.
void ForgetOwnGuild(sim::World& world) {
  const sim::Actor* player = world.Find(world.player_id());
  if (player == nullptr || player->guild_id == 0) {
    return;
  }
  sim::Actor again = *player;
  again.guild_id = 0;
  world.Spawn(std::move(again));
}

void SayDeparture(const sim::GuildDeparture& departure, sim::World& world,
                  const sim::BannedWords* banned, std::uint32_t now_ms) {
  if (departure.own) {
    Say(world,
        departure.expelled ? Tr("Vous avez été expulsé de la guilde.")
                           : Tr("Vous avez quitté la guilde."),
        kOwnDepartureColor, now_ms);
    ForgetOwnGuild(world);
    return;
  }
  // ⚠ LE MOTIF SE MASQUE AVEC SA LIGNE ENTIÈRE, le nom non.
  std::array<char, kComposedSize> who{};
  std::array<char, kComposedSize> why{};
  if (departure.expelled) {
    std::snprintf(who.data(), who.size(), Tr("%s a été expulsé de la guilde."),
                  departure.name.c_str());
    std::snprintf(why.data(), why.size(), Tr("Motif de l'expulsion : %s"),
                  departure.reason.c_str());
  } else {
    std::snprintf(who.data(), who.size(), Tr("%s a quitté la guilde."), departure.name.c_str());
    std::snprintf(why.data(), why.size(), Tr("Motif du départ : %s"), departure.reason.c_str());
  }
  Say(world, who.data(), kNewsColor, now_ms);
  Say(world, sim::BannedWords::MaskWith(banned, why.data()), kNewsColor, now_ms);
}

}  // namespace

bool SayGuildDepartures(sim::GuildBook& guild, sim::World& world, const sim::BannedWords* banned,
                        std::uint32_t now_ms) {
  bool own = false;
  for (const sim::GuildDeparture& departure : guild.TakeDepartures()) {
    SayDeparture(departure, world, banned, now_ms);
    own = own || departure.own;
  }
  return own;
}

}  // namespace ro::net
