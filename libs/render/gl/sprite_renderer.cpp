#include "ro/render/sprite_renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

#include <glad/glad.h>

#include "ro/render/fog_shader.h"

namespace ro::render {
namespace {

// La teinte d'un calque sous celle de son acteur : le produit, canal par
// canal, blanc valant un.
constexpr std::uint16_t kTintUnit = 255;
std::array<std::uint8_t, 4> Tinted(const std::array<std::uint8_t, 4>& layer,
                                   const std::array<std::uint8_t, 4>& actor) {
  std::array<std::uint8_t, 4> product{};
  for (std::size_t channel = 0; channel < product.size(); ++channel) {
    product[channel] = static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(layer[channel]) * actor[channel]) / kTintUnit);
  }
  return product;
}

// Aucun éclairage : un sprite RO porte déjà ses ombres dans ses pixels, et lui
// appliquer la lumière de la carte le noircirait deux fois.
constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texture_uv;
layout(location = 2) in vec4 a_tint;
layout(location = 3) in vec3 a_depth_position;
// Un pour un acteur que le brouillard prend, zéro sinon.
layout(location = 4) in float a_fogged;
// Ce qui s'ajoute à l'inverse de la distance à l'œil : voir `DrawnActor::depth_bias`.
layout(location = 5) in float a_depth_bias;

uniform mat4 u_view_projection;
// Ce que la profondeur normalisée gagne par unité d'inverse de distance : le
// terme constant de la projection.
uniform float u_depth_per_inverse_distance;

out vec2 v_texture_uv;
out vec4 v_tint;
out float v_fog_kept;

void main() {
    v_texture_uv = a_texture_uv;
    v_tint = a_tint;
    // La FORME vient du panneau face à l'écran ; la PROFONDEUR, d'un panneau
    // DEBOUT au même endroit — voir sprite_billboard.h. On garde donc x, y et w
    // du premier, et on remplace z par celui du second, remis à l'échelle de w
    // pour que la division perspective rende bien sa profondeur à lui.
    vec4 drawn = u_view_projection * vec4(a_position, 1.0);
    vec4 upright = u_view_projection * vec4(a_depth_position, 1.0);
    // Derrière l'œil, la remise à l'échelle n'a pas de sens : on garde alors la
    // profondeur du quad dessiné, qui est de toute façon hors du champ.
    float z = upright.w > 0.0001 ? upright.z * drawn.w / upright.w : drawn.z;
    // La profondeur normalisée est affine en l'inverse de la distance : le biais
    // s'y ajoute donc multiplié par ce terme, puis remis à l'échelle de w.
    z += a_depth_bias * u_depth_per_inverse_distance * drawn.w;
    gl_Position = vec4(drawn.xy, z, drawn.w);
    v_fog_kept = mix(1.0, FogKeptPart(drawn.w), a_fogged);
}
)GLSL";

constexpr std::string_view kFragmentShader = R"GLSL(
#version 330 core
in vec2 v_texture_uv;
in vec4 v_tint;
in float v_fog_kept;

uniform sampler2D u_texture;
uniform float u_alpha_floor;

out vec4 frag_color;

void main() {
    vec4 texel = texture(u_texture, v_texture_uv) * v_tint;
    // Les sprites palettisés n'ont qu'un alpha binaire : rejeter les texels
    // vides évite qu'ils n'écrivent quoi que ce soit, et le mélange traite les
    // bords adoucis des images BGRA.
    //
    // ⚠ LE SEUIL N'EST PAS LE MÊME SELON CE QU'ON ÉCRIT. Voir
    // `kSpriteDepthAlphaFloor`.
    if (texel.a < u_alpha_floor) {
        discard;
    }
    frag_color = vec4(Fogged(texel.rgb, v_fog_kept), texel.a);
}
)GLSL";

// LE SEUIL D'ALPHA DE LA PASSE DE COULEUR : tout ce qui n'est pas franchement
// vide se peint, bords adoucis compris.
constexpr float kSpriteColorAlphaFloor = 0.01F;

// ⚠ ET CELUI DE LA PASSE DE PROFONDEUR EST BIEN PLUS HAUT — UN DEMI.
//
// Le filtrage donne au sprite une JUPE d'un demi-texel dont l'alpha est faible
// mais NON NUL. Peinte, elle ne fait qu'adoucir le bord ; écrite en PROFONDEUR,
// elle agrandit la silhouette d'un texel, et tout ce qui se dessine ensuite en
// éprouvant la profondeur — les effets — se trouve découpé dans cet anneau. On
// voyait alors, autour du personnage, un liseré de ce qui est DERRIÈRE les
// pointes de terre. Et seulement sur elles : le sol, peint avant le sprite, ne
// s'éprouve contre rien.
//
// Un demi place la silhouette de profondeur exactement à la frontière du texel,
// là où l'œil voit le bord.
constexpr float kSpriteDepthAlphaFloor = 0.5F;

// LA MARQUE QUE LAISSE UN CORPS dans le tampon de stencil, et le masque qui la
// lit : voir `BodySilhouette`. Rien d'autre ne se sert du stencil, et il s'efface
// avec la profondeur au début de chaque image.
constexpr GLint kBodySilhouetteMark = 1;
constexpr GLuint kEveryStencilBit = 0xFF;

// Six sommets par quad : deux triangles, sans tampon d'index. À quelques
// calques par personnage, l'économie d'un index ne vaut pas sa complication.
constexpr std::size_t kVerticesPerQuad = 6;

// Le carré de la distance entre deux points.
//
// Écrit ici plutôt qu'emprunté à glm : la fonction équivalente vit dans une
// extension que glm déclare expérimentale, et deux soustractions ne valent pas
// une dépendance qui peut changer sous nos pieds.
float SquaredDistance(const glm::vec3& left, const glm::vec3& right) {
  const glm::vec3 delta = left - right;
  return glm::dot(delta, delta);
}

}  // namespace

// Un sommet de sprite. La teinte voyage par sommet plutôt que par uniforme :
// un seul tampon porte alors des calques de teintes différentes.
struct SpriteRenderer::SpriteVertex {
  glm::vec3 position;
  glm::vec2 texture_uv;
  std::array<std::uint8_t, 4> tint;
  // D'où le sommet prend sa profondeur, et rien d'autre.
  glm::vec3 depth_position;
  float fogged;
  float depth_bias;
};

// Un quad prêt à dessiner : sa géométrie, et la texture à lui lier.
struct SpriteRenderer::PreparedQuad {
  const GlTexture* texture = nullptr;
  std::array<glm::vec3, 4> corners{};
  std::array<glm::vec3, 4> depth_corners{};
  std::array<glm::vec2, 4> texture_uv{};
  std::array<std::uint8_t, 4> tint{};
  float fogged = 1.0F;
  bool additive = false;
  float depth_bias = 0.0F;
};

SpriteRenderer::SpriteRenderer() = default;

SpriteRenderer::SpriteRenderer(SpriteRenderer&& other) noexcept
    : program_(std::move(other.program_)),
      textures_(std::move(other.textures_)),
      vertex_array_(other.vertex_array_),
      vertex_buffer_(other.vertex_buffer_),
      ordered_(std::move(other.ordered_)),
      prepared_(std::move(other.prepared_)),
      vertices_(std::move(other.vertices_)),
      layer_draws_(std::move(other.layer_draws_)) {
  other.vertex_array_ = 0;
  other.vertex_buffer_ = 0;
}

SpriteRenderer& SpriteRenderer::operator=(SpriteRenderer&& other) noexcept {
  if (this != &other) {
    Release();
    program_ = std::move(other.program_);
    textures_ = std::move(other.textures_);
    vertex_array_ = other.vertex_array_;
    vertex_buffer_ = other.vertex_buffer_;
    ordered_ = std::move(other.ordered_);
    prepared_ = std::move(other.prepared_);
    vertices_ = std::move(other.vertices_);
    layer_draws_ = std::move(other.layer_draws_);
    other.vertex_array_ = 0;
    other.vertex_buffer_ = 0;
  }
  return *this;
}

SpriteRenderer::~SpriteRenderer() { Release(); }

void SpriteRenderer::Release() noexcept {
  if (vertex_buffer_ != 0) {
    glDeleteBuffers(1, &vertex_buffer_);
    vertex_buffer_ = 0;
  }
  if (vertex_array_ != 0) {
    glDeleteVertexArrays(1, &vertex_array_);
    vertex_array_ = 0;
  }
}

core::Result<SpriteRenderer> SpriteRenderer::Create() {
  auto program = GlProgram::Link(WithFog(kVertexShader, kFogVertexGlsl),
                                 WithFog(kFragmentShader, kFogFragmentGlsl));
  if (!program) {
    return std::unexpected(program.error());
  }

  SpriteRenderer renderer;
  renderer.program_ = std::move(*program);
  glGenVertexArrays(1, &renderer.vertex_array_);
  glGenBuffers(1, &renderer.vertex_buffer_);

  glBindVertexArray(renderer.vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, renderer.vertex_buffer_);
  const auto stride = static_cast<GLsizei>(sizeof(SpriteVertex));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(SpriteVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(SpriteVertex, texture_uv)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                        reinterpret_cast<void*>(offsetof(SpriteVertex, tint)));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(SpriteVertex, depth_position)));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(SpriteVertex, fogged)));
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(SpriteVertex, depth_bias)));
  glBindVertexArray(0);
  return renderer;
}

const GlTexture* SpriteRenderer::TextureFor(const SpriteImageKey& key) {
  auto found = textures_.find(key);
  if (found == textures_.end()) {
    // L'image n'est décodée qu'à la première demande, et jamais deux fois :
    // c'est tout l'intérêt de la clé.
    auto uploaded = GlTexture::Upload(ResolveImage(key), GlTexture::Filtering::kSprite);
    CachedTexture cached;
    if (uploaded) {
      cached.texture = std::move(*uploaded);
    }
    found = textures_.emplace(key, std::move(cached)).first;
  }
  found->second.used = true;
  return found->second.texture.valid() ? &found->second.texture : nullptr;
}

void SpriteRenderer::ForgetUnusedTextures() {
  std::erase_if(textures_, [](const auto& entry) { return !entry.second.used; });
  for (auto& [key, cached] : textures_) {
    cached.used = false;
  }
}

void SpriteRenderer::Draw(const Camera& camera, std::span<const DrawnActor> actors,
                          float aspect_ratio, Blending blending, DepthWrite depth_write,
                          DepthTest depth_test, const SceneFog& fog, DrawOrder order,
                          BodySilhouette silhouette) {
  if (actors.empty()) {
    return;
  }
  const BillboardBasis basis = MakeBillboardBasis(camera);
  const glm::vec3 eye = camera.Position();

  // Du plus LOINTAIN au plus proche, sauf si l'appelant a déjà donné l'ordre —
  // voir `DrawOrder`. L'écriture de profondeur étant coupée, c'est l'ordre de
  // dessin seul qui décide qui recouvre qui : sans ce tri, un personnage du
  // fond peut effacer celui du premier plan.
  ordered_.clear();
  for (const DrawnActor& actor : actors) {
    const bool body_shown = actor.actor != nullptr && !actor.actor->quads.empty();
    // Sans acteur, l'entrée ne porte que son ombre. Un acteur dont le corps n'a
    // rien à montrer à cette image ne montre pas non plus la sienne.
    const bool shadow_alone =
        actor.actor == nullptr && actor.shadow != nullptr && !actor.shadow->quads.empty();
    if (body_shown || shadow_alone) {
      ordered_.push_back(&actor);
    }
  }
  // Le tri est STABLE : deux acteurs au même endroit gardent l'ordre où
  // l'appelant les a donnés. Un tri instable les échangerait d'une image à
  // l'autre.
  if (order == DrawOrder::kFarthestFirst) {
    std::ranges::stable_sort(ordered_, [&eye](const DrawnActor* left, const DrawnActor* right) {
      return SquaredDistance(left->ground_position, eye) >
             SquaredDistance(right->ground_position, eye);
    });
  }

  prepared_.clear();
  // LA HAUTEUR DE LA VUE EN PIXELS, que seul un élargissement en pixels d'écran
  // consulte.
  std::array<GLint, 4> viewport{};
  glGetIntegerv(GL_VIEWPORT, viewport.data());
  constexpr std::size_t kViewportHeight = 3;
  const auto viewport_height = static_cast<float>(viewport[kViewportHeight]);
  // `layered` sépare l'acteur de son ombre : l'ombre n'a pas de calques, et ce
  // qu'un effet fait aux calques de son porteur ne la touche pas.
  const auto emit = [&](const ComposedActor& composed, const DrawnActor& drawn,
                        glm::vec2 screen_offset_pixels, bool layered) {
    LayerDrawsOf(composed, layered ? drawn.shown_parts : kAllActorParts,
                 layered ? drawn.additive_parts : ActorPartSet{0}, layered && drawn.doubles_weapon,
                 layer_draws_);
    for (const LayerDraw& layer : layer_draws_) {
      const std::size_t index = layer.quad;
      const SpriteQuad& quad = composed.quads[index];
      if (index >= composed.sources.size()) {
        continue;
      }
      const GlTexture* texture = TextureFor(composed.sources[index]);
      if (texture == nullptr) {
        continue;
      }
      // Le miroir échange les abscisses de texture, il ne déplace pas le quad.
      const float left_u = quad.mirrored ? 1.0F : 0.0F;
      const float right_u = quad.mirrored ? 0.0F : 1.0F;
      SpriteQuad scaled = quad;
      // LE DÉCALAGE D'UNE PARTIE, avant l'échelle. L'ombre n'a pas de parties.
      if (const glm::vec2 part_offset =
              drawn.part_offset_pixels[static_cast<std::size_t>(layer.part)];
          layered && part_offset != glm::vec2(0.0F, 0.0F)) {
        for (glm::vec2& corner : scaled.corners) {
          corner.x += part_offset.x;
          corner.y -= part_offset.y;
        }
      }
      if (drawn.scale != 1.0F) {
        for (glm::vec2& corner : scaled.corners) {
          corner *= drawn.scale;
        }
      }
      if (drawn.height_scale != 1.0F) {
        for (glm::vec2& corner : scaled.corners) {
          corner.y *= drawn.height_scale;
        }
      }
      // LA CULBUTE : le calque tourne sur son propre centre, une fois tassé.
      if (layered && drawn.layer_spin_degrees != 0.0F) {
        scaled = SpinSpriteQuad(scaled, drawn.layer_spin_degrees);
      }
      // LE DÉCALAGE D'ÉCRAN, après l'échelle et dans l'unité des coins.
      //
      // ⚠ L'AXE Y D'UN SPRITE DESCEND : un décalage vers le haut se retranche.
      // C'est la même inversion que fait BillboardCorners, et l'oublier ferait
      // sauter un corps vers le sol.
      if (screen_offset_pixels != glm::vec2(0.0F, 0.0F)) {
        for (glm::vec2& corner : scaled.corners) {
          corner.x += screen_offset_pixels.x;
          corner.y -= screen_offset_pixels.y;
        }
      }
      if (drawn.grow_sprite_pixels != 0.0F) {
        scaled = GrowSpriteQuad(scaled, drawn.grow_sprite_pixels);
      }
      // L'ÉLARGISSEMENT EN PIXELS D'ÉCRAN, converti à la profondeur de l'acteur.
      if (drawn.grow_screen_pixels != 0.0F) {
        scaled = GrowSpriteQuad(
            scaled, drawn.grow_screen_pixels *
                        SpritePixelsPerScreenPixel(camera, drawn.ground_position, viewport_height));
      }
      prepared_.push_back(PreparedQuad{texture,
                                       BillboardCorners(scaled, drawn.ground_position, basis),
                                       BillboardDepthCorners(scaled, drawn.ground_position, basis),
                                       {glm::vec2{left_u, 0.0F}, glm::vec2{right_u, 0.0F},
                                        glm::vec2{right_u, 1.0F}, glm::vec2{left_u, 1.0F}},
                                       Tinted(quad.tint, drawn.tint),
                                       drawn.fogged && !layer.additive ? 1.0F : 0.0F,
                                       blending == Blending::kAdditive || layer.additive,
                                       drawn.depth_bias});
    }
  };
  for (const DrawnActor* drawn : ordered_) {
    // L'ombre d'abord : elle est au sol, et tout le reste passe dessus.
    //
    // ⚠ ELLE NE SUIT PAS LE DÉCALAGE D'ÉCRAN. Le client la dessine hors de la
    // chaîne de calques où vivent les effets : un corps qui tremble tremble
    // seul, son ombre reste posée.
    if (drawn->shadow != nullptr) {
      emit(*drawn->shadow, *drawn, glm::vec2(0.0F, 0.0F), false);
    }
    if (drawn->actor != nullptr) {
      emit(*drawn->actor, *drawn, drawn->screen_offset_pixels, true);
    }
  }
  if (prepared_.empty()) {
    return;
  }
  // LES CALQUES ADDITIFS APRÈS TOUS LES AUTRES, dans leur ordre : le client les
  // range dans une file à part, qu'il vide après celles qui recouvrent. Un
  // calque qui recouvre, peint après un calque additif, l'effacerait.
  const auto first_additive =
      std::ranges::stable_partition(prepared_, [](const PreparedQuad& quad) {
        return !quad.additive;
      }).begin();
  const auto additive_from = static_cast<std::size_t>(first_additive - prepared_.begin());

  vertices_.clear();
  vertices_.reserve(prepared_.size() * kVerticesPerQuad);
  for (const PreparedQuad& quad : prepared_) {
    // Deux triangles, dans l'ordre haut-gauche, haut-droit, bas-droit puis
    // haut-gauche, bas-droit, bas-gauche.
    constexpr std::array<std::size_t, kVerticesPerQuad> kOrder = {0, 1, 2, 0, 2, 3};
    for (const std::size_t corner : kOrder) {
      vertices_.push_back(SpriteVertex{quad.corners[corner], quad.texture_uv[corner], quad.tint,
                                       quad.depth_corners[corner], quad.fogged, quad.depth_bias});
    }
  }

  glBindVertexArray(vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  // Le maillage change à chaque image ET à chaque mouvement de caméra : c'est
  // un tampon de flux, pas un tampon statique.
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(SpriteVertex)),
               vertices_.data(), GL_STREAM_DRAW);

  // ⚠ CE QUI SE LIT PAR-DESSUS LE MONDE NE S'ÉPROUVE PAS CONTRE LUI. Voir
  // `DepthTest` : un nombre de dégâts se pose à l'aplomb d'un acteur, donc à SA
  // profondeur, et la silhouette que l'acteur écrit le découpait.
  if (depth_test == DepthTest::kOn) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);

  program_.Use();
  const glm::mat4 projection = camera.ProjectionMatrix(aspect_ratio);
  program_.SetMatrix4("u_view_projection", projection * camera.ViewMatrix());
  // Sur une perspective, la profondeur normalisée vaut `a + b / distance` ; `b`
  // est ce terme constant de la dernière colonne.
  constexpr int kTranslationColumn = 3;
  constexpr int kDepthRow = 2;
  program_.SetFloat("u_depth_per_inverse_distance", projection[kTranslationColumn][kDepthRow]);
  program_.SetInt("u_texture", 0);
  SetFogUniforms(program_, fog);

  // ⚠ LES QUADS VOISINS D'UNE MÊME TEXTURE PARTENT ENSEMBLE : leurs sommets se
  // suivent déjà dans le tampon, et l'ordre de dessin ne change pas — seul le
  // nombre d'appels baisse. Les chiffres d'un nombre de dégâts en sont le cas
  // type.
  //
  // La DESTINATION du mélange change d'un lot à l'autre, la source non : un
  // quad additif ajoute ce que son alpha laisse passer au lieu d'en effacer
  // autant. Voir `Blending`.
  const auto paint = [this, additive_from]() {
    for (std::size_t first = 0; first < prepared_.size();) {
      const std::size_t lot_end = first < additive_from ? additive_from : prepared_.size();
      std::size_t end = first + 1;
      while (end < lot_end && prepared_[end].texture == prepared_[first].texture) {
        ++end;
      }
      glBlendFunc(GL_SRC_ALPHA, prepared_[first].additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
      prepared_[first].texture->Bind(0);
      glDrawArrays(GL_TRIANGLES, static_cast<GLint>(first * kVerticesPerQuad),
                   static_cast<GLsizei>((end - first) * kVerticesPerQuad));
      first = end;
    }
  };

  // ⚠ ON PEINT D'ABORD, ON ÉCRIT LA PROFONDEUR ENSUITE — jamais l'inverse.
  //
  // Les calques d'un acteur n'ont PAS la même profondeur : celle d'un calque
  // vient d'un panneau DEBOUT à sa place, et la tête, plus haute, est plus
  // proche que le corps. Une profondeur écrite AVANT la couleur rejetterait
  // donc tout calque plus lointain là où deux quads se recouvrent, quelle que
  // soit la comparaison choisie — « inférieur ou égal » n'y peut rien, les
  // profondeurs ne sont pas égales mais différentes. L'ordre du .act serait
  // remplacé par un tri par profondeur entre les calques d'un même acteur.
  //
  // Dans cet ordre-ci, chacune des deux passes ne fait que son office :
  //
  //   * la passe de couleur n'écrit rien et peint tous les calques dans
  //     l'ordre où l'acteur les donne. Elle s'éprouve contre ce que la scène a
  //     déjà écrit — le décor, les acteurs plus proches — donc l'acteur reste
  //     caché par ce qui doit le cacher ;
  //   * la passe de profondeur ne peint rien et laisse la silhouette de
  //     l'acteur, à la profondeur du calque le plus proche par pixel. C'est
  //     elle qui met l'acteur dans le tri de ce qui sera dessiné APRÈS — les
  //     effets.
  //
  // Le coût est un second passage sur la géométrie, et seul l'appel qui écrit
  // le paie.
  // CE QUI ÉVITE LA SILHOUETTE DES CORPS ne se peint pas là où elle est posée :
  // voir `BodySilhouette`.
  if (silhouette == BodySilhouette::kAvoided) {
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, kBodySilhouetteMark, kEveryStencilBit);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  }
  program_.SetFloat("u_alpha_floor", kSpriteColorAlphaFloor);
  paint();
  if (depth_write == DepthWrite::kOn) {
    // ET LA SILHOUETTE SE POSE AVEC LA PROFONDEUR, pixel pour pixel.
    if (silhouette == BodySilhouette::kLeft) {
      glEnable(GL_STENCIL_TEST);
      glStencilFunc(GL_ALWAYS, kBodySilhouetteMark, kEveryStencilBit);
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }
    // ⚠ ET LA PASSE DE PROFONDEUR COUPE PLUS HAUT : voir `kSpriteDepthAlphaFloor`.
    program_.SetFloat("u_alpha_floor", kSpriteDepthAlphaFloor);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    paint();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  glDisable(GL_BLEND);
  glDisable(GL_STENCIL_TEST);
  // Le mélange est REMIS tel qu'on l'a trouvé : un appel additif ne doit pas
  // laisser son état au suivant. L'ÉPREUVE DE PROFONDEUR AUSSI, pour la même
  // raison : l'appel qui s'en dispense ne doit pas en dispenser le suivant.
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glBindVertexArray(0);
}

}  // namespace ro::render
