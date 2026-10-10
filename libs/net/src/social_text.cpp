#include "ro/net/social_text.h"

#include <array>
#include <cstdio>

#include "ro/core/i18n.h"

namespace ro::net {
namespace {

using core::Tr;
using sim::FriendNoticeKind;
using sim::PartyNoticeKind;

// De quoi composer une phrase à trou : un nom de vingt-quatre octets y tient
// largement, même converti.
constexpr std::size_t kComposedSize = 256;

[[nodiscard]] SocialLine Plain(const char* text, SocialTone tone) {
  return SocialLine{.text = text, .tone = tone};
}

[[nodiscard]] std::string ExpText(bool shared) {
  return shared ? Tr("Partage égal") : Tr("Individuel");
}
[[nodiscard]] std::string PickupText(bool shared) {
  return shared ? Tr("Partage de groupe") : Tr("Individuel");
}
[[nodiscard]] std::string LootText(bool shared) {
  return shared ? Tr("Partagé") : Tr("Individuel");
}

[[nodiscard]] SocialLine OptionsLine(const sim::PartyOptions& options) {
  std::array<char, kComposedSize> line{};
  std::snprintf(line.data(), line.size(), Tr("%s - %s : %s | %s : %s | %s : %s"), Tr("Groupe"),
                Tr("Expérience"), ExpText(options.exp_shared).c_str(), Tr("Ramassage"),
                PickupText(options.pickup_shared).c_str(), Tr("Partage"),
                LootText(options.loot_shared).c_str());
  return SocialLine{.text = line.data(), .tone = SocialTone::kParty};
}

}  // namespace

SocialLine PartyNoticeLine(const sim::PartyNotice& notice) {
  std::array<char, kComposedSize> line{};
  const char* name = notice.name.c_str();
  switch (notice.kind) {
    case PartyNoticeKind::kCreated:
      return Plain(Tr("Le groupe a bien été créé."), SocialTone::kSuccess);
    case PartyNoticeKind::kNameTaken:
      return Plain(Tr("Ce nom de groupe existe déjà."), SocialTone::kRefusal);
    case PartyNoticeKind::kAlreadyInParty:
      return Plain(Tr("Ce personnage est déjà dans un groupe."), SocialTone::kRefusal);
    case PartyNoticeKind::kCreationMapRefused:
      return Plain(Tr("Impossible de former un groupe sur cette carte."), SocialTone::kRefusal);
    case PartyNoticeKind::kInviteeInOtherParty:
      return Plain(Tr("Ce personnage a déjà rejoint un autre groupe."), SocialTone::kRefusal);
    case PartyNoticeKind::kInviteRefused:
      std::snprintf(line.data(), line.size(), Tr("%s a refusé votre invitation de groupe."), name);
      return SocialLine{.text = line.data(), .tone = SocialTone::kRefusal};
    case PartyNoticeKind::kInviteAccepted:
      std::snprintf(line.data(), line.size(), Tr("%s a accepté votre invitation de groupe."), name);
      return SocialLine{.text = line.data(), .tone = SocialTone::kSuccess};
    case PartyNoticeKind::kPartyFull:
      return Plain(Tr("Le groupe est complet."), SocialTone::kRefusal);
    case PartyNoticeKind::kSameAccountJoined:
      return Plain(Tr("Un personnage du même compte a déjà rejoint."), SocialTone::kRefusal);
    case PartyNoticeKind::kInvitationsBlocked:
      return Plain(Tr("Ce personnage bloque les invitations de groupe."), SocialTone::kRefusal);
    case PartyNoticeKind::kInviteeNotFound:
      return Plain(Tr("Ce personnage n'est pas connecté ou n'existe pas."), SocialTone::kRefusal);
    case PartyNoticeKind::kInviteeMapRefused:
      std::snprintf(line.data(), line.size(),
                    Tr("(%s) se trouve sur une carte qui empêche de rejoindre un groupe."), name);
      return SocialLine{.text = line.data(), .tone = SocialTone::kRefusal};
    case PartyNoticeKind::kJoinMapRefused:
      return Plain(Tr("Impossible de rejoindre un groupe sur cette carte."), SocialTone::kRefusal);
    case PartyNoticeKind::kMemorialDungeon:
      return Plain(Tr("Impossible d'inviter ou de quitter le groupe dans un Memorial Dungeon."),
                   SocialTone::kRefusal);
    case PartyNoticeKind::kInviteeLevelRefused:
      return Plain(Tr("Un personnage de ce niveau ne peut pas rejoindre le groupe."),
                   SocialTone::kRefusal);
    case PartyNoticeKind::kInviteeLacksBasicSkill:
      return Plain(Tr("Ce personnage n'a pas appris le skill Basic Skill."), SocialTone::kRefusal);
    case PartyNoticeKind::kYouLeft:
      return Plain(Tr("Vous avez quitté le groupe."), SocialTone::kSuccess);
    case PartyNoticeKind::kLeaveRefused:
      return Plain(Tr("Impossible de quitter un groupe sur cette carte."), SocialTone::kRefusal);
    case PartyNoticeKind::kExpelRefused:
      return Plain(Tr("Impossible d'exclure un membre ou de dissoudre le groupe sur cette carte."),
                   SocialTone::kRefusal);
    case PartyNoticeKind::kLeaderChanged:
      std::snprintf(line.data(), line.size(), Tr("Le chef de groupe est [%s]."), name);
      return SocialLine{.text = line.data(), .tone = SocialTone::kParty};
    case PartyNoticeKind::kOptionsChanged:
      return OptionsLine(notice.options);
    case PartyNoticeKind::kExpShareRefused:
      return Plain(Tr("Changement impossible : écart de niveau trop grand."), SocialTone::kRefusal);
    case PartyNoticeKind::kLeaveNeedsHandOver:
      return Plain(Tr("Vous pourrez quitter le groupe une fois la direction déléguée."),
                   SocialTone::kRefusal);
  }
  return Plain(Tr("Résultat inconnu."), SocialTone::kRefusal);
}

SocialLine FriendNoticeLine(const sim::FriendNotice& notice) {
  std::array<char, kComposedSize> line{};
  const char* name = notice.name.c_str();
  switch (notice.kind) {
    case FriendNoticeKind::kBefriended:
      std::snprintf(line.data(), line.size(), Tr("Vous êtes désormais ami avec (%s)."), name);
      return SocialLine{.text = line.data(), .tone = SocialTone::kSuccess};
    case FriendNoticeKind::kDeclined:
      std::snprintf(line.data(), line.size(), Tr("(%s) ne souhaite pas devenir votre ami."), name);
      return SocialLine{.text = line.data(), .tone = SocialTone::kRefusal};
    case FriendNoticeKind::kOwnListFull:
      return Plain(Tr("Votre liste d'amis est pleine."), SocialTone::kRefusal);
    case FriendNoticeKind::kTheirListFull:
      std::snprintf(line.data(), line.size(), Tr("La liste d'amis de (%s) est pleine."), name);
      return SocialLine{.text = line.data(), .tone = SocialTone::kRefusal};
    case FriendNoticeKind::kAlreadyRequested:
      return Plain(Tr("Ce joueur vous a déjà envoyé une demande d'ami."), SocialTone::kRefusal);
    case FriendNoticeKind::kAlreadyFriends:
      return Plain(Tr("Vous êtes déjà amis."), SocialTone::kRefusal);
    case FriendNoticeKind::kNotFound:
      return Plain(Tr("Impossible de trouver l'autre joueur."), SocialTone::kRefusal);
    case FriendNoticeKind::kFriendOfAnotherCharacter:
      return Plain(Tr("Vous êtes déjà ami avec un autre personnage."), SocialTone::kRefusal);
  }
  return Plain(Tr("Résultat inconnu."), SocialTone::kRefusal);
}

}  // namespace ro::net
