#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// LES DONNÉES DE LA NAVIGATION : le graphe du monde, tel que le serveur le décrit.
//
// Six tables, GÉNÉRÉES par le serveur depuis ce qu'il a réellement chargé — les
// cartes, les PNJ qu'on peut chercher, les liaisons entre cartes, les apparitions
// de monstres, puis deux tables de DISTANCES précalculées à la marche :
//
//   · d'une liaison entrante à chaque liaison sortante de la carte d'arrivée ;
//   · d'une liaison entrante à chaque PNJ de la carte d'arrivée.
//
// Ces fichiers portent l'extension .lub mais sont du Lua EN CLAIR, et d'une
// grammaire bien plus étroite que celle que lit `ReadLuaSource` : des listes
// d'accolades à plat, des chaînes et des entiers. Ils pèsent une dizaine de
// mégaoctets, et le lecteur générique bâtirait une table partagée par ligne — des
// centaines de milliers d'allocations pour des enregistrements de huit champs. Ce
// lecteur-ci va droit aux structures, sans arbre intermédiaire.
//
// ⚠ UNE ENTRÉE INCOMPRÉHENSIBLE REFUSE LE FICHIER ; UNE ENTRÉE ORPHELINE NE LE
// REFUSE PAS. Une accolade manquante ou un champ du mauvais type disent que le
// fichier n'est pas celui qu'on croit : le lire à moitié donnerait un graphe
// troué sans que personne ne le sache. Mais un PNJ posé sur une carte que la
// table des cartes ne nomme pas n'est qu'une donnée périmée — la génération a
// été faite contre un autre jeu de cartes. Celui-là est COMPTÉ et sauté.

// Où le client range ces tables, et le suffixe de la variante de serveur.
inline constexpr std::string_view kNaviTableDirectory =
    "data\\luafiles514\\lua files\\navigation\\";
inline constexpr std::string_view kNaviMapTableFile = "navi_map_krpri.lub";
inline constexpr std::string_view kNaviNpcTableFile = "navi_npc_krpri.lub";
inline constexpr std::string_view kNaviMobTableFile = "navi_mob_krpri.lub";
inline constexpr std::string_view kNaviLinkTableFile = "navi_link_krpri.lub";
inline constexpr std::string_view kNaviLinkDistanceTableFile = "navi_linkdistance_krpri.lub";
inline constexpr std::string_view kNaviNpcDistanceTableFile = "navi_npcdistance_krpri.lub";

// Le rang d'une carte dans `NaviData::maps`. Les autres tables désignent une
// carte par son NOM ; on le résout une fois, au chargement, plutôt qu'à chaque
// question du pathfinder.
using NaviMapIndex = std::uint16_t;
inline constexpr NaviMapIndex kNoNaviMap = 0xFFFF;

// La nature d'une liaison — le troisième champ d'une ligne de la table des liaisons.
//
// 🔴 ELLE DÉCIDE SI LA LIAISON EST EMPRUNTABLE. Un portail ou un PNJ scripté se
// franchissent toujours ; un service de transport — sur Moonlight, le Warp
// Agent — et l'aéroport ne le sont que si l'itinéraire les autorise. C'est ce
// qui garde le graphe PÉDESTRE intact tout en y rangeant les raccourcis payants.
namespace navi_link_kind {
inline constexpr std::uint16_t kWarpPortal = 200;
inline constexpr std::uint16_t kScriptedNpc = 201;
inline constexpr std::uint16_t kKafraDungeonWarp = 202;
inline constexpr std::uint16_t kEventDungeonWarp = 203;
inline constexpr std::uint16_t kTransportService = 204;
inline constexpr std::uint16_t kAirport = 205;
}  // namespace navi_link_kind

// La classe de sprite qu'un PNJ porte quand il n'est qu'un PORTAIL. Elle dépasse
// seize bits : le champ d'un PNJ ne se masque jamais.
inline constexpr std::int32_t kNaviWarpPortalSprite = 99999;

// Le troisième champ des tables de PNJ et d'apparitions.
namespace navi_subtype {
inline constexpr std::uint16_t kNpc = 101;
inline constexpr std::uint16_t kShop = 102;
inline constexpr std::uint16_t kSpawn = 300;
inline constexpr std::uint16_t kBossSpawn = 301;
}  // namespace navi_subtype

struct NaviMap {
  // Nom INTERNE (« pay_fild10 ») : le seul qui désigne une carte ailleurs.
  std::string name;
  // Nom AFFICHÉ (« Payon Forest »). Retombe sur le nom interne quand le serveur
  // n'en connaît pas.
  std::string display_name;
  // 5001 simple, 5002 aéroport, 5003 carte en morceaux, 5005 en morceaux avec
  // monstres. Gardé tel quel : rien ne s'en sert encore pour choisir.
  std::uint16_t map_kind = 0;
  std::uint16_t width_cells = 0;
  std::uint16_t height_cells = 0;
};

struct NaviNpc {
  NaviMapIndex map = kNoNaviMap;
  std::uint32_t npc_id = 0;
  std::uint16_t subtype = navi_subtype::kNpc;
  // Classe de sprite, TELLE QUELLE : `kNaviWarpPortalSprite` pour un portail.
  std::int32_t sprite_class = 0;
  // Le nom VISIBLE — le serveur a déjà retiré ce qui suit le dièse.
  std::string name;
  std::int16_t cell_x = 0;
  std::int16_t cell_y = 0;

  [[nodiscard]] bool is_shop() const noexcept { return subtype == navi_subtype::kShop; }
};

struct NaviLink {
  NaviMapIndex map = kNoNaviMap;
  std::uint32_t link_id = 0;
  std::uint16_t kind = navi_link_kind::kWarpPortal;
  std::int32_t sprite_class = kNaviWarpPortalSprite;
  // Le libellé du passage — le nom du PNJ qu'on doit aller voir, ou un nom
  // fabriqué `<source>_<destination>_<id>` quand le serveur n'en avait aucun.
  std::string name;
  // Où l'on entre dans la liaison, sur la carte de départ.
  std::int16_t cell_x = 0;
  std::int16_t cell_y = 0;
  // Où l'on en sort.
  NaviMapIndex dest_map = kNoNaviMap;
  std::int16_t dest_cell_x = 0;
  std::int16_t dest_cell_y = 0;
};

// Une apparition de monstres. ⚠ ELLE N'A PAS DE POSITION : la table ne dit que
// sur quelle carte, combien, et ce que vaut la bête.
struct NaviSpawn {
  NaviMapIndex map = kNoNaviMap;
  std::uint32_t spawn_id = 0;
  std::uint16_t subtype = navi_subtype::kSpawn;
  std::uint16_t amount = 0;
  // La classe de SPRITE, pas un identifiant de la base de monstres : c'est ce
  // que le serveur écrit, et le client n'a pas de quoi faire la correspondance.
  std::uint16_t sprite_class = 0;
  std::string name;
  std::string sprite_name;
  std::int32_t level = 0;
  // ((niveau d'élément × 20 + élément) << 16) | (taille << 8) | race.
  std::uint32_t packed_stats = 0;

  [[nodiscard]] bool is_boss() const noexcept { return subtype == navi_subtype::kBossSpawn; }
  [[nodiscard]] std::uint32_t element() const noexcept;
  [[nodiscard]] std::uint32_t element_level() const noexcept;
  [[nodiscard]] std::uint32_t size() const noexcept;
  [[nodiscard]] std::uint32_t race() const noexcept;
};

// Une SUITE de liaisons consécutives à la même distance.
//
// Le serveur regroupe les identifiants consécutifs qui partagent une distance :
// un PNJ qui offre deux cents destinations depuis un seul point en produit deux
// cents à la même distance, relistées pour CHAQUE liaison entrante. Gardées
// groupées, elles coûtent une ligne au lieu de deux cents — en mémoire comme au
// parcours.
struct NaviPassRun {
  std::uint32_t first_link_id = 0;
  std::uint32_t link_count = 1;
  std::int32_t distance_cells = 0;
};

// La même chose vers les PNJ : la liaison entrante est nommée par sa carte de
// DÉPART, que la table porte en plus.
struct NaviNpcPassRun {
  NaviMapIndex from_map = kNoNaviMap;
  std::uint32_t first_link_id = 0;
  std::uint32_t link_count = 1;
  std::int32_t distance_cells = 0;
};

// Où trouver, dans un tableau plat, les suites d'un même propriétaire.
struct NaviPassSpan {
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

// Ce qu'une lecture a dû sauter, par table. Rien de tout cela n'est une erreur ;
// c'est ce qu'un journal doit dire UNE fois, pour qu'une génération périmée se
// voie.
struct NaviSkipCounts {
  std::size_t npcs_on_unknown_map = 0;
  std::size_t links_on_unknown_map = 0;
  std::size_t spawns_on_unknown_map = 0;
  std::size_t distances_from_unknown_link = 0;
};

class NaviData {
 public:
  std::vector<NaviMap> maps;
  std::vector<NaviNpc> npcs;
  std::vector<NaviLink> links;
  std::vector<NaviSpawn> spawns;
  NaviSkipCounts skipped;

  // Rend `kNoNaviMap` pour une carte inconnue. La casse ne compte pas.
  [[nodiscard]] NaviMapIndex FindMap(std::string_view internal_name) const;
  // Rend nullptr pour une liaison inconnue.
  [[nodiscard]] const NaviLink* FindLink(std::uint32_t link_id) const;
  // Son rang dans `links`.
  [[nodiscard]] std::optional<std::uint32_t> LinkSlot(std::uint32_t link_id) const;
  [[nodiscard]] const NaviNpc* FindNpc(std::uint32_t npc_id) const;

  // Les liaisons qui QUITTENT une carte, et les PNJ et apparitions qu'elle porte.
  [[nodiscard]] std::span<const std::uint32_t> LinksLeaving(NaviMapIndex map) const;
  [[nodiscard]] std::span<const std::uint32_t> NpcsOn(NaviMapIndex map) const;
  [[nodiscard]] std::span<const std::uint32_t> SpawnsOn(NaviMapIndex map) const;

  // Depuis la sortie de `link_id`, la marche jusqu'à chaque liaison sortante de
  // la carte d'arrivée. Vide si la table n'en dit rien.
  [[nodiscard]] std::span<const NaviPassRun> PassesAfter(std::uint32_t link_id) const;
  // Pour un PNJ, la marche depuis chaque liaison qui entre sur sa carte.
  [[nodiscard]] std::span<const NaviNpcPassRun> PassesToNpc(std::uint32_t npc_id) const;

  // Reconstruit les index. Chaque lecteur d'une table d'objets l'appelle en
  // finissant : la table des cartes doit être indexée pour que les suivantes y
  // résolvent leurs noms.
  void BuildIndexes();

  // Les tables brutes de distances, remplies par les lecteurs.
  std::vector<NaviPassRun> link_passes;
  std::unordered_map<std::uint32_t, NaviPassSpan> link_pass_spans;
  std::vector<NaviNpcPassRun> npc_passes;
  std::unordered_map<std::uint32_t, NaviPassSpan> npc_pass_spans;

 private:
  std::unordered_map<std::string, NaviMapIndex> map_by_lower_name_;
  static constexpr std::uint32_t kNoSlot = 0xFFFFFFFF;
  // Au-delà de cet écart entre identifiants, le tableau direct coûterait plus
  // qu'il ne rapporte.
  static constexpr std::uint64_t kMaxDenseLinkSpan = 1U << 20;
  std::uint32_t first_link_id_ = 0;
  std::vector<std::uint32_t> link_slot_dense_;
  std::unordered_map<std::uint32_t, std::uint32_t> link_slot_by_id_;
  std::unordered_map<std::uint32_t, std::uint32_t> npc_slot_by_id_;
  // Par carte, les RANGS dans `links` / `npcs` / `spawns`.
  std::vector<std::vector<std::uint32_t>> links_leaving_;
  std::vector<std::vector<std::uint32_t>> npcs_on_;
  std::vector<std::vector<std::uint32_t>> spawns_on_;
};

// Les six lecteurs. La table des cartes d'abord : les autres résolvent leurs noms
// de carte contre elle, d'où leur `NaviData&`.
//
// Chacun échoue si le fichier ne suit pas sa grammaire, et laisse alors `data`
// tel qu'il l'a trouvé pour SA table.
[[nodiscard]] core::Result<void> ReadNaviMaps(std::span<const std::uint8_t> bytes, NaviData& data);
[[nodiscard]] core::Result<void> ReadNaviNpcs(std::span<const std::uint8_t> bytes, NaviData& data);
[[nodiscard]] core::Result<void> ReadNaviSpawns(std::span<const std::uint8_t> bytes,
                                                NaviData& data);
[[nodiscard]] core::Result<void> ReadNaviLinks(std::span<const std::uint8_t> bytes, NaviData& data);
[[nodiscard]] core::Result<void> ReadNaviLinkDistances(std::span<const std::uint8_t> bytes,
                                                       NaviData& data);
[[nodiscard]] core::Result<void> ReadNaviNpcDistances(std::span<const std::uint8_t> bytes,
                                                      NaviData& data);

}  // namespace ro::formats
