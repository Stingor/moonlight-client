// ROGGLE DANS L'ÉTAGE DES MINI-JEUX : ce que la fenêtre demande, la partie qui
// avance et ce qu'elle fait entendre, la vue composée. Voir les ADR 0139 et
// 0140.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "game/minigame_stage.h"
#include "game/skin_image_of.h"

namespace moonclient {
namespace {

// La bille de Roggle est un Poring, composé après les monstres des gemmes.
constexpr std::uint32_t kBallClass = 1002;
constexpr std::size_t kBallSlot = ro::ui::kRojeweledKinds;
// Le côté du cadre de la bille, un peu plus grand que son contact.
constexpr int kBallBoxPx = 26;
// Les gemmes des plots : Red Gemstone à effacer, Blue Gemstone en bonus,
// Violet Jewel qui tourne, Emerald qui porte un pouvoir.
constexpr std::uint32_t kOrangePegItem = 716;
constexpr std::uint32_t kBluePegItem = 717;
constexpr std::uint32_t kPurplePegItem = 719;
constexpr std::uint32_t kGreenPegItem = 721;

// LA VIE D'UN AVIS et d'un éclat à l'écran, en temps de partie.
constexpr std::uint32_t kCalloutLifeMs = 900;
constexpr std::uint32_t kBurstLifeMs = 450;

// Les sons, sous le dossier des sons du jeu.
constexpr const char* kOrangeHitSound = "effect\\EF_hit2.wav";
constexpr const char* kBlueHitSound = "effect\\EF_hit4.wav";
constexpr const char* kWallSound = "effect\\EF_hit3.wav";
constexpr const char* kPowerSound = "effect\\EF_MagnumBreak.wav";
constexpr const char* kFreeBallSound = "levelup.wav";
constexpr const char* kRewardSound = "_heal_effect.wav";

constexpr float kMsPerSecond = 1000.0F;

// UNE PARTIE LIBRE se joue sur un plateau d'objet deux fois sur trois.
constexpr double kItemBoardShare = 2.0 / 3.0;
// Les objets essayés à la suite avant de se rabattre sur un plateau dessiné :
// une image peut manquer ou tracer trop peu de plots.
constexpr int kItemAttempts = 12;
constexpr int kChannels = 4;
constexpr int kAlphaChannel = 3;
constexpr int kOpaque = 255;

// L'IMAGE DE COLLECTION RELEVÉE pour les règles, ou rien quand elle manque.
[[nodiscard]] std::optional<ro::sim::PegPicture> PictureOf(ro::render::ItemIcons& icons,
                                                           std::uint32_t item_id) {
  const std::optional<ro::formats::Image> image = icons.ReadCollection(item_id);
  if (!image.has_value() || image->empty()) {
    return std::nullopt;
  }
  ro::sim::PegPicture picture{.item_id = item_id, .width = image->width, .height = image->height};
  const auto pixels =
      static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height);
  picture.opacity.resize(pixels);
  picture.vividness.resize(pixels);
  for (std::size_t index = 0; index < pixels; ++index) {
    const std::uint8_t* rgba = &image->pixels[index * kChannels];
    const int alpha = rgba[kAlphaChannel];
    const int strongest = std::max({rgba[0], rgba[1], rgba[2]});
    const int weakest = std::min({rgba[0], rgba[1], rgba[2]});
    picture.opacity[index] = static_cast<std::uint8_t>(alpha);
    picture.vividness[index] = static_cast<std::uint8_t>((strongest - weakest) * alpha / kOpaque);
  }
  return picture;
}

[[nodiscard]] InterfaceSoundFile SoundOf(const char* path) {
  return InterfaceSoundFile{.place = InterfaceSoundPlace::kSoundArchive, .path = path};
}

// LE JOUR DU DÉFI, compté depuis l'époque Unix en temps universel : le même
// partout à la même heure.
[[nodiscard]] std::uint32_t TodayUtc() {
  const auto days = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
  return static_cast<std::uint32_t>(days.time_since_epoch().count());
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

// Les sortes, les pouvoirs et les avis se correspondent rang pour rang.
[[nodiscard]] ro::ui::RogglePegKind KindOf(ro::sim::PegKind kind) {
  return static_cast<ro::ui::RogglePegKind>(kind);
}
static_assert(static_cast<int>(ro::ui::RogglePegKind::kGreen) ==
              static_cast<int>(ro::sim::PegKind::kGreen));
static_assert(static_cast<int>(ro::ui::RogglePower::kDoubleStrafe) ==
              static_cast<int>(ro::sim::PegPower::kDoubleStrafe));
static_assert(static_cast<int>(ro::ui::RoggleCalloutKind::kBin) ==
              static_cast<int>(ro::sim::PegCalloutKind::kBin));

[[nodiscard]] const char* LayoutName(ro::sim::PegLayout layout) {
  switch (layout) {
    case ro::sim::PegLayout::kScattered:
      return ro::core::Tr("Grille éparse");
    case ro::sim::PegLayout::kPyramid:
      return ro::core::Tr("Pyramide");
    case ro::sim::PegLayout::kRings:
      return ro::core::Tr("Anneaux");
    case ro::sim::PegLayout::kPoring:
      return "Poring";
    case ro::sim::PegLayout::kWaves:
      return ro::core::Tr("Vagues");
    case ro::sim::PegLayout::kItem:
      break;
  }
  return "";
}

// Le nom d'un objet de la table, ou vide.
[[nodiscard]] const char* ItemName(const ro::render::ItemIcons& icons, std::uint32_t item_id) {
  const ro::vfs::ItemEntry* entry = icons.items().Find(item_id);
  return entry != nullptr ? entry->name.c_str() : "";
}

[[nodiscard]] float Progress(std::uint32_t age_ms, std::uint32_t life_ms) {
  return std::min(1.0F, static_cast<float>(age_ms) / static_cast<float>(life_ms));
}

}  // namespace

void MinigameStage::StartRoggle(const MinigamePieces& pieces, bool daily) {
  const std::uint32_t today = TodayUtc();
  ro::render::ItemIcons* icons = pieces.item_icons;
  const bool item_board =
      icons != nullptr && (daily || std::bernoulli_distribution(kItemBoardShare)(roggle_boards_));
  std::vector<std::uint32_t> items;
  if (item_board) {
    // Le défi reste le même pour tous, quel que soit le carnet de chacun.
    items = daily ? icons->CollectionItems()
                  : ro::sim::RoggleItemChoices(icons->CollectionItems(), record_->record);
  }
  if (!items.empty()) {
    // ⚠ L'OBJET DU JOUR se tire de la sortie brute du générateur, que la norme
    // fixe : une distribution, elle, change d'une bibliothèque à l'autre.
    std::mt19937 daily_draw(today);
    std::size_t at = (daily ? daily_draw() : roggle_boards_()) % items.size();
    for (int attempt = 0; attempt < kItemAttempts; ++attempt, at = (at + 1) % items.size()) {
      const std::optional<ro::sim::PegPicture> picture = PictureOf(*icons, items[at]);
      if (!picture.has_value()) {
        continue;
      }
      if (daily ? roggle_.NewDailyItemGame(today, *picture) : roggle_.NewItemGame(*picture)) {
        return;
      }
    }
  }
  if (daily) {
    roggle_.NewDailyGame(today);
  } else {
    roggle_.NewGame();
  }
}

void MinigameStage::ApplyRoggleAnswer(MinigameWindows& windows, const MinigamePieces& pieces) {
  const ro::ui::RoggleAsk pegs = std::exchange(windows.roggle_answer, {});
  roggle_watching_ = pegs.watching;
  if (pegs.skip_revealed.has_value() && record_) {
    record_->record.roggle_skip_revealed = *pegs.skip_revealed;
    record_unsaved_ = true;
  }
  if (pegs.new_game) {
    StartRoggle(pieces, false);
  }
  if (pegs.daily_game) {
    StartRoggle(pieces, true);
  }
  if (pegs.replay_game) {
    // ⚠ UN DÉFI SE REJOUE SUR LE JOUR COURANT, qui l'a peut-être remplacé depuis :
    // sa partie se retire à l'identique, et un score ne s'inscrit pas à un jour passé.
    if (roggle_.daily()) {
      StartRoggle(pieces, true);
    } else {
      roggle_.Replay();
    }
  }
  if (pegs.new_game || pegs.daily_game || pegs.replay_game) {
    roggle_callouts_.clear();
    roggle_bursts_.clear();
  }
  if (pegs.aim.has_value()) {
    roggle_.Aim({pegs.aim->x, pegs.aim->y});
  }
  if (pegs.aim_nudge != 0) {
    roggle_.NudgeAim(pegs.aim_nudge);
  }
  if (pegs.launch) {
    roggle_.Launch();
  }
}

void MinigameStage::PlayRoggle(std::uint32_t step_ms, const MinigamePieces& pieces) {
  if (!roggle_.started()) {
    StartRoggle(pieces, false);
  }
  roggle_.Advance(static_cast<float>(step_ms) / kMsPerSecond);
  ro::sim::PegNews news = roggle_.TakeNews();
  if (news.orange_hits > 0 || news.special_hits > 0) {
    sounds_.push_back(SoundOf(kOrangeHitSound));
  }
  if (news.blue_hits > 0) {
    sounds_.push_back(SoundOf(kBlueHitSound));
  }
  if (news.wall_hits > 0) {
    sounds_.push_back(SoundOf(kWallSound));
  }
  if (news.power) {
    sounds_.push_back(SoundOf(kPowerSound));
  }
  if (news.free_ball) {
    sounds_.push_back(SoundOf(kFreeBallSound));
  }
  if (news.caught || (news.finished && roggle_.phase() == ro::sim::PegPhase::kWon)) {
    sounds_.push_back(SoundOf(kRewardSound));
  }
  const std::uint32_t watched_ms = roggle_clock_.watched_ms();
  for (const ro::sim::PegCallout& callout : news.callouts) {
    roggle_callouts_.push_back(RoggleCalloutBorn{.callout = callout, .born_ms = watched_ms});
  }
  for (const ro::sim::PegCleared& cleared : news.cleared) {
    roggle_bursts_.push_back(RoggleBurstBorn{.cleared = cleared, .born_ms = watched_ms});
  }
  while (!roggle_callouts_.empty() &&
         watched_ms - roggle_callouts_.front().born_ms >= kCalloutLifeMs) {
    roggle_callouts_.pop_front();
  }
  while (!roggle_bursts_.empty() && watched_ms - roggle_bursts_.front().born_ms >= kBurstLifeMs) {
    roggle_bursts_.pop_front();
  }
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
                                   .bucket_height = ro::sim::kPegBucketHeight,
                                   .bucket_y = ro::sim::kPegBucketY,
                                   .finale_bin_height = ro::sim::kPegFinaleBinHeight};
  pegs_.clear();
  for (const ro::sim::Peg& peg : roggle_.pegs()) {
    if (!peg.removed) {
      pegs_.push_back(ro::ui::RogglePeg{.at = {peg.x, peg.y},
                                        .kind = KindOf(peg.kind),
                                        .power = static_cast<ro::ui::RogglePower>(peg.power),
                                        .lit = peg.lit});
    }
  }
  view.pegs = pegs_;
  guide_.clear();
  for (const ro::sim::PegPoint point : roggle_.AimGuide()) {
    guide_.push_back({point.x, point.y});
  }
  view.guide = guide_;
  roggle_balls_.clear();
  for (const ro::sim::PegBall& ball : roggle_.balls()) {
    roggle_balls_.push_back({ball.at.x, ball.at.y});
  }
  view.balls = roggle_balls_;

  const std::uint32_t watched_ms = roggle_clock_.watched_ms();
  roggle_callout_views_.clear();
  for (const RoggleCalloutBorn& born : roggle_callouts_) {
    roggle_callout_views_.push_back(
        ro::ui::RoggleCallout{.kind = static_cast<ro::ui::RoggleCalloutKind>(born.callout.kind),
                              .at = {born.callout.at.x, born.callout.at.y},
                              .points = born.callout.points,
                              .progress = Progress(watched_ms - born.born_ms, kCalloutLifeMs)});
  }
  view.callouts = roggle_callout_views_;
  roggle_burst_views_.clear();
  for (const RoggleBurstBorn& born : roggle_bursts_) {
    roggle_burst_views_.push_back(
        ro::ui::RoggleBurst{.at = {born.cleared.at.x, born.cleared.at.y},
                            .kind = KindOf(born.cleared.kind),
                            .progress = Progress(watched_ms - born.born_ms, kBurstLifeMs)});
  }
  view.bursts = roggle_burst_views_;

  view.phase = PhaseOf(roggle_.phase());
  view.bucket_x = roggle_.bucket_x();
  view.bucket_width = roggle_.bucket_width();
  view.finale = roggle_.finale();
  view.finale_bins = ro::sim::kPegFinaleBins;
  view.slowed = roggle_.slowed();
  view.paused = !roggle_watching_;
  view.aim = {roggle_.aim().x, roggle_.aim().y};
  view.balls_left = roggle_.balls_left();
  view.score = roggle_.score();
  view.orange_left = roggle_.orange_left();
  view.multiplier = roggle_.multiplier();
  view.best_score = record_->record.roggle_best;
  view.layout_name = LayoutName(roggle_.layout());
  view.picture = {};
  roggle_revealed_.clear();
  roggle_notebook_.clear();
  view.notebook_total = 0;
  if (pieces.item_icons != nullptr) {
    ro::render::ItemIcons& icons = *pieces.item_icons;
    if (roggle_.layout() == ro::sim::PegLayout::kItem) {
      for (const ro::sim::Peg& peg : roggle_.pegs()) {
        if (peg.removed) {
          roggle_revealed_.push_back({peg.x, peg.y});
        }
      }
      const ro::sim::PegFrame frame = roggle_.picture_frame();
      view.picture =
          ro::ui::RogglePicture{.art = SizedSkinImageOf(icons.GetCollection(roggle_.item_id())),
                                .origin = {frame.origin.x, frame.origin.y},
                                .width = frame.width,
                                .height = frame.height,
                                .revealed = roggle_revealed_};
      view.layout_name = ItemName(icons, roggle_.item_id());
    }
    for (const std::uint32_t item_id : record_->record.roggle_revealed) {
      roggle_notebook_.push_back(ro::ui::RoggleNotebookEntry{.icon = ItemIconImage(&icons, item_id),
                                                             .name = ItemName(icons, item_id)});
    }
    view.notebook_total = static_cast<int>(icons.CollectionItems().size());
  }
  view.notebook = roggle_notebook_;
  view.skip_revealed = record_->record.roggle_skip_revealed;
  view.daily = roggle_.daily();
  view.daily_best = roggle_.daily() && record_->record.roggle_daily_day == roggle_.day()
                        ? record_->record.roggle_daily_best
                        : 0;
  view.orange_art = ItemIconImage(pieces.item_icons, kOrangePegItem);
  view.blue_art = ItemIconImage(pieces.item_icons, kBluePegItem);
  view.purple_art = ItemIconImage(pieces.item_icons, kPurplePegItem);
  view.green_art = ItemIconImage(pieces.item_icons, kGreenPegItem);
  view.ball_art = SpriteOf(kBallSlot, kBallClass, kBallBoxPx, pieces.wardrobe, now_ms);
  // ⚠ L'AVIS N'EST MONTRÉ QUE PAR UN JEU : Rojeweled le prend s'il est ouvert.
  view.toast = windows.rojeweled.open ? nullptr : Toast(now_ms, view.toast_alpha);
}

}  // namespace moonclient
