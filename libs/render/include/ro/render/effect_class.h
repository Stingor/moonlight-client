#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_screen_quad.h"

namespace ro::render {

// LES CLASSES DU GESTIONNAIRE D'EFFETS : un effet que le client ne décrit ni par
// un fichier ni par un patron de nœuds, mais par une CLASSE qui a son propre
// code — ses particules, ses `.str`, ses sons, sa fin. Une classe inscrite pour
// un identifiant passe devant le primitif : voir
// `docs/classes-du-gestionnaire-d-effets.md`.
//
// Description PURE, comme les nœuds : chaque instance calcule, tic après tic,
// ce qu'elle montre et ce qu'elle fait entendre ; `app` lui donne la place de
// ses acteurs et peint ce qu'elle rend.
//
// 🔴 LE CALCUL SE FAIT DANS LE REPÈRE DU CLIENT — ordonnée qui DESCEND,
// profondeur opposée à la nôtre —, et chaque formule s'y transcrit telle qu'elle
// est écrite. On ne passe au nôtre qu'aux bords : ce qui entre (la place d'un
// acteur) et ce qui sort (un point à peindre).

// Le passage d'un repère à l'autre : l'ordonnée et la profondeur changent de
// signe, dans un sens comme dans l'autre.
[[nodiscard]] constexpr glm::vec3 EffectClassFromClient(glm::vec3 client) {
  return {client.x, -client.y, -client.z};
}
[[nodiscard]] constexpr glm::vec3 EffectClassToClient(glm::vec3 ours) {
  return {ours.x, -ours.y, -ours.z};
}

// LES TABLES DE DEGRÉS DU CLIENT, lues comme il les lit : un angle entier de
// [−361, 360]. ⚠ Un angle négatif lit la table QUI PRÉCÈDE — pour le cosinus,
// celle des sinus opposés ; pour le sinus, celle des radians opposés —, à
// l'entrée `361 + angle`.
[[nodiscard]] float EffectClassSinTable(std::int32_t degrees);
[[nodiscard]] float EffectClassCosTable(std::int32_t degrees);

// CE QUE LA CAMÉRA DIT À UNE CLASSE : ses deux angles DANS LE REPÈRE DU
// CLIENT, les axes du panneau face à elle dans le NÔTRE, et la projection d'un
// point du nôtre en pixels d'écran (ordonnée qui descend), vide quand le point
// est derrière elle.
struct EffectClassView {
  float yaw_degrees = 0.0F;
  float pitch_degrees = 0.0F;
  glm::vec3 right{1.0F, 0.0F, 0.0F};
  glm::vec3 up{0.0F, 1.0F, 0.0F};
  glm::vec3 forward{0.0F, 0.0F, -1.0F};
  std::function<std::optional<glm::vec2>(glm::vec3)> project;
  // LA TAILLE DE L'ÉCRAN, en pixels : une classe qui peint en plein écran y pose
  // ses coins. Zéro quand elle n'est pas connue, et rien ne se peint alors.
  float viewport_width = 0.0F;
  float viewport_height = 0.0F;
};

// LA DISTANCE AU JOUEUR, en unités du monde, à partir de laquelle une instance
// se fige.
inline constexpr float kEffectClassCullDistance = 130.0F;
// LA DURÉE D'UNE INSTANCE qui ne pose pas la sienne, en tics.
inline constexpr std::int32_t kEffectClassDefaultDurationTicks = 60;

// LA NAISSANCE D'UNE INSTANCE, telle que le gestionnaire la reçoit.
struct EffectClassBirth {
  std::uint32_t effect_id = 0;
  // La place donnée à la naissance, dans le repère du client.
  glm::vec3 position{0.0F, 0.0F, 0.0F};
  // LA PLACE DE L'ACTEUR LIÉ À LA NAISSANCE, copiée une fois — vide sans acteur
  // lié.
  std::optional<glm::vec3> bound_position;
  // Le paramètre de la naissance : le niveau de la compétence pour une unité,
  // moins un sinon.
  std::int32_t level = -1;
  // Le retard que le site impose après la naissance, en millisecondes.
  std::int32_t delay_ms = 0;
  // Le réglage des effets allégés (`/mineffect`).
  bool minimal_effects = false;
  // L'horloge murale à la naissance, en millisecondes.
  std::uint32_t now_ms = 0;
  // La graine du tirage de l'instance.
  std::uint32_t seed = 1;
  // LES SPHÈRES DU PORTEUR à la naissance — leur élément (un l'eau, deux la
  // terre, trois le feu, quatre le vent) et leur nombre —, que lit la classe qui
  // les libère. Zéro sans sphère.
  std::int32_t owner_charm_element = 0;
  std::int32_t owner_charm_count = 0;
};

// CE QUE LE GESTIONNAIRE SAIT À CHAQUE TIC, dans le repère du client.
struct EffectClassTick {
  // La place du propriétaire, quand il est encore là ; l'instance la reprend
  // avant de se mettre à jour.
  std::optional<glm::vec3> owner_position;
  // Vrai tant que le propriétaire existe — ou qu'il n'y en a pas.
  bool owner_present = true;
  // La place de l'acteur lié à cet instant, quand il se résout encore.
  std::optional<glm::vec3> bound_position;
  // VRAI QUAND L'INSTANCE EST TROP LOIN du joueur — `kEffectClassCullDistance`
  // ou plus : elle ne vieillit pas, ne sonne pas, ne se met pas à jour et ne se
  // peint pas, mais elle vit.
  bool culled = false;
  // VRAI SUR LA SEULE CARTE que le client nomme en clair pour ses étincelles de
  // soin — voir `EffectScatterWorld::wide_scatter`.
  bool wide_scatter_map = false;
  // L'HORLOGE MURALE, en millisecondes : une classe qui compte en temps réel
  // et non en tics la lit.
  std::uint32_t now_ms = 0;
  const EffectClassView* view = nullptr;
  // LA DERNIÈRE IMAGE d'un `.str`, par son chemin relatif au dossier des effets
  // — vide quand le fichier ne se lit pas.
  std::function<std::optional<std::int32_t>(std::string_view)> str_last_frame;
  // LE NOMBRE D'IMAGES d'une action d'un sprite, par son chemin relatif au
  // dossier des sprites — zéro quand il ne se lit pas.
  std::function<std::int32_t(std::string_view, std::int32_t)> sprite_frame_count;
  // CE QUE LE PORTEUR MONTRE : la direction de son IMAGE, la caméra comptée —
  // zéro face à elle, puis sud-ouest, ouest, nord-ouest, nord, nord-est, est,
  // sud-est —, et l'angle de son CORPS en degrés, tel que le client le tient.
  // Vides sans porteur.
  std::optional<std::uint8_t> owner_sprite_direction;
  std::optional<float> owner_body_degrees;
  // LE MÉTIER DU PORTEUR, qui règle la hauteur de certains sprites posés sur
  // lui ; vide sans porteur.
  std::optional<std::uint32_t> owner_job;
  // DE COMBIEN LE CORPS DU PORTEUR EST RAPETISSÉ à cet instant, de un à un
  // demi : un sprite posé sur lui rapetisse avec lui. Un sans porteur.
  float owner_shrink_scale = 1.0F;
};

// UN `.str` À PEINDRE.
struct EffectClassStrDraw {
  // Relatif au dossier des effets.
  std::string path;
  // Vrai quand la variante allégée « min_ » le remplace sous `/mineffect`.
  bool allows_min_variant = false;
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  std::int32_t frame = 0;
  // Vrai pour la file de rendu du SOL, peinte avant les corps.
  bool behind_actors = false;
  float lift_pixels = 0.0F;
  float spin_degrees = 0.0F;
  float scale = 1.0F;
  float alpha = 1.0F;
  // RETOURNÉ À L'ÉCRAN : ses coins changent de côté autour du point qui le porte.
  bool flip_x = false;
};

// UN QUAD DU MONDE À PEINDRE, sa texture relative au dossier des textures et
// le mot de rendu de sa particule.
struct EffectClassQuadDraw {
  std::string_view texture;
  EffectWorldQuad quad;
  std::uint32_t render_word = 0;
};

// UN SPRITE D'ACTEUR À PEINDRE, face à la caméra.
struct EffectClassSpriteDraw {
  // Relatif au dossier des sprites, sans extension.
  std::string_view sprite;
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  std::int32_t action = 0;
  std::int32_t frame = 0;
  float scale = 1.0F;
  // Rotation dans le plan de l'écran, en degrés.
  float spin_degrees = 0.0F;
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  // Vrai quand la couleur de chaque calque remplace celle de la particule.
  bool keeps_layer_color = false;
  bool additive = false;
  // LE MOT DE RENDU de la particule, qui range le sprite dans son seau : le
  // mélange, l'épreuve de profondeur, la place avant ou après les fichiers et
  // les corps — voir `ro/render/effect_render_word.h`. Sans mot, le sprite
  // passe après les fichiers, éprouvé, et `additive` décide du mélange.
  std::optional<std::uint32_t> render_word;
  // DE COMBIEN LE SPRITE S'AVANCE VERS LA CAMÉRA, ajouté à l'INVERSE de sa
  // distance à l'œil, en unités du monde à la puissance moins un : un sprite
  // posé au même endroit qu'un corps passe ainsi devant lui. Zéro pour aucun.
  float depth_bias = 0.0F;
};

// UN QUAD D'ÉCRAN À PEINDRE : ses coins sont déjà des PIXELS, rien ne les
// projette (ADR 0022), et sa texture est relative au dossier des textures.
//
// ⚠ SES COORDONNÉES DE TEXTURE VONT DE ZÉRO À UN : celui qui compose le lot,
// et qui seul connaît la taille de l'image, y ajoute le demi-texel du client.
struct EffectClassScreenDraw {
  std::string_view texture;
  EffectScreenQuad quad;
};

// UN SON, à sa place dans notre repère.
struct EffectClassSound {
  std::string_view file;
  glm::vec3 position{0.0F, 0.0F, 0.0F};
};

// CE QU'UNE INSTANCE REND : ses dessins, et les évènements du tic.
struct EffectClassOutput {
  std::vector<EffectClassStrDraw> strs;
  std::vector<EffectClassQuadDraw> quads;
  std::vector<EffectClassSpriteDraw> sprites;
  std::vector<EffectClassScreenDraw> screen_quads;
  std::vector<EffectClassSound> sounds;
  // Le tremblement d'écran demandé, en millisecondes ; zéro pour aucun.
  std::uint32_t quake_ms = 0;
  // LES EFFETS QU'UNE INSTANCE POSE SUR SON PORTEUR EN S'EN ALLANT, par leur
  // numéro : l'ombre de Shadow Exceed lui laisse son aura. C'est à qui fait
  // battre l'instance de les faire naître — sur le porteur, et pas s'il les
  // porte déjà.
  std::vector<std::uint32_t> spawns_on_owner;
};

// UN `.str` JOUÉ PAR UNE CLASSE : l'hôte du lecteur, une image par tic.
//
// ⚠ IL NE BOUCLE PAS D'ORDINAIRE, il FINIT : à la dernière image, l'appel
// suivant le dit fini. Sa dernière image reste alors ce qu'il montre, tant que
// sa classe le garde. Un hôte en boucle repart de l'image zéro.
struct EffectClassStrHost {
  std::string path;
  bool allows_min_variant = true;
  // La place, dans le repère du client.
  glm::vec3 position{0.0F, 0.0F, 0.0F};
  // Un décalage que certaines classes lui ajoutent.
  glm::vec3 offset{0.0F, 0.0F, 0.0F};
  std::int32_t counter = 0;
  std::optional<std::int32_t> shown_frame;
  // Le retard avant la première image, en millisecondes ; seize par appel.
  std::int32_t delay_ms = 0;
  bool loops = false;
  bool finished = false;
  bool behind_actors = false;
  float lift_pixels = 0.0F;
  float spin_degrees = 0.0F;
  float scale = 1.0F;
  float alpha = 1.0F;
  // Retourné à l'écran, comme le dessin qu'il donne.
  bool flip_x = false;
};

// UN APPEL DU LECTEUR : rend faux quand le `.str` a fini.
bool AdvanceEffectClassStrHost(EffectClassStrHost& host, const EffectClassTick& tick);
// Ajoute l'image montrée, quand il y en a une.
void DrawEffectClassStrHost(const EffectClassStrHost& host, EffectClassOutput& output);

// UNE INSTANCE DE CLASSE.
class EffectClassInstance {
 public:
  explicit EffectClassInstance(const EffectClassBirth& birth);
  virtual ~EffectClassInstance() = default;
  EffectClassInstance(const EffectClassInstance&) = delete;
  EffectClassInstance& operator=(const EffectClassInstance&) = delete;

  // UN TIC DU GESTIONNAIRE : la place du propriétaire reprise, les sons en
  // file dus à cet âge, la mise à jour, puis l'âge qui avance.
  void Advance(const EffectClassTick& tick, EffectClassOutput& output);
  // Vrai tant que l'instance vit — par défaut, tant que son âge n'a pas atteint
  // sa durée.
  [[nodiscard]] virtual bool Alive(const EffectClassTick& tick) const;
  virtual void Draw(const EffectClassView& view, EffectClassOutput& output) const = 0;
  // UNE NAISSANCE REDEMANDÉE sur le même porteur, quand l'instance se reprend :
  // le client retrouve l'instance et lui passe le nouveau paramètre. Ne fait
  // rien d'ordinaire ; les sphères en tirent leur nouveau nombre.
  virtual void Retell(std::int32_t /*level*/) {}

  [[nodiscard]] std::uint32_t effect_id() const noexcept { return effect_id_; }
  [[nodiscard]] std::int32_t age() const noexcept { return age_; }
  // Vrai quand le dernier tic l'a trouvée trop loin : elle ne se peint pas.
  [[nodiscard]] bool culled() const noexcept { return culled_; }
  // Sa place, dans le repère du client.
  [[nodiscard]] glm::vec3 position() const noexcept { return position_; }
  // Vrai quand une seconde naissance sur le même porteur rend celle-ci au lieu
  // d'en créer une.
  [[nodiscard]] bool reused_on_same_owner() const noexcept { return reused_on_same_owner_; }

 protected:
  virtual void Update(const EffectClassTick& tick, EffectClassOutput& output) = 0;
  void QueueSound(std::string_view file, std::int32_t at_age);
  void QueueQuake(std::int32_t at_age);
  // Le tirage de l'instance, sur l'étendue de `rand()`.
  [[nodiscard]] std::int32_t Rand();
  [[nodiscard]] glm::vec3 OursOf(glm::vec3 client) const { return EffectClassFromClient(client); }

  std::uint32_t effect_id_ = 0;
  glm::vec3 position_{0.0F, 0.0F, 0.0F};
  std::optional<glm::vec3> bound_position_;
  std::int32_t level_ = -1;
  std::int32_t age_ = 0;
  std::int32_t duration_ticks_ = kEffectClassDefaultDurationTicks;
  bool minimal_effects_ = false;
  bool reused_on_same_owner_ = false;
  bool culled_ = false;

 private:
  struct Queued {
    std::string_view file;
    std::int32_t at_age = 0;
    bool quake = false;
  };
  std::vector<Queued> queued_;
  std::uint32_t generator_ = 1;
};

// VRAI QUAND LE SITE FAIT NAÎTRE LA CLASSE À LA PLACE DU LANCEUR même si elle
// suit la cible : le boulet de Cart Cannon part des pieds de qui le tire.
[[nodiscard]] constexpr bool EffectClassBornAtCaster(std::uint32_t effect_id) {
  constexpr std::uint32_t kCartCannon = 1357;
  return effect_id == kCartCannon;
}

// VRAI QUAND LE CLIENT PASSE À LA CLASSE LE NIVEAU DE LA COMPÉTENCE annoncée,
// là où les autres classes d'une compétence naissent sans paramètre : les
// carottes de Lunatic Carrot Beat en tirent leur nombre, Gale Storm l'étendue
// de ses rafales, Jupitel Thunder Storm le nombre et la portée de ses éclairs.
[[nodiscard]] constexpr bool EffectClassReceivesSkillLevel(std::uint32_t effect_id) {
  constexpr std::uint32_t kCarrotBeat = 1099;
  constexpr std::uint32_t kCarrotBeatBright = 1111;
  constexpr std::uint32_t kGaleStorm = 1973;
  constexpr std::uint32_t kJupitelThunderStorm = 2366;
  return effect_id == kCarrotBeat || effect_id == kCarrotBeatBright || effect_id == kGaleStorm ||
         effect_id == kJupitelThunderStorm;
}

// LE PARAMÈTRE QU'UNE COMPÉTENCE PASSE À L'OBJET QU'ELLE LANCE, quand elle en
// passe un : la potion de Potion Pitcher prend le niveau, ramené entre un et
// dix, et sa teinte en dépend ; celle de Berserk Pitcher et la spore reçoivent
// toujours quatre. Vide pour les autres classes, qui naissent sans paramètre.
[[nodiscard]] constexpr std::optional<std::int32_t> EffectClassThrownParam(
    std::uint32_t effect_id, std::int32_t skill_level) {
  constexpr std::uint32_t kPotion = 299;
  constexpr std::uint32_t kBerserkPotion = 541;
  constexpr std::uint32_t kSpore = 914;
  constexpr std::int32_t kLowestLevel = 1;
  constexpr std::int32_t kHighestLevel = 10;
  constexpr std::int32_t kBerserkTint = 4;
  if (effect_id == kPotion) {
    return skill_level < kLowestLevel    ? kLowestLevel
           : skill_level > kHighestLevel ? kHighestLevel
                                         : skill_level;
  }
  if (effect_id == kBerserkPotion || effect_id == kSpore) {
    return kBerserkTint;
  }
  return std::nullopt;
}

// VRAI QUAND LE CLIENT NE MONTRE CETTE CLASSE QUE SUR SON PROPRE PERSONNAGE :
// le givre plein écran d'une gelure n'a de sens que pour qui la subit.
[[nodiscard]] constexpr bool EffectClassOwnPlayerOnly(std::uint32_t effect_id) {
  constexpr std::uint32_t kFrostbiteScreen = 2272;
  return effect_id == kFrostbiteScreen;
}

// VRAI QUAND LA CLASSE TIENT SON NOMBRE DE LA VALEUR DE L'EFFET PORTÉ : les
// armes de servant et les sphères d'ombre, que le paquet des sphères compte,
// et les quatre sphères élémentaires, que compte le leur. L'effet porté naît
// par elle, et chaque nouvelle valeur la lui redit.
inline constexpr std::uint32_t kServantWeaponsEffect = 1694;
inline constexpr std::uint32_t kShadowSpheresEffect = 2109;
inline constexpr std::uint32_t kFirstElementalOrbEffect = 735;
inline constexpr std::uint32_t kLastElementalOrbEffect = 738;
[[nodiscard]] constexpr bool EffectClassCountsFromValue(std::uint32_t effect_id) {
  return effect_id == kServantWeaponsEffect || effect_id == kShadowSpheresEffect ||
         (effect_id >= kFirstElementalOrbEffect && effect_id <= kLastElementalOrbEffect);
}

// Vrai quand une classe du client est portée pour cet identifiant.
[[nodiscard]] bool IsPortedEffectClass(std::uint32_t effect_id);

// Tous les identifiants portés, triés.
[[nodiscard]] std::vector<std::uint32_t> PortedEffectClassIds();

// La naissance d'une instance, ou rien quand aucune classe portée ne répond.
[[nodiscard]] std::unique_ptr<EffectClassInstance> SpawnEffectClass(const EffectClassBirth& birth,
                                                                    const EffectClassView& view);

}  // namespace ro::render
