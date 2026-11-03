#pragma once

#include <memory>

namespace moonclient {

struct SurvivorsPresence;

// LA PLACE DE LA PRÉSENCE EN ARÈNE DANS LA PARTIE, sans son en-tête, pour la
// raison de `SurvivorsModeSlot` : `game.h`, inclus partout, n'a pas à porter
// le fil de la présence ni ses libellés. Seuls les fichiers qui s'en servent
// incluent `game/survivors_presence_mode.h`.
class SurvivorsPresenceSlot {
 public:
  SurvivorsPresenceSlot();
  SurvivorsPresenceSlot(const SurvivorsPresenceSlot&) = delete;
  SurvivorsPresenceSlot& operator=(const SurvivorsPresenceSlot&) = delete;
  SurvivorsPresenceSlot(SurvivorsPresenceSlot&&) = delete;
  SurvivorsPresenceSlot& operator=(SurvivorsPresenceSlot&&) = delete;
  ~SurvivorsPresenceSlot();

  [[nodiscard]] SurvivorsPresence& operator*() const noexcept { return *presence_; }
  [[nodiscard]] SurvivorsPresence* operator->() const noexcept { return presence_.get(); }

 private:
  std::unique_ptr<SurvivorsPresence> presence_;
};

}  // namespace moonclient
