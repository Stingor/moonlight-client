#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "ro/audio/ambient_sound.h"
#include "ro/audio/audio_engine.h"
#include "ro/audio/sound_placement.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_sound_table.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/skill_unit_sound_table.h"
#include "ro/render/sound_chance.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/vfs.h"

#include "world/map_stage.h"
#include "world/sprite_sound_mutes.h"

namespace ro::render {
class ActorWardrobe;
}  // namespace ro::render

// Les BRUITS des acteurs : ce que leurs animations font entendre.
namespace moonclient {

class EffectsInPlay;

// La graine des tirages « une fois sur N » d'une partie ou d'un décor. Leur
// suite n'a pas à varier d'une session à l'autre : ce qui compte est que chaque
// lancement tire à neuf.
inline constexpr std::uint32_t kSoundDrawSeed = 1;

// Ce qu'il faut pour savoir ce que l'on entend, et d'où.
struct SoundStage {
  ro::audio::AudioEngine* audio = nullptr;
  // Les tables du client : elles nomment le bruit d'une classe d'arme. Sans
  // elles, les coups d'arme se taisent et le reste sonne.
  const ro::render::SpriteCatalog* catalog = nullptr;
  const ro::render::Camera* camera = nullptr;
  const MapStage* stage = nullptr;
  const ro::sim::World* world = nullptr;
  ro::sim::WalkPlanner* planner = nullptr;
  std::uint32_t now_ms = 0;
  // LA GARDE-ROBE, pour qui doit savoir QUAND un coup touche : le vol d'une
  // flèche part au geste, et le geste se lit sur la tenue de qui tire. Nulle
  // quand aucune tenue n'est chargée — rien ne part alors.
  ro::render::ActorWardrobe* wardrobe = nullptr;
  // LES EFFETS EN JEU : `/mineffect`, et ce que chaque acteur porte déjà —
  // deux choses qui décident qu'un son de compétence part. Nuls, ni l'un ni
  // l'autre ne compte.
  const EffectsInPlay* effects = nullptr;
  // CE QUE LE JOUEUR A TU parmi les sons des images. Vide : tout s'entend.
  SpriteSoundMutes mutes;
  // OÙ LA TÊTE DE CHAQUE ACTEUR EST PASSÉE à cette image, que les effets de
  // chapeau attachés à la tête suivent : voir `ComposedScene::head_shifts`. Nul,
  // aucune tête ne bouge.
  const std::map<std::uint32_t, glm::vec2>* head_shifts = nullptr;
};

// UN EFFET QUI VIENT D'ÊTRE LANCÉ, et l'endroit du monde où il sonne.
//
// ⚠ LE BRUIT D'UN EFFET EST RANGÉ SOUS SON IDENTIFIANT, jamais sous la
// compétence : le client l'attache dans la fonction de mise à jour propre à
// chaque identifiant, si bien que deux compétences qui partagent un effet
// partagent son bruit. Voir `ro::render::EffectSound`.
//
// La structure vit ICI et non du côté des effets, parce qu'elle décrit ce qu'il
// faut pour FAIRE UN SON — un identifiant et un point — et non ce qu'il faut
// pour en peindre un.
struct LaunchedEffect {
  std::uint32_t effect_id = 0;
  std::array<float, 3> ground{0.0F, 0.0F, 0.0F};
  // L'ACTEUR QUI LE PORTE, ou zéro. Un bruit peut exiger un porteur ordinaire :
  // voir `ro::render::EffectSoundEntry::needs_plain_bearer`.
  std::uint32_t bearer_id = 0;
};

// Le bruit d'un effet NOMMÉ qui vient de partir, et la case d'où il part.
//
// ⚠ UN CHEMIN, ET NON UN IDENTIFIANT : cette famille-là ne nomme rien par un
// numéro. Sa table de données donne le `.wav` directement, et le chercher dans
// la table des sons d'effet ne rendrait rien.
struct SownNamedSound {
  std::string_view sound;
  ro::sim::CellPosition cell;
  // Entre combien de fichiers le bruit se tire, quand son nom porte `%d` à la
  // place du numéro ; zéro pour un nom fixe. Voir `ro::render::EffectSoundFile`.
  std::uint8_t variants = 0;
  // L'INSTANT où il sonne, quand il ne sonne pas tout de suite : un bruit
  // qu'une méthode empile sur l'effet qu'elle sème attend le tic où l'instance
  // le joue.
  std::uint32_t due_ms = 0;
  // L'acteur dont il suit la place jusqu'à ce qu'il sonne, ou zéro pour rester
  // à la case : l'effet qui le joue est attaché à son porteur.
  std::uint32_t carrier_id = 0;
  // LE POINT DU MONDE D'OÙ IL PART, dans notre repère, quand ce n'est pas une
  // case : le répartiteur d'une compétence posée au sol écrit le sien en dur.
  // Il sonne alors tout de suite, sans tirage.
  std::optional<std::array<float, 3>> point;
  // Sa portée, sa proximité et son volume, quand l'appel les donne.
  float hearing_range = ro::audio::kHearingRange;
  float near_distance = ro::audio::kNearDistance;
  float volume = 1.0F;
};

// Où sont les OREILLES, et de quel côté elles regardent.
// Une source de son de la carte, et le nom UTF-8 de son bruit.
struct MapAmbientSource {
  std::string name;
  ro::audio::AmbientSoundEmitter emitter;
};

struct Listener {
  std::array<float, 3> position{0.0F, 0.0F, 0.0F};
  std::array<float, 3> right{1.0F, 0.0F, 0.0F};
};

// L'auditeur d'une image.
//
// Il se tient sur le PERSONNAGE, jamais sur la caméra. Le client mesure la
// distance d'un son à son propre acteur, et le point de vue n'entre pas dans le
// calcul : la caméra de jeu se tient à plus de mille unités du personnage, soit
// cinq fois la portée d'un son, et écouter de là rendrait la carte muette tant
// qu'on ne zoome pas jusqu'à passer sous la portée.
//
// Sans personnage — devant l'écran des sprites, ou avant l'entrée sur la carte
// — c'est le point VISÉ qui écoute, faute de mieux.
//
// Seule l'ORIENTATION vient de la caméra, et c'est ainsi que le client règle
// son auditeur : la position est celle du personnage, le cap celui de la vue.
[[nodiscard]] Listener ListenerOf(const SoundStage& scene);

// Fait sonner les acteurs dessinés.
//
// Un .act porte une liste de sons et, par image, l'indice de celui qu'elle fait
// entendre : le bruit d'une épée, le cri d'un monstre. Le jeu ne les déclenche
// pas depuis son code, il les LIT DANS L'ANIMATION — et c'est pourquoi cette
// classe travaille sur ce qui vient d'être composé, et non sur ce que le monde
// annonce.
//
// Un son ne se joue qu'au CHANGEMENT d'image. Deux coups successifs jouent le
// même fichier ; sans ce repère, on ne saurait pas qu'il y en a eu deux, et une
// image tenue le rejouerait soixante fois par seconde.
class ActorSounds {
 public:
  // `draw_seed` est la graine des tirages « une fois sur N » : voir
  // `ro::render::SoundChance`.
  ActorSounds(ro::vfs::Vfs& vfs, std::uint32_t draw_seed) : vfs_(&vfs), chance_(draw_seed) {}

  // Tout ce que cette image donne à entendre.
  //
  // `drawn` et `ids` sont les deux moitiés de ce que la scène vient de
  // composer : le quoi et le qui.
  void Hear(const SoundStage& scene, std::span<const ro::render::DrawnActor> drawn,
            std::span<const std::uint32_t> ids);

  // Ce que les UNITÉS POSÉES font entendre, en deux moments.
  //
  // À la POSE, une fois : c'est tout ce que Firewall annonce en arrivant. À
  // l'ENTRETIEN, chaque fois que son effet est resemé — et c'est pourquoi
  // `sown` vient d'ailleurs : seul le semeur d'effets sait quand il a semé, et
  // il ne connaît pas l'audio. `app` porte l'un à l'autre.
  //
  // ⚠ Ces sons ne sont PAS ceux des effets : le client les joue depuis le cas
  // de l'unité. Les chercher dans la table des sons d'effet ne donnait rien.
  void HearSkillUnits(const SoundStage& scene, std::span<const std::uint32_t> sown);

  // Ce que les effets lancés PAR LEUR IDENTIFIANT font entendre : un paquet
  // d'effet nu, une ligne du banc d'essai.
  //
  // ⚠ NI LES COMPÉTENCES NI LES UNITÉS POSÉES NE PASSENT PAR ICI. Leur bruit
  // vient déjà de `PlaySkillSound` et de `HearSkillUnits`, et l'y ajouter le
  // jouerait deux fois — deux voix du même fichier au même instant s'entendent
  // comme un son plus fort, ce qui ressemble à un réglage et non à un défaut.
  void HearEffects(const SoundStage& scene, std::span<const LaunchedEffect> launched);

  // Ce que les effets NOMMÉS des fabriques d'unité font entendre.
  //
  // ⚠ ILS NE PASSENT PAR AUCUNE DES DEUX AUTRES PORTES. Leur bruit n'est ni
  // dans la table des sons d'unité — ces unités n'y sont pas — ni dans celle
  // des sons d'effet, qui est indexée par identifiant. Il est dans la fiche, à
  // côté du fichier, et le semeur est seul à savoir quand il a semé.
  void HearNamedEffects(const SoundStage& scene, std::span<const SownNamedSound> sown);

  // Ce que la TROUPE fait entendre à un instant qu'elle seule connaît.
  //
  // 🔴 UN QUATRIÈME ÉTAGE DE SON, et il ne ressemble à aucun des trois autres :
  // il ne se joue pas au lancement de l'effet mais à un INSTANT de la
  // simulation, et autant de fois qu'il a lieu. Les pièces d'`EF_COIN` sonnent
  // au rebond — joué par la mise à jour du nœud, hors de toute table de son —,
  // et chaque boule de Soul Strike à son semis.
  void HearNodeSounds(const SoundStage& scene, std::span<const ro::render::EffectNodeSound> sounds);

  // AU CHANGEMENT DE CARTE : les sons différés et les bourdons s'éteignent avec
  // les images qu'ils accompagnaient. Sans cela, une salve reçue au moment du
  // warp sonnerait encore sur la carte suivante, à l'ancienne position.
  //
  // ⚠ `seen_` RESTE : le joueur traverse avec son identifiant et sa pose, et
  // l'oublier ferait rejouer le bruit de son image courante.
  //
  // ⚠ LES SOURCES DE LA CARTE RESTENT AUSSI : elles appartiennent à la carte,
  // et un téléport sur la même carte la garde. Voir `LeaveMapAmbience`.
  void Forget() {
    pending_sounds_.clear();
    humming_.clear();
  }

  // LES SOURCES DE SON DE LA CARTE, posées une fois par carte depuis son RSW.
  // `frame` passe les places du client dans notre repère. Voir
  // `ro/audio/ambient_sound.h`.
  void RaiseMapAmbience(std::span<const ro::formats::RswSound> sounds,
                        const ro::render::MapEffectFrame& frame, std::uint32_t now_ms);

  // AU DÉPART POUR UNE AUTRE CARTE : ses sources s'en vont, et les bruits
  // qu'elles avaient lancés se taisent avec elles. Un bruit d'eau ou de foule
  // dure souvent plus que le chargement ; laissé à sa fin, il sonnerait sur la
  // carte d'arrivée.
  //
  // ⚠ SEULEMENT POUR UNE AUTRE CARTE, comme l'horloge des modèles : un
  // téléport sur la même carte garde ses sources, qui continuent.
  void LeaveMapAmbience(ro::audio::AudioEngine& audio);

  // Ce que les sources de la carte font entendre à cette image.
  void HearMapAmbience(const SoundStage& scene);

  // LES SOURCES, EN LECTURE SEULE, pour l'établi du staff qui les montre : leur
  // repère, et l'auditeur de la dernière écoute — c'est de lui que chacune a
  // calculé le point d'où son bruit part.
  [[nodiscard]] std::span<const MapAmbientSource> ambience() const noexcept { return ambience_; }
  [[nodiscard]] const ro::render::MapEffectFrame& ambience_frame() const noexcept {
    return ambience_frame_;
  }
  [[nodiscard]] std::array<float, 3> ambience_listener() const noexcept {
    return ambience_listener_;
  }

 private:
  // Le son des COUPS DE COMPÉTENCE portés depuis le dernier passage.
  //
  // ⚠ Ceux-là ne viennent pas de l'animation : le client les déclenche depuis
  // son code, et c'est une table propre au client qui les nomme. La
  // plupart des compétences classiques n'y sont pas — leur bruit est dans le
  // .act qui les joue, et passe donc par PlayFrames.
  void PlaySkills(const SoundStage& scene, const Listener& listener);

  // Le bruit d'une compétence, posé sur sa cible. Ne fait rien si nous n'en
  // connaissons aucun.
  // ⚠ `on_hit` DIT LEQUEL DES DEUX SONS DU LUA JOUER, et il ne change rien au
  // reste : les soixante-six compétences de `skilleffectinfolist.lub` ont un
  // son pour le chemin de l'EMPLOI (`waveFileName`) et un autre pour celui du
  // COUP (`targetWaveFileName`). Les jouer tous les deux aux deux moments
  // doublerait le bruit d'une compétence sur deux.
  //
  // 🔴 ET LA SUITE DE LA COMPÉTENCE, quand elle a des sons : le client en joue
  // parfois plusieurs, chacun à sa place — sur le lanceur, sur la cible, ou
  // dans le monde près d'elle —, et certains selon `argument`, le niveau ou le
  // montant du paquet. `caster` est nul quand le serveur ne le nomme pas.
  // `damage_type` est la sorte du coup, au coup : un son peut n'en suivre que
  // certaines.
  //
  // ⚠ LA CIBLE EST UN IDENTIFIANT ET UN POINT DU SOL, et non un acteur : une
  // unité au sol frappée — un bloc d'Ice Wall — sonne à sa case comme un
  // monstre à la sienne. Voir `world/struck_place.h`.
  void PlaySkillSound(const SoundStage& scene, const Listener& listener, std::uint32_t target_id,
                      const std::array<float, 3>& target_ground, const ro::sim::Actor* caster,
                      std::uint16_t skill_id, std::uint32_t started_ms, bool on_hit,
                      std::int32_t argument, std::uint8_t damage_type = 0);

  // L'IMPACT d'un coup porté : le bruit de l'arme sur ce qu'elle touche, ou le
  // bruit sourd d'une compétence — et AUTANT DE FOIS QU'IL Y A DE FRAPPES, à
  // l'instant où chacune touche. Voir AttackSound et SkillBlowSound.
  //
  // Il ne vient ni de l'animation ni d'une table de compétences, mais de qui
  // frappe et de ce qu'il tient.
  void PlayBlow(const SoundStage& scene, std::uint32_t target_id, const ro::sim::Strike& strike);

  // Ce que les acteurs dessinés font entendre à cet instant.
  void PlayFrames(const SoundStage& scene, const Listener& listener,
                  std::span<const ro::render::DrawnActor> drawn,
                  std::span<const std::uint32_t> ids);

  // UN ACTEUR PARTI S'OUBLIE, dans les deux témoins qui le suivent. Sans quoi
  // ils grossissent de tout ce qui a croisé l'écran depuis le lancement, et
  // celui qui revient sous le même identifiant se compare à l'image qu'avait
  // son prédécesseur : même action, même image, et son premier bruit se tait.
  void ForgetDeparted(const ro::sim::World& world);

  // Charge un son à la première demande. Faux si le fichier manque — l'échec
  // est retenu, un fichier absent ne se relit pas à chaque coup porté.
  [[nodiscard]] bool Known(ro::audio::AudioEngine& audio, const std::string& name);

  ro::vfs::Vfs* vfs_ = nullptr;
  // LE TIRAGE DES BRUITS « UNE FOIS SUR N », un par lancement.
  ro::render::SoundChance chance_;
  // La dernière image vue de chaque acteur : l'action et l'image, dans un seul
  // nombre.
  std::unordered_map<std::uint32_t, std::uint64_t> seen_;
  // Les noms déjà tentés, qu'ils aient abouti ou non.
  std::unordered_map<std::string, bool> tried_;

  std::vector<MapAmbientSource> ambience_;
  ro::render::MapEffectFrame ambience_frame_;
  // Où l'auditeur se tenait à la dernière écoute des sources, dans NOTRE repère.
  std::array<float, 3> ambience_listener_{0.0F, 0.0F, 0.0F};
  // Les coups déjà entendus. Voir `ro::sim::StrikeLog`.
  ro::sim::StrikeCursor heard_strikes_;
  // Les compétences SANS DÉGÂTS déjà entendues, TOUTES : voir
  // `ro::sim::SupportLog`.
  ro::sim::SupportCursor heard_supports_;
  // L'instant de pose de chaque unité déjà entendue : une unité vit des
  // minutes, et son bruit d'arrivée ne se fait qu'une fois.
  std::unordered_map<std::uint32_t, std::uint32_t> placed_units_;
  // Les unités de CHANSON vues, et leur compétence : leur départ tait la musique.
  std::unordered_map<std::uint32_t, std::uint16_t> song_units_;

  // UN SON QUI ATTEND SON INSTANT. Le client joue nombre de bruits d'effet à
  // un tic PRÉCIS de l'instance et non à sa naissance — trente-sept pour l'onde
  // de Bash, soixante pour Impositio Manus, douze pour la flèche de glace —,
  // que `EffectSoundEntry::delay_ticks` porte ; et le bruit d'un coup sonne à
  // CHAQUE message de dégâts, quand le coup touche, autant de fois qu'il y a
  // de coups. Le point est celui de l'effet ou de la cible ; l'écouteur sera
  // celui du moment où il sonne.
  //
  // ⚠ IL RETIENT SA FAMILLE : la file mêle les coups, les compétences et les
  // effets, et un impact différé sonnerait sinon au volume des effets.
  struct PendingSound {
    std::string sound;
    std::array<float, 3> ground{0.0F, 0.0F, 0.0F};
    std::uint32_t due_ms = 0;
    ro::audio::SoundChannel channel = ro::audio::SoundChannel::kEffects;
    // L'acteur dont il suit la place, ou zéro : voir `SownNamedSound::carrier_id`.
    std::uint32_t carrier_id = 0;
    // Sur l'auditeur tel qu'il sera à son tic : voir `EffectSoundEntry::at_listener`.
    bool at_listener = false;
    // Et à quelle hauteur au-dessus de lui : voir `EffectSoundEntry::listener_height`.
    std::uint8_t listener_height = 0;
  };
  std::vector<PendingSound> pending_sounds_;

  // UN BRUIT QUI BOURDONNE TANT QUE SON EFFET VIT.
  //
  // 🔴 CE N'EST PAS UN SON DIFFÉRÉ DE PLUS. Un `PendingSound` part une fois et
  // s'oublie ; celui-ci se REPOSE la question à chaque image, et c'est ce qui
  // le fait TAIRE. Le client détruit l'instance d'effet avec l'acteur qui l'a
  // semée, donc le bourdon s'arrête net ; une file de coups programmés à
  // l'avance, elle, chanterait encore trois secondes après que le portail a
  // disparu.
  //
  // ⚠ ET L'EXTINCTION EST GRATUITE, parce qu'une unité au sol RESÈME son effet
  // plus souvent qu'il ne vit : le portail relance toutes les 192 tics un effet
  // qui en dure 200. Chaque semis repousse l'échéance, et le silence tombe au
  // plus tard huit tics après le dernier — soit cent vingt-huit millisecondes.
  struct HummingEffect {
    std::uint32_t effect_id = 0;
    std::string sound;
    std::array<float, 3> ground{0.0F, 0.0F, 0.0F};
    std::uint32_t period_ms = 0;
    std::uint32_t next_due_ms = 0;
    std::uint32_t until_ms = 0;
  };
  std::vector<HummingEffect> humming_;

  // Pose un bourdon, ou repousse l'échéance de celui qui tient déjà ce point.
  void Hum(const LaunchedEffect& effect, const ro::render::EffectSoundEntry& entry,
           std::uint32_t now_ms);

  // Ce dont on a déjà dit quel bruit il rend : une entrée par DÉCISION — arme,
  // métier, sorte du coup, voie — et non une par coup. Un combat en porte des
  // centaines.
  std::set<std::uint64_t> reported_blows_;

  // Joue un son sur une case, s'il y a quelque chose à jouer et que la case
  // s'entend d'où l'on est.
  // `one_in` est le dénominateur du TIRAGE : un son sur autant s'entend, et un
  // veut dire à coup sûr. Chaque appel tire à neuf.
  void PlayOnCell(const SoundStage& scene, const Listener& listener, std::string_view sound,
                  std::uint32_t one_in, const ro::sim::CellPosition& cell);
};

}  // namespace moonclient
