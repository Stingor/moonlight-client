#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_scene.h"
#include "ro/render/effect_screen_quad.h"
#include "ro/render/gl_program.h"
#include "ro/render/gl_texture.h"
#include "ro/render/scene_fog.h"

namespace ro::render {

// De combien l'ancre d'un effet se tient AU-DESSUS du point où il est joué, en
// pixels d'effet.
//
// Le repère d'un fichier n'a pas son ORIGINE au sol : c'est le centre d'un écran
// de six cent quarante sur quatre cent quatre-vingts (voir kStrOriginY), et le
// sol tombe plus bas. Le client relève donc l'ancre, de quatre-vingts pixels par
// défaut — la valeur que la mise à jour d'un fichier d'effet compare pour
// savoir si un effet en demande une autre.
//
// La donnée le confirme : safeline.bmp est le cercle
// que les sorts au sol posent sous les pieds, et les fichiers le dessinent à
// l'ordonnée 310 pour SafetyWall, 315 pour les trois Pneuma. Retranchée
// l'origine, il tombe à cinq ou dix pixels au-dessus du point de jeu. Sans ce
// relèvement, un dôme de Pneuma reste aux pieds au lieu de couvrir la tête.
inline constexpr float kEffectAnchorLiftPixels = 80.0F;

// Un effet en train de jouer, à sa place dans le monde.
struct DrawnEffect {
  const LoadedEffect* effect = nullptr;
  // Le point du sol d'où l'effet part : l'origine de ses coordonnées, comme le
  // point d'appui d'un acteur.
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  std::int32_t frame = 0;
  // DE COMBIEN CET EFFET-CI SE RELÈVE, en pixels d'effet.
  //
  // 🔴 CE N'EST PAS LA MÊME POUR TOUS : la mise à jour d'un fichier d'effet
  // reçoit le décalage de son APPELANT et le compare à −80 pour savoir si
  // l'effet en demande un autre. Quatre-vingts est le DÉFAUT, pas la règle.
  //
  // ⚠ UN EFFET QUI TOUCHE LE SOL EN DEMANDE ZÉRO. La flaque d'un pilier de
  // butin est dessinée à l'ordonnée 240 de son fichier — `kStrOriginY`, donc
  // l'origine même —, si bien que le relèvement ordinaire la ferait flotter à
  // quatre-vingts pixels au-dessus du terrain.
  float lift_pixels = kEffectAnchorLiftPixels;
  // IL SE PEINT AVANT LES CORPS, et non par-dessus.
  //
  // ⚠ LES EFFETS PASSENT DEVANT LES ACTEURS PAR DÉFAUT, et c'est voulu : ils
  // ÉCLAIRCISSENT ce qui est déjà peint, et n'auraient rien à éclaircir plus
  // tôt. Mais une colonne qui SIGNALE quelque chose ne doit pas le noyer : le
  // pilier d'un butin rare recouvrait le butin qu'il montre.
  bool behind_actors = false;
  // ⚠ EN QUEUE, comme tout ce qui s'ajoute : des appelants bâtissent cette
  // structure par liste positionnelle.
  //
  // CE QU'UNE MÉTHODE NOMMÉE FAIT DE L'IMAGE, autour du point du SOL et non de
  // l'ancre relevée : l'échelle, puis les retournements, puis la rotation. La
  // rotation, en degrés dans le sens des aiguilles d'une montre à l'écran, ne
  // s'applique que STRICTEMENT POSITIVE.
  float spin_degrees = 0.0F;
  float scale = 1.0F;
  bool flip_x = false;
  bool flip_y = false;
  // LE MULTIPLICATEUR D'ALPHA que le lecteur applique à chaque couche : l'alpha
  // peint est l'entier de ce facteur fois celui de la couche. Un pour tout effet
  // que rien n'éteint.
  float alpha = 1.0F;
  // LA PART DE L'IMAGE SUIVANTE DÉJÀ ÉCOULÉE, sous l'interpolation d'images :
  // les couches s'y déforment entre deux images. Zéro peint `frame` tel quel.
  // Voir `EffectLayerAt`.
  float frame_fraction = 0.0F;
};

// Un lot de quads d'effet DÉJÀ POSÉS DANS LE MONDE, et la texture qui les
// peint.
//
// C'est ce que rend un dessinateur d'anneau : des points du monde, et non des
// pixels autour d'une ancre. Ils ne passent donc pas par le panneau face à la
// caméra — leur géométrie est déjà complète.
struct DrawnWorldQuads {
  std::span<const EffectWorldQuad> quads;
  // Le chemin dans l'archive IDENTIFIE la texture, comme pour un effet en
  // fichier : deux anneaux qui partagent un halo partagent son téléversement.
  const std::string* texture_path = nullptr;
  const formats::Image* texture = nullptr;
  // D'où le lot est joué. Ne sert qu'à le TRIER parmi les autres : sa
  // géométrie, elle, ne s'y rapporte pas.
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  // LE MOT DE RENDU du nœud qui a composé le lot : il décide du
  // mélange et de l'épreuve de profondeur. Vide, le lot garde le mélange de
  // chaque quad et passe l'épreuve. Voir `effect_render_word.h`.
  std::optional<std::uint32_t> render_word;
};

// UN LOT DE QUADS D'ÉCRAN, et l'image qu'ils partagent.
//
// La QUATRIÈME voie du client (ADR 0022) : ses sommets sortent déjà
// transformés, en pixels, et rien ne les projette. Elle passe pourtant par CE
// renderer et non par celui des superpositions, pour la raison qui a déjà mis
// les quads du monde ici : le CACHE DE TEXTURES. Un effet d'écran puise dans le
// même dossier de l'archive qu'un effet en fichier, et deux caches
// téléverseraient deux fois le même halo.
struct DrawnScreenQuads {
  std::span<const EffectScreenQuad> quads;
  const std::string* texture_path = nullptr;
  const formats::Image* texture = nullptr;
};

// Dessine les effets animés : en panneaux face à la caméra pour ceux qui
// viennent d'un fichier, en géométrie du monde pour les nœuds qui en bâtissent,
// en pixels pour ceux de la voie d'écran.
//
// Séparé du renderer de sprites, et pas par commodité : un effet ne se mélange
// pas comme un sprite. Chaque quad porte SES deux facteurs de mélange, tirés du
// fichier — le jeu en emploie une dizaine de couples différents —, là où un
// sprite se recouvre toujours de la même façon. Les mêler obligerait le
// renderer de sprites à changer d'état à chaque quad pour un cas qui ne le
// concerne pas.
//
// LES DEUX FAMILLES PARTAGENT CE RENDERER, et c'est le CACHE DE TEXTURES qui
// l'exige : un nœud d'anneau et un fichier puisent dans le même dossier de
// l'archive, et deux caches téléverseraient deux fois le même halo.
//
// ⚠ À dessiner APRÈS les acteurs. Un effet est presque toujours ADDITIF : il
// éclaircit ce qui est déjà peint, et n'a donc rien à éclaircir tant que le
// reste de la scène ne l'est pas.
class EffectRenderer {
 public:
  // Défini dans le fichier source, où les types des tampons de dessin sont
  // complets.
  EffectRenderer();
  EffectRenderer(const EffectRenderer&) = delete;
  EffectRenderer& operator=(const EffectRenderer&) = delete;
  EffectRenderer(EffectRenderer&& other) noexcept;
  EffectRenderer& operator=(EffectRenderer&& other) noexcept;
  ~EffectRenderer();

  [[nodiscard]] static core::Result<EffectRenderer> Create();

  // Dessine les effets donnés, du plus LOINTAIN au plus proche.
  //
  // L'écriture de profondeur est coupée : les couches d'un même effet se
  // recouvrent dans l'ordre où le fichier les range, et c'est le fichier qui
  // décide de ce qui passe devant.
  void Draw(const Camera& camera, std::span<const DrawnEffect> effects, float aspect_ratio);

  // Dessine des quads déjà posés dans le monde, du plus LOINTAIN au plus proche.
  //
  // Rien n'y est projeté ni relevé : un dessinateur d'anneau a déjà tout fait.
  // Le reste — le cache de textures, le mélange par quad, la profondeur qu'on
  // n'éprouve pas — est celui de `Draw`, et pour les mêmes raisons.
  //
  // `fog` ne prend que les lots dont le mot de rendu se vide sous le
  // brouillard : voir `EffectLotIsFogged`. Un effet en fichier et la voie
  // d'écran n'en reçoivent jamais.
  void DrawWorldQuads(const Camera& camera, std::span<const DrawnWorldQuads> lots,
                      float aspect_ratio, const SceneFog& fog = SceneFog());

  // Dessine des quads déjà posés EN PIXELS, dans l'ordre donné.
  //
  // ⚠ AUCUNE CAMÉRA ICI, et ce n'est pas un oubli : ces sommets sont sortis du
  // client déjà transformés, avec une profondeur d'un millionième écrite en dur.
  // Les projeter une seconde fois les ferait disparaître. La seule matrice est
  // une orthographique dont l'origine est le coin HAUT-GAUCHE et dont
  // l'ordonnée DESCEND — le repère de la souris, celui dans lequel l'appelant a
  // déjà tout calculé.
  //
  // ⚠ ET LE MODE DE MÉLANGE DE CETTE VOIE N'EST PAS ENCORE TRADUIT. Le client
  // passe UN nombre à sa file de rendu, qui le décompose ailleurs, et cette
  // décomposition-là n'est pas portée. Ces quads se peignent donc au mélange ordinaire — source par
  // son alpha —, et le jour où le nombre sera lu, c'est ici qu'il se traduira.
  void DrawScreenQuads(std::span<const DrawnScreenQuads> lots, float viewport_width,
                       float viewport_height);

  // Oublie les textures dont plus aucun effet joué ne se sert.
  void ForgetUnusedTextures();
  // OUBLIE TOUTES LES TEXTURES, rangées par chemin : le skin a changé, et un
  // même chemin désigne une autre image. ⚠ ENTRE DEUX IMAGES.
  void ForgetTextures() { textures_.clear(); }

  [[nodiscard]] std::size_t cached_texture_count() const noexcept { return textures_.size(); }

 private:
  // Si les quads préparés s'éprouvent contre la profondeur déjà écrite. Voir
  // `PaintQuads`.
  enum class DepthTest : std::uint8_t { kOff, kOn };
  struct PreparedQuad;
  struct EffectVertex;

  void Release() noexcept;
  // Rend la texture d'un chemin, en la téléversant à la première demande.
  [[nodiscard]] const GlTexture* TextureFor(const std::string& path, const formats::Image& image);
  // Téléverse `vertices_` et peint `prepared_`, un lot de mélange à la fois.
  void PaintQuads(const glm::mat4& view_projection, DepthTest depth_test, const SceneFog& fog);

  struct CachedTexture {
    GlTexture texture;
    // Vrai si un effet l'a réclamée depuis le dernier oubli.
    bool used = false;
  };

  GlProgram program_;
  std::map<std::string, CachedTexture, std::less<>> textures_;
  unsigned int vertex_array_ = 0;
  unsigned int vertex_buffer_ = 0;
  // LES TAMPONS D'UN DESSIN, gardés d'un appel à l'autre : chaque appel les
  // vide sans rendre leur mémoire, si bien qu'une scène qui ne grandit pas ne
  // fait plus aucune allocation.
  std::vector<const DrawnEffect*> ordered_effects_;
  std::vector<const DrawnWorldQuads*> ordered_lots_;
  std::vector<EffectQuad> effect_quads_;
  std::vector<EffectPaintedQuad> painted_;
  std::vector<const GlTexture*> lot_textures_;
  std::vector<PreparedQuad> prepared_;
  std::vector<EffectVertex> vertices_;
};

}  // namespace ro::render
