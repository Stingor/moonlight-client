#include "character/skill_facts_asker.h"

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/net/skill_facts.h"

namespace moonclient {

void SkillFactsAsker::Ask(const ro::ui::SkillDescriptionAction& asked, ro::net::Connection& link) {
  if (asked.refresh) {
    // ⚠ OUBLIER CE QU'ON A DEMANDÉ SUFFIT : la demande repart juste en dessous,
    // et le serveur remplacera ce que le monde porte.
    asked_cast_skill_id_ = 0;
    asked_damage_skill_id_ = 0;
  }
  if (asked.want_cast != 0 && asked.want_cast != asked_cast_skill_id_) {
    if (auto sent = ro::net::RequestSkillCastFacts(link, asked.want_cast); !sent) {
      ro::core::LogWarning(
          "description : cast d'une compétence — {} — la fenêtre gardera les chiffres précédents",
          ro::core::Describe(sent.error()));
    }
    asked_cast_skill_id_ = asked.want_cast;
  }
  if (asked.want_damage != 0 && (asked.want_damage != asked_damage_skill_id_ ||
                                 asked.damage_target != asked_damage_target_id_)) {
    // ⚠ UN RANG NUL VEUT DIRE « CELUI QUE J'AI APPRIS » : c'est le serveur qui le
    // relit, et lui écrire un rang qu'on n'a pas donnerait l'estimation de ce
    // qu'on ne peut pas lancer.
    if (auto sent = ro::net::RequestSkillDamage(link, asked.want_damage, 0, asked.damage_target);
        !sent) {
      ro::core::LogWarning(
          "description : dégâts d'une compétence — {} — l'estimation restera celle d'avant",
          ro::core::Describe(sent.error()));
    }
    asked_damage_skill_id_ = asked.want_damage;
    asked_damage_target_id_ = asked.damage_target;
  }
}

}  // namespace moonclient
