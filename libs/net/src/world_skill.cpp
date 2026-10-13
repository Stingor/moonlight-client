#include "ro/net/world_skill.h"

#include <algorithm>
#include <cstdint>

#include "ro/core/log.h"
#include "ro/net/skill.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/misc_effect.h"
#include "ro/sim/skill_banner.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/skill_unit_look.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// Le drapeau du Graffiti qui dit que l'unité porte un message.
constexpr std::uint8_t kGraffitiHasMessage = 1;

// La sorte d'apparence qu'un paquet d'apparence change : zéro est la BASE — le
// métier d'un acteur, le numéro d'une unité au sol.
constexpr std::uint8_t kBaseLook = 0;

}  // namespace

FrameOutcome ApplyWorldSkillFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  switch (frame.opcode) {
    // ── Chemin 4 : L'UNITÉ POSÉE AU SOL ────────────────────────────────────
    case proto::SkillUnitNotice::kOpcode:
      return Decoded<proto::SkillUnitNotice>(frame, [&world, now_ms](const auto& packet) {
        // Une unité que le serveur ne veut pas montrer se pose quand même : elle
        // occupe une case, et son retrait passera par le même identifiant.
        sim::SkillUnit unit;
        unit.id = packet.unit_actor_id;
        unit.creator_id = packet.creator_id;
        unit.cell = sim::CellPosition{packet.cell_x, packet.cell_y};
        // CELLE QUE L'ON A POSÉE SOI-MÊME SE MONTRE TOUJOURS, quoi que dise le
        // drapeau ; celle d'un autre, seulement s'il le dit.
        //
        // Le client montre aussi l'unité d'un MEMBRE DU GROUPE, reconnu au numéro
        // de PERSONNAGE de son poseur. Seule une forme plus récente du paquet
        // porte ce numéro, et le serveur ne l'envoie pas ; celle-ci ne le porte
        // pas, et le client cherche alors dans son groupe un mot que rien n'a
        // écrit. Aucune règle ne se tire d'un tel mot : le drapeau et le poseur
        // décident seuls.
        unit.visible = packet.visible != 0 || packet.creator_id == world.player_id();
        // La révélation d'une unité cachée ne vaut que pour les unités 139 à 153
        // et 20852 à 20855. Le serveur ne cache jamais une unité sous son
        // vrai numéro : il l'envoie sous celui de l'unité muette, qu'aucune
        // révélation ne concerne.
        unit.unit_id = unit.visible ? packet.unit_id : sim::kSilentSkillUnit;
        unit.placed_unit_id = unit.unit_id;
        unit.range = packet.range;
        unit.skill_level = packet.skill_level;
        unit.placed_ms = now_ms;
        unit.look_since_ms = now_ms;
        world.PlaceSkillUnit(unit);
        return FrameOutcome::kApplied;
      });

    // BLADE STOP : la prise d'une lame, et son relâchement.
    case proto::ZcBladestop::kOpcode:
      return Decoded<proto::ZcBladestop>(frame, [&world, now_ms](const auto& packet) {
        return world.SetBladeStop(packet.srcId, packet.targetId, packet.flag != 0, now_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    // LE GRAFFITI : une unité qui porte un MESSAGE, tracé au sol tant qu'elle
    // dure. Le client la tient pour visible quoi que dise le paquet ; sans
    // message, c'est une unité comme les autres.
    case proto::PacketGraffitiEntry::kOpcode:
      return Decoded<proto::PacketGraffitiEntry>(frame, [&world, now_ms](const auto& packet) {
        sim::SkillUnit unit;
        unit.id = packet.AID;
        unit.creator_id = packet.creatorAID;
        unit.cell = sim::CellPosition{static_cast<std::uint16_t>(packet.xPos),
                                      static_cast<std::uint16_t>(packet.yPos)};
        unit.unit_id = packet.job;
        unit.placed_unit_id = packet.job;
        unit.placed_ms = now_ms;
        unit.look_since_ms = now_ms;
        if (packet.isContens == kGraffitiHasMessage) {
          unit.scribbled = packet.msg;
        }
        world.PlaceSkillUnit(unit);
        return FrameOutcome::kApplied;
      });

    case proto::ZcSkillDisappear::kOpcode:
      return Decoded<proto::ZcSkillDisappear>(frame, [&world](const auto& packet) {
        return world.RemoveSkillUnit(packet.GID) ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
      });

    // ── Chemin 5 : L'EFFET NU ──────────────────────────────────────────────
    case proto::SpecialEffectNotice::kOpcode:
      return Decoded<proto::SpecialEffectNotice>(frame, [&world, now_ms](const auto& packet) {
        // ⚠ AUCUN TRI ICI, la taille d'un corps y comprise. Elle n'a pas de
        // champ dans le protocole — le serveur l'annonce par un effet, à chaque
        // changement comme à chaque apparition —, et `net` en déduisait
        // autrefois une taille qu'il posait dans le monde. Savoir ce qu'un
        // numéro montre est l'affaire de la table de l'exécutable.
        return world.RecordEffect(packet.actor_id, packet.effect_id, now_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    // 🔴 L'EFFET DIVERS, qui ne dit pas son effet mais son TYPE. C'est le
    // paquet de la MONTÉE DE NIVEAU, du raffinement et de la fabrication : le
    // joueur le voit plusieurs fois par heure, et il n'arrivait nulle part —
    // décodable depuis toujours, traité par aucun répartiteur, donc « trame non
    // traitée » et rien à l'écran.
    //
    // ⚠ LE CHAMP S'APPELLE `effectId` ET N'EN EST PAS UN : le serveur y met un
    // rang de `e_notify_effect`, que le client traduit par un switch fixe. Voir
    // ro/sim/misc_effect.h, qui porte les dix cas.
    case proto::ZcNotifyEffect::kOpcode:
      return Decoded<proto::ZcNotifyEffect>(frame, [&world, now_ms](const auto& packet) {
        // ⚠ SUR SOI, RIEN : le client ne cherche cet acteur que parmi les
        // autres. La montée de niveau du joueur se montre par le paramètre qui
        // la précède — voir ro/sim/level_up_effect.h —, et la semer ici aussi
        // la doublerait.
        if (packet.aid == world.player_id()) {
          return FrameOutcome::kIgnored;
        }
        const std::uint16_t effect = sim::MiscEffectId(packet.effectId);
        if (effect == 0) {
          // La fin de partie ne montre rien chez le client non plus : elle y
          // lève un drapeau. Se taire est donc fidèle, et le dire évite de
          // chercher un effet qui n'existe pas.
          ro::core::LogDebug("effet divers {} : le client n'en montre aucun", packet.effectId);
          return FrameOutcome::kIgnored;
        }
        if (world.RecordEffect(packet.aid, effect, now_ms)) {
          return FrameOutcome::kApplied;
        }
        ro::core::LogWarning(
            "effet divers {} : acteur {} hors de la scène, l'effet ne paraîtra pas",
            packet.effectId, packet.aid);
        return FrameOutcome::kIgnored;
      });

    case proto::ValuedEffectNotice::kOpcode:
      return Decoded<proto::ValuedEffectNotice>(frame, [&world, now_ms](const auto& packet) {
        // Le même effet, plus un NOMBRE qui monte au-dessus de l'acteur.
        //
        // Le client joue l'effet dans TOUS les cas et ne fait paraître le nombre
        // que pour dix numéros — voir EffectNumberColor. Le tri est donc son
        // affaire, pas celle du monde : ici on garde le fait, l'effet et sa
        // valeur.
        //
        // ⚠ Deux de ces dix numéros CHANGENT la vie ou la magie du joueur chez
        // le client, qui les ajoute à son propre compte avant de les montrer.
        // Nous ne tenons ni l'une ni l'autre : rien à ajouter
        // pour l'instant, et le serveur les redit de toute façon par ses
        // paquets de paramètres.
        return world.RecordEffect(packet.actor_id, packet.effect_id, now_ms, packet.value)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcRemoveEffect::kOpcode:
      return Decoded<proto::ZcRemoveEffect>(frame, [&world](const auto& packet) {
        // Le retrait d'un effet nommé. Voir World::RemoveEffect : le monde ne
        // garde que le dernier effet joué, là où le client les empile.
        return world.RemoveEffect(packet.aid, packet.effectId) ? FrameOutcome::kApplied
                                                               : FrameOutcome::kIgnored;
      });

    case proto::ZcSkillScale::kOpcode:
      return Decoded<proto::ZcSkillScale>(frame, [&world, now_ms](const auto& packet) {
        // LA PORTÉE d'un sort en cours d'incantation, centrée sur la case
        // VISÉE — voir sim::SkillScale, et la correction que Moonlight apporte
        // à l'émission.
        //
        // Le serveur ne l'envoie que sous un réglage (show_skill_scale) et pour
        // les seules compétences que sa base marque ShowScale : ne pas la
        // recevoir est le cas ordinaire.
        sim::SkillScale scale;
        scale.skill_id = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.skill_id, 0));
        scale.level = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.skill_lv, 0));
        scale.centre.x = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.x, 0));
        scale.centre.y = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.y, 0));
        scale.started_ms = now_ms;
        scale.duration_ms = packet.casttime;
        return world.RecordSkillScale(packet.AID, scale) ? FrameOutcome::kApplied
                                                         : FrameOutcome::kIgnored;
      });

    // ── Chemin 1 : L'INCANTATION ───────────────────────────────────────────
    case proto::ZcUseskillAck::kOpcode:
      return Decoded<proto::ZcUseskillAck>(frame, [&world, now_ms](const auto& packet) {
        // Le DÉBUT d'une incantation. Le serveur ne dira rien de sa fin : elle
        // s'éteint à son échéance, et seule son interruption a un paquet.
        sim::Cast cast;
        cast.skill_id = packet.skillId;
        cast.target_id = packet.dstId;
        cast.target_cell.x = packet.x;
        cast.target_cell.y = packet.y;
        cast.element = packet.element;
        cast.started_ms = now_ms;
        cast.duration_ms = packet.delayTime;
        // 🔴 ET LE NOM S'ANNONCE AU-DESSUS DU LANCEUR. Le client le fait AVANT
        // même de sortir pour une durée nulle, si bien qu'une compétence
        // instantanée l'annonce aussi — mais pas pour une créature : voir
        // `CastAnnouncedBy`.
        if (const sim::Actor* caster = world.Find(packet.srcId);
            caster != nullptr &&
            sim::CastAnnouncedBy(packet.skillId, caster->look.job, packet.disposable != 0)) {
          world.AnnounceSkill(packet.srcId, packet.skillId, now_ms);
        }
        return world.RecordCast(packet.srcId, cast) ? FrameOutcome::kApplied
                                                    : FrameOutcome::kIgnored;
      });

    case proto::ZcDispel::kOpcode:
      return Decoded<proto::ZcDispel>(frame, [&world, now_ms](const auto& packet) {
        // Malgré son nom, ce paquet ne dissipe rien : le serveur ne s'en sert
        // que pour dire qu'une incantation a été INTERROMPUE
        // (clif_skillcastcancel en est le seul émetteur).
        return world.CancelCast(packet.gid, now_ms) ? FrameOutcome::kApplied
                                                    : FrameOutcome::kIgnored;
      });

    // ── Chemin 2 : LE COUP QUI PORTE ───────────────────────────────────────
    case proto::ZcNotifySkill::kOpcode:
      return Decoded<proto::ZcNotifySkill>(frame, [&world, now_ms](const auto& packet) {
        // Un coup porté par une COMPÉTENCE. Le serveur l'annonce à part, mais
        // c'est le même fait : quelqu'un frappe quelqu'un, la cible bronche et
        // le nombre paraît. Seule la sorte du coup change, et le numéro de la
        // compétence, que le monde garde sans rien en faire.
        sim::Strike strike;
        strike.attacker = packet.AID;
        strike.target = packet.targetID;
        strike.started_ms = now_ms;
        strike.attack_motion_ms = static_cast<std::uint32_t>(std::max(packet.attackMT, 0));
        strike.hurt_motion_ms = static_cast<std::uint32_t>(std::max(packet.attackedMT, 0));
        strike.flinches = packet.damage > 0 &&
                          !Listed(kEnduredBlowActions, static_cast<std::uint8_t>(packet.action));
        strike.damage = packet.damage;
        // Une compétence ne frappe pas de la seconde main : le paquet n'a pas
        // de champ pour elle, et en inventer un ferait paraître un second
        // nombre à zéro.
        strike.hits = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.count, 1));
        strike.type = static_cast<std::uint8_t>(packet.action);
        strike.skill_id = packet.SKID;
        strike.skill_level = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.level, 0));
        strike.announced_level = packet.level;
        world.AnnounceSkill(packet.AID, packet.SKID, now_ms);
        return world.RecordStrike(strike) ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
      });

    case proto::ZcNotifySkillPosition::kOpcode:
      return Decoded<proto::ZcNotifySkillPosition>(frame, [&world, now_ms](const auto& packet) {
        // Le même coup, porté par une compétence visant le SOL. La case visée
        // ne nous sert pas : la cible, elle, est bien nommée.
        sim::Strike strike;
        strike.attacker = packet.AID;
        strike.target = packet.targetID;
        strike.started_ms = now_ms;
        strike.attack_motion_ms = static_cast<std::uint32_t>(std::max(packet.attackMT, 0));
        strike.hurt_motion_ms = static_cast<std::uint32_t>(std::max(packet.attackedMT, 0));
        strike.flinches = packet.damage > 0 &&
                          !Listed(kEnduredBlowActions, static_cast<std::uint8_t>(packet.action));
        strike.damage = packet.damage;
        strike.hits = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.count, 1));
        strike.type = static_cast<std::uint8_t>(packet.action);
        strike.skill_id = packet.SKID;
        strike.skill_level = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.level, 0));
        strike.announced_level = packet.level;
        world.AnnounceSkill(packet.AID, packet.SKID, now_ms);
        return world.RecordStrike(strike) ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
      });

    // ── Chemin 3 : LA COMPÉTENCE SANS DÉGÂTS ───────────────────────────────
    case proto::ZcUseSkill::kOpcode:
      return Decoded<proto::ZcUseSkill>(frame, [&world, now_ms](const auto& packet) {
        // Une compétence qui ne fait pas de dégâts : un soin, un état posé, un
        // renfort. Le champ « niveau » porte le MONTANT quand il y en a un.
        sim::SupportHit hit;
        hit.skill_id = packet.SKID;
        hit.source_id = packet.srcAID;
        hit.amount = packet.level;
        hit.succeeded = packet.result != 0;
        hit.started_ms = now_ms;
        world.AnnounceSkill(packet.srcAID, packet.SKID, now_ms, packet.targetAID,
                            static_cast<std::uint32_t>(packet.level));
        if (world.RecordSupport(packet.targetAID, hit)) {
          return FrameOutcome::kApplied;
        }
        // ⚠ UNE CIBLE INCONNUE AVALAIT LE PAQUET SANS UN MOT, et c'est le
        // dernier maillon muet de la chaîne : le paquet se décode, le
        // répartiteur d'effets ne voit jamais rien, et « je ne vois rien en
        // jeu » ne distingue plus rien. `kIgnored` n'est pas journalisé, à la
        // différence de l'indéchiffrable et du non traité.
        ro::core::LogWarning(
            "compétence {} sans dégâts : cible {} hors de la scène, l'effet ne paraîtra pas",
            packet.SKID, packet.targetAID);
        return FrameOutcome::kIgnored;
      });

    case proto::ZcNotifyGroundskill::kOpcode:
      return Decoded<proto::ZcNotifyGroundskill>(frame, [&world, now_ms](const auto& packet) {
        // Une compétence posée SUR UNE CASE et qui n'y fait pas de dégâts. Le
        // serveur ne dit ni cible ni montant : la case et le niveau, rien de
        // plus.
        sim::GroundSkillUse use;
        use.skill_id = packet.SKID;
        use.level = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.level, 0));
        use.cell.x = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.xPos, 0));
        use.cell.y = static_cast<std::uint16_t>(std::max<std::int16_t>(packet.yPos, 0));
        use.started_ms = now_ms;
        // ⚠ LA BANNIÈRE N'EST PAS POUR TOUT LE MONDE : une créature n'en montre
        // pas à ce paquet — voir `GroundSkillAnnouncedBy`.
        if (const sim::Actor* caster = world.Find(packet.AID);
            caster != nullptr && sim::GroundSkillAnnouncedBy(caster->look.job)) {
          world.AnnounceSkill(packet.AID, packet.SKID, now_ms);
        }
        // ⚠ UN LANCEUR HORS DE VUE N'ÉTEINT PAS LE PAQUET : la case se montre
        // sans lui — voir `UnseenGroundSkillLog`.
        static_cast<void>(world.RecordGroundSkill(packet.AID, use));
        return FrameOutcome::kApplied;
      });

    // ── LE REFUS : rien ne se montre, et c'est justement ce qu'il faut dire ─
    case proto::ZcAckTouseskill::kOpcode:
      return Decoded<proto::ZcAckTouseskill>(frame, [&world, now_ms](const auto& packet) {
        // Seul l'échec se dit : le même paquet sert aussi à annoncer qu'une
        // compétence a bien été lancée.
        if (packet.flag != kSkillUseFailed) {
          return FrameOutcome::kIgnored;
        }
        // L'ATELIER L'APPREND AUSSI : un skill refusé ne rouvrira pas sa liste.
        world.craft().NoteSkillRefused(packet.skillId);
        // ⚠ LA PHRASE NE S'ÉCRIT PAS ICI : elle veut le nom affiché de la
        // compétence et celui de l'objet manquant, que seul `app` connaît.
        // Le grimoire garde le fait, avec tout ce que le paquet en dit.
        world.skill_book().RecordRefusal(sim::SkillRefusal{.skill_id = packet.skillId,
                                                           .cause = packet.cause,
                                                           .count = packet.btype,
                                                           .item_id = packet.itemId,
                                                           .refused_ms = now_ms});
        return FrameOutcome::kApplied;
      });

    case proto::ZcCombodelay::kOpcode:
      return Decoded<proto::ZcCombodelay>(frame, [&world, now_ms](const auto& packet) {
        // LE DÉLAI AVANT LE COUP SUIVANT D'UN ENCHAÎNEMENT : la pose qu'un
        // enchaînement impose à son lanceur dure ce délai.
        return world.RecordComboDelay(packet.AID, packet.delay, now_ms) ? FrameOutcome::kApplied
                                                                        : FrameOutcome::kIgnored;
      });

    case proto::ZcSpirits::kOpcode:
      return Decoded<proto::ZcSpirits>(frame, [&world, now_ms](const auto& packet) {
        // LES SPHÈRES SPIRITUELLES : leur nombre, annoncé aussi à l'entrée en
        // vue d'un personnage qui en porte.
        const bool recorded = world.RecordSpiritSpheres(packet.AID, packet.num, now_ms);
        ro::core::LogInfo("sphères : {} pour l'acteur {}{}", packet.num, packet.AID,
                          recorded ? "" : ", inconnu du monde");
        return recorded ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
      });

    case proto::ZcSpiritsAttribute::kOpcode:
      return Decoded<proto::ZcSpiritsAttribute>(frame, [&world, now_ms](const auto& packet) {
        // LES SPHÈRES ÉLÉMENTAIRES : leur élément et leur nombre.
        return world.RecordSpiritCharm(packet.aid, packet.spiritsType, packet.num, now_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    // DEVOTION : la liste entière des protégés d'un croisé, ou le maître d'un
    // mercenaire. Chaque annonce retend les traits — voir `sim::DevotionList`.
    // ⚠ LA PORTÉE QUE LE PAQUET PORTE NE SERT À RIEN : le client ne la lit pas,
    // et un protégé hors de portée garde son trait.
    case proto::DevotionListNotice::kOpcode:
      return Decoded<proto::DevotionListNotice>(frame, [&world](const auto& packet) {
        return world.TellDevotion(packet.devoter_id, packet.devotee_ids) ? FrameOutcome::kApplied
                                                                         : FrameOutcome::kIgnored;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

FrameOutcome ApplySkillUnitLook(sim::World& world, std::uint32_t instance_id,
                                std::uint8_t look_type, std::uint32_t look, std::uint32_t now_ms) {
  if (look_type != kBaseLook) {
    return FrameOutcome::kIgnored;
  }
  return sim::ChangeSkillUnitLook(world, instance_id, look, now_ms) ? FrameOutcome::kApplied
                                                                    : FrameOutcome::kIgnored;
}

}  // namespace ro::net
