#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/camera.h"
#include "ro/render/gl_program.h"
#include "ro/render/gl_texture.h"
#include "ro/render/scene_fog.h"
#include "ro/render/sprite_billboard.h"

namespace ro::render {

// La plus petite emprise cliquable d'un acteur quand rien ne la règle, en
// pixels : celle du client dans une fenêtre de 640 de large.
inline constexpr float kDefaultPickFloorPixels = 40.0F;

// Un acteur composé, à sa place dans le monde.
struct DrawnActor {
  const ComposedActor* actor = nullptr;
  // L'ombre au sol, peinte AVANT l'acteur et au même endroit. Nulle ou vide
  // pour qui n'en porte pas. Elle voyage avec lui plutôt que comme un acteur de
  // plus : le tri en profondeur les séparerait, et l'ombre passerait par-dessus
  // son porteur une image sur deux.
  //
  // Elle se peint aussi SANS acteur : un joueur caché ne laisse que son ombre,
  // rangée avec les corps pour que l'eau la couvre comme les leurs.
  const ComposedActor* shadow = nullptr;
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  // Une teinte pour l'acteur entier, par-dessus celle de ses calques ; blanc
  // pour aucune. C'est ainsi que le jeu grise un pétrifié et bleuit un gelé.
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  // De combien l'acteur RECULE quand plusieurs se disputent le pixel cliqué,
  // en unités du monde. Zéro pour ne compter que la distance : voir PickActor.
  float pick_bias = 0.0F;
  // De combien l'acteur ENTIER est grandi à cette image, son ombre comprise.
  //
  // Elle est ici, et non dans la composition, parce qu'elle change d'une image
  // à l'autre : un corps qui rapetisse le fait par une courbe, alors que ses
  // calques, eux, ne changent qu'à l'image d'animation suivante. Le client
  // multiplie de même l'ombre par l'échelle du sprite de son porteur.
  float scale = 1.0F;
  // DE COMBIEN L'ACTEUR EST DÉCALÉ À L'ÉCRAN, en pixels de sprite : x vers la
  // droite, y vers le haut.
  //
  // Le client travaille en pixels d'écran pour tous ses décalages d'effet, et
  // ceux-là ne se disent pas en coordonnées du monde : un corps qui tremble
  // tremble LATÉRALEMENT, dans le repère du panneau, pas le long d'un axe de la
  // carte. Le décalage s'ajoute donc aux coins du quad, là où le panneau est
  // déjà exprimé en pixels de sprite.
  //
  // ⚠ IL NE SE MULTIPLIE PAS PAR L'ÉCHELLE. Le client l'applique après avoir
  // grandi ses quads : un corps géant qui tremble tremble d'autant de pixels
  // qu'un corps ordinaire.
  glm::vec2 screen_offset_pixels{0.0F, 0.0F};
  // DE COMBIEN L'ACTEUR EST ÉCRASÉ EN HAUTEUR, en plus de l'échelle. Un pour un
  // corps debout, quatre dixièmes pour un corps aplati.
  //
  // ⚠ ELLE EST ANCRÉE AUX PIEDS, sans réglage : les coins d'un panneau sont
  // comptés depuis le sol où l'acteur se tient, si bien que multiplier leur
  // ordonnée écrase vers le bas et non vers le milieu.
  float height_scale = 1.0F;
  // DE COMBIEN CHAQUE CALQUE S'ÉLARGIT, en pixels d'ÉCRAN, sur chacun de ses
  // quatre côtés — après l'échelle, comme le client l'ajoute au rectangle déjà
  // projeté. Zéro pour tout acteur ; le halo d'un sprite éclatant en porte un.
  float grow_screen_pixels = 0.0F;
  // UN ACTEUR EN MODÈLE n'a pas de quad : son emprise est un CARRÉ d'écran
  // autour du point `model_pick_center`, d'un demi-côté de `√(côté² / 2)` unités
  // de la vue, où `model_pick_side` est ce côté. Zéro pour tout autre acteur.
  // Voir `ro/render/actor_model_scene.h`.
  float model_pick_side = 0.0F;
  glm::vec3 model_pick_center{0.0F, 0.0F, 0.0F};
  // UN ACTEUR POSÉ AU SOL — un warp — n'a ni quad ni modèle : son emprise est
  // le DISQUE horizontal de ce rayon autour de `ground_position`, tel que la
  // caméra le voit. Zéro pour tout autre acteur. Voir `PickActor`.
  //
  // ⚠ CE DISQUE NE SERT QU'À DÉSIGNER. `ActorScreenBounds` n'en rend rien : ce
  // qu'on ancre à un acteur — jauge, nom, bulle — n'a rien à faire sur un warp.
  float ground_pick_radius = 0.0F;
  // LE BROUILLARD LE PREND-IL, quand le dessin en porte un ? Oui pour un corps
  // ordinaire ; non pour ce que le client range à part — un corps additif, les
  // halos et les voiles qu'un effet pose sur lui, un sprite d'effet dont le mot
  // de rendu l'en sort.
  bool fogged = true;
  // LES CALQUES QUI S'AJOUTENT au décor au lieu de le recouvrir, par partie.
  //
  // ⚠ LE MÉLANGE SE CHOISIT PAR CALQUE, pas par acteur : la lame d'aura ne rend
  // additive que l'ARME, la rage que le corps, la tête, les coiffes et la cape.
  // Un calque additif se peint APRÈS tous les calques qui recouvrent, hors du
  // brouillard, comme le client range ses primitives additives dans une file
  // qu'il vide après les autres et sans brouillard.
  ActorPartSet additive_parts = 0;
  // LES CALQUES QUI SE PEIGNENT, par partie. Tous pour un acteur ; une copie que
  // le client ne pose que sur certains calques en porte moins.
  ActorPartSet shown_parts = kAllActorParts;
  // L'ARME SE PEINT UNE SECONDE FOIS, en addition, à cette image.
  bool doubles_weapon = false;
  // DE COMBIEN L'ACTEUR S'AVANCE VERS LA CAMÉRA pour l'épreuve de profondeur,
  // ajouté à l'INVERSE de sa distance à l'œil : un sprite d'effet posé au même
  // endroit qu'un corps passe ainsi devant lui. Zéro pour tout acteur ordinaire.
  //
  // ⚠ C'EST L'INVERSE DE LA DISTANCE QUI BOUGE, pas la distance : un même biais
  // avance de peu un acteur proche et de beaucoup un acteur lointain.
  float depth_bias = 0.0F;
  // LA PLUS PETITE EMPRISE CLIQUABLE de cet acteur, en pixels : un rectangle
  // plus étroit s'élargit de part et d'autre jusque-là. Le client la tire de la
  // largeur de la fenêtre et de la famille de l'acteur ; voir `PickFloorPixels`.
  float pick_floor_pixels = kDefaultPickFloorPixels;
  // LA PART DE SON EMPRISE ÉLARGIE qu'il garde autour de son centre. Un pour
  // tout acteur ; moins pour celui que le serveur ne peut pas viser.
  float pick_share = 1.0F;
};

// Dessine les acteurs, en panneaux face à la caméra.
//
// Séparé des renderers de terrain et de décor : un sprite n'a ni normale ni
// lightmap, il se compose par recouvrement alpha et son maillage change à
// chaque image. Les mêler obligerait chacun à porter les cas de l'autre.
//
// ⚠ À dessiner APRÈS le terrain et le décor. Le mélange alpha n'est correct que
// si ce qui se trouve derrière est déjà peint, et l'écriture de profondeur est
// coupée pour que les calques d'un même personnage se recouvrent dans l'ordre
// où ils sont donnés plutôt que par leur distance.
class SpriteRenderer {
 public:
  // Défini dans le fichier source, où les types des tampons de dessin sont
  // complets.
  SpriteRenderer();
  SpriteRenderer(const SpriteRenderer&) = delete;
  SpriteRenderer& operator=(const SpriteRenderer&) = delete;
  SpriteRenderer(SpriteRenderer&& other) noexcept;
  SpriteRenderer& operator=(SpriteRenderer&& other) noexcept;
  ~SpriteRenderer();

  [[nodiscard]] static core::Result<SpriteRenderer> Create();

  // COMMENT les quads se posent sur ce qui est déjà peint.
  //
  // Un acteur RECOUVRE : son sprite porte un fond parfaitement transparent, et
  // le mélange ordinaire suffit à le découper.
  //
  // Un effet ADDITIONNE, et ce n'est pas une préférence d'aspect : le fond de
  // ses particules n'est pas transparent mais NOIR À FAIBLE ALPHA — mesuré à 15
  // et 30 sur 255 dans `이팩트\particle1`, qui est une image BGRA sans couleur
  // clé. Recouvrir avec ce fond-là assombrit le sol de plus d'un dixième, et
  // dessine le rectangle du sprite autour de chaque particule ; l'addition,
  // elle, ne fait rien d'un noir.
  enum class Blending : std::uint8_t {
    kOver,
    kAdditive,
  };

  // QUI ÉCRIT SA PROFONDEUR, et pourquoi ce n'est pas tout le monde.
  //
  // ⚠ ELLE S'ÉCRIT EN UNE PASSE À PART, SANS COULEUR. Les calques d'un acteur
  // n'ont PAS la même profondeur — celle d'un calque vient d'un panneau DEBOUT
  // à sa place —, si bien qu'écrire en peignant fait rejeter les calques plus
  // lointains : le corps passait par-dessus la tête. Voir la mise en œuvre.
  //
  // ⚠ UN ACTEUR DU JEU DOIT L'ÉCRIRE, sans quoi rien ne le protège de ce qui
  // se peint après lui. Les vingt-cinq pointes de terre le recouvraient
  // entièrement, là où le client le montre ENTRE elles : lui n'a pas de
  // couches, il range toutes ses primitives mélangées dans une file triée par
  // profondeur. Nous avons ce qu'il n'avait pas — un vrai tampon —, et l'y
  // faire participer revient au même.
  //
  // ⚠ UN SPRITE D'EFFET NE DOIT PAS. Son ancre est en l'air, au centre d'un
  // écran de 640 sur 480, et la moitié de son contenu est sous elle : il se
  // couperait dans le sol. C'est la raison pour laquelle le renderer d'effets
  // n'éprouve pas la profondeur, et elle vaut aussi pour l'écriture.
  enum class DepthWrite : std::uint8_t {
    kOff,
    kOn,
  };

  // SI CE QUI SE DESSINE SE LAISSE CACHER PAR LA SCÈNE, ou passe par-dessus.
  //
  // ⚠ CE QUI SE LIT PAR-DESSUS LE MONDE N'EN FAIT PAS PARTIE. Les nombres de
  // dégâts et les bulles d'humeur se posent à l'aplomb d'un acteur, donc à SA
  // profondeur — et depuis que l'acteur ÉCRIT la sienne, sa silhouette les
  // découpait : on lisait « 11 » et « 10 » de part et d'autre du personnage, les
  // chiffres du milieu mangés. Ils ne sont pas des objets de la scène mais ce
  // qu'elle dit ; le client les vide lui aussi en dernier, sans épreuve.
  //
  // L'épreuve reste le défaut : un acteur, lui, doit se laisser cacher par le
  // décor.
  enum class DepthTest : std::uint8_t {
    kOn,
    kOff,
  };

  // DANS QUEL ORDRE ils se peignent, et donc qui recouvre qui.
  //
  // ⚠ LE PLUS LOINTAIN D'ABORD est le défaut, et il est indispensable aux
  // acteurs : l'écriture de profondeur étant coupée, c'est l'ordre seul qui
  // décide. Sans lui, un personnage au fond peut effacer celui du premier plan.
  //
  // ⚠ TELS QUE DONNÉS pour ce que le client pose à une profondeur UNIQUE,
  // devant la scène — les nombres de dégâts et leurs mots : il n'y a alors
  // rien à trier, et l'appelant donne l'ordre, le dernier par-dessus.
  enum class DrawOrder : std::uint8_t {
    kFarthestFirst,
    kAsGiven,
  };

  // LA SILHOUETTE DES CORPS, tenue dans le tampon de stencil le temps d'une
  // image.
  //
  // L'eau se peint APRÈS les corps et leurs ombres, et c'est ce qui les
  // plonge : sa nappe passe devant tout ce qui est sous sa surface. Mais les
  // halos et les rémanences, qui passent DERRIÈRE les corps, ne se peignent plus
  // avant eux, sans quoi l'eau les recouvrirait. Ils se peignent après la nappe
  // et évitent la silhouette : un halo reste sous tout corps opaque, comme
  // lorsqu'il passait le premier.
  //
  //   * `kLeft` : ce dont l'appel écrit la profondeur laisse sa marque — les
  //     corps, leurs ombres, les charrettes opaques. Sans `DepthWrite::kOn`, il
  //     n'en laisse aucune ;
  //   * `kAvoided` : l'appel ne peint nulle part où elle est posée ;
  //   * `kIgnored` : ni l'un ni l'autre, le défaut.
  //
  // ⚠ LA MARQUE SE POSE AVEC LA PROFONDEUR, pas à la peinture. Le jeu peint ses
  // halos après ses corps, éprouvés contre la profondeur qu'ils ont écrite : un
  // pixel de corps trop transparent pour l'écrire ne les arrête pas.
  enum class BodySilhouette : std::uint8_t {
    kIgnored,
    kLeft,
    kAvoided,
  };

  // Dessine les acteurs donnés, dans l'ordre choisi.
  //
  // Les textures sont mises en cache par identité d'image : deux personnages
  // qui portent la même tête à la même pose n'en téléversent qu'une.
  //
  // `fog` n'embrume que les acteurs dont `DrawnActor::fogged` est vrai.
  void Draw(const Camera& camera, std::span<const DrawnActor> actors, float aspect_ratio,
            Blending blending = Blending::kOver, DepthWrite depth_write = DepthWrite::kOff,
            DepthTest depth_test = DepthTest::kOn, const SceneFog& fog = SceneFog(),
            DrawOrder order = DrawOrder::kFarthestFirst,
            BodySilhouette silhouette = BodySilhouette::kIgnored);

  // Oublie les textures dont plus aucun acteur ne se sert.
  //
  // À appeler quand la composition de la scène change franchement — un
  // changement de carte, un joueur qui se déconnecte. Le cache ne se purge pas
  // tout seul : il ne sait pas ce que l'appelant dessinera à la frame suivante.
  void ForgetUnusedTextures();

  [[nodiscard]] std::size_t cached_texture_count() const noexcept { return textures_.size(); }

 private:
  void Release() noexcept;
  // Rend la texture d'une image, en la téléversant à la première demande.
  [[nodiscard]] const GlTexture* TextureFor(const SpriteImageKey& key);

  struct CachedTexture {
    GlTexture texture;
    // Vrai si un acteur l'a réclamée depuis le dernier oubli.
    bool used = false;
  };

  struct PreparedQuad;
  struct SpriteVertex;

  GlProgram program_;
  std::map<SpriteImageKey, CachedTexture> textures_;
  unsigned int vertex_array_ = 0;
  unsigned int vertex_buffer_ = 0;
  // LES TAMPONS D'UN DESSIN, gardés d'un appel à l'autre : chaque `Draw` les
  // vide sans rendre leur mémoire, si bien qu'une scène qui ne grandit pas ne
  // fait plus aucune allocation.
  std::vector<const DrawnActor*> ordered_;
  std::vector<PreparedQuad> prepared_;
  std::vector<SpriteVertex> vertices_;
  std::vector<LayerDraw> layer_draws_;
};

}  // namespace ro::render
