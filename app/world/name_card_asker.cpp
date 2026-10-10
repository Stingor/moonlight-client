#include "world/name_card_asker.h"

namespace moonclient {
namespace {

// UN OBJET AU SOL ET UNE UNITÉ DE COMPÉTENCE N'ONT PAS DE CARTE : le serveur
// ne sait pas les nommer, et une demande n'y obtient qu'une ligne d'erreur
// dans son journal. Le nom d'un butin se compose chez nous.
[[nodiscard]] bool HasNameCard(ro::sim::ActorKind kind) {
  return kind != ro::sim::ActorKind::kItem && kind != ro::sim::ActorKind::kSkillUnit;
}

}  // namespace

std::uint32_t NameCardAsker::AskedFor(const ro::sim::Actor* hovered) {
  const std::uint32_t hovered_id = hovered != nullptr ? hovered->id : 0;
  if (hovered_id != asked_id_) {
    asked_id_ = 0;
  }
  if (hovered == nullptr || hovered->card.has_value() || !HasNameCard(hovered->kind) ||
      asked_id_ != 0) {
    return 0;
  }
  asked_id_ = hovered_id;
  return hovered_id;
}

}  // namespace moonclient
