#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/sim/party_roster.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world.h"
#include "ro/ui/damage_meter_window.h"

#include "options/damage_meter_options.h"

namespace moonclient {

// Le volet de la fenêtre a la hauteur du palmarès que le compteur garde.
static_assert(ro::ui::kDamageMeterRecordRows == ro::sim::DamageMeter::kKeptRecords,
              "le volet du palmarès n'a pas autant de rangs que le palmarès");
static_assert(ro::ui::kDamageMeterModeCount == ro::sim::kMeterModeCount,
              "les modes de la fenêtre ne sont plus ceux du compteur");

// LE COMPTEUR ENTRE LE MONDE ET SA FENÊTRE.
//
// `sim` compte, `ui` peint ; ici se fait tout ce que ni l'un ni l'autre ne peut
// faire seul : dire au compteur qui suivre, poser les réglages du joueur,
// nommer les compétences d'après les tables, composer le titre et la
// languette, écrire les cumuls de zone dans la conversation, et exécuter les
// gestes de la fenêtre.
class DamageMeterStage {
 public:
  // AVANT LE DESSIN, à chaque image — même fenêtre fermée : le compteur doit
  // avancer pour que sa réouverture montre le combat en cours, et les cumuls
  // de zone doivent gagner la conversation.
  void Compose(ro::sim::World& world, const ro::sim::PartyRoster& party,
               const ro::sim::SkillTree* skills, DamageMeterOptions& options, ImFont* bold,
               std::uint32_t now_ms, ro::ui::DamageMeterView* into);

  // CE QUE LA FENÊTRE A DEMANDÉ. Rend vrai quand un réglage a changé — à
  // l'appelant d'écrire le fichier.
  [[nodiscard]] bool Apply(const ro::ui::DamageMeterAction& asked, ro::sim::World& world,
                           DamageMeterOptions& options);

  // CE QUE PORTE LA LANGUETTE de l'onglet où le compteur est rangé : l'unité
  // du mode, puis ce que le joueur a coché.
  [[nodiscard]] const std::string& tab_label() const noexcept { return tab_label_; }

 private:
  void Follow(ro::sim::World& world, const ro::sim::PartyRoster& party);
  void WriteAreaCycles(ro::sim::World& world, const DamageMeterOptions& options,
                       std::uint32_t now_ms);
  void NameRecords(const ro::sim::DamageMeter& meter, ro::sim::MeterMode mode,
                   const ro::sim::SkillTree* skills, std::uint32_t now_ms);
  void ComposeGroup(const ro::sim::DamageMeter& meter, ro::sim::MeterMode mode,
                    const ro::sim::MeterSegment& segment, std::uint32_t active_ms,
                    const ro::sim::SkillTree* skills);
  void ComposeCaption(const ro::sim::DamageMeter& meter, ro::sim::MeterMode mode,
                      const DamageMeterOptions& options, std::uint32_t now_ms);
  // Le nom d'une clé de ventilation : une compétence, un coup ordinaire, ou
  // l'une des clés qui n'en sont pas. ⚠ RANGÉ DANS `names_`, dont la vue garde
  // des vues : une file ne déplace jamais ce qu'elle tient déjà.
  [[nodiscard]] std::string_view SkillName(std::uint16_t key, const ro::sim::SkillTree* skills);

  std::vector<ro::sim::MeterActor> followed_;
  std::deque<std::string> names_;
  std::vector<ro::ui::DamageMeterRecordView> records_;
  std::vector<ro::ui::DamageMeterBarView> bars_;
  std::vector<ro::ui::DamageMeterSkillView> detail_;
  std::string detail_name_;
  std::string title_;
  std::string tab_label_;
  // L'acteur dont le détail est ouvert, ou zéro.
  std::uint32_t detail_id_ = 0;
  // La largeur de colonne appliquée au compteur. Elle change le sens de tout
  // le graphique : la changer l'oublie.
  std::optional<std::uint32_t> slot_ms_;
};

}  // namespace moonclient
