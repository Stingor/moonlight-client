#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/map_effect_maker.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/map_effect_table.h"
#include "ro/render/map_emitter.h"
#include "ro/render/map_smoke.h"
#include "ro/render/map_wind.h"

namespace ro::render {

// LE MOT DE RENDU QUE LE CLIENT DONNE AUX PARTICULES D'UN EFFET DE CARTE : un
// seau à mélange propre pour l'émetteur et l'anneau de vent, le seau ordinaire
// pour une bouffée de cheminée — la seule que le brouillard prenne. Voir
// `effect_render_word.h`.
inline constexpr std::uint32_t kMapParticleRenderWord =
    kEffectRenderWordOfConstructor | kEffectRenderWordOwnBlend;
inline constexpr std::uint32_t kMapSmokeRenderWord = kEffectRenderWordOfConstructor;

// UN LOT À PEINDRE : les quads d'un effet de carte, et ce qui décide de leur
// peinture.
struct MapEffectLot {
  // L'identifiant d'effet : c'est lui que `/effect` regarde.
  std::int32_t effect_id = 0;
  // Relatif au dossier des textures, en octets CP949, tel que la table l'écrit.
  // Il pointe dans le réglage de l'effet, qui vit aussi longtemps que lui.
  //
  // ⚠ QUAND LE LOT PORTE SA PROPRE IMAGE — la fumée des cheminées, un sprite —,
  // c'est la clé du renderer (`ChimneySmokeTextureKey`) et `image` la désigne.
  const std::string* texture = nullptr;
  const formats::Image* image = nullptr;
  std::vector<EffectWorldQuad> quads;
  // `zenable` : faux, le lot se peint par-dessus tout.
  bool depth_tested = true;
  // D'où l'effet est joué, dans notre repère : de quoi TRIER les lots.
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  std::uint32_t render_word = kMapParticleRenderWord;
  // La cheminée d'un ACTEUR — posée par un effet annoncé —, et non un objet de
  // la carte : ce qui masque le décor ne la masque pas.
  bool carried = false;
};

// UN SEMIS À JOUER : un semeur vient de lancer son effet, et c'est à `app` de
// l'engager — `render` ne connaît pas le moteur d'effets.
struct MapEffectSowing {
  std::int32_t effect_id = 0;
  // Dans NOTRE repère, prête pour `PlayById`.
  glm::vec3 world_position{0.0F, 0.0F, 0.0F};
  // L'ÉCHELLE D'INSTANCE que l'effet semé lira — le premier paramètre de
  // l'objet du RSW. Voir `MapEffectMaker::param`.
  float instance_scale = 0.0F;
  // LE SECOND PARAMÈTRE de l'objet du RSW, que l'effet semé lit aussi : l'eau
  // de 838 y prend son cap.
  float instance_second = 0.0F;
};

// LES EFFETS D'UNE CARTE : ses cheminées (44), ses émetteurs (974), ses anneaux
// de vent (2343) — qui se dessinent eux-mêmes — et ses SEMEURS, qui ne
// dessinent rien et lancent l'effet ordinaire de leur numéro.
//
// 🔴 CHAQUE OBJET 974 OU 2343 REÇOIT L'ENTRÉE DE SON RANG, et seulement elle :
// le k-ième objet 974 du RSW prend l'entrée de clé k de la table, le k-ième
// objet 2343 celle de clé k de la table du vent. Un objet sans entrée garde les
// défauts de sa classe — il souffle alors la fumée blanche du constructeur. Les
// cheminées, elles, ne lisent aucune table.
class MapEffects {
 public:
  // Pose les effets des objets du RSW, dans l'ordre du fichier.
  void Raise(const MapEffectTable& table, std::span<const formats::RswEffect> objects,
             std::int32_t gnd_width, std::int32_t gnd_height);

  // UNE CHEMINÉE POSÉE EN COURS DE PARTIE, à cette place de NOTRE repère : la
  // même classe que celle d'un objet 44 du RSW, que le gestionnaire d'effets
  // crée aussi pour un effet 44 annoncé sur un acteur. Elle vit comme les
  // autres — rien avant sa première renaissance, puis sans fin —, mais suit
  // son porteur et part avec lui : voir `FollowOwners`. Au plus tard, elle part
  // avec la carte, au prochain `Raise`.
  void AddChimney(glm::vec3 world_position, std::uint32_t owner_id);

  // LES CHEMINÉES QUI ONT UN PORTEUR le suivent, et meurent quand il n'est plus
  // là. `place_of` rend la place d'un porteur dans NOTRE repère, ou rien s'il a
  // quitté le monde. Celles du RSW n'en ont pas, et ne bougent jamais.
  template <typename PlaceOf>
  void FollowOwners(PlaceOf place_of) {
    for (auto chimney = chimneys_.begin(); chimney != chimneys_.end();) {
      if (chimney->owner_id() != 0) {
        const std::optional<glm::vec3> place = place_of(chimney->owner_id());
        if (!place.has_value()) {
          chimney = chimneys_.erase(chimney);
          continue;
        }
        chimney->MoveTo(frame_.ToNative(*place));
      }
      ++chimney;
    }
  }

  // Fait vivre les effets jusqu'à cet instant, au pas de seize millisecondes du
  // jeu, en rattrapant au plus une seconde. `player_world` est la place du
  // personnage dans NOTRE repère : c'est d'elle que se mesure la portée.
  void Advance(std::uint32_t now_ms, glm::vec3 player_world);
  // LA PORTÉE DES EFFETS DE CARTE, en unités du monde : celle du client,
  // `kMapEffectReachUnits`, ou `kUnboundedMapEffectReach` pour qu'ils vivent à
  // toute distance. Elle vaut dès le pas suivant ; la levée ne la touche pas.
  void SetReachUnits(float reach_units) noexcept { reach_units_ = reach_units; }

  // Les lots à peindre à cette image. Sans l'image des cheminées — une
  // installation qui n'a pas leur sprite —, elles ne se peignent pas.
  //
  // ⚠ LA VUE RENDUE VIT JUSQU'À L'APPEL SUIVANT, ou jusqu'à `Raise` : les lots
  // sont rangés dans un tampon que chaque image réemploie, pour que leurs quads
  // gardent leur mémoire d'une image à l'autre.
  [[nodiscard]] std::span<const MapEffectLot> Lots(const Camera& camera,
                                                   const ChimneySmokeLook* smoke = nullptr);

  // Ce que les semeurs ont lancé pendant le dernier `Advance`, dans l'ordre des
  // pas. Vidé à chaque appel : `app` le draine dans la foulée.
  [[nodiscard]] std::span<const MapEffectSowing> sown() const noexcept { return sown_; }

  [[nodiscard]] bool empty() const noexcept {
    return emitters_.empty() && winds_.empty() && chimneys_.empty() && makers_.empty();
  }
  [[nodiscard]] std::span<const MapEmitter> emitters() const noexcept { return emitters_; }
  [[nodiscard]] std::span<const MapWind> winds() const noexcept { return winds_; }
  [[nodiscard]] std::span<const MapChimney> chimneys() const noexcept { return chimneys_; }
  [[nodiscard]] std::span<const MapEffectMaker> makers() const noexcept { return makers_; }

 private:
  MapEffectFrame frame_;
  std::vector<MapEmitter> emitters_;
  std::vector<MapWind> winds_;
  std::vector<MapChimney> chimneys_;
  std::vector<MapEffectMaker> makers_;
  std::vector<MapEffectSowing> sown_;
  // Les lots de la dernière image, et au-delà ceux d'images plus chargées, que
  // `Lots` reprend avant d'en créer de nouveaux.
  std::vector<MapEffectLot> lots_;
  // Le générateur partagé : le client tire tous ses effets dans le même `rand()`.
  std::uint32_t draws_ = 1;
  std::uint32_t last_ms_ = 0;
  std::uint32_t owed_ms_ = 0;
  float reach_units_ = kMapEffectReachUnits;
  bool started_ = false;
};

// Les textures qu'emploieront les effets que ces objets poseront, pour qu'on
// les charge une fois — défauts de classe compris. Des noms RELATIFS au dossier
// des textures, tels que la table les écrit.
[[nodiscard]] std::set<std::string> MapEffectTextures(const MapEffectTable& table,
                                                      std::span<const formats::RswEffect> objects);

// Le chemin complet d'une texture d'effet de carte dans l'archive : c'est la
// clé sous laquelle la scène la range et le renderer la garde.
[[nodiscard]] std::string MapEffectTexturePath(std::string_view name);

}  // namespace ro::render
