#pragma once

#include <cstdint>
#include <optional>

#include "ro/sim/skill_book.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {

// CE QUI EMPÊCHE LE JOUEUR DE LANCER UN SKILL, et que le serveur TAIT.
//
// 🔴 LE SERVEUR JETTE CES DEMANDES SANS UN PAQUET : ni refus, ni motif. Le
// lancer ne part pas, et le joueur ne sait pas pourquoi. Chaque cas ci-dessous
// est un refus SANS EXCEPTION côté serveur pour la compétence demandée, et un
// état que le client tient de ses propres paquets :
//
//   - assis : la demande est jetée avant tout autre examen, sur cible comme au
//     sol ;
//   - le corps pris — Stone Curse, Frozen, Stun, Sleep, White Imprison —, sauf
//     pour les trois skills qui en délivrent ;
//   - Silence, Berserk, Steel Body, Deep Sleep, Saturday Night Fever, et les
//     bonnes manières négatives, sauf pour les mêmes trois skills, et sauf pour
//     un skill qu'un objet prête ;
//   - Play Dead, qui n'autorise que lui-même ;
//   - Chase Walk, qui n'autorise que lui-même ;
//   - la monture d'événement, qui n'autorise rien.
//
// ⚠ CE QUI DÉPEND D'UNE TABLE QUE LE CLIENT N'A PAS RESTE DEHORS. Hiding, la
// danse ou Gospel laissent passer certains skills, nommés dans la base du
// serveur : en décider ici bloquerait un lancer légitime.
//
// ⚠ LE PREMIER ÉTAT TROUVÉ TRANCHE, dans l'ordre où le serveur les examine :
// assis d'abord, puis le corps, puis le reste.
struct SkillUseBlock {
  LocalRefusal reason = LocalRefusal::kNone;
  // L'état qui interdit, pour `LocalRefusal::kStatus` ; zéro sinon.
  StatusEffectId status = 0;

  friend bool operator==(const SkillUseBlock&, const SkillUseBlock&) = default;
};

// Ce qui interdit au joueur de lancer `skill_id` maintenant, ou rien.
// `lent_by_item` dit que l'objet qu'on vient d'utiliser prête ce skill : le
// serveur lui passe le Silence, Berserk et les manières.
[[nodiscard]] std::optional<SkillUseBlock> SkillUseBlockFor(const World& world,
                                                            std::uint16_t skill_id,
                                                            bool lent_by_item);

// Le refus que le grimoire note pour ce blocage, pour que la conversation le
// dise comme un refus du serveur.
[[nodiscard]] SkillRefusal BlockedRefusal(std::uint16_t skill_id, const SkillUseBlock& block,
                                          std::uint32_t now_ms);

// LA PORTE DES LANCERS DU JOUEUR : ce qui est bloqué ne part pas, et se dit.
//
// ⚠ UNE SEULE FOIS PAR BLOCAGE : une touche tenue redemande le même skill à
// chaque répétition, et le même refus ne se redit qu'après un lancer parti
// (`NoteSent`) ou un blocage levé.
class SkillBlockGate {
 public:
  // Vrai quand le lancer est bloqué : rien ne doit partir. Le refus se note
  // alors dans le grimoire, sauf s'il vient d'être dit.
  [[nodiscard]] bool Refuse(World& world, std::uint16_t skill_id, bool lent_by_item,
                            std::uint32_t now_ms);

  // Un lancer est parti : le prochain blocage se dira de nouveau.
  void NoteSent() noexcept { said_.reset(); }

 private:
  struct Said {
    std::uint16_t skill_id = 0;
    SkillUseBlock block;
    friend bool operator==(const Said&, const Said&) = default;
  };
  std::optional<Said> said_;
};

// LES SKILLS QUI DÉLIVRENT d'un corps pris ou d'un état qui interdit les
// autres : le serveur les laisse partir malgré l'un et l'autre.
inline constexpr std::uint16_t kRefreshSkill = 2013;
inline constexpr std::uint16_t kGentleTouchCureSkill = 2345;
inline constexpr std::uint16_t kGroomingSkill = 5049;
// Les deux skills qui s'autorisent eux-mêmes sous leur propre état.
inline constexpr std::uint16_t kPlayDeadSkill = 143;
inline constexpr std::uint16_t kChaseWalkSkill = 389;

// LES ÉTATS QUI INTERDISENT DE LANCER, portés par leur icône : ceux qui ont
// `NoCast` dans la table des états du serveur et que le client voit.
inline constexpr StatusEffectId kEfstSteelBody = 87;
inline constexpr StatusEffectId kEfstBerserk = 107;

}  // namespace ro::sim
