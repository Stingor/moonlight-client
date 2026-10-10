#pragma once

// L'HOMONCULE DANS LA BOUCLE DE LA PARTIE : ce que le serveur en dit, ce que son
// onglet de la fiche montre, et ce que ses gestes deviennent sur le fil.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/homunculus.h"
#include "ro/sim/hunger_alarm.h"
#include "ro/sim/world.h"
#include "ro/ui/description_window.h"
#include "ro/ui/homunculus_tab.h"

#include "game/companion_aim.h"
#include "game/homunculus_staff_words.h"
#include "world/item_obtain_toast.h"

namespace moonclient {

struct GamePieces;

class HomunculusStage {
 public:
  // Lit une trame du serveur ; `kUnhandled` si elle ne le regarde pas. Les
  // accusés — nourrissage, auto-alimentation, faim — s'écrivent dans la
  // conversation, comme chez le client.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                                 const GamePieces& pieces, std::uint32_t now_ms);

  // Compose la vue de l'onglet, à chaque image, avant le dessin.
  void Stage(const ro::sim::World& world, GamePieces& pieces);
  [[nodiscard]] const ro::ui::HomunculusTabView& view() const noexcept { return view_; }

  // Ce que l'onglet a demandé, mis sur le fil. Une description demandée ouvre
  // la fenêtre des compétences.
  void Apply(const ro::ui::HomunculusTabAction& asked, ro::net::Connection& link,
             ro::sim::World& world, std::uint32_t now_ms,
             ro::ui::SkillDescriptionWindowState& description);

  // ── POUR LE MENU D'ENTITÉ ET LES RACCOURCIS ───────────────────────────────
  //
  // L'acteur de NOTRE homoncule, zéro sans homoncule.
  [[nodiscard]] std::uint32_t own_homunculus_id() const noexcept {
    return homunculus_.present() ? homunculus_.actor_id() : 0;
  }
  // Au repos : rentré, rien à nourrir.
  [[nodiscard]] bool resting() const noexcept { return homunculus_.sheet().resting; }
  // Nourrir, comme le bouton de l'onglet.
  void Feed(ro::net::Connection& link);

  // ── POUR LA BARRE DE RACCOURCIS ───────────────────────────────────────────
  //
  // Une compétence de NOTRE homoncule, ou rien.
  [[nodiscard]] const ro::sim::HomunculusSkill* FindSkill(std::uint16_t skill_id) const noexcept {
    return homunculus_.present() ? homunculus_.FindSkill(skill_id) : nullptr;
  }
  // La lance au niveau `level`, borné à l'appris. Rend vrai quand la demande
  // est partie.
  //
  // ⚠ SEULE CELLE QUI SE LANCE SUR SOI PART D'ICI. Celle qui vise une cible ou
  // une case s'ARME, voir `TakeAimRequest` ; un refus — passive, repos —
  // s'écrit dans la conversation.
  [[nodiscard]] bool UseSkill(ro::net::Connection& link, ro::sim::World& world,
                              std::uint16_t skill_id, int level, std::uint32_t now_ms);
  // LE SKILL QUI VIENT DE S'ARMER, pour l'IA du compagnon ; vide sinon.
  [[nodiscard]] std::optional<CompanionAim> TakeAimRequest() noexcept {
    return std::exchange(aim_request_, std::nullopt);
  }

  [[nodiscard]] const ro::sim::Homunculus& homunculus() const noexcept { return homunculus_; }

  // L'ALERTE DE FAIM qui vient de sonner, pour le bandeau ; vide sinon.
  [[nodiscard]] std::optional<ToastNotice> TakeHungerNotice() noexcept {
    return std::exchange(hunger_notice_, std::nullopt);
  }

 private:
  // Un autre personnage : ce qui précède appartenait au précédent.
  void FollowOwner(const ro::sim::World& world);
  // Une commande de la section « Staff » de l'onglet, sur le fil.
  void SendStaffCommand(const ro::ui::HomunculusTabAction& asked, ro::net::Connection& link,
                        const ro::sim::World& world);

  ro::sim::Homunculus homunculus_;
  // Le personnage dont l'homoncule est suivi.
  std::string owner_name_;
  // Le nom de ressource de l'espèce, relu quand l'acteur change de classe.
  std::uint32_t species_job_ = 0;
  std::string species_;
  std::vector<ro::ui::HomunculusSkillRow> rows_;
  ro::ui::HomunculusTabView view_;
  ro::sim::HungerAlarm hunger_alarm_;
  std::optional<ToastNotice> hunger_notice_;
  std::optional<CompanionAim> aim_request_;
  // Les formes que `@hommutate` peut donner : la vue de l'onglet pointe dedans.
  std::vector<ro::ui::StaffHomunculusChoice> mutations_ = MutatedHomunculi();
};

// DIT POURQUOI UNE COMPÉTENCE DE COMPAGNON NE PART PAS — homoncule comme
// mercenaire. Rien pour `kNow`.
void SayCompanionCastRefusal(ro::sim::World& world, ro::sim::CompanionCast cast,
                             std::uint32_t now_ms);

}  // namespace moonclient
