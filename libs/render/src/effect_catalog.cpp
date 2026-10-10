#include "ro/render/effect_catalog.h"

#include <string>
#include <utility>

#include "ro/formats/str.h"
#include "ro/vfs/encoding.h"

namespace ro::render {

std::string EffectFolderOf(std::string_view path) {
  const std::size_t cut = path.find_last_of("\\/");
  return cut == std::string_view::npos ? std::string() : std::string(path.substr(0, cut + 1));
}

const LoadedEffect* EffectCatalog::Load(std::string_view path) {
  const auto known = effects_.find(path);
  if (known != effects_.end()) {
    return known->second.has_value() ? &*known->second : nullptr;
  }

  const auto remember = [&](std::optional<LoadedEffect> loaded) -> const LoadedEffect* {
    const auto placed = effects_.emplace(std::string(path), std::move(loaded)).first;
    return placed->second.has_value() ? &*placed->second : nullptr;
  };

  auto bytes = vfs_->Read(path);
  if (!bytes) {
    return remember(std::nullopt);
  }
  auto effect = formats::StrEffect::Parse(*bytes);
  if (!effect) {
    return remember(std::nullopt);
  }

  LoadedEffect loaded;
  loaded.prepared = PrepareEffect(*effect);
  const std::string folder = EffectFolderOf(path);
  loaded.textures.reserve(loaded.prepared.texture_names.size());
  loaded.texture_paths.reserve(loaded.prepared.texture_names.size());
  for (const std::string& name : loaded.prepared.texture_names) {
    // ⚠ DEUX ENCODAGES SE REJOIGNENT ICI, et les coudre tels quels donne un
    // chemin qui n'est ni l'un ni l'autre : le DOSSIER vient de nos tables,
    // donc en UTF-8, et le NOM vient des octets du `.str`, donc en CP949. Le
    // nom passe en UTF-8 avant la couture, et le tout par `Read` — c'est la
    // règle de `ro/vfs/encoding.h` : au-dessus de `vfs`, plus rien ne manipule
    // de CP949. Un nom intraduisible garde ses octets, ce qui laisse au moins
    // les dossiers ASCII fonctionner comme avant.
    auto utf8_name = vfs::Cp949ToUtf8(name);
    loaded.texture_paths.push_back(folder + (utf8_name ? *utf8_name : name));
    auto texture = vfs_->Read(loaded.texture_paths.back());
    if (!texture) {
      loaded.textures.emplace_back();
      continue;
    }
    auto image = formats::DecodeImage(name, *texture);
    if (!image) {
      loaded.textures.emplace_back();
      continue;
    }
    // LA TRANSPARENCE D'UN FICHIER D'EFFET N'EST PAS CELLE D'UN SPRITE : le jeu y
    // efface le NOIR en plus du magenta, à la précision de sa texture de seize
    // bits. Cinquante-quatre fichiers peignent une BMP à fond noir sous un
    // mélange qui le montre — le bouclier de Smite, les boîtes vides de Tetra
    // Vortex et de Cannon Spear —, et ce fond ne se voit pas en jeu.
    formats::ApplyEffectFileColorKey(*image);
    // Et la texture n'a que seize bits : ses dégradés se font en paliers.
    formats::QuantizeToEffectTextureDepth(*image);
    loaded.textures.push_back(std::move(*image));
  }
  return remember(std::move(loaded));
}

}  // namespace ro::render
