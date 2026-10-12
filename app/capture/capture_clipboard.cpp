#include "capture/capture_clipboard.h"

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/formats/gif.h"

#if defined(_WIN32)
#include <shlobj_core.h>  // DROPFILES
#include <windows.h>
#else
#include <SDL3/SDL.h>
#endif

namespace moonclient {
namespace {

#if defined(_WIN32)

// Le nom de ce format est un MOT DE PROTOCOLE, pas un texte affiché.
//
// 🔴 IL NE SE TRADUIT PAS. Le passer par le catalogue enregistrerait, dans une
// autre langue, un format que personne ne vient lire — et le collage
// redeviendrait un DÉPLACEMENT en silence. C'est exactement l'écueil que
// l'implémentation de référence a laissé passer : chez elle, le nom traverse la
// traduction et ne survit que parce que le français le rend inchangé.
constexpr const char* kPreferredDropEffect = "Preferred DropEffect";

// LES PORTEURS D'ANIMATION, dans le dossier temporaire : leur préfixe, et l'âge
// au-delà duquel on les retire.
constexpr std::string_view kCarrierPrefix = "moonclient_";
constexpr std::string_view kCarrierSuffix = ".gif";
constexpr std::chrono::hours kCarrierKeptFor{24};

// RETIRE LES PORTEURS D'UN AUTRE JOUR.
//
// 🔴 RIEN D'AUTRE NE LES EFFACE : le presse-papier cite le fichier, il ne le
// garde pas, et un joueur qui fait des clips chaque jour accumulait des
// centaines de mégaoctets. ⚠ PAS LES RÉCENTS : le dernier copié peut encore
// être collé, et c'est toute sa raison d'être.
void ForgetOldCarriers(const std::filesystem::path& folder) {
  const auto oldest = std::filesystem::file_time_type::clock::now() - kCarrierKeptFor;
  std::error_code failed;
  for (auto entry = std::filesystem::directory_iterator(folder, failed);
       !failed && entry != std::filesystem::directory_iterator(); entry.increment(failed)) {
    const std::string name = ro::core::Utf8FromPath(entry->path().filename());
    if (!name.starts_with(kCarrierPrefix) || !name.ends_with(kCarrierSuffix)) {
      continue;
    }
    std::error_code ignored;
    if (entry->last_write_time(ignored) < oldest && !ignored) {
      std::filesystem::remove(entry->path(), ignored);
    }
  }
}

// Dépose un bloc dans le presse-papier. En cas de SUCCÈS le presse-papier en
// devient PROPRIÉTAIRE : le libérer serait une seconde libération.
[[nodiscard]] bool Hand(UINT format, HGLOBAL block) {
  if (block == nullptr) {
    return false;
  }
  if (SetClipboardData(format, block) != nullptr) {
    return true;
  }
  GlobalFree(block);
  return false;
}

// La liste d'un seul fichier, telle qu'un gestionnaire de fichiers la dépose.
//
// ⚠ CHEMINS LARGES, ET NON ANSI : un joueur dont le nom de session porte un
// accent a un dossier temporaire qui en porte un aussi, et la conversion
// étroite le remplacerait par un point d'interrogation — le collage viserait
// alors un fichier qui n'existe pas.
[[nodiscard]] HGLOBAL DropListOf(const std::wstring& path) {
  // Deux zéros à la fin : un pour le chemin, un pour la liste.
  const std::size_t bytes = sizeof(DROPFILES) + (path.size() + 2) * sizeof(wchar_t);
  HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (block == nullptr) {
    return nullptr;
  }
  auto* drop = static_cast<DROPFILES*>(GlobalLock(block));
  if (drop == nullptr) {
    GlobalFree(block);
    return nullptr;
  }
  std::memset(drop, 0, bytes);
  drop->pFiles = sizeof(DROPFILES);
  drop->fWide = TRUE;
  std::memcpy(reinterpret_cast<char*>(drop) + sizeof(DROPFILES), path.c_str(),
              path.size() * sizeof(wchar_t));
  GlobalUnlock(block);
  return block;
}

// « Copier », et non « déplacer ».
//
// ⚠ SANS CELA, UN COLLAGE DANS L'EXPLORATEUR PEUT DÉPLACER le fichier porteur —
// le presse-papier pointerait alors sur un chemin vidé, et un second collage ne
// donnerait rien.
[[nodiscard]] HGLOBAL CopyEffect() {
  HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
  if (block == nullptr) {
    return nullptr;
  }
  auto* value = static_cast<DWORD*>(GlobalLock(block));
  if (value == nullptr) {
    GlobalFree(block);
    return nullptr;
  }
  *value = DROPEFFECT_COPY;
  GlobalUnlock(block);
  return block;
}

[[nodiscard]] bool OwnClipboard() {
  // Échec normal et passager : une autre application le tient à cet instant.
  // Rien à réparer, mais le code se trace.
  if (OpenClipboard(nullptr) == 0) {
    // EN ALERTE, et le commentaire ci-dessus dit pourquoi : l'échec est
    // passager et le joueur le lit à l'écran. L'erreur est réservée à ce qu'on
    // ne rattrape plus.
    ro::core::LogWarning("presse-papier refusé (code {})", GetLastError());
    return false;
  }
  EmptyClipboard();
  return true;
}

#else

// Le presse-papier des autres systèmes transporte des types MIME : une animation
// y entre telle quelle, sans fichier porteur.
struct ClipboardBytes {
  std::string mime;
  std::vector<std::uint8_t> data;
};

const void* HandBytes(void* held, const char* mime, std::size_t* size) {
  auto* kept = static_cast<ClipboardBytes*>(held);
  if (kept == nullptr || mime == nullptr || kept->mime != mime) {
    return nullptr;
  }
  *size = kept->data.size();
  return kept->data.data();
}

void DropBytes(void* held) { delete static_cast<ClipboardBytes*>(held); }

[[nodiscard]] bool OfferBytes(const char* mime, std::span<const std::uint8_t> data) {
  auto* kept = new ClipboardBytes{.mime = mime, .data = {data.begin(), data.end()}};
  const std::array<const char*, 1> types = {mime};
  if (!SDL_SetClipboardData(&HandBytes, &DropBytes, kept, types.data(), types.size())) {
    delete kept;
    return false;
  }
  return true;
}

#endif

}  // namespace

bool PutAnimationOnClipboard(std::span<const std::uint8_t> gif) {
  if (gif.empty()) {
    return false;
  }
#if defined(_WIN32)
  // Un nom qui ne se répète pas : deux captures d'affilée ne doivent pas se
  // disputer le même porteur, dont la première peut encore être collée.
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  std::error_code no_temp;
  const std::filesystem::path temp = std::filesystem::temp_directory_path(no_temp);
  if (no_temp) {
    ro::core::LogError("dossier temporaire introuvable — l'animation ne sera pas copiée");
    return false;
  }
  ForgetOldCarriers(temp);
  const std::filesystem::path carrier =
      temp / (std::string(kCarrierPrefix) + std::to_string(stamp) + std::string(kCarrierSuffix));
  {
    std::ofstream out(carrier, std::ios::binary);
    if (!out) {
      // ⚠ AVEC SON CHEMIN : le porteur est bâti juste au-dessus dans le dossier
      // temporaire, sous une estampille d'horloge, donc introuvable après coup —
      // et c'est justement l'emplacement qui est en cause quand un flux refuse
      // de s'ouvrir. Un `ofstream` en échec, lui, ne rend aucun code.
      ro::core::LogError(
          "porteur de presse-papier impossible à écrire : {} — l'animation ne "
          "sera pas copiée",
          ro::core::Utf8FromPath(carrier));
      return false;
    }
    out.write(reinterpret_cast<const char*>(gif.data()), static_cast<std::streamsize>(gif.size()));
    if (!out) {
      return false;
    }
  }

  HGLOBAL list = DropListOf(carrier.wstring());
  HGLOBAL effect = CopyEffect();
  if (!OwnClipboard()) {
    if (list != nullptr) {
      GlobalFree(list);
    }
    if (effect != nullptr) {
      GlobalFree(effect);
    }
    return false;
  }
  const bool placed = Hand(CF_HDROP, list);
  const UINT wanted = RegisterClipboardFormatA(kPreferredDropEffect);
  if (wanted == 0 || !Hand(wanted, effect)) {
    if (effect != nullptr && wanted == 0) {
      GlobalFree(effect);
    }
  }
  CloseClipboard();
  return placed;
#else
  return OfferBytes("image/gif", gif);
#endif
}

bool PutPictureOnClipboard(const ro::formats::Image& picture) {
  if (picture.empty() || picture.width <= 0 || picture.height <= 0) {
    return false;
  }
#if defined(_WIN32)
  // Un bitmap indépendant du périphérique : en-tête, puis les pixels.
  //
  // ⚠ EN BGRA ET DE BAS EN HAUT — c'est le rangement de ce format, l'inverse du
  // nôtre sur les deux plans. Les deux inversions se font ici, une fois.
  const auto pixels =
      static_cast<std::size_t>(picture.width) * static_cast<std::size_t>(picture.height) * 4U;
  HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + pixels);
  if (block == nullptr) {
    return false;
  }
  auto* header = static_cast<BITMAPINFOHEADER*>(GlobalLock(block));
  if (header == nullptr) {
    GlobalFree(block);
    return false;
  }
  *header = BITMAPINFOHEADER{};
  header->biSize = sizeof(BITMAPINFOHEADER);
  header->biWidth = picture.width;
  header->biHeight = picture.height;
  header->biPlanes = 1;
  header->biBitCount = 32;
  header->biCompression = BI_RGB;
  header->biSizeImage = static_cast<DWORD>(pixels);

  auto* out = reinterpret_cast<std::uint8_t*>(header) + sizeof(BITMAPINFOHEADER);
  const std::size_t row = static_cast<std::size_t>(picture.width) * 4U;
  for (int y = 0; y < picture.height; ++y) {
    const std::uint8_t* from =
        picture.pixels.data() + static_cast<std::size_t>(picture.height - 1 - y) * row;
    std::uint8_t* to = out + static_cast<std::size_t>(y) * row;
    for (int x = 0; x < picture.width; ++x) {
      to[x * 4 + 0] = from[x * 4 + 2];
      to[x * 4 + 1] = from[x * 4 + 1];
      to[x * 4 + 2] = from[x * 4 + 0];
      to[x * 4 + 3] = 255;
    }
  }
  GlobalUnlock(block);

  if (!OwnClipboard()) {
    GlobalFree(block);
    return false;
  }
  const bool placed = Hand(CF_DIB, block);
  CloseClipboard();
  return placed;
#else
  // Ailleurs, une image fixe est une animation d'une seule image : cela évite
  // d'emmener un second encodeur pour un seul usage.
  const auto once = ro::formats::EncodeAnimatedGif({&picture, 1}, ro::formats::kFastestGifDelayCs);
  return once.has_value() && OfferBytes("image/gif", *once);
#endif
}

}  // namespace moonclient
