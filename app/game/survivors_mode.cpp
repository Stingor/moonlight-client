#include "game/survivors_mode.h"

#include <chrono>
#include <cstdint>
#include <string>

#include "ro/core/build_flavor.h"
#include "ro/core/log.h"
#include "ro/sim/level_up_effect.h"
#include "ro/sim/survivors_journal.h"

#include "game/survivors_codex_desk.h"
#include "game/survivors_coop_mode.h"
#include "game/survivors_mode_slot.h"
#include "game/survivors_ranking_desk.h"
#include "options/game_option_ids.h"
#include "survivors/arena_meter.h"
#include "survivors/survivors_door.h"
#include "survivors/survivors_file.h"
#include "survivors/survivors_looks.h"
#include "survivors/survivors_screen_look.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

using Clock = std::chrono::steady_clock;

// LE HÉROS SANS PERSONNAGE NOMMÉ, faute de mieux : un homme à la première
// coiffure, comme le banc.
constexpr std::uint8_t kFallbackHeroSex = 1;
constexpr std::uint16_t kFallbackHeroHair = 1;

[[nodiscard]] double MsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

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
    case ro::ui::SurvivorsOfferVerb::kRaise:
      answer.kind = ro::sim::AnswerKind::kRaise;
      break;
  }
  return answer;
}

}  // namespace

SurvivorsModeSlot::SurvivorsModeSlot() : mode_(std::make_unique<SurvivorsMode>()) {}

SurvivorsModeSlot::~SurvivorsModeSlot() = default;

const ro::sim::SurvivorsRules& SurvivorsMode::Rules() { return ro::sim::ShippedSurvivorsRules(); }

void SurvivorsMode::PoseArenaOptions() {
  ArenaEffectSwitches switches;
  switches.reduced = options_.reduced_effects;
  arena_->SetSwitches(switches);
  arena_->SetHordeSunShadows(options_.horde_sun_shadows);
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
  intent.typing = context.typing && options_.pause_while_typing;
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
  // Hors du client de développement, la case ne paraît pas et ne change rien.
  if (asked.show_measures.has_value() && ro::core::kDevTools) {
    options_.show_measures = *asked.show_measures;
  }
  if (asked.horde_sun_shadows.has_value()) options_.horde_sun_shadows = *asked.horde_sun_shadows;
  if (asked.pause_while_typing.has_value()) {
    options_.pause_while_typing = *asked.pause_while_typing;
  }
  if (asked.presence_shown.has_value()) options_.presence_shown = *asked.presence_shown;
  if (options_ == before) return;
  if (arena_.has_value()) PoseArenaOptions();
  SaveOptions(before, context);
}

void SurvivorsMode::FollowObjective(std::optional<std::uint16_t> wanted,
                                    const SurvivorsContext& context) {
  if (wanted.has_value() && *wanted != options_.objective_skill_id) {
    const SurvivorsOptions before = options_;
    options_.objective_skill_id = *wanted;
    SaveOptions(before, context);
  }
  context.minigames->survivors_meta.objective_skill_id = options_.objective_skill_id;
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
    if (!ArenaShown(stage) && !loading_shown_) {
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
    if (const std::optional<RealWorldAlert> banner = report.banner()) {
      banner_ = banner;
      if (stage.coop()) NoteCoopAlert(context.now_ms);
    }
  }

  SurvivorsGestures gestures = route_.TakeGestures();
  pressed_card_key_ = gestures.pressed_card_key;
  if (gestures.toggle_minimap) minimap_hidden_ = !minimap_hidden_;
  // LE CODEX PAR-DESSUS LA PAUSE prend Échap et la molette avant les options :
  // ouvert, il est le dernier venu, la pause qui les ouvre étant cachée sous lui.
  (void)FollowCodexOverPause(stage.screen(), ask_.pause.open_codex, gestures,
                             context.minigames->survivors_meta.codex);
  FollowObjective(ask_.codex.objective_skill_id, context);
  // LES OPTIONS DU JEU PAR-DESSUS LA PAUSE prennent Échap d'abord, et la
  // molette, qui fait défiler leurs pages et non reculer la caméra.
  bool* options_open = context.options_window != nullptr ? &context.options_window->open : nullptr;
  if (options_open != nullptr) {
    const bool shown = pause_options_.shown(*options_open, stage.screen());
    if (pause_options_.Follow(*options_open, ask_.pause.open_options, gestures.escape,
                              stage.screen())) {
      gestures.escape = false;
    }
    if (shown) gestures.wheel_notches = 0.0F;
  }
  if (arena_.has_value() && gestures.wheel_notches != 0.0F) arena_->Wheel(gestures.wheel_notches);
  const SurvivorsIntent intent = IntentOf(stage, context, gestures, report);
  ApplyOptions(ask_.pause, context);
  // LE VOLUME DE LA MUSIQUE CHANGÉ DANS LES OPTIONS DE LA PAUSE, déjà posé par
  // elles, devient celui de la partie : la pause le baisse, la sortie le rend.
  if (ask_.options.changed == static_cast<int>(GameOption::kMusicVolume)) {
    music_volume_.Follow(context.audio->music_volume());
  }
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
        // « Rejouer » ne vaut que seul : une équipe repasse par son salon.
        if (stage.board().coop) {
          Exit(stage, context);
          context.minigames->survivors_prepare.open = true;
          return SurvivorsPainted{};
        }
        Enter(stage, context, ChosenBolt());
        if (!stage.owns_screen()) return SurvivorsPainted{};
        return Frame(stage, context);
    }
  }

  const Clock::time_point simulated = Clock::now();
  stage.Advance(elapsed_ms, intent);
  measures_.Record(ArenaMeasure::kSimulation, MsSince(simulated));
  FollowCoopFrame(stage, context);
  if (options_open != nullptr) pause_options_.Settle(*options_open, stage.screen());
  FollowEntered(stage, context);

  SurvivorsPainted painted;
  const SurvivorsEnding* ending = stage.ending();
  // LA PAUSE D'UNE COOP N'EST QU'UN MENU : le monde continue, et se dessine comme tel.
  const bool coop_menu = stage.coop() && stage.screen() == SurvivorsScreen::kPaused;
  painted.desaturation = SurvivorsDesaturation(
      coop_menu ? SurvivorsScreen::kPlaying : stage.screen(),
      ending != nullptr && ending->tally.outcome == ro::sim::SurvivorsOutcome::kFell);
  if (arena_.has_value() && stage.play() != nullptr && SurvivorsArenaShown(stage.screen())) {
    PaintArena(stage, context);
    painted.arena = true;
  }
  if (stage.owns_screen()) {
    views_.NoticeFinds(stage.news(), local_hero_, stage.presentation_ms());
    if (stage.play() != nullptr && context.damage_meter != nullptr) {
      const ro::vfs::MonsterList* monsters = context.tables.monsters;
      RecordArenaBlows(
          stage.news(), stage.play()->run().field(),
          [monsters](std::uint16_t foe_id) {
            const ro::vfs::MonsterListEntry* entry =
                monsters != nullptr ? monsters->Find(foe_id) : nullptr;
            return entry != nullptr ? entry->name : "#" + std::to_string(foe_id);
          },
          *context.damage_meter, context.now_ms);
    }
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
      // UNE PARTIE LIBRE COMPTE POUR LES TROPHÉES, quelle que soit son issue, sauf en
      // coop, hors classement elle aussi.
      if (!stage.board().coop) {
        trophies_.FollowEntered(stage, context.minigame_stage, context.client_root);
      }
      // UNE PARTIE CLASSÉE FINIE ENVOIE SES COMPOSANTES, une fois.
      if (context.ranking != nullptr && !stage.board().coop) {
        context.ranking->FollowEnded(stage, false);
      }
      // TOUTE PARTIE CRÉDITE LE CARNET (Q10), une fois ; l'écran de fin lit
      // le carnet relu.
      book_.FollowEntered(stage, Rules(), context.client_root);
      TellDamagedCopy(book_.TakeDamagedCopy(), context.say);
      desk_.Reload(context.client_root, context.say);
      if (stage.board().coop) run_saved_ = true;
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
  // LE HÉROS PORTE LE VISAGE DU PERSONNAGE (U7), et le corps de son métier :
  // évolué dès la première évolution, monté sur sa monture.
  const std::uint16_t hero_job =
      field.hero_class != nullptr ? ro::sim::HeroJobOf(*field.hero_class, field.hero) : 0;
  if (hero_job_ != hero_job && field.hero_class != nullptr) {
    const ro::sim::Actor* player = context.world->player();
    const ro::render::ActorLook face =
        player != nullptr ? LookOf(*player)
                          : ro::render::ActorLook{
                                .player = true, .sex = kFallbackHeroSex, .hair = kFallbackHeroHair};
    arena_->SetHeroLook(HeroLook(face, *field.hero_class, hero_job));
    hero_job_ = hero_job;
  }
  PoseAllyLooks(stage);
  play.run().Orbs(orbs_);
  const ArenaFrameFacts facts{
      .field = &field,
      .orbs = orbs_,
      .news = &stage.news(),
      .presentation_ms = stage.presentation_ms(),
      .blend_per_mille = stage.screen() == SurvivorsScreen::kPlaying ||
                                 (stage.coop() && stage.screen() == SurvivorsScreen::kPaused)
                             ? play.blend_per_mille()
                             : 0,
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
                         .team_notice = TeamNotice(stage, context.now_ms),
                         .unread_whispers = whispers_.received(),
                         .minimap_shown = !minimap_hidden_ && !stage.board().without_minimap,
                         .options = options_,
                         .measures = &measures_,
                         .busy_voices = sounds_.busy(context.now_ms),
                         .credit = book_.credit().has_value() ? &*book_.credit() : nullptr,
                         .save = &desk_.save(),
                         .trophies = trophies_.won(),
                         .meta = &context.minigames->survivors_meta,
                         .ranking = context.ranking,
                         .coop_names = context.coop != nullptr ? context.coop->player_names()
                                                               : std::span<const std::string>{},
                         .now_ms = context.now_ms},
      context.tables);
  if (sign_view_.has_value()) view.map_sign = &*sign_view_;
  if (context.options_window != nullptr &&
      pause_options_.shown(context.options_window->open, stage.screen())) {
    view.options_state = context.options_window;
    view.options = context.option_sections;
  }
  measures_.Record(ArenaMeasure::kHud, MsSince(started));
  return view;
}

}  // namespace moonclient
