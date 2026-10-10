#include "game/adoption_stage.h"

#include <array>
#include <cstdio>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/adoption.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// La couleur d'un refus dans la conversation.
constexpr std::uint32_t kFailedColor = 0xFF0000;

// Assez pour la question et le plus long des noms.
constexpr std::size_t kQuestionBytes = 512;

}  // namespace

std::string AdoptionQuestion(std::string_view parent_name) {
  std::array<char, kQuestionBytes> question{};
  const std::string name(parent_name);
  std::snprintf(question.data(), question.size(),
                Tr("%s et son conjoint veulent vous adopter. Accepter, c'est devenir pour toujours "
                   "un bébé de votre classe. Accepter ?"),
                name.c_str());
  return question.data();
}

const char* AdoptionRefusalText(ro::sim::AdoptionRefusal refusal) {
  switch (refusal) {
    case ro::sim::AdoptionRefusal::kAlreadyParent:
      return Tr("Adoption refusée : vous ou votre conjoint avez déjà un enfant.");
    case ro::sim::AdoptionRefusal::kParentLevel:
      return Tr("Adoption refusée : vous et votre conjoint devez avoir atteint le niveau 70.");
    case ro::sim::AdoptionRefusal::kChildMarried:
      return Tr("Adoption refusée : on n'adopte pas un joueur marié.");
  }
  return Tr("Adoption refusée.");
}

void AdoptionStage::StartSession() {
  desk_ = ro::sim::AdoptionDesk();
  asked_seq_ = 0;
}

void AdoptionStage::Advance(AdoptionBox& box, ro::net::Connection& link, ro::sim::World& world,
                            std::uint32_t now_ms) {
  // ⚠ LA RÉPONSE VISE LA DEMANDE QUE LA BOÎTE MONTRAIT, qui est la dernière
  // reçue : le serveur n'en garde pas d'autre.
  if (const std::optional<bool> answer = std::exchange(box.answer, std::nullopt);
      answer.has_value() && desk_.invitation().has_value()) {
    if (auto sent = ro::net::AnswerAdoption(link, *desk_.invitation(), *answer); !sent) {
      ro::core::LogWarning("adoption : réponse non émise — {}", ro::core::Describe(sent.error()));
    }
    desk_.ForgetInvitation();
  }
  for (const ro::sim::AdoptionRefusal refusal : desk_.TakeRefusals()) {
    world.RecordChat(ro::sim::ChatKind::kServer, 0, AdoptionRefusalText(refusal), now_ms,
                     kFailedColor);
  }
  // La question ne se compose qu'à l'arrivée d'une demande.
  if (desk_.invitation_seq() != asked_seq_) {
    asked_seq_ = desk_.invitation_seq();
    box.ask = desk_.invitation().has_value();
    box.question = desk_.invitation().has_value()
                       ? AdoptionQuestion(desk_.invitation()->parent_name)
                       : std::string();
  } else if (!desk_.invitation().has_value()) {
    box.question.clear();
  }
}

}  // namespace moonclient
