#include "game/survivors_mode.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/formats/gat.h"
#include "ro/net/action.h"
#include "ro/render/attack_sound.h"
#include "ro/render/skill_sound_table.h"
#include "ro/sim/level_up_effect.h"
#include "ro/sim/survivors_journal.h"

#include "game/survivors_mode_slot.h"
#include "game/survivors_ranking_desk.h"
#include "survivors/arena_card_cues.h"
#include "survivors/survivors_door.h"
#include "survivors/survivors_file.h"
#include "survivors/survivors_looks.h"
#include "survivors/survivors_screen_look.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

using Clock = std::chrono::steady_clock;

// Le premier bolt présélectionné tant que le joueur n'en a choisi aucun
// (D2) : Fire Bolt.
constexpr std::uint16_t kFireBoltSkill = 19;

// LE HÉROS SANS PERSONNAGE NOMMÉ, faute de mieux : un homme à la première
// coiffure, comme le banc.
constexpr std::uint8_t kFallbackHeroSex = 1;
constexpr std::uint16_t kFallbackHeroHair = 1;

// Le bruit d'une créature qui frappe le héros, et le gain de tous les bruits
// de l'arène, au centre.
constexpr std::string_view kHeroHurtSound = ro::render::kEnemyHitSounds[0];
constexpr float kArenaSoundGain = 1.0F;
constexpr float kArenaSoundPan = 0.0F;

[[nodiscard]] double MsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

[[nodiscard]] const ro::sim::SurvivorsRules& Rules() { return ro::sim::ShippedSurvivorsRules(); }

// L'écran tient une partie qui court ou qui attend le joueur : le monde réel
// la surveille.
[[nodiscard]] bool Watched(SurvivorsScreen screen) noexcept {
  switch (screen) {
    case SurvivorsScreen::kArriving:
    case SurvivorsScreen::kPlaying:
    case SurvivorsScreen::kPaused:
    case SurvivorsScreen::kChoosing:
    case SurvivorsScreen::kChest:
      return true;
    case SurvivorsScreen::kClosed:
    case SurvivorsScreen::kPreparing:
    case SurvivorsScreen::kLoading:
    case SurvivorsScreen::kDying:
    case SurvivorsScreen::kWon:
    case SurvivorsScreen::kSummary:
      return false;
  }
  return false;
}

[[nodiscard]] bool Sitting(const ro::sim::World& world) {
  const ro::sim::Actor* player = world.player();
  return player != nullptr && player->posture == ro::sim::Posture::kSitting;
}

// Le sol praticable de l'arène, lu dans l'installation sans monter la carte :
// la partie naît avant l'arène, derrière l'écran de chargement.
[[nodiscard]] ro::core::Result<ro::sim::WalkMap> ReadArenaWalkMap(ro::vfs::Vfs& vfs) {
  const ro::sim::SurvivorsRules& rules = Rules();
  if (rules.arenas.empty()) {
    return ro::core::Fail(ro::core::ErrorCode::kNotFound, "aucune arène dans les règles");
  }
  auto bytes =
      vfs.Read(vfs.res_names().MapFilePath(std::string(rules.arenas.front().map_name) + ".gat"));
  if (!bytes) return std::unexpected(bytes.error());
  auto gat = ro::formats::GatMap::Parse(*bytes);
  if (!gat) return std::unexpected(gat.error());
  return ro::sim::WalkMap::FromGat(*gat);
}

[[nodiscard]] std::optional<ro::sim::SurvivorsAnswer> AnswerOf(
    const std::optional<ro::ui::SurvivorsOfferAsk>& asked) {
  if (!asked.has_value()) return std::nullopt;
  ro::sim::SurvivorsAnswer answer{.offer_index = asked->card_index};
  switch (asked->verb) {
    case ro::ui::SurvivorsOfferVerb::kPick:
      answer.kind = ro::sim::AnswerKind::kPick;
      break;
    case ro::ui::SurvivorsOfferVerb::kReroll:
      answer.kind = ro::sim::AnswerKind::kReroll;
      break;
    case ro::ui::SurvivorsOfferVerb::kSkip:
      answer.kind = ro::sim::AnswerKind::kSkip;
      break;
    case ro::ui::SurvivorsOfferVerb::kBanish:
      answer.kind = ro::sim::AnswerKind::kBanish;
      break;
  }
  return answer;
}

[[nodiscard]] ArenaEffectSwitches SwitchesOf(const SurvivorsOptions& options) {
  ArenaEffectSwitches switches;
  switches.reduced = options.reduced_effects;
  return switches;
}

}  // namespace

SurvivorsModeSlot::SurvivorsModeSlot() : mode_(std::make_unique<SurvivorsMode>()) {}

SurvivorsModeSlot::~SurvivorsModeSlot() = default;

std::uint16_t SurvivorsMode::ChosenBolt() const {
  const ro::sim::SurvivorsRules& rules = Rules();
  if (rules.classes.empty() || rules.classes.front().starter_skills.empty()) return 0;
  const std::span<const std::uint16_t> bolts = rules.classes.front().starter_skills;
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
  // CE QUE LE CLASSEMENT A À DIRE AU JOUEUR (Q3, Q5, Q6), partie ou non.
  if (context.ranking != nullptr) SayRankingNotices(*context.ranking, stage, context.say);
  if (std::optional<SurvivorsEnding> ending =
          hub_door_.EndingToShow(windows.hub.open, stage.screen(), context.client_root)) {
    entry_music_volume_ = context.audio->music_volume();
    stage.ShowEnding(std::move(*ending));
    clock_.Follow(false, context.now_ms);
  }
  if (stage.owns_screen()) {
    windows.survivors_prepare_composed = false;
    return;
  }
  if (!windows.survivors_prepare.open) {
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
  views_.ComposePrepare(windows.survivors_prepare_view, Rules(), ChosenBolt(),
                        refusal.has_value() ? RefusalText(*refusal) : nullptr, key_labels_,
                        context.tables);
  views_.ComposeMeta(windows.survivors_prepare_view, windows.survivors_meta, desk_, book_, Rules(),
                     context.tables, context.ranking);
  windows.survivors_prepare_composed = true;
  if (asked.start && !refusal.has_value()) Enter(stage, context, ChosenBolt());
}

void SurvivorsMode::Enter(SurvivorsStage& stage, const SurvivorsContext& context,
                          std::uint16_t first_skill_id) {
  if (context.vfs == nullptr || context.wardrobe == nullptr) {
    context.say(ro::core::Tr("L'arène des Survivants de Midgard ne peut pas se monter."));
    ro::core::LogWarning("survivors : ni archives ni garde-robe, la partie ne commence pas");
    return;
  }
  auto walk = arena_.has_value() ? arena_->WalkMapOf() : ReadArenaWalkMap(*context.vfs);
  if (!walk) {
    context.say(ro::core::Tr("L'arène des Survivants de Midgard ne peut pas se monter."));
    ro::core::LogWarning("survivors : sol de l'arène illisible — {}",
                         ro::core::Describe(walk.error()));
    return;
  }
  std::random_device entropy;
  const std::uint64_t seed = (std::uint64_t{entropy()} << 32U) | entropy();
  // UNE PARTIE CLASSÉE OU UN DÉFI DEMANDE SON JETON (Q2, Q3) : l'écran de
  // chargement couvre l'attente, et `Frame` refait le départ qu'elle règle.
  const SurvivorsBookMode::Begun book_start =
      context.ranking != nullptr
          ? context.ranking->Launch(book_, Rules(), context.client_root, seed, first_skill_id)
      : book_.chosen() == SurvivorsBoardKind::kRanked
          ? book_.StartAs(SurvivorsBoardKind::kFree, Rules(), context.client_root, seed,
                          first_skill_id)
          : book_.StartOf(Rules(), context.client_root, seed, first_skill_id);
  TellDamagedCopy(book_.TakeDamagedCopy(), context.say);
  if (auto begun = stage.Begin(Rules(), std::move(*walk), book_start.start, book_start.board);
      !begun) {
    context.say(ro::core::Tr("L'arène des Survivants de Midgard ne peut pas se monter."));
    ro::core::LogWarning("survivors : la partie refuse de commencer — {}",
                         ro::core::Describe(begun.error()));
    return;
  }
  // « Rejouer » garde le personnage assis par nous : rien à redemander.
  if (!posture_.seated_by_us() && posture_.SitOnEntry(Sitting(*context.world)) &&
      context.link->is_open()) {
    if (auto sat = ro::net::RequestSit(*context.link, true); !sat) {
      ro::core::LogWarning("survivors : s'asseoir — {}", ro::core::Describe(sat.error()));
    }
  }
  context.minigames->survivors_prepare.open = false;
  context.minigames->survivors_meta.CloseAll();
  entry_music_volume_ = context.audio->music_volume();
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

bool SurvivorsMode::Mount(SurvivorsStage& stage, const SurvivorsContext& context) {
  const ro::sim::SurvivorsRules& rules = Rules();
  if (!arena_.has_value()) {
    arena_.emplace(*context.vfs, *context.wardrobe, rules);
    if (auto opened = arena_->Open(*context.vfs, rules.arenas.front()); !opened) {
      ro::core::LogWarning("survivors : {}", ro::core::Describe(opened.error()));
      context.say(ro::core::Tr("L'arène des Survivants de Midgard ne peut pas se monter."));
      arena_.reset();
      Exit(stage, context);
      return false;
    }
  }
  arena_->Restart();
  arena_->SetSwitches(SwitchesOf(options_));
  arena_->SetHordeSunShadows(options_.horde_sun_shadows);
  arena_->SetQuake(context.quake);
  hero_evolved_.reset();
  // LA CHAUFFE : chaque espèce de la première vague, et les bolts de départ.
  const ro::sim::SurvivorsField& field = stage.play()->run().field();
  std::vector<std::uint16_t> skills;
  if (!rules.classes.empty()) {
    const std::span<const std::uint16_t> starters = rules.classes.front().starter_skills;
    skills.assign(starters.begin(), starters.end());
  }
  const std::span<const std::uint16_t> species =
      rules.waves.empty() ? std::span<const std::uint16_t>() : rules.waves.front().foe_ids;
  arena_->Warm(species, skills,
               CellOfField(glm::vec2(static_cast<float>(field.hero.position.x),
                                     static_cast<float>(field.hero.position.y))),
               context.width, context.height, context.now_ms);
  stage.Loaded();
  sign_.Enter(context.map_names != nullptr
                  ? context.map_names->SignOf(MapSignFileOf(rules.arenas.front().map_name))
                  : nullptr,
              context.sign_mode, context.now_ms);
  music_.Reset();
  music_volume_set_.reset();
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
  auto walk = arena_.has_value() ? arena_->WalkMapOf() : ReadArenaWalkMap(*context.vfs);
  ro::core::Result<void> begun =
      walk.has_value() ? stage.Begin(Rules(), std::move(*walk), again->start, again->board)
                       : ro::core::Result<void>(std::unexpected(walk.error()));
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
  // LA MUSIQUE REFAIT SUIVRE LA CARTE DU PERSONNAGE, au volume des options.
  context.music->Forget();
  context.audio->SetMusicVolume(entry_music_volume_);
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
}

void SurvivorsMode::Freeze(SurvivorsStage& stage, MinigameStage& minigames,
                           const std::filesystem::path& client_root) {
  FreezeRun(stage, &minigames, client_root);
}

void SurvivorsMode::FreezeRun(SurvivorsStage& stage, MinigameStage* minigames,
                              const std::filesystem::path& client_root) {
  if (!trophies_.Freeze(stage, minigames, client_root)) return;
  run_saved_ = true;
  // UNE PARTIE CLASSÉE FIGÉE N'ENVOIE RIEN : la liaison qui la porte est perdue.
  if (context_.ranking != nullptr) context_.ranking->FollowEnded(stage, true);
  // TOUTE PARTIE INTERROMPUE CRÉDITE LE CARNET (Q10), avec sa fin écrite. Une
  // copie d'un carnet abîmé se dira au départ suivant.
  book_.Credit(stage, Rules(), client_root);
  desk_.Reload(client_root, {});
}

SurvivorsIntent SurvivorsMode::IntentOf(const SurvivorsStage& stage,
                                        const SurvivorsContext& context,
                                        const SurvivorsGestures& gestures,
                                        const RealWorldReport& report) {
  SurvivorsIntent intent;
  MouseAim aim;
  const ro::sim::SurvivorsPlay* play = stage.play();
  if (route_.mouse_held() && play != nullptr && arena_.has_value()) {
    aim.hero = play->run().field().hero.position;
    if (const std::optional<ro::sim::FieldPoint> target =
            arena_->FieldPointUnder(glm::vec2(route_.mouse_x(), route_.mouse_y()), aim.hero,
                                    context.width, context.height)) {
      aim.held = true;
      aim.target = *target;
    }
  }
  intent.heading = HeadingOf(route_.keys(), aim);
  intent.escape = gestures.escape;
  intent.focus_lost = gestures.focus_lost;
  intent.world_alert = report.pauses();
  intent.resume = ask_.pause.resume;
  intent.abandon = ask_.pause.abandon;
  // Entrée et Espace valent « Continuer » sur le coffre seul : ailleurs,
  // l'écran de fin relancerait une partie sans qu'on l'ait demandé.
  intent.proceed = ask_.chest_proceed || ask_.summary.proceed ||
                   (gestures.confirm && stage.screen() == SurvivorsScreen::kChest);
  intent.answer = AnswerOf(ask_.offer);
  return intent;
}

void SurvivorsMode::ApplyOptions(const ro::ui::SurvivorsPauseAsk& asked,
                                 const SurvivorsContext& context) {
  const SurvivorsOptions before = options_;
  if (asked.reduced_effects.has_value()) options_.reduced_effects = *asked.reduced_effects;
  if (asked.show_measures.has_value()) options_.show_measures = *asked.show_measures;
  if (asked.horde_sun_shadows.has_value()) options_.horde_sun_shadows = *asked.horde_sun_shadows;
  if (options_ == before) return;
  if (arena_.has_value()) {
    arena_->SetSwitches(SwitchesOf(options_));
    arena_->SetHordeSunShadows(options_.horde_sun_shadows);
  }
  SaveOptions(before, context);
}

void SurvivorsMode::SaveOptions(const SurvivorsOptions& before, const SurvivorsContext& context) {
  const ro::core::Result<SurvivorsSaveRead> saved =
      SaveSurvivorsOptions(context.client_root, before, options_);
  if (!saved) {
    ro::core::LogWarning("survivors : réglages non écrits — {}", ro::core::Describe(saved.error()));
    return;
  }
  TellDamagedCopy(saved->damaged_copy, context.say);
}

SurvivorsPainted SurvivorsMode::Frame(SurvivorsStage& stage, const SurvivorsContext& context) {
  loading_now_ = false;
  // LA LIAISON PERDUE REND LA BOÎTE DE LIAISON PERDUE (Q3) : la partie se
  // fige et s'écrit, le mode sort, et le bilan attend la salle rouverte.
  const LinkLossSteps lost = LinkLossStepsOf(stage.screen(), context.aside.link_open);
  if (lost.freeze) FreezeRun(stage, context.minigame_stage, context.client_root);
  if (lost.exit) {
    Exit(stage, context);
    return SurvivorsPainted{};
  }
  if (stage.screen() == SurvivorsScreen::kLoading) {
    // LE DÉPART QUI ATTEND SON JETON reste derrière l'écran de chargement (Q3),
    // puis se refait sur la graine du serveur, ou libre, s'il le faut.
    if (context.ranking != nullptr && context.ranking->holding()) {
      loading_shown_ = true;
      loading_now_ = true;
      return SurvivorsPainted{.loading = true};
    }
    if (!Resettle(stage, context)) return SurvivorsPainted{};
    // L'ÉCRAN DE CHARGEMENT D'ABORD, présenté une fois, quand l'arène est à
    // monter : le montage, synchrone, fige l'image suivante derrière lui. Une
    // arène gardée pour « Rejouer » repart sans lui.
    if (!arena_.has_value() && !loading_shown_) {
      loading_shown_ = true;
      loading_now_ = true;
      return SurvivorsPainted{.loading = true};
    }
    if (!Mount(stage, context)) return SurvivorsPainted{};
  }
  const std::uint32_t elapsed_ms = clock_.Follow(true, context.now_ms);

  // LE MONDE RÉEL, image après image, tant qu'une partie court ou attend.
  RealWorldReport report;
  if (Watched(stage.screen())) {
    report = watch_.Observe(
        RealFactsOf(*context.world, context.aside, whispers_.Follow(context.world->chat_lines())));
    if (const std::optional<RealWorldAlert> banner = report.banner()) banner_ = banner;
  }

  const SurvivorsGestures gestures = route_.TakeGestures();
  pressed_card_key_ = gestures.pressed_card_key;
  if (arena_.has_value() && gestures.wheel_notches != 0.0F) arena_->Wheel(gestures.wheel_notches);
  const SurvivorsIntent intent = IntentOf(stage, context, gestures, report);
  ApplyOptions(ask_.pause, context);
  const ro::ui::SurvivorsSummaryAsk summary = ask_.summary;
  // L'ÉGLISE ET LE TABLEAU MONDIAL PAR-DESSUS L'ÉCRAN DE FIN : l'Église
  // achète, le tableau demande ses pages, et Échap ferme celle du dessus.
  SurvivorsMetaWindows& meta = context.minigames->survivors_meta;
  desk_.FollowSummary(ask_, meta, context.client_root, context.say);
  if (context.ranking != nullptr) {
    FollowRankingAsks(*context.ranking, meta.ranking,
                      SurvivorsRankingAsks{.open_on = summary.open_ranking, .window = ask_.ranking},
                      Rules());
  }
  if (gestures.escape) meta.CloseTopOverSummary();
  ask_ = {};
  if (stage.screen() == SurvivorsScreen::kSummary && stage.accepts_input()) {
    // Le monde réel a pu changer pendant le bilan : « Rejouer » rejuge la garde.
    const bool refused =
        summary.replay && Judge(EntryFactsOf(*context.world, context.aside)).has_value();
    switch (SummaryStepOf(summary.back, summary.replay, refused)) {
      case SummaryStep::kStay:
        break;
      case SummaryStep::kBack:
        Exit(stage, context);
        return SurvivorsPainted{};
      case SummaryStep::kBackToPrepare:
        Exit(stage, context);
        context.minigames->survivors_prepare.open = true;
        return SurvivorsPainted{};
      case SummaryStep::kReplay:
        Enter(stage, context, ChosenBolt());
        if (!stage.owns_screen()) return SurvivorsPainted{};
        return Frame(stage, context);
    }
  }

  const Clock::time_point simulated = Clock::now();
  stage.Advance(elapsed_ms, intent);
  measures_.Record(ArenaMeasure::kSimulation, MsSince(simulated));
  FollowEntered(stage, context);

  SurvivorsPainted painted;
  const SurvivorsEnding* ending = stage.ending();
  painted.desaturation = SurvivorsDesaturation(
      stage.screen(),
      ending != nullptr && ending->tally.outcome == ro::sim::SurvivorsOutcome::kFell);
  if (arena_.has_value() && stage.play() != nullptr && SurvivorsArenaShown(stage.screen())) {
    PaintArena(stage, context);
    painted.arena = true;
  }
  if (stage.owns_screen()) {
    views_.NoticeCards(stage.news(), stage.presentation_ms());
    PlaySounds(stage.news(), context);
    FollowMusic(stage, context);
  }
  return painted;
}

void SurvivorsMode::FollowEntered(SurvivorsStage& stage, const SurvivorsContext& context) {
  const std::optional<SurvivorsScreen> entered = stage.entered();
  if (!entered.has_value()) return;
  switch (*entered) {
    case SurvivorsScreen::kChoosing:
      views_.ResetOffer();
      return;
    case SurvivorsScreen::kPaused:
      views_.ResetPause();
      if (stage.pause_cause() != PauseCause::kWorldAlert) banner_.reset();
      return;
    case SurvivorsScreen::kSummary:
      // UNE PARTIE LIBRE COMPTE POUR LES TROPHÉES, quelle que soit son issue.
      trophies_.FollowEntered(stage, context.minigame_stage, context.client_root);
      // UNE PARTIE CLASSÉE FINIE ENVOIE SES COMPOSANTES, une fois.
      if (context.ranking != nullptr) context.ranking->FollowEnded(stage, false);
      // TOUTE PARTIE CRÉDITE LE CARNET (Q10), une fois ; l'écran de fin lit
      // le carnet relu.
      book_.FollowEntered(stage, Rules(), context.client_root);
      TellDamagedCopy(book_.TakeDamagedCopy(), context.say);
      desk_.Reload(context.client_root, context.say);
      if (!run_saved_ && stage.play() != nullptr) {
        if (auto saved = SaveLastRun(context.client_root, stage.play()->journal()); !saved) {
          ro::core::LogWarning("survivors : journal non écrit — {}",
                               ro::core::Describe(saved.error()));
        }
        run_saved_ = true;
        LogMeasures();
      }
      return;
    case SurvivorsScreen::kWon:
      // LA VICTOIRE SONNE COMME UNE MONTÉE DE NIVEAU (Q7).
      if (context.audio->sound_audible()) {
        PlaySound(ro::sim::kBaseLevelUpSound, ArenaSoundRank::kRise, context);
      }
      return;
    case SurvivorsScreen::kClosed:
    case SurvivorsScreen::kPreparing:
    case SurvivorsScreen::kLoading:
    case SurvivorsScreen::kArriving:
    case SurvivorsScreen::kPlaying:
    case SurvivorsScreen::kChest:
    case SurvivorsScreen::kDying:
      return;
  }
}

void SurvivorsMode::PaintArena(const SurvivorsStage& stage, const SurvivorsContext& context) {
  const ro::sim::SurvivorsPlay& play = *stage.play();
  const ro::sim::SurvivorsField& field = play.run().field();
  // LE HÉROS PORTE LE VISAGE DU PERSONNAGE (U7), et le corps de l'évolution
  // dès la première.
  const bool evolved = field.hero.evolutions > 0;
  if (hero_evolved_ != evolved && field.hero_class != nullptr) {
    const ro::sim::Actor* player = context.world->player();
    const ro::render::ActorLook face =
        player != nullptr ? LookOf(*player)
                          : ro::render::ActorLook{
                                .player = true, .sex = kFallbackHeroSex, .hair = kFallbackHeroHair};
    arena_->SetHeroLook(HeroLook(face, *field.hero_class, evolved));
    hero_evolved_ = evolved;
  }
  play.run().Orbs(orbs_);
  const ArenaFrameFacts facts{
      .field = &field,
      .orbs = orbs_,
      .news = &stage.news(),
      .presentation_ms = stage.presentation_ms(),
      .blend_per_mille = stage.screen() == SurvivorsScreen::kPlaying ? play.blend_per_mille() : 0,
      .arrival_ms = stage.arrival_ms(),
      .victory_ms = stage.screen() == SurvivorsScreen::kWon
                        ? std::optional<std::uint64_t>(stage.screen_elapsed_ms())
                        : std::nullopt,
      .viewport_width = context.width,
      .viewport_height = context.height};
  const Clock::time_point composed = Clock::now();
  arena_->Compose(facts, *context.graphics, context.now_ms);
  measures_.Record(ArenaMeasure::kComposition, MsSince(composed));
  const Clock::time_point painted = Clock::now();
  arena_->Paint(facts, *context.graphics, context.now_ms);
  measures_.Record(ArenaMeasure::kScene, MsSince(painted));
  measures_.Record(ArenaMeasure::kEffects, arena_->effects_ms());
}

void SurvivorsMode::PlaySounds(const ro::sim::SurvivorsNews& news,
                               const SurvivorsContext& context) {
  if (!context.audio->sound_audible()) return;
  if (news.hero_hurt > 0) PlaySound(kHeroHurtSound, ArenaSoundRank::kHeroHurt, context);
  if (news.level_ups > 0 || !news.evolutions.empty()) {
    PlaySound(ro::sim::kBaseLevelUpSound, ArenaSoundRank::kRise, context);
  }
  const ArenaCardSounds cards = CardSoundsOf(news);
  PlaySound(cards.drop, ArenaSoundRank::kTreasure, context);
  PlaySound(cards.pickup, ArenaSoundRank::kTreasure, context);
  for (const ro::sim::SurvivorsCast& cast : news.casts) {
    const ro::sim::SurvivorsSkillRule* rule = ro::sim::FindSkill(Rules(), cast.skill_id);
    if (rule == nullptr) continue;
    for (const ro::render::EffectSoundEntry& entry : SkillSoundsOf(SkillLookOf(*rule))) {
      PlaySound(entry.sound, ArenaSoundRank::kSkill, context);
    }
  }
}

void SurvivorsMode::PlaySound(std::string_view name, ArenaSoundRank rank,
                              const SurvivorsContext& context) {
  if (name.empty() || missing_sounds_.contains(name)) return;
  ro::audio::AudioEngine& audio = *context.audio;
  const std::string held(name);
  if (!audio.KnowsSound(held)) {
    auto bytes = context.vfs->Read(std::string(ro::render::kSoundFolder) + held);
    if (!bytes) {
      ro::core::LogWarning("survivors : son {} absent", held);
      missing_sounds_.insert(held);
      return;
    }
    if (auto loaded = audio.LoadSound(held, *bytes); !loaded) {
      ro::core::LogWarning("survivors : son {} illisible — {}", held,
                           ro::core::Describe(loaded.error()));
      missing_sounds_.insert(held);
      return;
    }
  }
  const ArenaSoundVerdict verdict =
      sounds_.Ask(held, rank, audio.SoundLengthMs(held), context.now_ms);
  if (!verdict.silenced.empty()) audio.StopSound(verdict.silenced);
  if (verdict.play) {
    audio.PlaySound(held, kArenaSoundGain, kArenaSoundPan, ro::audio::SoundChannel::kEffects);
  }
}

void SurvivorsMode::FollowMusic(const SurvivorsStage& stage, const SurvivorsContext& context) {
  ro::audio::AudioEngine& audio = *context.audio;
  // UNE PISTE ANNONCÉE SOUS UNE MUSIQUE COUPÉE NE JOUE PAS : rallumée, la piste
  // du moment repart.
  if (!audio.music_audible()) {
    music_muted_ = true;
    return;
  }
  if (music_muted_) {
    music_muted_ = false;
    music_.Reset();
  }
  const ArenaMusicCue cue =
      music_.Advance(ArenaMusicFacts{.presentation_ms = stage.presentation_ms(),
                                     .paused = SurvivorsMusicPaused(stage.screen()),
                                     .player_track = context.music->chosen(),
                                     .music_volume = entry_music_volume_});
  if (!cue.start_track.empty()) context.music->PlayForMode(audio, cue.start_track);
  if (music_volume_set_ != cue.volume) {
    audio.SetMusicVolume(cue.volume);
    music_volume_set_ = cue.volume;
  }
}

void SurvivorsMode::LogMeasures() const {
  static constexpr std::array<std::string_view, kArenaMeasureCount> kNames = {
      "simulation", "composition", "scène", "effets", "interface"};
  for (std::size_t rank = 0; rank < kArenaMeasureCount; ++rank) {
    const MeasureSummary whole = measures_.Whole(static_cast<ArenaMeasure>(rank));
    ro::core::LogInfo("survivors : {} p50 {:.3f} ms, p95 {:.3f} ms, max {:.3f} ms ({} images)",
                      kNames[rank], whole.p50_ms, whole.p95_ms, whole.max_ms, whole.samples);
  }
}

SurvivorsFrameView SurvivorsMode::View(const SurvivorsStage& stage,
                                       const SurvivorsContext& context) {
  const Clock::time_point started = Clock::now();
  sign_view_ = stage.screen() == SurvivorsScreen::kArriving || sign_.showing()
                   ? sign_.Stage(context.images, context.now_ms)
                   : std::nullopt;
  SurvivorsFrameView view = views_.Compose(
      stage, Rules(),
      SurvivorsViewFacts{.arena = arena_.has_value() ? &*arena_ : nullptr,
                         .width = context.width,
                         .height = context.height,
                         .pressed_card_key = pressed_card_key_,
                         .banner = banner_,
                         .unread_whispers = whispers_.received(),
                         .options = options_,
                         .measures = &measures_,
                         .busy_voices = sounds_.busy(context.now_ms),
                         .credit = book_.credit().has_value() ? &*book_.credit() : nullptr,
                         .save = &desk_.save(),
                         .trophies = trophies_.won(),
                         .meta = &context.minigames->survivors_meta,
                         .ranking = context.ranking},
      context.tables);
  if (sign_view_.has_value()) view.map_sign = &*sign_view_;
  measures_.Record(ArenaMeasure::kHud, MsSince(started));
  return view;
}

}  // namespace moonclient
