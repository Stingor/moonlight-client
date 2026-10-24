#include "game/minigame_stage.h"

#include <algorithm>
#include <random>
#include <utility>

#include "game/skin_image_of.h"

namespace moonclient {
namespace {

// LES MONSTRES DES GEMMES, un par espèce et dans l'ordre des couleurs de la
// fenêtre : Poring, Drops, Metaling, Poporing, Marin, Deviling.
constexpr std::array<std::uint32_t, ro::ui::kRojeweledKinds> kGemClasses = {1002, 1113, 1613,
                                                                            1031, 1242, 1582};
// La bille de Roggle est un Poring.
constexpr std::uint32_t kBallClass = 1002;
constexpr std::size_t kBallSlot = ro::ui::kRojeweledKinds;
// Les gemmes des plots : Red Gemstone à effacer, Blue Gemstone en bonus.
constexpr std::uint32_t kOrangePegItem = 716;
constexpr std::uint32_t kBluePegItem = 717;

// Le côté du cadre où se compose un monstre, en pixels : une case de grille
// moins ses marges, la bille un peu plus grande que son contact.
constexpr int kGemBoxPx = 36;
constexpr int kBallBoxPx = 26;

// L'avis d'un trophée : plein, puis il s'efface.
constexpr std::uint32_t kToastShownMs = 2500;
constexpr std::uint32_t kToastFadeMs = 500;

// Les sons, sous le dossier des sons du jeu.
constexpr const char* kBurstSound = "_stone_explosion.wav";
constexpr const char* kOrangeHitSound = "effect\\EF_hit2.wav";
constexpr const char* kBlueHitSound = "effect\\EF_hit4.wav";
constexpr const char* kRewardSound = "_heal_effect.wav";

[[nodiscard]] InterfaceSoundFile SoundOf(const char* path) {
  return InterfaceSoundFile{.place = InterfaceSoundPlace::kSoundArchive, .path = path};
}

[[nodiscard]] ro::ui::RogglePhase PhaseOf(ro::sim::PegPhase phase) {
  switch (phase) {
    case ro::sim::PegPhase::kAiming:
      return ro::ui::RogglePhase::kAiming;
    case ro::sim::PegPhase::kFlying:
      return ro::ui::RogglePhase::kFlying;
    case ro::sim::PegPhase::kWon:
      return ro::ui::RogglePhase::kWon;
    case ro::sim::PegPhase::kLost:
      return ro::ui::RogglePhase::kLost;
  }
  return ro::ui::RogglePhase::kAiming;
}

}  // namespace

void DrawMinigameWindows(MinigameWindows& windows, const ro::ui::SkinArt& art,
                         ro::ui::EscapeStack& escape) {
  if (!windows.composed) {
    return;
  }
  escape.Register(ro::ui::kMinigamesId, &windows.hub.open);
  escape.Register(ro::ui::kRojeweledId, &windows.rojeweled.open);
  escape.Register(ro::ui::kRoggleId, &windows.roggle.open);
  windows.hub_answer = ro::ui::DrawMinigamesWindow(windows.hub, windows.hub_view, art);
  windows.rojeweled_answer =
      ro::ui::DrawRojeweledWindow(windows.rojeweled, windows.rojeweled_view, art);
  windows.roggle_answer = ro::ui::DrawRoggleWindow(windows.roggle, windows.roggle_view, art);
}

static_assert(ro::ui::kRojeweledSide == ro::sim::kMatch3Side);
static_assert(ro::ui::kRojeweledKinds == ro::sim::kMatch3Kinds);

std::uint32_t WatchedClock::Follow(bool watched, std::uint32_t now_ms) {
  if (!watched) {
    last_ms_.reset();
    return 0;
  }
  const std::uint32_t step_ms = std::min(now_ms - last_ms_.value_or(now_ms), kMostWatchedStepMs);
  last_ms_ = now_ms;
  watched_ms_ += step_ms;
  return step_ms;
}

void MinigameStage::Advance(MinigameWindows& windows, const MinigamePieces& pieces,
                            const std::filesystem::path& client_root, std::uint32_t now_ms) {
  if (!seeded_) {
    std::random_device entropy;
    rojeweled_ = ro::sim::Match3Game(entropy());
    roggle_ = ro::sim::PegGame(entropy());
    seeded_ = true;
  }
  // ⚠ LES RÉPONSES AVANT LES HORLOGES : un clic date de l'image qui l'a reçu.
  ApplyAnswers(windows);
  rojeweled_clock_.Follow(windows.rojeweled.open, now_ms);
  const std::uint32_t roggle_step_ms = roggle_clock_.Follow(windows.roggle.open, now_ms);

  const bool was_composed = windows.composed;
  windows.composed = windows.hub.open || windows.rojeweled.open || windows.roggle.open;
  if (!windows.composed) {
    if (was_composed && record_unsaved_) {
      Save(client_root);
    }
    return;
  }
  const ro::sim::MinigameRecord before = Record(client_root);
  if (windows.rojeweled.open) {
    PlayRojeweled();
    const std::vector<ro::sim::MinigameTrophy> fresh = RecordRojeweled(record_->record, rojeweled_);
    toasts_.insert(toasts_.end(), fresh.begin(), fresh.end());
  }
  if (windows.roggle.open) {
    PlayRoggle(roggle_step_ms);
    const std::vector<ro::sim::MinigameTrophy> fresh = RecordRoggle(record_->record, roggle_);
    toasts_.insert(toasts_.end(), fresh.begin(), fresh.end());
  }
  if (record_->record != before) {
    Save(client_root);
  }
  ComposeHub(windows);
  ComposeRojeweled(windows, pieces, now_ms);
  ComposeRoggle(windows, pieces, now_ms);
}

void MinigameStage::Leave(const std::filesystem::path& client_root) {
  if (record_unsaved_) {
    Save(client_root);
  }
}

void MinigameStage::Save(const std::filesystem::path& client_root) {
  record_unsaved_ = !SaveMinigameRecord(client_root, *record_);
}

void MinigameStage::ApplyAnswers(MinigameWindows& windows) {
  const ro::ui::MinigamesAsk hub = std::exchange(windows.hub_answer, {});
  windows.rojeweled.open = windows.rojeweled.open || hub.open_rojeweled;
  windows.roggle.open = windows.roggle.open || hub.open_roggle;

  const ro::ui::RojeweledAsk jewels = std::exchange(windows.rojeweled_answer, {});
  if (jewels.new_game) {
    rojeweled_.NewGame(rojeweled_clock_.watched_ms());
  }
  if (jewels.clicked.has_value()) {
    rojeweled_.Click({jewels.clicked->row, jewels.clicked->column}, rojeweled_clock_.watched_ms());
  }

  const ro::ui::RoggleAsk pegs = std::exchange(windows.roggle_answer, {});
  if (pegs.new_game) {
    roggle_.NewGame();
  }
  if (pegs.aim.has_value()) {
    roggle_.Aim({pegs.aim->x, pegs.aim->y});
  }
  if (pegs.launch) {
    roggle_.Launch();
  }
}

void MinigameStage::PlayRojeweled() {
  if (!rojeweled_.started()) {
    rojeweled_.NewGame(rojeweled_clock_.watched_ms());
  }
  rojeweled_.Advance(rojeweled_clock_.watched_ms());
  const ro::sim::Match3News news = rojeweled_.TakeNews();
  for (int burst = 0; burst < news.bursts; ++burst) {
    sounds_.push_back(SoundOf(kBurstSound));
  }
  if (news.finished) {
    sounds_.push_back(SoundOf(kRewardSound));
  }
}

void MinigameStage::PlayRoggle(std::uint32_t step_ms) {
  if (!roggle_.started()) {
    roggle_.NewGame();
  }
  constexpr float kMsPerSecond = 1000.0F;
  roggle_.Advance(static_cast<float>(step_ms) / kMsPerSecond);
  const ro::sim::PegNews news = roggle_.TakeNews();
  if (news.orange_hits > 0) {
    sounds_.push_back(SoundOf(kOrangeHitSound));
  }
  if (news.blue_hits > 0) {
    sounds_.push_back(SoundOf(kBlueHitSound));
  }
  if (news.caught || (news.finished && roggle_.phase() == ro::sim::PegPhase::kWon)) {
    sounds_.push_back(SoundOf(kRewardSound));
  }
}

std::vector<InterfaceSoundFile> MinigameStage::TakeSounds() { return std::exchange(sounds_, {}); }

ro::sim::MinigameRecord& MinigameStage::Record(const std::filesystem::path& client_root) {
  if (!record_.has_value()) {
    record_ = LoadMinigameRecord(client_root);
  }
  return record_->record;
}

void MinigameStage::Announce(ro::net::Connection& link, bool verified, std::uint32_t map_entries,
                             const std::filesystem::path& client_root) {
  // ⚠ L'ENREGISTREMENT SE LIT DÈS LA POIGNÉE DE MAIN, fenêtres fermées : les
  // trophées d'une session précédente sont à redire.
  announcer_.Pump(link, verified, map_entries,
                  verified ? Record(client_root) : ro::sim::MinigameRecord{});
}

const char* MinigameStage::Toast(std::uint32_t now_ms, float& alpha) {
  alpha = 0.0F;
  if (toasts_.empty()) {
    return nullptr;
  }
  if (!toast_started_ms_.has_value()) {
    toast_started_ms_ = now_ms;
    sounds_.push_back(SoundOf(kRewardSound));
  }
  const std::uint32_t shown_ms = now_ms - *toast_started_ms_;
  if (shown_ms >= kToastShownMs + kToastFadeMs) {
    toasts_.pop_front();
    toast_started_ms_.reset();
    return Toast(now_ms, alpha);
  }
  alpha = shown_ms <= kToastShownMs ? 1.0F
                                    : 1.0F - static_cast<float>(shown_ms - kToastShownMs) /
                                                 static_cast<float>(kToastFadeMs);
  return ro::sim::MinigameTrophies()[static_cast<std::size_t>(toasts_.front())].name;
}

ro::ui::SkinImage MinigameStage::SpriteOf(std::size_t slot, std::uint32_t sprite_class, int box_px,
                                          ro::render::ActorWardrobe* wardrobe,
                                          std::uint32_t now_ms) {
  if (wardrobe == nullptr) {
    return {};
  }
  std::optional<MonsterSprite>& sprite = sprites_[slot];
  if (!sprite.has_value()) {
    sprite.emplace(*wardrobe);
  }
  const MonsterSprite::Shown shown =
      sprite->Compose(MonsterSprite::Framing{.sprite_class = sprite_class,
                                             .box_width = box_px,
                                             .box_height = box_px,
                                             .fit = ro::render::VignetteFit::kCenteredShrinkOnly},
                      now_ms);
  // Une gemme ne parle pas : les sons des images se jettent.
  static_cast<void>(sprite->TakeSounds());
  return shown.missing ? ro::ui::SkinImage{} : shown.image;
}

void MinigameStage::ComposeHub(MinigameWindows& windows) {
  trophy_rows_.clear();
  for (const ro::sim::MinigameTrophyText& text : ro::sim::MinigameTrophies()) {
    trophy_rows_.push_back(ro::ui::MinigameTrophyRow{.name = text.name,
                                                     .description = text.description,
                                                     .unlocked = record_->record.Has(text.trophy)});
  }
  windows.hub_view = ro::ui::MinigamesView{.rojeweled_best = record_->record.rojeweled_best,
                                           .roggle_best = record_->record.roggle_best,
                                           .trophies = trophy_rows_};
}

void MinigameStage::ComposeRojeweled(MinigameWindows& windows, const MinigamePieces& pieces,
                                     std::uint32_t now_ms) {
  if (!windows.rojeweled.open) {
    return;
  }
  ro::ui::RojeweledView& view = windows.rojeweled_view;
  const std::uint32_t watched_ms = rojeweled_clock_.watched_ms();
  for (int index = 0; index < ro::ui::kRojeweledCells; ++index) {
    const ro::sim::Match3Cell cell{index / ro::ui::kRojeweledSide, index % ro::ui::kRojeweledSide};
    const ro::sim::Match3Game::Offset offset = rojeweled_.OffsetAt(cell, watched_ms);
    view.gems[static_cast<std::size_t>(index)] =
        ro::ui::RojeweledGem{.kind = rojeweled_.KindAt(cell),
                             .offset_rows = offset.rows,
                             .offset_columns = offset.columns};
  }
  bursts_.clear();
  for (const ro::sim::Match3Burst& burst : rojeweled_.bursts()) {
    bursts_.push_back(ro::ui::RojeweledBurst{
        .cell = {burst.cell.row, burst.cell.column},
        .kind = burst.kind,
        .progress = std::min(1.0F, static_cast<float>(watched_ms - burst.started_ms) /
                                       static_cast<float>(ro::sim::kMatch3BurstMs))});
  }
  view.bursts = bursts_;
  view.selected.reset();
  if (const std::optional<ro::sim::Match3Cell> selected = rojeweled_.selected()) {
    view.selected = ro::ui::RojeweledCell{selected->row, selected->column};
  }
  view.hint.reset();
  if (const auto hint = rojeweled_.Hint(watched_ms)) {
    view.hint = std::array<ro::ui::RojeweledCell, 2>{
        ro::ui::RojeweledCell{(*hint)[0].row, (*hint)[0].column},
        ro::ui::RojeweledCell{(*hint)[1].row, (*hint)[1].column}};
  }
  view.score = rojeweled_.score();
  view.moves_left = rojeweled_.moves_left();
  view.combo = rojeweled_.combo();
  view.best_score = record_->record.rojeweled_best;
  view.over = rojeweled_.phase() == ro::sim::Match3Phase::kOver;
  for (std::size_t kind = 0; kind < kGemClasses.size(); ++kind) {
    view.art[kind] = SpriteOf(kind, kGemClasses[kind], kGemBoxPx, pieces.wardrobe, now_ms);
  }
  view.toast = Toast(now_ms, view.toast_alpha);
}

void MinigameStage::ComposeRoggle(MinigameWindows& windows, const MinigamePieces& pieces,
                                  std::uint32_t now_ms) {
  if (!windows.roggle.open) {
    return;
  }
  ro::ui::RoggleView& view = windows.roggle_view;
  view.board = ro::ui::RoggleBoard{.width = ro::sim::kPegBoardWidth,
                                   .height = ro::sim::kPegBoardHeight,
                                   .ball_radius = ro::sim::kPegBallRadius,
                                   .peg_radius = ro::sim::kPegRadius,
                                   .cannon = {ro::sim::kPegCannonX, ro::sim::kPegCannonY},
                                   .bucket_width = ro::sim::kPegBucketWidth,
                                   .bucket_height = ro::sim::kPegBucketHeight,
                                   .bucket_y = ro::sim::kPegBucketY};
  pegs_.clear();
  for (const ro::sim::Peg& peg : roggle_.pegs()) {
    if (!peg.removed) {
      pegs_.push_back(
          ro::ui::RogglePeg{.at = {peg.x, peg.y}, .orange = peg.orange, .lit = peg.lit});
    }
  }
  view.pegs = pegs_;
  guide_.clear();
  for (const ro::sim::PegPoint point : roggle_.AimGuide()) {
    guide_.push_back({point.x, point.y});
  }
  view.guide = guide_;
  view.phase = PhaseOf(roggle_.phase());
  view.ball = {roggle_.ball().x, roggle_.ball().y};
  view.bucket_x = roggle_.bucket_x();
  view.aim = {roggle_.aim().x, roggle_.aim().y};
  view.balls_left = roggle_.balls_left();
  view.score = roggle_.score();
  view.orange_left = roggle_.orange_left();
  view.multiplier = roggle_.multiplier();
  view.best_score = record_->record.roggle_best;
  view.orange_art = ItemIconImage(pieces.item_icons, kOrangePegItem);
  view.blue_art = ItemIconImage(pieces.item_icons, kBluePegItem);
  view.ball_art = SpriteOf(kBallSlot, kBallClass, kBallBoxPx, pieces.wardrobe, now_ms);
  // ⚠ L'AVIS N'EST MONTRÉ QUE PAR UN JEU : Rojeweled le prend s'il est ouvert.
  view.toast = windows.rojeweled.open ? nullptr : Toast(now_ms, view.toast_alpha);
}

}  // namespace moonclient
