#include "loading/loading_art.h"

#include <string_view>

namespace moonclient {
namespace {

// Le dossier des textures d'interface et la forme du nom : le rang en deux
// chiffres décimaux, entre le préfixe et l'extension.
constexpr std::string_view kLoadingArtPrefix = "data\\texture\\유저인터페이스\\loading";
constexpr std::string_view kLoadingArtSuffix = ".jpg";
constexpr std::size_t kDecimalBase = 10;

}  // namespace

std::size_t LoadingArtRank(std::uint32_t draw) noexcept { return draw % kLoadingArtCount; }

std::string LoadingArtPath(std::size_t rank) {
  const std::size_t bounded = rank % kLoadingArtCount;
  std::string path(kLoadingArtPrefix);
  path += static_cast<char>('0' + bounded / kDecimalBase);
  path += static_cast<char>('0' + bounded % kDecimalBase);
  path += kLoadingArtSuffix;
  return path;
}

bool LoadingArtExpired(std::optional<std::uint32_t> last_shown_ms, std::uint32_t now_ms) noexcept {
  return !last_shown_ms.has_value() || now_ms - *last_shown_ms > kLoadingArtKeepMs;
}

}  // namespace moonclient
