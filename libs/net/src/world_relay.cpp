#include "world_relay.h"

#include "ro/net/actor_guild.h"
#include "ro/net/actor_name.h"
#include "ro/net/area_damage.h"
#include "ro/net/attack_reach.h"
#include "ro/net/bank.h"
#include "ro/net/buying_store.h"
#include "ro/net/card_composition.h"
#include "ro/net/character_notice.h"
#include "ro/net/chat_channels.h"
#include "ro/net/class_change.h"
#include "ro/net/companion.h"
#include "ro/net/craft_session.h"
#include "ro/net/discord_relay.h"
#include "ro/net/flag_graffiti.h"
#include "ro/net/gm_outfit.h"
#include "ro/net/inventory.h"
#include "ro/net/map_mode.h"
#include "ro/net/player_bonuses.h"
#include "ro/net/player_settings.h"
#include "ro/net/player_styles.h"
#include "ro/net/pvp_rank.h"
#include "ro/net/recovery.h"
#include "ro/net/server_rules.h"
#include "ro/net/skill_facts.h"
#include "ro/net/snap.h"
#include "ro/net/storage_facts.h"
#include "ro/net/storage_list.h"
#include "ro/net/talkbox.h"
#include "ro/net/target_info.h"
#include "ro/net/unit_master.h"
#include "ro/net/warp_list.h"
#include "ro/net/world_notice.h"
#include "ro/sim/world.h"

namespace ro::net {

FrameOutcome RelayFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  // 🔴 ON DEMANDE À CHAQUE TRADUCTEUR, ON NE LISTE PAS SES OPCODES ICI. Une
  // seconde liste à tenir d'accord avec celle du traducteur finit toujours par
  // dériver, et sans un mot : un opcode qui n'arrive nulle part ne casse rien —
  // il ne fait rien.
  //
  // ⚠ L'ORDRE COMPTE, et il va du plus précis au plus général : chacun rend
  // `kUnhandled` sur ce qui n'est pas à lui, et le dernier tranche.
  if (const FrameOutcome outcome = ApplyInventoryFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LE SERTISSAGE DES CARTES : la liste proposée, l'accusé qui change le sac, et
  // la liste inverse du sertissage rapide — voir ro/net/card_composition.h.
  if (const FrameOutcome outcome = ApplyCardCompositionFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // L'ATELIER : listes de fabrication et d'affinage, et leurs réponses — voir
  // ro/net/craft_session.h.
  if (const FrameOutcome outcome = ApplyCraftFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LA LISTE DE DESTINATIONS d'une compétence de déplacement, qui attend le
  // choix du joueur — voir ro/net/warp_list.h.
  if (const FrameOutcome outcome = ApplyWarpListFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LE COUP REFUSÉ FAUTE DE PORTÉE, et la poursuite qu'il laisse à faire. Un
  // paquet, et c'est le seul mot du serveur sur le sujet — voir
  // ro/net/attack_reach.h.
  if (const FrameOutcome outcome = ApplyAttackReachFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // CE QUE LA CARTE AUTORISE : qui l'on peut viser, et ce que l'écran montre en
  // plus. Un paquet, qui ne nomme AUCUN acteur et les change tous — voir
  // ro/net/map_mode.h.
  if (const FrameOutcome outcome = ApplyMapModeFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LES ENSEIGNES DES ÉCHOPPES D'ACHAT, posées sur l'acheteur — voir
  // ro/net/buying_store.h.
  if (const FrameOutcome outcome = ApplyBuyingStoreSignboardFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // 🔴 LA VENTE À UN ACHETEUR RETIRE L'OBJET DU SAC, et par ce seul paquet : le
  // serveur n'envoie pas le retrait ordinaire. Le commerce lit la même trame
  // après le monde, pour la dire — voir ro/net/buying_store.h.
  if (const FrameOutcome outcome = ApplyBuyingStoreSaleToBag(frame, world.inventory());
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LE CLASSEMENT PVP DE LA CARTE : rang et nombre de combattants. Il arrive en
  // ZONE, donc on reçoit aussi celui des voisins — voir ro/net/pvp_rank.h.
  if (const FrameOutcome outcome = ApplyPvpRankFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LE DÉTAIL DE CE QUE L'ÉQUIPEMENT APPORTE. Un paquet, et il n'arrive qu'aux
  // clients que le serveur a reconnus — voir ro/net/player_bonuses.h.
  if (const FrameOutcome outcome = ApplyStatBonusFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LA VIE ET LA MAGIE RENDUES AU JOUEUR, avec leur chiffre et leur effet —
  // voir ro/net/recovery.h.
  if (const FrameOutcome outcome = ApplyRecoveryFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // CE QUE LE PERSONNAGE PEUT APPELER : chariot, monture, faucon. Poussé, jamais
  // demandé — voir ro/net/companion.h.
  if (const FrameOutcome outcome = ApplyCompanionFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LES COFFRES QUE CE PERSONNAGE PEUT OUVRIR, et lequel l'est. Poussée au
  // login puis à chaque ouverture — voir ro/net/storage_list.h.
  if (const FrameOutcome outcome = ApplyStorageListFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // CE QUE LE SERVEUR DIT DES OBJETS DU COFFRE : prix, sous-type, emplacement.
  // Il suit la liste du coffre, et lui seul les porte — voir
  // ro/net/storage_facts.h.
  if (const FrameOutcome outcome = ApplyStorageFactsFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // CE QUE LE SERVEUR SEUL SAIT D'UNE COMPÉTENCE : son cast, ses dégâts. ⚠ SUR
  // DEMANDE, jamais spontanément — voir ro/net/skill_facts.h.
  if (const FrameOutcome outcome = ApplySkillFactsFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LA BANQUE DE ZENY : le solde, la bascule de la fenêtre, et le verdict du
  // dernier transfert — voir ro/net/bank.h.
  if (const FrameOutcome outcome = ApplyBankFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LES CANAUX DU SERVEUR, poussés au login et réservés aux clients reconnus.
  // Le client n'a aucune autre source — voir ro/net/chat_channels.h.
  if (const FrameOutcome outcome = ApplyChatChannelFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LE NOM AU-DESSUS D'UN ACTEUR, et ce que le serveur écrit sous lui : deux
  // paquets, l'un pour les joueurs et les monstres, l'autre pour les PNJ — voir
  // ro/net/actor_name.h.
  if (const FrameOutcome outcome = ApplyActorNameFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LA GUILDE D'UN ACTEUR QUI EN CHANGE sous nos yeux — voir
  // ro/net/actor_guild.h. Le carnet de guilde lit la même trame, pour sa
  // version d'emblème.
  if (const FrameOutcome outcome = ApplyActorGuildFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // CE QUE LE SERVEUR SAIT DE L'ENTITÉ DÉSIGNÉE. Deux paquets, tous deux
  // réservés aux clients reconnus, et tous deux demandés par NOUS : il n'y a pas
  // d'abonnement — voir ro/net/target_info.h.
  if (const FrameOutcome outcome = ApplyTargetInfoFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  if (const FrameOutcome outcome = ApplyTargetStatusFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LES RÉGLAGES DE SESSION, et le DROIT qui s'y cache : le serveur y met le
  // niveau de groupe du compte — voir ro/net/player_settings.h.
  if (const FrameOutcome outcome = ApplyPlayerSettingsFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // CE QUI SE DIT SUR LE CANAL DISCORD, relayé à Gonryun, et jeté ici quand le
  // joueur l'a décoché — voir ro/net/discord_relay.h.
  if (const FrameOutcome outcome = ApplyDiscordRelayFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LES DÉGÂTS D'UN SORT DE ZONE, que rien d'autre ne nous rattache : leur
  // source est l'unité qui frappe — voir ro/net/area_damage.h.
  if (const FrameOutcome outcome = ApplyAreaDamageFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LES COULEURS QUE LES JOUEURS ONT COMPOSÉES, par lots — voir
  // ro/net/player_styles.h. ⚠ ELLES NE CHANGENT AUCUN ACTEUR ICI : le lot peut
  // décrire un joueur qui n'est pas encore paru, et c'est `app` qui croise les
  // deux au moment de l'habiller.
  if (const FrameOutcome outcome = ApplyPlayerStylesFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // ET LE STYLISTE QUI OUVRE LA FENÊTRE, par le paquet voisin.
  if (const FrameOutcome outcome = ApplyStyleOpenFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LE MESSAGE D'UNE TALKIE BOX, dit par une unité au sol — voir
  // ro/net/talkbox.h.
  if (const FrameOutcome outcome = ApplyTalkboxFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // L'EMBLÈME D'UN FLAG GRAFFITI, posé au sol — voir ro/net/flag_graffiti.h.
  if (const FrameOutcome outcome = ApplyFlagGraffitiFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LES RÈGLES DU SERVEUR — voir ro/net/server_rules.h.
  if (const FrameOutcome outcome = ApplyServerRulesFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LE MAÎTRE D'UN MONSTRE INVOQUÉ — voir ro/net/unit_master.h.
  if (const FrameOutcome outcome = ApplyUnitMasterFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // L'HABIT DU GM D'UN JOUEUR — voir ro/net/gm_outfit.h.
  if (const FrameOutcome outcome = ApplyGmOutfitFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LE BOND DE BODY RELOCATION — voir ro/net/snap.h.
  if (const FrameOutcome outcome = ApplySnapFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // LA CLASSE D'UN PNJ OU D'UN MONSTRE QUI CHANGE — voir ro/net/class_change.h.
  if (const FrameOutcome outcome = ApplyClassChangeFrame(frame, world);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // L'EXPÉRIENCE GAGNÉE ET L'IDENTIFIANT DE COMPTE — voir
  // ro/net/character_notice.h.
  if (const FrameOutcome outcome = ApplyCharacterNoticeFrame(frame, world, now_ms);
      outcome != FrameOutcome::kUnhandled) {
    return outcome;
  }
  // CE QUE LE SERVEUR DIT : la parole, les bulles, les annonces, l'heure. Aucun
  // de ces paquets ne déplace un corps ni n'en change l'apparence — voir
  // ro/net/world_notice.h.
  return ApplyNoticeFrame(frame, world, now_ms);
}

}  // namespace ro::net
