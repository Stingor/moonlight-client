// Les phrases des refus de compétence. Voir `world/skill_refusal_texts.h`.
//
// ⚠ AUCUNE N'EST TRADUITE ICI : elles sont marquées par `TrText` et traduites à
// l'écriture de la ligne, dans la langue du moment.

#include "world/skill_refusal_texts.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "ro/core/i18n.h"
#include "ro/sim/actor.h"
#include "ro/sim/skill_use_block.h"
#include "ro/sim/status_effects.h"

namespace moonclient {
namespace {

using ro::core::TrText;

// UNE PHRASE PAR MOTIF, au rang du motif : les valeurs de `useskill_fail_cause`
// du serveur, prolongées des quatre que le client connaît au-delà.
//
// Les phrases sont celles de la table de messages de l'installation, en
// français de traduction ; le jargon du jeu reste en anglais.
//
// ⚠ UNE PHRASE NULLE SE TAIT, comme chez le client : ce sont des motifs que le
// serveur n'envoie plus, et celui qui accompagne un message déjà envoyé à part.
//
// ⚠ LE MOTIF ZÉRO NE DIAGNOSTIQUE RIEN : c'est la valeur PAR DÉFAUT du
// paramètre côté serveur, et un délai, un terrain ou un état manquant y
// échouent sous le même numéro qu'un vrai manque de niveau. D'où « Le skill a
// échoué. », et non « niveau insuffisant ».
constexpr std::array kRefusalPhrases = {
    RefusalPhrase{TrText("Le skill a échoué."), RefusalHole::kNone},                    // 0
    RefusalPhrase{TrText("SP insuffisants."), RefusalHole::kNone},                      // 1
    RefusalPhrase{TrText("HP insuffisants."), RefusalHole::kNone},                      // 2
    RefusalPhrase{TrText("Vous n'avez pas assez d'ingrédients."), RefusalHole::kNone},  // 3
    RefusalPhrase{TrText("Impossible d'utiliser [%s] : son délai de réutilisation court encore."),
                  RefusalHole::kSkillName},  // 4
    RefusalPhrase{TrText("Le skill a échoué : vous n'avez pas assez de zeny."),
                  RefusalHole::kNone},  // 5
    RefusalPhrase{TrText("Ce skill ne peut pas être utilisé avec cette arme."),
                  RefusalHole::kNone},                                            // 6
    RefusalPhrase{TrText("Une Red Gemstone est requise."), RefusalHole::kNone},   // 7
    RefusalPhrase{TrText("Une Blue Gemstone est requise."), RefusalHole::kNone},  // 8
    RefusalPhrase{TrText("Vous ne pouvez pas utiliser de skill en surcharge de poids."),
                  RefusalHole::kNone},                                // 9
    RefusalPhrase{TrText("Le skill a échoué."), RefusalHole::kNone},  // 10
    RefusalPhrase{TrText("Ce skill ne peut pas être utilisé sur cette cible."),
                  RefusalHole::kNone},  // 11
    RefusalPhrase{TrText("Impossible d'utiliser le skill : vous dépassez la limite d'Ancilla."),
                  RefusalHole::kNone},                                                      // 12
    RefusalPhrase{TrText("[Holy Water] est requise."), RefusalHole::kNone},                 // 13
    RefusalPhrase{TrText("[Ancilla] est requise."), RefusalHole::kNone},                    // 14
    RefusalPhrase{TrText("Ne peut pas se cumuler à cette distance."), RefusalHole::kNone},  // 15
    RefusalPhrase{TrText("Ce skill demande d'autres skills pour être utilisé."),
                  RefusalHole::kNone},                                                     // 16
    RefusalPhrase{TrText("Ce skill ne peut pas être utilisé seul."), RefusalHole::kNone},  // 17
    RefusalPhrase{TrText("Ce skill ne s'utilise que dans certaines directions."),
                  RefusalHole::kNone},                                                         // 18
    RefusalPhrase{TrText("Impossible d'invoquer davantage de sphères."), RefusalHole::kNone},  // 19
    RefusalPhrase{TrText("Aucune sphère invoquée, ou pas assez de sphères."),
                  RefusalHole::kNone},                                                     // 20
    RefusalPhrase{TrText("Aucun skill imité disponible."), RefusalHole::kNone},            // 21
    RefusalPhrase{TrText("Vous ne pouvez pas réutiliser ce skill."), RefusalHole::kNone},  // 22
    RefusalPhrase{TrText("Ce skill ne peut pas être utilisé dans cet état."),
                  RefusalHole::kNone},                                                  // 23
    RefusalPhrase{TrText("[Paint Brush] est requis."), RefusalHole::kNone},             // 24
    RefusalPhrase{TrText("Utilisable uniquement sur un dragon."), RefusalHole::kNone},  // 25
    RefusalPhrase{TrText("Ce skill ne peut pas être utilisé à cet endroit."),
                  RefusalHole::kNone},                                                         // 26
    RefusalPhrase{TrText("SP insuffisants."), RefusalHole::kNone},                             // 27
    RefusalPhrase{TrText("Utilisable uniquement près d'un mur."), RefusalHole::kNone},         // 28
    RefusalPhrase{TrText("Ce skill demande un pour cent d'expérience."), RefusalHole::kNone},  // 29
    RefusalPhrase{TrText("Pas assez de membres du groupe pour lancer ce chœur."),
                  RefusalHole::kNone},  // 30
    RefusalPhrase{TrText("Utilisable uniquement après Weapon Blocking."),
                  RefusalHole::kNone},  // 31
    RefusalPhrase{TrText("Une arme enduite de poison de Guillotine Cross est requise."),
                  RefusalHole::kNone},  // 32
    RefusalPhrase{TrText("Item utilisable uniquement à bord du Mado Gear."),
                  RefusalHole::kNone},  // 33
    RefusalPhrase{TrText("Des Kunai doivent être équipés pour utiliser ce skill."),
                  RefusalHole::kNone},                                               // 34
    RefusalPhrase{TrText("Ce skill ne vise que les joueurs."), RefusalHole::kNone},  // 35
    RefusalPhrase{TrText("Ce skill n'est disponible que pendant la WoE."),
                  RefusalHole::kNone},                                                       // 36
    RefusalPhrase{TrText("Chargez un [Cannon Ball]."), RefusalHole::kNone},                  // 37
    RefusalPhrase{},                                                                         // 38
    RefusalPhrase{},                                                                         // 39
    RefusalPhrase{TrText("Utilisable uniquement en Hovering."), RefusalHole::kNone},         // 40
    RefusalPhrase{},                                                                         // 41
    RefusalPhrase{},                                                                         // 42
    RefusalPhrase{TrText("Un poison de Guillotine Cross est requis."), RefusalHole::kNone},  // 43
    RefusalPhrase{},                                                                         // 44
    RefusalPhrase{},                                                                         // 45
    RefusalPhrase{},                                                                         // 46
    RefusalPhrase{},                                                                         // 47
    RefusalPhrase{},                                                                         // 48
    RefusalPhrase{},                                                                         // 49
    RefusalPhrase{TrText("Inutilisable à bord du Mado Gear."), RefusalHole::kNone},          // 50
    RefusalPhrase{TrText("[Magic Book] est requis."), RefusalHole::kNone},                   // 51
    RefusalPhrase{TrText("Ce Magic Book est trop difficile : vous vous assoupissez."),
                  RefusalHole::kNone},                                                        // 52
    RefusalPhrase{TrText("Pas assez de points de conservation."), RefusalHole::kNone},        // 53
    RefusalPhrase{TrText("Vous ne pouvez pas lire ce Magic Book."), RefusalHole::kNone},      // 54
    RefusalPhrase{},                                                                          // 55
    RefusalPhrase{},                                                                          // 56
    RefusalPhrase{TrText("Disponible uniquement avec un cart équipé."), RefusalHole::kNone},  // 57
    RefusalPhrase{},                                                                          // 58
    RefusalPhrase{},                                                                          // 59
    RefusalPhrase{TrText("Ne peut plus être utilisé."), RefusalHole::kNone},                  // 60
    RefusalPhrase{},                                                                          // 61
    RefusalPhrase{},                                                                          // 62
    RefusalPhrase{},                                                                          // 63
    RefusalPhrase{},                                                                          // 64
    RefusalPhrase{},                                                                          // 65
    RefusalPhrase{},                                                                          // 66
    RefusalPhrase{},                                                                          // 67
    RefusalPhrase{},                                                                          // 68
    RefusalPhrase{},                                                                          // 69
    RefusalPhrase{},                                                                          // 70
    RefusalPhrase{TrText("[%s] requis en quantité %d."), RefusalHole::kItemAndCount},         // 71
    RefusalPhrase{TrText("Équipez [%s]."), RefusalHole::kItem},                               // 72
    RefusalPhrase{TrText("Ce skill demande [%s]."), RefusalHole::kSkillOfCount},              // 73
    RefusalPhrase{TrText("Demande %d Spirit Sphere."), RefusalHole::kCount},                  // 74
    RefusalPhrase{},                                                                          // 75
    RefusalPhrase{},                                                                          // 76
    RefusalPhrase{},                                                                          // 77
    RefusalPhrase{},                                                                          // 78
    RefusalPhrase{TrText("Vous n'avez aucun esprit invoqué."), RefusalHole::kNone},           // 79
    RefusalPhrase{TrText("Vous manquez de familiarité."), RefusalHole::kNone},                // 80
    RefusalPhrase{TrText("Disponible uniquement dans le style de combat Fighter."),
                  RefusalHole::kNone},  // 81
    RefusalPhrase{TrText("Disponible uniquement dans le style de combat Grappler."),
                  RefusalHole::kNone},  // 82
    RefusalPhrase{
        TrText("Impossible d'ouvrir une boutique ou un salon de discussion à cet endroit."),
        RefusalHole::kNone},                                              // 83
    RefusalPhrase{TrText("Balles insuffisantes."), RefusalHole::kNone},   // 84
    RefusalPhrase{TrText("%d Coins sont requis."), RefusalHole::kCount},  // 85
    RefusalPhrase{},                                                      // 86
    RefusalPhrase{TrText("Impossible d'utiliser un skill sur cette carte."),
                  RefusalHole::kNone},                                                      // 87
    RefusalPhrase{TrText("Les SP de l'invocation ne suffisent pas."), RefusalHole::kNone},  // 88
    RefusalPhrase{TrText("Ce NPC marchand est déjà installé."), RefusalHole::kNone},        // 89
    RefusalPhrase{TrText("Il faut %d balle(s) pour utiliser ce skill."),
                  RefusalHole::kCount},  // 90
    RefusalPhrase{TrText("Il faut %d flèche(s) pour utiliser ce skill."),
                  RefusalHole::kCount},                                                // 91
    RefusalPhrase{TrText("Ce skill ne s'utilise qu'en groupe."), RefusalHole::kNone},  // 92
    RefusalPhrase{TrText("Ce skill ne s'utilise qu'en groupe."), RefusalHole::kNone},  // 93
    RefusalPhrase{TrText("Les SP du partenaire sont insuffisants, ou son skill est désactivé."),
                  RefusalHole::kNone},                                              // 94
    RefusalPhrase{TrText("Aucune arme équipée."), RefusalHole::kNone},              // 95
    RefusalPhrase{TrText("Sphères d'énergie insuffisantes."), RefusalHole::kNone},  // 96
    RefusalPhrase{TrText("Après le déclenchement de Weapon Blocking, certains skills sont "
                         "utilisables un court moment."),
                  RefusalHole::kNone},  // 97
    RefusalPhrase{TrText("Au-delà de %d, plus rien ne peut être installé."),
                  RefusalHole::kCount},                                              // 98
    RefusalPhrase{TrText("Aucun Magic Spellbook disponible."), RefusalHole::kNone},  // 99
    RefusalPhrase{TrText("Vous n'avez pas assez d'AP."), RefusalHole::kNone},        // 100
    RefusalPhrase{TrText("Demande %d Servant Weapons."), RefusalHole::kCount},       // 101
    RefusalPhrase{TrText("Utilisable dans la portée du skill Twinkling Galaxy."),
                  RefusalHole::kNone},                                        // 102
    RefusalPhrase{TrText("Demande de la Soul Energy."), RefusalHole::kNone},  // 103
    RefusalPhrase{TrText("Pas assez d'Amulets."), RefusalHole::kNone},        // 104
    RefusalPhrase{TrText("Vous devez équiper un Gatling Gun ou un Shotgun."),
                  RefusalHole::kNone},  // 105
    RefusalPhrase{TrText("Vous devez équiper un Rifle ou un Revolver."),
                  RefusalHole::kNone},  // 106
    RefusalPhrase{TrText("Vous devez équiper un Grenade Launcher ou un Rifle."),
                  RefusalHole::kNone},  // 107
    RefusalPhrase{TrText("Vous devez équiper un Gatling Gun ou un Revolver."),
                  RefusalHole::kNone},  // 108
    RefusalPhrase{TrText("Vous devez équiper un Shotgun ou un Grenade Launcher."),
                  RefusalHole::kNone},                                             // 109
    RefusalPhrase{TrText("Vous devez équiper un bouclier."), RefusalHole::kNone},  // 110
    RefusalPhrase{TrText("L'ombre de la cible n'a pas de place où se déplacer."),
                  RefusalHole::kNone},  // 111
    RefusalPhrase{TrText("Impossible d'invoquer l'Homunculus S avec votre classe actuelle : passez "
                         "d'abord à une classe supérieure."),
                  RefusalHole::kNone},  // 112
    RefusalPhrase{TrText("Impossible de l'invoquer avec votre classe actuelle : passez d'abord à "
                         "une classe supérieure."),
                  RefusalHole::kNone},  // 113
    RefusalPhrase{
        TrText("Les skills de Strip ne sont pas disponibles pour un personnage en Mado Gear."),
        RefusalHole::kNone},  // 114
    RefusalPhrase{TrText("Vous devez équiper un Shotgun, un Gatling Gun ou un Grenade Launcher."),
                  RefusalHole::kNone},  // 115
};

// Les motifs dont la phrase dépend d'autre chose que de leur numéro.
constexpr std::uint8_t kLevelCause = 0;
// Le délai de réutilisation, et l'échec sans autre précision.
constexpr std::uint8_t kCooldownCause = 4;
constexpr std::uint8_t kFailedCause = 10;
constexpr std::uint8_t kComboSkillCause = 73;
constexpr std::uint8_t kSpiritsCause = 74;
constexpr std::uint8_t kWeaponClassCause = 78;
constexpr std::uint8_t kCurrencyCause = 85;
// Celui dont le paquet porte, à la place du nombre, le numéro d'un message.
constexpr std::uint8_t kMessageByNumberCause = 86;
// « Ce skill ne peut pas être utilisé dans cet état. »
constexpr std::uint8_t kStateCause = 23;

// Les compétences que le motif zéro nomme à part.
constexpr std::uint16_t kBasicSkill = 1;
constexpr std::uint16_t kWarpPortalSkill = 27;
constexpr std::uint16_t kStealSkill = 50;
constexpr std::uint16_t kEnvenomSkill = 52;

// LES GESTES QUE BASIC SKILL DÉVERROUILLE, au rang que le paquet porte : un
// refus de Basic Skill dit lequel manque. Le huitième écrit deux lignes.
constexpr std::array<const char*, 9> kBasicSkillGestures = {
    TrText("Vous n'avez pas assez de Basic Skill pour échanger."),
    TrText("Vous n'avez pas assez de Basic Skill pour utiliser les émotes."),
    TrText("Vous n'avez pas assez de Basic Skill pour vous asseoir."),
    TrText("Vous n'avez pas assez de Basic Skill pour créer un salon de discussion."),
    TrText("Vous n'avez pas assez de Basic Skill pour former un groupe."),
    TrText("Vous n'avez pas assez de skills pour crier."),
    TrText("Vous n'avez pas assez de skills pour le PK."),
    TrText("Vous n'avez pas assez de skills pour juger les manières."),
    TrText("Niveau de skill insuffisant pour rejoindre un groupe."),
};
constexpr std::size_t kMannerGesture = 7;
constexpr std::size_t kJoinPartyGesture = 8;

// LA CLASSE D'ARME QU'UN SKILL EXIGE, telle que le paquet la numérote.
struct WeaponClassPhrase {
  std::int32_t weapon_class = 0;
  const char* french = nullptr;
};
constexpr std::array kWeaponClassPhrases = {
    WeaponClassPhrase{4, TrText("Une [Spear] doit être équipée.")},
    WeaponClassPhrase{11, TrText("[Bow] doit être équipé.")},
    WeaponClassPhrase{13, TrText("[Musical Instrument/Whip] doit être équipé.")},
    WeaponClassPhrase{14, TrText("[Musical Instrument/Whip] doit être équipé.")},
    WeaponClassPhrase{17, TrText("Une arme de type [Revolver] doit être équipée.")},
    WeaponClassPhrase{18, TrText("Une arme de type [Rifle] doit être équipée.")},
    WeaponClassPhrase{19, TrText("Une arme de type [Gatling Gun] doit être équipée.")},
    WeaponClassPhrase{20, TrText("Une arme de type [Shotgun] doit être équipée.")},
    WeaponClassPhrase{21, TrText("Une arme de type [Grenade Launcher] doit être équipée.")},
    WeaponClassPhrase{22, TrText("[Shuriken] doit être équipé.")},
};

// LES ÉTATS QUI INTERDISENT DE LANCER, sous le nom que le jeu leur donne.
struct BlockingStatus {
  ro::sim::StatusEffectId status = 0;
  const char* name = nullptr;
};
constexpr std::array kBlockingStatuses = {
    BlockingStatus{ro::sim::kEfstStoneCurse, "Stone Curse"},
    BlockingStatus{ro::sim::kEfstFreezing, "Frozen"},
    BlockingStatus{ro::sim::kEfstStun, "Stun"},
    BlockingStatus{ro::sim::kEfstSleep, "Sleep"},
    BlockingStatus{ro::sim::kEfstImprison, "White Imprison"},
    BlockingStatus{ro::sim::kEfstSilence, "Silence"},
    BlockingStatus{ro::sim::kEfstBerserk, "Berserk"},
    BlockingStatus{ro::sim::kEfstSteelBody, "Steel Body"},
    BlockingStatus{ro::sim::kEfstDeepSleep, "Deep Sleep"},
    BlockingStatus{ro::sim::kEfstSaturdayNightFever, "Saturday Night Fever"},
    BlockingStatus{ro::sim::kEfstTrickDead, "Play Dead"},
};
constexpr const char* kChaseWalkName = "Chase Walk";

// La phrase d'un refus que le client a retenu lui-même.
RefusalPhrase LocalPhrase(const ro::sim::SkillRefusal& refusal) {
  constexpr RefusalPhrase kUnderStatus{.french = TrText("Impossible d'utiliser un skill sous %s."),
                                       .hole = RefusalHole::kBlockingStatus};
  switch (refusal.local) {
    case ro::sim::LocalRefusal::kNone:
      break;
    case ro::sim::LocalRefusal::kInterrupted:
      return RefusalPhrase{.french = TrText("Incantation interrompue.")};
    case ro::sim::LocalRefusal::kSitting:
      return RefusalPhrase{.french = TrText("Impossible d'utiliser un skill en étant assis.")};
    case ro::sim::LocalRefusal::kMuted:
      return RefusalPhrase{
          .french = TrText("Impossible d'utiliser un skill : vos bonnes manières sont négatives.")};
    case ro::sim::LocalRefusal::kChaseWalk:
      return kUnderStatus;
    case ro::sim::LocalRefusal::kStatus:
      if (refusal.blocking_status == ro::sim::kStatusAllRiding) {
        return RefusalPhrase{.french = TrText("Impossible d'utiliser un skill sur cette monture.")};
      }
      // Un état que la table ne nomme pas dit encore que c'est un état.
      return BlockingStatusName(refusal) != nullptr ? kUnderStatus : kRefusalPhrases[kStateCause];
  }
  return {};
}

RefusalPhrase LevelPhrase(const ro::sim::SkillRefusal& refusal) {
  switch (refusal.skill_id) {
    case kBasicSkill: {
      const auto gesture = static_cast<std::size_t>(refusal.count);
      if (refusal.count < 0 || gesture >= kBasicSkillGestures.size()) {
        return {};
      }
      return RefusalPhrase{.french = kBasicSkillGestures[gesture],
                           .then_french = gesture == kMannerGesture
                                              ? kBasicSkillGestures[kJoinPartyGesture]
                                              : nullptr};
    }
    case kWarpPortalSkill:
      return RefusalPhrase{.french = TrText("Le niveau du skill n'est pas assez élevé.")};
    case kStealSkill:
      return RefusalPhrase{.french = TrText("Steal a échoué.")};
    case kEnvenomSkill:
      return RefusalPhrase{.french = TrText("Envenom a échoué.")};
    default:
      return kRefusalPhrases[kLevelCause];
  }
}

}  // namespace

RefusalPhrase RefusalPhraseOf(const ro::sim::SkillRefusal& refusal) {
  if (refusal.local != ro::sim::LocalRefusal::kNone) {
    return LocalPhrase(refusal);
  }
  switch (refusal.cause) {
    case kLevelCause:
      return LevelPhrase(refusal);
    // Un nombre nul veut dire « sans précision » : la phrase change, elle ne
    // dit pas « 0 ».
    case kComboSkillCause:
      return refusal.count == 0
                 ? RefusalPhrase{.french = TrText("Ce skill demande un skill particulier.")}
                 : kRefusalPhrases[kComboSkillCause];
    case kSpiritsCause:
      return refusal.count == 0 ? RefusalPhrase{.french = TrText("Une Spirit Sphere est requise.")}
                                : kRefusalPhrases[kSpiritsCause];
    case kCurrencyCause:
      return refusal.count <= 0 ? RefusalPhrase{.french = TrText("Un Coin est requis.")}
                                : kRefusalPhrases[kCurrencyCause];
    case kWeaponClassCause:
      for (const WeaponClassPhrase& phrase : kWeaponClassPhrases) {
        if (phrase.weapon_class == refusal.count) {
          return RefusalPhrase{.french = phrase.french};
        }
      }
      return {};
    default:
      break;
  }
  if (refusal.cause >= kRefusalPhrases.size()) {
    return RefusalPhrase{.french = TrText("Le skill a échoué (motif %d)."),
                         .hole = RefusalHole::kCause};
  }
  return kRefusalPhrases[refusal.cause];
}

const char* RefusalLineFrench() { return TrText("%s : %s"); }

const char* BlockingStatusName(const ro::sim::SkillRefusal& refusal) {
  switch (refusal.local) {
    case ro::sim::LocalRefusal::kChaseWalk:
      return kChaseWalkName;
    case ro::sim::LocalRefusal::kStatus:
      for (const BlockingStatus& blocking : kBlockingStatuses) {
        if (blocking.status == refusal.blocking_status) {
          return blocking.name;
        }
      }
      return nullptr;
    default:
      return nullptr;
  }
}

bool RefusalFollowsSkillFailSwitch(const ro::sim::SkillRefusal& refusal) {
  // ⚠ L'INTERRUPTION NE DIT PAS POURQUOI, comme l'échec sans motif : elle se
  // tait avec lui. Les autres refus du client disent ce qui manque.
  if (refusal.local != ro::sim::LocalRefusal::kNone) {
    return refusal.local == ro::sim::LocalRefusal::kInterrupted;
  }
  switch (refusal.cause) {
    case kLevelCause:
      // Seule la phrase générale : Basic Skill, Warp Portal, Steal et Envenom
      // disent toujours la leur.
      return refusal.skill_id != kBasicSkill && refusal.skill_id != kWarpPortalSkill &&
             refusal.skill_id != kStealSkill && refusal.skill_id != kEnvenomSkill;
    case kCooldownCause:
    case kFailedCause:
      return true;
    default:
      return false;
  }
}

std::uint32_t RefusalMessageId(const ro::sim::SkillRefusal& refusal) {
  return refusal.cause == kMessageByNumberCause && refusal.count > 0
             ? static_cast<std::uint32_t>(refusal.count)
             : 0;
}

}  // namespace moonclient
