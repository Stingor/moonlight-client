#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/style.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES TEINTURES DE CALQUES : la tête, la peau, les coiffes et la cape, à côté de
// la recette v7 du corps, qui ne change pas. Voir l'ADR 0146.
//
// `CZ 0x0F5E` envoie en UNE trame tout ce qu'une validation touche ; `ZC 0x0F5F`
// apporte, par blocs, ce que chaque joueur AFFICHE ; `ZC 0x0F60` dit ce que le
// serveur a fait de chaque demande, et quand l'atelier du styliste s'ouvre.
//
// UNE ENTRÉE, commune aux deux sens :
//   [taille:1][calque:1][clé:4][profil:1][drapeaux:1][masque:2]
//   puis, pour chaque bit posé du masque, dans l'ordre des rangs :
//       [teinte:int16][saturation:int8][luminosité:int8][absolu:uint8]
// Le masque ne porte que les réglages non neutres, et la taille permet
// d'enjamber un champ qu'un serveur plus récent ajouterait en queue.
//
// LA QUEUE DU PROFIL MIXTE, annoncée par le drapeau 0x02, suit les réglages :
//   [masque_reste:2] puis 5 octets par classe de teinte réglée (rangs 0 à 12),
//   [nombre:1] puis nombre × [index:1][rouge:1][vert:1][bleu:1]
// Le reste ne vaut qu'aux profils 1 et 3 ; les retouches, triées par index,
// jamais sur une peau, ni à l'index zéro, ni en magenta pur. Un lecteur plus
// ancien enjambe la queue par la taille, et voit les pièces seules.
//
// LES DEUX PEAUX : celle du visage (calque 4, clé zéro) et celle d'un corps
// (calque 6, sous la clé du corps), un seul réglage chacune, au profil 1. Un
// lecteur plus ancien enjambe le calque 6 par la taille, sans le compter.
//
// 🔴 UN OBJET SE DÉSIGNE PAR SON EMPLACEMENT, jamais par un identifiant global.
// Le serveur trouve l'exemplaire dans l'équipement de la session émettrice, et
// refuse s'il n'y est pas porté, de l'identifiant attendu.

// L'en-tête d'une entrée, et la taille d'un réglage.
inline constexpr std::size_t kStyleLayerEntryHead = 10;
inline constexpr std::size_t kStyleLayerAdjustBytes = 5;
// La queue : le masque du reste et le nombre de retouches, puis 4 octets par
// retouche.
inline constexpr std::size_t kStyleLayerTailHead = 3;
inline constexpr std::size_t kStyleLayerRetouchBytes = 4;
// La plus grosse charge brute d'un ZC : un joueur n'est jamais coupé entre deux
// trames, et la décompression s'arrête là.
inline constexpr std::size_t kStyleLayersRawMost = 16384;

// Les profils de détection que ce client sait lire, et les rangs qu'ils emploient.
inline constexpr std::uint8_t kStyleLayerRampProfile = 1;
inline constexpr std::uint8_t kStyleLayerHueClassProfile = 2;
inline constexpr std::size_t kStyleLayerRampRanks = 8;
// La suite du corps : les rampes 9 à 12, aux rangs 0 à 3, sous la clé du corps.
inline constexpr std::uint8_t kStyleLayerBodyMoreProfile = 3;
inline constexpr std::size_t kStyleLayerBodyMoreRanks = 4;
// Un effet de chapeau : un seul réglage, au rang 0, ni reste ni retouche.
inline constexpr std::uint8_t kStyleLayerEffectProfile = 4;
// Le plus d'effets de chapeau qu'un CZ teint d'un coup : ceux qu'un joueur
// affiche tiennent largement en dessous.
inline constexpr std::size_t kStyleHatEffectsMost = 8;

// La plus grosse entrée : huit pièces, le reste en treize classes, et toutes
// les retouches. Elle tient sa taille sur un octet.
inline constexpr std::size_t kStyleLayerEntryMost =
    kStyleLayerEntryHead +
    (kStyleLayerRampRanks + sim::kStyleLayerAdjustCount) * kStyleLayerAdjustBytes +
    kStyleLayerTailHead + sim::kStyleRetouchesMost * kStyleLayerRetouchBytes;
// La plus grosse entrée d'une peau : un seul réglage, sans queue.
inline constexpr std::size_t kStyleSkinEntryMost = kStyleLayerEntryHead + kStyleLayerAdjustBytes;
// LE PLUS GROS CZ que le serveur accepte : la tête, la peau du visage, les
// quatre suites du corps et les huit emplacements, chacun à sa plus grosse
// entrée, et les peaux des quatre corps, tiennent
// dans 4 Kio.
inline constexpr std::size_t kStyleLayersRequestMost = 4096;

// L'EMPLACEMENT D'ÉQUIPEMENT de l'émetteur qu'une teinture d'objet vise.
enum class StyleSlot : std::uint8_t {
  kHeadTop = 10,
  kHeadMid = 11,
  kHeadLow = 12,
  kGarment = 13,
  kCostumeHeadTop = 14,
  kCostumeHeadMid = 15,
  kCostumeHeadLow = 16,
  kCostumeGarment = 17,
};

// Le calque que peint un emplacement : une coiffe ou la cape.
[[nodiscard]] sim::StyleLayerKind LayerOfSlot(StyleSlot slot) noexcept;

// UNE TEINTURE À VALIDER.
//
// La tête, les peaux et la suite du corps s'appliquent toujours au
// personnage : `slot` reste vide.
// Une coiffe ou une cape se désigne par `slot`, et la clé de `recipe` est alors
// l'identifiant de l'objet ATTENDU à cet emplacement.
struct StyleLayerOrder {
  sim::StyleLayerRecipe recipe;
  std::optional<StyleSlot> slot;
  // Retire cette teinture ; les réglages ne partent pas.
  bool clear = false;
};

// LA TRAME, ou la raison de la refuser avant qu'elle ne parte : un calque et un
// emplacement qui ne s'accordent pas, une clé nulle, un rang que le profil
// n'emploie pas, trop d'entrées.
//
// `clear_character` retire d'abord les teintures du PERSONNAGE (tête et peau).
// Celles des objets leur appartiennent : elles ne se retirent qu'une à une.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeStyleLayers(
    std::span<const StyleLayerOrder> orders, bool clear_character);
// ⚠ PARTIE NE VEUT PAS DIRE RETENUE : le verdict le dira.
[[nodiscard]] core::Result<void> SendStyleLayers(Connection& link,
                                                 std::span<const StyleLayerOrder> orders,
                                                 bool clear_character);

// LES BLOCS D'UNE CHARGE déjà décompressée.
struct StyleLayerBlock {
  std::uint32_t gid = 0;
  std::vector<sim::StyleLayerRecipe> layers;
};
// 🔴 TOUT OU RIEN : une entrée qui déborde de son bloc, ou un bloc qui déborde de
// la charge, rend une erreur, et rien n'est appliqué. Une entrée d'un calque ou
// d'un profil inconnus, elle, est seulement enjambée.
[[nodiscard]] core::Result<std::vector<StyleLayerBlock>> ReadStyleLayerPayload(
    std::span<const std::uint8_t> raw);

// LA CHARGE BRUTE d'une trame de styles : telle quelle, ou décompressée si le
// drapeau 0x01 est posé. 🔴 À LA TAILLE EXACTE ANNONCÉE, plafonnée à
// `kStyleLayersRawMost` : une bombe zlib s'arrête au tampon.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> StylePayloadOf(
    std::uint8_t flags, std::size_t raw_size, std::span<const std::uint8_t> body);

// `ZC 0x0F5F` : chaque bloc REDÉFINIT les teintures de son joueur, un bloc vide
// les efface. La décompression se fait à la taille exacte annoncée, plafonnée.
[[nodiscard]] FrameOutcome ApplyStyleLayersFrame(const Frame& frame, sim::World& world);
// `ZC 0x0F60` : le verdict d'une demande, ou l'état de l'atelier du styliste.
[[nodiscard]] FrameOutcome ApplyStyleVerdictFrame(const Frame& frame, sim::World& world,
                                                  std::uint32_t now_ms);

// UNE ENTRÉE À LA FORME DU ZC : le calque tel qu'il se peint (1 à 5), la clé
// affichée, aucun drapeau. C'est ainsi qu'une tenue range ses teintures.
[[nodiscard]] core::Result<void> WriteShownStyleLayer(core::ByteWriter& writer,
                                                      const sim::StyleLayerRecipe& recipe);
// Une entrée lue : la teinture, ou rien si ce client ne sait pas la lire (calque
// ou profil inconnus, enjambés) ; une erreur si elle déborde.
[[nodiscard]] core::Result<std::optional<sim::StyleLayerRecipe>> ReadShownStyleLayer(
    core::ByteReader& reader);

// Le bit d'annonce des calques si la table des longueurs connaît leurs ZC,
// zéro sinon : un opcode inconnu couperait la liaison.
[[nodiscard]] std::uint32_t StyleLayerCaps();

}  // namespace ro::net
