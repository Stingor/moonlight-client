#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/audio/audio_engine.h"
#include "ro/core/error.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/camera.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "options/graphics_options.h"
#include "shell/backdrop_effects.h"
#include "world/actor_ghosts.h"
#include "world/actor_sounds.h"
#include "world/actor_view.h"
#include "world/frozen_poses.h"
#include "world/item_drops.h"
#include "world/lit_stage.h"
#include "world/map_stage.h"

namespace moonclient {

// LA VILLE DU REPLI LOCAL, quand le décor vivant n'est pas possible.
//
// 🔴 PRONTERA, ET CE N'EST PAS UN GOÛT : c'est la ville de départ, celle
// qu'aucune installation ne peut ne pas avoir, et la seule dont on soit sûr
// qu'elle existe avant d'avoir parlé au serveur. Le décor VIVANT, lui, ne
// choisit rien — le serveur tire son lieu au sort.
inline constexpr std::string_view kLocalBackdropMap = "prontera";

// LE DÉCOR DE L'ÉCRAN DE CONNEXION : une ville, vue d'en haut, qui tourne.
//
// 🔴 C'EST UNE SCÈNE, PAS UNE VUE DU MONDE, et cette distinction règle à elle
// seule la fin de la session spectateur. Le dessin du jeu compose ses acteurs
// depuis `sim::World` et VIDE la scène quand la liaison se ferme : il ne sait
// pas geler un décor. Ce décor-ci ne lit aucun monde — il possède sa carte —,
// si bien que rendre la session spectateur ne lui retire rien du tout. La ville
// reste debout, et il n'y a pas de gel à écrire.
//
// 🔴 IL SUIT LA PAGE « GRAPHICS » DU JOUEUR, SAUF LE BROUILLARD : ombres et
// lumière de la carte, lumière froide, filtrage du décor, fondus et lumière
// des passants, ombres temps réel du soleil et des lumières ponctuelles,
// cycle jour/nuit — et, posés par l'appelant, la cible de la scène et son
// post-traitement. C'est l'aperçu de ce que le joueur règle depuis l'écran de
// connexion, et la vitrine du rendu : ce que le client sait faire s'y montre
// quand le réglage est allumé, et s'éteint avec lui. Le brouillard reste
// éteint : voir `kBackdropFogShown`.
//
// ⚠ ET IL NE CONSULTE PAS L'ÉTABLI DU STAFF. Un décor qui passerait par le
// dessin du monde honorerait les réglages de l'établi — `staff_ground_solid` peint
// un aplat gris à la place de la ville, derrière le formulaire. Ce chemin de
// dessin ne connaît pas ces réglages : il ne peut pas en hériter.
//
// ⚠ PAS DE COUCHE D'ÉCRAN : ni jauges, ni noms, ni bulles — ce sont des pièces
// de JEU, et il n'y a pas de jeu ici. Les EFFETS, eux, sont la ville : ses
// émetteurs, ses fumées, son ciel, et ce que font ses passants. Voir
// `BackdropEffects`.
//
// 🔴 MAIS IL S'ENTEND, ET C'EST UN AJOUT VOULU. Un bruit n'est pas une pièce de
// jeu : c'est ce que fait la ville. La montrer muette pendant que ses passants
// marchent et frappent reviendrait à filmer derrière une vitre — et le joueur qui
// regarde cet écran est précisément celui qui a le temps d'écouter.
class BackdropScene : private LitLayers {
 public:
  BackdropScene() = default;

  BackdropScene(const BackdropScene&) = delete;
  BackdropScene& operator=(const BackdropScene&) = delete;
  BackdropScene(BackdropScene&&) = delete;
  BackdropScene& operator=(BackdropScene&&) = delete;
  ~BackdropScene() override = default;

  // Charge la carte et téléverse sa géométrie.
  //
  // ⚠ ÉCHOUE PLUTÔT QUE DE SE TAIRE : un écran de connexion sans fond est un
  // écran noir, et un écran noir ne se distingue pas d'un plantage.
  //
  // `read` dit le sol à bâtir, à qui dire l'avancement, et le magasin des
  // documents d'édition : la ville se montre retouchée comme en partie.
  [[nodiscard]] ro::core::Result<void> Open(ro::vfs::Vfs& vfs, std::string_view map_name,
                                            const MapStageRead& read = {});

  // OÙ LA CAMÉRA REGARDE : la case où le serveur a posé son spectateur.
  //
  // 🔴 SANS ELLE, LE DÉCOR VIVANT MONTRERAIT LE CENTRE GÉOMÉTRIQUE DE LA CARTE,
  // qui n'est pas le lieu où il y a du monde. Le serveur tire son point au sort
  // parmi des places FRÉQUENTÉES (`char_spectator_pick_point`) : viser ailleurs
  // reviendrait à ouvrir une session peuplée pour filmer un champ vide.
  //
  // ⚠ SANS EFFET SUR LE REPLI LOCAL, qui n'a personne à regarder et garde le
  // centre.
  void WatchCell(ro::sim::CellPosition where);

  // Dessine la ville à l'instant donné, et ceux qui s'y trouvent.
  //
  // `world` est nul pour le repli local : il n'y a alors ni passants, ni
  // garde-robe à consulter.
  //
  // 🔴 `world_map` EST LA CARTE OÙ CE MONDE SE TROUVE, et la scène REFUSE d'y
  // peindre qui que ce soit si ce n'est pas la sienne. Le repli local est
  // prontera ; les passants de louyang posés dessus se retrouveraient dans un
  // mur ou dans le vide, à des coordonnées qui ne veulent rien dire ici. Le
  // cas arrive pour de bon — la ville du serveur peut ne pas se monter, et la
  // session, elle, continue de verser son monde.
  //
  // ⚠ LA COMPARAISON EST ICI ET NON CHEZ L'APPELANT, pour qu'il ne puisse pas
  // l'oublier : c'est la scène qui sait sur quoi elle est montée.
  //
  // ⚠ LA CIBLE D'AFFICHAGE DOIT ÊTRE DÉJÀ LIÉE. Cette scène ne possède pas le
  // contexte graphique — c'est la coquille qui l'a —, et elle ne le cherche pas
  // : ce qui décide de l'ordre des couches dans une image est l'appelant, pas
  // chacun de ceux qui y peignent.
  //
  // ⚠ `audio` PEUT ÊTRE NUL, et l'est quand rien ne doit s'entendre : la scène
  // ne consulte alors aucune table et ne décode aucun fichier. Chercher un
  // bruit qu'on n'entendra pas est du travail pur. ⚠ LES EFFETS NE SE PEIGNENT
  // PAS SANS LUI : leur couche fait entendre ce qu'elle sème.
  //
  // `viewport_width` et `viewport_height` sont la cible, en pixels : la voie
  // d'écran des effets y pose ses quads.
  //
  // `sky_hour` est l'heure du ciel, dans [0, 24) : l'heure fixe du joueur ou
  // celle du serveur. Rien garde la lumière du RSW, comme en partie quand le
  // fuseau est inconnu. Seul le cycle jour/nuit allumé la lit.
  //
  // 🔴 `graphics` SE RELIT À CHAQUE IMAGE, comme en partie : le joueur règle
  // depuis l'écran de connexion, et la ville est son seul aperçu. Voir
  // `kBackdropFogShown` pour l'unique réglage que le décor n'écoute pas.
  void Draw(float aspect_ratio, std::uint32_t elapsed_ms, ro::render::ActorWardrobe* wardrobe,
            const ro::sim::World* world, std::string_view world_map, std::uint32_t now_ms,
            ro::audio::AudioEngine* audio, float viewport_width, float viewport_height,
            const GraphicsOptions& graphics, std::optional<float> sky_hour);

  // Ce que la dernière image a constaté, rendu au monde des passants HORS du
  // dessin : les disparitions et les effets finis à `now_ms` (`SettleWorld`),
  // et ce que les effets ont posé sur eux.
  void Settle(ro::sim::World& world, std::uint32_t now_ms);

  // Le skin a changé : voir `BackdropEffects::ForgetTextures`.
  void ForgetEffectTextures() {
    if (effects_.has_value()) {
      effects_->ForgetTextures();
    }
  }

  // LA COULEUR DE FOND DE LA VILLE MONTÉE, ou celle d'un écran sans ville.
  //
  // ⚠ ELLE SERT À QUI CHOISIT DE NE PAS DESSINER LA SCÈNE : c'est le terrain
  // qui efface d'ordinaire, et ne pas l'appeler laisse la dernière image. Voir
  // `GlContext::ClearBoundTarget`.
  [[nodiscard]] const std::array<float, 4>& background() const noexcept {
    return lit_.background();
  }

  [[nodiscard]] bool ready() const noexcept { return lit_.ready(); }
  // Le nom de la carte montée, ou vide.
  [[nodiscard]] std::string_view map_name() const noexcept { return lit_.map_name(); }

 private:
  // Où la caméra regarde : la case surveillée, ou le centre de la carte tant
  // que personne n'en a désigné une.
  [[nodiscard]] glm::vec3 LookAt() const;
  // Compose les passants de cette carte et les fait entendre. ⚠ AVANT TOUTE
  // PEINTURE : leur silhouette entre dans l'ombre du soleil, que le sol lit.
  // Sous le soleil, un corps perd son ombre ronde.
  void ComposePassersby(const ro::render::Camera& camera, std::uint32_t elapsed_ms,
                        ro::render::ActorWardrobe& wardrobe, const ro::sim::World& world,
                        std::uint32_t now_ms, ro::audio::AudioEngine* audio,
                        const GraphicsOptions& graphics, bool sun_shadows);
  // LES EFFETS DE LA VILLE, à leur place dans la séquence de la scène éclairée.
  void DrawBehindBodies(const ro::render::Camera& camera, float aspect_ratio,
                        const LitFrame& frame) override;
  void DrawOverWater(const ro::render::Camera& camera, float aspect_ratio,
                     const LitFrame& frame) override;

  // LA VILLE, SES RENDERERS, SON CIEL ET SES OMBRES, et la séquence qui les
  // peint : celle de toute scène qui montre une carte hors de la partie. Voir
  // `LitStage`.
  LitStage lit_;
  // La case que la caméra vise, ou rien.
  std::optional<ro::sim::CellPosition> watched_;
  // Le premier convive peint a-t-il été annoncé au journal ? Remis à faux à
  // chaque montage : la question se repose pour chaque ville.
  bool told_drawn_ = false;

  // ── DE QUOI COMPOSER LES PASSANTS ─────────────────────────────────────────
  //
  // ⚠ LES MÊMES PIÈCES QUE LA PARTIE, et par la même porte (`ComposeScene`) :
  // deux compositions du même monde finiraient par ne pas montrer la même
  // chose, et c'est le décor qui se tromperait — celui que personne ne
  // regarde d'assez près pour s'en apercevoir.
  //
  // ⚠ MAIS SANS STYLES : les recettes de couleur que le serveur diffuse
  // arrivent par des paquets de JEU, qu'une session spectateur ne reçoit pas.
  // Chacun garde donc son apparence native, ce qui est déjà ce que
  // `ComposeScene` fait d'un style nul.
  // ⚠ SANS CARTE TANT QU'IL N'Y EN A PAS : il en prend une à chaque montage,
  // et un planificateur qui garderait celle de la ville précédente enverrait
  // les passants marcher dans des murs qui ne sont plus là.
  ro::sim::WalkPlanner planner_{nullptr};
  // CE QUE LES PASSANTS FONT ENTENDRE.
  //
  // ⚠ À ELLE, ET NON CELLE DE LA PARTIE : elle retient ce que CHAQUE acteur a
  // déjà fait entendre, par identifiant. Deux sessions qui partageraient cet
  // état se prêteraient des identifiants sans rapport — et c'est le décor qui
  // se tromperait, celui que personne ne regarde d'assez près.
  //
  // ⚠ REFAITE À CHAQUE MONTAGE, pour la même raison que le planificateur : une
  // autre ville, d'autres gens.
  std::optional<ActorSounds> sounds_;
  ComposedScene actors_;
  ActorGhosts ghosts_;
  ItemDrops drops_;
  FrozenPoses frozen_poses_;
  // LES EFFETS DE LA VILLE ET DE SES PASSANTS. ⚠ REFAITS À CHAQUE MONTAGE, pour
  // la même raison que les bruits : une autre ville, d'autres gens.
  std::optional<BackdropEffects> effects_;
  // LE MONDE DU REPLI LOCAL, vide : la ville y a ses effets sans passants.
  ro::sim::World empty_world_;
};

}  // namespace moonclient
