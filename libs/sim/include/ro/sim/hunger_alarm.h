#pragma once

namespace ro::sim {

// LE PALIER DE FAIM OÙ UN COMPAGNON COMMENCE À PERDRE DE L'INTIMITÉ : le pet et
// l'homoncule le partagent, de zéro à dix sur cent.
inline constexpr int kCompanionStarvingHunger = 10;

// L'ALERTE DE FAIM D'UN COMPAGNON : elle sonne quand la faim DESCEND jusqu'au
// palier, une seule fois par descente.
//
// ⚠ ELLE SE RÉARME EN REMONTANT, et seulement ainsi : la faim d'un compagnon
// qui a faim est renvoyée à chaque baisse d'un point, et une alerte à chaque
// point ne dirait plus rien. Un compagnon inconnu la réarme aussi — le suivant
// qui arrive affamé doit la faire sonner.
class HungerAlarm {
 public:
  // Rend vrai à l'instant où l'alerte doit sonner.
  [[nodiscard]] bool Observe(bool known, int hunger) noexcept {
    if (!known || hunger > kCompanionStarvingHunger) {
      armed_ = true;
      return false;
    }
    const bool rings = armed_;
    armed_ = false;
    return rings;
  }

 private:
  bool armed_ = true;
};

}  // namespace ro::sim
