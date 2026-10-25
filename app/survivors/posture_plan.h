#pragma once

namespace moonclient {

// LA POSTURE DU PERSONNAGE CONNECTÉ PENDANT UNE PARTIE (U4) : il s'assoit à
// l'entrée et se relève à la sortie, sans rien dire d'autre au serveur. On ne
// touche qu'à ce que l'on a fait soi-même : un personnage déjà assis le reste
// à la sortie, et un personnage que le jeu a relevé entre-temps (un coup, une
// téléportation) ne se relève pas une seconde fois.
class PosturePlan {
 public:
  // L'entrée : vrai quand app doit demander à s'asseoir.
  [[nodiscard]] bool SitOnEntry(bool sitting_now) noexcept;
  // La sortie : vrai quand app doit demander à se relever. Le plan est
  // oublié ensuite.
  [[nodiscard]] bool StandOnExit(bool sitting_now) noexcept;
  [[nodiscard]] bool seated_by_us() const noexcept { return seated_by_us_; }

 private:
  bool seated_by_us_ = false;
};

}  // namespace moonclient
