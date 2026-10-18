#include "character/doll_gif_export.h"

#include <cstddef>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

#include <SDL3/SDL.h>

#include "ro/core/ascii.h"
#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/core/utf8_path.h"
#include "ro/formats/gif.h"

#include "capture/capture_clipboard.h"
#include "capture/screenshot_folder.h"

namespace moonclient {

// CE QUE LE DIALOGUE RÉPOND, et ce qui doit lui survivre.
//
// 🔴 PARTAGÉE, ET NON TENUE PAR L'EXPORT SEUL : le rappel peut arriver après la
// fin de la partie, d'un autre fil. Il garde sa propre part de la boîte, et ne
// la lâche qu'après y avoir écrit — il n'écrit donc jamais dans une mémoire
// rendue.
struct DollGifExport::DialogMailbox {
  enum class Answer : std::uint8_t { kWaiting, kChosen, kCancelled, kFailed };

  std::mutex lock;
  Answer answer = Answer::kWaiting;
  std::string chosen_utf8;
  std::string error;

  // ⚠ LE FILTRE DOIT VIVRE JUSQU'AU RAPPEL : le système le relit pendant que
  // le dialogue est ouvert. Ses textes sont des copies, pour la même raison
  // que le message.
  std::string filter_name;
  std::string filter_pattern;
  SDL_DialogFileFilter filter{};
};

namespace {

using Mailbox = DollGifExport::DialogMailbox;

// L'extension d'un GIF, sans point : la forme qu'un filtre de SDL attend.
constexpr std::string_view kGifExtension = "gif";

// Le rappel du dialogue. `userdata` est une part de la boîte, allouée pour lui
// seul : il l'efface en partant.
void SDLCALL OnSaveChosen(void* userdata, const char* const* filelist, int /*filter*/) {
  auto* held = static_cast<std::shared_ptr<Mailbox>*>(userdata);
  {
    Mailbox& box = **held;
    const std::lock_guard<std::mutex> guard(box.lock);
    if (filelist == nullptr) {
      box.answer = Mailbox::Answer::kFailed;
      box.error = SDL_GetError();
    } else if (filelist[0] == nullptr) {
      box.answer = Mailbox::Answer::kCancelled;
    } else {
      box.answer = Mailbox::Answer::kChosen;
      box.chosen_utf8 = filelist[0];
    }
  }
  delete held;
}

[[nodiscard]] std::string SavedMessage(const std::string& file_name) {
  return ro::core::Printed(ro::core::Tr("GIF enregistré : %s"), file_name.c_str());
}

[[nodiscard]] std::string SavedChatLine(const std::string& shown_path) {
  return ro::core::Printed(ro::core::Tr("GIF du personnage enregistré : %s"), shown_path.c_str());
}

[[nodiscard]] std::string SavedUnderFreeNameMessage(const std::string& file_name) {
  return ro::core::Printed(ro::core::Tr("GIF enregistré sous un nom libre : %s"),
                           file_name.c_str());
}

// Le plus de numéros qu'on essaie pour trouver un nom libre.
constexpr int kMostFreeNameTries = 999;
// Le premier numéro proposé : `avatar (2).gif`, le fichier existant étant le 1.
constexpr int kFirstFreeNameNumber = 2;

// L'extension est-elle `.gif`, quelle qu'en soit la casse ?
[[nodiscard]] bool HasGifExtension(const std::filesystem::path& path) {
  const std::string extension = ro::core::Utf8FromPath(path.extension());
  const std::string wanted = "." + std::string(kGifExtension);
  return ro::core::AsciiEqualsFolded(extension, wanted);
}

// LE PREMIER NOM LIBRE à côté de `taken` : `nom (2).gif`, `nom (3).gif`… Rien
// s'ils sont tous pris.
[[nodiscard]] std::optional<std::filesystem::path> FreeNameBeside(
    const std::filesystem::path& taken) {
  const std::string stem = ro::core::Utf8FromPath(taken.stem());
  const std::string extension = ro::core::Utf8FromPath(taken.extension());
  for (int number = kFirstFreeNameNumber; number <= kMostFreeNameTries; ++number) {
    std::filesystem::path candidate =
        taken.parent_path() /
        ro::core::PathFromUtf8(stem + " (" + std::to_string(number) + ")" + extension);
    std::error_code failed;
    if (!std::filesystem::exists(candidate, failed) && !failed) {
      return candidate;
    }
  }
  return std::nullopt;
}

// CE QUE LA CONVERSATION MONTRE DU CHEMIN : relatif à l'installation quand le
// fichier est dans le dossier des captures — le joueur le reconnaît —, entier
// sinon.
//
// ⚠ LE DOSSIER SE COMPARE PAR LE SYSTÈME, et non par le texte : la casse, un
// lien ou un chemin court désignent le même dossier sous deux écritures.
[[nodiscard]] std::string ShownPath(const std::filesystem::path& written,
                                    const std::filesystem::path& client_root) {
  const std::filesystem::path folder = client_root / kSavedPicturesFolder;
  std::error_code failed;
  if (std::filesystem::equivalent(written.parent_path(), folder, failed) && !failed) {
    std::filesystem::path shown = std::filesystem::path(kSavedPicturesFolder) / written.filename();
    return ro::core::Utf8FromPath(shown.make_preferred());
  }
  return ro::core::Utf8FromPath(written);
}

}  // namespace

DollGifExport::DollGifExport() = default;

DollGifExport::~DollGifExport() {
  if (encoder_.joinable()) {
    encoder_.join();
  }
}

ro::ui::DollGifView DollGifExport::view() const noexcept {
  return ro::ui::DollGifView{.busy = verb_ != ro::ui::DollGifVerb::kNothing, .message = message_};
}

void DollGifExport::Refuse(const std::string& cause) {
  if (verb_ == ro::ui::DollGifVerb::kNothing) {
    Fail(cause);
  }
}

void DollGifExport::Fail(const std::string& cause) {
  ro::core::LogError("GIF du personnage : {}", cause);
  message_ = ro::core::Tr("Échec du GIF");
  verb_ = ro::ui::DollGifVerb::kNothing;
  mailbox_.reset();
}

void DollGifExport::Start(const DollGifOrder& order, ro::render::ActorWardrobe& wardrobe,
                          SDL_Window* window, const std::filesystem::path& client_root) {
  if (verb_ != ro::ui::DollGifVerb::kNothing || order.verb == ro::ui::DollGifVerb::kNothing) {
    return;
  }
  ro::render::ActorOutfit* outfit = wardrobe.OutfitFor(order.look);
  if (outfit == nullptr) {
    Fail("aucun corps à montrer");
    return;
  }
  const ro::render::DollTimeline timeline = ro::render::PlanDollTimeline(ro::render::DollClocksOf(
      *outfit, order.shot.animation, order.shot.direction, order.shot.body_plays));
  const auto poses = ro::render::ComposeDollPoses(*outfit, order.shot, timeline);
  if (!poses) {
    Fail(ro::core::Describe(poses.error()));
    return;
  }
  // 🔴 LE PLAFOND SE JUGE AVANT DE DESSINER : les images se font ici, sur le
  // fil de la boucle, et c'est à l'échelle que le joueur peut le corriger.
  if (const std::size_t pixels = ro::render::DollGifPixels(*poses, order.shot);
      pixels > ro::render::kMostDollGifPixels) {
    ro::core::LogError("GIF du personnage : {} pixels, plafond {}", pixels,
                       ro::render::kMostDollGifPixels);
    message_ = ro::core::Tr("GIF trop grand : choisis une taille plus petite");
    return;
  }
  auto frames = ro::render::DrawDollFrames(*poses, order.shot);
  if (!frames) {
    Fail(ro::core::Describe(frames.error()));
    return;
  }

  verb_ = order.verb;
  client_root_ = client_root;
  message_.clear();
  Encode(std::move(*frames), timeline.delays_cs);
  if (verb_ == ro::ui::DollGifVerb::kSave) {
    const std::filesystem::path folder = client_root / kSavedPicturesFolder;
    std::error_code failed;
    std::filesystem::create_directories(folder, failed);
    if (failed) {
      ro::core::LogWarning("GIF du personnage : le dossier {} n'a pas pu se créer — {}",
                           ro::core::Utf8FromPath(folder), failed.message());
    }
    OpenSaveDialog(window, folder, order.file_name);
  }
}

void DollGifExport::Encode(std::vector<ro::formats::Image> frames, std::vector<int> delays_cs) {
  encoded_.store(false);
  bytes_.clear();
  encode_error_.clear();
  // ⚠ LES IMAGES SONT CÉDÉES AU FIL, pas partagées : il en est seul lecteur.
  encoder_ = std::thread([this, frames = std::move(frames), delays_cs = std::move(delays_cs)] {
    auto bytes = ro::formats::EncodeTransparentGif(frames, delays_cs);
    if (bytes) {
      bytes_ = std::move(*bytes);
    } else {
      encode_error_ = ro::core::Describe(bytes.error());
    }
    // 🔴 POSÉ EN DERNIER : tout ce que le fil écrit doit être visible avant.
    encoded_.store(true);
  });
}

void DollGifExport::OpenSaveDialog(SDL_Window* window, const std::filesystem::path& folder,
                                   const std::string& file_name) {
  mailbox_ = std::make_shared<Mailbox>();
  mailbox_->filter_name = ro::core::Tr("GIF animé (*.gif)");
  mailbox_->filter_pattern = std::string(kGifExtension);
  mailbox_->filter = SDL_DialogFileFilter{.name = mailbox_->filter_name.c_str(),
                                          .pattern = mailbox_->filter_pattern.c_str()};
  const std::string title = ro::core::Tr("Enregistrer le GIF du personnage");
  const std::string proposed = ro::core::Utf8FromPath(folder / ro::core::PathFromUtf8(file_name));

  // ⚠ LES PROPRIÉTÉS SE RELISENT À L'OUVERTURE et peuvent être détruites
  // aussitôt ; seul le filtre doit durer, et il vit dans la boîte.
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, &mailbox_->filter);
  SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 1);
  SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window);
  SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, proposed.c_str());
  SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, title.c_str());
  SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_SAVEFILE, OnSaveChosen,
                                   new std::shared_ptr<Mailbox>(mailbox_), props);
  SDL_DestroyProperties(props);
}

std::optional<std::string> DollGifExport::Tick() {
  if (verb_ == ro::ui::DollGifVerb::kNothing || !encoded_.load()) {
    return std::nullopt;
  }
  if (verb_ == ro::ui::DollGifVerb::kSave) {
    // ⚠ LE DIALOGUE PEUT RÉPONDRE APRÈS L'ENCODAGE : on l'attend.
    const std::lock_guard<std::mutex> guard(mailbox_->lock);
    if (mailbox_->answer == Mailbox::Answer::kWaiting) {
      return std::nullopt;
    }
  }
  if (encoder_.joinable()) {
    encoder_.join();
  }
  return Deliver();
}

std::optional<std::string> DollGifExport::Deliver() {
  if (!encode_error_.empty() || bytes_.empty()) {
    Fail("encodage impossible : " + encode_error_);
    return std::nullopt;
  }

  if (verb_ == ro::ui::DollGifVerb::kCopy) {
    if (!PutAnimationOnClipboard(bytes_)) {
      Fail("le presse-papier a refusé l'animation");
      return std::nullopt;
    }
    message_ = ro::core::Tr("GIF copié — Ctrl+V pour le coller.");
    verb_ = ro::ui::DollGifVerb::kNothing;
    return std::nullopt;
  }

  Mailbox::Answer answer = Mailbox::Answer::kWaiting;
  std::string chosen;
  std::string error;
  {
    const std::lock_guard<std::mutex> guard(mailbox_->lock);
    answer = mailbox_->answer;
    chosen = mailbox_->chosen_utf8;
    error = mailbox_->error;
  }
  switch (answer) {
    case Mailbox::Answer::kWaiting:
    case Mailbox::Answer::kFailed:
      Fail("le dialogue d'enregistrement n'a pas pu s'ouvrir — " + error);
      return std::nullopt;
    case Mailbox::Answer::kCancelled:
      message_ = ro::core::Tr("Export annulé");
      verb_ = ro::ui::DollGifVerb::kNothing;
      mailbox_.reset();
      return std::nullopt;
    case Mailbox::Answer::kChosen:
      break;
  }

  // ⚠ UN NOM TAPÉ SANS `.gif` reçoit l'extension, AJOUTÉE et non substituée :
  // `perso.v2` devient `perso.v2.gif`. Un fichier sans elle ne s'ouvrirait chez
  // personne d'un double clic.
  //
  // 🔴 ET CE NOM-LÀ, LE DIALOGUE NE L'A JAMAIS VU : il n'a pas pu demander
  // avant d'écraser. S'il existe, on en prend un libre à côté, et on le dit —
  // jamais on n'écrase un fichier que le joueur n'a pas désigné.
  std::filesystem::path written = ro::core::PathFromUtf8(chosen);
  bool renamed = false;
  if (!HasGifExtension(written)) {
    written += "." + std::string(kGifExtension);
    std::error_code failed;
    if (std::filesystem::exists(written, failed) || failed) {
      const std::optional<std::filesystem::path> free = FreeNameBeside(written);
      if (!free.has_value()) {
        Fail(ro::core::Utf8FromPath(written) + " existe, et aucun nom libre à côté");
        return std::nullopt;
      }
      written = *free;
      renamed = true;
    }
  }
  const std::string_view content(reinterpret_cast<const char*>(bytes_.data()), bytes_.size());
  if (const ro::core::Result<void> saved = ro::core::WriteFileAtomically(written, content);
      !saved) {
    Fail(ro::core::Utf8FromPath(written) + " : " + ro::core::Describe(saved.error()));
    return std::nullopt;
  }
  ro::core::LogInfo("GIF du personnage enregistré : {}", ro::core::Utf8FromPath(written));
  const std::string written_name = ro::core::Utf8FromPath(written.filename());
  message_ = renamed ? SavedUnderFreeNameMessage(written_name) : SavedMessage(written_name);
  verb_ = ro::ui::DollGifVerb::kNothing;
  mailbox_.reset();
  return SavedChatLine(ShownPath(written, client_root_));
}

}  // namespace moonclient
