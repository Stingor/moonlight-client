#pragma once

#include <memory>

namespace moonclient {

class SurvivorsMode;

// LA PLACE DU MODE SURVIVORS DANS LA PARTIE, sans son en-tête : l'arène, ses
// effets et ses fenêtres tirent des centaines de déclarations, et `game.h`,
// inclus partout, n'a pas à les porter. Seuls les fichiers qui jouent le mode
// incluent `game/survivors_mode.h`.
//
// Le mode naît avec la place, vide de toute partie : il ne lit rien pour se
// construire.
class SurvivorsModeSlot {
 public:
  SurvivorsModeSlot();
  SurvivorsModeSlot(const SurvivorsModeSlot&) = delete;
  SurvivorsModeSlot& operator=(const SurvivorsModeSlot&) = delete;
  SurvivorsModeSlot(SurvivorsModeSlot&&) = delete;
  SurvivorsModeSlot& operator=(SurvivorsModeSlot&&) = delete;
  ~SurvivorsModeSlot();

  [[nodiscard]] SurvivorsMode& operator*() const noexcept { return *mode_; }
  [[nodiscard]] SurvivorsMode* operator->() const noexcept { return mode_.get(); }

 private:
  std::unique_ptr<SurvivorsMode> mode_;
};

}  // namespace moonclient
