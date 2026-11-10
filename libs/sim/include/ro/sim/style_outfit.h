#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/sim/style.h"

namespace ro::sim {

// LES TENUES : le corps et les calques d'un joueur, d'un seul tenant.
//
// Une tenue se partage par un CODE COURT que le serveur attribue, et se range
// dans la GARDE-ROBE du compte, que tous ses personnages relisent. Le fil est
// dans `ro/net/style_outfits.h` ; ici, ce que le serveur a répondu.
//
// 🔴 UNE TENUE N'EST QU'UNE PROPOSITION. Rien de ce qu'elle porte ne s'écrit
// sur le personnage sans passer par l'éditeur et sa validation ordinaire : un
// code collé ne repeint personne, et une teinture d'objet reste à valider
// auprès du styliste.

// Les huit signes d'un code court.
inline constexpr std::size_t kOutfitCodeLength = 8;

struct StyleOutfit {
  // Les corps habillés, dans l'ordre de la tenue, chacun sous sa clé.
  std::vector<std::pair<std::uint32_t, StyleRecipe>> bodies;
  // Le corps principal, ou zéro.
  std::uint32_t fallback_body = 0;
  // Les teintures de calques, sous les clés AFFICHÉES : l'apparence d'une
  // coiffe, la robe d'une cape. Elles valent pour l'objet qui montre cette
  // apparence, pas pour un exemplaire.
  std::vector<StyleLayerRecipe> layers;

  [[nodiscard]] bool empty() const noexcept { return bodies.empty() && layers.empty(); }
};

// Un rangement de la garde-robe.
struct StyleWardrobeEntry {
  std::uint8_t slot = 0;
  // En UTF-8, tel que le joueur l'a tapé.
  std::string name;
  StyleOutfit outfit;
};

enum class OutfitAction : std::uint8_t {
  kPublish = 1,
  kFetch = 2,
  kWardrobeList = 3,
  kWardrobeStore = 4,
  kWardrobeClear = 5,
};

// Les résultats : ceux du verdict des couleurs, plus deux propres aux tenues.
enum class OutfitResult : std::uint8_t {
  kAccepted = 0,
  kTooSoon = 2,
  kMalformed = 3,
  kCap = 4,
  kUnavailable = 9,
  kUnknownCode = 10,
  kEmptySlot = 11,
};

// CE QUE LE SERVEUR A RÉPONDU à une demande.
struct OutfitReply {
  OutfitAction action = OutfitAction::kPublish;
  // Le résultat tel qu'il est venu : un numéro que ce client ne nomme pas
  // reste un refus.
  std::uint8_t raw_result = 0;
  std::uint16_t wait_ms = 0;
  std::uint8_t slot = 0;
  // Le code publié, ou celui qu'on a demandé ; vide sinon.
  std::string code;
  // La tenue d'un code demandé, quand il existe et se lit.
  std::optional<StyleOutfit> outfit;
  // Le numéro de cette réponse : il avance à chacune. Zéro : aucune.
  std::uint32_t sequence = 0;

  [[nodiscard]] bool accepted() const noexcept {
    return raw_result == static_cast<std::uint8_t>(OutfitResult::kAccepted);
  }
};

// LE GUICHET DES TENUES : la dernière réponse, et la garde-robe telle que le
// serveur l'a listée en dernier.
class StyleOutfitDesk {
 public:
  // Une liste acceptée REMPLACE la garde-robe connue.
  void Hear(OutfitReply reply, std::vector<StyleWardrobeEntry> wardrobe = {});

  [[nodiscard]] const OutfitReply& last() const noexcept { return last_; }
  [[nodiscard]] const std::vector<StyleWardrobeEntry>& wardrobe() const noexcept {
    return wardrobe_;
  }
  // Le numéro de la dernière liste reçue ; zéro tant qu'aucune n'est venue.
  [[nodiscard]] std::uint32_t wardrobe_sequence() const noexcept { return wardrobe_sequence_; }
  [[nodiscard]] const StyleWardrobeEntry* Slot(std::uint8_t slot) const;

 private:
  OutfitReply last_;
  std::vector<StyleWardrobeEntry> wardrobe_;
  std::uint32_t wardrobe_sequence_ = 0;
  std::uint32_t last_sequence_ = 0;
};

}  // namespace ro::sim
