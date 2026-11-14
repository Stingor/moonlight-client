// LE MENU DU CLIC DROIT SUR UNE ENTITÉ : sur quoi il s'ouvre, et ce qu'il porte.
//
// 🔴 AUCUNE DE CES RÈGLES NE SE VOIT EN JOUANT QUAND ELLE CASSE. Un menu qui
// s'ouvre sur un monstre sans qu'on l'ait demandé avale un clic droit ; une
// ligne grisée sans raison ne dit plus pourquoi ; une ligne de staff glissée au
// milieu du menu tombe dans le sous-menu d'à côté. Le banc les tient, une à une.

#include "world/entity_menu.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/ui/entity_menu.h"

using moonclient::BuildEntityMenu;
using moonclient::ClassifyEntity;
using moonclient::EntityFamily;
using moonclient::EntityMenuFacts;
using moonclient::EntityMenuSettings;
using moonclient::EntityVerb;
using moonclient::FrameClickOpensMenu;
using moonclient::OwnCompanionIds;
using moonclient::WorldClickOpensMenu;
using moonclient::WorldMenuGesture;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::ui::EntityMenuItem;
using ro::ui::EntityMenuQuestion;
using ro::ui::EntityMenuView;

namespace {

constexpr std::uint32_t kSelf = 2000001;
constexpr std::uint32_t kPlayer = 2000002;
constexpr std::uint32_t kPoring = 110001;
constexpr std::uint32_t kKafra = 3000001;
constexpr std::uint32_t kDynamicNpc = 110500;
constexpr std::uint32_t kOwnGuild = 7;
constexpr std::uint32_t kOtherGuild = 9;
constexpr std::uint32_t kRedPotion = 501;

[[nodiscard]] Actor Spawned(std::uint32_t id, ActorKind kind) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  return actor;
}

[[nodiscard]] std::vector<EntityVerb> Verbs(const EntityMenuView& view) {
  std::vector<EntityVerb> verbs;
  for (const EntityMenuItem& item : view.items) {
    verbs.push_back(static_cast<EntityVerb>(item.verb));
  }
  return verbs;
}

[[nodiscard]] const EntityMenuItem* Find(const EntityMenuView& view, EntityVerb verb) {
  const auto found = std::ranges::find_if(view.items, [verb](const EntityMenuItem& item) {
    return item.verb == static_cast<int>(verb);
  });
  return found != view.items.end() ? &*found : nullptr;
}

[[nodiscard]] EntityMenuFacts Facts(EntityFamily family, std::uint32_t id) {
  EntityMenuFacts facts;
  facts.family = family;
  facts.id = id;
  facts.name = "Quelqu'un";
  return facts;
}

}  // namespace

TEST_CASE("Menu d'entité : le genre du serveur tranche, pas l'apparence", "[app][entity_menu]") {
  CHECK(ClassifyEntity(Spawned(kPlayer, ActorKind::kPlayer), kSelf) == EntityFamily::kPlayer);
  CHECK(ClassifyEntity(Spawned(kSelf, ActorKind::kPlayer), kSelf) == EntityFamily::kSelf);
  CHECK(ClassifyEntity(Spawned(kPoring, ActorKind::kMonster), kSelf) == EntityFamily::kMonster);
  CHECK(ClassifyEntity(Spawned(kDynamicNpc, ActorKind::kNonPlayer), kSelf) == EntityFamily::kNpc);
  CHECK(ClassifyEntity(Spawned(kRedPotion, ActorKind::kItem), kSelf) == EntityFamily::kGroundItem);
  CHECK(ClassifyEntity(Spawned(kPoring, ActorKind::kUnknown), kSelf) == EntityFamily::kOther);
  CHECK(ClassifyEntity(Spawned(0, ActorKind::kPlayer), kSelf) == EntityFamily::kNone);
}

TEST_CASE("Menu d'entité : un GID épinglé est un PNJ, même déguisé en monstre",
          "[app][entity_menu]") {
  // ⚠ SANS CELA, un PNJ déguisé se verrait proposer « Attaquer », et perdrait
  // sa case de blocage.
  CHECK(ClassifyEntity(Spawned(kKafra, ActorKind::kMonster), kSelf) == EntityFamily::kNpc);
}

TEST_CASE("Menu d'entité : un familier n'a pas le menu d'un PNJ", "[app][entity_menu]") {
  Actor pet = Spawned(kDynamicNpc, ActorKind::kNonPlayer);
  pet.familiar = ro::sim::Familiar::kPet;
  CHECK(ClassifyEntity(pet, kSelf) == EntityFamily::kOther);
  pet.familiar = ro::sim::Familiar::kHomunculus;
  CHECK(ClassifyEntity(pet, kSelf) == EntityFamily::kOther);
}

TEST_CASE("Menu d'entité : d'usine, le monde ne l'ouvre que sur un joueur", "[app][entity_menu]") {
  const EntityMenuSettings settings;
  const WorldMenuGesture plain;
  CHECK(WorldClickOpensMenu(EntityFamily::kPlayer, kPlayer, plain, false, settings, false));
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kMonster, kPoring, plain, false, settings, false));
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kNpc, kDynamicNpc, plain, false, settings, false));
  CHECK_FALSE(
      WorldClickOpensMenu(EntityFamily::kGroundItem, kRedPotion, plain, false, settings, false));
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kSelf, kSelf, plain, false, settings, false));
}

TEST_CASE("Menu d'entité : un PNJ épinglé s'ouvre sans « toutes les entités »",
          "[app][entity_menu]") {
  // 🔴 C'EST LE SEUL ENDROIT D'OÙ L'ON DÉCOCHE SON BLOCAGE.
  EntityMenuSettings settings;
  const WorldMenuGesture plain;
  CHECK(WorldClickOpensMenu(EntityFamily::kNpc, kKafra, plain, false, settings, false));
  settings.npc_block = false;
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kNpc, kKafra, plain, false, settings, false));
}

TEST_CASE("Menu d'entité : « toutes les entités » ouvre le reste, soi-même compris",
          "[app][entity_menu]") {
  EntityMenuSettings settings;
  settings.all_entities = true;
  const WorldMenuGesture plain;
  CHECK(WorldClickOpensMenu(EntityFamily::kMonster, kPoring, plain, false, settings, false));
  CHECK(WorldClickOpensMenu(EntityFamily::kGroundItem, kRedPotion, plain, false, settings, false));
  CHECK(WorldClickOpensMenu(EntityFamily::kSelf, kSelf, plain, false, settings, false));
  settings.self_menu = false;
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kSelf, kSelf, plain, false, settings, false));
  // Ce qui n'a aucune action de jeu reste au staff.
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kOther, kPoring, plain, false, settings, false));
  CHECK(WorldClickOpensMenu(EntityFamily::kOther, kPoring, plain, false, settings, true));
}

TEST_CASE("Menu d'entité : le staff outillé ouvre sur tout, sans autre réglage",
          "[app][entity_menu]") {
  EntityMenuSettings settings;
  const WorldMenuGesture plain;
  CHECK(WorldClickOpensMenu(EntityFamily::kMonster, kPoring, plain, false, settings, true));
  settings.staff_extras = false;
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kMonster, kPoring, plain, false, settings, true));
}

TEST_CASE("Menu d'entité : les gardes du client d'origine tiennent", "[app][entity_menu]") {
  EntityMenuSettings settings;
  settings.all_entities = true;
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kPlayer, kPlayer, {.left_held = true}, false,
                                  settings, false));
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kPlayer, kPlayer, {.shift_held = true}, false,
                                  settings, false));
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kPlayer, kPlayer, {.over_interface = true}, false,
                                  settings, false));
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kPlayer, kPlayer, {}, true, settings, false));
  // Alt + clic droit sur un monstre est un ordre au familier ; sur un joueur,
  // rien ne le réclame.
  CHECK_FALSE(WorldClickOpensMenu(EntityFamily::kMonster, kPoring, {.alt_held = true}, false,
                                  settings, false));
  CHECK(WorldClickOpensMenu(EntityFamily::kPlayer, kPlayer, {.alt_held = true}, false, settings,
                            false));
}

TEST_CASE("Menu d'entité : le cadre du HUD s'ouvre sans les gardes du monde",
          "[app][entity_menu]") {
  EntityMenuSettings settings;
  CHECK(FrameClickOpensMenu(EntityFamily::kMonster, settings));
  CHECK(FrameClickOpensMenu(EntityFamily::kSelf, settings));
  settings.self_menu = false;
  CHECK_FALSE(FrameClickOpensMenu(EntityFamily::kSelf, settings));
  CHECK_FALSE(FrameClickOpensMenu(EntityFamily::kNone, settings));
}

TEST_CASE("Menu d'entité : un joueur sans guilde, hors Ciblage", "[app][entity_menu]") {
  const EntityMenuView view = BuildEntityMenu(Facts(EntityFamily::kPlayer, kPlayer), {});
  CHECK(view.subject == kPlayer);
  CHECK(view.name == "Quelqu'un");
  // ⚠ LE CARNET DE CHASSE ENTRE LES DEUX CHUCHOTEMENTS, toujours présent.
  CHECK(Verbs(view) == std::vector<EntityVerb>{EntityVerb::kViewEquip, EntityVerb::kWhisper,
                                               EntityVerb::kMvpInvite, EntityVerb::kWhisperWindow,
                                               EntityVerb::kDealPropose, EntityVerb::kFollow,
                                               EntityVerb::kFriendAdd, EntityVerb::kMailSend,
                                               EntityVerb::kWhisperBlock, EntityVerb::kReportPlayer,
                                               EntityVerb::kCopyName});
  // Trois blocs, deux traits : l'équipement ouvre le menu, les deux
  // chuchotements — l'invitation au carnet, l'échange, le suivi, l'amitié, le
  // courrier, le blocage et le signalement avec eux — puis la copie ouvrent
  // chacun le leur.
  CHECK_FALSE(view.items[0].separator);
  CHECK(view.items[1].separator);
  for (std::size_t rank = 2; rank < view.items.size() - 1; ++rank) {
    CHECK_FALSE(view.items[rank].separator);
  }
  CHECK(view.items.back().separator);
}

TEST_CASE("Menu d'entité : le blocage des chuchotements suit la liste du serveur",
          "[app][entity_menu][whisper_block]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  const EntityMenuView open = BuildEntityMenu(facts, {});
  const EntityMenuItem* block = Find(open, EntityVerb::kWhisperBlock);
  REQUIRE(block != nullptr);
  CHECK(block->label == "Bloquer les chuchotements");
  CHECK_FALSE(block->disabled);
  CHECK(block->tip.find("chuchotements") != std::string::npos);
  // UNE SEULE DES DEUX : l'autre ne ferait rien.
  CHECK(Find(open, EntityVerb::kWhisperAllow) == nullptr);

  facts.whisper_blocked = true;
  const EntityMenuView blocked = BuildEntityMenu(facts, {});
  const EntityMenuItem* allow = Find(blocked, EntityVerb::kWhisperAllow);
  REQUIRE(allow != nullptr);
  CHECK(allow->label == "Autoriser les chuchotements");
  CHECK(Find(blocked, EntityVerb::kWhisperBlock) == nullptr);

  // SUR SOI : ni l'une ni l'autre.
  const EntityMenuView self = BuildEntityMenu(Facts(EntityFamily::kSelf, kSelf), {});
  CHECK(Find(self, EntityVerb::kWhisperBlock) == nullptr);
  CHECK(Find(self, EntityVerb::kWhisperAllow) == nullptr);
}

TEST_CASE("Menu d'entité : l'invitation au carnet MVP se grise avec sa raison",
          "[app][entity_menu][mvp]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  // OFFERTE : son infobulle dit la règle.
  const EntityMenuView offered = BuildEntityMenu(facts, {});
  const EntityMenuItem* open = Find(offered, EntityVerb::kMvpInvite);
  REQUIRE(open != nullptr);
  CHECK(open->label == "Inviter dans mon carnet MVP");
  CHECK_FALSE(open->disabled);
  CHECK(open->tip.starts_with("Guilde et liste d'amis uniquement."));

  // REFUSÉE : grisée, et la raison remplace la règle.
  facts.mvp_invite_refusal = "Créez un groupe de chasse.";
  const EntityMenuView refused = BuildEntityMenu(facts, {});
  const EntityMenuItem* shut = Find(refused, EntityVerb::kMvpInvite);
  REQUIRE(shut != nullptr);
  CHECK(shut->disabled);
  CHECK(shut->tip == facts.mvp_invite_refusal);

  // CARNET ÉTEINT : absente, et « Chuchoter » garde sa place.
  facts.mvp_invite_withdrawn = true;
  const EntityMenuView withdrawn = BuildEntityMenu(facts, {});
  CHECK(Find(withdrawn, EntityVerb::kMvpInvite) == nullptr);
  CHECK(Find(withdrawn, EntityVerb::kWhisper) != nullptr);
  CHECK(Find(withdrawn, EntityVerb::kWhisperWindow) != nullptr);

  // SUR SOI : absente.
  CHECK(Find(BuildEntityMenu(Facts(EntityFamily::kSelf, kSelf), {}), EntityVerb::kMvpInvite) ==
        nullptr);
}

TEST_CASE("Menu d'entité : un échange en cours grise la proposition", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  CHECK_FALSE(Find(BuildEntityMenu(facts, {}), EntityVerb::kDealPropose)->disabled);
  facts.dealing = true;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  const EntityMenuItem* deal = Find(view, EntityVerb::kDealPropose);
  REQUIRE(deal != nullptr);
  CHECK(deal->disabled);
  CHECK_FALSE(deal->tip.empty());
}

TEST_CASE("Menu d'entité : hors groupe, aucune ligne de groupe", "[app][entity_menu]") {
  const EntityMenuView view = BuildEntityMenu(Facts(EntityFamily::kPlayer, kPlayer), {});
  CHECK(Find(view, EntityVerb::kPartyInvite) == nullptr);
  CHECK(Find(view, EntityVerb::kPartyLeader) == nullptr);
  CHECK(Find(view, EntityVerb::kPartyExpel) == nullptr);
}

TEST_CASE("Menu d'entité : l'invitation de groupe se grise avec sa raison", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  facts.in_party = true;
  facts.party_leader = true;
  const EntityMenuView free_view = BuildEntityMenu(facts, {});
  const EntityMenuItem* open = Find(free_view, EntityVerb::kPartyInvite);
  REQUIRE(open != nullptr);
  CHECK_FALSE(open->disabled);

  facts.target_in_party = true;
  const EntityMenuView member = BuildEntityMenu(facts, {});
  const EntityMenuItem* already = Find(member, EntityVerb::kPartyInvite);
  REQUIRE(already != nullptr);
  CHECK(already->disabled);
  CHECK(already->tip == "Déjà membre de votre groupe.");

  facts.target_in_party = false;
  facts.target_other_party = true;
  const EntityMenuView other_view = BuildEntityMenu(facts, {});
  const EntityMenuItem* elsewhere = Find(other_view, EntityVerb::kPartyInvite);
  REQUIRE(elsewhere != nullptr);
  CHECK(elsewhere->disabled);
  CHECK(elsewhere->tip.starts_with("Ce joueur appartient déjà à un groupe"));

  facts.target_other_party = false;
  facts.party_leader = false;
  const EntityMenuView follower_view = BuildEntityMenu(facts, {});
  const EntityMenuItem* follower = Find(follower_view, EntityVerb::kPartyInvite);
  REQUIRE(follower != nullptr);
  CHECK(follower->disabled);
  CHECK(follower->tip == "Seul le chef du groupe peut inviter.");
}

TEST_CASE("Menu d'entité : le chef nomme et expulse un membre, après confirmation",
          "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  facts.in_party = true;
  facts.target_in_party = true;
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kPartyLeader) == nullptr);
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kPartyExpel) == nullptr);

  facts.party_leader = true;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  const EntityMenuItem* leader = Find(view, EntityVerb::kPartyLeader);
  const EntityMenuItem* expel = Find(view, EntityVerb::kPartyExpel);
  REQUIRE(leader != nullptr);
  REQUIRE(expel != nullptr);
  CHECK(leader->question == EntityMenuQuestion::kPartyLeader);
  CHECK(expel->question == EntityMenuQuestion::kPartyExpel);
}

TEST_CASE("Menu d'entité : ajouter ou retirer un ami, selon qu'il l'est", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  const EntityMenuView stranger = BuildEntityMenu(facts, {});
  CHECK(Find(stranger, EntityVerb::kFriendRemove) == nullptr);
  REQUIRE(Find(stranger, EntityVerb::kFriendAdd) != nullptr);
  CHECK_FALSE(Find(stranger, EntityVerb::kFriendAdd)->disabled);

  facts.friends_full = true;
  const EntityMenuView full_view = BuildEntityMenu(facts, {});
  const EntityMenuItem* full = Find(full_view, EntityVerb::kFriendAdd);
  REQUIRE(full != nullptr);
  CHECK(full->disabled);
  CHECK(full->tip == "Votre liste d'amis est pleine.");

  facts.target_is_friend = true;
  const EntityMenuView chum = BuildEntityMenu(facts, {});
  CHECK(Find(chum, EntityVerb::kFriendAdd) == nullptr);
  REQUIRE(Find(chum, EntityVerb::kFriendRemove) != nullptr);
  CHECK(Find(chum, EntityVerb::kFriendRemove)->question == EntityMenuQuestion::kFriendRemove);
}

TEST_CASE("Menu d'entité : « Cibler » n'existe que sous le mode Ciblage", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kMonster, kPoring);
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kTarget) == nullptr);
  facts.targeting = true;
  CHECK(Verbs(BuildEntityMenu(facts, {})) ==
        std::vector<EntityVerb>{EntityVerb::kAttack, EntityVerb::kTarget, EntityVerb::kMonsterSheet,
                                EntityVerb::kMonsterLink, EntityVerb::kCopyName});
}

TEST_CASE("Menu d'entité : ce que le serveur refuserait se grise, avec sa raison",
          "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  facts.own_guild_id = kOwnGuild;
  facts.guild_master = true;
  facts.target_guild_id = kOwnGuild;
  const EntityMenuView same = BuildEntityMenu(facts, {});
  const EntityMenuItem* invite = Find(same, EntityVerb::kGuildInvite);
  REQUIRE(invite != nullptr);
  CHECK(invite->disabled);
  CHECK_FALSE(invite->tip.empty());
  const EntityMenuItem* ally = Find(same, EntityVerb::kGuildAlliance);
  REQUIRE(ally != nullptr);
  CHECK(ally->disabled);
  CHECK(ally->separator);
  CHECK(Find(same, EntityVerb::kGuildOpposition)->disabled);

  facts.target_guild_id = 0;
  const EntityMenuView guildless = BuildEntityMenu(facts, {});
  CHECK_FALSE(Find(guildless, EntityVerb::kGuildInvite)->disabled);
  CHECK(Find(guildless, EntityVerb::kGuildAlliance)->disabled);

  facts.target_guild_id = kOtherGuild;
  const EntityMenuView other = BuildEntityMenu(facts, {});
  CHECK(Find(other, EntityVerb::kGuildInvite)->disabled);
  CHECK_FALSE(Find(other, EntityVerb::kGuildAlliance)->disabled);
  CHECK_FALSE(Find(other, EntityVerb::kGuildOpposition)->disabled);

  // Un rang sans droit d'inviter : c'est le premier refus, avant la guilde de
  // la cible. Droits inconnus ou accordés, rien ne grise (`guildless` plus haut).
  facts.guild_invite_forbidden = true;
  const EntityMenuView forbidden = BuildEntityMenu(facts, {});
  CHECK(Find(forbidden, EntityVerb::kGuildInvite)->disabled);
  CHECK(Find(forbidden, EntityVerb::kGuildInvite)->tip ==
        "Votre rang ne peut pas inviter dans la guilde.");
}

TEST_CASE("Menu d'entité : alliance et hostilité ne s'offrent qu'au maître", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  facts.own_guild_id = kOwnGuild;
  const EntityMenuView member = BuildEntityMenu(facts, {});
  CHECK(Find(member, EntityVerb::kGuildInvite) != nullptr);
  CHECK(Find(member, EntityVerb::kGuildAlliance) == nullptr);
  CHECK(Find(member, EntityVerb::kGuildOpposition) == nullptr);
}

TEST_CASE("Menu d'entité : le trait d'un monstre tombe sur la tête de son bloc",
          "[app][entity_menu]") {
  // La fiche du monstre ouvre le bloc d'information : le
  // trait est à elle, et « Copier le nom » la suit sans le sien.
  const EntityMenuView view = BuildEntityMenu(Facts(EntityFamily::kMonster, kPoring), {});
  const EntityMenuItem* sheet = Find(view, EntityVerb::kMonsterSheet);
  REQUIRE(sheet != nullptr);
  CHECK(sheet->separator);
  const EntityMenuItem* copy = Find(view, EntityVerb::kCopyName);
  REQUIRE(copy != nullptr);
  CHECK_FALSE(copy->separator);
}

TEST_CASE("Menu d'entité : un portail ne propose pas d'interagir", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kNpc, kDynamicNpc);
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kTalkToNpc) != nullptr);
  facts.job = ro::sim::kPortalJob;
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kTalkToNpc) == nullptr);
}

TEST_CASE("Menu d'entité : « Linker ce NPC » sur un PNJ nommé, jamais sur un portail",
          "[app][entity_menu][navi]") {
  EntityMenuFacts facts = Facts(EntityFamily::kNpc, kDynamicNpc);
  const EntityMenuView view = BuildEntityMenu(facts, {});
  CHECK(Verbs(view) == std::vector<EntityVerb>{EntityVerb::kTalkToNpc, EntityVerb::kNpcLink,
                                               EntityVerb::kCopyName});
  const EntityMenuItem* link = Find(view, EntityVerb::kNpcLink);
  REQUIRE(link != nullptr);
  CHECK(link->label == "Linker ce NPC");
  // UN ANONYME n'a rien à chercher.
  facts.name.clear();
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kNpcLink) == nullptr);
  // NI UN NOM QUE LA BALISE NE PEUT PORTER : tout caché, ou à chevron.
  facts.name = "#caché";
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kNpcLink) == nullptr);
  facts.name = "<Gardien>";
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kNpcLink) == nullptr);
  // La partie cachée, elle, ne retire pas l'entrée : le lien publie le reste.
  facts.name = "Kafra Employee#prt_kafra1";
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kNpcLink) != nullptr);
  // UN PORTAIL non plus : la recherche de son nom ne mène nulle part.
  facts.name = "Warp";
  facts.job = ro::sim::kPortalJob;
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kNpcLink) == nullptr);
  // ⚠ NI SUR UN MONSTRE, qui a son propre lien.
  CHECK(Find(BuildEntityMenu(Facts(EntityFamily::kMonster, kPoring), {}), EntityVerb::kNpcLink) ==
        nullptr);
}

TEST_CASE("Menu d'entité : la case de blocage, sur un PNJ épinglé seulement",
          "[app][entity_menu]") {
  EntityMenuSettings settings;
  CHECK(Find(BuildEntityMenu(Facts(EntityFamily::kNpc, kDynamicNpc), settings),
             EntityVerb::kNpcClickBlock) == nullptr);

  EntityMenuFacts facts = Facts(EntityFamily::kNpc, kKafra);
  facts.npc_blocked = true;
  const EntityMenuView view = BuildEntityMenu(facts, settings);
  const EntityMenuItem* block = Find(view, EntityVerb::kNpcClickBlock);
  REQUIRE(block != nullptr);
  CHECK(block->toggle);
  CHECK(block->checked);
  // ⚠ « INTERAGIR » RESTE sur un PNJ bloqué : on bloque le clic accidentel,
  // pas la volonté de parler.
  CHECK(Find(view, EntityVerb::kTalkToNpc) != nullptr);

  settings.npc_block = false;
  CHECK(Find(BuildEntityMenu(facts, settings), EntityVerb::kNpcClickBlock) == nullptr);
}

TEST_CASE("Menu d'entité : un objet non identifié ne trahit pas son nom", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kGroundItem, kRedPotion);
  facts.item_id = kRedPotion;
  facts.item_identified = false;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  CHECK(Verbs(view) == std::vector<EntityVerb>{EntityVerb::kPickup, EntityVerb::kItemDescription,
                                               EntityVerb::kItemLink, EntityVerb::kItemInfo,
                                               EntityVerb::kWhoDrops});
  CHECK_FALSE(view.items[0].disabled);
  for (std::size_t rank = 1; rank < view.items.size(); ++rank) {
    CHECK(view.items[rank].disabled);
    CHECK_FALSE(view.items[rank].tip.empty());
  }

  facts.item_identified = true;
  const EntityMenuView known = BuildEntityMenu(facts, {});
  CHECK(
      std::ranges::none_of(known.items, [](const EntityMenuItem& item) { return item.disabled; }));
}

TEST_CASE("Menu d'entité : « masquer la fenêtre » seulement quand elle montre l'entité",
          "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kMonster, kPoring);
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kHideTargetFrame) == nullptr);
  facts.target_frame_shown = true;
  const EntityMenuItem* hide = Find(BuildEntityMenu(facts, {}), EntityVerb::kHideTargetFrame);
  REQUIRE(hide != nullptr);
  CHECK(hide->separator);
}

TEST_CASE("Menu d'entité : le staff d'un seul tenant, en fin de liste", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  facts.staff = true;
  facts.target_frame_shown = true;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  const auto first_staff =
      std::ranges::find_if(view.items, [](const EntityMenuItem& item) { return item.submenu; });
  REQUIRE(first_staff != view.items.end());
  // 🔴 UNE LIGNE DE JOUEUR GLISSÉE AU MILIEU TOMBERAIT DANS LE SOUS-MENU.
  CHECK(std::all_of(first_staff, view.items.end(),
                    [](const EntityMenuItem& item) { return item.submenu && item.staff; }));
  CHECK(Find(view, EntityVerb::kHideTargetFrame)->submenu == false);
  // Le premier du sous-menu n'ouvre pas de bloc : le sous-menu lui-même en est un.
  CHECK_FALSE(first_staff->separator);
  CHECK(static_cast<EntityVerb>(first_staff->verb) == EntityVerb::kInspect);

  CHECK(Find(view, EntityVerb::kEventPoints)->question == EntityMenuQuestion::kEventPoints);
  const EntityMenuItem* block = Find(view, EntityVerb::kBlockAccount);
  REQUIRE(block != nullptr);
  CHECK(block->danger);
  CHECK(block->question == EntityMenuQuestion::kAccountBlock);
  CHECK(Find(view, EntityVerb::kKick)->danger);
  CHECK_FALSE(Find(view, EntityVerb::kMute)->danger);
  // L'outillage des PNJ ne concerne pas un joueur.
  CHECK(Find(view, EntityVerb::kNpcUnload) == nullptr);
}

TEST_CASE("Menu d'entité : le staff se replie avec son réglage", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  facts.staff = true;
  EntityMenuSettings settings;
  settings.staff_extras = false;
  const EntityMenuView view = BuildEntityMenu(facts, settings);
  CHECK(std::ranges::none_of(view.items, [](const EntityMenuItem& item) { return item.submenu; }));
}

TEST_CASE("Menu d'entité : l'outillage des PNJ attend le niveau 99", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kNpc, kDynamicNpc);
  facts.staff = true;
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kNpcReloadFile) == nullptr);
  facts.admin = true;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  CHECK(Find(view, EntityVerb::kNpcReloadFile) != nullptr);
  CHECK(Find(view, EntityVerb::kNpcMoveHere) != nullptr);
  const EntityMenuItem* unload = Find(view, EntityVerb::kNpcUnload);
  REQUIRE(unload != nullptr);
  CHECK(unload->danger);
  CHECK(unload->question == EntityMenuQuestion::kNpcUnload);
}

TEST_CASE("Menu d'entité : l'identifiant se nomme selon ce qu'on vise", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kSelf, kSelf);
  facts.staff = true;
  const std::string self = Find(BuildEntityMenu(facts, {}), EntityVerb::kCopyId)->label;
  facts.family = EntityFamily::kPlayer;
  const std::string player = Find(BuildEntityMenu(facts, {}), EntityVerb::kCopyId)->label;
  facts.family = EntityFamily::kMonster;
  const std::string monster = Find(BuildEntityMenu(facts, {}), EntityVerb::kCopyId)->label;
  CHECK(self != player);
  CHECK(player != monster);
}

TEST_CASE("Menu d'entité : nos compagnons se reconnaissent à leur identifiant ET à leur genre",
          "[app][entity_menu]") {
  constexpr std::uint32_t kOwnPet = 110700;
  constexpr std::uint32_t kOwnHomunculus = 110701;
  constexpr std::uint32_t kOwnMercenary = 110702;
  const OwnCompanionIds ours{
      .pet_id = kOwnPet, .homunculus_id = kOwnHomunculus, .mercenary_id = kOwnMercenary};
  Actor pet = Spawned(kOwnPet, ActorKind::kNonPlayer);
  pet.familiar = ro::sim::Familiar::kPet;
  CHECK(ClassifyEntity(pet, kSelf, ours) == EntityFamily::kOwnPet);
  Actor homunculus = Spawned(kOwnHomunculus, ActorKind::kNonPlayer);
  homunculus.familiar = ro::sim::Familiar::kHomunculus;
  CHECK(ClassifyEntity(homunculus, kSelf, ours) == EntityFamily::kOwnHomunculus);
  Actor mercenary = Spawned(kOwnMercenary, ActorKind::kNonPlayer);
  mercenary.familiar = ro::sim::Familiar::kMercenary;
  CHECK(ClassifyEntity(mercenary, kSelf, ours) == EntityFamily::kOwnMercenary);
  // Le familier d'autrui reste un inconnu.
  pet.id = kDynamicNpc;
  CHECK(ClassifyEntity(pet, kSelf, ours) == EntityFamily::kOther);
  // Un identifiant retenu ne vaut que sur le genre qu'il désigne.
  homunculus.id = kOwnPet;
  CHECK(ClassifyEntity(homunculus, kSelf, ours) == EntityFamily::kOther);
}

TEST_CASE("Menu d'entité : nos compagnons s'ouvrent d'usine, comme dans le jeu",
          "[app][entity_menu]") {
  const EntityMenuSettings settings;
  const WorldMenuGesture plain;
  CHECK(WorldClickOpensMenu(EntityFamily::kOwnPet, 1, plain, false, settings, false));
  CHECK(WorldClickOpensMenu(EntityFamily::kOwnHomunculus, 1, plain, false, settings, false));
  CHECK(WorldClickOpensMenu(EntityFamily::kOwnMercenary, 1, plain, false, settings, false));
}

TEST_CASE("Menu d'entité : le menu de notre pet, dans l'ordre du jeu", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kOwnPet, 1);
  const EntityMenuView view = BuildEntityMenu(facts, {});
  CHECK(Verbs(view) == std::vector<EntityVerb>{EntityVerb::kPetStatus, EntityVerb::kPetFeed,
                                               EntityVerb::kPetPerform, EntityVerb::kPetUnequip,
                                               EntityVerb::kPetToEgg});
  // Nourrir se confirme ; l'œuf est en rouge ; sans accessoire, rien à retirer.
  CHECK(Find(view, EntityVerb::kPetFeed)->question == EntityMenuQuestion::kPetFeed);
  CHECK(Find(view, EntityVerb::kPetToEgg)->danger);
  const EntityMenuItem* unequip = Find(view, EntityVerb::kPetUnequip);
  CHECK(unequip->disabled);
  CHECK_FALSE(unequip->tip.empty());
  facts.pet_has_accessory = true;
  CHECK_FALSE(Find(BuildEntityMenu(facts, {}), EntityVerb::kPetUnequip)->disabled);
  // Un courrier en rédaction refuse le repas AVANT la question.
  CHECK_FALSE(Find(view, EntityVerb::kPetFeed)->disabled);
  facts.pet_feed_refusal = "Fermez le courrier.";
  const EntityMenuView refused = BuildEntityMenu(facts, {});
  const EntityMenuItem* feed = Find(refused, EntityVerb::kPetFeed);
  CHECK(feed->disabled);
  CHECK(feed->tip == "Fermez le courrier.");
}

TEST_CASE("Menu d'entité : l'homoncule au repos ne se nourrit pas", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kOwnHomunculus, 1);
  CHECK(Verbs(BuildEntityMenu(facts, {})) ==
        std::vector<EntityVerb>{EntityVerb::kHomunculusStatus, EntityVerb::kHomunculusFeed,
                                EntityVerb::kHomunculusStandby});
  const EntityMenuView offered = BuildEntityMenu(facts, {});
  const EntityMenuItem* feed = Find(offered, EntityVerb::kHomunculusFeed);
  CHECK_FALSE(feed->disabled);
  // Rassasié, il perd de l'intimité à manger : le jeu demande d'abord.
  CHECK(feed->question == EntityMenuQuestion::kHomunculusFeed);
  facts.homunculus_resting = true;
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kHomunculusFeed)->disabled);
}

TEST_CASE("Menu d'entité : notre mercenaire a son statut et son attente", "[app][entity_menu]") {
  CHECK(Verbs(BuildEntityMenu(Facts(EntityFamily::kOwnMercenary, 1), {})) ==
        std::vector<EntityVerb>{EntityVerb::kMercenaryStatus, EntityVerb::kMercenaryStandby});
}

TEST_CASE("Menu d'entité : « En attente » est une case qui suit l'IA du compagnon",
          "[app][entity_menu]") {
  for (const auto& [family, verb] :
       {std::pair{EntityFamily::kOwnHomunculus, EntityVerb::kHomunculusStandby},
        std::pair{EntityFamily::kOwnMercenary, EntityVerb::kMercenaryStandby}}) {
    EntityMenuFacts facts = Facts(family, 1);
    const EntityMenuView idle = BuildEntityMenu(facts, {});
    const EntityMenuItem* standby = Find(idle, verb);
    REQUIRE(standby != nullptr);
    CHECK(standby->toggle);
    CHECK_FALSE(standby->checked);
    facts.companion_standby = true;
    CHECK(Find(BuildEntityMenu(facts, {}), verb)->checked);
  }
}

TEST_CASE("Menu d'entité : la tactique du compagnon, un sous-menu par compagnon",
          "[app][entity_menu]") {
  using moonclient::TacticMenuChoice;
  using moonclient::TacticVerb;
  using ro::sim::AiCompanion;
  const auto verb_of = [](AiCompanion companion, int choice) {
    return static_cast<EntityVerb>(TacticVerb(companion, choice));
  };
  const auto submenu_lines = [](const EntityMenuView& view, const std::string& label) {
    std::size_t count = 0;
    for (const EntityMenuItem& item : view.items) {
      count += item.submenu && item.submenu_label == label ? 1U : 0U;
    }
    return count;
  };
  // Sept rangs, trois déclencheurs, retirer, avancé.
  constexpr std::size_t kTacticLines = 12;

  EntityMenuFacts facts = Facts(EntityFamily::kMonster, 1);
  CHECK(submenu_lines(BuildEntityMenu(facts, {}), "Tactique du compagnon") == 0);

  facts.companion_tactics = {
      moonclient::CompanionTacticFacts{.companion = AiCompanion::kHomunculus}};
  const EntityMenuView bare = BuildEntityMenu(facts, {});
  CHECK(submenu_lines(bare, "Tactique du compagnon") == kTacticLines);
  CHECK(Find(bare, verb_of(AiCompanion::kHomunculus, static_cast<int>(TacticMenuChoice::kRemove)))
            ->disabled);

  // Une ligne « haut » : sa case est cochée, et elle seule parmi les rangs.
  ro::sim::CompanionTactic line{.rank = ro::sim::TacticRank::kHigh};
  facts.companion_tactics.front().line = line;
  const EntityMenuView set = BuildEntityMenu(facts, {});
  const int high = static_cast<int>(TacticMenuChoice::kRankFirst) + 1;
  CHECK(Find(set, verb_of(AiCompanion::kHomunculus, high))->checked);
  CHECK_FALSE(Find(set, verb_of(AiCompanion::kHomunculus, high + 1))->checked);
  CHECK(Find(set,
             verb_of(AiCompanion::kHomunculus, static_cast<int>(TacticMenuChoice::kTriggerFirst)))
            ->checked);
  CHECK_FALSE(
      Find(set, verb_of(AiCompanion::kHomunculus, static_cast<int>(TacticMenuChoice::kRemove)))
          ->disabled);

  // Les deux compagnons : un sous-menu chacun, nommé.
  facts.companion_tactics.push_back(
      moonclient::CompanionTacticFacts{.companion = AiCompanion::kMercenary});
  const EntityMenuView both = BuildEntityMenu(facts, {});
  CHECK(submenu_lines(both, "Tactique de l'homoncule") == kTacticLines);
  CHECK(submenu_lines(both, "Tactique du mercenaire") == kTacticLines);

  // Les verbes se relisent.
  const auto parts = moonclient::TacticVerbOf(TacticVerb(AiCompanion::kMercenary, high));
  REQUIRE(parts.has_value());
  CHECK(parts->companion == AiCompanion::kMercenary);
  CHECK(parts->choice == high);
  CHECK_FALSE(moonclient::TacticVerbOf(static_cast<int>(EntityVerb::kAttack)).has_value());
}

TEST_CASE("Menu d'entité : « Ami du compagnon » est une case sur un joueur", "[app][entity_menu]") {
  using moonclient::CompanionFriendFacts;
  using ro::sim::AiCompanion;
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  // Sans compagnon, aucune case.
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kHomunculusFriend) == nullptr);

  facts.companion_friends = {
      CompanionFriendFacts{.companion = AiCompanion::kHomunculus, .befriended = true}};
  const EntityMenuView one = BuildEntityMenu(facts, {});
  const EntityMenuItem* befriended = Find(one, EntityVerb::kHomunculusFriend);
  REQUIRE(befriended != nullptr);
  CHECK(befriended->toggle);
  CHECK(befriended->checked);
  CHECK(befriended->label == "Ami du compagnon");
  CHECK_FALSE(befriended->disabled);

  // Les deux compagnons : une case chacun, nommée ; une liste pleine grise la
  // case d'un joueur qui n'y est pas encore.
  facts.companion_friends.push_back(
      CompanionFriendFacts{.companion = AiCompanion::kMercenary, .full = true});
  const EntityMenuView both = BuildEntityMenu(facts, {});
  CHECK(Find(both, EntityVerb::kHomunculusFriend)->label == "Ami de l'homoncule");
  const EntityMenuItem* full = Find(both, EntityVerb::kMercenaryFriend);
  CHECK(full->label == "Ami du mercenaire");
  CHECK(full->disabled);
  CHECK_FALSE(full->tip.empty());

  // Un ami est reconnu à son nom : sans nom, la case se grise.
  facts.name.clear();
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kHomunculusFriend)->disabled);
}

TEST_CASE("Menu d'entité : « Conduite du compagnon », un sous-menu sur un joueur",
          "[app][entity_menu]") {
  using moonclient::CompanionConductFacts;
  using moonclient::ConductVerb;
  using moonclient::TacticMenuChoice;
  using ro::sim::AiCompanion;
  const auto verb_of = [](AiCompanion companion, int choice) {
    return static_cast<EntityVerb>(ConductVerb(companion, choice));
  };
  const auto submenu_lines = [](const EntityMenuView& view, const std::string& label) {
    std::size_t count = 0;
    for (const EntityMenuItem& item : view.items) {
      count += item.submenu && item.submenu_label == label ? 1U : 0U;
    }
    return count;
  };
  // Sept rangs, trois déclencheurs, retirer, avancé.
  constexpr std::size_t kConductLines = 12;
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  CHECK(submenu_lines(BuildEntityMenu(facts, {}), "Conduite du compagnon") == 0);

  facts.companion_conducts = {CompanionConductFacts{.companion = AiCompanion::kMercenary}};
  const EntityMenuView bare = BuildEntityMenu(facts, {});
  CHECK(submenu_lines(bare, "Conduite du compagnon") == kConductLines);
  CHECK(Find(bare, verb_of(AiCompanion::kMercenary, static_cast<int>(TacticMenuChoice::kRemove)))
            ->disabled);

  // Une conduite « Ignorer » : sa case est cochée.
  facts.companion_conducts.front().line =
      ro::sim::CompanionPlayerConduct{.rank = ro::sim::TacticRank::kIgnore};
  const int ignore = static_cast<int>(TacticMenuChoice::kRankFirst) +
                     static_cast<int>(ro::sim::TacticRank::kIgnore);
  const EntityMenuView set = BuildEntityMenu(facts, {});
  CHECK(Find(set, verb_of(AiCompanion::kMercenary, ignore))->checked);
  // Le profil change : les cases se recochent d'après lui.
  EntityMenuView refreshed = set;
  moonclient::RefreshConductChecks(refreshed, AiCompanion::kMercenary, std::nullopt);
  CHECK_FALSE(Find(refreshed, verb_of(AiCompanion::kMercenary, ignore))->checked);
  CHECK(
      Find(refreshed, verb_of(AiCompanion::kMercenary, static_cast<int>(TacticMenuChoice::kRemove)))
          ->disabled);

  // Sans nom connu, les rangs se grisent.
  facts.name.clear();
  CHECK(Find(BuildEntityMenu(facts, {}), verb_of(AiCompanion::kMercenary, ignore))->disabled);

  // LES VERBES NE SE CONFONDENT PAS avec ceux des tactiques.
  const auto parts = moonclient::ConductVerbOf(ConductVerb(AiCompanion::kHomunculus, ignore));
  REQUIRE(parts.has_value());
  CHECK(parts->companion == AiCompanion::kHomunculus);
  CHECK(parts->choice == ignore);
  CHECK_FALSE(moonclient::TacticVerbOf(ConductVerb(AiCompanion::kHomunculus, ignore)).has_value());
  CHECK_FALSE(moonclient::ConductVerbOf(moonclient::TacticVerb(AiCompanion::kMercenary, ignore))
                  .has_value());
}
