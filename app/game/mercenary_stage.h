#pragma once

// LE MERCENAIRE DANS LA BOUCLE DE LA PARTIE : ce que le serveur en dit, ce que son
// onglet de la fiche montre, et ce que ses gestes deviennent sur le fil.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/mercenary.h"
#include "ro/sim/world.h"
#include "ro/ui/description_window.h"
#include "ro/ui/mercenary_tab.h"

#include "game/companion_aim.h"

namespace moonclient {

struct GamePieces;

class MercenaryStage {
 public:
  // Lit une trame du serveur ; `kUnhandled` si elle ne le regarde pas. Le départ
  // annoncé par le serveur s'écrit dans la conversation.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                                 std::uint32_t now_ms);

  // Compose la vue de l'onglet, à chaque image, avant le dessin.
  void Stage(const ro::sim::World& world, GamePieces& pieces);
  [[nodiscard]] const ro::ui::MercenaryTabView& view() const noexcept { return view_; }

  // Ce que l'onglet a demandé, mis sur le fil. La description d'une compétence
  // s'ouvre dans la fenêtre de description du grimoire.
  //
  // ⚠ C'EST AUSSI LUI QUI SONNE LES RAPPELS DE FIN DE CONTRAT, à chaque image :
  // le serveur ne prévient pas avant la fin.
  void Apply(const ro::ui::MercenaryTabAction& asked, ro::net::Connection& link,
             ro::sim::World& world, std::uint32_t now_ms,
             ro::ui::SkillDescriptionWindowState& description);

  // L'identifiant de l'acteur de MON mercenaire, zéro sans mercenaire.
  [[nodiscard]] std::uint32_t own_mercenary_id() const noexcept;

  // « Statut du mercenaire » : la commande que le client envoie en ouvrant la
  // fiche. ⚠ LE SERVEUR L'IGNORE et rien ne revient : la fiche montrée est
  // celle déjà reçue.
  void RequestInfo(ro::net::Connection& link);

  // UNE COMPÉTENCE DU MERCENAIRE EST-ELLE CELLE-CI ? Pour la barre de
  // raccourcis, qui doit savoir à qui adresser une case.
  [[nodiscard]] const ro::sim::MercenarySkill* FindSkill(std::uint16_t skill_id) const {
    return book_.FindSkill(skill_id);
  }

  // LANCE une compétence du mercenaire. Celle qu'il se lance à lui-même part ;
  // celle qui vise une cible ou une case s'ARME, voir `TakeAimRequest`. Rend nul
  // quand la demande est partie ou armée, sinon la raison du refus, déjà
  // traduite, à dire au joueur. Un appelant qui a déjà trié la compétence peut
  // ignorer ce retour.
  const char* Cast(ro::net::Connection& link, std::uint16_t skill_id);
  // LE SKILL QUI VIENT DE S'ARMER, pour l'IA du compagnon ; vide sinon.
  [[nodiscard]] std::optional<CompanionAim> TakeAimRequest() noexcept {
    return std::exchange(aim_request_, std::nullopt);
  }

  // Ce que le serveur a dit du mercenaire, pour l'IA du compagnon.
  [[nodiscard]] const ro::sim::MercenaryBook& book() const noexcept { return book_; }

 private:
  ro::sim::MercenaryBook book_;
  // Le nombre d'entrées sur une carte déjà vues : une de plus fait oublier le
  // mercenaire, que le serveur renverra s'il est toujours là.
  std::uint32_t seen_map_entries_ = 0;
  // La classe de son acteur, retenue la dernière fois qu'il était en vue.
  std::uint16_t seen_job_ = 0;
  // ⚠ L'HORLOGE DE LA DERNIÈRE IMAGE : la composition n'en reçoit pas, et le
  // décompte du contrat se lit à la seconde.
  std::uint32_t last_now_ms_ = 0;

  ro::ui::MercenaryTabView view_;
  std::string name_;
  std::string kind_;
  std::string expires_at_;
  std::vector<std::string> skill_names_;
  std::vector<ro::ui::MercenarySkillRow> skill_rows_;
  std::optional<CompanionAim> aim_request_;
};

}  // namespace moonclient
