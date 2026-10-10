// Les lignes d'une étiquette d'acteur. Voir l'en-tête : la composition est
// celle du client, à la lettre.

#include "ro/render/name_plate.h"

#include <format>

namespace ro::render {
namespace {

// Ce qui coupe un nom de joueur. Le client le cherche par `memchr(nom, 35, …)`
// et tronque à cet endroit ; trente-cinq est le code du croisillon.
constexpr char kNameCut = '#';

// Assemble « base (suite) » ou « base [suite] ».
//
// ⚠ LES DEUX LIGNES N'EMPLOIENT LA MÊME PAIRE QU'EN SERVICE CORÉEN : ailleurs,
// la parenthèse va à la ligne du nom et le crochet à celle de la bannière. En
// service coréen, les deux prennent la parenthèse, suivie d'une espace que le
// centrage de l'étiquette compte comme un caractère.
[[nodiscard]] std::string WithTail(std::string_view base, std::string_view tail, bool brackets,
                                   const NamePlateLook& look) {
  if (look.korean_service) {
    return std::format("{} ({}) ", base, tail);
  }
  return brackets ? std::format("{} [{}]", base, tail) : std::format("{} ({})", base, tail);
}

// Ce que chaque ligne emploie, nommé plutôt que passé en booléen nu.
constexpr bool kParentheses = false;
constexpr bool kBrackets = true;

}  // namespace

std::vector<std::string> ComposeNamePlateLines(const NamePlateText& what,
                                               const NamePlateLook& look) {
  std::string_view name = what.name;
  if (what.cut_at_hash) {
    if (const std::size_t cut = name.find(kNameCut); cut != std::string_view::npos) {
      name = name.substr(0, cut);
    }
  }

  std::vector<std::string> lines;
  if (!name.empty()) {
    const std::string titled =
        what.title.empty() ? std::string(name) : std::format("[{}] {}", what.title, name);
    lines.push_back(what.aside.empty() ? titled : WithTail(titled, what.aside, kParentheses, look));
  }
  // ⚠ L'INSIGNE NE FAIT PAS DE LIGNE À LUI SEUL : le client ne regarde que la
  // bannière pour décider d'écrire la seconde ligne — et ne regarde PAS
  // l'insigne pour la composer, qui s'écrit donc même vide.
  if (!what.banner.empty()) {
    lines.push_back(WithTail(what.banner, what.badge, kBrackets, look));
  }
  return lines;
}

std::array<std::uint8_t, 4> NameColorForLevelGap(const std::array<std::uint8_t, 4>& by_kind,
                                                 int mine, int theirs) {
  if (mine <= 0 || theirs <= 0) {
    return by_kind;
  }
  const int gap = mine - theirs;
  if (gap < kOverLevelGap) {
    return kOverLevelNameColor;
  }
  if (gap > kUnderLevelGap) {
    return kUnderLevelNameColor;
  }
  return by_kind;
}

}  // namespace ro::render
