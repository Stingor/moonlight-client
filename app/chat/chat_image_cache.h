#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/web_preview_policy.h"
#include "ro/render/chat_picture.h"
#include "ro/render/gl_texture.h"
#include "ro/ui/chat_rich_art.h"

namespace moonclient {

// ── LES IMAGES DU CHAT, TÉLÉCHARGÉES PUIS TÉLÉVERSÉES ───────────────────────
//
// Un fil détaché par téléchargement, qui écrit dans un casier partagé ; la
// boucle, seule à tenir le contexte OpenGL, téléverse ce qui est prêt.
//
// 🔴 SEULES LES ADRESSES ADMISES SE TÉLÉCHARGENT : la liste blanche se repasse
// à chaque saut de redirection, et c'est elle qui décide — pas l'appelant.

// L'aperçu au survol et la vignette, ou la visionneuse en grand : deux budgets.
enum class ChatPictureSize : std::uint8_t { kPreview, kLarge };

// Une image prête : ses textures, et ses images d'animation qui les désignent —
// une seule pour une image fixe, qui porte alors un délai nul.
struct ChatPictureTextures {
  std::vector<ro::render::GlTexture> textures;
  std::vector<ro::ui::ChatPictureFrame> frames;
  float width_px = 0.0F;
  float height_px = 0.0F;
};

// Ce qu'un enregistrement a donné : le fichier écrit, ou l'échec.
struct ChatSaveOutcome {
  bool ok = false;
  std::filesystem::path written;
};

// Les adresses se donnent OUVRABLES (`ro::core::OpenableWebAddress`) : c'est
// sous cette forme que la liste blanche et les accords les jugent.
class ChatImageCache {
 public:
  // Pose les budgets des deux tailles.
  ChatImageCache();

  // Une adresse qui n'est plus permise perd son image au `Poll` suivant ; un
  // échec que l'accord neuf peut lever est oublié, et se relancera.
  void SetAllowList(ro::core::PreviewAllowList allow);
  // Rien ne part pour une adresse que la liste ne permet pas, ni au-delà de
  // trois téléchargements en vol : la demande suivante retentera.
  void Request(std::string_view url, ChatPictureSize size);
  // Fil principal : téléverse ce qui est prêt, puis évince.
  //
  // ⚠ AVANT DE COMPOSER L'IMAGE, jamais pendant : une texture libérée ici et
  // encore citée par une liste de dessin d'ImGui se peindrait d'un autre nom.
  void Poll();
  [[nodiscard]] ro::ui::ChatPictureState State(std::string_view url, ChatPictureSize size) const;
  [[nodiscard]] const ChatPictureTextures* Ready(std::string_view url, ChatPictureSize size) const;
  // Retélécharge et écrit les octets D'ORIGINE dans `folder`, sous
  // `chat_AAAAMMJJ_HHMMSS.<ext>` à l'heure de la demande — `_2`, `_3`… pour
  // la même seconde. Les enregistrements passent un par un, dans l'ordre.
  void RequestSave(std::string_view url, const std::filesystem::path& folder);
  [[nodiscard]] std::optional<ChatSaveOutcome> TakeSaved();
  // Croît à chaque changement d'état d'une image, ou de la liste : ce qu'on a
  // lu du cache sous une autre valeur est périmé.
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

 private:
  // LE CASIER QU'UN FIL REMPLIT.
  //
  // 🔴 PARTAGÉ PAR POINTEUR COMPTÉ, ET LE FIL EST DÉTACHÉ : un `std::future`
  // bloquerait dans son destructeur jusqu'au délai de garde, et fermer le
  // client pendant un téléchargement le figerait. Lâcher le casier suffit ; le
  // fil finit dans sa copie, qui meurt avec lui.
  struct PictureSlot {
    std::mutex lock;
    bool ok = false;
    ro::render::ChatPicture picture;
    std::string failure;
    std::atomic<bool> done{false};
  };
  struct SaveSlot {
    std::mutex lock;
    std::string url;
    ChatSaveOutcome outcome;
    std::string failure;
    std::atomic<bool> done{false};
  };
  // UNE ÉTAGÈRE PAR TAILLE : son registre, ses fils en vol, ses textures.
  struct Shelf {
    ro::core::PreviewBook book;
    ro::render::ChatPictureLimits limits;
    std::map<std::string, std::shared_ptr<PictureSlot>, std::less<>> pending;
    std::map<std::string, ChatPictureTextures, std::less<>> ready;
    // Les échecs gardés, pour qu'un accord neuf puisse les relancer.
    std::set<std::string, std::less<>> failed;
  };

  // Un enregistrement en attente : l'adresse, et le chemin final sans son
  // extension, fixé à la demande.
  struct SaveJob {
    std::string url;
    std::filesystem::path stem_path;
  };

  [[nodiscard]] Shelf& ShelfOf(ChatPictureSize size) noexcept;
  [[nodiscard]] const Shelf& ShelfOf(ChatPictureSize size) const noexcept;
  void PollShelf(Shelf& shelf);
  // Range l'enregistrement fini, et lance le suivant.
  void PumpSaves();

  ro::core::PreviewAllowList allow_;
  Shelf preview_;
  Shelf large_;
  // 🔴 UN SEUL DÉCODAGE À LA FOIS, tous fils confondus : trois images géantes
  // décodées ensemble tripleraient la mémoire que chaque garde borne. Partagé
  // par pointeur compté, comme les casiers, pour survivre au cache.
  std::shared_ptr<std::mutex> decoding_ = std::make_shared<std::mutex>();
  std::deque<SaveJob> save_queue_;
  std::shared_ptr<SaveSlot> save_running_;
  std::deque<ChatSaveOutcome> saves_done_;
  // La seconde du dernier nom donné, et son rang dans cette seconde.
  std::string save_second_;
  int save_rank_ = 0;
  std::uint64_t generation_ = 0;
};

}  // namespace moonclient
