// LA TÊTE D'UN JOUEUR : ses fichiers, et le seul cas où ses deux fichiers ne
// portent pas la même coupe.

#include <string>
#include <string_view>
#include <utility>

#include "ro/render/sprite_catalog.h"

#include "sprite_catalog_internal.h"

namespace ro::render::catalog_internal {
namespace {

// La coupe que le client du joueur RÉÉCRIT dans un Doram qui n'en a pas.
constexpr std::uint16_t kDoramHairRewritten = 2;

// LE VISAGE D'ORC, parmi les sprites d'effet : un pour les humains, un pour les
// Doram.
constexpr std::string_view kOrcFacePath = "data\\sprite\\이팩트\\orcface";
constexpr std::string_view kDoramOrcFacePath = "data\\sprite\\이팩트\\orcface_doram";

}  // namespace

ActorPieceFiles HeadPieceFiles(const ActorLook& look) {
  // 🔴 LA TÊTE D'ORC REMPLACE LA TÊTE, animation comprise, et sans palette : le
  // client charge ce couple à la place de celui de la coupe dès que le bit 0x800
  // des options est posé.
  if (look.orc_head) {
    return ActorPieceFiles{
        .part = ActorPart::kHead,
        .candidates = {std::string(IsDoram(look.job) ? kDoramOrcFacePath : kOrcFacePath)}};
  }
  ActorPieceFiles head{.part = ActorPart::kHead,
                       .candidates = {HeadSpritePath(look)},
                       .palette_path = HairPalettePath(look)};
  // 🔴 UN DORAM SANS COUPE : le client du joueur cherche le .act de la dixième
  // coupe, et, ce faisant, réécrit la coupe de l'acteur à DEUX ; il cherche
  // ensuite le .spr avec cette coupe-là. La tête prend donc l'animation de la
  // dixième et l'image de la deuxième, jusqu'à ce que la coupe change.
  if (IsDoram(look.job) && look.hair < kDoramHairFirst) {
    ActorLook rewritten = look;
    rewritten.hair = kDoramHairRewritten;
    head.animation_path = std::move(head.candidates.front());
    head.candidates = {HeadSpritePath(rewritten)};
  }
  return head;
}

}  // namespace ro::render::catalog_internal
