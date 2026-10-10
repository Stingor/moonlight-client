#include "ro/net/character.h"

#include <array>
#include <cstdint>
#include <utility>

#include "ro/core/log.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// CE QUE LE VERDICT D'UN PARAMÈTRE DEVIENT POUR LE RELEVÉ DES TRAMES.
//
// ⚠ LES TROIS CAS NE SE RÉDUISENT PAS À DEUX. Un paramètre qu'on a REGARDÉ et
// délibérément laissé de côté — le karma, la politesse — est « ignoré » : la
// décision est prise et se relit. Un paramètre qu'on n'a jamais classé est
// « non traité », c'est-à-dire du travail qui reste. Les confondre ferait
// disparaître le second du relevé, qui est la seule liste disant ce qu'il reste
// à porter.
[[nodiscard]] FrameOutcome OutcomeOfParameter(sim::ParameterOutcome outcome,
                                              std::uint16_t parameter) {
  // ⚠ LE RELEVÉ DES TRAMES COMPTE PAR OPCODE, PAS PAR CAUSE, et pour ce
  // paquet-ci cela ne suffit pas : il dit « vingt-quatre perdues sur 0x00B0 »
  // sans dire LESQUELLES, alors que chacune est un paramètre différent et un
  // travail différent. Le numéro s'écrit donc au journal, où la fenêtre le
  // montre, et « vingt-quatre » devient une liste de noms à relever chez le
  // serveur.
  //
  // AU NIVEAU DÉTAIL : ce n'est pas une anomalie, c'est du travail qui reste, et
  // il se lit quand on le cherche.
  //
  // 🔴 CE NIVEAU N'EST PAS ÉTEINT POUR AUTANT, contrairement à ce qui était écrit
  // ici : `InitLogging` pose `set_level(debug)` ET `flush_on(debug)`, si bien
  // que chacune de ces lignes part sur le disque. ⚠ ET RIEN NE DÉDOUBLONNE
  // CELLE-CI : un numéro non porté que le serveur renvoie à chaque battement de
  // régénération se réécrit autant de fois. Le témoin manque parce que son seul
  // logement propre est `sim::World`, qui touche son plafond de lignes.
  if (outcome == sim::ParameterOutcome::kUnknown) {
    ro::core::LogDebug("paramètre non classé : {} (voir enum _sp du serveur)", parameter);
  }
  switch (outcome) {
    case sim::ParameterOutcome::kStored:
      return FrameOutcome::kApplied;
    case sim::ParameterOutcome::kKnownButUnused:
      return FrameOutcome::kIgnored;
    case sim::ParameterOutcome::kUnknown:
      return FrameOutcome::kUnhandled;
  }
  return FrameOutcome::kUnhandled;
}

// Le serveur dit OUI par un, et non par zéro.
//
// ⚠ C'EST SA TROISIÈME CONVENTION SUR UN CHAMP NOMMÉ « ok ». Porter un objet
// réussit par ZÉRO, le retirer aussi, et celui-ci par UN. Chaque paquet a la
// sienne, aucune ne se devine, et chacune se relève chez le serveur.
constexpr std::uint8_t kStatUpAccepted = 1;

}  // namespace

FrameOutcome ApplyCharacterFrame(const Frame& frame, sim::World& world) {
  switch (frame.opcode) {
    case proto::ZcParChange::kOpcode:
      return Decoded<proto::ZcParChange>(frame, [&world](const auto& packet) {
        // ⚠ CE PAQUET NE NOMME PERSONNE : il ne parle que du joueur, et c'est
        // pourquoi il n'y a aucun identifiant à lire. Le serveur l'émet pour
        // CHAQUE valeur qui change, une à la fois.
        return OutcomeOfParameter(world.SetPlayerParameter(packet.varID, packet.count),
                                  packet.varID);
      });

    case proto::ZcStatusChange::kOpcode:
      return Decoded<proto::ZcStatusChange>(frame, [&world](const auto& packet) {
        // 🔴 LE PRIX DU POINT SUIVANT d'une des six, sur un octet. C'est par
        // elle, et elle seule, que le serveur le redit après une montée ou une
        // remise à zéro : sans elle, le coût connu reste celui de la connexion,
        // ou zéro — et les boutons qui le paient restent éteints.
        return OutcomeOfParameter(world.SetPlayerParameter(packet.statusID, packet.value),
                                  packet.statusID);
      });

    case proto::ZcLongparChange::kOpcode:
      return Decoded<proto::ZcLongparChange>(frame, [&world](const auto& packet) {
        // 🔴 LE TROISIÈME, ET C'EST CELUI DE LA BOURSE. Il ne se distingue du
        // premier ni par sa taille ni par sa forme — mêmes huit octets, mêmes
        // deux champs —, seulement par son opcode, et le serveur réserve
        // celui-ci à ce qui ne tient pas dans un entier SIGNÉ de seize bits.
        // C'est par là que passe `SP_ZENY` (`clif_longpar_change`), et le
        // laisser tomber affichait la sentinelle « pas encore dit » à la place
        // du montant.
        return OutcomeOfParameter(world.SetPlayerParameter(packet.varID, packet.amount),
                                  packet.varID);
      });

    case proto::ZcLonglongparChange::kOpcode:
      return Decoded<proto::ZcLonglongparChange>(frame, [&world](const auto& packet) {
        // LE MÊME PAQUET, EN SOIXANTE-QUATRE BITS. Le serveur a dû le dédoubler
        // parce que la bourse et l'expérience débordent d'un entier de
        // trente-deux bits ; les identifiants de paramètre sont les mêmes, et
        // c'est la même table qui les range.
        return OutcomeOfParameter(world.SetPlayerParameter(packet.varID, packet.amount),
                                  packet.varID);
      });

    case proto::ZcCouplestatus::kOpcode:
      return Decoded<proto::ZcCouplestatus>(frame, [&world](const auto& packet) {
        // UNE CARACTÉRISTIQUE ET SON BONUS, ensemble : la valeur de base que le
        // personnage a montrée, et ce que l'équipement y ajoute. Les montrer
        // séparément est tout l'intérêt — « 40 + 12 » dit d'où vient le total.
        //
        // ⚠ SON `statusType` EST NUMÉROTÉ COMME `enum _sp`, donc SP_STR pour la
        // force et non zéro.
        return world.SetPlayerAttribute(static_cast<std::uint16_t>(packet.statusType),
                                        packet.defaultStatus, packet.plusStatus)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kUnhandled;
      });

    case proto::ZcStatus::kOpcode:
      return Decoded<proto::ZcStatus>(frame, [&world](const auto& packet) {
        // LA FEUILLE ENTIÈRE EN UNE TRAME. Le serveur l'envoie à l'entrée en jeu,
        // là où `ZC_PAR_CHANGE` n'annonce qu'une valeur à la fois.
        //
        // ⚠ ELLE PASSE PAR LA MÊME PORTE, paramètre par paramètre, et ce n'est pas
        // de la paresse : `SetPlayerParameter` est le seul endroit qui sache où
        // va chaque valeur — la vie sur l'acteur, le reste sur le personnage.
        // Une seconde voie qui rangerait directement finirait par diverger.
        //
        // 🔴 LE SERVEUR INVERSE LA PAIRE DES MATK, ET ELLE SEULE. Pour l'attaque,
        // la défense et la défense magique, `SP_*1` porte le côté GAUCHE et
        // `SP_*2` le côté droit. Pour la magie, `clif_updatestatus` fait le
        // contraire : `SP_MATK1` porte `pc_rightside_matk` — le MAXIMUM — et
        // `SP_MATK2` le côté gauche.
        //
        // Les noms du paquet, eux, sont honnêtes : `clif_initialstatus` écrit
        // `max_mattPower = pc_rightside_matk`. Le maximum va donc dans
        // `SP_MATK1`, et lire ces deux champs dans l'ordre de leur nom rangeait
        // le maximum sous le minimum — ce qui donnait un intervalle à l'envers,
        // « 163615 ~ 84380 », dès que les deux voies se croisaient.
        //
        // ⚠ ET LES SIX CARACTÉRISTIQUES ARRIVENT BORNÉES À 255 :
        // `clif_initialstatus` écrit `min(sd.status.str, UINT8_MAX)`. Ce sont
        // les valeurs de BASE, celles qu'on achète avec des points, et aucune ne
        // dépasse ce plafond — le bonus de l'équipement, lui, passe par
        // `ZC_COUPLESTATUS` sur seize bits.
        const std::array<std::pair<sim::ServerParameter, std::int64_t>, 24> sheet = {{
            {sim::ServerParameter::kStatusPoint, packet.point},
            {sim::ServerParameter::kStr, packet.str},
            {sim::ServerParameter::kAgi, packet.agi},
            {sim::ServerParameter::kVit, packet.vit},
            {sim::ServerParameter::kInt, packet.int_},
            {sim::ServerParameter::kDex, packet.dex},
            {sim::ServerParameter::kLuk, packet.luk},
            {sim::ServerParameter::kUStr, packet.standardStr},
            {sim::ServerParameter::kUAgi, packet.standardAgi},
            {sim::ServerParameter::kUVit, packet.standardVit},
            {sim::ServerParameter::kUInt, packet.standardInt},
            {sim::ServerParameter::kUDex, packet.standardDex},
            {sim::ServerParameter::kULuk, packet.standardLuk},
            {sim::ServerParameter::kAtk1, packet.attPower},
            {sim::ServerParameter::kAtk2, packet.refiningPower},
            {sim::ServerParameter::kMatk1, packet.max_mattPower},
            {sim::ServerParameter::kMatk2, packet.min_mattPower},
            {sim::ServerParameter::kDef1, packet.itemdefPower},
            {sim::ServerParameter::kDef2, packet.plusdefPower},
            {sim::ServerParameter::kMdef1, packet.mdefPower},
            {sim::ServerParameter::kMdef2, packet.plusmdefPower},
            {sim::ServerParameter::kHit, packet.hitSuccessValue},
            {sim::ServerParameter::kFlee1, packet.avoidSuccessValue},
            {sim::ServerParameter::kCritical, packet.criticalSuccessValue},
        }};
        for (const auto& [parameter, value] : sheet) {
          static_cast<void>(world.SetPlayerParameter(static_cast<std::uint16_t>(parameter), value));
        }
        // Les deux qui restent, séparés parce qu'ils portent une NUANCE :
        // l'esquive parfaite arrive dans son propre champ, et l'ASPD du paquet
        // est le délai de motion — voir `PlayerStatus::amotion`.
        static_cast<void>(
            world.SetPlayerParameter(static_cast<std::uint16_t>(sim::ServerParameter::kFlee2),
                                     packet.plusAvoidSuccessValue));
        static_cast<void>(world.SetPlayerParameter(
            static_cast<std::uint16_t>(sim::ServerParameter::kAspd), packet.ASPD));
        return FrameOutcome::kApplied;
      });

    case proto::ZcStatusChangeAck::kOpcode:
      return Decoded<proto::ZcStatusChangeAck>(frame, [&world](const auto& packet) {
        if (packet.ok != kStatUpAccepted) {
          // REFUSÉ, ET RIEN À RANGER. Le serveur refuse pour trois raisons —
          // numéro hors des six, plafond du personnage atteint, points
          // insuffisants — et n'en nomme aucune. Rien ne change, donc rien ne
          // se montre : les points qu'on croyait dépenser sont toujours là, et
          // c'est cela qui se voit.
          return FrameOutcome::kApplied;
        }
        // ⚠ LA VALEUR EST BORNÉE À 255 PAR LE SERVEUR. Au-delà, il envoie tout
        // de suite après un `ZC_PAR_CHANGE` qui porte la vraie et écrase
        // celle-ci ; l'ordre des deux trames fait le travail, et il n'y a rien
        // à corriger ici.
        return OutcomeOfParameter(world.SetPlayerParameter(packet.sp, packet.value), packet.sp);
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestStatUp(Connection& link, std::uint16_t parameter, std::uint8_t amount) {
  proto::StatUpRequest request;
  request.parameter = parameter;
  request.amount = amount;
  auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace ro::net
