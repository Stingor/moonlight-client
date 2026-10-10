#pragma once

#include <cstdint>
#include <optional>

namespace ro::sim {

// LES RÉGLAGES QUE LE SERVEUR TIENT POUR LE PERSONNAGE, tels qu'il les a dits.
//
// ⚠ TROIS SONT SUIVIS ICI : « Montrer mon équipement », « Voir les costumes » et
// l'appel d'urgence. L'alimentation des compagnons est lue par leurs propres
// fiches.
//
// ⚠ HORS DU MONDE, comme les titres et la guilde, et pour la même raison : le
// monde touche son plafond de lignes. Et c'est un état de SESSION : le serveur
// n'annonce l'équipement qu'à l'entrée sur son serveur de carte, si bien qu'un
// état rangé avec la carte serait perdu au premier voyage. Les costumes, eux,
// reviennent à chaque carte.
//
// ⚠ À NE PAS CONFONDRE AVEC `MapMode::costumes_hidden`, qui est la règle de la
// CARTE : le serveur y masque les costumes quel que soit le réglage.
class PlayerConfig {
 public:
  // Les autres joueurs peuvent-ils voir notre équipement ? Rien tant que le
  // serveur ne l'a pas dit.
  [[nodiscard]] const std::optional<bool>& equipment_shown() const noexcept {
    return equipment_shown_;
  }

  // Combien de fois le serveur a dit l'état de notre équipement. ⚠ CHAQUE
  // ANNONCE COMPTE, même quand la valeur ne change pas : c'est la réponse à un
  // clic, et elle s'écrit dans la conversation.
  [[nodiscard]] std::uint32_t equipment_announcements() const noexcept {
    return equipment_announcements_;
  }

  // Nos costumes se montrent-ils — à nous comme aux autres ? Rien tant que le
  // serveur ne l'a pas dit.
  [[nodiscard]] const std::optional<bool>& costumes_shown() const noexcept {
    return costumes_shown_;
  }

  void AnnounceEquipmentShown(bool shown) noexcept {
    equipment_shown_ = shown;
    ++equipment_announcements_;
  }

  void AnnounceCostumesShown(bool shown) noexcept { costumes_shown_ = shown; }

  // L'appel d'urgence (`/call`) peut-il nous atteindre ? Rien tant que le
  // serveur ne l'a pas dit.
  [[nodiscard]] const std::optional<bool>& callable() const noexcept { return callable_; }

  // Combien de fois le serveur l'a dit. ⚠ CHAQUE ANNONCE COMPTE, comme pour
  // l'équipement : chacune s'écrit dans la conversation.
  [[nodiscard]] std::uint32_t call_announcements() const noexcept { return call_announcements_; }

  void AnnounceCallable(bool callable) noexcept {
    callable_ = callable;
    ++call_announcements_;
  }

 private:
  std::optional<bool> equipment_shown_;
  std::uint32_t equipment_announcements_ = 0;
  std::optional<bool> costumes_shown_;
  std::optional<bool> callable_;
  std::uint32_t call_announcements_ = 0;
};

}  // namespace ro::sim
