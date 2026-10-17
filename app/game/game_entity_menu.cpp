// LE MENU DU CLIC DROIT SUR UNE ENTITÉ : ce qui l'ouvre, ce qu'on relève de la
// cible, et ce que ses lignes jouent. Ce qu'il propose se décide dans
// `world/entity_menu.h` ; ce fichier n'en est que le câblage.

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL.h>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/adoption.h"
#include "ro/net/friends.h"
#include "ro/net/guild.h"
#include "ro/net/inventory.h"
#include "ro/net/manner_points.h"
#include "ro/net/npc_dialog.h"
#include "ro/net/party.h"
#include "ro/net/staff_requests.h"
#include "ro/net/whisper_blocks.h"
#include "ro/sim/adoption.h"
#include "ro/sim/bank.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_link.h"

#include "chat/player_link_judge.h"
#include "game/game.h"
#include "world/blocked_npcs.h"
#include "world/click_action.h"
#include "world/follow_action.h"
#include "world/overlay_view.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// Une demande d'ouverture que l'interface n'a pas dessinée dans ce délai a
// péri.
constexpr std::uint32_t kMenuAskLifeMs = 500;

// Assez pour l'identité de pick la plus longue, et pour une phrase de blocage.
constexpr std::size_t kLineBytes = 256;

// La saisie des minutes d'un point de manière ne doit pas promettre plus que le
// fil n'en laisse partir.
static_assert(ro::ui::kMannerMinutesMost == ro::net::kMannerPointsMostMinutes);

[[nodiscard]] ro::net::PlayerAdminAction PlayerAdminOf(EntityVerb verb) {
  switch (verb) {
    case EntityVerb::kComeHere:
      return ro::net::PlayerAdminAction::kComeHere;
    case EntityVerb::kSitStand:
      return ro::net::PlayerAdminAction::kSitStand;
    case EntityVerb::kEventPoints:
      return ro::net::PlayerAdminAction::kEventPoints;
    case EntityVerb::kMute:
      return ro::net::PlayerAdminAction::kMute;
    case EntityVerb::kUnmute:
      return ro::net::PlayerAdminAction::kUnmute;
    case EntityVerb::kNuke:
      return ro::net::PlayerAdminAction::kNuke;
    case EntityVerb::kJail:
      return ro::net::PlayerAdminAction::kJailToggle;
    case EntityVerb::kBlockAccount:
    default:
      break;
  }
  return ro::net::PlayerAdminAction::kBlock;
}

// LES CASES DU SOUS-MENU D'UN COMPAGNON, recochées d'après sa ligne : l'état
// vrai est celui du profil, pas celui de la case qu'on vient de cliquer.
void RefreshTacticChecks(ro::ui::EntityMenuView& view, ro::sim::AiCompanion companion,
                         const std::optional<ro::sim::CompanionTactic>& line) {
  for (ro::ui::EntityMenuItem& item : view.items) {
    const std::optional<TacticVerbParts> parts = TacticVerbOf(item.verb);
    if (!parts.has_value() || parts->companion != companion) {
      continue;
    }
    const int rank = parts->choice - static_cast<int>(TacticMenuChoice::kRankFirst);
    const int trigger = parts->choice - static_cast<int>(TacticMenuChoice::kTriggerFirst);
    if (rank >= 0 && rank < static_cast<int>(ro::sim::kTacticRankCount)) {
      item.checked = line.has_value() && static_cast<int>(line->rank) == rank;
    } else if (trigger >= 0 && trigger < static_cast<int>(ro::sim::kTacticTriggerCount)) {
      item.checked = line.has_value() && static_cast<int>(line->trigger) == trigger;
    } else if (parts->choice == static_cast<int>(TacticMenuChoice::kRemove)) {
      item.disabled = !line.has_value();
      item.tip = line.has_value()
                     ? std::string()
                     : std::string(ro::core::Tr("Aucune tactique n'est posée pour ce monstre."));
    }
  }
}

// LES ACTEURS DE NOS COMPAGNONS, tels que leurs stages les tiennent.
[[nodiscard]] OwnCompanionIds OwnCompanionsOf(const GamePieces& pieces) {
  return {.pet_id = pieces.pet.own_pet_id(),
          .homunculus_id = pieces.homunculus.own_homunculus_id(),
          .mercenary_id = pieces.mercenary.own_mercenary_id()};
}

[[nodiscard]] ro::net::NpcAdminAction NpcAdminOf(EntityVerb verb) {
  switch (verb) {
    case EntityVerb::kNpcMoveHere:
      return ro::net::NpcAdminAction::kMoveToMe;
    case EntityVerb::kNpcUnload:
      return ro::net::NpcAdminAction::kUnload;
    default:
      break;
  }
  return ro::net::NpcAdminAction::kReloadFile;
}

}  // namespace

void Game::AdvanceEntityMenu(std::uint32_t now_ms) {
  ContextMenuOptions& page = settings_->options().context_menu();
  const EntityMenuSettings& settings = page.settings();
  // Ce que le clic gauche lira, et ce que la page de réglages montre.
  menu_gestures_.blocked = &page.blocked();
  menu_gestures_.npc_block = settings.npc_block;
  page.NoteStaff(Rights().staff_tools);
  windows_.entity_menu.view = &menu_view_;
  menu_gestures_.staff_models = Rights().staff_tools && stage_.has_value();

  ro::ui::EntityMenuAnswer answer = std::exchange(windows_.entity_menu.answer, {});
  if (!link_.is_open()) {
    menu_gestures_.ask.reset();
    menu_gestures_.model_ask.reset();
    return;
  }
  // LE MENU D'UN MODÈLE DE LA CARTE : sa ligne choisie va à l'éditeur.
  if (menu_model_.has_value() && answer.chosen != 0) {
    PlayMapModelVerb(std::exchange(answer.chosen, 0));
  }
  // L'INSPECTEUR DU STAFF : sa fenêtre, puis la demande qu'elle attend.
  windows_.entity_inspector.view = &pieces_.entity_inspector.view();
  pieces_.entity_inspector.Advance(windows_.entity_inspector.state,
                                   std::exchange(windows_.entity_inspector.answer, {}),
                                   world_.map_name(), now_ms);
  pieces_.entity_inspector.SendRequest(link_);
  // ET SON JOURNAL DES BLOCAGES DE CHAT, de même.
  windows_.chat_block_log.view = &pieces_.chat_block_log.view();
  pieces_.chat_block_log.Advance(windows_.chat_block_log.state,
                                 std::exchange(windows_.chat_block_log.answer, {}), now_ms);
  pieces_.chat_block_log.SendRequest(link_);

  // ── CE QUE LE MENU A RENDU À L'IMAGE PRÉCÉDENTE ───────────────────────────
  //
  // ⚠ AVANT TOUTE OUVERTURE : une ligne choisie vise la cible relevée quand le
  // menu s'est ouvert, et une nouvelle ouverture la remplacerait.
  if (answer.toggled == static_cast<int>(EntityVerb::kNpcClickBlock)) {
    ToggleNpcBlock(menu_target_.id, menu_target_.name, now_ms);
    // L'état vrai est celui de la LISTE, pas celui de la case.
    for (ro::ui::EntityMenuItem& item : menu_view_.items) {
      if (item.verb == answer.toggled) {
        item.checked = page.blocked().Has(menu_target_.id);
      }
    }
  }
  if (answer.toggled == static_cast<int>(EntityVerb::kHomunculusStandby) ||
      answer.toggled == static_cast<int>(EntityVerb::kMercenaryStandby)) {
    const ro::sim::AiCompanion companion =
        answer.toggled == static_cast<int>(EntityVerb::kHomunculusStandby)
            ? ro::sim::AiCompanion::kHomunculus
            : ro::sim::AiCompanion::kMercenary;
    // ⚠ L'ÉTAT DEMANDÉ, ET NON UNE BASCULE AVEUGLE : l'attente a pu changer
    // pendant que le menu était ouvert (Alt+T, un ordre direct, une carte).
    if (answer.toggled_on != pieces_.companion_ai.standby(companion)) {
      static_cast<void>(pieces_.companion_ai.ToggleStandby(companion, world_, now_ms));
    }
    // L'état vrai est celui de l'IA, pas celui de la case.
    for (ro::ui::EntityMenuItem& item : menu_view_.items) {
      if (item.verb == answer.toggled) {
        item.checked = pieces_.companion_ai.standby(companion);
      }
    }
  }
  // « AMI DU COMPAGNON » : le nom du joueur entre dans la liste du profil, ou
  // en sort. L'état vrai est celui de la liste.
  if (answer.toggled == static_cast<int>(EntityVerb::kHomunculusFriend) ||
      answer.toggled == static_cast<int>(EntityVerb::kMercenaryFriend)) {
    const ro::sim::AiCompanion companion =
        answer.toggled == static_cast<int>(EntityVerb::kHomunculusFriend)
            ? ro::sim::AiCompanion::kHomunculus
            : ro::sim::AiCompanion::kMercenary;
    pieces_.companion_ai.SetCompanionFriend(companion, menu_target_.name, answer.toggled_on,
                                            now_ms);
    for (ro::ui::EntityMenuItem& item : menu_view_.items) {
      if (item.verb == answer.toggled) {
        item.checked = pieces_.companion_ai.IsCompanionFriend(companion, menu_target_.name);
      }
    }
  }
  // LE SOUS-MENU « TACTIQUE DU COMPAGNON » : un rang ou un déclencheur coché
  // devient la ligne de ce monstre ; une case décochée ne retire rien.
  if (const std::optional<TacticVerbParts> parts = TacticVerbOf(answer.toggled);
      parts.has_value()) {
    if (answer.toggled_on) {
      pieces_.companion_ai.ChooseTactic(parts->companion, menu_target_.job, parts->choice);
    }
    RefreshTacticChecks(menu_view_, parts->companion,
                        pieces_.companion_ai.TacticFor(parts->companion, menu_target_.job));
  }
  // LE SOUS-MENU « CONDUITE DU COMPAGNON », sur un joueur, comme la tactique.
  if (const std::optional<TacticVerbParts> parts = ConductVerbOf(answer.toggled);
      parts.has_value()) {
    if (answer.toggled_on) {
      pieces_.companion_ai.ChooseConduct(parts->companion, menu_target_.name, parts->choice);
    }
    RefreshConductChecks(menu_view_, parts->companion,
                         pieces_.companion_ai.ConductFor(parts->companion, menu_target_.name));
  }
  if (const std::optional<TacticVerbParts> parts = ConductVerbOf(answer.chosen);
      parts.has_value()) {
    if (parts->choice == static_cast<int>(TacticMenuChoice::kAdvanced)) {
      pieces_.companion_ai.OpenConductWindow(parts->companion, windows_.companion_ai);
    } else {
      pieces_.companion_ai.ChooseConduct(parts->companion, menu_target_.name, parts->choice);
    }
  } else if (const std::optional<TacticVerbParts> tactic = TacticVerbOf(answer.chosen);
             tactic.has_value()) {
    if (tactic->choice == static_cast<int>(TacticMenuChoice::kAdvanced)) {
      pieces_.companion_ai.OpenTacticWindow(tactic->companion, menu_target_.job, menu_target_.name,
                                            windows_.companion_tactic);
    } else {
      pieces_.companion_ai.ChooseTactic(tactic->companion, menu_target_.job, tactic->choice);
    }
  } else if (answer.chosen != 0) {
    PlayEntityVerb(static_cast<EntityVerb>(answer.chosen), menu_target_.id, 0, now_ms);
  }
  // ⚠ LA CIBLE D'UNE QUESTION EST CELLE QU'ELLE NOMMAIT, recopiée au clic : le
  // menu a pu se rouvrir sur autre chose pendant qu'elle était à l'écran.
  if (answer.confirmed != 0) {
    PlayEntityVerb(static_cast<EntityVerb>(answer.confirmed), answer.confirmed_subject,
                   answer.points, now_ms);
  }

  // ── LE CLIC DROIT D'UN CADRE DU HUD, qui se comporte comme l'entité ──────
  const std::uint32_t framed = targeting_.TakeFrameMenu();
  if (framed != 0 && targeting_.enabled() && targeting_.settings().proxy_click) {
    if (const ro::sim::Actor* actor = world_.Find(framed); actor != nullptr) {
      const EntityFamily family =
          ClassifyEntity(*actor, world_.player_id(), OwnCompanionsOf(pieces_));
      if (FrameClickOpensMenu(family, settings)) {
        OpenEntityMenu(*actor, family, now_ms);
      }
    }
  }

  // ── LE CLIC DROIT DU MONDE ────────────────────────────────────────────────
  if (const std::optional<EntityMenuAsk> ask = std::exchange(menu_gestures_.ask, std::nullopt);
      ask.has_value()) {
    if (const ro::sim::Actor* actor = world_.Find(ask->id); actor != nullptr) {
      const EntityFamily family =
          ClassifyEntity(*actor, world_.player_id(), OwnCompanionsOf(pieces_));
      if (WorldClickOpensMenu(family, actor->id, ask->gesture, world_.storage_open(), settings,
                              Rights().staff_tools)) {
        OpenEntityMenu(*actor, family, now_ms);
      }
    }
  }

  // ── CTRL+ALT+CLIC DROIT SUR UN MODÈLE, pour le staff ─────────────────────
  if (const std::optional<std::size_t> model =
          std::exchange(menu_gestures_.model_ask, std::nullopt);
      model.has_value()) {
    OpenMapModelMenu(*model, now_ms);
  }

  if (windows_.entity_menu.state.open_requested && now_ms - menu_asked_ms_ > kMenuAskLifeMs) {
    windows_.entity_menu.state.open_requested = false;
  }
}

void Game::OpenEntityMenu(const ro::sim::Actor& actor, EntityFamily family, std::uint32_t now_ms) {
  ContextMenuOptions& page = settings_->options().context_menu();
  EntityMenuFacts facts;
  facts.family = family;
  facts.id = actor.id;
  facts.job = actor.look.job;
  if (family == EntityFamily::kGroundItem) {
    facts.item_id = actor.ground_item.has_value() ? actor.ground_item->item_id : 0;
    facts.item_identified = actor.ground_item.has_value() && actor.ground_item->identified;
    // 🔴 LE VRAI NOM, ET SEULEMENT S'IL EST IDENTIFIÉ : le jeu cache encore
    // celui d'un équipement tombé, et l'en-tête reste alors anonyme.
    if (facts.item_id != 0 && facts.item_identified) {
      facts.name = GroundItemName(
          actor, pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
          pieces_.item_words);
    }
    // LA LISTE DE RAMASSAGE AUTOMATIQUE, celle que la description tient.
    if (item_description_.has_value() && item_description_->facts().autoloot_told()) {
      facts.item_autolooted = item_description_->facts().Autoloots(facts.item_id);
    }
  } else {
    facts.name = actor.name;
  }
  facts.targeting = targeting_.enabled();
  facts.target_frame_shown = targeting_.ShownFor(world_, actor.id);
  if (const std::optional<ro::sim::GuildBelonging>& belonging = guild_book_.belonging();
      belonging.has_value()) {
    facts.own_guild_id = static_cast<std::uint32_t>(belonging->guild_id);
    facts.guild_master = guild_book_.IsMaster(ticket_.character_id);
    const ro::sim::GuildRights rights = guild_book_.RightsOf(ticket_.character_id);
    facts.guild_invite_forbidden = rights.known && !rights.may_invite;
  }
  facts.target_guild_id = actor.guild_id;
  facts.in_party = party_.in_party();
  facts.party_leader = party_.AmILeader(world_.player_id());
  facts.target_in_party = party_.Find(actor.id) != nullptr;
  // Sous le nom d'un joueur, la parenthèse de sa carte de nom est son groupe.
  facts.target_other_party = !facts.target_in_party && actor.card.has_value() &&
                             !actor.card->aside.empty() && actor.card->aside != party_.name();
  const ro::sim::Friend* befriended = friends_.FindByName(actor.name);
  facts.target_is_friend = befriended != nullptr && befriended->account_id == actor.id;
  facts.friends_full = friends_.full();
  facts.whisper_blocked = pieces_.whispers.blocks().Blocks(actor.name);
  facts.dealing = pieces_.trade.deal().deal().phase() != ro::sim::DealPhase::kNone;
  facts.followed = world_mouse_.follow.followed_id == actor.id;
  facts.store_open =
      actor.signboard.has_value() && MarketKindOfSignboard(actor.signboard->kind).has_value();
  facts.npc_blocked = page.blocked().Has(actor.id);
  facts.pet_has_accessory = pieces_.pet.has_accessory();
  if (const char* refusal = pieces_.pet.FeedRefusal(); refusal != nullptr) {
    facts.pet_feed_refusal = refusal;
  }
  facts.homunculus_resting = pieces_.homunculus.resting();
  facts.companion_standby = (family == EntityFamily::kOwnHomunculus &&
                             pieces_.companion_ai.standby(ro::sim::AiCompanion::kHomunculus)) ||
                            (family == EntityFamily::kOwnMercenary &&
                             pieces_.companion_ai.standby(ro::sim::AiCompanion::kMercenary));
  if (family == EntityFamily::kMonster) {
    for (const ro::sim::AiCompanion companion :
         {ro::sim::AiCompanion::kHomunculus, ro::sim::AiCompanion::kMercenary}) {
      if (pieces_.companion_ai.present(companion)) {
        facts.companion_tactics.push_back(CompanionTacticFacts{
            .companion = companion,
            .line = pieces_.companion_ai.TacticFor(companion, actor.look.job)});
      }
    }
  }
  if (family == EntityFamily::kPlayer) {
    // « INVITER DANS MON CARNET MVP » : ses raisons, jugées à l'ouverture ;
    // carnet éteint, l'entrée disparaît.
    const MvpInviteCircle hunt{
        .book = pieces_.mvp_tracker.book(),
        .guild = guild_book_,
        .friends = friends_,
        .enabled = settings_->options().pages().mvp_tracker.settings().enabled,
        .link_ready = pieces_.mvp_tracker.link_ready()};
    facts.mvp_invite_withdrawn = !hunt.enabled;
    if (const char* refusal = JudgeMvpInvite(actor.name, hunt); refusal != nullptr) {
      facts.mvp_invite_refusal = refusal;
    }
    // LE COURRIER : le serveur refuse d'en ouvrir un second, et pendant un
    // échange.
    if (pieces_.mail.writing()) {
      facts.mail_refusal = Tr("Un courrier est déjà en cours d'écriture.");
    } else if (facts.dealing) {
      facts.mail_refusal = Tr("Un échange est en cours : le serveur refuse d'ouvrir un courrier.");
    }
    facts.adoption_possible = ro::sim::AdoptionLooksPossible(
        ro::sim::AdoptionFactsOf(world_.player_status().base_level, world_.inventory(),
                                 world_.skill_book(), facts.target_in_party, actor.look.job));
    for (const ro::sim::AiCompanion companion :
         {ro::sim::AiCompanion::kHomunculus, ro::sim::AiCompanion::kMercenary}) {
      if (pieces_.companion_ai.present(companion)) {
        facts.companion_friends.push_back(CompanionFriendFacts{
            .companion = companion,
            .befriended = pieces_.companion_ai.IsCompanionFriend(companion, actor.name),
            .full = pieces_.companion_ai.CompanionFriendsFull(companion)});
        facts.companion_conducts.push_back(
            CompanionConductFacts{.companion = companion,
                                  .line = pieces_.companion_ai.ConductFor(companion, actor.name)});
      }
    }
  }
  const ro::ui::SessionRights rights = Rights();
  facts.staff = rights.staff_tools;
  facts.admin = rights.staff_tools && world_.group_level() >= kAdminMinGroupLevel;

  ro::ui::EntityMenuView built = BuildEntityMenu(facts, page.settings());
  if (built.items.empty()) {
    return;
  }
  menu_target_ = std::move(facts);
  menu_model_.reset();
  menu_view_ = std::move(built);
  windows_.entity_menu.state.open_requested = true;
  menu_asked_ms_ = now_ms;
}

void Game::PlayEntityVerb(EntityVerb verb, std::uint32_t id, int points, std::uint32_t now_ms) {
  const ro::sim::Actor* actor = world_.Find(id);
  // 🔴 LE NOM DE L'ACTEUR AU MOMENT DU CLIC, celui du menu en repli : les gestes
  // qui voyagent par le nom (groupe, amis, guilde) échouent chez le serveur au
  // moindre écart, et le nom retenu à l'ouverture peut avoir changé depuis.
  const std::string& name =
      actor != nullptr && !actor->name.empty() ? actor->name : menu_target_.name;
  if (verb == EntityVerb::kPartyInvite || verb == EntityVerb::kFriendAdd ||
      verb == EntityVerb::kGuildInvite) {
    // Les octets en clair : un caractère invisible ne se voit pas autrement.
    std::string bytes;
    for (const char byte : name) {
      std::array<char, 4> hex{};
      std::snprintf(hex.data(), hex.size(), "%02X ", static_cast<unsigned char>(byte));
      bytes += hex.data();
    }
    ro::core::LogInfo("menu d'entité : demande par le nom pour {} — « {} » [{}], menu : « {} »", id,
                      name, bytes, menu_target_.name);
  }
  switch (verb) {
    case EntityVerb::kNone:
    case EntityVerb::kNpcClickBlock:
    case EntityVerb::kHomunculusStandby:
    case EntityVerb::kMercenaryStandby:
    case EntityVerb::kHomunculusFriend:
    case EntityVerb::kMercenaryFriend:
      return;
    case EntityVerb::kMvpInvite:
      // ⚠ LE NOM PART EN UTF-8 : le carnet convertit lui-même pour le fil.
      pieces_.mvp_tracker.Invite(name, link_);
      return;
    case EntityVerb::kTarget:
      static_cast<void>(targeting_.DesignateByProxy(world_, id));
      return;
    case EntityVerb::kViewEquip:
      // 🔴 L'AID EST RETENU ICI, parce que la réponse ne le porte pas : sans lui,
      // « Actualiser » n'aurait personne à qui redemander. La fenêtre, elle,
      // attend la réponse pour s'ouvrir.
      if (viewed_equipment_.has_value()) {
        viewed_equipment_->NotePending(id);
      }
      ReportRefusal(ro::net::RequestViewEquip(link_, id));
      return;
    case EntityVerb::kGuildInvite:
      ReportRefusal(ro::net::RequestGuildInvite(link_, name));
      return;
    case EntityVerb::kGuildAlliance:
      ReportRefusal(
          ro::net::RequestGuildAlliance(link_, id, world_.player_id(), ticket_.character_id));
      return;
    case EntityVerb::kGuildOpposition:
      ReportRefusal(ro::net::RequestGuildOpposition(link_, id));
      return;
    case EntityVerb::kWhisper: {
      // Le nom part dans la case du destinataire, et le clavier dans la phrase :
      // la barre se déplie, comme pour un lien qu'on vient de poser.
      if (name.empty()) {
        return;
      }
      std::array<char, ro::ui::kChatWhisperNameSize>& box = windows_.chat.main.input.whisper_to;
      box.fill('\0');
      std::copy_n(name.begin(), std::min(name.size(), box.size() - 1), box.begin());
      windows_.chat.main.input.unfolded = true;
      windows_.chat.main.input.focus_on_whisper = false;
      windows_.chat.main.input.wants_focus = true;
      return;
    }
    case EntityVerb::kWhisperBlock:
    case EntityVerb::kWhisperAllow:
      // La liste ne change qu'à la réponse du serveur : le menu suivant la lira.
      ReportRefusal(ro::net::RequestWhisperBlock(link_, pieces_.whispers.blocks(), name,
                                                 verb == EntityVerb::kWhisperBlock
                                                     ? ro::sim::WhisperBlockWay::kBlock
                                                     : ro::sim::WhisperBlockWay::kAllow));
      return;
    case EntityVerb::kMailSend:
      // ⚠ JAMAIS POUR SOI : le menu de soi n'a pas l'entrée, et le clic le
      // revérifie.
      if (id != world_.player_id() && !name.empty()) {
        pieces_.mail.Compose(name);
      }
      return;
    case EntityVerb::kAdopt: {
      // Le serveur ne répond qu'à trois refus ; une demande transmise ne se
      // voit que chez l'adopté. Le dire ici est donc la seule trace du geste.
      ReportRefusal(ro::net::RequestAdoption(link_, id));
      std::array<char, kLineBytes> line{};
      std::snprintf(line.data(), line.size(),
                    Tr("Demande d'adoption envoyée à %s : il lui reste à l'accepter."),
                    name.c_str());
      world_.RecordChat(ro::sim::ChatKind::kServer, 0, line.data(), now_ms, kClientSaysColor);
      return;
    }
    case EntityVerb::kAutolootAdd:
    case EntityVerb::kAutolootRemove:
      // La même liste et le même chemin que la description ; la réponse du
      // serveur revient dans la conversation.
      if (item_description_.has_value()) {
        item_description_->SetAutoloot(link_, menu_target_.item_id,
                                       verb == EntityVerb::kAutolootAdd);
      }
      return;
    case EntityVerb::kFollow:
      // La même garde de poids que Maj + clic droit : voir `StartFollowing`.
      if (actor != nullptr) {
        static_cast<void>(StartFollowing(world_mouse_.follow, world_, *actor, now_ms));
      }
      return;
    case EntityVerb::kUnfollow:
      StopFollowing(world_mouse_.follow, world_, FollowEnd::kLetGo, now_ms);
      return;
    case EntityVerb::kWhisperWindow:
      // LA CONVERSATION 1:1 À PART : elle s'ouvre, ou revient devant, et prend
      // le clavier.
      pieces_.whispers.Open(name);
      return;
    case EntityVerb::kDealPropose:
      // ⚠ PAS PENDANT L'ÉCRITURE D'UN COURRIER : le serveur l'ignorerait en
      // silence. Le refus se dit à l'image suivante.
      if (pieces_.mail.HoldsDealProposal()) {
        return;
      }
      // La proposition part à l'image suivante, une fois à portée.
      pieces_.trade.deal().Propose(
          id, name,
          actor != nullptr ? static_cast<std::uint16_t>(std::max<int>(actor->level, 0)) : 0);
      return;
    case EntityVerb::kCopyName:
      SDL_SetClipboardText(name.c_str());
      return;
    case EntityVerb::kAttack:
      // 🔴 UN ORDRE EXPLICITE : il frappe même sous « le clic cible sans
      // attaquer ». Sans cette dispense, l'entrée emprunterait le chemin du clic
      // et serait annulée avec lui.
      if (actor != nullptr && ClickEngagesAttack(*actor)) {
        EngageTargetAttack(
            *actor,
            Targeting::WantsContinuousAttack(SiegeMap(world_mouse_.engagement, world_.map_mode()),
                                             (SDL_GetModState() & SDL_KMOD_CTRL) != 0,
                                             settings_->options().no_ctrl()),
            now_ms);
      }
      return;
    case EntityVerb::kTalkToNpc:
      // ⚠ DIRECTEMENT, sans repasser par le clic : c'est ce qui laisse parler à
      // un PNJ dont le clic gauche est bloqué.
      ReportRefusal(ro::net::ContactNpc(link_, id));
      return;
    case EntityVerb::kPickup:
      // Comme le clic gauche, et avec sa garde : pendant une visée, le clic
      // appartient au sort, pas au ramassage.
      if (actor == nullptr || grimoire_.aim() != SkillAim::kNothing) {
        ro::core::LogInfo("menu d'entité : ramassage refusé (objet {})", id);
        return;
      }
      pickup_.reset();
      attack_.reset();
      grimoire_.CancelPending();
      ReportRefusal(ClickOn(link_, world_, *actor, now_ms,
                            ClickIntents{pickup_, attack_, grimoire_.approach_clock(),
                                         &world_mouse_.deferred_walk, scene_actors_.player_gesture},
                            false));
      NoteDeedOfClickOn(&world_mouse_.engagement, world_, *actor, false);
      return;
    case EntityVerb::kItemDescription:
      // ⚠ L'IDENTIFIANT DE BASE SEULEMENT : le fil ne donne ni raffinement ni
      // cartes pour un objet au sol.
      ro::ui::OpenItemDescription(windows_.description, menu_target_.item_id, 0,
                                  ro::ui::ItemHolder::kInventory);
      return;
    case EntityVerb::kItemLink:
      // La balise est POSÉE dans la barre, jamais envoyée : le joueur relit et
      // valide.
      PostChatLink(ro::vfs::ItemLink{.item_id = menu_target_.item_id});
      return;
    case EntityVerb::kItemInfo:
    case EntityVerb::kWhoDrops: {
      // Par le chemin COMPLET d'une ligne tapée : mêmes droits, et la réponse
      // revient dans la conversation.
      const char* const command = verb == EntityVerb::kItemInfo ? "@iteminfo " : "@whodrops ";
      SubmitChat(*chat_dispatch_, command + std::to_string(menu_target_.item_id), now_ms);
      return;
    }
    case EntityVerb::kHideTargetFrame:
      targeting_.HideFor(world_, id);
      return;
    case EntityVerb::kCopyId:
      SDL_SetClipboardText(std::to_string(id).c_str());
      return;
    case EntityVerb::kCopyPickIdentity: {
      // Pour un objet au sol, ce qui l'identifie est l'objet, pas sa classe.
      const std::uint32_t shown = menu_target_.family == EntityFamily::kGroundItem
                                      ? menu_target_.item_id
                                      : menu_target_.job;
      std::array<char, kLineBytes> line{};
      std::snprintf(line.data(), line.size(), Tr("%s | AID %u (0x%08X) | job %u"),
                    name.empty() ? Tr("(nom inconnu)") : name.c_str(), id, id, shown);
      SDL_SetClipboardText(line.data());
      return;
    }
    case EntityVerb::kInspect: {
      // En tête de la fenêtre : la famille, le nom, et l'identifiant demandé.
      std::array<char, kLineBytes> subject{};
      std::snprintf(subject.data(), subject.size(), Tr("%s — %s — identifiant %u"),
                    EntityFamilyLabel(menu_target_.family),
                    name.empty() ? Tr("(nom inconnu)") : name.c_str(), id);
      pieces_.entity_inspector.Open(windows_.entity_inspector.state, id, subject.data(),
                                    world_.map_name(), now_ms);
      return;
    }
    case EntityVerb::kChatBlockLog: {
      // En tête de la fenêtre : le nom du personnage, et l'AID demandé.
      std::array<char, kLineBytes> subject{};
      std::snprintf(subject.data(), subject.size(), Tr("%s — AID %u"),
                    name.empty() ? Tr("(nom inconnu)") : name.c_str(), id);
      pieces_.chat_block_log.Open(windows_.chat_block_log.state, id, subject.data(), now_ms);
      return;
    }
    case EntityVerb::kMarketCompare:
      // L'ENSEIGNE EST RELUE AU CLIC : l'échoppe a pu fermer depuis l'ouverture.
      if (const ro::sim::Actor* shop = world_.Find(id); shop != nullptr && shop->signboard) {
        if (const auto kind = MarketKindOfSignboard(shop->signboard->kind); kind.has_value()) {
          pieces_.trade.market().AskCompare({.owner_aid = id, .kind = *kind, .owner_name = name});
        }
      }
      return;
    case EntityVerb::kReportPlayer:
      // ⚠ JAMAIS SOI : le menu de soi n'a pas l'entrée, et le clic le revérifie.
      if (id != world_.player_id()) {
        pieces_.player_report.Open(windows_.player_report, id, name);
      }
      return;
    // ── L'OUTILLAGE DU STAFF ───────────────────────────────────────────────
    // Aucun compte rendu ici : le serveur rejoue la commande @ et SON verdict —
    // agi, ou refusé faute de permission — revient dans la conversation. Le dire
    // nous-mêmes, ce serait promettre une action avant de savoir si elle a eu
    // lieu.
    case EntityVerb::kComeHere:
    case EntityVerb::kSitStand:
    case EntityVerb::kEventPoints:
    case EntityVerb::kMute:
    case EntityVerb::kUnmute:
    case EntityVerb::kNuke:
    case EntityVerb::kJail:
    case EntityVerb::kBlockAccount:
      ReportRefusal(ro::net::RequestPlayerAdmin(link_, id, PlayerAdminOf(verb), points));
      return;
    case EntityVerb::kKick:
      ReportRefusal(ro::net::RequestKick(link_, id));
      return;
    case EntityVerb::kMannerGood:
    case EntityVerb::kMannerBad:
      // Le verdict revient en ZC 0x014A, et s'écrit dans la conversation.
      ReportRefusal(ro::net::RequestMannerPoint(link_, id,
                                                verb == EntityVerb::kMannerGood
                                                    ? ro::net::MannerPointWay::kGood
                                                    : ro::net::MannerPointWay::kBad,
                                                points));
      return;
    case EntityVerb::kFullStrip:
      ReportRefusal(ro::net::RequestFullStrip(link_, id));
      return;
    case EntityVerb::kNpcReloadFile:
    case EntityVerb::kNpcMoveHere:
    case EntityVerb::kNpcUnload:
      ReportRefusal(ro::net::RequestNpcAdmin(link_, id, NpcAdminOf(verb)));
      return;
    case EntityVerb::kMonsterSheet:
      // 🔴 LA CLASSE DE SPRITE, RELEVÉE À L'OUVERTURE : c'est la seule identité
      // qu'un acteur porte à l'écran — le client n'a pas la base des monstres.
      // Le serveur remonte d'elle à l'identifiant, exactement comme pour Sense.
      if (monster_sheet_.has_value() && menu_target_.job != 0) {
        monster_sheet_->Open(windows_.monster_sheet.state, menu_target_.job, /*by_view=*/true);
        monster_sheet_->NoteSeenOn(world_.map_name());
      }
      return;
    case EntityVerb::kMonsterLink:
      // ⚠ LA CLASSE DE SPRITE PART COMME IDENTIFIANT : un acteur n'en porte pas
      // d'autre, et les deux coïncident pour l'écrasante majorité des monstres.
      // Le RANG est ordinaire : un acteur ne dit pas s'il est MVP — la fiche, à
      // l'ouverture chez le lecteur, le saura.
      PostChatLink(MonsterLink{.monster_id = menu_target_.job, .name = name});
      return;
    case EntityVerb::kNpcLink:
      // 🔴 AVEC NOTRE CARTE : un nom de PNJ est un RÔLE, posé à plusieurs
      // endroits du serveur, et le lien désigne celui qu'on a sous les yeux.
      // ⚠ SANS LA PARTIE CACHÉE DU NOM : voir `NpcSearchTerm`.
      PostChatLink(NaviSearchLink{
          .family = NaviSearchFamily::kNpc, .map = world_.map_name(), .term = NpcSearchTerm(name)});
      return;
    // ── LE GROUPE ET LES AMIS ──────────────────────────────────────────────
    // Le serveur revalide chaque geste, et sa réponse passe par la conversation.
    case EntityVerb::kPartyInvite:
      ReportRefusal(ro::net::RequestPartyInvite(link_, name));
      return;
    case EntityVerb::kPartyLeader:
      ReportRefusal(ro::net::RequestPartyLeader(link_, id));
      return;
    case EntityVerb::kPartyExpel: {
      // Le nom de la liste plutôt que celui du menu : le serveur compare les deux.
      const ro::sim::PartyMember* member = party_.Find(id);
      ReportRefusal(ro::net::RequestPartyExpel(link_, id, member != nullptr ? member->name : name));
      return;
    }
    case EntityVerb::kFriendAdd:
      ReportRefusal(ro::net::RequestFriend(link_, name));
      return;
    case EntityVerb::kFriendRemove: {
      // Un ami est un PERSONNAGE : son compte et son nom le désignent ensemble,
      // et le compte seul quand le menu s'est rouvert ailleurs depuis la question.
      const ro::sim::Friend* chum = friends_.FindByName(name);
      if (chum == nullptr || chum->account_id != id) {
        chum = friends_.FindByAccount(id);
      }
      if (chum != nullptr) {
        ReportRefusal(ro::net::RequestFriendRemoval(link_, chum->account_id, chum->char_id));
      }
      return;
    }
    case EntityVerb::kPetStatus:
      pieces_.pet.ToggleWindow(windows_.pet, link_);
      return;
    case EntityVerb::kPetFeed:
      pieces_.pet.Feed(link_, world_);
      return;
    case EntityVerb::kPetPerform:
      pieces_.pet.Perform(link_);
      return;
    case EntityVerb::kPetUnequip:
      pieces_.pet.UnequipAccessory(link_);
      return;
    case EntityVerb::kPetToEgg:
      pieces_.pet.ReturnToEgg(link_);
      return;
    // LE STATUT DE L'HOMONCULE ET DU MERCENAIRE est leur onglet de la fiche,
    // ouvert dessus même s'il était ailleurs.
    case EntityVerb::kHomunculusStatus:
      windows_.character.open = true;
      windows_.character.tab = ro::ui::CharacterTab::kHomunculus;
      windows_.character.tab_request = ro::ui::CharacterTab::kHomunculus;
      return;
    case EntityVerb::kHomunculusFeed:
      pieces_.homunculus.Feed(link_);
      return;
    case EntityVerb::kMercenaryStatus:
      pieces_.mercenary.RequestInfo(link_);
      windows_.character.open = true;
      windows_.character.tab = ro::ui::CharacterTab::kMercenary;
      windows_.character.tab_request = ro::ui::CharacterTab::kMercenary;
      return;
  }
}

void Game::ToggleNpcBlock(std::uint32_t id, const std::string& name, std::uint32_t now_ms) {
  const bool blocked = settings_->ToggleBlockedNpc(id, name);
  const std::string who = BlockedNpcLabel(id, name);
  std::array<char, kLineBytes> line{};
  if (blocked) {
    // Marqué comme déjà expliqué : la phrase qui suit dit tout, et le premier
    // clic de vérification ne doit pas la redire deux secondes plus tard.
    static_cast<void>(settings_->options().context_menu().blocked().FirstExplanation(id));
    std::snprintf(line.data(), line.size(),
                  Tr("%s : le clic gauche ne fait plus rien sur lui. Clic droit pour le "
                     "débloquer ou lui parler."),
                  who.c_str());
  } else {
    std::snprintf(line.data(), line.size(), Tr("%s : le clic gauche est de nouveau actif."),
                  who.c_str());
  }
  world_.RecordChat(ro::sim::ChatKind::kServer, 0, line.data(), now_ms, kClientSaysColor);
}

}  // namespace moonclient
