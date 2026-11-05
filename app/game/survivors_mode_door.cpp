#include <algorithm>
#include <cstdint>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/action.h"

#include "game/survivors_coop_mode.h"
#include "game/survivors_mode.h"
#include "game/survivors_ranking_desk.h"
#include "survivors/arena_choice.h"
#include "survivors/survivors_book.h"
#include "survivors/survivors_door.h"

// LA PORTE DU MODE : l'accueil, l'entrée en partie, le montage de l'arène, la
// sortie et la partie figée.

namespace moonclient {
namespace {

// Le premier bolt présélectionné tant que le joueur n'en a choisi aucun
// (D2) : Fire Bolt.
constexpr std::uint16_t kFireBoltSkill = 19;

[[nodiscard]] bool Sitting(const ro::sim::World& world) {
  const ro::sim::Actor* player = world.player();
  return player != nullptr && player->posture == ro::sim::Posture::kSitting;
}

}  // namespace

const ro::sim::ClassRule* SurvivorsMode::PlayingClass() const {
  const ro::sim::SurvivorsRules& rules = Rules();
  if (book_.chosen() == SurvivorsBoardKind::kFree) {
    return FreeClassOf(rules, desk_.save(), book_.free_class_id());
  }
  return rules.classes.empty() ? nullptr : &rules.classes.front();
}

std::uint16_t SurvivorsMode::ChosenBolt() const {
  const ro::sim::ClassRule* playing = PlayingClass();
  if (playing == nullptr || playing->starter_skills.empty()) return 0;
  const std::span<const std::uint16_t> bolts = playing->starter_skills;
  if (std::ranges::find(bolts, options_.first_skill_id) != bolts.end()) {
    return options_.first_skill_id;
  }
  if (std::ranges::find(bolts, kFireBoltSkill) != bolts.end()) return kFireBoltSkill;
  return bolts.front();
}

void SurvivorsMode::FollowDoor(SurvivorsStage& stage, const SurvivorsContext& context) {
  MinigameWindows& windows = *context.minigames;
  if (!options_loaded_) {
    desk_.Reload(context.client_root, context.say);
    options_ = desk_.save().options;
    options_loaded_ = true;
  }
  FollowObjective(std::exchange(windows.survivors_meta.codex_answer, {}).objective_skill_id,
                  context);
  // CE QUE LE CLASSEMENT A À DIRE AU JOUEUR (Q3, Q5, Q6), partie ou non.
  if (context.ranking != nullptr) SayRankingNotices(*context.ranking, stage, context.say);
  if (std::optional<SurvivorsEnding> ending =
          hub_door_.EndingToShow(windows.hub.open, stage.screen(), context.client_root)) {
    music_volume_.Follow(context.audio->music_volume());
    stage.ShowEnding(std::move(*ending));
    clock_.Follow(false, context.now_ms);
  }
  // QUITTER L'ACCUEIL QUITTE SON SALON : les autres n'attendront pas un joueur parti
  // ailleurs, et aucun départ ne le rattrapera plus tard.
  const auto forsake_lobby = [&] {
    if (context.coop != nullptr && !stage.coop()) context.coop->Forsake(*context.link);
  };
  if (stage.owns_screen()) {
    forsake_lobby();
    windows.survivors_prepare_composed = false;
    return;
  }
  if (!windows.survivors_prepare.open) {
    forsake_lobby();
    if (stage.screen() == SurvivorsScreen::kPreparing) stage.Close();
    windows.survivors_prepare_composed = false;
    windows.survivors_meta.CloseAll();
    return;
  }
  // L'ACCUEIL QUI S'OUVRE RELIT LE CARNET : une partie ou un autre client a pu l'écrire.
  if (stage.screen() != SurvivorsScreen::kPreparing) desk_.Reload(context.client_root, context.say);
  stage.Open();
  const ro::ui::SurvivorsPrepareAsk asked = std::exchange(windows.survivors_prepare_answer, {});
  desk_.FollowPrepare(asked, windows.survivors_meta, book_, Rules(), context.client_root,
                      context.say);
  desk_.FollowShops(windows.survivors_meta, context.client_root, context.say);
  if (context.ranking != nullptr) {
    FollowRankingAsks(
        *context.ranking, windows.survivors_meta.ranking,
        SurvivorsRankingAsks{.open = asked.open_ranking,
                             .window = std::exchange(windows.survivors_meta.ranking_answer, {})},
        Rules());
  }
  if (asked.choose_skill_id.has_value() && *asked.choose_skill_id != options_.first_skill_id) {
    const SurvivorsOptions before = options_;
    options_.first_skill_id = *asked.choose_skill_id;
    SaveOptions(before, context);
  }
  const std::optional<EntryRefusal> refusal = Judge(EntryFactsOf(*context.world, context.aside));
  const MoveKeyLabels labels = LabelsOf(context.key_namer);
  key_labels_ = {labels.north, labels.west, labels.south, labels.east};
  views_.ComposePrepare(windows.survivors_prepare_view, Rules(), PlayingClass(), ChosenBolt(),
                        refusal.has_value() ? RefusalText(*refusal) : nullptr, key_labels_,
                        context.tables);
  views_.ComposeMeta(windows.survivors_prepare_view, windows.survivors_meta, desk_, book_, Rules(),
                     context.tables, context.ranking, context.now_ms);
  windows.survivors_prepare_composed = true;
  FollowCoopDoor(stage, context, asked, refusal.has_value());
  if (asked.start && !refusal.has_value() && !stage.coop()) {
    forsake_lobby();
    // La vue pointe dans le salon qui vient de s'effacer, et un départ qui échoue laisse
    // la fenêtre ouverte.
    windows.survivors_prepare_view.coop = {};
    Enter(stage, context, ChosenBolt());
  }
}

void SurvivorsMode::Enter(SurvivorsStage& stage, const SurvivorsContext& context,
                          std::uint16_t first_skill_id) {
  if (context.vfs == nullptr || context.wardrobe == nullptr) {
    context.say(ro::core::Tr("L'arène des Survivants de Midgard ne peut pas se monter."));
    ro::core::LogWarning("survivors : ni archives ni garde-robe, la partie ne commence pas");
    return;
  }
  std::random_device entropy;
  const std::uint64_t seed = (std::uint64_t{entropy()} << 32U) | entropy();
  // UNE PARTIE CLASSÉE OU UN DÉFI DEMANDE SON JETON (Q2, Q3) : l'écran de
  // chargement couvre l'attente, et `Frame` refait le départ qu'elle règle.
  // L'arène vient du départ : la prairie pour le défi et la partie classée,
  // celle de l'accueil pour la partie libre ; une classée sans classement
  // part libre sur la prairie.
  const SurvivorsBookMode::Begun book_start =
      context.ranking != nullptr
          ? context.ranking->Launch(book_, Rules(), context.client_root, seed, first_skill_id)
      : book_.chosen() == SurvivorsBoardKind::kRanked
          ? book_.StartRankedAsFree(Rules(), context.client_root, seed, first_skill_id)
          : book_.StartOf(Rules(), context.client_root, seed, first_skill_id);
  TellDamagedCopy(book_.TakeDamagedCopy(), context.say);
  if (auto begun = BeginRun(stage, context, book_start); !begun) {
    context.say(ro::core::Tr("L'arène des Survivants de Midgard ne peut pas se monter."));
    ro::core::LogWarning("survivors : la partie refuse de commencer — {}",
                         ro::core::Describe(begun.error()));
    return;
  }
  local_hero_ = 0;
  // « Rejouer » garde le personnage assis par nous : rien à redemander.
  if (!posture_.seated_by_us() && posture_.SitOnEntry(Sitting(*context.world)) &&
      context.link->is_open()) {
    if (auto sat = ro::net::RequestSit(*context.link, true); !sat) {
      ro::core::LogWarning("survivors : s'asseoir — {}", ro::core::Describe(sat.error()));
    }
  }
  AfterBegin(context);
}

void SurvivorsMode::AfterBegin(const SurvivorsContext& context) {
  context.minigames->survivors_prepare.open = false;
  context.minigames->survivors_meta.CloseAll();
  music_volume_.Follow(context.audio->music_volume());
  watch_.Start(RealFactsOf(*context.world, context.aside, 0));
  whispers_.Start(context.world->chat_lines());
  banner_.reset();
  route_.Reset();
  run_saved_ = false;
  trophies_.Restart();
  book_.Restart();
  loading_shown_ = false;
  clock_.Follow(false, context.now_ms);
}

ro::core::Result<void> SurvivorsMode::BeginRun(SurvivorsStage& stage,
                                               const SurvivorsContext& context,
                                               const SurvivorsBookMode::Begun& begun) {
  const ro::sim::ArenaRule* arena = ro::sim::FindArena(Rules(), begun.start.arena_id);
  if (arena == nullptr) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument, "arène du départ inconnue");
  }
  // LE SOL DE L'ARÈNE CHOISIE, lu sans monter la scène : la partie naît avant
  // l'arène, derrière l'écran de chargement — sans installation sur le sol
  // généré.
  auto walk = ArenaWalkOf(context.vfs, *arena);
  if (!walk) return std::unexpected(walk.error());
  return stage.Begin(Rules(), std::move(*walk), begun.start, begun.board);
}

bool SurvivorsMode::ArenaShown(const SurvivorsStage& stage) const {
  return arena_.has_value() && stage.play() != nullptr &&
         arena_->Shows(stage.play()->run().field().start);
}

bool SurvivorsMode::Mount(SurvivorsStage& stage, const SurvivorsContext& context) {
  const ro::sim::SurvivorsRules& rules = Rules();
  const ro::sim::SurvivorsStart& start = stage.play()->run().field().start;
  // UNE ARÈNE GARDÉE POUR « REJOUER » repart telle quelle ; une autre arène, ou
  // le sol généré sous une autre graine, se remonte.
  if (!ArenaShown(stage)) {
    if (!arena_.has_value()) arena_.emplace(*context.vfs, *context.wardrobe, rules);
    const MapStageRead read{.rounded_ground =
                                context.graphics != nullptr && context.graphics->rounded_ground()};
    if (auto opened = arena_->Open(*context.vfs, rules, start, read); !opened) {
      ro::core::LogWarning("survivors : {}", ro::core::Describe(opened.error()));
      context.say(ro::core::Tr("L'arène des Survivants de Midgard ne peut pas se monter."));
      arena_.reset();
      Exit(stage, context);
      return false;
    }
  }
  arena_->Restart();
  PoseArenaOptions();
  arena_->SetQuake(context.quake);
  hero_job_.reset();
  // LA CHAUFFE : chaque espèce de la première vague, et les bolts de départ.
  const ro::sim::SurvivorsField& field = stage.play()->run().field();
  std::vector<std::uint16_t> skills;
  if (const ro::sim::ClassRule* hero_class = ro::sim::FindClass(rules, start.class_id)) {
    const std::span<const std::uint16_t> starters = hero_class->starter_skills;
    skills.assign(starters.begin(), starters.end());
  }
  const std::span<const ro::sim::WaveRule> waves = field.content.waves;
  const std::span<const std::uint16_t> species =
      waves.empty() ? std::span<const std::uint16_t>() : waves.front().foe_ids;
  arena_->Warm(species, skills,
               CellOfField(glm::vec2(static_cast<float>(field.hero.position.x),
                                     static_cast<float>(field.hero.position.y))),
               context.width, context.height, context.now_ms);
  stage.Loaded();
  // LE BANDEAU DE L'ARÈNE pendant la descente : son nom, du catalogue du sol
  // généré ou de la table des cartes.
  const ro::sim::ArenaRule* arena = ro::sim::FindArena(rules, start.arena_id);
  const std::optional<ro::vfs::MapSign> sign =
      arena != nullptr ? ArenaSignOf(rules, *arena, context.map_names) : std::nullopt;
  sign_.Enter(sign.has_value() ? &*sign : nullptr, context.sign_mode, context.now_ms);
  music_.Reset();
  music_volume_.Forget();
  sounds_.Clear();
  measures_.Reset();
  views_.ResetOffer();
  views_.ResetPause();
  return true;
}

bool SurvivorsMode::Resettle(SurvivorsStage& stage, const SurvivorsContext& context) {
  if (context.ranking == nullptr) return true;
  const std::optional<SurvivorsBookMode::Begun> again =
      context.ranking->Settle(book_, Rules(), context.client_root);
  if (!again.has_value()) return true;
  const ro::core::Result<void> begun = BeginRun(stage, context, *again);
  if (begun) return true;
  ro::core::LogWarning("survivors : le départ réglé par le classement ne commence pas — {}",
                       ro::core::Describe(begun.error()));
  context.say(ro::core::Tr("L'arène des Survivants de Midgard ne peut pas se monter."));
  Exit(stage, context);
  return false;
}

void SurvivorsMode::Exit(SurvivorsStage& stage, const SurvivorsContext& context) {
  if (posture_.StandOnExit(Sitting(*context.world)) && context.link->is_open()) {
    if (auto stood = ro::net::RequestSit(*context.link, false); !stood) {
      ro::core::LogWarning("survivors : se relever — {}", ro::core::Describe(stood.error()));
    }
  }
  if (stage.board().coop && !coop_left_told_ && context.coop != nullptr) {
    context.coop->Ask(*context.link, ro::net::SurvivorsCoopAction::kLeave);
  }
  coop_left_told_ = false;
  // LA MUSIQUE REFAIT SUIVRE LA CARTE DU PERSONNAGE, au volume des options.
  context.music->Forget();
  context.audio->SetMusicVolume(music_volume_.player_volume());
  // Les bruits de l'arène se taisent avec elle ; ceux de l'interface restent.
  context.audio->StopSounds(ro::audio::SoundChannel::kEffects);
  sounds_.Clear();
  arena_.reset();
  route_.Reset();
  sign_.Forget();
  sign_view_.reset();
  banner_.reset();
  clock_.Follow(false, context.now_ms);
  stage.Close();
  if (context.options_window != nullptr) {
    pause_options_.Settle(context.options_window->open, stage.screen());
  }
}

void SurvivorsMode::Freeze(SurvivorsStage& stage, MinigameStage& minigames,
                           const std::filesystem::path& client_root) {
  FreezeRun(stage, &minigames, client_root);
}

void SurvivorsMode::FreezeRun(SurvivorsStage& stage, MinigameStage* minigames,
                              const std::filesystem::path& client_root) {
  // Une partie coop ne compte pour aucun trophée.
  if (!trophies_.Freeze(stage, stage.board().coop ? nullptr : minigames, client_root)) return;
  run_saved_ = true;
  // UNE PARTIE CLASSÉE FIGÉE N'ENVOIE RIEN : la liaison qui la porte est perdue.
  if (context_.ranking != nullptr) context_.ranking->FollowEnded(stage, true);
  // TOUTE PARTIE INTERROMPUE CRÉDITE LE CARNET (Q10), avec sa fin écrite. Une
  // copie d'un carnet abîmé se dira au départ suivant.
  book_.Credit(stage, Rules(), client_root);
  desk_.Reload(client_root, {});
}

}  // namespace moonclient
