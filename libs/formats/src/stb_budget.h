#pragma once

#include <cstddef>
#include <cstdint>

namespace ro::formats::detail {

// ── LA MÉMOIRE DE `stb_image`, BORNÉE LE TEMPS D'UN DÉCODAGE ────────────────
//
// `stb_image` alloue d'après ce que le flux lui dit : un PNG d'un pixel dont
// le flux compressé se dilate en un gigaoctet de zéros lui fait allouer un
// gigaoctet, et l'en-tête n'en laisse rien voir. Ses allocations passent donc
// par ici (`STBI_MALLOC`, `STBI_REALLOC_SIZED`, `STBI_FREE`, posés dans
// `stb_image_impl.cpp`).
//
// ⚠ HORS D'UNE PORTÉE `StbBudget`, RIEN NE CHANGE : les images du jeu se
// décodent sans borne, comme avant. Seul le décodage d'une image venue du web
// ouvre une portée.
//
// ⚠ UN ÉTAT PAR FIL (`thread_local`), et c'est la seule forme possible : les
// macros de `stb_image` n'ont pas d'argument de contexte. Chaque fil de
// téléchargement a donc sa borne, sans rien partager.

void* StbAllocate(std::size_t bytes);
void* StbReallocate(void* block, std::size_t bytes);
void StbRelease(void* block);

// Une portée de décodage borné : au-delà de `most_bytes` vivants alloués
// depuis son ouverture, l'allocation échoue, et `stb_image` rend son erreur.
class StbBudget {
 public:
  explicit StbBudget(std::size_t most_bytes) noexcept;
  StbBudget(const StbBudget&) = delete;
  StbBudget& operator=(const StbBudget&) = delete;
  ~StbBudget();

  // Une allocation a-t-elle été refusée dans cette portée ?
  [[nodiscard]] bool exceeded() const noexcept;

 private:
  // La portée englobante, rétablie à la fermeture.
  bool outer_armed_ = false;
  std::size_t outer_most_bytes_ = 0;
  std::int64_t outer_base_bytes_ = 0;
  bool outer_exceeded_ = false;
};

}  // namespace ro::formats::detail
