// L'habit du GM : le corps et l'épée qui remplacent ceux du métier.

#include <string>
#include <string_view>

#include "ro/render/sprite_catalog.h"

#include "sprite_catalog_internal.h"

namespace ro::render::catalog_internal {
namespace {

constexpr std::string_view kGmBodyFolder = "몸통";
constexpr std::string_view kGmName = "운영자";
// L'épée se range dans un dossier à elle, sous le nom de l'habit.
constexpr std::string_view kGmSwordSuffix = "_검";

}  // namespace

bool WearsGmOutfit(const ActorLook& look) { return look.player && look.gm_outfit; }

// ⚠ TOUJOURS SOUS LA RACE HUMAINE : le client n'a qu'un habit, et un Doram le
// porte comme un humain.
std::string GmBodyPath(std::uint8_t sex) {
  const std::string_view token = SexToken(sex);
  return Join({kSpriteRoot, kHumanRace, kSeparator, kGmBodyFolder, kSeparator, token, kSeparator,
               kGmName, "_", token});
}

std::string GmSwordPath(std::uint8_t sex) {
  return Join({kSpriteRoot, kHumanRace, kSeparator, kGmName, kSeparator, kGmName, "_",
               SexToken(sex), kGmSwordSuffix});
}

}  // namespace ro::render::catalog_internal
