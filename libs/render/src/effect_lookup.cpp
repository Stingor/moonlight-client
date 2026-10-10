#include <algorithm>
#include <cstddef>

#include "ro/render/effect_table.h"

namespace ro::render {
namespace {

// Le marqueur qu'un chemin à variantes porte à la place de son numéro.
constexpr std::string_view kVariantMark = "%d";
// Les variantes se numérotent à partir de UN : le client tire un reste de zéro
// à deux, puis ajoute un.
constexpr std::uint32_t kFirstVariant = 1;

// L'entrée d'un identifiant dans une table, ou nulle.
const EffectEntry* Lookup(std::span<const EffectEntry> table, std::uint32_t effect_id) {
  const auto found = std::ranges::lower_bound(table, effect_id, {}, &EffectEntry::id);
  return found != table.end() && found->id == effect_id ? &*found : nullptr;
}

}  // namespace

std::string EffectFile(std::uint32_t effect_id, std::uint32_t draw) {
  const EffectEntry* found = Lookup(EffectTable(), effect_id);
  if (found == nullptr) {
    // Le SECOND résolveur du client, celui des effets de compétence.
    found = Lookup(EffectPrimitiveTable(), effect_id);
  }
  if (found == nullptr) {
    return {};
  }
  if (found->variants == 0) {
    return std::string(found->path);
  }
  const std::size_t mark = found->path.find(kVariantMark);
  if (mark == std::string_view::npos) {
    // Un chemin qui annonce des variantes sans porter leur marque n'en a pas :
    // mieux vaut le chemin tel quel qu'aucun effet.
    return std::string(found->path);
  }
  const std::uint32_t variant = draw % found->variants + kFirstVariant;
  std::string path(found->path.substr(0, mark));
  path += std::to_string(variant);
  path.append(found->path.substr(mark + kVariantMark.size()));
  return path;
}

}  // namespace ro::render
