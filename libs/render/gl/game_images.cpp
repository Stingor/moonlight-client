#include "ro/render/game_images.h"

#include <string>
#include <string_view>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/log.h"
#include "ro/formats/bmp.h"
#include "ro/formats/image.h"

namespace ro::render {
namespace {

// LE SEUL FORMAT QUI PORTE LA CLÉ MAGENTA.
//
// ⚠ PAR L'EXTENSION, parce que c'est tout ce qu'on a : la convention est celle
// du CLIENT et non du format, et elle ne vaut que là où il n'y a pas d'alpha à
// respecter — ses bitmaps. Voir `jpeg.h` pour ce qu'elle ferait d'un JPEG.
constexpr std::string_view kColorKeyedSuffix = ".bmp";

}  // namespace

std::string SafeGamePath(std::string_view path) {
  if (path.empty()) {
    return {};
  }
  // Une racine ou un lecteur : le chemin ne désigne plus l'installation.
  if (path.front() == '/' || path.front() == '\\') {
    return {};
  }
  if (path.size() > 1 && path[1] == ':') {
    return {};
  }
  // Une remontée, sous l'une ou l'autre écriture de séparateur.
  if (path.find("..") != std::string_view::npos) {
    return {};
  }
  // ⚠ LES SÉPARATEURS SE NORMALISENT VERS CELUI DU CLIENT : ses archives
  // nomment leurs entrées à l'antislash, et un script écrit volontiers l'autre.
  std::string safe(path);
  for (char& letter : safe) {
    if (letter == '/') {
      letter = '\\';
    }
  }
  return safe;
}

const IconTexture& GameImages::Get(std::string_view cp949_path) {
  return Fetch(cp949_path, /*ours=*/false, Treatment::kKeyed);
}

const IconTexture& GameImages::GetOurs(std::string_view utf8_path) {
  return Fetch(utf8_path, /*ours=*/true, Treatment::kKeyed);
}

const IconTexture& GameImages::GetOpaque(std::string_view utf8_path) {
  return Fetch(utf8_path, /*ours=*/true, Treatment::kOpaque);
}

const IconTexture& GameImages::GetOursForDarkPaper(std::string_view utf8_path) {
  return Fetch(utf8_path, /*ours=*/true, Treatment::kForDarkPaper);
}

const IconTexture& GameImages::GetOursMatted(std::string_view utf8_path) {
  return Fetch(utf8_path, /*ours=*/true, Treatment::kMatted);
}

const IconTexture& GameImages::GetOursInkShape(std::string_view utf8_path) {
  return Fetch(utf8_path, /*ours=*/true, Treatment::kInkShape);
}

void GameImages::Forget(std::string_view utf8_path) {
  const std::string key = SafeGamePath(utf8_path);
  for (auto& cache : caches_) {
    cache.erase(key);
  }
}

void GameImages::Clear() {
  for (auto& cache : caches_) {
    cache.clear();
  }
  ++generation_;
}

const IconTexture& GameImages::Fetch(std::string_view path, bool ours, Treatment treatment) {
  const std::string key = SafeGamePath(path);
  std::unordered_map<std::string, IconTexture>& cache =
      caches_[static_cast<std::size_t>(treatment)];
  const auto known = cache.find(key);
  if (known != cache.end()) {
    return known->second;
  }
  // ⚠ LA CASE SE POSE D'ABORD, ET VIDE. C'est elle qui retient l'absence — et le
  // refus, dont la clé est la chaîne vide.
  IconTexture& image = cache[key];
  if (vfs_ == nullptr || key.empty()) {
    core::LogWarning("image : chemin refusé — {}", path);
    return image;
  }
  // 🔴 DEUX PORTES POUR DEUX ENCODAGES : voir l'en-tête, et ce que leur
  // confusion a coûté.
  auto bytes = ours ? vfs_->Read(key) : vfs_->ReadRaw(key);
  if (!bytes) {
    // 🔴 UNE IMAGE INTROUVABLE SE DIT, UNE FOIS. Elle se retient, donc cette
    // ligne ne se répète pas — et sans elle, un fond noir ne se distingue pas
    // d'un décodage raté, d'un chemin mal encodé ou d'une archive absente.
    core::LogWarning("image : introuvable — {}", key);
    return image;
  }
  // ⚠ PAR L'EXTENSION ET NON PLUS EN BMP D'OFFICE : une illustration de cet
  // endroit peut être un JPEG — celle de l'écran de connexion en est une.
  auto decoded = formats::DecodeImage(key, *bytes);
  if (!decoded) {
    core::LogWarning("image : {} — {}", key, core::Describe(decoded.error()));
    return image;
  }
  // 🔴 LA CLÉ MAGENTA NE VAUT QUE POUR LE BMP. Les bitmaps du client n'ont
  // aucun canal alpha : le magenta pur EST leur transparence, et c'est
  // antérieur à l'alpha. Un JPEG, lui, code AVEC PERTE — le magenta n'y ressort
  // pas pur, et des couleurs qui n'en étaient pas s'en approchent : la clé y
  // percerait des trous au hasard et en manquerait ailleurs.
  if (treatment != Treatment::kOpaque && core::AsciiLower(key).ends_with(kColorKeyedSuffix)) {
    formats::ApplyMagentaColorKey(*decoded);
  }
  if (treatment == Treatment::kForDarkPaper) {
    formats::RemapForDarkPaper(*decoded);
  } else if (treatment == Treatment::kMatted) {
    formats::MattePaperBackground(*decoded);
  } else if (treatment == Treatment::kInkShape) {
    formats::ReduceToInkShape(*decoded);
  }
  // ⚠ `kSprite` ET NON `kPixelArt` : une image de script est une IMAGE SEULE,
  // mise à l'échelle de la place qu'elle a, et non de l'art au pixel qu'on
  // étire. Elle porte la même couleur clé, et ne se répète pas.
  auto texture = GlTexture::Upload(*decoded, GlTexture::Filtering::kSprite);
  if (!texture) {
    core::LogWarning("image : {} — téléversement refusé ({})", key,
                     core::Describe(texture.error()));
    return image;
  }
  image.texture = std::move(*texture);
  image.width_px = decoded->width;
  image.height_px = decoded->height;
  return image;
}

}  // namespace ro::render
