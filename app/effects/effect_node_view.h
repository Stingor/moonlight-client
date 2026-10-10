#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_magic_target.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_sprite.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_seed.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/effect_screen_seed.h"
#include "ro/render/effect_sound_table.h"
#include "ro/render/effect_spawn_request.h"
#include "ro/render/effect_track_motion.h"
#include "ro/render/effect_track_seed.h"
#include "ro/render/effect_visibility.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_renderer.h"
#include "ro/vfs/vfs.h"

#include "effects/drawn_ground_cells.h"
#include "effects/effect_clock.h"
#include "effects/effect_draw_gate.h"
#include "effects/effect_node_command.h"
#include "effects/effect_node_sowing.h"

// LES NŒUDS procéduraux en jeu : la famille d'effets que le client dessine en
// CODE, et dont aucun n'a de fichier — voir `docs/adr/0014`.
//
// Un effet procédural est une TROUPE de nœuds — des particules, un anneau, une
// colonne de lumière — semée à un endroit du monde, qui bat à cadence fixe puis
// s'éteint. Les patrons de chaque effet sont décrits et engendrés en table ; ce qui vit ici les
// sème, les fait battre, et compose ce qu'ils montrent.
//
// ⚠ DEUX FORMES SORTENT D'ICI, PAR DEUX CHEMINS. Les sprites s'en vont au
// renderer d'acteurs, les figures du MONDE au renderer d'effets. C'est le même
// `Drawn` qui compose les deux ; `WorldQuads` ne fait que rendre ce qu'il vient
// de bâtir.
//
// Le paramètre nu et la levée du semis vivent dans `effect_node_sowing.h`, la
// commande « /noeud » dans `effect_node_command.h`, les cases de terrain à
// peindre dans `drawn_ground_cells.h`.
namespace moonclient {

// Une troupe de nœuds en train de jouer.
class EffectNodesInPlay {
 public:
  explicit EffectNodesInPlay(ro::vfs::Vfs& vfs) : vfs_(&vfs) {}

  // Sème une troupe du sprite nommé. Faux si le couple .spr/.act ne se lit
  // pas : c'est à l'appelant de le dire, lui seul sait à qui.
  bool Play(std::string_view sprite_base, glm::vec3 ground_position);

  // TIRE UN SPRITE D'UN POINT À UN AUTRE : la flèche d'un arc, le trait d'une
  // baguette, la munition d'un monstre. De `from` à `to`, hauteur comprise, en
  // `life_ticks` à l'horloge du client — que `flight` peut diviser —, tourné de
  // la face de son vol vue de la caméra à chaque image : voir
  // `ro::render::EffectProjectileScreenAngleDegrees`. Opaque dès sa naissance,
  // au mélange ordinaire. Ses images, son horloge et l'angle ajouté à son image
  // suivent `flight` : voir `ro::render::ProjectileFlight`. Faux quand le
  // sprite ne se lit pas.
  bool ShootSprite(std::string_view sprite_base, glm::vec3 from, glm::vec3 to,
                   std::int32_t life_ticks, const ro::render::ProjectileFlight& flight);

  // JOUE UNE FOIS UNE ACTION D'UN SPRITE D'ACTEUR, aux pieds de `ground_position`
  // — le coup d'une Mandragore, la fumée de Hiding, la glace qui éclate —, à la
  // cadence du `.act` ou à `frame_period_units` s'il est positif, en tics de
  // 24 ms par image ; mort à la dernière. Opaque, au mélange ordinaire. Faux
  // quand le sprite ne se lit pas, ou que l'action manque ou est vide.
  bool PlayActOnce(std::string_view sprite_base, glm::vec3 ground_position,
                   float frame_period_units, std::size_t action_index = 0);

  // Sème TOUS les patrons donnés, chacun selon sa forme. Faux quand aucun n'a
  // pu être semé — ressource illisible, ou liste vide.
  // Tous, et non le premier : un effet dresse sa colonne de lumière ET lâche
  // ses particules, et n'en semer qu'une le tronque sans rien dire.
  // ⚠ SEULS LES PATRONS DUS À CE TIC : le client rappelle son bâtisseur à
  // chaque tic, voir `EffectClock`. `param` est le PARAMÈTRE DE L'INSTANCE —
  // voir `EffectClock::param`. ⚠ `instance_scale` et `instance_second` sont
  // les deux premiers paramètres de l'objet du RSW, tout autre chose.
  bool PlaySeedsDueAt(std::span<const ro::render::EffectNodePattern> patterns,
                      glm::vec3 ground_position, std::int32_t tick, float owner_facing_degrees,
                      const std::optional<glm::vec3>& target_position = std::nullopt,
                      std::int32_t param = kRawEffectParam, float instance_scale = 0.0F,
                      float instance_second = 0.0F);

  // Sème TOUT ce qu'un effet procédural pose : ses patrons de nœuds ET ses
  // anneaux à pistes. Faux quand rien n'a pu être semé.
  //
  // ⚠ LES DEUX TABLES, qui ne se recouvrent pas : les anneaux à pistes tirent
  // leur géométrie d'un ÉTAT, et ne se décrivent pas comme un patron.
  // ⚠ ET LA CIBLE, quand l'appelant en connaît une : le trait tendu du
  // sous-type 38 y accroche son bout lointain, et sans elle il s'éteint, comme
  // le client fait d'une cible introuvable.
  // ⚠ ET LE TROISIÈME, quand l'appelant a un porteur : `owner_facing_degrees`
  // est l'angle de base que le client compose depuis l'acteur sur lequel
  // l'effet naît, et dont partent le cap d'une particule ET l'orientation d'un
  // anneau. Voir `EffectOwnerFacingDegrees`. Son défaut n'est pas « pas
  // d'orientation » mais un porteur tourné vers le NORD, la direction zéro.
  // ⚠ ET LE PORTEUR, quand l'effet lui est ATTACHÉ : ses semis à venir partent
  // de sa place du moment, voir `FollowActor`. Zéro veut dire « posé là », et
  // c'est le cas de tout ce qui n'est pas un état ou un chapeau.
  // ⚠ ET LE PARAMÈTRE DE L'INSTANCE : le gestionnaire de coup y pose le nombre
  // de coups d'un coup multi-coups, et tout autre chemin — le paquet 0x01F3 de
  // `@effect`, un état, un chapeau — pose UN. Seuls les semis qui le suivent le
  // lisent.
  // ⚠ ET LA VIE MINIMALE, quand l'effet accompagne une action dont la durée est
  // connue : une incantation. Le patron ne connaît que la bouffée ; zéro le
  // laisse décider, ce qui est le cas de tout le reste. Voir
  // `EffectNodePool::LiveAtLeast`.
  // ⚠ ET L'ACTEUR DÉSIGNÉ, que la suite d'une compétence nomme et sur qui un
  // nœud fait naître un autre effet — voir `effect_spawn_request.h`. Zéro n'en
  // désigne aucun, comme chez le client pour un effet qu'aucune suite ne pose.
  bool PlayProcedural(std::uint32_t effect_id, glm::vec3 ground_position,
                      const std::optional<glm::vec3>& target_position = std::nullopt,
                      float owner_facing_degrees = 0.0F, std::uint32_t attached_actor_id = 0,
                      std::int32_t param = kRawEffectParam, std::int32_t life_floor_ticks = 0,
                      std::int32_t life_exact_ticks = 0, float instance_scale = 0.0F,
                      std::uint32_t designated_actor_id = 0, float instance_second = 0.0F);
  // LE TIC OÙ LA PROCHAINE INSTANCE SEMÉE PERD SON PORTEUR — un lanceur de
  // case —, consommé par `PlayProcedural`. Voir `EffectNodePool::DieWithBearerAt`.
  void DieWithBearerAtNext(std::int32_t tick) noexcept { next_bearer_death_tick_ = tick; }
  // SI LES INSTANCES À SEMER NAISSENT D'UN CHAPEAU, avec tout ce que leur horloge
  // sèmera ensuite. Voir `ro::render::EffectNodePool::SowAsHat`.
  void SowAsHat(bool from_hat) noexcept { sowing_hat_ = from_hat; }

  // Si nous savons quelque chose de cet effet, par l'une ou l'autre table.
  [[nodiscard]] static bool KnowsProcedural(std::uint32_t effect_id);

  // Combien de tics la troupe d'un effet procédural vit au plus. Zéro quand
  // nous n'en savons rien.
  [[nodiscard]] static std::int32_t ProceduralLifeTicks(std::uint32_t effect_id);

  // FAIT SUIVRE À UN ACTEUR les effets qui lui sont attachés : leurs semis à
  // venir partent de sa place du moment et de sa face. Le client recopie la
  // position de l'acteur dans l'instance à chaque tic, si bien que la boule de
  // feu de Sight tourne autour d'un porteur qui marche.
  //
  // ⚠ SEULS SUIVENT LES EFFETS QUE LA TABLE DE SUIVI DU PORTEUR DÉSIGNE, et
  // parfois à partir d'un tic précis — voir `StepClocks`. 🔴 Les nœuds déjà
  // nés sont emmenés aussi, du pas du porteur (`EffectNodePool::MoveAnchored`) :
  // un effet qui sème une fois puis dure n'a plus d'horloge pour le faire.
  // `owner_body_degrees` : l'angle du corps, que le bouclier relit — voir `MoveAnchored`.
  void FollowActor(std::uint32_t actor_id, glm::vec3 ground_position, float owner_facing_degrees,
                   std::optional<float> owner_body_degrees = std::nullopt);
  [[nodiscard]] bool HasClocksAttachedTo(std::uint32_t actor_id) const;

  // UN PORTEUR PARTI EMPORTE CE QU'IL PORTAIT : ses nœuds, d'écran compris, ET
  // son horloge ; et le trait tendu vers lui s'éteint (`LoseTargetsUnless`).
  // 🔴 LES DEUX : l'horloge laissée seule sèmerait encore là où il se tenait.
  // ⚠ PAS AU CHANGEMENT DE CARTE, où `Forget` fait tout. Voir
  // `EffectNodePool::DropAnchoredUnless` pour ce que zéro veut dire.
  template <typename Predicate>
  void ForgetAbsentBearers(Predicate still_here) {
    pool_.DropAnchoredUnless(still_here);
    pool_.LoseTargetsUnless(still_here);
    std::erase_if(clocks_, [&still_here](const EffectClock& clock) {
      return clock.attached_actor_id != 0 && !still_here(clock.attached_actor_id);
    });
    std::erase_if(screen_nodes_, [&still_here](const ScreenNodeInPlay& live) {
      return live.owner_actor_id != 0 && !still_here(live.owner_actor_id);
    });
  }

  // RETIRE UN EFFET PROCÉDURAL D'UN PORTEUR : ce qu'il a semé, ET son horloge.
  // 🔴 LES DEUX, comme pour un porteur qui s'en va. Ne retirer que les nœuds
  // laisserait l'horloge en resemer au tic suivant, et l'effet repousserait
  // derrière la coupe. C'est ce que fait le client, qui DÉTRUIT l'instance :
  // elle emporte ce qu'elle a bâti et ne sème plus rien.
  void StopEffect(std::uint32_t effect_id, std::uint32_t actor_id) {
    pool_.DropFromEffect(effect_id, actor_id);
    std::erase_if(clocks_, [effect_id, actor_id](const EffectClock& clock) {
      return clock.effect_id == effect_id && clock.attached_actor_id == actor_id;
    });
  }

  // DÉFAIT AU PLUS `count` INSTANCES, les plus anciennes d'abord, d'un effet qui
  // ne sème qu'un nœud chacune : voir `EffectNodePool::DropOldestFromEffect`.
  void StopOldestNodes(std::uint32_t effect_id, std::uint32_t actor_id, std::size_t count) {
    pool_.DropOldestFromEffect(effect_id, actor_id, count);
  }

  // Vrai si ce porteur porte encore cet effet procédural : un nœud, ou son horloge.
  [[nodiscard]] bool CarriesEffect(std::uint32_t effect_id, std::uint32_t actor_id) const {
    return pool_.HasFromEffect(effect_id, actor_id) ||
           std::ranges::any_of(clocks_, [effect_id, actor_id](const EffectClock& clock) {
             return clock.effect_id == effect_id && clock.attached_actor_id == actor_id;
           });
  }

  // Fait battre les troupes et rend les SPRITES à peindre. L'horloge est celle
  // de l'appelant ; la troupe garde le reste d'un tic incomplet.
  //
  // ⚠ C'EST CET APPEL QUI COMPOSE TOUT, anneaux compris : `WorldQuads` ne rend
  // que ce que le même passage a bâti.
  //
  // ⚠ LA CAMÉRA ENTRE ICI, et elle n'est pas facultative : les nœuds dont le
  // mouvement bâtit sa matrice À PARTIR D'ELLE — le quad orienté — se posent
  // dans un plan arbitraire sans elle, et se voient alors par la tranche.
  [[nodiscard]] std::span<const ro::render::DrawnActor> Drawn(std::uint32_t now_ms,
                                                              const ro::render::Camera& camera);

  // LES SPRITES DU DERNIER `Drawn`, par lots qui partagent leur mélange et leur
  // épreuve de profondeur — ce que le MOT DE RENDU de leur nœud décide (voir
  // `ro/render/effect_render_word.h`). Les lots éprouvés viennent d'abord, comme
  // chez le client qui vide les seaux sans épreuve en dernier. Un nœud sans mot
  // se peint en addition et passe l'épreuve.

  //
  // ⚠ ET LES LOTS QUI SE PEIGNENT SOUS LES FICHIERS `.str` VIENNENT EN TÊTE,
  // marqués : voir `ro::render::EffectLotPaintsBeforeStr`.
  struct SpriteNodeBatch {
    std::span<const ro::render::DrawnActor> actors;
    bool additive = true;
    bool depth_tested = true;
    bool paints_before_str = false;
  };
  [[nodiscard]] std::span<const SpriteNodeBatch> SpriteBatches() const noexcept {
    return sprite_batches_;
  }

  // LES DEUX BASCULES D'EFFET DU JEU. Posées par `EffectsInPlay`, qui les tient
  // des réglages — voir `EffectsInPlay::SetEffectSwitches`.
  void SetEffectSwitches(bool shown, bool minimal) noexcept {
    effects_shown_ = shown;
    minimal_effects_ = minimal;
  }
  // LA PLACE DU JOUEUR LOCAL, à chaque image : certains nœuds se sèment autour de lui.
  void SetLocalPlayerPosition(std::optional<glm::vec3> position) noexcept {
    local_player_position_ = position;
  }
  // Vrai quand `/effect` laisse voir cet effet-là, liste blanche comprise.
  //
  // ⚠ EN LIGNE, ET PAS DANS `effect_switches.cpp` COMME SA JUMELLE : la
  // bibliothèque des nœuds sert aussi roc-cli, qui ne lie pas la troupe en
  // fichier. Le corps posé là-bas lui manquerait à l'édition de liens.
  [[nodiscard]] bool ShowsEffect(std::uint32_t effect_id) const {
    return effects_shown_ || ro::render::IsForcedVisibleEffect(effect_id);
  }
  // Vrai quand `/mineffect` demande la variante allégée d'un effet.
  [[nodiscard]] bool minimal_effects() const noexcept { return minimal_effects_; }

  // LES QUADS D'ECRAN de la quatrieme voie (ADR 0022). ⚠ ILS NE SE PROJETTENT
  // PAS : leurs coins sont deja des PIXELS, peints sous une orthographique.
  // `dark_veil` rend le SEUL voile de ténèbres et la brume du poison, et sinon
  // tout sauf eux : ce client les peint par-dessus le monde et sous
  // l'interface, dans une passe à eux. Voir `OverWorldLayer`.
  [[nodiscard]] std::span<const ro::render::DrawnScreenQuads> ScreenQuads(
      const ro::render::Camera& camera, float viewport_width, float viewport_height,
      bool dark_veil = false);

  [[nodiscard]] std::span<const ro::render::DrawnWorldQuads> WorldQuads() const noexcept {
    return world_quads_;
  }

  // LES CASES DE TERRAIN du dernier `Drawn`, à part des quads du monde : sans
  // géométrie encore, seul celui qui tient la carte peut la leur donner.
  [[nodiscard]] std::span<const DrawnGroundCells> GroundCells() const noexcept {
    return ground_cells_;
  }

  // CE QUE LA CARTE DIT À LA TROUPE : l'altitude du sol sous un point — sans
  // elle, un nœud qui REBONDIT traverse le sol —, et si elle élargit les
  // dispersions. ⚠ REPOSÉE À CHAQUE IMAGE : la vue ne tient pas la carte.
  void NoticeWorld(ro::render::EffectScatterWorld world) { pool_.SetWorld(std::move(world)); }
  // LA PORTE DE DESSIN DE L'IMAGE, reposée de même : voir `EffectDrawGate`.
  void NoticeDrawGate(EffectDrawGate gate);

  // LES SONS que le dernier `Drawn` a fait avoir, à emporter pour les jouer :
  // les rebonds de la troupe, et les semis qui sonnent — leur nom se forme ici,
  // depuis la table des sons d'effet, au point de l'instance qui a semé.
  // ⚠ APRÈS `Drawn` ET NON AVANT : c'est lui qui fait battre la troupe, et les
  // demander d'abord rendrait toujours ceux de l'image précédente.
  [[nodiscard]] std::vector<ro::render::EffectNodeSound> TakeSounds() {
    std::vector<ro::render::EffectNodeSound> sounds = pool_.TakeBounces();
    for (const SowingSound& sowing : sowing_sounds_) {
      sounds.push_back(
          {std::string(ro::render::EffectSound(sowing.effect_id)), sowing.ground_position});
    }
    sowing_sounds_.clear();
    return sounds;
  }

  [[nodiscard]] std::size_t living() const noexcept { return pool_.size(); }

  // CE QUE LE DERNIER `Drawn` A ÉCARTÉ, et pour laquelle des deux raisons.
  // ⚠ DEUX FAÇONS DE SORTIR SANS RIEN RENDRE, et elles accusent des pièces
  // OPPOSÉES : une texture que la fenêtre n'a pas chargée met en cause la
  // ressource, une figure qui ne rend aucun quad met en cause le dessin ou le
  // mouvement. Les compter coûte deux entiers et évite de deviner ; le juge des
  // effets s'en sert pour dire POURQUOI un effet est muet, au lieu de laisser
  // rouvrir chaque cas à la main.
  [[nodiscard]] std::size_t nodes_without_texture() const noexcept { return sans_texture_; }
  [[nodiscard]] std::size_t nodes_without_quad() const noexcept { return sans_quad_; }

  // LA TROUPE VIVANTE, en lecture. Sert à DIAGNOSTIQUER, et à rien d'autre :
  // qui veut peindre appelle `Drawn`.
  //
  // ⚠ « LA FIGURE NE REND AUCUN QUAD » NE DIT PAS POURQUOI, et les causes
  // n'envoient pas au même endroit : une piste jamais allumée accuse le SEMIS,
  // un alpha qui reste nul accuse le MOUVEMENT, un rayon nul accuse la graine,
  // et le reste accuse le DESSINATEUR. Le juge des effets lit ces quatre-là.
  [[nodiscard]] std::span<const ro::render::PooledEffectNode> living_nodes() const noexcept {
    return pool_.nodes();
  }

  // Combien de quads du monde le dernier `Drawn` a composés. Sert à répondre
  // « j'ai semé, et voici ce que cela donne » plutôt qu'à laisser l'écran seul
  // témoin.
  [[nodiscard]] std::size_t world_quad_count() const noexcept {
    std::size_t total = 0;
    for (const std::vector<ro::render::EffectWorldQuad>& lot : ring_quads_) {
      total += lot.size();
    }
    return total;
  }

  // Oublie TOUT ce qui joue, sans rien oublier de ce qui est LU.
  //
  // Les deux moitiés sont délibérées. Le juge des effets en essaie mille cent
  // douze à la suite, et chacun doit partir d'une troupe vide : ce que le
  // précédent a laissé — un effet INFINI n'en laisse jamais rien d'autre — se
  // compterait sinon au crédit du suivant. Mais les textures déjà décodées
  // restent : les relire à chaque essai ferait du juge une affaire de minutes
  // pour rien, et deux effets sur trois partagent leurs images. Voir
  // `effect_node_spawn.cpp`.
  void Forget();

  // LES EFFETS QUE LES NŒUDS ONT DEMANDÉS au dernier `Drawn`, que l'appelant
  // EMPORTE pour les faire naître. Voir `effect_spawn_request.h`.
  //
  // ⚠ LE CLIENT DEMANDE À CHAQUE IMAGE PEINTE ; NOUS, À CHAQUE IMAGE QUI SUIT AU
  // MOINS UN TIC. C'est une demande par tic quand l'image est plus courte, une
  // par image sinon — la cadence du client à soixante images par seconde, sans
  // que cent quarante images doublent les effets semés.
  [[nodiscard]] std::vector<ro::render::EffectSpawnRequest> TakeSpawnRequests() {
    return std::exchange(spawn_requests_, {});
  }

 private:
  // Sème une troupe de SPRITES d'après son patron. Faux si le couple .spr/.act
  // ne se lit pas — c'est le seul échec possible, et il vaut pour tous les
  // appelants.
  // ⚠ L'ÉCHELLE EST UNE FOURCHETTE, et chaque nœud de la troupe tire la
  // sienne : le client en fait une nuée de tailles mêlées. Une borne haute nulle
  // dit « pas de tirage », et tous prennent alors la même.
  // ⚠ LA CIBLE N'ENTRE QUE POUR LES PROJECTILES VISÉS et les actions au quart
  // de tour — `aims_at_target`, `action_from_aim_quadrant` — et vide veut dire
  // « pas de cible » : le nœud ne part pas et garde l'action du patron.
  bool Sow(const std::string& sprite_path, const ro::render::EffectNodeSeed& seed,
           ro::render::EffectNodeRange sprite_scale, float scatter, glm::vec3 ground_position,
           std::size_t how_many, float owner_facing_degrees,
           const std::optional<glm::vec3>& target_position = std::nullopt);

  // Le couple .spr/.act d'un chemin sans extension, lu une fois puis retenu ;
  // nul quand il ne se lit pas.
  const ro::render::LoadedActorSprite* LoadedSprite(const std::string& sprite_path);

  // La texture d'un chemin, lue une fois puis retenue — l'échec comme le
  // succès. Rend une image VIDE quand le fichier ne se lit pas.
  const ro::formats::Image& TextureFor(const std::string& path);

  // Sème UN nœud d'anneau, au tic `effect_tick` de son instance. Faux si sa
  // PREMIÈRE texture ne se lit pas. Un seul, sans dispersion : un anneau est
  // une figure entière, là où les particules sont une nuée.
  // ⚠ LE PARAMÈTRE DE L'INSTANCE VA JUSQU'ICI, et il n'y sert qu'à la TEINTE :
  // l'anneau du classement PVP y lit le rang qui décide de sa couleur. Voir
  // `ro/render/pvp_rank_tint.h`.
  bool SowRing(const ro::render::EffectNodePattern& pattern, glm::vec3 ground_position,
               std::int32_t life_left_ticks, std::int32_t effect_tick, float owner_facing_degrees,
               std::int32_t param, const std::optional<glm::vec3>& target_position = std::nullopt);

  // UN exemplaire d'un patron d'anneau, à sa place dans la disposition. Sortie
  // de `SowRing` quand un patron a cessé de n'en semer qu'un.
  // LES ANGLES DU DERNIER PROJECTILE de l'instance qui sème, et sa place dans
  // `place` quand il vit encore. Voir `EffectNodeSupplement::records_projectile`.
  [[nodiscard]] ro::render::EffectProjectileAngles LastProjectileOf(float owner_facing_degrees,
                                                                    glm::vec3& place) const;
  bool SowOneRing(const ro::render::EffectNodePattern& pattern, glm::vec3 ground_position,
                  std::int32_t life_left_ticks, std::int32_t effect_tick,
                  const std::vector<std::string>& paths, std::uint16_t exemplaire,
                  std::uint16_t how_many, std::uint32_t& generator, float owner_facing_degrees,
                  std::int32_t param, const std::optional<glm::vec3>& target_position);

  // Sème les ANNEAUX À PISTES d'un effet. Faux quand il n'en a pas, ou
  // qu'aucune de leurs textures ne se lit.
  bool PlayRings(std::uint32_t effect_id, glm::vec3 ground_position);

  // Sème les NŒUDS DE PARTICULES d'un effet — les DEUX bâtisseurs du sous-type
  // 43. Seule la bouffée blanche se resème après le tic zéro. Faux de même.
  bool PlayRisingParticles(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // Les arcs de ruban du sous-type 82 : un par appel, et le 681 en pose quatre.
  // ⚠ ET L'ÉCHELLE DE L'INSTANCE : neuf styles d'arc en
  // tirent leur rayon. Zéro veut dire « aucune », et le style prend alors son
  // rayon constant — le cas de tout effet lancé par une compétence. Seul un
  // semeur de carte en pose une, prise sur son objet du RSW.
  bool PlayArcRibbons(std::uint32_t effect_id, glm::vec3 ground_position,
                      float instance_scale = 0.0F, std::uint32_t bearer_id = 0);
  // Les carrés teintés du sous-type 47.
  bool PlayTintedQuads(std::uint32_t effect_id, glm::vec3 ground_position);
  // Les champs dispersés du sous-type 33 — nuages, brouillard, étoiles.
  bool PlayScatterFields(std::uint32_t effect_id, glm::vec3 ground_position);
  // Les objets qui tombent du sous-type 87 — potions et lances.
  bool PlayFallingItems(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES MONSTRES DU 153 ET LES CARRÉS DES 143, 156 ET 172 : `effect_node_sown_figures.cpp`.
  bool PlaySownFigures(std::uint32_t effect_id, glm::vec3 at, std::int32_t tick,
                       float rsw_first_param, float rsw_second_param);
  // Les éventails du sous-type 147 — les compteurs de rotation, trois effets de prêtre.
  bool PlaySpiralFans(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES ANNEAUX DE TÉLÉPORTATION, sous-type 57 : un seul nœud par effet, semé au
  // tic zéro — le bâtisseur ne fait rien aux autres tics.
  bool PlayTeleportRings(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES ANNEAUX EN COEUR : un noeud par point du dessin, vingt pour EF_HEARTCASTING.
  bool PlayHeartRings(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES ONDES DE RUBAN du sous-type 36 : un anneau qui respire, parcouru
  // d'une vague. Faux quand l'effet n'en a pas, ou que sa texture ne se lit pas.
  bool PlayRibbonWaves(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES ANNEAUX QUI TOURNENT du sous-type 50 : le frère du précédent, même
  // figure et même onde, mouvement tout autre.
  bool PlayTurningRings(std::uint32_t effect_id, glm::vec3 ground_position);
  // Les anneaux en COUCHES du sous-type 23 — quatre couronnes emboîtées, chacune
  // à son rayon et à son inclinaison.
  bool PlayLayeredRings(std::uint32_t effect_id, glm::vec3 ground_position);
  // Les anneaux de PARTICULES du sous-type 31 — une croix de quatre couronnes
  // qui s'élargit.
  bool PlayParticleRings(std::uint32_t effect_id, glm::vec3 ground_position);
  // La famille `Ring_0` dans son entier : une vingtaine de bâtisseurs, une seule
  // figure, et leurs quatre pistes reprises telles quelles.
  // ⚠ AU TIC DU, comme les étincelles : un effet pose souvent DEUX anneaux, à
  // deux tics et sous deux textures.
  bool PlaySeededRings(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);

  // LES ÉTINCELLES QUI MONTENT, sous-type 59 — celles d'EF_HEAL.
  // ⚠ AU TIC DU, ET NON UNE FOIS POUR TOUTES. Un effet lâche plusieurs bouffées
  // d'étincelles — EF_HEAL4 en lâche sept, une tous les neuf tics jusqu'au
  // cinquantième —, et tout semer d'un coup donne sept fois moins de vert qu'il
  // n'en faut.
  bool PlayRisingSparks(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES CASCADES : quatre rideaux qui défilent, semés une fois pour toutes.
  //
  // ⚠ ELLES NE TIRENT RIEN, seules de la famille : leurs deux bâtisseurs ne
  // consultent aucun générateur, et une cascade rejouée est la même.
  bool PlayScrollingBands(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES VOILES : un carré de ténèbres qui s'épaissit sur celui qui le porte.
  bool PlayDarkVeils(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES RUBANS TYPÉS : quatre rubans qui ondulent et tournent autour du lanceur.
  bool PlayTypedRibbons(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES TRAÎNÉES PENTAGONALES : quatre couches de nuage qui s'élèvent.
  bool PlayPentagonTrails(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES ÉCLATS DE GEL : quatre nuances d'une palette en spirale, aux tics 0 et 10.
  bool PlayPaletteBillboards(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES TOURBILLONS : des carrés empilés qui tournent et rétrécissent.
  bool PlaySpiralStacks(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES CARRÉS QUI RESPIRENT : un carré posé à plat dont le rayon oscille.
  bool PlayBreathingQuads(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES CERCLES MAGIQUES : des disques au sol empilés, dont un contre-tourne.
  bool PlayGroundDiscs(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES ÉVENTAILS : un nœud par tour de la boucle du site d'appel.
  bool PlayConeWedges(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES RUBANS AU SOL : un trait qui glisse vers une cible.
  bool PlayGroundRibbons(std::uint32_t id, glm::vec3 ground, const std::optional<glm::vec3>& aim);
  // LES NŒUDS D'ECRAN : un quad plat, en pixels, tourne d'un angle.
  bool PlayScreenNodes(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick,
                       std::uint32_t owner_id);
  // LA GERBE DE REFLETS d'un impact : huit quads d'ecran, un tous les
  // quarante-cinq degres, dont l'image alterne d'un rang a l'autre.
  bool PlayLensFlares(std::uint32_t effect_id, glm::vec3 ground_position, std::uint32_t owner_id);

  // LA CROIX D'UN IMPACT : deux quads d'écran à quatre-vingt-dix degrés. ⚠ SON
  // PROPRE SEMIS, comme la gerbe de reflets, et pour la même raison : ses deux
  // branches partagent un tirage d'angle, et la table des nœuds d'écran ne sait
  // dire ni le compte ni le partage.
  bool PlayHitCrosses(std::uint32_t effect_id, glm::vec3 ground_position, std::uint32_t owner_id);
  // L'ÉTOILE DE POINTES DE GLACE : neuf traits fendus, un tous les quarante
  // degrés, dont un sur trois est deux fois plus long. ⚠ SON PROPRE SEMIS pour
  // la même raison que la croix — le compte et l'alternance vivent dans la
  // BOUCLE du bâtisseur, et la table des nœuds d'écran ne lit que le sous-type
  // zéro.
  bool PlaySplitBeams(std::uint32_t effect_id, glm::vec3 ground_position, std::uint32_t owner_id);
  // JUSQU'À QUEL TIC CET EFFET SÈME ENCORE, toutes familles confondues. Zéro
  // quand tout naît au premier tic, et l'effet n'a alors pas d'horloge.
  [[nodiscard]] std::int32_t LastSowTick(std::uint32_t effect_id,
                                         std::span<const ro::render::EffectNodePattern> patterns,
                                         glm::vec3 ground_position,
                                         const std::optional<glm::vec3>& target_position) const;
  // LES COLONNES QUI TOURNENT : quatre piliers, dresses sur la cible s'ils visent.
  bool PlaySpiralColumns(std::uint32_t effect_id, glm::vec3 ground_position,
                         const std::optional<glm::vec3>& target_position);
  // LES CARRES CONCENTRIQUES : une gerbe de carres qui tremblent et orbitent.
  bool PlayConcentricSquares(std::uint32_t effect_id, glm::vec3 ground, std::int32_t count);
  // LES MURS TRIPLES : deux noeuds, quatre pistes, trois quads par piste. Un
  // batisseur sur trois attend le cinquieme tic, d'ou le tic.
  bool PlayTripleWalls(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES COQUILLES SPHERIQUES : quinze panneaux tangents, peints sur trois rayons.
  bool PlayGrandCrosses(std::uint32_t effect_id, glm::vec3 ground_position);
  bool PlaySphereShells(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES ESSAIMS : trois bouffees a dix tics d'intervalle, d'ou le tic.
  bool PlaySwarms(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES TENTURES AU SOL : les marques des chants et des danses.
  bool PlayGroundCurtains(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES TUBES D'ETINCELLE : une rafale dont la fenetre vit dans le batisseur.
  bool PlaySparkTubes(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick,
                      const std::optional<glm::vec3>& target_position);
  // LES TAILLADES QUI GLISSENT : quatre carres plats qui filent et accelerent.
  bool PlayGlidingSlashes(std::uint32_t effect_id, glm::vec3 ground_position,
                          const std::optional<glm::vec3>& target_position);
  // LES RAFALES : seize particules par noeud, et une vie qui ne finit pas.
  bool PlayQuadBursts(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES ECLATS QUI FUIENT : quatre barreaux qui traversent le centre.
  bool PlayFlyingShards(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES GRAINS QUI DERIVENT : l'entier du site est aussi le tic du semis.
  bool PlayDriftingMotes(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES PETALES QUI S'ECARTENT : seize noeuds, au tic que donne l'entier du site.
  bool PlayRadialPetals(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES TROIS PLANS : trois carres par piste, et l un des deux attend le tic 30.
  bool PlayTriPlanes(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES ETINCELLES QUI CONVERGENT : quatre lames doubles qui foncent au centre.
  bool PlayHealSparkles(std::uint32_t effect_id, glm::vec3 ground_position);
  bool PlayHomingSparks(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES BOITES AU SOL : une seule piste vive, et l octet du site porte tout.
  bool PlayGroundBoxes(std::uint32_t effect_id, glm::vec3 ground_position);
  bool PlayGroundGusts(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES BOURRASQUES QUI POURSUIVENT : la cible est relue à chaque tic, et le 621
  // sème un nœud par coup — `param` en porte le nombre.
  bool PlayHomingGusts(std::uint32_t effect_id, glm::vec3 ground_position,
                       const std::optional<glm::vec3>& target_position, std::int32_t param);
  // LES ROUES DE VENT : cinq nœuds, un par tic, sur une croix.
  bool PlayRollingGusts(std::uint32_t effect_id, glm::vec3 ground_position,
                        const std::optional<glm::vec3>& target_position);
  // LES CARRÉS QUI FILENT (116) : un objet tous les trois tics, jusqu'au douzième.
  bool PlaySwirlTrails(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES ANNEAUX QUI S'ÉLARGISSENT (169) : un nœud par tic, du zéro au troisième.
  bool PlayExpandingRings(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES ICÔNES QUI FLOTTENT (123) : quatre-vingts nœuds d'un coup, au tic zéro.
  bool PlayDriftingIcons(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES DISPERSIONS D'ÉCLAIRS (135) : deux nœuds au tic zéro, au cap du porteur.
  bool PlayElectricArcs(std::uint32_t effect_id, glm::vec3 ground_position,
                        float owner_facing_degrees);
  // LES ECLATS DE QUARTIERS : douze triangles qui ferment le tour en trois tics.
  bool PlayPieWedges(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES FAISCEAUX EN PIQUE : douze rubans plats qui ferment le tour en trois tics.
  bool PlaySpikeBeams(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES COLONNES D AURA : dix carres emboites qui s ouvrent couche par couche.
  bool PlayAuraColumns(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES TRAINEES QUI S EFFACENT : sans cible, elles ne sement RIEN.
  bool PlayFadingStreaks(std::uint32_t effect_id, glm::vec3 ground_position,
                         const std::optional<glm::vec3>& target_position);
  // LES ARCS-EN-CIEL : sept bandes, et un plan fige au tic de naissance.
  bool PlayRainbowTrails(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES GERBES DE FEUX D'ARTIFICE : quatre grains retardes, qui renaissent.
  bool PlaySpiralTrails(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES AURAS EN ANNEAU DOUBLE : deux noeuds, dont le second nait en retard.
  bool PlayDoubleRibbons(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES PLUIES DE GRAINS : une bouffee de quatre grains tous les dix tics.
  bool PlayRainParticles(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES SPRITES QUI ENFLENT : leur retard les echelonne, pas leur semis.
  bool PlayGrowingSprites(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES NAPPES AU SOL : soixante carreaux jointifs, tournes vers la camera.
  bool PlayGridPlanes(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES JUPES QUI DEFILENT : quatre anneaux imbriques dont l image change au tic.
  bool PlayCyclingSkirts(std::uint32_t effect_id, glm::vec3 ground_position);
  // LE BOUMERANG : il part du lanceur, vise sa victime et revient. Sans
  // cible, il ne sème RIEN — voir `SowEffectShieldBoomerangNode`. `param` fait le
  // rayon de son frère qui éclate, l'effet 265.
  bool PlayShieldBoomerangs(std::uint32_t effect_id, glm::vec3 ground_position,
                            const std::optional<glm::vec3>& target_position, std::int32_t param);
  // LES ÉTINCELLES DU SOUS-TYPE 45 : nées sur le point visé, RIEN sans lui.
  bool PlayThunderScatters(std::uint32_t effect_id, glm::vec3 ground_position,
                           const std::optional<glm::vec3>& target_position);
  // LE CŒUR QUI ERRE DU SOUS-TYPE 145 : deux nœuds autour du porteur.
  bool PlayThunderFreezes(std::uint32_t effect_id, glm::vec3 ground_position);
  // LA LUEUR QUI PASSE EN ARC DU SOUS-TYPE 109 : un nœud sur le porteur. Les
  // deux suivants ne peignent rien et font naître d'autres effets. Voir
  // `effect_node_spawn.cpp`.
  bool PlayArcingLights(std::uint32_t effect_id, glm::vec3 ground_position,
                        std::uint32_t designated_actor_id);
  bool PlaySpiralEmitters(std::uint32_t effect_id, glm::vec3 ground_position,
                          std::uint32_t designated_actor_id);
  bool PlayWalkingSowers(std::uint32_t effect_id, glm::vec3 ground_position,
                         const std::optional<glm::vec3>& target_position);
  // LES DEUX RAYONS DU SOUS-TYPE 154 : un nœud tourné une fois vers la cible.
  bool PlayAimedSpokes(std::uint32_t effect_id, glm::vec3 ground_position,
                       const std::optional<glm::vec3>& target_position);
  // LES RAIS DE L'ONDE DE CHOC DU SOUS-TYPE 54 : un nœud par appel, à la création.
  bool PlayShockwaveRays(std::uint32_t effect_id, glm::vec3 ground_position);
  bool PlayTaperedSheets(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES COURONNES DE MAGNUM BREAK : un nœud tous les trois tics, du 27e au 54e.
  bool PlayBreakRings(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LE TOURBILLON DU SOUS-TYPE 140 : au point visé, tous les 25 tics jusqu'au 75e.
  bool PlayTetraVortices(std::uint32_t effect_id, glm::vec3 ground_position,
                         const std::optional<glm::vec3>& target_position, std::int32_t tick);
  // LES TRAITS TENDUS : le bout lointain suit l'acteur visé, quand il y en a un.
  bool PlayTetheredBeams(std::uint32_t effect_id, glm::vec3 ground_position,
                         const std::optional<glm::vec3>& target_position,
                         std::uint32_t target_actor_id);
  // LES OBJETS LANCÉS : ils partent du lanceur vers sa cible, et sans cible ils
  // ne sèment RIEN — voir `SowEffectThrownItemNode`. Le paramètre de l'instance
  // donne sa teinte au flacon du style un.
  //
  // ⚠ CHAQUE OBJET NAÎT À UN SEUL TIC, que son style fixe : voir
  // `EffectThrownItemBirthTick`. L'horloge rappelle donc ce semis à chaque tic,
  // et `ground_position` est la place du lanceur À CE TIC-LÀ.
  bool PlayThrownItems(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick,
                       const std::optional<glm::vec3>& target_position, std::int32_t param);
  bool PlaySpinRibbons(std::uint32_t effect_id, glm::vec3 ground_position);
  bool PlaySnakingArcs(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  bool PlaySpinningRounds(std::uint32_t effect_id, glm::vec3 ground_position);
  bool PlayWanderingMotes(std::uint32_t effect_id, glm::vec3 ground_position);
  bool PlayJobAuras(std::uint32_t effect_id, glm::vec3 ground_position);
  // ⚠ LA FUMÉE DU MUR ATTEND LE TIC DEUX : son bâtisseur pose trois nœuds de
  // trois sous-types aux trois premiers tics, et celui-ci vient en dernier.
  bool PlayCloudRings(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  bool PlayTwinDiscs(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES DEUX DISQUES QUI S'ÉTEIGNENT (161) : un nœud au tic zéro.
  bool PlayFadingTwinDiscs(std::uint32_t effect_id, glm::vec3 ground_position);
  // LE CYLINDRE DE NOTES (173) : quatre nœuds au tic zéro, un par appel du cas.
  bool PlayRainstormCylinders(std::uint32_t effect_id, glm::vec3 ground_position);
  bool PlayRingQuartets(std::uint32_t effect_id, glm::vec3 ground_position);
  // LA GERBE D'ANNEAUX (487) : un nœud au tic un, un autre au tic deux.
  bool PlayRingStarbursts(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  bool PlaySpriteFans(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  bool PlayScreenBursts(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick,
                        std::uint32_t owner_actor_id);
  bool PlayAnchoredSkirts(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  bool PlayLightningFans(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES CADRES ET LES BARRES : deux noeuds par effet, a deux octets suivis.
  bool PlayFrameAndBars(std::uint32_t effect_id, glm::vec3 ground_position);
  // LES TRAITS GAINES : un noeud par tic sous la borne, chacun vers la cible.
  bool PlaySheathedBolts(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick,
                         const std::optional<glm::vec3>& target_position);
  // LES EVENTAILS TOURNES VERS LA CAMERA : deux noeuds a vingt tics d'ecart,
  // trois trainees dans chacun, et trois images par noeud.
  bool PlayCameraFans(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);
  // LES SPRITES QUI S'ELEVENT : l'ange, ses ailes et ses plumes. Les ailes se
  // resement tous les trois tics jusqu'au dix-huitieme.
  bool PlayFadingSprites(std::uint32_t effect_id, glm::vec3 ground_position, std::int32_t tick);

  // LES NOEUDS QUI NE TIENNENT PAS DANS UN SEUL LOT : leurs quads ne prennent
  // pas tous la MEME image, et le renderer paie un lot par texture. Rend VRAI
  // quand ce noeud a ete compose ici. Voir effect_node_compose_multi.cpp.
  bool ComposeNodeInSeveralLots(const ro::render::PooledEffectNode& pooled,
                                std::size_t& missing_texture, std::size_t& missing_quad);
  // UNE PASSE DES NŒUDS D'ÉCRAN, entrelacée avec celle de la troupe.
  void ScreenPass(bool single_frame, std::uint32_t& generator);
  // LES ANNEAUX QUI PULSENT : trois jupes décalées, et une piste qui ne se
  // dessine pas mais porte le mode de tout le nœud.
  bool PlayPulsingRings(std::uint32_t effect_id, glm::vec3 ground_position);
  // Les anneaux d'un portail de téléportation, sous-type 65 : deux étages de
  // quatre, dont le rayon se referme puis se rembobine sans fin.
  bool PlayWarpRings(std::uint32_t effect_id, glm::vec3 ground_position);
  // LA GRILLE DE VISÉE : sept cases sur sept, peintes à même le terrain.
  bool PlayMagicTargets(std::uint32_t effect_id, glm::vec3 ground_position);

  // COMPOSE LA GRILLE DE VISÉE d'un nœud, dans la voie des cases de terrain.
  // Rend VRAI quand quelque chose y est entré.
  // UN NŒUD QUI SE DESSINE EN SPRITE : son image, ses sources, et où il se
  // pose. Voir `effect_node_compose_shapes.cpp`.
  void ComposeNodeAsSprite(const ro::render::PooledEffectNode& pooled,
                           const ro::render::Camera& camera);
  bool ComposeGroundCellsOf(const ro::render::PooledEffectNode& pooled, std::size_t& sans_texture,
                            std::size_t& sans_quad);

  ro::vfs::Vfs* vfs_ = nullptr;
  // Les sprites déjà lus, par chemin. Une troupe en sème des dizaines : les
  // relire à chaque semis ferait un à-coup à chaque lancement.
  //
  // ⚠ CHAQUE NŒUD PORTE SON CHEMIN et le relit ici, plutôt qu'un tableau de
  // pointeurs rangé à côté de la troupe : la troupe TASSE ses morts, et un
  // tableau parallèle ferait hériter chaque survivant du sprite de son voisin.
  std::map<std::string, ro::render::LoadedActorSprite, std::less<>> sprites_;
  // Les textures d'anneau lues, par chemin d'archive ; VIDE pour un fichier
  // illisible, qu'on ne relit pas à chaque coup. Un skin changé les fait relire.
  std::map<std::string, ro::formats::Image, std::less<>> textures_;
  std::uint64_t textures_revision_ = 0;
  ro::render::EffectNodePool pool_;
  std::vector<ro::render::ComposedActor> composed_;

  // OÙ UN ACTEUR COMPOSÉ SE POSE, et avec quelle teinte — un par DESSIN et non
  // un par acteur. Un sprite à traînée se dessine plusieurs fois, au même
  // acteur, à des points et des alphas différents ; ranger le point à côté de
  // l'acteur interdirait ce cas.
  //
  // ⚠ LE POINTEUR VERS L'ACTEUR SE POSE EN DERNIER. La liste des acteurs n'a pas
  // fini de grandir quand on remplit celle-ci ; le prendre au fil des ajouts le
  // laisserait pendre à la première réallocation.
  struct ComposedPlacement {
    std::size_t composed_index = 0;
    glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
    std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
    float scale = 1.0F;
    // Le mot de rendu du nœud posé. Voir `SpriteBatches`.
    std::optional<std::uint32_t> render_word;
    float grow_screen_pixels = 0.0F;  // voir `DrawnActor::grow_screen_pixels`
  };
  std::vector<ComposedPlacement> placements_;
  std::vector<ro::render::DrawnActor> drawn_;
  std::vector<SpriteNodeBatch> sprite_batches_;
  // Les quads des anneaux, et les lots qui les désignent. Les deux vivent
  // jusqu'au prochain `Drawn` : le renderer ne garde rien.
  std::vector<std::vector<ro::render::EffectWorldQuad>> ring_quads_;
  std::vector<ro::render::DrawnWorldQuads> world_quads_;
  // Les cases de terrain, et les lots qui les désignent — les mêmes deux
  // tableaux parallèles que pour les quads, et pour la même raison : les
  // pointeurs ne se prennent qu'une fois la liste finie de grandir.
  std::vector<std::vector<ro::render::EffectGroundCellPaint>> ground_cell_lots_;
  std::vector<DrawnGroundCells> ground_cells_;

  // UN NŒUD D'ECRAN EN VIE : son etat, son image, et OU SON PORTEUR SE TIENT
  // dans le monde.
  //
  // ⚠ LA POSITION DU PORTEUR EST GARDEE EN COORDONNEES DU MONDE, et projetee a
  // chaque image : la projeter une fois pour toutes au semis figerait le nœud a
  // l'ecran des que la camera bouge.
  struct ScreenNodeInPlay {
    ro::render::EffectScreenNode node;
    // ⚠ PLUSIEURS IMAGES, ET L'INDEX DE VUE CHOISIT. Un batisseur qui COMPOSE le
    // nom de son image en pose huit, et l'index du noeud tourne dessus. N'en
    // porter qu'une figerait l'explosion sur sa premiere.
    std::vector<std::string> texture_paths;
    glm::vec3 owner_position{0.0F, 0.0F, 0.0F};
    bool beats_every_frame = false;  // voir `EffectBeatsEveryFrame`
    // Voir `PooledEffectNode::instance_beats_left`.
    std::int32_t instance_beats_left = -1;
    bool instance_beats_every_frame = false;  // voir `EffectInstanceBeatsEveryFrame`
    std::uint32_t owner_actor_id = 0;         // qui le porte, ou zéro : voir `FollowActor`
    bool from_hat = false;                    // voir `SowAsHat`
  };
  std::vector<ScreenNodeInPlay> screen_nodes_;
  EffectDrawGate draw_gate_;
  std::vector<std::vector<ro::render::EffectScreenQuad>> screen_quad_lots_;
  std::vector<ro::render::DrawnScreenQuads> screen_quads_;
  std::uint32_t beat_ms_ = 0;
  std::int32_t shared_param_sowings_ = 0;           // voir `EffectParamCounterIsShared`
  EffectClock* sowing_clock_ = nullptr;             // l'instance qui sème, pour ses projectiles
  bool sowing_hat_ = false;                         // voir `SowAsHat`
  std::uint32_t projectiles_numerotes_ = 0;         // voir `EffectClock::projectile_serial`
  std::optional<glm::vec3> local_player_position_;  // voir `SetLocalPlayerPosition`
  // LE GRÉSILLEMENT : le générateur que la SEULE figure qui tire au sort
  // consomme, une image après l'autre.
  //
  // ⚠ IL VIT DANS LA FENÊTRE ET NON DANS LA TROUPE, parce qu'il appartient au
  // DESSIN et non à l'état : deux images d'un même tic doivent grésiller
  // différemment, là où la troupe ne bat qu'une fois par tic. Voir
  // `ComposeLightningFanQuads`.
  std::uint32_t shimmer_ = 1U;
  // Depuis combien d'images de suite une troupe vivante ne compose RIEN. Une
  // seule ne veut rien dire — un nœud naît sans aire ; c'est la SUITE qui
  // accuse, et le compte se remet à zéro dès qu'une figure sort.
  std::uint32_t silent_frames_ = 0;
  // Les deux comptes du dernier `Drawn`, gardés pour qui veut savoir pourquoi
  // rien n'est sorti. Voir `nodes_without_texture`.
  std::size_t sans_texture_ = 0;
  std::size_t sans_quad_ = 0;
  // Combien de lots de quads ont été composés DEPUIS LE LANCEMENT. Jamais remis
  // à zéro : c'est ce qui distingue un effet qui ne se montre pas d'une voie de
  // rendu qui n'a jamais rien montré.
  std::size_t lots_composes_ = 0;

  // LES DÉFAUTS DU CLIENT : les effets s'affichent, l'allègement est coupé.
  bool effects_shown_ = true;
  bool minimal_effects_ = false;
  std::vector<EffectClock> clocks_;
  // Voir `DieWithBearerAtNext`. Zéro : l'instance vit sa vie.
  std::int32_t next_bearer_death_tick_ = 0;
  // LES SEMIS QUI ONT SONNÉ depuis le dernier `TakeSounds` : l'effet, et le
  // point de l'instance qui a semé. Voir `EffectNodePattern::sound_at_sowing`.
  struct SowingSound {
    std::uint32_t effect_id = 0;
    glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  };
  std::vector<SowingSound> sowing_sounds_;
  // Les effets que les nœuds ont demandés : voir `TakeSpawnRequests`.
  std::vector<ro::render::EffectSpawnRequest> spawn_requests_;

  // UN PAS DES HORLOGES : chaque effet en cours sème ce que son âge réclame,
  // puis vieillit. `single_frame` pour une image qui ne doit aucun tic.
  void StepClocks(bool single_frame);
};

}  // namespace moonclient
