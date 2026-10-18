#pragma once

#include <cstdint>
#include <functional>

#include "ro/sim/card_composition.h"
#include "ro/sim/inventory.h"
#include "ro/ui/card_drop.h"
#include "ro/ui/inventory_window.h"

namespace moonclient {

// SERTIR UNE CARTE PAR GLISSER-DÉPOSER : le jugement, et ce qu'il fait partir.
//
// 🔴 CE QUE LE SERVEUR EXIGE. La demande de sertissage se suffit à elle-même :
// il n'attend ni liste de pièces préalable, ni carte « en cours d'emploi ». Il
// la refuse pendant un échange, et pour toute pièce que son prédicat de
// sertissage écarte — pas une arme ni une armure, non identifiée, forgée ou
// signée, emplacement de la carte étranger à la pièce, PORTÉE, sans case
// libre. ⚠ CHACUN DE CES REFUS EST MUET : aucun accusé ne revient. C'est
// pourquoi le client les dit lui-même, AVANT d'envoyer.
//
// ⚠ LE CLIENT NE CONNAÎT PAS L'EMPLACEMENT D'UNE CARTE. Cette règle-là se
// demande au serveur, par la question du sertissage rapide : « quelles cartes
// cette pièce accepte-t-elle ? », à laquelle il répond par son propre
// prédicat, liste vide comprise. Le reste se lit sur l'objet.

// LE VERDICT, dans l'ordre où il se prononce : la première raison l'emporte.
enum class CardDropVerdict : std::uint8_t {
  kAccepted,
  // Tout ce qui se lit sur l'objet passe ; la réponse du serveur manque.
  kPending,
  // L'une des deux places ne désigne plus l'objet visé : rien à dire.
  kVanished,
  kDealing,
  kWorn,
  kUnidentified,
  kForged,
  kNoSocket,
  kFull,
  kIncompatible,
};

// CE QUE LE JUGEMENT LIT.
struct CardDropFacts {
  const ro::sim::Inventory* bag = nullptr;
  // La réponse du serveur à la question du sertissage rapide.
  const ro::sim::CompatibleCards* compatible = nullptr;
  // Un échange est ouvert : le serveur jette toute demande de sertissage.
  bool dealing = false;
  // Les emplacements de carte d'un objet, selon la table du client.
  std::function<int(std::uint32_t item_id)> card_slots_of;
};

[[nodiscard]] CardDropVerdict JudgeCardDrop(const ro::ui::CardDropAim& aim,
                                            const CardDropFacts& facts);

// LA RAISON D'UN REFUS, telle que le joueur la lit. `nullptr` pour ce qui
// n'en est pas un : accepté, en attente, ou objet disparu.
[[nodiscard]] const char* CardDropRefusalText(CardDropVerdict verdict);

// LA DEMANDE QUE FAIT PARTIR UN DÉPÔT ACCEPTÉ : celle du sertissage rapide du
// sac, mot pour mot. Le même verbe passe par le même `InventoryView::Apply` —
// demande, carte et pièce retenues pour l'accusé, liste rapide oubliée.
[[nodiscard]] ro::ui::InventoryWindowAction CardDropAsQuickInsert(const ro::ui::CardDropAim& aim);

// Combien de temps un dépôt attend la réponse du serveur, en millisecondes.
// Passé ce délai, la demande part quand même : le serveur reste juge, et un
// geste qui ne fait rien serait pire qu'un refus qu'il tait.
inline constexpr std::uint32_t kCardDropWaitMs = 2000;

// CE QU'UNE IMAGE FAIT DU GLISSER.
struct CardDropStep {
  // Ce que les deux fenêtres montrent à l'image suivante.
  ro::ui::CardDropHint hint;
  // La pièce sur laquelle poser la question du sertissage rapide, ou zéro.
  std::uint16_t ask_compatible = 0;
  // La demande à envoyer. Rien si la paire est vide.
  ro::ui::CardDropAim order;
  // Le refus à écrire dans la conversation, ou `nullptr`.
  const char* refusal = nullptr;
};

// LE GLISSER D'UNE IMAGE À L'AUTRE : le seul état est un dépôt qui attend la
// réponse du serveur.
class CardDropFlow {
 public:
  // `bag_seen` et `doll_seen` : ce que le sac et la fiche du personnage ont vu.
  [[nodiscard]] CardDropStep Step(const ro::ui::CardDropSeen& bag_seen,
                                  const ro::ui::CardDropSeen& doll_seen, const CardDropFacts& facts,
                                  std::uint32_t now_ms);

  [[nodiscard]] const ro::ui::CardDropAim& waiting() const noexcept { return waiting_; }

 private:
  // Tranche un dépôt. Vrai s'il reste en attente.
  bool Settle(const ro::ui::CardDropAim& aim, CardDropVerdict verdict, CardDropStep& step);

  ro::ui::CardDropAim waiting_;
  std::uint32_t waiting_since_ms_ = 0;
};

}  // namespace moonclient
