#include "ro/render/effect_renderer.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>

#include "ro/render/effect_laid.h"
#include "ro/render/effect_quad_depth.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_scene.h"
#include "ro/render/effect_texture_touchup.h"
#include "ro/render/fog_shader.h"
#include "ro/render/sprite_billboard.h"

namespace ro::render {
namespace {

// Les facteurs de mélange de Direct3D, dans l'ordre de D3DBLEND. Le fichier les
// porte tels quels ; la traduction est ici, et nulle part ailleurs.
constexpr std::int32_t kBlendZero = 1;
constexpr std::int32_t kBlendOne = 2;
constexpr std::int32_t kBlendSourceColor = 3;
constexpr std::int32_t kBlendInverseSourceColor = 4;
constexpr std::int32_t kBlendSourceAlpha = 5;
constexpr std::int32_t kBlendInverseSourceAlpha = 6;
constexpr std::int32_t kBlendDestinationAlpha = 7;
constexpr std::int32_t kBlendInverseDestinationAlpha = 8;
constexpr std::int32_t kBlendDestinationColor = 9;
constexpr std::int32_t kBlendInverseDestinationColor = 10;
constexpr std::int32_t kBlendSourceAlphaSaturate = 11;

// Le facteur OpenGL d'un facteur Direct3D.
//
// ⚠ L'ALPHA DE LA DESTINATION vaut UN, et son inverse ZÉRO. Ce n'est pas une
// approximation : la scène est peinte sur une cible OPAQUE, et c'est la même
// condition qui fait du couple dominant des fichiers — source par son alpha,
// destination par l'alpha du fond — une addition, c'est-à-dire le halo des
// effets du jeu. Poser les constantes plutôt que GL_DST_ALPHA rend le résultat
// indépendant de l'existence d'un canal alpha dans le tampon d'image, et de ce
// que les acteurs y ont écrit avant nous.
GLenum BlendFactorOf(std::int32_t blend, GLenum fallback) {
  switch (blend) {
    case kBlendZero:
      return GL_ZERO;
    case kBlendOne:
      return GL_ONE;
    case kBlendSourceColor:
      return GL_SRC_COLOR;
    case kBlendInverseSourceColor:
      return GL_ONE_MINUS_SRC_COLOR;
    case kBlendSourceAlpha:
      return GL_SRC_ALPHA;
    case kBlendInverseSourceAlpha:
      return GL_ONE_MINUS_SRC_ALPHA;
    case kBlendDestinationAlpha:
      return GL_ONE;
    case kBlendInverseDestinationAlpha:
      return GL_ZERO;
    case kBlendDestinationColor:
      return GL_DST_COLOR;
    case kBlendInverseDestinationColor:
      return GL_ONE_MINUS_DST_COLOR;
    case kBlendSourceAlphaSaturate:
      return GL_SRC_ALPHA_SATURATE;
    default:
      // Zéro n'est pas un facteur de Direct3D : c'est ce que laisse une clé
      // d'interpolation, et le fichier ne dit alors rien du mélange.
      return fallback;
  }
}

// Le même panneau que pour un sprite : la FORME face à l'écran, la PROFONDEUR
// d'un panneau debout au même endroit. Voir sprite_billboard.h.
constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texture_uv;
layout(location = 2) in vec4 a_tint;
layout(location = 3) in vec3 a_depth_position;
// Un pour un quad dont le seau se vide sous le brouillard, zéro sinon.
layout(location = 4) in float a_fogged;
// Un pour un quad APLATI au premier plan : sa texture se pose sans correction
// de perspective, comme sur un sommet dont la profondeur est la même partout.
layout(location = 5) in float a_affine;

uniform mat4 u_view_projection;

out vec2 v_texture_uv;
noperspective out vec2 v_texture_uv_affine;
flat out float v_affine;
out vec4 v_tint;
out float v_fog_kept;

void main() {
    v_texture_uv = a_texture_uv;
    v_texture_uv_affine = a_texture_uv;
    v_affine = a_affine;
    v_tint = a_tint;
    vec4 drawn = u_view_projection * vec4(a_position, 1.0);
    vec4 upright = u_view_projection * vec4(a_depth_position, 1.0);
    float z = upright.w > 0.0001 ? upright.z * drawn.w / upright.w : drawn.z;
    gl_Position = vec4(drawn.xy, z, drawn.w);
    v_fog_kept = mix(1.0, FogKeptPart(drawn.w), a_fogged);
}
)GLSL";

// LES TEXELS VIDES SE REJETTENT, et c'est le client qui l'exige.
//
// On a cru pouvoir s'en passer : un effet ADDITIF n'écrit rien là où sa texture
// est noire, et le raisonnement tenait tant que toutes les couches étaient
// additives. Elles ne le sont pas. Une couche d'Angelus.str se mélange « un
// vers zéro » — elle REMPLACE ce qui est dessous, et le mélange ignore alors
// l'alpha : le fond à couleur clé de la texture se peint en plein, un
// rectangle opaque au-dessus du personnage.
//
// Le client passe ses primitives mélangées avec un TEST D'ALPHA : seuil un,
// comparaison « supérieur ou égal », posé autour de tout le lot. Le test porte
// sur l'alpha FINAL, texture et teinte multipliées : un
// effet qui s'éteint disparaît par là aussi.
constexpr std::string_view kFragmentShader = R"GLSL(
#version 330 core
in vec2 v_texture_uv;
noperspective in vec2 v_texture_uv_affine;
flat in float v_affine;
in vec4 v_tint;
in float v_fog_kept;

uniform sampler2D u_texture;

out vec4 frag_color;

// Le seuil du client : un sur deux cent cinquante-cinq.
const float kAlphaReference = 1.0 / 255.0;

void main() {
    vec2 uv = v_affine > 0.5 ? v_texture_uv_affine : v_texture_uv;
    vec4 color = texture(u_texture, uv) * v_tint;
    if (color.a < kAlphaReference) {
        discard;
    }
    frag_color = vec4(Fogged(color.rgb, v_fog_kept), color.a);
}
)GLSL";

constexpr std::size_t kVerticesPerQuad = 6;
// L'alpha le plus haut qu'un sommet porte.
constexpr std::uint8_t kOpaqueAlpha = 255;

// Les six sommets d'un quad, pris dans ses quatre coins : deux triangles qui
// partagent la diagonale du premier au troisième.
constexpr std::array<std::size_t, kVerticesPerQuad> kQuadVertexOrder = {0, 1, 2, 0, 2, 3};

float SquaredDistance(const glm::vec3& left, const glm::vec3& right) {
  const glm::vec3 delta = left - right;
  return glm::dot(delta, delta);
}

}  // namespace

struct EffectRenderer::EffectVertex {
  glm::vec3 position;
  glm::vec2 texture_uv;
  std::array<std::uint8_t, 4> tint;
  glm::vec3 depth_position;
  float fogged;
  // Voir `a_affine`.
  float affine = 0.0F;
};

// Un quad prêt à dessiner : sa géométrie, sa texture, et son mélange à lui.
struct EffectRenderer::PreparedQuad {
  const GlTexture* texture = nullptr;
  GLenum blend_source = GL_SRC_ALPHA;
  GLenum blend_destination = GL_ONE;
};

// Commun aux trois entrées, et il faut qu'il le soit : ce qui suit — la
// profondeur qu'on n'éprouve pas, le mélange par quad, la remise en état — est
// une décision, et la dédoubler la ferait diverger au premier changement.
void EffectRenderer::PaintQuads(const glm::mat4& view_projection, DepthTest depth_test,
                                const SceneFog& fog) {
  if (prepared_.empty()) {
    return;
  }

  glBindVertexArray(vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(EffectVertex)),
               vertices_.data(), GL_STREAM_DRAW);

  // ⚠ SANS ÉPREUVE DE PROFONDEUR, et c'est une décision, pas un oubli.
  //
  // Un effet est un panneau face à l'écran, à la profondeur de son ancre. La
  // caméra du jeu PLONGE : tout ce que l'écran montre sous cette ancre est du
  // terrain PLUS PROCHE que le panneau, et le terrain gagne. La moitié basse
  // d'un effet disparaît donc dans le sol, et l'on ne voit l'effet entier qu'à
  // la verticale, quand le panneau devient parallèle au sol. C'est
  // géométriquement inévitable tant qu'on éprouve la profondeur, quelle que
  // soit la profondeur qu'on donne aux coins.
  //
  // Un ACTEUR y échappe parce que son origine est à ses PIEDS : rien de lui ne
  // pend sous le sol, et ce qui pend s'y couche (BillboardDepthCorners).
  // L'ancre d'un effet est en l'air, au centre d'un écran de 640 sur 480, et la
  // moitié de son contenu est authored sous elle.
  //
  // Le client ne rencontre pas le problème parce qu'il n'empile pas des
  // triangles du monde : il pose des quads en coordonnées d'écran dans une file
  // triée, une profondeur par PRIMITIVE, et les mélangés passent en dernier.
  // Un effet y est donc entier
  // ou absent, jamais coupé. On obtient le même résultat en le peignant
  // par-dessus : les effets sont déjà dessinés après le terrain, le décor et
  // les acteurs.
  //
  // Ce qu'on y perd : un effet derrière un mur se voit à travers. Le jeu le
  // laisse aussi.
  //
  // ⚠ UN QUAD DU MONDE ÉPROUVE LA PROFONDEUR, un panneau non. Le raisonnement
  // ci-dessus vaut pour les PANNEAUX : leur ancre est en l'air et leur moitié
  // basse se couperait dans le sol. La géométrie d'un quad du monde, elle, est
  // réelle — et sans l'épreuve, les vingt-cinq pointes de terre recouvraient
  // entièrement le personnage, là où le client le montre ENTRE elles.
  //
  // ⚠ CE QUE CELA ROUVRE, et c'est dit : un anneau et le fichier qui
  // l'accompagne appartiennent au même effet, et ne sont plus éprouvés
  // pareil. S'ils se croisent, ils se tranchent. Le défaut mesuré l'emporte
  // sur le défaut redouté.
  //
  // L'écriture reste COUPÉE : ces quads sont mélangés, et deux d'entre eux qui
  // se croisent doivent se voir l'un l'autre. C'est le tri par profondeur qui
  // les ordonne entre eux — voir `EffectWorldQuadsBackToFront`.
  if (depth_test == DepthTest::kOn) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);

  program_.Use();
  program_.SetMatrix4("u_view_projection", view_projection);
  program_.SetInt("u_texture", 0);
  SetFogUniforms(program_, fog);

  // ⚠ LES QUADS VOISINS D'UNE MÊME TEXTURE ET D'UN MÊME MÉLANGE PARTENT
  // ENSEMBLE : l'ordre de dessin ne change pas, seul le nombre d'appels baisse.
  // Les particules d'un même émetteur en sont le cas type.
  const auto same_state = [this](std::size_t left, std::size_t right) {
    return prepared_[left].texture == prepared_[right].texture &&
           prepared_[left].blend_source == prepared_[right].blend_source &&
           prepared_[left].blend_destination == prepared_[right].blend_destination;
  };
  for (std::size_t first = 0; first < prepared_.size();) {
    std::size_t end = first + 1;
    while (end < prepared_.size() && same_state(first, end)) {
      ++end;
    }
    prepared_[first].texture->Bind(0);
    glBlendFunc(prepared_[first].blend_source, prepared_[first].blend_destination);
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(first * kVerticesPerQuad),
                 static_cast<GLsizei>((end - first) * kVerticesPerQuad));
    first = end;
  }

  glDisable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glBindVertexArray(0);
}

EffectRenderer::EffectRenderer() = default;

EffectRenderer::EffectRenderer(EffectRenderer&& other) noexcept
    : program_(std::move(other.program_)),
      textures_(std::move(other.textures_)),
      vertex_array_(other.vertex_array_),
      vertex_buffer_(other.vertex_buffer_),
      ordered_effects_(std::move(other.ordered_effects_)),
      ordered_lots_(std::move(other.ordered_lots_)),
      effect_quads_(std::move(other.effect_quads_)),
      painted_(std::move(other.painted_)),
      lot_textures_(std::move(other.lot_textures_)),
      prepared_(std::move(other.prepared_)),
      vertices_(std::move(other.vertices_)) {
  other.vertex_array_ = 0;
  other.vertex_buffer_ = 0;
}

EffectRenderer& EffectRenderer::operator=(EffectRenderer&& other) noexcept {
  if (this != &other) {
    Release();
    program_ = std::move(other.program_);
    textures_ = std::move(other.textures_);
    vertex_array_ = other.vertex_array_;
    vertex_buffer_ = other.vertex_buffer_;
    ordered_effects_ = std::move(other.ordered_effects_);
    ordered_lots_ = std::move(other.ordered_lots_);
    effect_quads_ = std::move(other.effect_quads_);
    painted_ = std::move(other.painted_);
    lot_textures_ = std::move(other.lot_textures_);
    prepared_ = std::move(other.prepared_);
    vertices_ = std::move(other.vertices_);
    other.vertex_array_ = 0;
    other.vertex_buffer_ = 0;
  }
  return *this;
}

EffectRenderer::~EffectRenderer() { Release(); }

void EffectRenderer::Release() noexcept {
  if (vertex_buffer_ != 0) {
    glDeleteBuffers(1, &vertex_buffer_);
    vertex_buffer_ = 0;
  }
  if (vertex_array_ != 0) {
    glDeleteVertexArrays(1, &vertex_array_);
    vertex_array_ = 0;
  }
}

core::Result<EffectRenderer> EffectRenderer::Create() {
  auto program = GlProgram::Link(WithFog(kVertexShader, kFogVertexGlsl),
                                 WithFog(kFragmentShader, kFogFragmentGlsl));
  if (!program) {
    return std::unexpected(program.error());
  }

  EffectRenderer renderer;
  renderer.program_ = std::move(*program);
  glGenVertexArrays(1, &renderer.vertex_array_);
  glGenBuffers(1, &renderer.vertex_buffer_);

  glBindVertexArray(renderer.vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, renderer.vertex_buffer_);
  const auto stride = static_cast<GLsizei>(sizeof(EffectVertex));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(EffectVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(EffectVertex, texture_uv)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                        reinterpret_cast<void*>(offsetof(EffectVertex, tint)));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(EffectVertex, depth_position)));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(EffectVertex, fogged)));
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(EffectVertex, affine)));
  glBindVertexArray(0);
  return renderer;
}

const GlTexture* EffectRenderer::TextureFor(const std::string& path, const formats::Image& image) {
  auto found = textures_.find(path);
  if (found == textures_.end()) {
    // kSprite : une image SEULE, adressée d'un bord à l'autre et sans
    // répétition — au bord, le filtrage irait chercher l'autre extrémité.
    //
    // 🔴 ET LA TEXTURE EST RETOUCHÉE, ce qui est le SEUL écart voulu du dépôt
    // avec le client de référence : son fond presque noir tombe à zéro, parce
    // qu'une vingtaine de couches additives l'empilent et lèvent tout leur
    // quad ; et son texel de bord prend celui d'à côté, parce que deux textures
    // de Storm Gust portent un cadre gris d'un texel laissé par l'outil de
    // dessin. Voir `effect_texture_touchup.h` et l'ADR 0049.
    auto uploaded = GlTexture::Upload(WithoutBlackPedestal(WithoutLeftoverFrame(image)),
                                      GlTexture::Filtering::kSprite);
    CachedTexture cached;
    if (uploaded) {
      cached.texture = std::move(*uploaded);
    }
    found = textures_.emplace(path, std::move(cached)).first;
  }
  found->second.used = true;
  return found->second.texture.valid() ? &found->second.texture : nullptr;
}

void EffectRenderer::ForgetUnusedTextures() {
  std::erase_if(textures_, [](const auto& entry) { return !entry.second.used; });
  for (auto& [path, cached] : textures_) {
    cached.used = false;
  }
}

void EffectRenderer::Draw(const Camera& camera, std::span<const DrawnEffect> effects,
                          float aspect_ratio) {
  if (effects.empty()) {
    return;
  }
  const BillboardBasis basis = MakeBillboardBasis(camera);
  const glm::vec3 eye = camera.Position();
  // Le relèvement suit l'axe du PANNEAU, et non la verticale du monde : les
  // coins d'un effet sont posés dans ce repère-là, et le relever autrement le
  // décalerait dès que la caméra s'incline.
  //
  // ⚠ IL EST PROPRE À CHAQUE EFFET : voir `DrawnEffect::lift_pixels`.
  const auto lift_of = [&basis](float pixels) {
    return basis.up * (pixels / kSpritePixelsPerWorldUnit);
  };

  ordered_effects_.clear();
  for (const DrawnEffect& effect : effects) {
    if (effect.effect != nullptr) {
      ordered_effects_.push_back(&effect);
    }
  }
  // Du plus LOINTAIN au plus proche, et STABLEMENT : deux effets joués au même
  // endroit gardent l'ordre où l'appelant les a donnés.
  std::ranges::stable_sort(ordered_effects_,
                           [&eye](const DrawnEffect* left, const DrawnEffect* right) {
                             return SquaredDistance(left->ground_position, eye) >
                                    SquaredDistance(right->ground_position, eye);
                           });

  prepared_.clear();
  vertices_.clear();
  for (const DrawnEffect* drawn : ordered_effects_) {
    const LoadedEffect& loaded = *drawn->effect;
    ComposeEffect(loaded.prepared, drawn->frame, effect_quads_, drawn->frame_fraction);
    for (const EffectQuad& quad : effect_quads_) {
      if (quad.texture >= loaded.textures.size() || loaded.textures[quad.texture].empty()) {
        continue;
      }
      const GlTexture* texture =
          TextureFor(loaded.texture_paths[quad.texture], loaded.textures[quad.texture]);
      if (texture == nullptr) {
        continue;
      }
      prepared_.push_back(PreparedQuad{texture, BlendFactorOf(quad.blend_source, GL_SRC_ALPHA),
                                       BlendFactorOf(quad.blend_destination, GL_ONE)});

      // Les coins d'un effet sont en pixels autour de son point d'appui,
      // exactement comme ceux d'un sprite : le jeu les grandit de la même
      // échelle tirée de la profondeur.
      const glm::vec3 anchor = drawn->ground_position + lift_of(drawn->lift_pixels);
      std::array<glm::vec2, 4> pixels = quad.corners;
      ShapeStandingEffectCorners(pixels, drawn->lift_pixels, drawn->spin_degrees, drawn->scale,
                                 drawn->flip_x, drawn->flip_y);
      const std::array<glm::vec3, 4> corners = BillboardCorners(pixels, anchor, basis);
      // ⚠ La profondeur d'un effet est celle de son PANNEAU, et pas celle d'un
      // acteur : on ne lui applique pas BillboardDepthCorners.
      //
      // Cette règle-là couche sur le sol ce qui pend SOUS l'origine, parce que
      // l'origine d'un acteur est à ses pieds et que ce qui pend dessous est un
      // corps à terre. L'ancre d'un effet est en l'air, à quatre-vingts pixels
      // du sol : sa moitié basse n'est pas couchée, et la coucher tire ses coins
      // le long du rayon de visée d'une longueur qui DIVISE par le plongeon de
      // la caméra. La profondeur de l'effet se met alors à dépendre fortement de
      // l'angle de vue, et le terrain vient en manger des morceaux — des
      // coupures droites qui balaient l'effet quand la caméra tourne.
      //
      // Les quatre coins d'un panneau face à l'écran sont de toute façon à la
      // même distance ; une marge les fait passer devant le sol qu'ils touchent,
      // comme pour un sprite.
      std::array<glm::vec3, 4> depth_corners = corners;
      for (glm::vec3& corner : depth_corners) {
        corner -= basis.forward * kSpriteGroundClearance;
      }
      const std::array<glm::vec2, 4> texture_uv = {
          glm::vec2{quad.uv_min.x, quad.uv_min.y}, glm::vec2{quad.uv_max.x, quad.uv_min.y},
          glm::vec2{quad.uv_max.x, quad.uv_max.y}, glm::vec2{quad.uv_min.x, quad.uv_max.y}};
      std::array<std::uint8_t, 4> tint = quad.tint;
      if (drawn->alpha != 1.0F) {
        tint[3] = static_cast<std::uint8_t>(
            std::clamp(static_cast<std::int32_t>(drawn->alpha * static_cast<float>(tint[3])), 0,
                       static_cast<std::int32_t>(kOpaqueAlpha)));
      }
      // ⚠ JAMAIS DE BROUILLARD : le client range un effet en fichier dans un
      // seau à mélange propre, que la file vide après l'avoir coupé.
      for (const std::size_t corner : kQuadVertexOrder) {
        vertices_.push_back(
            EffectVertex{corners[corner], texture_uv[corner], tint, depth_corners[corner], 0.0F});
      }
    }
  }
  PaintQuads(camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix(), DepthTest::kOff,
             SceneFog());
}

void EffectRenderer::DrawWorldQuads(const Camera& camera, std::span<const DrawnWorldQuads> lots,
                                    float aspect_ratio, const SceneFog& fog) {
  if (lots.empty()) {
    return;
  }
  const glm::vec3 eye = camera.Position();
  // L'AXE DE VISÉE, la troisième ligne de la matrice de vue retournée : la vue
  // regarde vers ses profondeurs négatives.
  const glm::mat4 view = camera.ViewMatrix();
  const glm::vec3 view_forward = -glm::vec3(view[0][2], view[1][2], view[2][2]);

  ordered_lots_.clear();
  for (const DrawnWorldQuads& lot : lots) {
    if (lot.texture != nullptr && lot.texture_path != nullptr && !lot.quads.empty()) {
      ordered_lots_.push_back(&lot);
    }
  }
  std::ranges::stable_sort(ordered_lots_,
                           [&eye](const DrawnWorldQuads* left, const DrawnWorldQuads* right) {
                             return SquaredDistance(left->ground_position, eye) >
                                    SquaredDistance(right->ground_position, eye);
                           });

  // 🔴 DEUX PASSES, ET LE MOT DE RENDU CHOISIT LA SIENNE. Le client vide les
  // seaux des primitives levées de 0x200 APRÈS tous les autres, l'épreuve de
  // profondeur coupée : ni le
  // décor ni un acteur ne les cachent. Le reste passe l'épreuve.
  //
  // ⚠ ET UN QUAD PEUT QUITTER LE SIEN : aplati au premier plan, il passe avec
  // les seconds quel que soit son lot. Voir `EffectQuadDepth`.
  const glm::mat4 camera_transform = camera.ProjectionMatrix(aspect_ratio) * view;
  lot_textures_.clear();
  for (const DrawnWorldQuads* lot : ordered_lots_) {
    lot_textures_.push_back(TextureFor(*lot->texture_path, *lot->texture));
  }
  const auto paint = [&](bool depth_tested_pass) {
    prepared_.clear();
    vertices_.clear();
    // 🔴 LES PRIMITIVES DE TOUS LES LOTS SE TRIENT ENSEMBLE, seau par seau : la
    // file du client range chacune avec sa propre profondeur, quel que soit
    // l'effet qui l'a posée. Voir `OrderEffectPaintedQuads`. Une figure FERMÉE
    // — la croix de pointes — y trouve aussi l'ordre de ses propres faces.
    painted_.clear();
    for (std::size_t lot_rank = 0; lot_rank < ordered_lots_.size(); ++lot_rank) {
      const DrawnWorldQuads* lot = ordered_lots_[lot_rank];
      if (lot_textures_[lot_rank] == nullptr) {
        continue;
      }
      const bool lot_depth_tested = EffectLotIsDepthTested(lot->render_word);
      const EffectBucketRank bucket = EffectLotBucketRank(lot->render_word);
      for (const EffectWorldQuad& quad : lot->quads) {
        const bool in_front = EffectQuadDepthIsFrontmost(quad.depth);
        if ((lot_depth_tested && !in_front) == depth_tested_pass) {
          painted_.push_back(EffectPaintedQuad{.quad = &quad, .lot = lot_rank, .bucket = bucket});
        }
      }
    }
    OrderEffectPaintedQuads(painted_, eye, view_forward);
    for (const EffectPaintedQuad& entry : painted_) {
      const DrawnWorldQuads* lot = ordered_lots_[entry.lot];
      const EffectWorldQuad& quad = *entry.quad;
      const bool in_front = EffectQuadDepthIsFrontmost(quad.depth);
      const auto placed = PlaceEffectQuadVertices(quad, camera_transform, eye);
      if (!placed.has_value()) {
        continue;
      }
      // ⚠ LE SEAU PEUT IMPOSER SON MÉLANGE, et celui du quad ne compte plus.
      const EffectRenderBlend blend_mode =
          EffectWorldQuadBlend(quad.blend_source, quad.blend_destination, lot->render_word);
      prepared_.push_back(PreparedQuad{lot_textures_[entry.lot],
                                       BlendFactorOf(blend_mode.source, GL_SRC_ALPHA),
                                       BlendFactorOf(blend_mode.destination, GL_ONE)});
      // LA PROFONDEUR EST CELLE DU POINT, sans détour : un quad du monde n'est
      // pas un panneau, il n'y a donc pas de panneau debout dont il faudrait
      // emprunter la profondeur. Le shader s'en aperçoit tout seul — les deux
      // positions étant égales, il garde celle qu'il a projetée.
      // LE SEAU DÉCIDE AUSSI DU BROUILLARD (`EffectLotIsFogged`), et un quad
      // aplati passe HORS DU BROUILLARD : sa profondeur n'en est plus une.
      const float fog_factor = !in_front && EffectLotIsFogged(lot->render_word) ? 1.0F : 0.0F;
      for (std::size_t vertex = 0; vertex < kEffectQuadVertexCount; ++vertex) {
        const std::size_t corner = kEffectQuadVertexCorners[vertex];
        vertices_.push_back(EffectVertex{(*placed)[vertex].position, quad.texture_uv[corner],
                                         quad.tint, (*placed)[vertex].depth_position, fog_factor,
                                         in_front ? 1.0F : 0.0F});
      }
    }
    PaintQuads(camera_transform, depth_tested_pass ? DepthTest::kOn : DepthTest::kOff, fog);
  };
  paint(true);
  paint(false);
}

void EffectRenderer::DrawScreenQuads(std::span<const DrawnScreenQuads> lots, float viewport_width,
                                     float viewport_height) {
  if (lots.empty() || viewport_width <= 0.0F || viewport_height <= 0.0F) {
    return;
  }

  prepared_.clear();
  vertices_.clear();
  for (const DrawnScreenQuads& lot : lots) {
    if (lot.texture == nullptr || lot.texture_path == nullptr || lot.quads.empty()) {
      continue;
    }
    const GlTexture* texture = TextureFor(*lot.texture_path, *lot.texture);
    if (texture == nullptr) {
      continue;
    }
    for (const EffectScreenQuad& quad : lot.quads) {
      // ⚠ LE MODE DE MELANGE DE CETTE VOIE N'EST PAS ENCORE TRADUIT : le client
      // passe UN nombre a sa file de rendu, qui le decompose ailleurs. Ces
      // quads prennent donc le melange ordinaire, et le nombre attend sa
      // mesure. Lui inventer une paire de facteurs peindrait faux avec
      // l'assurance du juste.
      prepared_.push_back(PreparedQuad{texture, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
      for (const std::size_t corner : kQuadVertexOrder) {
        // LA PROFONDEUR EST NULLE, et les deux positions sont egales : le
        // shader garde alors celle qu'il a transformee, comme pour un quad du
        // monde. Rien ici n'est eprouve en profondeur de toute facon.
        const glm::vec3 point{quad.corners[corner], 0.0F};
        vertices_.push_back(EffectVertex{point, quad.texture_uv[corner], quad.tint, point, 0.0F});
      }
    }
  }

  // ⚠ L'ORIGINE EST EN HAUT A GAUCHE ET L'ORDONNEE DESCEND. C'est le repere de
  // la souris et celui d'un sprite, donc celui dans lequel l'appelant a deja
  // tout calcule — et c'est pour cela que le HAUT de la fenetre est le
  // troisieme argument et le BAS le quatrieme, a l'envers de l'usage.
  const glm::mat4 screen = glm::ortho(0.0F, viewport_width, viewport_height, 0.0F, -1.0F, 1.0F);
  PaintQuads(screen, DepthTest::kOff, SceneFog());
}

}  // namespace ro::render
