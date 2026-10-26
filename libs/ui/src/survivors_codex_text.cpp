#include "survivors_codex_text.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ro/core/i18n.h"
#include "ro/ui/database_window.h"
#include "ro/ui/survivors_hud.h"

#include "survivors_text.h"

namespace ro::ui {
namespace {

constexpr std::int64_t kMsPerSecond = 1000;
constexpr std::int64_t kTenthsPerCell = 10;
constexpr std::int64_t kHundredthsPerPct = 100;
// Une décimale de plus ne dirait rien au joueur.
constexpr int kMostDecimals = 2;
constexpr std::int64_t kDecimalBase = 10;
constexpr std::size_t kNumberSize = 32;
using NumberText = std::array<char, kNumberSize>;

// `value / scale`, arrondi à deux décimales au plus, les décimales nulles
// tues ; `scale` est une puissance de dix.
[[nodiscard]] NumberText DecimalText(std::int64_t value, std::int64_t scale) {
  int decimals = 0;
  std::int64_t shown_scale = 1;
  while (shown_scale < scale && decimals < kMostDecimals) {
    shown_scale *= kDecimalBase;
    ++decimals;
  }
  const std::int64_t step = scale / shown_scale;
  const std::int64_t rounded = (std::llabs(value) + step / 2) / step;
  const std::int64_t whole = rounded / shown_scale;
  std::int64_t fraction = rounded % shown_scale;
  while (decimals > 0 && fraction % kDecimalBase == 0) {
    fraction /= kDecimalBase;
    --decimals;
  }
  const char* sign = value < 0 && rounded != 0 ? "-" : "";
  NumberText text = {};
  if (decimals == 0) {
    std::snprintf(text.data(), text.size(), "%s%lld", sign, static_cast<long long>(whole));
    return text;
  }
  NumberText digits = {};
  std::snprintf(digits.data(), digits.size(), "%0*lld", decimals, static_cast<long long>(fraction));
  NumberText number = {};
  // La partie entière et les décimales, séparées comme la langue le veut.
  std::snprintf(number.data(), number.size(), core::Tr("%lld,%s"), static_cast<long long>(whole),
                digits.data());
  std::snprintf(text.data(), text.size(), "%s%s", sign, number.data());
  return text;
}

// Ajoute `piece` à `text`, séparé par une virgule quand `text` n'est pas vide.
void Append(CodexText& text, const char* piece) {
  if (piece == nullptr || piece[0] == '\0') return;
  const std::size_t used = std::strlen(text.data());
  if (used + 1 >= text.size()) return;
  std::snprintf(text.data() + used, text.size() - used, used == 0 ? "%s" : ", %s", piece);
}

[[nodiscard]] const char* GaitName(SurvivorsCodexGait gait) {
  switch (gait) {
    case SurvivorsCodexGait::kChase:
      return core::Tr("Poursuit le héros");
    case SurvivorsCodexGait::kStill:
      return core::Tr("Immobile");
    case SurvivorsCodexGait::kFlyer:
      return core::Tr("Vole droit sur le héros");
    case SurvivorsCodexGait::kThief:
      return core::Tr("Vole les gemmes");
    case SurvivorsCodexGait::kRanged:
      return core::Tr("Tire à distance");
    case SurvivorsCodexGait::kCharger:
      return core::Tr("Charge en ligne droite");
    case SurvivorsCodexGait::kBlinker:
      return core::Tr("Bondit vers le héros");
  }
  return "";
}

[[nodiscard]] CodexText SecondsText(std::uint32_t ms) {
  return CodexValueText(SurvivorsCodexUnit::kMs, ms);
}

[[nodiscard]] CodexText CellsText(std::int32_t tenths) {
  return CodexValueText(SurvivorsCodexUnit::kCellTenths, tenths);
}

}  // namespace

const char* CodexTabLabel(SurvivorsCodexTab tab) {
  switch (tab) {
    case SurvivorsCodexTab::kSkills:
      return core::Tr("Sorts");
    case SurvivorsCodexTab::kCards:
      return core::Tr("Cartes");
    case SurvivorsCodexTab::kItems:
      return core::Tr("Objets");
    case SurvivorsCodexTab::kPlants:
      return core::Tr("Plantes");
    case SurvivorsCodexTab::kFoes:
      return core::Tr("Monstres");
    case SurvivorsCodexTab::kBosses:
      return core::Tr("Boss et événements");
  }
  return "";
}

namespace {

// Le libellé d'une ligne qui ne nomme aucun compte.
[[nodiscard]] const char* PlainFigureLabel(SurvivorsCodexFigureLabel label) {
  using enum SurvivorsCodexFigureLabel;
  switch (label) {
    case kDamage:
      return core::Tr("Dégâts");
    case kCooldown:
      return core::Tr("Recharge");
    case kAmount:
      return core::Tr("Quantité");
    case kPierce:
      return core::Tr("Perce");
    case kReach:
      return core::Tr("Portée");
    case kArea:
      return core::Tr("Aire");
    case kLength:
      return core::Tr("Longueur");
    case kLife:
      return core::Tr("Durée");
    case kZoneHits:
      return core::Tr("Touches");
    case kChill:
      return core::Tr("Ralentit");
    case kFreeze:
      return core::Tr("Gèle");
    case kKnockback:
      return core::Tr("Recul");
    case kBounces:
      return core::Tr("Rebonds");
    case kSplash:
      return core::Tr("Éclats");
    case kHp:
      return core::Tr("PV");
    case kHpGrowthPerMinute:
      return core::Tr("PV en plus par minute");
    case kTouchDamage:
      return core::Tr("Dégâts au contact");
    case kTouchInterval:
      return core::Tr("Intervalle de contact");
    case kWalkPerCell:
      return core::Tr("Marche, par case");
    case kExp:
      return core::Tr("XP");
    case kHeal:
      return core::Tr("Soin");
    case kZeny:
      return core::Tr("Zeny");
    case kExpBoost:
      return core::Tr("XP en plus");
    case kWalkBoost:
      return core::Tr("Marche en plus");
    case kDuration:
      return core::Tr("Durée de l'effet");
    case kGainsOdds:
      return "";
    case kEvolutionsFrom:
      return core::Tr("Évolutions dès");
    case kGemExp:
      return core::Tr("XP dès");
    case kDrawWeight:
      return core::Tr("Chance au tirage");
    case kHpBoost:
      return core::Tr("PV en plus");
    case kTouchBoost:
      return core::Tr("Contact en plus");
  }
  return "";
}

}  // namespace

CodexText CodexFigureLabelText(const SurvivorsCodexFigure& figure) {
  CodexText text = {};
  if (figure.label == SurvivorsCodexFigureLabel::kGainsOdds) {
    std::snprintf(text.data(), text.size(), core::Tr("Chance de %d gains"),
                  static_cast<int>(figure.gain_count));
  } else {
    std::snprintf(text.data(), text.size(), "%s", PlainFigureLabel(figure.label));
  }
  return text;
}

const char* CodexLinkName(const SurvivorsCodexLink& link) {
  return link.seen ? link.name : kCodexUnknown;
}

CodexText CodexValueText(SurvivorsCodexUnit unit, std::int64_t value) {
  CodexText text = {};
  switch (unit) {
    case SurvivorsCodexUnit::kPlain:
      std::snprintf(text.data(), text.size(), "%lld", static_cast<long long>(value));
      break;
    case SurvivorsCodexUnit::kMs:
      std::snprintf(text.data(), text.size(), core::Tr("%s s"),
                    DecimalText(value, kMsPerSecond).data());
      break;
    case SurvivorsCodexUnit::kCellTenths:
      std::snprintf(text.data(), text.size(),
                    value == kTenthsPerCell ? core::Tr("%s case") : core::Tr("%s cases"),
                    DecimalText(value, kTenthsPerCell).data());
      break;
    case SurvivorsCodexUnit::kPercent:
      std::snprintf(text.data(), text.size(), core::Tr("%s %%"), DecimalText(value, 1).data());
      break;
    case SurvivorsCodexUnit::kPercentHundredths:
      std::snprintf(text.data(), text.size(), core::Tr("%s %%"),
                    DecimalText(value, kHundredthsPerPct).data());
      break;
  }
  return text;
}

const char* CodexElementName(std::uint8_t element) {
  return element < kDatabaseElements.size() ? SurvivorsText(kDatabaseElements[element]) : "";
}

const char* CodexSizeName(std::uint8_t size) {
  return size < kDatabaseSizes.size() ? SurvivorsText(kDatabaseSizes[size]) : "";
}

CodexText CodexBehaviourText(const SurvivorsCodexBehaviour& behaviour) {
  CodexText text = {};
  Append(text, GaitName(behaviour.gait));
  if (behaviour.pack > 1) {
    CodexText pack = {};
    std::snprintf(pack.data(), pack.size(), core::Tr("en meute de %d"),
                  static_cast<int>(behaviour.pack));
    Append(text, pack.data());
  }
  if ((behaviour.traits & kCodexTraitKnockbackImmune) != 0) {
    Append(text, core::Tr("insensible au recul"));
  }
  if ((behaviour.traits & kCodexTraitFreezeImmune) != 0)
    Append(text, core::Tr("insensible au gel"));
  if ((behaviour.traits & kCodexTraitPassesWalls) != 0) Append(text, core::Tr("traverse les murs"));
  if ((behaviour.traits & kCodexTraitHides) != 0) Append(text, core::Tr("se cache par moments"));
  if ((behaviour.traits & kCodexTraitOverHorde) != 0) {
    Append(text, core::Tr("passe au-dessus de la horde"));
  }
  return text;
}

CodexText CodexSourceText(const SurvivorsCodexSource& source) {
  CodexText text = {};
  const auto clock = SurvivorsClockText(source.at_ms);
  switch (source.kind) {
    case SurvivorsCodexSourceKind::kWave:
      std::snprintf(text.data(), text.size(), core::Tr("vagues, dès %s"), clock.data());
      break;
    case SurvivorsCodexSourceKind::kEvent:
      std::snprintf(text.data(), text.size(), core::Tr("événement de %s"), clock.data());
      break;
    case SurvivorsCodexSourceKind::kEscort:
      std::snprintf(text.data(), text.size(), core::Tr("escorte de %s, à %s"),
                    CodexLinkName(source.bringer), clock.data());
      break;
    case SurvivorsCodexSourceKind::kSummon:
      std::snprintf(text.data(), text.size(), core::Tr("invoqué par %s, dès %s"),
                    CodexLinkName(source.bringer), clock.data());
      break;
  }
  return text;
}

CodexText CodexAffinitiesText(std::span<const SurvivorsCodexAffinity> affinities) {
  CodexText text = {};
  for (const SurvivorsCodexAffinity& affinity : affinities) {
    CodexText piece = {};
    std::snprintf(piece.data(), piece.size(), "%s %s", CodexElementName(affinity.element),
                  CodexValueText(SurvivorsCodexUnit::kPercent, affinity.pct).data());
    Append(text, piece.data());
  }
  return text;
}

CodexText CodexMoveText(const SurvivorsCodexMove& move) {
  CodexText deed = {};
  const auto period = SecondsText(move.period_ms);
  const auto telegraph = SecondsText(move.telegraph_ms);
  const auto effect = SecondsText(move.effect_ms);
  switch (move.kind) {
    case SurvivorsCodexMoveKind::kSummon:
      std::snprintf(deed.data(), deed.size(), core::Tr("Invoque %d %s toutes les %s"),
                    static_cast<int>(move.count), CodexLinkName(move.foe), period.data());
      break;
    case SurvivorsCodexMoveKind::kLeap:
      std::snprintf(deed.data(), deed.size(),
                    core::Tr("Bondit de %s toutes les %s, annoncé %s avant"),
                    CellsText(move.reach_tenths).data(), period.data(), telegraph.data());
      break;
    case SurvivorsCodexMoveKind::kCharge:
      std::snprintf(deed.data(), deed.size(),
                    core::Tr("Charge sur %s toutes les %s, annoncée %s avant"),
                    CellsText(move.reach_tenths).data(), period.data(), telegraph.data());
      break;
    case SurvivorsCodexMoveKind::kQuake:
      std::snprintf(deed.data(), deed.size(),
                    core::Tr("Séisme : %d cercles de %s toutes les %s, %d dégâts"),
                    static_cast<int>(move.count), CellsText(move.radius_tenths).data(),
                    period.data(), static_cast<int>(move.damage));
      break;
    case SurvivorsCodexMoveKind::kHowl:
      std::snprintf(deed.data(), deed.size(),
                    core::Tr("Hurle toutes les %s : les %s marchent %d %% plus vite pendant %s"),
                    period.data(), CodexLinkName(move.foe), static_cast<int>(move.boost_pct),
                    effect.data());
      break;
    case SurvivorsCodexMoveKind::kFlee:
      std::snprintf(deed.data(), deed.size(),
                    core::Tr("Touché, s'éloigne %s, au plus toutes les %s"), effect.data(),
                    period.data());
      break;
  }
  constexpr std::uint8_t kWholeLifePct = 100;
  if (move.from_hp_pct >= kWholeLifePct && move.to_hp_pct == 0) return deed;
  CodexText text = {};
  std::snprintf(text.data(), text.size(), core::Tr("%s — de %d %% à %d %% de PV"), deed.data(),
                static_cast<int>(move.from_hp_pct), static_cast<int>(move.to_hp_pct));
  return text;
}

const char* CodexEventKindName(SurvivorsCodexEventKind kind) {
  switch (kind) {
    case SurvivorsCodexEventKind::kSwarm:
      return core::Tr("Nuée");
    case SurvivorsCodexEventKind::kRaid:
      return core::Tr("Raid");
    case SurvivorsCodexEventKind::kRing:
      return core::Tr("Anneau");
    case SurvivorsCodexEventKind::kGroup:
      return core::Tr("Groupe");
    case SurvivorsCodexEventKind::kElites:
      return core::Tr("Élites");
    case SurvivorsCodexEventKind::kNight:
      return core::Tr("Nuit");
  }
  return "";
}

CodexText CodexEventText(const SurvivorsCodexEntry& entry) {
  CodexText text = {};
  const auto at = SurvivorsClockText(entry.at_ms);
  const auto until = SurvivorsClockText(entry.until_ms);
  switch (entry.event_kind) {
    case SurvivorsCodexEventKind::kElites:
      std::snprintf(text.data(), text.size(),
                    core::Tr("Une élite à %s, puis toutes les %s jusqu'à %s"), at.data(),
                    SecondsText(entry.interval_ms).data(), until.data());
      break;
    case SurvivorsCodexEventKind::kNight:
      std::snprintf(text.data(), text.size(), core::Tr("La nuit tombe à %s ; minuit à %s"),
                    at.data(), until.data());
      break;
    case SurvivorsCodexEventKind::kSwarm:
    case SurvivorsCodexEventKind::kRaid:
    case SurvivorsCodexEventKind::kRing:
    case SurvivorsCodexEventKind::kGroup:
      std::snprintf(text.data(), text.size(), core::Tr("À %s : %d %s"), at.data(),
                    static_cast<int>(entry.event_count), CodexLinkName(entry.event_foe));
      break;
  }
  return text;
}

}  // namespace ro::ui
