#include "ro/net/player_styles.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <span>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/style.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// LA VERSION DU FIL vit dans `sim` : elle vaut aussi pour les CODES ÉCRITS —
// presse-papiers, préréglages, brouillon —, et deux déclarations laisseraient
// un jour le fil et le disque ne plus parler de la même chose.
constexpr std::uint8_t kWireVersion = sim::kStyleWireVersion;

// L'entrée ne porte PAS de recette : elle efface ce corps.
constexpr std::uint8_t kFlagClear = 0x01;
// Cette variante est celle à reprendre pour un corps qui n'a pas la sienne.
constexpr std::uint8_t kFlagFallback = 0x04;
// Tout effacer : le client ÉMET ce bit, et le serveur y répond par un lot.
// ⚠ IL NE SE LIT PAS À LA RÉCEPTION — une entrée de lot n'efface jamais que le
// corps qu'elle nomme.
constexpr std::uint8_t kFlagClearAll = 0x02;

// L'en-tête du lot : opcode, longueur, nombre d'entrées.
//
// ⚠ IL SE RECOUPE CONTRE LA TABLE ENGENDRÉE, et ce n'est pas une précaution de
// style : la queue se lit à la main, donc rien d'autre ne relierait ce
// décalage à ce que le serveur déclare. Le jour où l'en-tête change, c'est la
// compilation qui le dit.
constexpr std::size_t kBatchHeaderSize = 6;
static_assert(kBatchHeaderSize == proto::ZcMoonlightStyles::kWireSize,
              "l'en-tête du lot ne vaut plus ce que le serveur déclare");

// Une entrée : gid, version, drapeaux, corps, trois numéros, huit réglages.
constexpr std::size_t kRampBytes = 5;
constexpr std::size_t kEntrySize = 4 + 1 + 1 + 4 + 2 + 2 + 2 + sim::kStyleRampCount * kRampBytes;
// 🔴 UNE ENTRÉE DU LOT A EXACTEMENT LA TAILLE DE LA DEMANDE QUE NOUS ÉMETTONS,
// et la table engendrée porte la seconde : c'est le seul recoupement mécanique
// possible sur une disposition que le générateur ne modélise pas.
static_assert(kEntrySize == proto::CzMoonlightStyle::kWireSize,
              "l'entrée du lot a changé : bumper la version du fil et mettre à jour le serveur");

[[nodiscard]] sim::StyleRecipe ReadRecipe(core::ByteReader& reader) {
  sim::StyleRecipe recipe;
  recipe.body_palette = reader.I16();
  recipe.hair_palette = reader.I16();
  recipe.hair_style = reader.I16();
  for (sim::StyleAdjust& adjust : recipe.ramps) {
    adjust.hue = reader.I16();
    adjust.saturation = reader.I8();
    adjust.value = reader.I8();
    // ⚠ TOUT NON-ZÉRO VAUT « ABSOLU » : un serveur plus récent pourrait y
    // mettre autre chose que un, et une comparaison stricte ferait
    // silencieusement retomber la rampe en mode relatif.
    adjust.absolute = reader.U8() != 0;
  }
  return recipe;
}

// Écrit un réglage dans la demande : cinq octets, petit-boutiste.
//
// 🔴 CHAMP PAR CHAMP, ET JAMAIS LA STRUCTURE TELLE QUELLE. Un `StyleAdjust`
// porte un octet de bourrage — deux plus un plus un plus un font cinq utiles
// et six alloués — dont la position dépend du compilateur. Le recopier
// enverrait ce bourrage sur le fil, décalé d'une rampe à l'autre.
void WriteAdjust(std::span<std::uint8_t> into, const sim::StyleAdjust& adjust) {
  const auto hue = static_cast<std::uint16_t>(adjust.hue);
  into[0] = static_cast<std::uint8_t>(hue & 0xFFU);
  into[1] = static_cast<std::uint8_t>(hue >> 8U);
  into[2] = static_cast<std::uint8_t>(adjust.saturation);
  into[3] = static_cast<std::uint8_t>(adjust.value);
  into[4] = adjust.absolute ? 1 : 0;
}

// La demande, montée une fois pour les trois portes.
[[nodiscard]] proto::CzMoonlightStyle Request(std::uint8_t flags, std::uint32_t body_key) {
  proto::CzMoonlightStyle request;
  // ⚠ LA LONGUEUR S'ÉCRIT : le serveur la LIT, et la laisser à zéro ferait
  // rejeter la trame sans un mot.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightStyle::kWireSize);
  request.version = kWireVersion;
  request.flags = flags;
  request.body_key = body_key;
  // ⚠ MOINS UN VAUT « CELUI DU PERSONNAGE » : zéro serait la première palette,
  // et un effacement repeindrait tout le monde en couleur numéro zéro.
  request.palette_id = -1;
  request.hair_palette_id = -1;
  request.hair_style = -1;
  return request;
}

}  // namespace

core::Result<void> SendPlayerStyle(Connection& link, const sim::StyleRecipe& recipe,
                                   std::uint32_t body_key, bool fallback) {
  proto::CzMoonlightStyle request = Request(fallback ? kFlagFallback : 0, body_key);
  request.palette_id = recipe.body_palette;
  request.hair_palette_id = recipe.hair_palette;
  request.hair_style = recipe.hair_style;
  for (std::size_t rank = 0; rank < sim::kStyleRampCount; ++rank) {
    WriteAdjust(std::span(request.adjusts).subspan(rank * kRampBytes, kRampBytes),
                recipe.ramps[rank]);
  }
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> SendPlayerStyleClear(Connection& link, std::uint32_t body_key) {
  link.Send(proto::Encode(Request(kFlagClear, body_key)));
  return {};
}

core::Result<void> SendPlayerStyleClearAll(Connection& link) {
  // ⚠ SANS CORPS : la demande ne vise personne en particulier, et y mettre une
  // clé laisserait croire qu'elle n'efface que celui-là.
  link.Send(proto::Encode(Request(kFlagClearAll, 0)));
  return {};
}

FrameOutcome ApplyStyleOpenFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightStyleOpen::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonlightStyleOpen packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  // ⚠ TOUT AUTRE MODE BASCULE, comme chez la référence : un serveur plus
  // récent pourrait en ajouter un, et refuser vaudrait moins que le geste le
  // plus proche.
  switch (packet.mode) {
    case 0:
      world.styles().Ask(sim::StyleEditorRequest::kClose);
      break;
    case 1:
      world.styles().Ask(sim::StyleEditorRequest::kOpen);
      break;
    default:
      world.styles().Ask(sim::StyleEditorRequest::kToggle);
      break;
  }
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyPlayerStylesFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightStyles::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  core::ByteReader reader(frame.bytes);
  if (!reader.Seek(kBatchHeaderSize - sizeof(std::uint16_t))) {
    return FrameOutcome::kUndecodable;
  }
  const std::size_t announced = reader.U16();
  // 🔴 LE COMPTE EST CRU, MAIS PAS SUR PAROLE : une trame tronquée en
  // annoncerait plus qu'elle n'en porte.
  const std::size_t count = std::min(announced, reader.remaining() / kEntrySize);

  // 🔴 UN LOT REDÉFINIT INTÉGRALEMENT LES VARIANTES DES JOUEURS QU'IL MENTIONNE,
  // et le vidage a lieu à la PREMIÈRE entrée rencontrée pour un identifiant —
  // pas avant. Vider d'entrée de jeu ferait perdre des variantes valides
  // lorsqu'un lot est tronqué, ou que toutes ses entrées sont d'une version
  // que nous refusons : on effacerait sans rien mettre à la place.
  std::set<std::uint32_t> started;
  for (std::size_t rank = 0; rank < count; ++rank) {
    const std::uint32_t gid = reader.U32();
    const std::uint8_t version = reader.U8();
    const std::uint8_t flags = reader.U8();
    const std::uint32_t body_key = reader.U32();
    if (gid == 0 || version != kWireVersion) {
      // ⚠ ON SAUTE LE RESTE DE L'ENTRÉE, et non la trame : les suivantes
      // peuvent être bonnes, et un lot mêle les joueurs.
      reader.Skip(kEntrySize - (4 + 1 + 1 + 4));
      continue;
    }
    const bool clears = (flags & kFlagClear) != 0;
    sim::StyleRecipe recipe;
    if (clears) {
      reader.Skip(kEntrySize - (4 + 1 + 1 + 4));
    } else {
      recipe = ReadRecipe(reader);
    }
    if (started.insert(gid).second) {
      world.styles().BeginBatchFor(gid);
    }
    if (!clears) {
      world.styles().Remember(gid, body_key, recipe, (flags & kFlagFallback) != 0);
    }
  }
  if (!reader.ok()) {
    return FrameOutcome::kUndecodable;
  }

  // ⚠ UN JOUEUR QUI N'A PLUS AUCUNE VARIANTE NE SE GARDE PAS : un lot d'effacement
  // ne laisse rien derrière lui, et une entrée vide se lirait « on sait qu'il
  // n'a pas de couleurs » alors qu'elle dit « il n'en a plus ».
  for (const std::uint32_t gid : started) {
    const sim::StyleVariants* variants = world.styles().Find(gid);
    if (variants != nullptr && variants->empty()) {
      world.styles().Forget(gid);
    }
  }
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
