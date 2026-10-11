// Les phrases des états. Voir l'en-tête.
//
// Chaque cas est l'`index` du paquet, l'EFST du serveur, dont le nom suit en
// commentaire. Un nom de skill reste en anglais, comme partout ailleurs.

#include "status_chat_texts.h"

#include "ro/core/i18n.h"

namespace ro::net::status_chat {

const char* SharedPoseText(sim::StatusEffectId effect) {
  switch (effect) {
    case 2:    // EFST_TWOHANDQUICKEN
    case 37:   // EFST_ATTHASTE_POTION1
    case 38:   // EFST_ATTHASTE_POTION2
    case 39:   // EFST_ATTHASTE_POTION3
    case 40:   // EFST_ATTHASTE_INFINITY
    case 68:   // EFST_SPEARQUICKEN
    case 161:  // EFST_ONEHANDQUICKEN
      return core::Tr("La vitesse d'attaque augmente.");
    case 6:  // EFST_ENCHANTPOISON
      return core::Tr("L'arme est temporairement enchantée de l'élément Poison.");
    case 7:  // EFST_POISONREACT
      return core::Tr("Poison React est lancé.");
    case 8:  // EFST_QUAGMIRE
      return core::Tr("La vitesse de déplacement diminue.");
    case 9:  // EFST_ANGELUS
      return core::Tr("Les PV et la DEF augmentent.");
    case 10:  // EFST_BLESSING
      return core::Tr("La STR, l'INT, la DEX et le HIT augmentent.");
    case 12:  // EFST_INC_AGI
    case 41:  // EFST_MOVHASTE_POTION
    case 42:  // EFST_MOVHASTE_INFINITY
      return core::Tr("La vitesse augmente.");
    case 13:  // EFST_DEC_AGI
      return core::Tr("La vitesse diminue.");
    case 14:  // EFST_SLOWPOISON
      return core::Tr("Slow Poison est lancé.");
    case 15:  // EFST_IMPOSITIO
      return core::Tr("L'attaque physique et l'attaque magique augmentent.");
    case 16:  // EFST_SUFFRAGIUM
      return core::Tr("Le délai d'incantation diminue.");
    case 17:  // EFST_ASPERSIO
    case 64:  // EFST_WEAPONPROPERTY
      return core::Tr("L'arme est temporairement enchantée de l'élément Sacré.");
    case 18:  // EFST_BENEDICTIO
      return core::Tr("L'armure est temporairement enchantée de l'élément Sacré.");
    case 19:  // EFST_KYRIE
      return core::Tr("Une barrière se forme.");
    case 20:  // EFST_MAGNIFICAT
      return core::Tr("La régénération des SP augmente.");
    case 21:  // EFST_GLORIA
      return core::Tr("LUK accrue.");
    case 22:  // EFST_LEXAETERNA
      return core::Tr("La prochaine attaque reçue fera des dégâts doublés.");
    case 23:   // EFST_ADRENALINE
    case 147:  // EFST_ADRENALINE2
      return core::Tr("Vitesse d'attaque accrue.");
    case 24:  // EFST_WEAPONPERFECT
      return core::Tr("Weapon Perfection est lancé.");
    case 25:   // EFST_OVERTHRUST
    case 188:  // EFST_OVERTHRUSTMAX
      return core::Tr("Les dégâts de l'arme augmentent.");
    case 26:  // EFST_MAXIMIZE
      return core::Tr("Maximize Power est lancé.");
    case 30:  // EFST_SHOUT
      return core::Tr("La STR et l'ATK augmentent.");
    case 31:  // EFST_ENERGYCOAT
      return core::Tr("Energy Coat est lancé.");
    case 32:  // EFST_BROKENARMOR
      return core::Tr("L'armure est endommagée.");
    case 33:  // EFST_BROKENWEAPON
      return core::Tr("L'arme est endommagée.");
    case 241:  // EFST_FOOD_STR
    case 271:  // EFST_FOOD_STR_CASH
      return core::Tr("La STR augmente.");
    case 242:  // EFST_FOOD_AGI
    case 272:  // EFST_FOOD_AGI_CASH
      return core::Tr("L'AGI augmente.");
    case 243:  // EFST_FOOD_VIT
    case 273:  // EFST_FOOD_VIT_CASH
      return core::Tr("La VIT augmente.");
    case 244:  // EFST_FOOD_DEX
    case 274:  // EFST_FOOD_DEX_CASH
      return core::Tr("La DEX augmente.");
    case 245:  // EFST_FOOD_INT
    case 275:  // EFST_FOOD_INT_CASH
      return core::Tr("L'INT augmente.");
    case 246:  // EFST_FOOD_LUK
    case 276:  // EFST_FOOD_LUK_CASH
      return core::Tr("La LUK augmente.");
    case 247:  // EFST_FOOD_BASICAVOIDANCE
      return core::Tr("Le taux d'esquive (Flee) augmente.");
    case 248:  // EFST_FOOD_BASICHIT
      return core::Tr("Le taux de précision (Hit) augmente.");
    case 249:  // EFST_FOOD_CRITICALSUCCESSVALUE
      return core::Tr("Le taux de coup critique (Critical) augmente.");
    case 251:  // EFST_CASH_DEATHPENALTY
      return core::Tr(
          "Ce personnage ne perdra pas d'EXP s'il meurt dans les 30 prochaines minutes.");
    case 253:  // EFST_CASH_BOSS_ALARM
      return core::Tr("Les boss de la carte sont révélés pendant les 10 prochaines minutes.");
    default:
      return nullptr;
  }
}

const char* SharedLiftText(sim::StatusEffectId effect) {
  switch (effect) {
    case 2:    // EFST_TWOHANDQUICKEN
    case 37:   // EFST_ATTHASTE_POTION1
    case 38:   // EFST_ATTHASTE_POTION2
    case 39:   // EFST_ATTHASTE_POTION3
    case 68:   // EFST_SPEARQUICKEN
    case 161:  // EFST_ONEHANDQUICKEN
      return core::Tr("La vitesse d'attaque diminue.");
    case 6:   // EFST_ENCHANTPOISON
    case 17:  // EFST_ASPERSIO
    case 64:  // EFST_WEAPONPROPERTY
      return core::Tr("L'arme retrouve son état normal.");
    case 7:  // EFST_POISONREACT
      return core::Tr("Poison React est annulé.");
    case 8:  // EFST_QUAGMIRE
      return core::Tr("Quagmire est annulé.");
    case 9:  // EFST_ANGELUS
      return core::Tr("Angelus est annulé.");
    case 10:  // EFST_BLESSING
      return core::Tr("Blessing est annulé.");
    // ⚠ UNE POTION DE VITESSE QUI S'ÉTEINT RALENTIT, et la baisse d'AGI qui
    // s'éteint accélère : le client écrit le sens du changement, pas l'état.
    case 12:  // EFST_INC_AGI
    case 40:  // EFST_ATTHASTE_INFINITY
    case 41:  // EFST_MOVHASTE_POTION
    case 42:  // EFST_MOVHASTE_INFINITY
      return core::Tr("La vitesse diminue.");
    case 13:  // EFST_DEC_AGI
      return core::Tr("La vitesse augmente.");
    case 14:  // EFST_SLOWPOISON
      return core::Tr("Slow Poison est annulé.");
    case 15:  // EFST_IMPOSITIO
      return core::Tr("L'attaque physique et l'attaque magique diminuent.");
    case 16:  // EFST_SUFFRAGIUM
      return core::Tr("Le délai d'incantation revient à la normale.");
    case 18:  // EFST_BENEDICTIO
      return core::Tr("L'armure retrouve son état normal.");
    case 19:  // EFST_KYRIE
      return core::Tr("La barrière est annulée.");
    case 20:  // EFST_MAGNIFICAT
      return core::Tr("Magnificat est annulé.");
    case 21:  // EFST_GLORIA
      return core::Tr("Gloria est annulé.");
    case 22:  // EFST_LEXAETERNA
      return core::Tr("Lex Aeterna est annulé.");
    case 23:   // EFST_ADRENALINE
    case 147:  // EFST_ADRENALINE2
      return core::Tr("Vitesse d'attaque réduite.");
    case 24:  // EFST_WEAPONPERFECT
      return core::Tr("Weapon Perfection est annulé.");
    case 25:   // EFST_OVERTHRUST
    case 188:  // EFST_OVERTHRUSTMAX
      return core::Tr("Les dégâts de l'arme diminuent.");
    case 26:  // EFST_MAXIMIZE
      return core::Tr("Maximize Power est annulé.");
    case 30:  // EFST_SHOUT
      return core::Tr("Crazy Uproar est annulé.");
    case 31:  // EFST_ENERGYCOAT
      return core::Tr("Energy Coat est annulé.");
    case 241:  // EFST_FOOD_STR
    case 271:  // EFST_FOOD_STR_CASH
      return core::Tr("La STR revient à la normale.");
    case 242:  // EFST_FOOD_AGI
    case 272:  // EFST_FOOD_AGI_CASH
      return core::Tr("L'AGI revient à la normale.");
    case 243:  // EFST_FOOD_VIT
    case 273:  // EFST_FOOD_VIT_CASH
      return core::Tr("La VIT revient à la normale.");
    case 244:  // EFST_FOOD_DEX
    case 274:  // EFST_FOOD_DEX_CASH
      return core::Tr("La DEX revient à la normale.");
    case 245:  // EFST_FOOD_INT
    case 275:  // EFST_FOOD_INT_CASH
      return core::Tr("L'INT revient à la normale.");
    case 246:  // EFST_FOOD_LUK
    case 276:  // EFST_FOOD_LUK_CASH
      return core::Tr("La LUK revient à la normale.");
    case 247:  // EFST_FOOD_BASICAVOIDANCE
      return core::Tr("Le taux d'esquive revient à la normale.");
    case 248:  // EFST_FOOD_BASICHIT
      return core::Tr("Le taux de précision revient à la normale.");
    case 249:  // EFST_FOOD_CRITICALSUCCESSVALUE
      return core::Tr("Le taux de coup critique revient à la normale.");
    default:
      return nullptr;
  }
}

Phrase TimedPosePhrase(sim::StatusEffectId effect) {
  switch (effect) {
    case 1152:  // EFST_CLIMAX
      return {core::Tr("Les effets de certains skills changent."), kPoseColor};
    case 1160:  // EFST_POWERFUL_FAITH
      return {core::Tr("L'ATK et la P.ATK augmentent."), kPoseColor};
    case 1161:  // EFST_SINCERE_FAITH
      return {core::Tr("La vitesse d'attaque et la chance de Perfect Hit augmentent."), kPoseColor};
    case 1162:  // EFST_FIRM_FAITH
      return {core::Tr("Les PV et la RES augmentent."), kPoseColor};
    case 1192:  // EFST_SHADOW_EXCEED
      return {core::Tr("Les effets de certains skills reviennent à la normale."), kPoseColor};
    case 1194:  // EFST_POTENT_VENOM
      return {core::Tr("L'effet qui ignore la RES est accordé."), kPoseColor};
    case 1198:  // EFST_A_VITA
      return {core::Tr("L'effet qui ignore la M.RES est accordé."), kPoseColor};
    case 1199:  // EFST_A_TELUM
      return {core::Tr("L'effet qui ignore la RES vous est accordé."), kPoseColor};
    case 1200:  // EFST_PRE_ACIES
      return {core::Tr("La chance de dégâts critiques augmente."), kPoseColor};
    case 1201:  // EFST_COMPETENTIA
      return {core::Tr("La P.ATK et la S.MATK augmentent."), kPoseColor};
    case 1202:  // EFST_GUARD_STANCE
      return {core::Tr("La défense augmente et la puissance d'attaque diminue."), kPoseColor};
    case 1203:  // EFST_ATTACK_STANCE
      return {core::Tr("La puissance d'attaque augmente et la défense diminue."), kPoseColor};
    case 1204:  // EFST_GUARDIAN_S
      return {core::Tr("Guardian Barrier est accordé."), kPoseColor};
    case 1217:  // EFST_REBOUND_S
      return {core::Tr("Les dégâts reçus par l'effet de Devotion diminuent."), kPoseColor};
    case 1220:  // EFST_HOLY_S
      return {core::Tr("Les dégâts de magie Sacré et la résistance aux éléments Ombre et "
                       "Mort-vivant augmentent."),
              kPoseColor};
    case 1221:  // EFST_ULTIMATE_S
      return {core::Tr("Un effet de résurrection instantanée est accordé."), kPoseColor};
    case 1226:  // EFST_SHADOW_WEAPON
      return {core::Tr("Chaque attaque physique peut infliger Shadow Wound à l'ennemi."),
              kPoseColor};
    case 1227:  // EFST_RELIGIO
      return {core::Tr("La SPL, la WIS et la CRT augmentent."), kPoseColor};
    case 1228:  // EFST_BENEDICTUM
      return {core::Tr("La POW, la CRT et la CON augmentent."), kPoseColor};
    case 1236:  // EFST_A_MACHINE
      return {core::Tr("L'Attack Device est activé."), kPoseColor};
    case 1237:  // EFST_D_MACHINE
      return {core::Tr("La DEF et la RES augmentent."), kPoseColor};
    case 1245:  // EFST_ABYSS_SLAYER
      return {core::Tr("La P.ATK, la S.MATK et le HIT augmentent."), kPoseColor};
    case 1247:  // EFST_PROTECTSHADOWEQUIP
      return {core::Tr("L'équipement Shadow est protégé de la destruction et du désarmement."),
              kPoseColor};
    case 1248:  // EFST_RESEARCHREPORT
      return {core::Tr("Le statut Research Report s'affichera."), kPoseColor};
    case 1249:  // EFST_BO_HELL_DUSTY
      return {core::Tr("Les dégâts contre les monstres de race Plante et Informe augmentent."),
              kPoseColor};
    case 1250:  // EFST_WINDSIGN
      return {core::Tr("Le vent révèle votre faiblesse et votre apparence."), kLiftColor};
    case 1256:  // EFST_MYSTIC_SYMPHONY
      return {core::Tr("L'effet de Mystic Symphony est accordé."), kPoseColor};
    case 1257:  // EFST_KVASIR_SONATA
      return {core::Tr("Vous pouvez désormais lancer seul les skills d'ensemble."), kPoseColor};
    case 1259:  // EFST_GEF_NOCTURN
      return {core::Tr("La M.RES diminue."), kLiftColor};
    case 1260:  // EFST_AIN_RHAPSODY
      return {core::Tr("La RES diminue."), kLiftColor};
    case 1261:  // EFST_MUSICAL_INTERLUDE
      return {core::Tr("La RES augmente."), kPoseColor};
    case 1262:  // EFST_JAWAII_SERENADE
      return {core::Tr("La S.MATK et la vitesse de déplacement augmentent."), kPoseColor};
    case 1263:  // EFST_PRON_MARCH
      return {core::Tr("La P.ATK et la vitesse de déplacement augmentent."), kPoseColor};
    case 1271:  // EFST_SPELL_ENCHANTING
      return {core::Tr("La S.MATK augmente."), kPoseColor};
    default:
      return {};
  }
}

Phrase BareLiftPhrase(sim::StatusEffectId effect) {
  switch (effect) {
    case 1152:  // EFST_CLIMAX
      return {core::Tr("L'effet de Climax est annulé."), kLiftColor};
    case 1160:  // EFST_POWERFUL_FAITH
      return {core::Tr("L'effet de Powerful Faith est annulé."), kLiftColor};
    case 1161:  // EFST_SINCERE_FAITH
      return {core::Tr("Sincere Faith est annulé."), kLiftColor};
    case 1162:  // EFST_FIRM_FAITH
      return {core::Tr("L'effet de Firm Faith est annulé."), kLiftColor};
    case 1172:  // EFST_SERVANTWEAPON
      return {core::Tr("L'effet de Servant Weapon est annulé."), kLiftColor};
    case 1174:  // EFST_CHARGINGPIERCE
      return {core::Tr("L'effet de Charging Pierce est annulé."), kLiftColor};
    case 1178:  // EFST_VIGOR
      return {core::Tr("L'effet de Vigor est annulé."), kLiftColor};
    case 1192:  // EFST_SHADOW_EXCEED
      return {core::Tr("L'effet de Shadow Exceed est annulé."), kLiftColor};
    case 1194:  // EFST_POTENT_VENOM
      return {core::Tr("L'effet de Potent Venom est annulé."), kLiftColor};
    case 1198:  // EFST_A_VITA
      return {core::Tr("L'effet d'Argutus Vita est annulé."), kLiftColor};
    case 1199:  // EFST_A_TELUM
      return {core::Tr("L'effet d'Argutus Telum est annulé."), kLiftColor};
    case 1200:  // EFST_PRE_ACIES
      return {core::Tr("L'effet de Presens Acies est annulé."), kLiftColor};
    case 1201:  // EFST_COMPETENTIA
      return {core::Tr("L'effet de Competentia est annulé."), kLiftColor};
    case 1202:  // EFST_GUARD_STANCE
      return {core::Tr("L'effet de Guard Stance est annulé."), kLiftColor};
    case 1203:  // EFST_ATTACK_STANCE
      return {core::Tr("L'effet d'Attack Stance est annulé."), kLiftColor};
    case 1204:  // EFST_GUARDIAN_S
      return {core::Tr("L'effet de Guardian Barrier est annulé."), kLiftColor};
    case 1217:  // EFST_REBOUND_S
      return {core::Tr("L'effet de Rebound Shield est annulé."), kLiftColor};
    case 1220:  // EFST_HOLY_S
      return {core::Tr("L'effet de Holy Shield est annulé."), kLiftColor};
    case 1221:  // EFST_ULTIMATE_S
      return {core::Tr("Ultimate Sacrifice est annulé."), kLiftColor};
    case 1226:  // EFST_SHADOW_WEAPON
      return {core::Tr("L'effet d'Enchanting Shadow est annulé."), kLiftColor};
    case 1227:  // EFST_RELIGIO
      return {core::Tr("Religio est annulé."), kLiftColor};
    case 1228:  // EFST_BENEDICTUM
      return {core::Tr("L'effet de Benedictum est annulé."), kLiftColor};
    case 1236:  // EFST_A_MACHINE
      return {core::Tr("L'Attack Device est désactivé."), kLiftColor};
    case 1237:  // EFST_D_MACHINE
      return {core::Tr("Le Defense Device est désactivé."), kLiftColor};
    case 1244:  // EFST_ABYSSFORCEWEAPON
      return {core::Tr("L'effet de From the Abyss est annulé."), kLiftColor};
    case 1245:  // EFST_ABYSS_SLAYER
      return {core::Tr("L'effet d'Abyss Slayer est annulé."), kLiftColor};
    case 1247:  // EFST_PROTECTSHADOWEQUIP
      return {core::Tr("La Full Shadow Protection est annulée."), kLiftColor};
    case 1248:  // EFST_RESEARCHREPORT
      return {core::Tr("Le statut Research Report est levé."), kLiftColor};
    case 1249:  // EFST_BO_HELL_DUSTY
      return {core::Tr("L'effet de Hell Tree Powder disparaît."), kLiftColor};
    case 1256:  // EFST_MYSTIC_SYMPHONY
      return {core::Tr("L'effet de Mystic Symphony est annulé."), kLiftColor};
    case 1257:  // EFST_KVASIR_SONATA
      return {core::Tr("L'effet de Sonata of Kvasir est annulé."), kLiftColor};
    // ⚠ CES DEUX-LÀ SONT DES MALUS : leur levée s'écrit en cyan.
    case 1259:  // EFST_GEF_NOCTURN
      return {core::Tr("L'effet de Geffenia Nocturne est annulé."), kPoseColor};
    case 1260:  // EFST_AIN_RHAPSODY
      return {core::Tr("L'effet de Rhapsody of Mineworker est annulé."), kPoseColor};
    case 1261:  // EFST_MUSICAL_INTERLUDE
      return {core::Tr("L'effet de Musical Interlude est annulé."), kLiftColor};
    case 1262:  // EFST_JAWAII_SERENADE
      return {core::Tr("L'effet de Serenade of Jawaii est annulé."), kLiftColor};
    case 1263:  // EFST_PRON_MARCH
      return {core::Tr("L'effet de March of Prontera est annulé."), kLiftColor};
    case 1271:  // EFST_SPELL_ENCHANTING
      return {core::Tr("L'effet de Spell Enchanting est annulé."), kLiftColor};
    default:
      return {};
  }
}

}  // namespace ro::net::status_chat
