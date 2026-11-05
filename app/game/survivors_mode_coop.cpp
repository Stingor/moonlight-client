#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/action.h"
#include "ro/sim/survivors_match.h"
#include "ro/sim/survivors_meta.h"

#include "game/survivors_coop_mode.h"
#include "game/survivors_mode.h"
#include "survivors/arena_choice.h"
#include "survivors/survivors_looks.h"
#include "voice/voice_mode.h"
#include "world/actor_view.h"

// LA COOP DANS LE MODE : le salon du groupe depuis l'accueil, le départ que le
// serveur donne, et ce que chaque image de partie doit au transport.

namespace moonclient {
namespace {

using ro::net::SurvivorsCoopAction;
using ro::net::SurvivorsCoopState;

// Le visage d'un personnage sans acteur : le même que seul.
constexpr std::uint8_t kFallbackFaceSex = 1;
constexpr std::uint16_t kFallbackFaceHair = 1;
// L'attente des autres se dit après ceci, et ce joueur renonce après cela : un allié
// figé mais connecté ne doit retenir personne à jamais.
constexpr std::uint64_t kCoopWaitNoticeMs = 3000;
constexpr std::uint64_t kCoopGiveUpMs = 30000;

[[nodiscard]] bool Sitting(const ro::sim::World& world) {
  const ro::sim::Actor* player = world.player();
  return player != nullptr && player->posture == ro::sim::Posture::kSitting;
}

}  // namespace

void SurvivorsMode::FollowCoopDoor(SurvivorsStage& stage, const SurvivorsContext& context,
                                   const ro::ui::SurvivorsPrepareAsk& asked, bool refused) {
  if (context.coop == nullptr) return;
  SurvivorsCoopMode& coop = *context.coop;
  MinigameWindows& windows = *context.minigames;
  const ro::net::SurvivorsCoopLobby& lobby = coop.lobby();
  const bool in_lobby = lobby.state == SurvivorsCoopState::kLobby;

  if (asked.coop_enter) coop.Ask(*context.link, SurvivorsCoopAction::kEnter);
  if (asked.coop_leave) {
    coop.Ask(*context.link, SurvivorsCoopAction::kLeave);
  }
  if (asked.coop_ready && !refused) {
    // LA PART DE CE JOUEUR : la partie libre que l'accueil a réglée, et son apparence.
    const SurvivorsBookMode::Begun begun =
        book_.StartAs(SurvivorsBoardKind::kFree, Rules(), context.client_root, 0, ChosenBolt());
    const ro::sim::ClassRule* hero_class = ro::sim::FindClass(Rules(), begun.start.class_id);
    if (hero_class != nullptr) {
      ro::sim::SurvivorsCoopPart part;
      part.ruleset_version = Rules().ruleset_version;
      part.rules_digest = ro::sim::SurvivorsRulesDigest(Rules());
      part.arena_id = begun.start.arena_id;
      part.cursed_water = begun.start.cursed_water;
      part.player.class_id = begun.start.class_id;
      part.player.first_skill_id = begun.start.first_skill_id;
      part.player.loadout = begun.start.loadout;
      part.player.lives = begun.start.lives;
      part.player.leaves =
          ro::sim::LoadoutOf(ro::sim::ShippedSurvivorsMeta(), desk_.save().meta.ranks, *hero_class)
              .leaves;
      part.player.locked_ids = begun.start.locked_ids;
      std::vector<std::uint8_t> bytes = ro::sim::EncodeSurvivorsCoopPart(part);
      const ro::sim::Actor* player = context.world->player();
      const ro::render::ActorLook face =
          player != nullptr
              ? LookOf(*player)
              : ro::render::ActorLook{
                    .player = true, .sex = kFallbackFaceSex, .hair = kFallbackFaceHair};
      const std::vector<std::uint8_t> look = EncodeCoopLook(face);
      bytes.insert(bytes.end(), look.begin(), look.end());
      coop.Ask(*context.link, SurvivorsCoopAction::kReady, bytes);
    }
  }

  // CE JOUEUR EST PRÊT QUAND LE SERVEUR LE DIT : un paquet refusé par sa cadence ne
  // laisse pas croire le contraire.
  bool own_ready = false;
  coop_members_.clear();
  for (const ro::net::SurvivorsCoopMember& member : lobby.members) {
    own_ready = own_ready || (member.char_id == context.own_char_id && member.ready);
    coop_members_.push_back(
        ro::ui::SurvivorsPrepareCoopMember{.name = member.name.c_str(), .ready = member.ready});
  }
  coop_refusals_.clear();
  const auto say_if = [&](std::uint32_t bit, const char* text) {
    if ((lobby.refusals & bit) != 0) coop_refusals_.push_back(text);
  };
  say_if(ro::net::kSurvivorsCoopTooMany,
         ro::core::TrText("Le groupe compte plus de quatre membres."));
  say_if(ro::net::kSurvivorsCoopTooFew,
         ro::core::TrText("Il faut au moins deux membres du groupe en ligne."));
  say_if(ro::net::kSurvivorsCoopOffline, ro::core::TrText("Un membre du groupe est hors ligne."));
  say_if(ro::net::kSurvivorsCoopOutOfTown,
         ro::core::TrText("Un membre du groupe n'est ni en ville ni en intérieur."));
  say_if(ro::net::kSurvivorsCoopNoClient,
         ro::core::TrText("Un membre du groupe joue sur un client qui ne connaît pas la coop."));
  if (lobby.state == SurvivorsCoopState::kPlaying) {
    coop_refusals_.push_back(ro::core::TrText("Votre groupe est déjà en partie."));
  }
  windows.survivors_prepare_view.coop = ro::ui::SurvivorsPrepareCoop{.shown = coop.announced(),
                                                                     .in_lobby = in_lobby,
                                                                     .ready = own_ready,
                                                                     .members = coop_members_,
                                                                     .refusals = coop_refusals_};
  EnterCoop(stage, context);
}

void SurvivorsMode::EnterCoop(SurvivorsStage& stage, const SurvivorsContext& context) {
  if (context.coop == nullptr || stage.owns_screen()) return;
  SurvivorsCoopMode& coop = *context.coop;
  const std::optional<ro::net::SurvivorsCoopStart> started = coop.TakeStart();
  if (!started.has_value()) return;
  const std::optional<SurvivorsCoopDeparture> departure =
      DepartureOf(*started, context.own_char_id, Rules());
  const auto refuse = [&](const char* why) {
    context.say(ro::core::Tr("La partie du groupe ne peut pas commencer."));
    ro::core::LogWarning("coop : {}", why);
    coop.Ask(*context.link, SurvivorsCoopAction::kLeave);
  };
  if (!departure.has_value()) {
    refuse("le départ se lit mal, ou les règles des clients diffèrent");
    return;
  }
  const ro::sim::ArenaRule* arena = ro::sim::FindArena(Rules(), departure->start.arena_id);
  if (arena == nullptr || context.vfs == nullptr || context.wardrobe == nullptr) {
    refuse("arène inconnue, ou archives absentes");
    return;
  }
  auto walk = ArenaWalkOf(context.vfs, *arena);
  if (!walk) {
    refuse("le sol de l'arène ne se lit pas");
    return;
  }
  coop.Roster(*started, context.own_char_id);
  // Ce que le chemin rapide a gardé vient de la partie d'avant.
  if (context.voice != nullptr) (void)context.voice->TakeCoop();
  stage.Open();
  if (auto begun = stage.BeginCoop(Rules(), std::move(*walk), departure->start,
                                   departure->local_player, departure->input_delay_ticks);
      !begun) {
    ro::core::LogWarning("coop : {}", ro::core::Describe(begun.error()));
    refuse("la partie refuse de commencer");
    return;
  }
  local_hero_ = departure->local_player;
  ally_faces_ = departure->looks;
  ally_jobs_ = {};
  coop_left_told_ = false;
  coop_wait_told_ = false;
  if (!posture_.seated_by_us() && posture_.SitOnEntry(Sitting(*context.world)) &&
      context.link->is_open()) {
    if (auto sat = ro::net::RequestSit(*context.link, true); !sat) {
      ro::core::LogWarning("survivors : s'asseoir — {}", ro::core::Describe(sat.error()));
    }
  }
  AfterBegin(context);
}

bool SurvivorsMode::FollowCoopFrame(SurvivorsStage& stage, const SurvivorsContext& context) {
  if (context.coop == nullptr || !stage.board().coop) return true;
  SurvivorsCoopFastPath fast_path;
  if (context.voice != nullptr) {
    fast_path.take = [voice = context.voice] {
      std::vector<ro::net::SurvivorsCoopFrame> frames;
      for (ro::voice::CoopRelayed& relayed : voice->TakeCoop()) {
        frames.push_back(ro::net::SurvivorsCoopFrame{.char_id = relayed.sender_id,
                                                     .payload = std::move(relayed.payload)});
      }
      return frames;
    };
    fast_path.post = [voice = context.voice](std::vector<std::uint8_t> payload) {
      voice->PostCoop(std::move(payload));
    };
  }
  context.coop->Carry(stage, *context.link, fast_path);
  const bool excluded = context.coop->TakeExcluded();
  if (stage.coop() && !stage.coop_over() && !coop_left_told_) {
    if (excluded) {
      context.say(ro::core::Tr("Vous n'êtes plus dans la partie du groupe."));
      stage.AbandonCoop();
    } else if (stage.coop_stalled_ms() >= kCoopGiveUpMs) {
      context.say(ro::core::Tr("Le groupe ne répond plus : vous quittez la partie."));
      stage.AbandonCoop();
    } else if (stage.coop_stalled_ms() < kCoopWaitNoticeMs) {
      coop_wait_told_ = false;
    } else if (!coop_wait_told_) {
      coop_wait_told_ = true;
      context.say(ro::core::Tr("En attente des autres joueurs…"));
    }
  }
  // UN JOUEUR QUI ABANDONNE OU QUI A FINI quitte l'équipe tout de suite, sans attendre
  // de quitter son bilan : c'est ce qui rend le salon au groupe. Ses dernières entrées
  // viennent de partir.
  if ((!stage.coop() || stage.coop_over()) && !coop_left_told_) {
    context.coop->Ask(*context.link, SurvivorsCoopAction::kLeave);
    coop_left_told_ = true;
  }
  const ro::sim::SurvivorsMatch* match = stage.match();
  if (match == nullptr || !match->diverged()) return true;
  context.say(ro::core::Tr("La partie du groupe a divergé : elle s'arrête ici."));
  ro::core::LogWarning("coop : l'empreinte d'un autre client diffère de la nôtre");
  Exit(stage, context);
  return false;
}

void SurvivorsMode::PoseAllyLooks(const SurvivorsStage& stage) {
  if (!arena_.has_value() || stage.play() == nullptr) return;
  const ro::sim::SurvivorsField& field = stage.play()->run().field();
  for (std::size_t index = 0; index < ro::sim::HeroCount(field); ++index) {
    if (index == field.stage) continue;
    const ro::sim::ClassRule* hero_class = ro::sim::ClassAt(field, index);
    if (hero_class == nullptr) continue;
    const std::uint16_t job = ro::sim::HeroJobOf(*hero_class, ro::sim::HeroAt(field, index));
    if (ally_jobs_[index] == job) continue;
    arena_->SetAllyLook(index, HeroLook(ally_faces_[index], *hero_class, job));
    ally_jobs_[index] = job;
  }
}

}  // namespace moonclient
