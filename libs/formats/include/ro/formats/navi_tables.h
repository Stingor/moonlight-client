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
// Une table, `navigation.yml`, que `scripts/yaml_tables/navigation.py` tire du
// générateur de navigation du serveur : les cartes, les PNJ qu'on peut chercher,
// les apparitions de monstres, les liaisons entre cartes, et, depuis la sortie de
// chaque liaison, la marche jusqu'à chaque liaison de la carte d'arrivée. La
// marche jusqu'à une case ou un PNJ ne s'y trouve pas : le guidage la mesure sur
// la grille de la carte visée.
//
// C'est du YAML en style FLUX, une entrée par ligne, que le script écrit dans un
// ordre fixe : les cartes d'abord, puisque les autres sections les nomment. Il
// pèse quelques mégaoctets ; un lecteur YAML générique bâtirait un nœud par
// champ. Ce lecteur-ci n'accepte que ce que le script écrit et va droit aux
// structures.
//
// ⚠ UNE ENTRÉE INCOMPRÉHENSIBLE REFUSE LE FICHIER : une accolade manquante, un
// champ inconnu, absent ou hors bornes, une carte ou une liaison que le fichier
// ne définit pas. Le script a vérifié la table entre elle avant de l'écrire ;
// un écart dit que le fichier n'est pas celui qu'on croit, et le lire à moitié
// donnerait un graphe troué sans que personne ne le sache.

// Le rang d'une carte dans `NaviData::maps`. Les autres sections désignent une
// carte par son NOM ; on le résout une fois, au chargement, plutôt qu'à chaque
// question du pathfinder.
using NaviMapIndex = std::uint16_t;
inline constexpr NaviMapIndex kNoNaviMap = 0xFFFF;

// La nature d'une liaison (`kind` d'une entrée de `links`).
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

// La nature d'une carte (`kind` d'une entrée de `maps`).
namespace navi_map_kind {
inline constexpr std::uint16_t kPlain = 5001;
inline constexpr std::uint16_t kAirport = 5002;
// Des liaisons de la carte ne se joignent pas à pied.
inline constexpr std::uint16_t kSegmented = 5003;
inline constexpr std::uint16_t kSegmentedWithMonsters = 5005;
}  // namespace navi_map_kind

// La classe de sprite qu'un PNJ porte quand il n'est qu'un PORTAIL. Elle dépasse
// seize bits : le champ d'un PNJ ne se masque jamais.
inline constexpr std::int32_t kNaviWarpPortalSprite = 99999;

// La sorte d'un PNJ ou d'une apparition.
namespace navi_subtype {
inline constexpr std::uint16_t kNpc = 101;
inline constexpr std::uint16_t kShop = 102;
inline constexpr std::uint16_t kSpawn = 300;
inline constexpr std::uint16_t kBossSpawn = 301;
}  // namespace navi_subtype

struct NaviMap {
  // Nom INTERNE (« pay_fild10 ») : le seul qui désigne une carte ailleurs.
  std::string name;
  // Nom AFFICHÉ (« Payon Forest »). Le nom interne quand le serveur n'en
  // connaît pas.
  std::string display_name;
  // Une valeur de `navi_map_kind`. Gardée telle quelle : rien ne s'en sert
  // encore pour choisir.
  std::uint16_t map_kind = navi_map_kind::kPlain;
  std::uint16_t width_cells = 0;
  std::uint16_t height_cells = 0;
};

struct NaviNpc {
  NaviMapIndex map = kNoNaviMap;
  std::uint32_t npc_id = 0;
  std::uint16_t subtype = navi_subtype::kNpc;
  // Classe de sprite, TELLE QUELLE : `kNaviWarpPortalSprite` pour un portail.
  std::int32_t sprite_class = 0;
  // Le nom VISIBLE — sans ce qui suit le dièse.
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
// Le générateur regroupe les identifiants consécutifs qui partagent une
// distance : un PNJ qui offre deux cents destinations depuis un seul point en
// produit deux cents à la même distance, relistées pour CHAQUE liaison
// entrante. Gardées groupées, elles coûtent une ligne au lieu de deux cents — en
// mémoire comme au parcours.
struct NaviPassRun {
  std::uint32_t first_link_id = 0;
  std::uint32_t link_count = 1;
  // La marche, en PAS.
  std::int32_t distance_cells = 0;
};

// Où trouver, dans un tableau plat, les suites d'un même propriétaire.
struct NaviPassSpan {
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

class NaviData {
 public:
  std::vector<NaviMap> maps;
  std::vector<NaviNpc> npcs;
  std::vector<NaviLink> links;
  std::vector<NaviSpawn> spawns;

  // Faux quand le script a dû REDRESSER les longueurs de marche d'une
  // génération qui n'en gardait que l'octet du bas : la génération est à refaire.
  bool exact_steps = true;

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

  // Reconstruit les index, après toute retouche des tableaux.
  void BuildIndexes();

  // Les distances entre liaisons, à plat, et la part de chaque liaison.
  std::vector<NaviPassRun> link_passes;
  std::unordered_map<std::uint32_t, NaviPassSpan> link_pass_spans;

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

// Le texte de `navigation.yml`. Refuse le fichier entier au premier écart, en
// nommant la ligne.
[[nodiscard]] core::Result<NaviData> ReadNaviTable(std::span<const std::uint8_t> bytes);

}  // namespace ro::formats
