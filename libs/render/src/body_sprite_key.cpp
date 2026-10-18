#include "ro/render/body_sprite_key.h"

#include <string>

#include "ro/core/ascii.h"
#include "ro/core/error.h"
#include "ro/vfs/encoding.h"

namespace ro::render {

std::uint32_t BodySpriteKey(std::string_view sprite_path) {
  // FNV-1a trente-deux bits. Choisie pour être trivialement réimplémentable
  // ailleurs : la clé voyage sur le fil et se range en base, donc elle
  // survivra à ce code.
  constexpr std::uint32_t kFnvOffset = 2166136261U;
  constexpr std::uint32_t kFnvPrime = 16777619U;
  constexpr std::string_view kSpriteSuffix = ".spr";

  // L'extension ne fait pas partie de l'identité : selon l'appelant, le chemin
  // est une BASE ou un fichier, et les deux désignent le même corps.
  // ⚠ COMPARÉE À LA CASSE ASCII PRÈS COMME TOUT LE RESTE : un `tolower` de la
  // bibliothèque dépend de la locale, et c'est précisément ce que cette
  // fonction existe pour éviter.
  std::string_view path = sprite_path;
  if (path.size() > kSpriteSuffix.size() && core::AsciiEndsWithFolded(path, kSpriteSuffix)) {
    path.remove_suffix(kSpriteSuffix.size());
  }
  if (path.empty()) {
    return 0;
  }

  // 🔴 LE CONDENSÉ SE PREND SUR LES OCTETS CP949, jamais sur nos octets UTF-8.
  // La référence hache le chemin tel que le CLIENT le tient, et le client est
  // en CP949 — « du CP949 au milieu », dit-elle. Tous les chemins de corps
  // portent du coréen : hacher l'UTF-8 donnerait une clé que personne d'autre
  // ne trouverait, donc une variante de couleurs jamais choisie. L'écart ne se
  // verrait qu'en jeu, et seulement à deux.
  const core::Result<std::string> cp949 = vfs::Utf8ToCp949(path);
  if (!cp949) {
    // Un chemin que le client ne saurait pas nommer ne désigne aucun corps.
    return 0;
  }

  std::uint32_t hash = kFnvOffset;
  for (const char letter : *cp949) {
    auto byte = static_cast<unsigned char>(letter);
    if (byte == '/') {
      byte = '\\';  // le client mélange les deux séparateurs
    }
    // 🔴 ASCII SEUL : voir l'en-tête, et `ro/core/ascii.h` qui porte la règle.
    byte = core::AsciiLower(byte);
    hash ^= byte;
    hash *= kFnvPrime;
  }
  // Zéro est la sentinelle « corps inconnu » : on la déplace plutôt que de la
  // rendre par accident sur un chemin parfaitement valide.
  return hash != 0 ? hash : 1U;
}

}  // namespace ro::render
