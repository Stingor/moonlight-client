#include "character/character_bonus_rows.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/vfs/item_words.h"

#include "world/entity_traits.h"

namespace moonclient {

// Les groupes de monstres, `e_race2`.
//
// ⚠ LA TABLE EST INCOMPLÈTE ET LE RESTERA : le serveur en ajoute au fil de ses
// épisodes, et un groupe sans nom vaut mieux nommé par son NUMÉRO que « ? » —
// c'est le numéro qui permet de le retrouver dans `mob_db`.
std::string RaceGroupName(std::int16_t index) {
  switch (index) {
    case 1:
      return ro::core::Tr("Goblin");
    case 2:
      return ro::core::Tr("Kobold");
    case 3:
      return ro::core::Tr("Orc");
    case 4:
      return ro::core::Tr("Golem");
    case 5:
      return ro::core::Tr("Guardian");
    case 6:
      return ro::core::Tr("Ninja");
    case 7:
      return ro::core::Tr("GvG");
    case 8:
      return ro::core::Tr("Battlefield");
    case 9:
      return ro::core::Tr("Treasure");
    case 10:
      return ro::core::Tr("Biolab");
    case 11:
      return ro::core::Tr("Manuk");
    case 12:
      return ro::core::Tr("Splendide");
    case 13:
      return ro::core::Tr("Scaraba");
    default:
      break;
  }
  char named[24] = {};
  std::snprintf(named, sizeof(named), ro::core::Tr("groupe #%d"), index);
  return named;
}

namespace {

// Les cinq familles dans lesquelles un index de bonus conditionnel se lit.
enum class TargetFamily { kUnknown, kElement, kRace, kSize, kMonsterClass, kRaceGroup };

// ── LES CIBLES D'UN BONUS CONDITIONNEL ──────────────────────────────────────
//
// ⚠ CINQ FAMILLES. Élément, race et taille se disent avec les mots de la table de
// messages du client (voir `entity_traits.h`), dans la langue de l'installation ;
// la classe et le groupe de monstres, que la table ne nomme pas, passent par
// `core::Tr`.
//
// ⚠ ET CE SONT DES AIGUILLAGES DE LITTÉRAUX, pas des tables : `core::Tr` veut
// voir sa chaîne à l'appel. Même raison que les six caractéristiques.

// À quoi s'applique un code, et contre qui.
//
// 🔴 DEUX MOITIÉS, ET C'EST CE QUI REND LA LISTE COURTE : le serveur croise dix-
// huit verbes avec cinq familles de cibles, mais ne les envoie que par paires
// déjà faites. Écrire les trente-deux lignes comme trente-deux phrases entières
// aurait multiplié les occasions de se tromper d'une table.
struct ConditionalKind {
  std::string_view verb;
  TargetFamily family = TargetFamily::kUnknown;
};

[[nodiscard]] ConditionalKind KindOf(std::uint16_t code) {
  switch (static_cast<ro::sim::ConditionalBonusKind>(code)) {
    case ro::sim::ConditionalBonusKind::kSubElement:
      return {ro::core::Tr("Résist. vs"), TargetFamily::kElement};
    case ro::sim::ConditionalBonusKind::kSubRace:
      return {ro::core::Tr("Résist. vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kSubSize:
      return {ro::core::Tr("Résist. vs"), TargetFamily::kSize};
    case ro::sim::ConditionalBonusKind::kAddElement:
      return {ro::core::Tr("Dégâts vs"), TargetFamily::kElement};
    case ro::sim::ConditionalBonusKind::kAddRace:
      return {ro::core::Tr("Dégâts vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kAddSize:
      return {ro::core::Tr("Dégâts vs"), TargetFamily::kSize};
    case ro::sim::ConditionalBonusKind::kMagicAddElement:
      return {ro::core::Tr("Dég. mag. vs"), TargetFamily::kElement};
    case ro::sim::ConditionalBonusKind::kMagicAddRace:
      return {ro::core::Tr("Dég. mag. vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kMagicAddSize:
      return {ro::core::Tr("Dég. mag. vs"), TargetFamily::kSize};
    case ro::sim::ConditionalBonusKind::kCriticalRace:
      return {ro::core::Tr("Crit vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kIgnoreDefenseRace:
      return {ro::core::Tr("Ignore DEF vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kIgnoreMagicDefenseRace:
      return {ro::core::Tr("Ignore MDEF vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kSubDefenseElement:
      return {ro::core::Tr("Résist. arme"), TargetFamily::kElement};
    case ro::sim::ConditionalBonusKind::kSubClass:
      return {ro::core::Tr("Réduc. vs"), TargetFamily::kMonsterClass};
    case ro::sim::ConditionalBonusKind::kSubRaceGroup:
      return {ro::core::Tr("Réduc. vs"), TargetFamily::kRaceGroup};
    case ro::sim::ConditionalBonusKind::kExperienceRace:
      return {ro::core::Tr("EXP vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kExperienceClass:
      return {ro::core::Tr("EXP vs"), TargetFamily::kMonsterClass};
    case ro::sim::ConditionalBonusKind::kDropRace:
      return {ro::core::Tr("Drop vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kDropClass:
      return {ro::core::Tr("Drop vs"), TargetFamily::kMonsterClass};
    case ro::sim::ConditionalBonusKind::kSetDefenseRace:
      return {ro::core::Tr("DEF fixée vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kSetMagicDefenseRace:
      return {ro::core::Tr("MDEF fixée vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kVanishHpRace:
      return {ro::core::Tr("Vanish PV vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kVanishSpRace:
      return {ro::core::Tr("Vanish SP vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kComaRace:
      return {ro::core::Tr("Coma vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kComaClass:
      return {ro::core::Tr("Coma vs"), TargetFamily::kMonsterClass};
    case ro::sim::ConditionalBonusKind::kIgnoreResistanceRace:
      return {ro::core::Tr("Ignore RES vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kIgnoreMagicResistanceRace:
      return {ro::core::Tr("Ignore MRES vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kMagicAddRaceGroup:
      return {ro::core::Tr("Dég. mag. vs"), TargetFamily::kRaceGroup};
    case ro::sim::ConditionalBonusKind::kIgnoreMagicDefenseRaceGroup:
      return {ro::core::Tr("Ignore MDEF vs"), TargetFamily::kRaceGroup};
    case ro::sim::ConditionalBonusKind::kSpGainRace:
      return {ro::core::Tr("SP/kill vs"), TargetFamily::kRace};
    case ro::sim::ConditionalBonusKind::kIgnoreDefenseClass:
      return {ro::core::Tr("Ignore DEF vs"), TargetFamily::kMonsterClass};
    case ro::sim::ConditionalBonusKind::kIgnoreMagicDefenseClass:
      return {ro::core::Tr("Ignore MDEF vs"), TargetFamily::kMonsterClass};
  }
  // ⚠ UN CODE QUE NOUS NE CONNAISSONS PAS SE MONTRE QUAND MÊME, avec son numéro.
  // Le serveur peut en ajouter un trente-troisième ; le taire ferait disparaître
  // un bonus que le joueur porte vraiment, et « Bonus ? » ne dirait pas lequel.
  return {};
}

// Le nom de la cible, dans la famille que le code désigne.
[[nodiscard]] std::string TargetNameOf(const ro::vfs::ItemWords& words, TargetFamily family,
                                       std::int16_t index) {
  switch (family) {
    case TargetFamily::kElement:
      return std::string(ElementName(words, index));
    case TargetFamily::kRace:
      return std::string(RaceName(words, index));
    case TargetFamily::kSize:
      return std::string(SizeName(words, index));
    case TargetFamily::kMonsterClass:
      return std::string(MonsterClassName(index));
    case TargetFamily::kRaceGroup:
      return RaceGroupName(index);
    case TargetFamily::kUnknown:
      break;
  }
  return std::string(kUnnamedTarget);
}

// ── CE QUI TIENT À UNE COMPÉTENCE, OU À UN STATUT ───────────────────────────

// Le nom d'une compétence, ou son numéro si la table ne la connaît pas.
[[nodiscard]] std::string SkillNameOf(const ro::sim::SkillTree* skills, std::uint16_t skill_id) {
  if (skills != nullptr) {
    const ro::sim::SkillNode* node = skills->Find(skill_id);
    if (node != nullptr && !node->display_name.empty()) {
      return node->display_name;
    }
  }
  char named[24] = {};
  std::snprintf(named, sizeof(named), ro::core::Tr("skill #%d"), static_cast<int>(skill_id));
  return named;
}

// Le nom d'un STATUT, ou son numéro si cette installation ne le nomme pas.
//
// ⚠ LE NUMÉRO N'EST PAS UN PIS-ALLER HONTEUX : l'installation nomme huit cent
// deux statuts là où ses identifiants en déclarent mille deux cent
// soixante-deux, et le serveur peut en envoyer un que la table ignore. Le
// numéro se retrouve dans `efstids` ; « ? » ne se retrouverait nulle part.
[[nodiscard]] std::string StatusNameOf(const StatusNamer& status_of, std::uint16_t effect_id) {
  if (status_of) {
    const std::string_view named = status_of(effect_id);
    if (!named.empty()) {
      return std::string(named);
    }
  }
  char numbered[24] = {};
  std::snprintf(numbered, sizeof(numbered), ro::core::Tr("statut #%d"),
                static_cast<int>(effect_id));
  return numbered;
}

// ⚠ CES QUATRE PHRASES SONT SORTIES EN FONCTIONS, et ce n'est pas un goût :
// `core::Tr` veut voir sa chaîne À L'APPEL, et le formateur coupe volontiers un
// appel de sa chaîne quand la ligne est trop longue. Écrites dans le ternaire
// qui choisit entre les deux, elles finiraient par `Tr(` seul sur sa ligne — et
// le contrôle de traduction, qui aurait raison, refuserait le fichier.
[[nodiscard]] std::string_view AutoSpellTooltip(bool when_hit) {
  if (when_hit) {
    return ro::core::Tr("Chance de lancer ce sort automatiquement quand vous êtes touché.");
  }
  return ro::core::Tr("Chance de lancer ce sort automatiquement en attaquant.");
}

[[nodiscard]] std::string_view AddEffectTooltip(bool when_hit) {
  if (when_hit) {
    return ro::core::Tr("Chance d'infliger ce statut à l'attaquant quand vous êtes touché.");
  }
  return ro::core::Tr("Chance d'infliger ce statut à la cible en attaquant.");
}

// Ce qu'un modificateur par-compétence dit de lui-même : son mot, et son unité.
struct SkillModifier {
  std::string_view word;
  ro::ui::BonusUnit unit = ro::ui::BonusUnit::kPercent;
};

[[nodiscard]] SkillModifier ModifierOf(ro::sim::SkillBonusKind kind) {
  using Kind = ro::sim::SkillBonusKind;
  using Unit = ro::ui::BonusUnit;
  switch (kind) {
    case Kind::kSkillSpRate:
      return {ro::core::Tr("Coût SP"), Unit::kPercent};
    // ⚠ LE MÊME MOT, UNE AUTRE UNITÉ : le serveur a deux champs pour le coût en
    // SP — l'un en pour cent, l'autre en points. Les distinguer par le libellé
    // demanderait d'inventer un mot qu'aucun des deux clients n'écrit.
    case Kind::kSkillSpCost:
      return {ro::core::Tr("Coût SP"), Unit::kFlat};
    case Kind::kSkillVariableCastRate:
      return {ro::core::Tr("Cast var."), Unit::kPercent};
    case Kind::kSkillFixedCastRate:
      return {ro::core::Tr("Cast fixe"), Unit::kPercent};
    case Kind::kSkillVariableCastMs:
      return {ro::core::Tr("Cast var."), Unit::kMilliseconds};
    case Kind::kSkillFixedCastMs:
      return {ro::core::Tr("Cast fixe"), Unit::kMilliseconds};
    case Kind::kSkillCooldownMs:
      return {ro::core::Tr("Cooldown"), Unit::kMilliseconds};
    case Kind::kSkillDelayRate:
      return {ro::core::Tr("Délai"), Unit::kPercent};
    case Kind::kSkillHealGiven:
      return {ro::core::Tr("Soin"), Unit::kPercent};
    case Kind::kSkillHealTaken:
      return {ro::core::Tr("Soin reçu"), Unit::kPercent};
    case Kind::kSkillKnockback:
      return {ro::core::Tr("Knockback"), Unit::kFlat};
    default:
      break;
  }
  return {};
}

// Le libellé et l'unité d'une entrée de la liste des compétences.
struct SkillRowText {
  std::string label;
  std::string_view tooltip;
  ro::ui::BonusUnit unit = ro::ui::BonusUnit::kFlat;
};

[[nodiscard]] SkillRowText SkillRowTextOf(const ro::sim::SkillBonus& entry,
                                          const ro::sim::SkillTree* skills,
                                          const StatusNamer& status_of) {
  using Kind = ro::sim::SkillBonusKind;
  using Unit = ro::ui::BonusUnit;
  char label[128] = {};
  switch (static_cast<Kind>(entry.code)) {
    case Kind::kAutoSpell:
    case Kind::kAutoSpellWhenHit: {
      const bool when_hit = static_cast<Kind>(entry.code) == Kind::kAutoSpellWhenHit;
      const std::string cast = SkillNameOf(skills, entry.skill_id);
      if (entry.level > 0) {
        std::snprintf(label, sizeof(label), ro::core::Tr("%s %s Niv %d"),
                      when_hit ? ro::core::Tr("Riposte") : ro::core::Tr("Autocast"), cast.c_str(),
                      static_cast<int>(entry.level));
      } else {
        std::snprintf(label, sizeof(label), "%s %s",
                      when_hit ? ro::core::Tr("Riposte") : ro::core::Tr("Autocast"), cast.c_str());
      }
      return {label, AutoSpellTooltip(when_hit), Unit::kPerMille};
    }
    case Kind::kAutoSpellOnSkill: {
      // ⚠ DEUX NOMS DANS LA MÊME PHRASE, et le second peut manquer : un
      // déclencheur nul veut dire « n'importe lequel » chez le serveur.
      const std::string cast = SkillNameOf(skills, entry.skill_id);
      const std::string trigger = entry.triggered_by_skill != 0
                                      ? SkillNameOf(skills, entry.triggered_by_skill)
                                      : std::string(ro::core::Tr("un skill"));
      if (entry.level > 0) {
        std::snprintf(label, sizeof(label), ro::core::Tr("Autocast %s Niv %d sur %s"), cast.c_str(),
                      static_cast<int>(entry.level), trigger.c_str());
      } else {
        std::snprintf(label, sizeof(label), ro::core::Tr("Autocast %s sur %s"), cast.c_str(),
                      trigger.c_str());
      }
      return {label, ro::core::Tr("Chance de lancer ce sort en utilisant le skill déclencheur."),
              Unit::kPerMille};
    }
    case Kind::kSkillAttack:
      std::snprintf(label, sizeof(label), ro::core::Tr("Dégâts %s"),
                    SkillNameOf(skills, entry.skill_id).c_str());
      return {label, ro::core::Tr("Bonus de dégâts sur ce skill précis (%)."), Unit::kPercent};
    case Kind::kSubSkill:
      std::snprintf(label, sizeof(label), ro::core::Tr("Réduc. %s"),
                    SkillNameOf(skills, entry.skill_id).c_str());
      return {label, ro::core::Tr("Réduit les dégâts subis de ce skill (%)."), Unit::kPercent};
    case Kind::kAddEffect:
    case Kind::kAddEffectWhenHit: {
      const bool when_hit = static_cast<Kind>(entry.code) == Kind::kAddEffectWhenHit;
      std::snprintf(label, sizeof(label), "%s %s",
                    when_hit ? ro::core::Tr("Riposte statut") : ro::core::Tr("Inflige"),
                    StatusNameOf(status_of, entry.skill_id).c_str());
      return {label, AddEffectTooltip(when_hit), Unit::kCentiPercent};
    }
    case Kind::kResistEffect:
      std::snprintf(label, sizeof(label), ro::core::Tr("Résist. %s"),
                    StatusNameOf(status_of, entry.skill_id).c_str());
      return {label, ro::core::Tr("Résistance à ce statut : chance et durée réduites."),
              Unit::kCentiPercent};
    default:
      break;
  }

  const SkillModifier modifier = ModifierOf(static_cast<Kind>(entry.code));
  if (modifier.word.empty()) {
    // ⚠ UN CODE INCONNU GARDE SON NOM DE COMPÉTENCE : c'est la moitié de
    // l'information, et elle suffit à savoir de quoi on parle.
    return {SkillNameOf(skills, entry.skill_id), ro::core::Tr("Bonus lié à un skill."),
            Unit::kFlat};
  }
  std::snprintf(label, sizeof(label), "%.*s %s", static_cast<int>(modifier.word.size()),
                modifier.word.data(), SkillNameOf(skills, entry.skill_id).c_str());
  return {label, ro::core::Tr("Modificateur appliqué à ce skill précis."), modifier.unit};
}

}  // namespace

// CE QUE L'ÉQUIPEMENT APPORTE, trié et nommé.
//
// 🔴 QUARANTE-CINQ APPORTS, DONT PRESQUE TOUS VALENT ZÉRO. Le serveur les pousse
// tous, parce qu'un bloc de taille fixe coûte moins cher qu'une liste ; les
// montrer tous donnerait une colonne de « +0 » où l'on ne verrait plus les trois
// qui comptent. On ne garde que le non-nul.
//
// ⚠ L'ORDRE N'EST PAS ALPHABÉTIQUE : il suit les lots dans lesquels le serveur
// les a ajoutés — offensif, survie, utilitaire, réduction, vol de vie, puis le
// très niche. Le joueur qui change de client relit sa fiche au même endroit.
//
// ⚠ ET `eatk`, `ematk`, `refine_atk`, `refine_def` N'Y SONT PAS : ce sont des
// PARTS d'une valeur déjà montrée — « dont tant vient du raffinage » — et elles
// se liront dans les rangées ATK et DEF, pas dans cette liste.
void BuildBonusRows(const ro::sim::PlayerBonuses& bonuses,
                    std::vector<ro::ui::CharacterBonusRow>& into) {
  into.clear();
  if (!bonuses.told) {
    // ⚠ « PAS ENCORE DIT » N'EST PAS « TOUT À ZÉRO » : le serveur ne parle de
    // cela qu'aux clients qu'il a reconnus, et une section vide vaut mieux
    // qu'une section de zéros qui aurait l'air d'un relevé.
    return;
  }
  const auto add = [&into](std::string_view label, std::int32_t value, std::string_view tooltip,
                           bool percent) {
    if (value == 0) {
      return;
    }
    into.push_back(ro::ui::CharacterBonusRow{
        .label = label,
        .tooltip = tooltip,
        .value = value,
        .unit = percent ? ro::ui::BonusUnit::kPercent : ro::ui::BonusUnit::kFlat});
  };
  const auto Pct = [&add](std::string_view label, std::int32_t value, std::string_view tooltip) {
    add(label, value, tooltip, true);
  };
  const auto Flat = [&add](std::string_view label, std::int32_t value, std::string_view tooltip) {
    add(label, value, tooltip, false);
  };

  Pct(ro::core::Tr("Mêlée"), bonuses.melee_pct, ro::core::Tr("Dégâts de mêlée à mains nues (%)."));
  Pct(ro::core::Tr("Distance"), bonuses.ranged_pct,
      ro::core::Tr("Dégâts des attaques à distance (%)."));
  Pct(ro::core::Tr("Dég. crit."), bonuses.crit_dmg_pct,
      ro::core::Tr("Dégâts des coups critiques (%)."));
  Flat(ro::core::Tr("PV max"), bonuses.hp_add, ro::core::Tr("PV max ajoutés par l'équipement."));
  Flat(ro::core::Tr("SP max"), bonuses.sp_add, ro::core::Tr("SP max ajoutés par l'équipement."));
  Flat(ro::core::Tr("ASPD"), bonuses.aspd_add,
       ro::core::Tr("Vitesse d'attaque ajoutée (valeur plate)."));
  Pct(ro::core::Tr("Cast var."), bonuses.vcast_pct,
      ro::core::Tr("Temps de cast variable (%). Négatif = réduction."));
  Pct(ro::core::Tr("Cast fixe"), bonuses.fcast_pct,
      ro::core::Tr("Temps de cast fixe (%). Négatif = réduction."));
  Pct(ro::core::Tr("ATK %"), bonuses.atk_pct, ro::core::Tr("Bonus d'ATK physique global (%)."));
  Pct(ro::core::Tr("MATK %"), bonuses.matk_pct, ro::core::Tr("Bonus d'ATK magique global (%)."));
  Pct(ro::core::Tr("Renvoi mêlée"), bonuses.dmg_ret_melee,
      ro::core::Tr("Renvoie une part des dégâts de mêlée reçus (%)."));
  Pct(ro::core::Tr("Renvoi dist."), bonuses.dmg_ret_ranged,
      ro::core::Tr("Renvoie une part des dégâts à distance reçus (%)."));
  Pct(ro::core::Tr("Renvoi mag."), bonuses.dmg_ret_magic,
      ro::core::Tr("Renvoie une part des dégâts magiques reçus (%)."));
  Pct(ro::core::Tr("Double att."), bonuses.double_pct,
      ro::core::Tr("Chance de frapper deux fois (%)."));
  Pct(ro::core::Tr("Coup parfait"), bonuses.perfect_hit,
      ro::core::Tr("Chance de coup parfait (%) : ignore FLEE et DEF."));
  Pct(ro::core::Tr("PV max %"), bonuses.hp_pct, ro::core::Tr("Bonus de PV maximum (%)."));
  Pct(ro::core::Tr("SP max %"), bonuses.sp_pct, ro::core::Tr("Bonus de SP maximum (%)."));
  Pct(ro::core::Tr("Régén. PV"), bonuses.hp_regen_pct,
      ro::core::Tr("Récupération naturelle de PV (%)."));
  Pct(ro::core::Tr("Régén. SP"), bonuses.sp_regen_pct,
      ro::core::Tr("Récupération naturelle de SP (%)."));
  Pct(ro::core::Tr("Réduc. crit"), bonuses.crit_def_pct,
      ro::core::Tr("Réduit la probabilité de subir un critique (%)."));
  Flat(ro::core::Tr("PV/kill"), bonuses.hp_on_kill,
       ro::core::Tr("PV récupérés en tuant un ennemi."));
  Flat(ro::core::Tr("SP/kill"), bonuses.sp_on_kill,
       ro::core::Tr("SP récupérés en tuant un ennemi."));
  Pct(ro::core::Tr("Incassable"), bonuses.unbreak_pct,
      ro::core::Tr("Chance d'éviter la casse d'un équipement (%)."));
  Pct(ro::core::Tr("Potions PV"), bonuses.pot_hp_pct,
      ro::core::Tr("Efficacité des items de soin PV (%)."));
  Pct(ro::core::Tr("Potions SP"), bonuses.pot_sp_pct,
      ro::core::Tr("Efficacité des items de soin SP (%)."));
  Pct(ro::core::Tr("Soin donné"), bonuses.heal_up_pct,
      ro::core::Tr("Puissance des soins que vous prodiguez (%)."));
  Pct(ro::core::Tr("Délai skill"), bonuses.delay_pct,
      ro::core::Tr("After-cast delay (%). Négatif = réduction."));
  Flat(ro::core::Tr("Cast var. ms"), bonuses.add_vcast_ms,
       ro::core::Tr("Ajout ou retrait au cast variable, en millisecondes."));
  Flat(ro::core::Tr("Cast fixe ms"), bonuses.add_fcast_ms,
       ro::core::Tr("Ajout ou retrait au cast fixe, en millisecondes."));
  Pct(ro::core::Tr("Vol"), bonuses.steal_pct, ro::core::Tr("Taux de vol d'items (%)."));
  Pct(ro::core::Tr("Réduc. mêlée"), bonuses.def_melee_pct,
      ro::core::Tr("Réduit les dégâts de mêlée reçus (%)."));
  Pct(ro::core::Tr("Réduc. distance"), bonuses.def_ranged_pct,
      ro::core::Tr("Réduit les dégâts à distance reçus (%)."));
  Pct(ro::core::Tr("Réduc. magie"), bonuses.def_magic_pct,
      ro::core::Tr("Réduit les dégâts magiques reçus (%)."));
  Pct(ro::core::Tr("Réduc. divers"), bonuses.def_misc_pct,
      ro::core::Tr("Réduit les dégâts divers reçus (%)."));
  Flat(ro::core::Tr("Splash"), bonuses.splash,
       ro::core::Tr("Portée de la zone d'effet de vos attaques (cases)."));
  Flat(ro::core::Tr("Splash+"), bonuses.splash_add,
       ro::core::Tr("Portée de splash additionnelle (cases)."));
  Pct(ro::core::Tr("Vol PV"), bonuses.hp_drain_pct,
      ro::core::Tr("PV volés à chaque attaque, en pour cent des dégâts."));
  Pct(ro::core::Tr("Vol SP"), bonuses.sp_drain_pct,
      ro::core::Tr("SP volés à chaque attaque, en pour cent des dégâts."));
  Pct(ro::core::Tr("Casse arme"), bonuses.break_weapon_pct,
      ro::core::Tr("Chance de casser l'arme de la cible (%)."));
  Pct(ro::core::Tr("Casse armure"), bonuses.break_armor_pct,
      ro::core::Tr("Chance de casser l'armure de la cible (%)."));
  Pct(ro::core::Tr("Zeny bonus"), bonuses.zeny_bonus_pct,
      ro::core::Tr("Bonus de Zeny obtenu sur les monstres (%)."));
  Pct(ro::core::Tr("Transforme"), bonuses.classchange_pct,
      ro::core::Tr("Chance de transformer la cible en un autre monstre (%)."));
  Pct(ro::core::Tr("Réduc. renvoi"), bonuses.dmg_ret_reduce,
      ro::core::Tr("Réduit les dégâts que vous subissez du renvoi (%)."));
  Flat(ro::core::Tr("PV au sort"), bonuses.magic_hp_gain,
       ro::core::Tr("PV récupérés en lançant un sort."));
  Flat(ro::core::Tr("SP au sort"), bonuses.magic_sp_gain,
       ro::core::Tr("SP récupérés en lançant un sort."));
}

// CE QUI NE VAUT QUE CONTRE QUELQU'UN.
//
// 🔴 LES LIBELLÉS SE COMPOSENT — « Résist. vs » plus « Undead » —, donc ils
// doivent VIVRE quelque part : une rangée ne porte qu'une vue. Elles vivent dans
// `labels`, que l'appelant garde aussi longtemps que les rangées.
//
// ⚠ ET LA LISTE SE REMPLIT EN ENTIER AVANT QU'UNE SEULE VUE NE SOIT PRISE : une
// chaîne courte se range DANS son propre objet, et agrandir le tableau en cours
// de route déplacerait ces objets — les vues déjà prises pointeraient dans le
// vide. Le `reserve` seul suffirait ; les deux passages disent pourquoi.
void BuildConditionalRows(const ro::sim::PlayerBonuses& bonuses, const ro::vfs::ItemWords& words,
                          std::vector<std::string>& labels,
                          std::vector<ro::ui::CharacterBonusRow>& into) {
  labels.clear();
  into.clear();
  if (!bonuses.told) {
    return;
  }

  labels.reserve(bonuses.conditional.size());
  for (const ro::sim::ConditionalBonus& entry : bonuses.conditional) {
    const ConditionalKind kind = KindOf(entry.code);
    if (kind.verb.empty()) {
      char unknown[32] = {};
      std::snprintf(unknown, sizeof(unknown), ro::core::Tr("Bonus #%d"),
                    static_cast<int>(entry.code));
      labels.emplace_back(unknown);
      continue;
    }
    labels.push_back(std::string(kind.verb) + " " + TargetNameOf(words, kind.family, entry.index));
  }

  into.reserve(labels.size());
  for (std::size_t rank = 0; rank < labels.size(); ++rank) {
    into.push_back(ro::ui::CharacterBonusRow{
        .label = labels[rank],
        .tooltip = ro::core::Tr("Bonus conditionnel : ne s'applique que contre ce type de cible."),
        // ⚠ TOUJOURS UN POUR CENT : les trente-deux codes sont des magnitudes
        // relatives, y compris « SP/kill vs », qui est un taux et non des points.
        .value = bonuses.conditional[rank].value,
        .unit = ro::ui::BonusUnit::kPercent});
  }
}

// CE QUI TIENT À UNE COMPÉTENCE, OU À UN STATUT.
//
// ⚠ MÊME RÈGLE DE VIE QUE LES CONDITIONNELS : les libellés se composent, donc
// ils vivent dans `labels`, que l'appelant garde aussi longtemps que les rangées.
void BuildSkillRows(const ro::sim::PlayerBonuses& bonuses, const ro::sim::SkillTree* skills,
                    const StatusNamer& status_of, std::vector<std::string>& labels,
                    std::vector<ro::ui::CharacterBonusRow>& into) {
  labels.clear();
  into.clear();
  if (!bonuses.told) {
    return;
  }
  std::vector<SkillRowText> written;
  written.reserve(bonuses.skills.size());
  labels.reserve(bonuses.skills.size());
  for (const ro::sim::SkillBonus& entry : bonuses.skills) {
    written.push_back(SkillRowTextOf(entry, skills, status_of));
    labels.push_back(written.back().label);
  }
  into.reserve(labels.size());
  for (std::size_t rank = 0; rank < labels.size(); ++rank) {
    into.push_back(ro::ui::CharacterBonusRow{.label = labels[rank],
                                             .tooltip = written[rank].tooltip,
                                             .value = bonuses.skills[rank].value,
                                             .unit = written[rank].unit});
  }
}

// CE QUI TIENT À UN OBJET — un bonus de butin, et rien d'autre à ce jour.
//
// ⚠ LE NOM ARRIVE PAR UNE FONCTION, et ce n'est pas un détour : cette
// bibliothèque ne connaît que `sim` et `ui`, et la table des objets vit dans
// `vfs`. Lui donner le droit de la lire coûterait la propriété qui la rend
// essayable seule.
void BuildItemRows(const ro::sim::PlayerBonuses& bonuses, const ItemNamer& name_of,
                   std::vector<std::string>& labels, std::vector<ro::ui::CharacterBonusRow>& into) {
  labels.clear();
  into.clear();
  if (!bonuses.told) {
    return;
  }
  labels.reserve(bonuses.items.size());
  for (const ro::sim::ItemBonus& entry : bonuses.items) {
    char label[128] = {};
    if (static_cast<ro::sim::ItemBonusKind>(entry.code) == ro::sim::ItemBonusKind::kAddDropGroup) {
      // ⚠ LE CHAMP PORTE UN GROUPE, PAS UN OBJET : le serveur réemploie le même
      // mot, et le passer au nommeur d'objets donnerait le nom d'un objet qui
      // n'a rien à voir.
      std::snprintf(label, sizeof(label), ro::core::Tr("Drop groupe #%u"), entry.item_id);
    } else {
      const std::string named = name_of ? name_of(entry.item_id) : std::string();
      std::snprintf(label, sizeof(label), ro::core::Tr("Drop %s"), named.c_str());
    }
    labels.emplace_back(label);
  }
  into.reserve(labels.size());
  for (std::size_t rank = 0; rank < labels.size(); ++rank) {
    into.push_back(ro::ui::CharacterBonusRow{
        .label = labels[rank],
        .tooltip = ro::core::Tr("Chance de butin supplémentaire sur cet item, en tuant."),
        .value = bonuses.items[rank].rate,
        // ⚠ DE UN À DIX MILLE : le serveur compte ce taux au centième de pour
        // cent, comme les chances de statut.
        .unit = ro::ui::BonusUnit::kCentiPercent});
  }
}

}  // namespace moonclient
