#include "chat/player_link_judge.h"

#include <algorithm>
#include <cstddef>

#include "ro/core/i18n.h"
#include "ro/ui/mvp_tracker_window.h"

namespace moonclient {
namespace {

using ro::core::Tr;

[[nodiscard]] bool InGuild(const ro::sim::GuildBook& guild, std::string_view name) {
  return std::ranges::any_of(
      guild.members(), [name](const ro::sim::GuildMember& member) { return member.name == name; });
}

}  // namespace

// LA RÈGLE DU SERVEUR EST « GUILDE OU AMIS », et le client sait juger les deux :
// un grisage exact plutôt qu'une invitation vouée au refus. ⚠ LES NOMS SE
// COMPARENT EXACTEMENT, en UTF-8 des deux côtés : tous sont convertis à
// l'analyse.
const char* JudgeMvpInvite(std::string_view name_utf8, const MvpInviteCircle& circle,
                           bool same_account) {
  if (!circle.link_ready) {
    return ro::ui::MvpLinkPendingText();
  }
  if (!circle.enabled) {
    return ro::ui::MvpTrackerOffText();
  }
  if (circle.book.group().group_id == 0) {
    return Tr(
        "Il faut appartenir à un groupe de chasse pour y inviter quelqu'un — créez-en un "
        "depuis le carnet.");
  }
  if (!circle.friends.HasName(name_utf8) && !InGuild(circle.guild, name_utf8)) {
    return Tr(
        "Seuls vos amis et vos compagnons de guilde peuvent rejoindre votre carnet de "
        "chasse.");
  }
  // Avoir passé la deuxième raison, c'est être du groupe : un personnage de
  // notre compte y est donc, avec tous les autres.
  if (same_account || std::ranges::any_of(circle.book.group().members,
                                          [name_utf8](const ro::sim::MvpMember& member) {
                                            return member.name == name_utf8;
                                          })) {
    return Tr("Déjà dans votre carnet : son compte Moonlight y est, avec tous ses personnages.");
  }
  return nullptr;
}

ro::ui::PlayerLinkRefusals JudgePlayerLink(std::string_view name, const PlayerLinkCircle& circle) {
  ro::ui::PlayerLinkRefusals refused;
  // LE GROUPE : le serveur ne laisse inviter que le chef d'un groupe qui a une
  // place, et refuse qui en a déjà un.
  if (!circle.party.in_party()) {
    refused.party = Tr("Vous n'êtes dans aucun groupe.");
  } else if (circle.party.FindByName(name) != nullptr) {
    refused.party = Tr("Déjà membre de votre groupe.");
  } else if (!circle.party.AmILeader(circle.own_account_id)) {
    refused.party = Tr("Seul le chef du groupe peut inviter.");
  } else if (circle.party.members().size() >= ro::sim::kMaxPartyMembers) {
    refused.party = Tr("Votre groupe est complet.");
  }
  // LA GUILDE : il faut en avoir une, et que sa position permette d'inviter —
  // le serveur refuse sinon sans rien dire.
  if (!circle.guild.in_guild()) {
    refused.guild = Tr("Vous n'êtes dans aucune guilde.");
  } else if (InGuild(circle.guild, name)) {
    refused.guild = Tr("Déjà membre de votre guilde.");
  } else if (const ro::sim::GuildRights rights = circle.guild.RightsOf(circle.own_char_id);
             rights.known && !rights.may_invite) {
    refused.guild = Tr("Votre position dans la guilde ne permet pas d'inviter.");
  } else if (const auto& summary = circle.guild.summary();
             summary.has_value() && circle.guild.members_known() && summary->member_most > 0 &&
             circle.guild.members().size() >= static_cast<std::size_t>(summary->member_most)) {
    refused.guild = Tr("Votre guilde est complète.");
  }
  // LES AMIS : pas deux fois le même, pas au-delà de la liste.
  if (circle.friends.HasName(name)) {
    refused.befriend = Tr("Déjà dans votre liste d'amis.");
  } else if (circle.friends.full()) {
    refused.befriend = Tr("Votre liste d'amis est pleine.");
  }
  // LE CARNET DE CHASSE : ses raisons, quand on nous le confie. Éteint, l'entrée
  // disparaît.
  refused.mvp = circle.mvp != nullptr ? JudgeMvpInvite(name, *circle.mvp) : nullptr;
  refused.mvp_withdrawn = circle.mvp != nullptr && !circle.mvp->enabled;
  return refused;
}

}  // namespace moonclient
