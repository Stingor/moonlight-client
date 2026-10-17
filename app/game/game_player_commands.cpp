// LES COMMANDES DU JOUEUR QUI TOUCHENT LE GROUPE, LA GUILDE OU UN ÉCHANGE :
// le routage du chat les lit (`chat/player_command.h`), la partie les exécute
// ici, parce qu'elle seule tient le groupe, la guilde et l'échange.
//
// Chacune suit le client du joueur : ses épreuves avant d'émettre, son paquet,
// et la ligne qu'il écrit ensuite.

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/net/deal.h"
#include "ro/net/guild.h"
#include "ro/net/party.h"
#include "ro/sim/world.h"

#include "chat/banned_input.h"
#include "game/game.h"
#include "party/party_panel.h"
#include "world/click_action.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// UN NOM SE BORNE À VINGT-TROIS OCTETS de la page de code du serveur : le
// vingt-quatrième est la fin de chaîne de sa base.
constexpr std::size_t kNameBytesLimit = 24;

// LE SYMBOLE QU'UN NOM DE GROUPE NE PORTE PAS, refusé par le client du joueur :
// c'est celui des commandes de personnage du serveur.
constexpr char kForbiddenPartyNameSymbol = '#';

// L'ESPACE PLEINE CHASSE, qu'un nom de guilde ne porte ni en tête ni en queue.
constexpr std::string_view kFullWidthSpace = "\xE3\x80\x80";

// LA TEINTE DES REFUS DE NOM DE GUILDE, celle que leur donne le client du
// joueur, en `0xRRGGBB`.
constexpr std::uint32_t kGuildNameRefusalColor = 0xFF966E;

[[nodiscard]] bool TooLong(std::string_view utf8_name) {
  return ro::core::Cp1252FromUtf8(utf8_name).size() >= kNameBytesLimit;
}

[[nodiscard]] const char* TooLongText() {
  return Tr("Ce nom est trop long : vingt-trois caractères au plus.");
}

[[nodiscard]] bool GuildNameHasSpace(std::string_view name) {
  return name.find(' ') != std::string_view::npos || name.starts_with(kFullWidthSpace) ||
         name.ends_with(kFullWidthSpace);
}

// Le joueur visible qui porte ce nom, moi excepté, ou nul.
[[nodiscard]] const ro::sim::Actor* VisiblePlayerNamed(const ro::sim::World& world,
                                                       std::string_view name) {
  for (const auto& [id, actor] : world.actors()) {
    if (id != world.player_id() && actor.kind == ro::sim::ActorKind::kPlayer &&
        actor.name == name) {
      return &actor;
    }
  }
  return nullptr;
}

}  // namespace

void Game::RunPlayerAsks(std::uint32_t now_ms) {
  std::vector<PlayerCommand> asks = std::exchange(player_asks_, {});
  const auto say = [this, now_ms](std::string text, std::uint32_t color = 0) {
    world_.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(text), now_ms, color);
  };
  for (const PlayerCommand& asked : asks) {
    if (!link_.is_open()) {
      return;
    }
    const std::string& name = asked.name;
    switch (asked.kind) {
      case PlayerCommandKind::kDeal: {
        // ⚠ PAS PENDANT L'ÉCRITURE D'UN COURRIER : le serveur l'ignorerait.
        if (pieces_.mail.HoldsDealProposal()) {
          break;
        }
        const ro::sim::Actor* partner = VisiblePlayerNamed(world_, name);
        if (partner == nullptr) {
          say(Tr("Aucun joueur de ce nom n'est en vue :") + std::string(" ") + name);
          break;
        }
        // ⚠ LA DEMANDE PART TELLE QUELLE, à portée ou non : c'est le serveur
        // qui juge la distance, et sa réponse le dit.
        if (auto sent = ro::net::ProposeDeal(
                link_, pieces_.trade.deal().deal(),
                ro::sim::DealPartner{
                    .account_id = partner->id,
                    .name = partner->name,
                    .base_level = static_cast<std::uint16_t>(std::max<int>(partner->level, 0))},
                now_ms);
            !sent) {
          ro::core::LogError("échange : {}", ro::core::Describe(sent.error()));
        }
        break;
      }
      case PlayerCommandKind::kOrganizeParty:
        // Déjà d'un groupe : le client du joueur se tait.
        if (party_.in_party()) {
          break;
        }
        if (name.find(kForbiddenPartyNameSymbol) != std::string::npos) {
          say(Tr("Un nom de groupe ne peut pas porter de symbole spécial."));
          break;
        }
        if (TooLong(name)) {
          say(TooLongText());
          break;
        }
        // 🔴 LA FENÊTRE S'OUVRE, LE NOM POSÉ : c'est elle qui fonde le groupe,
        // comme au client du joueur. Rien ne part avant son bouton.
        windows_.party.window.open = true;
        windows_.party.window.tab = ro::ui::PartyFriendTab::kParty;
        windows_.party.window.new_party_name = {};
        std::copy_n(name.begin(),
                    std::min(name.size(), windows_.party.window.new_party_name.size() - 1),
                    windows_.party.window.new_party_name.begin());
        break;
      case PlayerCommandKind::kInviteToParty:
        if (TooLong(name)) {
          say(TooLongText());
          break;
        }
        ReportRefusal(ro::net::RequestPartyInvite(link_, name));
        if (party_.in_party()) {
          say(name + " " + Tr("a reçu une invitation à rejoindre le groupe."));
        }
        break;
      case PlayerCommandKind::kLeaveParty:
        if (party_.in_party() && LeaveParty(link_, party_, world_.player_id())) {
          windows_.party.ask_disband = true;
        }
        break;
      case PlayerCommandKind::kExpelFromParty:
        // ⚠ LE COMPTE VIENT DE LA LISTE, LE NOM DE LA LIGNE : le serveur exige
        // les deux et refuse sans un mot ce qui ne correspond à aucun membre.
        if (party_.in_party()) {
          const ro::sim::PartyMember* member = party_.FindByName(name);
          ReportRefusal(
              ro::net::RequestPartyExpel(link_, member != nullptr ? member->account_id : 0, name));
        }
        break;
      case PlayerCommandKind::kCreateGuild:
        if (GuildNameHasSpace(name)) {
          say(Tr("Un nom de guilde ne peut pas porter d'espace."), kGuildNameRefusalColor);
          break;
        }
        if (const std::optional<std::string> refused =
                BannedInputRefusal(pieces_.banned_words, name, BannedInput::kGuildName);
            refused.has_value()) {
          say(*refused, BannedInputColor(BannedInput::kGuildName));
          break;
        }
        if (TooLong(name)) {
          say(TooLongText());
          break;
        }
        ReportRefusal(ro::net::RequestGuildCreation(link_, ticket_.character_id, name));
        break;
      case PlayerCommandKind::kInviteToGuild:
        if (!guild_book_.in_guild()) {
          break;
        }
        if (TooLong(name)) {
          say(TooLongText());
          break;
        }
        ReportRefusal(ro::net::RequestGuildInvite(link_, name));
        say(Tr("Invitation à la guilde envoyée à") + std::string(" ") + name);
        break;
      case PlayerCommandKind::kBreakGuild:
        // Le maître seul : pour tout autre, le client du joueur se tait.
        if (!guild_book_.in_guild() || !guild_book_.IsMaster(ticket_.character_id)) {
          break;
        }
        // ⚠ LE NOM DOIT ÊTRE CELUI DE LA GUILDE, à l'octet près : c'est la
        // confirmation que le client du joueur demande, et le serveur la
        // revérifie.
        if (name != guild_book_.belonging()->name) {
          say(Tr("La dissolution de la guilde a échoué : ce n'est pas son nom."));
          break;
        }
        ReportRefusal(ro::net::RequestGuildDisband(link_, name));
        break;
      default:
        break;
    }
  }
}

}  // namespace moonclient
