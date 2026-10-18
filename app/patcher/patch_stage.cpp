#include "patcher/patch_stage.h"

#include <array>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <SDL3/SDL.h>

#include "ro/core/byte_source.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/formats/image.h"
#include "ro/render/gl_texture.h"
#include "ro/render/icon_texture.h"
#include "ro/ui/login_window.h"
#include "ro/ui/patch_screen.h"

#include "game/client_shell.h"
#include "game/skin_image_of.h"
#include "patcher/http_patch_fetcher.h"
#include "patcher/install_lock.h"
#include "patcher/patcher_state.h"
#include "patcher/relaunch.h"
#include "shell/skin_folder.h"

namespace moonclient::patcher {
namespace {

constexpr std::array<float, 4> kPatchClearColor = {0.0F, 0.0F, 0.0F, 1.0F};
// La cadence de l'écran : il n'anime qu'une barre, trente images suffisent.
constexpr std::uint32_t kPatchFrameMs = 33;
constexpr std::size_t kLineCapacity = 256;

// Ce que le fil du patcher rend, une fois fini.
struct WorkerResult {
  std::atomic<bool> done{false};
  std::optional<ro::core::Result<PatchRunOutcome>> outcome;
};

// Les textes d'une image, tenus le temps qu'ImGui les lise.
struct PatchScreenText {
  std::array<char, kLineCapacity> headline{};
  std::string detail;
};

[[nodiscard]] ro::ui::PatchScreenView ViewOf(const PatchProgress& progress, PatchScreenText& text) {
  ro::ui::PatchScreenView view;
  view.retry_label = ro::core::Tr("Réessayer");
  view.play_label = ro::core::Tr("Jouer quand même");
  view.quit_label = ro::core::Tr("Quitter le jeu");
  switch (progress.phase) {
    case PatchPhase::kListing:
      std::snprintf(text.headline.data(), text.headline.size(), "%s",
                    ro::core::Tr("Recherche des mises à jour…"));
      break;
    case PatchPhase::kDownloading:
    case PatchPhase::kApplying:
      std::snprintf(text.headline.data(), text.headline.size(),
                    progress.phase == PatchPhase::kDownloading
                        ? ro::core::Tr("Téléchargement de la mise à jour %zu sur %zu")
                        : ro::core::Tr("Installation de la mise à jour %zu sur %zu"),
                    progress.patch_number, progress.patch_count);
      text.detail = progress.archive_name;
      if (progress.phase == PatchPhase::kDownloading &&
          progress.download.announced_bytes.value_or(0) > 0) {
        view.fraction = static_cast<float>(progress.download.received_bytes) /
                        static_cast<float>(*progress.download.announced_bytes);
      }
      break;
    case PatchPhase::kDone:
      std::snprintf(text.headline.data(), text.headline.size(), "%s",
                    ro::core::Tr("Le jeu est à jour."));
      break;
    case PatchPhase::kFailed:
    case PatchPhase::kStopped:
      std::snprintf(text.headline.data(), text.headline.size(), "%s",
                    ro::core::Tr("La mise à jour a échoué."));
      text.detail = progress.failure;
      view.failed = true;
      break;
  }
  view.headline = text.headline.data();
  view.detail = text.detail.c_str();
  return view;
}

// L'illustration du skin choisi, sinon du skin d'origine, décodée et
// téléversée, ou rien : l'écran se contente alors du noir. Le tout premier
// lancement d'une installation n'a pas encore déposé l'image — voir
// `SeedDefaultSkin`.
[[nodiscard]] ro::render::IconTexture LoadSkinBackdrop(const std::filesystem::path& install_root,
                                                       std::string_view skin_name) {
  ro::render::IconTexture backdrop;
  const std::optional<std::filesystem::path> path =
      SkinPieceOnDisk(install_root, skin_name, ro::ui::kLoginArtPath);
  if (!path.has_value()) {
    return backdrop;
  }
  auto source = ro::core::FileByteSource::Open(*path);
  if (!source) {
    return backdrop;
  }
  auto bytes = (*source)->ReadBlock(0, (*source)->size());
  if (!bytes) {
    return backdrop;
  }
  auto image = ro::formats::DecodeImage(ro::core::Utf8FromPath(path->filename()), *bytes);
  if (!image) {
    ro::core::LogWarning("skin : {} illisible ({})", path->string(),
                         ro::core::Describe(image.error()));
    return backdrop;
  }
  auto texture = ro::render::GlTexture::Upload(*image);
  if (!texture) {
    return backdrop;
  }
  backdrop.texture = std::move(*texture);
  backdrop.width_px = image->width;
  backdrop.height_px = image->height;
  return backdrop;
}

}  // namespace

ro::core::Result<PatchStageEnd> RunPatchStage(ClientShell& shell,
                                              const PatchRunSettings& settings) {
  RemoveReplacedExecutable(settings.running_executable);
  // ⚠ LUE AVANT LE FIL, et sur le disque : aucune archive n'est ouverte
  // pendant qu'on les réécrit.
  const ro::render::IconTexture backdrop =
      LoadSkinBackdrop(shell.client_root(), shell.settings().options().pages().skin.active());
  const ro::ui::SkinImage backdrop_image = SkinImageOf(backdrop);
  while (true) {
    PatchProgressBoard board;
    WorkerResult result;
    std::optional<InstallLock> lock;
    if (auto taken = InstallLock::Take(settings.install_root); taken) {
      lock.emplace(std::move(*taken));
    } else {
      ro::core::LogWarning("patcher : verrou du dossier refusé — {}",
                           ro::core::Describe(taken.error()));
      PatchProgress refused;
      refused.phase = PatchPhase::kFailed;
      refused.failure = ro::core::Describe(taken.error());
      board.Publish(refused);
      result.done = true;
    }
    // ⚠ LE FIL S'ARRÊTE AVEC CETTE PORTÉE : `jthread` demande l'arrêt et
    // l'attend, et le patcher ne quitte un patch qu'entier.
    std::jthread worker;
    if (lock.has_value()) {
      worker = std::jthread([&settings, &board, &result](std::stop_token stop) {
        HttpPatchFetcher fetcher;
        result.outcome = RunPatches(settings, fetcher, board, stop);
        result.done.store(true, std::memory_order_release);
      });
    }

    while (true) {
      const auto frame_started_ms = static_cast<std::uint32_t>(SDL_GetTicks());
      SDL_Event event;
      bool quit_asked = false;
      while (SDL_PollEvent(&event)) {
        shell.interface().TakeEvent(event);
        quit_asked = quit_asked || event.type == SDL_EVENT_QUIT;
      }
      if (quit_asked) {
        ro::core::LogInfo("patcher : fenêtre fermée par le joueur pendant la mise à jour");
        return PatchStageEnd::kQuit;
      }
      const bool finished = result.done.load(std::memory_order_acquire);
      const PatchProgress progress = board.Snapshot();
      if (finished && progress.phase == PatchPhase::kDone && result.outcome.has_value() &&
          result.outcome->has_value()) {
        ro::core::LogInfo("patcher : à jour, {} patch(s) appliqué(s)",
                          (*result.outcome)->applied_patches);
        return (*result.outcome)->executable_replaced ? PatchStageEnd::kRestart
                                                      : PatchStageEnd::kUpToDate;
      }

      PatchScreenText text;
      shell.gl().BindForDisplay();
      shell.gl().ClearBoundTarget(kPatchClearColor);
      ro::ui::PatchScreenView view = ViewOf(progress, text);
      view.backdrop = backdrop_image;
      const ro::ui::PatchScreenAnswers answers = shell.interface().DrawPatchOnly(view);
      shell.gl().SwapBuffers();
      if (finished && answers.quit) {
        ro::core::LogInfo("patcher : le joueur quitte après l'échec ({})", progress.failure);
        return PatchStageEnd::kQuit;
      }
      if (finished && answers.play_anyway) {
        ro::core::LogWarning("patcher : le joueur joue sans la mise à jour ({})", progress.failure);
        return PatchStageEnd::kPlayOutdated;
      }
      if (finished && answers.retry) {
        ro::core::LogInfo("patcher : le joueur réessaie après l'échec ({})", progress.failure);
        break;
      }
      const auto spent_ms = static_cast<std::uint32_t>(SDL_GetTicks()) - frame_started_ms;
      if (spent_ms < kPatchFrameMs) {
        SDL_Delay(kPatchFrameMs - spent_ms);
      }
    }
  }
}

ro::core::Result<PatchStageEnd> UpdateInstall(ClientShell& shell, const PatcherConfig& config) {
  auto executable = RunningExecutablePath();
  if (!executable) {
    return std::unexpected(executable.error());
  }
  PatchRunSettings settings;
  settings.config = config;
  settings.install_root = shell.client_root();
  settings.running_executable = std::move(*executable);
  const std::optional<std::uint64_t> last_applied = ReadLastAppliedPatch(settings.install_root);
  ro::core::LogInfo(
      "patcher : recherche des mises à jour ({} miroir(s), dernier index appliqué {})",
      config.mirrors.size(), last_applied.has_value() ? std::to_string(*last_applied) : "aucun");
  auto ended = RunPatchStage(shell, settings);
  if (!ended || *ended != PatchStageEnd::kRestart) {
    return ended;
  }
  // 🔴 LE JOURNAL SE FERME AVANT LA RELANCE : le nouveau processus rouvre le
  // même fichier, et deux processus qui y écrivent ensemble s'écrasent l'un
  // l'autre.
  ro::core::EndLogFile("l'exécutable a changé, le client se relance");
  if (auto relaunched = RelaunchSelf(); !relaunched) {
    return std::unexpected(relaunched.error());
  }
  return ended;
}

}  // namespace moonclient::patcher
