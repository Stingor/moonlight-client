#pragma once

#include <cstdint>
#include <optional>

#include "ro/net/connection.h"
#include "ro/net/skill_facts.h"
#include "ro/sim/skill_facts.h"
#include "ro/ui/description_window.h"

namespace moonclient {

// L'anti-rebond du champ de cible : une estimation ne part que lorsque la frappe
// s'est posée depuis ce délai. Taper « 1002 » ne doit pas poser quatre questions.
inline constexpr std::uint32_t kSkillDamageSettleMs = 250;

// L'écart minimal entre deux demandes d'estimation : celui que le serveur exige,
// plus une marge pour la gigue du fil, qui peut rapprocher deux paquets.
inline constexpr std::uint32_t kSkillDamageSpacingMarginMs = 50;
inline constexpr std::uint32_t kSkillDamageSpacingMs =
    ro::net::kSkillDamageServerSpacingMs + kSkillDamageSpacingMarginMs;

// Ce qu'une image doit envoyer : la table de cast d'une compétence (zéro sinon),
// et une question d'estimation.
struct SkillFactsSends {
  std::uint16_t cast_skill_id = 0;
  std::optional<ro::sim::SkillDamageQuestion> damage;
};

// CE QUE LES ONGLETS TECHNIQUES D'UNE COMPÉTENCE DEMANDENT AU SERVEUR : le
// temps d'incantation, et l'estimation des dégâts contre une cible.
//
// 🔴 LA FENÊTRE REDEMANDE À CHAQUE IMAGE où son onglet est ouvert, et c'est
// voulu : elle ne sait pas ce que le monde porte déjà — `ui` ne possède rien.
// C'est donc ICI qu'on décide de ne pas harceler le serveur, et nulle part
// ailleurs. Sans ce filtre, ce serait soixante paquets par seconde.
//
// ⚠ LA COMPÉTENCE ET LA CIBLE FONT PARTIE DE LA CLÉ : changer de compétence ou
// de cible, c'est une AUTRE question, et la réponse d'avant n'y répond pas.
//
// ⚠ ON NE REDEMANDE PAS PARCE QUE LE TEMPS PASSE : ces chiffres ne changent
// qu'avec l'équipement et les états, et rien ne nous en avertit. C'est le bouton
// du joueur qui rouvre la question — d'où `refresh`.
//
// 🔴 ET L'ESTIMATION A SA CADENCE. Une cible qui change pour la même compétence
// attend `kSkillDamageSettleMs` sans nouvelle frappe ; deux envois sont toujours
// séparés de `kSkillDamageSpacingMs`. Une question que le serveur refuse comme
// trop rapprochée repart d'elle-même UNE fois, après l'écart — et si la question
// a changé entre-temps, c'est la dernière qui part.
class SkillFactsAsker {
 public:
  // Ce qu'il faut envoyer à cette image, d'après ce que la fenêtre veut et ce que
  // le serveur a refusé. Ne touche pas au fil.
  [[nodiscard]] SkillFactsSends Plan(const ro::ui::SkillDescriptionAction& asked,
                                     const ro::sim::SkillFacts& told, std::uint32_t now_ms);

  // `Plan`, puis l'envoi. Un envoi qui échoue se journalise : la fenêtre garde
  // les chiffres précédents.
  void Ask(const ro::ui::SkillDescriptionAction& asked, const ro::sim::SkillFacts& told,
           std::uint32_t now_ms, ro::net::Connection& link);

 private:
  [[nodiscard]] std::optional<ro::sim::SkillDamageQuestion> PlanDamage(
      const ro::ui::SkillDescriptionAction& asked, const ro::sim::SkillFacts& told,
      std::uint32_t now_ms);

  std::uint16_t asked_cast_skill_id_ = 0;

  // La question que la fenêtre pose en ce moment, et depuis quand.
  ro::sim::SkillDamageQuestion wanted_;
  std::uint32_t wanted_since_ms_ = 0;
  // Elle doit partir ; et elle attend d'abord que la frappe se pose.
  bool due_ = false;
  bool settling_ = false;
  // La dernière question envoyée, et quand.
  std::optional<ro::sim::SkillDamageQuestion> sent_;
  std::uint32_t sent_at_ms_ = 0;
  // L'envoi qui attend est une relance ; et le dernier envoi en était une — un
  // second refus ne relance plus.
  bool resend_pending_ = false;
  bool resent_ = false;
  // Les refus « trop rapproché » déjà lus dans le monde.
  std::uint32_t too_soon_seen_ = 0;
};

}  // namespace moonclient
