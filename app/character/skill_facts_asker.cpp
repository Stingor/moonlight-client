#include "character/skill_facts_asker.h"

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/net/skill_facts.h"

namespace moonclient {

SkillFactsSends SkillFactsAsker::Plan(const ro::ui::SkillDescriptionAction& asked,
                                      const ro::sim::SkillFacts& told, std::uint32_t now_ms) {
  SkillFactsSends sends;
  if (asked.refresh) {
    // ⚠ OUBLIER CE QU'ON A DEMANDÉ SUFFIT : la demande repart juste en dessous,
    // et le serveur remplacera ce que le monde porte.
    asked_cast_skill_id_ = 0;
  }
  if (asked.want_cast != 0 && asked.want_cast != asked_cast_skill_id_) {
    sends.cast_skill_id = asked.want_cast;
    asked_cast_skill_id_ = asked.want_cast;
  }
  sends.damage = PlanDamage(asked, told, now_ms);
  return sends;
}

std::optional<ro::sim::SkillDamageQuestion> SkillFactsAsker::PlanDamage(
    const ro::ui::SkillDescriptionAction& asked, const ro::sim::SkillFacts& told,
    std::uint32_t now_ms) {
  // Un refus lu une fois : il relance la question envoyée, si c'est bien elle que
  // le serveur a refusée et qu'elle n'était pas déjà une relance.
  bool refused = false;
  if (told.damage_too_soon_count() != too_soon_seen_) {
    too_soon_seen_ = told.damage_too_soon_count();
    refused = sent_.has_value() && told.last_damage_too_soon() == *sent_ && !resent_;
  }

  // L'onglet fermé n'attend plus rien ; rouvert, il repose sa question.
  if (asked.want_damage == 0) {
    wanted_ = ro::sim::SkillDamageQuestion();
    due_ = false;
    settling_ = false;
    resend_pending_ = false;
    return std::nullopt;
  }

  const ro::sim::SkillDamageQuestion question{.skill_id = asked.want_damage,
                                              .target_mob_id = asked.damage_target};
  if (question != wanted_) {
    // ⚠ SEULE LA CIBLE SE TAPE : une autre compétence, ou l'onglet qu'on ouvre,
    // part sans attendre ; une cible qui change pour la même compétence attend
    // que la frappe se pose.
    settling_ = wanted_.skill_id == question.skill_id;
    wanted_ = question;
    wanted_since_ms_ = now_ms;
    due_ = !sent_.has_value() || *sent_ != question;
    // Une autre question n'est plus une relance : elle a droit à la sienne.
    resend_pending_ = false;
  }
  // La relance part pour la question EN COURS : si la frappe a repris depuis,
  // c'est la dernière qui part, comme une question neuve.
  if (refused) {
    due_ = true;
    resend_pending_ = wanted_ == *sent_;
  }
  if (asked.refresh) {
    due_ = true;
    settling_ = false;
    resend_pending_ = false;
  }
  if (!due_) {
    return std::nullopt;
  }
  if (settling_ && now_ms - wanted_since_ms_ < kSkillDamageSettleMs) {
    return std::nullopt;
  }
  if (sent_.has_value() && now_ms - sent_at_ms_ < kSkillDamageSpacingMs) {
    return std::nullopt;
  }
  due_ = false;
  settling_ = false;
  resent_ = resend_pending_;
  resend_pending_ = false;
  sent_ = wanted_;
  sent_at_ms_ = now_ms;
  return wanted_;
}

void SkillFactsAsker::Ask(const ro::ui::SkillDescriptionAction& asked,
                          const ro::sim::SkillFacts& told, std::uint32_t now_ms,
                          ro::net::Connection& link) {
  const SkillFactsSends sends = Plan(asked, told, now_ms);
  if (sends.cast_skill_id != 0) {
    if (auto sent = ro::net::RequestSkillCastFacts(link, sends.cast_skill_id); !sent) {
      ro::core::LogWarning(
          "description : cast d'une compétence — {} — la fenêtre gardera les chiffres précédents",
          ro::core::Describe(sent.error()));
    }
  }
  if (sends.damage.has_value()) {
    // ⚠ UN RANG NUL VEUT DIRE « CELUI QUE J'AI APPRIS » : c'est le serveur qui le
    // relit, et lui écrire un rang qu'on n'a pas donnerait l'estimation de ce
    // qu'on ne peut pas lancer.
    if (auto sent = ro::net::RequestSkillDamage(link, sends.damage->skill_id, 0,
                                                sends.damage->target_mob_id);
        !sent) {
      ro::core::LogWarning(
          "description : dégâts d'une compétence — {} — l'estimation restera celle d'avant",
          ro::core::Describe(sent.error()));
    }
  }
}

}  // namespace moonclient
