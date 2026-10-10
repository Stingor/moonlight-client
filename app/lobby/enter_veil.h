#pragma once

#include <cstdint>

namespace moonclient::lobby {

// LE VOILE NOIR DE L'ENTRÉE EN JEU, câblé sur des ÉTATS et non sur une durée.
//
// 🔴 LA CARTE SE CHARGE EN BLOQUANT, et la dernière image présentée reste à
// l'écran tout ce temps. La main ne doit donc passer à la partie qu'une fois
// une image ENTIÈREMENT noire dessinée : c'est elle qui couvre le chargement,
// et elle ne se lève qu'à la première image du jeu — donc sur un jeu prêt,
// quelle que soit la durée du chargement.
//
// ⚠ UNE MINUTERIE SEULE LE PRÉSUMERAIT : une image lente sauterait
// l'animation, une rapide rendrait la main sur un voile à moitié monté. La
// durée ne règle ici que la vitesse à laquelle le noir monte.
class EnterVeil {
 public:
  enum class Stage : std::uint8_t {
    kNone,      // on n'entre pas
    kRising,    // le choix est parti, le voile monte
    kCovering,  // une image pleinement noire a été dessinée
  };

  // EN TÊTE D'IMAGE : `entering` vaut vrai de l'envoi du choix à la remise.
  //
  // ⚠ UN REFUS RAMÈNE À LA TABLÉE : le voile tombe, et repart de zéro au choix
  // suivant.
  void Follow(bool entering, std::uint32_t now_ms);

  // EN FIN D'IMAGE : ce qu'on vient de dessiner était-il pleinement noir ?
  // `fade_seconds` est la durée de l'animation.
  void Drawn(std::uint32_t now_ms, float fade_seconds);

  // Depuis combien de secondes le voile monte ; zéro hors de l'entrée.
  [[nodiscard]] float seconds(std::uint32_t now_ms) const;

  // LA MAIN PEUT-ELLE PASSER ? Seulement sur une image déjà noire.
  [[nodiscard]] bool covering() const noexcept { return stage_ == Stage::kCovering; }
  [[nodiscard]] Stage stage() const noexcept { return stage_; }

 private:
  Stage stage_ = Stage::kNone;
  std::uint32_t since_ms_ = 0;
};

}  // namespace moonclient::lobby
