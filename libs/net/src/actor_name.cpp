// Le nom d'un acteur et sa carte. Voir l'en-tête pour les deux paquets, et pour
// le troisième qui répond sous un identifiant que le monde n'indexe pas.

#include "ro/net/actor_name.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/text.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// Le texte vient du JEU, donc de sa page de codes : voir ro/core/text.h.
[[nodiscard]] std::string Said(const std::string& wire) { return core::Utf8FromCp1252(wire); }

// L'ÉTIQUETTE D'ABSENCE que le serveur colle devant le pseudo d'un joueur
// absent — dans ce paquet seulement, jamais dans celui d'apparition.
constexpr std::string_view kAbsenceTag = "[AFK] ";
// Ce que le champ de nom porte, zéro final non compris (`NAME_LENGTH` moins
// un). L'étiquette s'y paie sur la FIN du pseudo, et un caractère de la page de
// codes du jeu y occupe un octet : on compte donc en caractères.
constexpr std::size_t kNameFieldChars = 23;

// Vrai quand `shown` est `known` derrière l'étiquette d'absence, tronqué comme
// le serveur le compose.
//
// ⚠ LA COMPARAISON EST EXACTE, et pas un simple préfixe : un joueur qui
// s'appelle vraiment « [AFK] Bob » garde son nom quand il est présent.
[[nodiscard]] bool IsAbsenceLabel(std::string_view shown, std::string_view known) {
  if (known.empty() || !shown.starts_with(kAbsenceTag)) {
    return false;
  }
  const std::size_t room_chars = kNameFieldChars - kAbsenceTag.size();
  std::size_t kept_bytes = 0;
  for (std::size_t kept_chars = 0; kept_bytes < known.size() && kept_chars < room_chars;
       ++kept_chars) {
    static_cast<void>(core::NextCodePoint(known, kept_bytes));
  }
  return shown.substr(kAbsenceTag.size()) == known.substr(0, kept_bytes);
}

// Pose la carte, ou dit que personne ne portait cet identifiant.
//
// ⚠ UN ACTEUR INCONNU N'EN CRÉE PAS UN, et c'est la règle de toute cette
// couche : la carte arrive parfois juste après qu'un monstre est sorti de vue,
// et inventer un acteur sans apparence ni case ne donnerait rien à dessiner.
[[nodiscard]] FrameOutcome Posted(sim::World& world, std::uint32_t id, std::string name,
                                  sim::NameCard card) {
  return world.TellActorName(id, std::move(name), std::move(card)) ? FrameOutcome::kApplied
                                                                   : FrameOutcome::kIgnored;
}

}  // namespace

FrameOutcome ApplyActorNameFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode == proto::ZcAckReqnameall::kOpcode) {
    proto::ZcAckReqnameall packet;
    if (!proto::Decode(frame.bytes, packet)) {
      return FrameOutcome::kUndecodable;
    }
    sim::NameCard card;
    card.aside = Said(packet.party_name);
    card.banner = Said(packet.guild_name);
    card.badge = Said(packet.position_name);
    // 🔴 LE TITRE N'EST QU'UN NUMÉRO ICI, et c'est une table de l'installation
    // qui le traduit — `title_tbl`, dans `titletable.lub`. La couche réseau ne
    // lit aucun fichier du jeu : elle porte le numéro, et `render` le résout.
    card.title_id = packet.title_id;
    const auto id = static_cast<std::uint32_t>(packet.gid);
    std::string name = Said(packet.name);
    // 🔴 L'ABSENCE NE RENOMME PAS : elle se peint, et le nom du personnage
    // reste celui de l'apparition — c'est lui que le serveur cherche quand on
    // l'invite par son nom. Un nom vide garde celui qu'on a.
    if (const sim::Actor* actor = world.Find(id);
        actor != nullptr && IsAbsenceLabel(name, actor->name)) {
      card.shown_name = std::move(name);
      name.clear();
    }
    return Posted(world, id, std::move(name), std::move(card));
  }

  if (frame.opcode == proto::ZcAckReqnameallNpc::kOpcode) {
    proto::ZcAckReqnameallNpc packet;
    if (!proto::Decode(frame.bytes, packet)) {
      return FrameOutcome::kUndecodable;
    }
    sim::NameCard card;
    // ⚠ LES TROIS AUTRES CHAÎNES RESTENT VIDES : le client natif passe par le
    // MÊME poseur que pour 0x0A30, avec quatre chaînes vides et le titre. Un PNJ n'a ni
    // groupe ni guilde, et le `groupId` que le paquet porte ne ressort nulle
    // part — le client le charge et ne le lit pas.
    card.title = Said(packet.title);
    return Posted(world, static_cast<std::uint32_t>(packet.gid), Said(packet.name),
                  std::move(card));
  }

  return FrameOutcome::kUnhandled;
}

core::Result<void> RequestActorName(Connection& link, std::uint32_t actor_id) {
  proto::NameRequest request;
  request.actor_id = actor_id;
  auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace ro::net
