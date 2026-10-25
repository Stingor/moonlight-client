#include "survivors_meta_text.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/survivors_ranking_window.h"

namespace ro::ui {
namespace {

constexpr std::int64_t kSecondsPerDay = 86'400;

}  // namespace

// ⚠ UN FORMAT LITTÉRAL PAR CLÉ, et non une table de formats : un format lu
// dans une table n'est plus vérifié par le compilateur contre ses arguments.
SurvivorsEffectText SurvivorsBlessingEffect(std::string_view blessing_key, int total) {
  SurvivorsEffectText text = {};
  char* out = text.data();
  const std::size_t size = text.size();
  if (blessing_key == "blessing") {
    std::snprintf(out, size, core::Tr("Puissance +%d %%"), total);
  } else if (blessing_key == "kyrie_eleison") {
    std::snprintf(out, size, core::Tr("PV +%d %%"), total);
  } else if (blessing_key == "angelus") {
    std::snprintf(out, size, core::Tr("Armure +%d"), total);
  } else if (blessing_key == "magnificat") {
    std::snprintf(out, size, core::Tr("PV +%d par minute"), total);
  } else if (blessing_key == "increase_agility") {
    std::snprintf(out, size, core::Tr("Marche +%d %%"), total);
  } else if (blessing_key == "suffragium") {
    std::snprintf(out, size, core::Tr("Recharge –%d %%"), total);
  } else if (blessing_key == "impositio_manus") {
    std::snprintf(out, size, core::Tr("Zone +%d %%"), total);
  } else if (blessing_key == "aspersio") {
    std::snprintf(out, size, core::Tr("Durée +%d %%"), total);
  } else if (blessing_key == "gloria") {
    std::snprintf(out, size, core::Tr("Chance +%d %%"), total);
  } else if (blessing_key == "benedictio") {
    std::snprintf(out, size, core::Tr("Expérience +%d %%"), total);
  } else if (blessing_key == "resurrection") {
    std::snprintf(out, size, core::Tr("Vies +%d"), total);
  } else {
    std::snprintf(out, size, "%+d", total);
  }
  return text;
}

SurvivorsEffectText SurvivorsKafraEffect(int charges) {
  SurvivorsEffectText text = {};
  std::snprintf(text.data(), text.size(), core::Tr("%d charge(s) par partie"), charges);
  return text;
}

const char* SurvivorsKafraDescription(std::string_view service_key) {
  if (service_key == "magnifier") return core::Tr("Relancer : une offre de cartes neuve.");
  if (service_key == "fly_wing") return core::Tr("Passer : laisser une offre sans rien prendre.");
  if (service_key == "butterfly_wing") {
    return core::Tr("Bannir : retirer une carte des offres de la partie.");
  }
  return "";
}

const char* SurvivorsFeatGoal(std::string_view feat_key) {
  if (feat_key == "sight") return core::Tr("Survivre en une seule partie");
  if (feat_key == "frost_diver") return core::Tr("Glaçages par Cold Bolt, au cumul");
  return "";
}

SurvivorsDateText SurvivorsDateOfDay(std::int64_t days_since_epoch) {
  const std::chrono::year_month_day date{
      std::chrono::sys_days{std::chrono::days{days_since_epoch}}};
  SurvivorsDateText text = {};
  std::snprintf(text.data(), text.size(), "%04d-%02u-%02u", static_cast<int>(date.year()),
                static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()));
  return text;
}

SurvivorsDateText SurvivorsDateOfUtc(std::int64_t utc_s) {
  // La division tronque vers zéro : un instant d'avant 1970 tombe la veille.
  std::int64_t days = utc_s / kSecondsPerDay;
  if (utc_s % kSecondsPerDay < 0) --days;
  return SurvivorsDateOfDay(days);
}

SurvivorsZenyText SurvivorsZeny(std::uint64_t zeny) {
  constexpr std::uint64_t kMostShown = std::numeric_limits<std::int64_t>::max();
  const std::string grouped = GroupThousands(static_cast<std::int64_t>(std::min(zeny, kMostShown)));
  SurvivorsZenyText text = {};
  std::snprintf(text.data(), text.size(), "%s", grouped.c_str());
  return text;
}

// ───────────── Le classement, déclaré dans `survivors_ranking_window.h` ─────────────

const char* SurvivorsTicketRefusalText(SurvivorsTicketRefusal refusal) {
  switch (refusal) {
    case SurvivorsTicketRefusal::kPlace:
      return core::Tr("Partie libre : le serveur ne classe pas une partie lancée d'ici.");
    case SurvivorsTicketRefusal::kUnlinked:
      return core::Tr("Partie libre : ce compte n'est rattaché à aucun joueur.");
    case SurvivorsTicketRefusal::kRules:
      return core::Tr("Partie libre : le serveur ne classe pas ces règles.");
    case SurvivorsTicketRefusal::kTooSoon:
      return core::Tr("Partie libre : demande trop proche de la précédente.");
    case SurvivorsTicketRefusal::kSilent:
      return core::Tr("Partie libre : le serveur n'a pas répondu.");
    case SurvivorsTicketRefusal::kChallengeSeed:
      return core::Tr("Partie libre : le serveur compte une autre semaine.");
    case SurvivorsTicketRefusal::kUnknown:
      break;
  }
  return core::Tr("Partie libre : refusée par le serveur.");
}

std::array<char, kSurvivorsRankTextSize> SurvivorsRankText(int rank) {
  std::array<char, kSurvivorsRankTextSize> text = {};
  std::snprintf(text.data(), text.size(), core::Tr("%de"), rank);
  return text;
}

std::array<char, kSurvivorsStandingTextSize> SurvivorsStandingText(
    const SurvivorsRankStanding& standing) {
  std::array<char, kSurvivorsStandingTextSize> text = {};
  char* out = text.data();
  const std::size_t size = text.size();
  const bool placed = standing.rank > 0;
  switch (standing.status) {
    case SurvivorsRankStatus::kSending:
      std::snprintf(out, size, "%s", core::Tr("Envoi au serveur…"));
      break;
    case SurvivorsRankStatus::kPending:
      std::snprintf(out, size, "%s",
                    core::Tr("En attente du serveur : le résultat sera renvoyé à la prochaine "
                             "connexion de ce personnage."));
      break;
    case SurvivorsRankStatus::kPersonalBest:
      if (placed) {
        std::snprintf(out, size, core::Tr("Classement : %s — meilleur personnel !"),
                      SurvivorsRankText(standing.rank).data());
      } else {
        std::snprintf(out, size, "%s",
                      core::Tr("Classement : hors tableau — meilleur personnel !"));
      }
      break;
    case SurvivorsRankStatus::kAccepted:
      if (placed) {
        std::snprintf(out, size, core::Tr("Classement : %s (votre meilleur : %s)"),
                      SurvivorsRankText(standing.rank).data(),
                      SurvivorsZeny(standing.best_score).data());
      } else {
        std::snprintf(out, size, core::Tr("Classement : hors tableau (votre meilleur : %s)"),
                      SurvivorsZeny(standing.best_score).data());
      }
      break;
    case SurvivorsRankStatus::kLost:
      std::snprintf(out, size, "%s",
                    core::Tr("Résultat non compté : déjà reçu ou oublié par le serveur."));
      break;
    case SurvivorsRankStatus::kImplausible:
      std::snprintf(out, size, "%s", core::Tr("Refusée par le serveur : partie invraisemblable."));
      break;
    case SurvivorsRankStatus::kRefused:
      std::snprintf(out, size, "%s", core::Tr("Refusée par le serveur, sans raison connue."));
      break;
  }
  return text;
}

}  // namespace ro::ui
