// Point d'implémentation de `stb_image`, pour TOUT le programme, et son
// allocateur.
//
// 🔴 UN SEUL DANS LE BINAIRE, ET C'EST UNE CONTRAINTE DU PROCÉDÉ. La macro
// déploie les définitions ; deux unités qui la posent donnent des symboles en
// double à l'édition de liens. `roc-cli` la posait pour lui seul ; elle vit ici
// depuis que le CLIENT décode aussi des JPEG, et l'outil s'y raccorde.
//
// ⚠ CE FICHIER NE CONTIENT QUE CELA — le point d'implémentation et la mémoire
// qu'il emploie —, comme `image_write.cpp` de l'outil : les milliers de lignes
// de C que la macro déploie se recompileraient à chaque unité qui lit une
// image.
//
// ⚠ EN INCLUSION SYSTÈME : ce code n'est pas le nôtre, et `/WX` ferait de ses
// avertissements des erreurs de compilation.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

#include "ro/formats/web_picture.h"

#include "stb_budget.h"

namespace ro::formats::detail {
namespace {

// L'EN-TÊTE DE CHAQUE BLOC : sa taille, pour que la libération sache ce
// qu'elle rend. Aligné comme `malloc` aligne, pour que le bloc qui suit le
// soit aussi.
struct alignas(std::max_align_t) BlockHeader {
  std::size_t bytes = 0;
};

// L'état du fil : les octets vivants, et la portée ouverte s'il y en a une.
struct ThreadBudget {
  std::int64_t live_bytes = 0;
  bool armed = false;
  std::size_t most_bytes = 0;
  std::int64_t base_bytes = 0;
  bool exceeded = false;
};

thread_local ThreadBudget t_budget;

// L'allocation de `grow` octets de plus tient-elle dans la portée ouverte ?
[[nodiscard]] bool Fits(std::size_t grow) {
  if (!t_budget.armed) {
    return true;
  }
  const std::int64_t used = t_budget.live_bytes - t_budget.base_bytes;
  const auto most = static_cast<std::int64_t>(t_budget.most_bytes);
  if (grow > static_cast<std::size_t>(most) || used + static_cast<std::int64_t>(grow) > most) {
    t_budget.exceeded = true;
    return false;
  }
  return true;
}

[[nodiscard]] BlockHeader* HeaderOf(void* block) { return static_cast<BlockHeader*>(block) - 1; }

}  // namespace

void* StbAllocate(std::size_t bytes) {
  if (bytes > SIZE_MAX - sizeof(BlockHeader) || !Fits(bytes)) {
    return nullptr;
  }
  void* raw = std::malloc(sizeof(BlockHeader) + bytes);
  if (raw == nullptr) {
    return nullptr;
  }
  auto* header = ::new (raw) BlockHeader{bytes};
  t_budget.live_bytes += static_cast<std::int64_t>(bytes);
  return header + 1;
}

void* StbReallocate(void* block, std::size_t bytes) {
  if (block == nullptr) {
    return StbAllocate(bytes);
  }
  const std::size_t old_bytes = HeaderOf(block)->bytes;
  if (bytes > SIZE_MAX - sizeof(BlockHeader) || (bytes > old_bytes && !Fits(bytes - old_bytes))) {
    // Le bloc d'origine reste valide : `stb_image` le libère lui-même.
    return nullptr;
  }
  void* raw = std::realloc(HeaderOf(block), sizeof(BlockHeader) + bytes);
  if (raw == nullptr) {
    return nullptr;
  }
  auto* header = static_cast<BlockHeader*>(raw);
  header->bytes = bytes;
  t_budget.live_bytes += static_cast<std::int64_t>(bytes) - static_cast<std::int64_t>(old_bytes);
  return header + 1;
}

void StbRelease(void* block) {
  if (block == nullptr) {
    return;
  }
  BlockHeader* header = HeaderOf(block);
  t_budget.live_bytes -= static_cast<std::int64_t>(header->bytes);
  std::free(header);
}

StbBudget::StbBudget(std::size_t most_bytes) noexcept
    : outer_armed_(t_budget.armed),
      outer_most_bytes_(t_budget.most_bytes),
      outer_base_bytes_(t_budget.base_bytes),
      outer_exceeded_(t_budget.exceeded) {
  t_budget.armed = true;
  t_budget.most_bytes = most_bytes;
  t_budget.base_bytes = t_budget.live_bytes;
  t_budget.exceeded = false;
}

StbBudget::~StbBudget() {
  t_budget.armed = outer_armed_;
  t_budget.most_bytes = outer_most_bytes_;
  t_budget.base_bytes = outer_base_bytes_;
  t_budget.exceeded = outer_exceeded_ || t_budget.exceeded;
}

bool StbBudget::exceeded() const noexcept { return t_budget.exceeded; }

}  // namespace ro::formats::detail

// ⚠ UN CÔTÉ PLAFONNÉ : un en-tête forgé annonce des dimensions que rien ne
// porte, et `stb_image` allouerait d'après lui. Aucune image du client ni
// capture de l'outil n'approche ce côté.
#define STBI_MAX_DIMENSIONS 16384
static_assert(STBI_MAX_DIMENSIONS == ro::formats::kWebPictureMostSide);
#define STBI_MALLOC(bytes) ::ro::formats::detail::StbAllocate(bytes)
#define STBI_REALLOC(block, bytes) ::ro::formats::detail::StbReallocate(block, bytes)
#define STBI_FREE(block) ::ro::formats::detail::StbRelease(block)
#define STB_IMAGE_IMPLEMENTATION
// ⚠ LES MACROS D'ALLOCATION SONT ÉCRITES ICI, hors de l'inclusion système : le
// transtypage à la C que `stb_image` pose devant chacune serait attribué à ce
// fichier. Le code reste le sien, l'avertissement se tait pour lui seul.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include <stb_image.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
