#pragma once

#include <cstdint>

namespace ro::render {

// LE TIRAGE D'UN BRUIT QUI NE SONNE QU'UNE FOIS SUR N.
//
// Certains bruits ne s'entendent qu'une fois sur N : celui du bourbier une fois
// sur douze, celui d'une unité de feu du ninja une fois sur huit. Le client tire
// À CHAQUE LANCEMENT, si bien que vingt-cinq cellules nées dans la même image
// tirent vingt-cinq fois, et que quelques-unes seulement sonnent.
//
// ⚠ UN TIRAGE TIRÉ DE L'INSTANT NE CONVIENT PAS : tous les lancements d'une même
// image auraient le même, et sonneraient tous ou aucun. La suite est donc
// propre à qui la possède et avance d'un pas par tirage ; elle part d'une
// graine donnée, ce qui la rend rejouable sous test.
class SoundChance {
 public:
  explicit SoundChance(std::uint32_t seed) : generator_(seed) {}

  // Vrai si le bruit s'entend à ce lancement. `one_in` à zéro ou un veut dire
  // à coup sûr, et ne consomme aucun tirage ; au-delà, chaque appel tire.
  [[nodiscard]] bool Heard(std::uint32_t one_in);

 private:
  std::uint32_t generator_ = 0;
};

}  // namespace ro::render
