#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/named_effects.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/effect_table.h"
#include "ro/render/hat_effect_table.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_named_recipe.h"
#include "ro/render/skill_named_table.h"
#include "ro/render/skill_suite_table.h"
#include "ro/render/skill_unit_table.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_actor_view.h"
#include "effects/effect_class_view.h"
#include "effects/effect_draw_gate.h"
#include "effects/effect_map_memory.h"
#include "effects/effect_node_sowing.h"
#include "effects/effect_rules.h"
#include "effects/effect_suite_condition.h"
#include "world/actor_sounds.h"
#include "world/map_stage.h"

// Les EFFETS ANIMÉS en place dans la scène : ce qui a été lancé, et depuis
// quand. C'est ici que le temps de la fenêtre devient une image de fichier.
namespace moonclient {

// La troupe des effets PROCÉDURAUX, déclarée plus bas : les effets en fichier
// la connaissent, parce qu'une compétence n'a pas à savoir de quelle famille
// relève son effet.
class EffectNodesInPlay;

// Les effets en cours de lecture.
//
// Un effet ne BOUCLE pas : il court de sa première image à sa dernière, puis
// s'efface. Ce qui doit durer est relancé, jamais prolongé — c'est ainsi que le
// jeu procède, et c'est ce qui laisse la fin d'un effet se décider d'un seul
// endroit.
class EffectsInPlay {
 public:
  // Le VFS est pris en plus du catalogue parce qu'une famille d'effets ne se
  // décrit pas dans le code du client : celle des effets NOMMÉS, dont la table est
  // un fichier de l'installation. Elle se lit à la première unité qui en
  // demande un, et pas au démarrage — la plupart des parties n'en croisent
  // aucune.
  EffectsInPlay(ro::render::EffectCatalog& catalog, EffectNodesInPlay& nodes, ro::vfs::Vfs& vfs)
      : catalog_(&catalog), nodes_(&nodes), vfs_(&vfs), classes_(catalog, vfs) {}

  // LES CLASSES DU GESTIONNAIRE D'EFFETS : voir `EffectClassesInPlay`.
  [[nodiscard]] EffectClassesInPlay& Classes() noexcept { return classes_; }

  // LES DEUX BASCULES D'EFFET DU JEU — `/effect` et `/mineffect`, réglages 8
  // et 229 du client —, poussées ici et à la troupe procédurale. ⚠ ELLES NE
  // TUENT RIEN : les portiers relisent le drapeau à chaque image, et ce qui
  // était caché revient si l'on rebascule.
  void SetEffectSwitches(bool shown, bool minimal);
  // LA PORTE DE DESSIN DE L'IMAGE, recalculée sur le monde et le groupe : voir
  // `EffectDrawGate`. Les chapeaux de ses porteurs cachés y sont nommés.
  void NoticeDrawGate(const ro::sim::World& world, const ro::sim::PartyRoster* party);
  [[nodiscard]] const EffectDrawGate& draw_gate() const noexcept { return draw_gate_; }

  // Vrai quand `/effect` laisse voir cet effet-là. ⚠ LA LISTE BLANCHE PASSE
  // OUTRE : cent quatorze identifiants restent visibles même coupés.
  [[nodiscard]] bool ShowsEffect(std::uint32_t effect_id) const;

  // Vrai quand `/mineffect` est posé : certaines compétences choisissent alors
  // leur variante allégée.
  [[nodiscard]] bool MinimalEffects() const { return minimal_effects_; }

  // Vrai quand `/effect` est posé, liste blanche À PART : c'est ce que la
  // météo d'une carte lit pour savoir si elle se lève.
  [[nodiscard]] bool EffectsShown() const { return effects_shown_; }

  // OUBLIE TOUT CE QUI JOUE, ET TOUT CE QUI A ÉTÉ VU — sans rien oublier de ce
  // qui a été LU.
  //
  // 🔴 LE CHANGEMENT DE CARTE ÉTEINT TOUT, même quand la carte est la même :
  // `@refresh` et `@load` passent par le même paquet qu'un vrai voyage, et le
  // client natif y détruit son monde entier, effets compris, puis le rebâtit
  // de ce que le serveur réannonce. Les mémoires de transition partent avec :
  // ce que le serveur redit après la carte est NEUF, comme chez lui. Les tables
  // lues — chapeaux, effets nommés — et les avertissements déjà donnés restent.
  // ⚠ LA TROUPE PROCÉDURALE S'OUBLIE PAR SON PROPRE `Forget` : l'appelant fait
  // les deux.
  void Forget();

  // Repère les effets que le monde porte depuis le dernier passage, et lance
  // ceux qu'il sait nommer.
  //
  // Le monde ne garde qu'un NUMÉRO ; c'est ici qu'il devient un fichier, par la
  // table des effets du client d'origine.
  //
  // Un numéro que cette table ne nomme pas est PROCÉDURAL — le client le
  // dessine en code —, et il a désormais sa chance : les patrons de nœuds sont
  // indexés par l'effet, et c'est par ici que passent les impacts que l'on voit
  // à chaque coup porté. Ce qui n'a ni fichier ni patron se dit une fois sur la
  // sortie d'erreur, et non à chaque image.
  void Notice(const ro::sim::World& world, const MapStage& stage, ro::sim::WalkPlanner& planner);

  // Repère les compétences JOUÉES depuis le dernier passage, et lance l'effet
  // de chacune.
  //
  // Trois chemins, et le client les traite séparément : le coup qui PORTE, la
  // compétence SANS DÉGÂTS — un soin, un renfort —, et celle lancée sur une
  // CASE. Une compétence sans dégâts n'arrive par aucun des deux autres, et
  // c'est pourquoi la famille acolyte et prêtre restait muette.
  //
  // Sur le LANCEUR ou sur la CIBLE : c'est la compétence qui le dit, et le
  // client ne les traite pas de la même façon. Voir SkillImpl.
  //
  // La plupart des compétences n'ont rien à jouer ici : leur effet est
  // procédural, et nous ne rendons que les fichiers.
  //
  // Les compétences lancées SUR UNE CASE y passent aussi : leur effet se joue
  // à l'endroit visé, et non sur quelqu'un.
  void NoticeSkills(const ro::sim::World& world, const MapStage& stage,
                    ro::sim::WalkPlanner& planner, std::uint32_t now_ms);

  // LES PROJECTILES DU COUP ORDINAIRE, ET LES COUPS QUI S'ABATTENT. Aucun
  // paquet ne les annonce : le client les fait naître dans le geste d'attaque
  // lui-même — la flèche d'un arc, l'âme d'un Doram, la munition d'une
  // espèce —, et ils volent huit pas de vingt-quatre millisecondes jusqu'à la
  // cible, le retard qu'on donne déjà à sa blessure ; ou bien, pour les espèces
  // de la Mandragore, un sprite s'abat aux pieds de la cible sans voler. Voir
  // `ArrowSpriteOf` et `StrikeSpriteOf`.
  void NoticeArrows(const SoundStage& scene);

  // LES EFFETS QUE SOIXANTE-SIX COMPÉTENCES TIENNENT DE LEUR LUA.
  // 🔴 LA SEULE DE NOS TABLES D'EFFETS QUI NE VIENNE PAS DE L'EXÉCUTABLE. Le
  // client va les chercher dans `skilleffectinfolist.lub`, au DÉFAUT de son
  // répartiteur — et un défaut a l'air d'un défaut, ce qui est la raison pour
  // laquelle rien ne signalait le manque. Voir `ro/render/skill_lua_effect.h`.
  void NoticeLuaEffects(const ro::sim::World& world, const MapStage& stage,
                        ro::sim::WalkPlanner& planner);

  // Les effets que les ÉTATS entretiennent : Sight et les siens. Le client
  // resème l'effet ENTIER sur l'acteur tous les « durée déclarée » tics tant que
  // le bit d'option tient ; voir `OptionEffect`.
  void NoticeOptionEffects(const ro::sim::World& world, const MapStage& stage,
                           ro::sim::WalkPlanner& planner, std::uint32_t now_ms);

  // Ce qu'un état reçu en direct montre une fois : Hiding, le silence, la course.
  void NoticeStateChanges(const ro::sim::World& world, const MapStage& stage,
                          ro::sim::WalkPlanner& planner, std::uint32_t now_ms);

  // Les effets qu'un ÉTAT NOMMÉ joue quand il est posé, et quand il tombe.
  //
  // ⚠ CE N'EST PAS LE CHEMIN DES EFFETS D'OPTION. Ceux-là se cherchent par un
  // bit du mot d'option et s'entretiennent tant qu'il tient ; ceux-ci se
  // cherchent par l'identifiant d'état du serveur, et ne se jouent qu'aux DEUX
  // instants où cet état change — la donnée les fait durer ensuite.
  //
  // Trente-huit états de quatrième classe n'ont que ce chemin-là.
  void NoticeStateEffects(const ro::sim::World& world, const MapStage& stage,
                          ro::sim::WalkPlanner& planner, std::uint32_t now_ms);

  // COUPE un effet qu'un acteur porte, désigné par son NUMÉRO.
  // 🔴 LE CLIENT D'ORIGINE RETIRE CE QU'UN ÉTAT A POSÉ quand cet état tombe,
  // quoi que l'effet soit : un fichier, des nœuds ou une classe.
  void StopById(std::uint32_t effect_id, std::uint32_t actor_id);
  // Vrai si ce porteur porte encore l'effet semé sous cet identifiant, par son
  // fichier, ses nœuds ou sa classe — le même couple que `StopById`.
  [[nodiscard]] bool Carries(std::uint32_t effect_id, std::uint32_t actor_id) const;
  // UN EFFET QU'UN ÉTAT POSE SUR SON PORTEUR : voir `effect_state_classes.cpp`.
  bool PlayStateEffect(std::uint32_t effect_id, std::uint32_t bearer_id,
                       std::optional<std::int32_t> state_value, const std::array<float, 3>& ground,
                       std::uint32_t now_ms, bool endless);
  // LES CLASSES D'UN ÉTAT RENAISSENT À CHAQUE VALEUR NOUVELLE.
  void RenewStateClasses(const ro::sim::World& world, const MapStage& stage,
                         ro::sim::WalkPlanner& planner, std::uint32_t now_ms);
  // COUPE NET CE QUE LE MONDE A RETIRÉ, comme le client. Avant `Notice`, qui
  // repose ce qu'on a retiré puis reposé.
  void NoticeEffectRemovals(const ro::sim::World& world);

  // LES ÉTATS DONT L'EFFET SE CALCULE SUR LEUR VALEUR.
  // 🔴 ET ILS SE REJOUENT QUAND LA VALEUR CHANGE, non quand l'état change. Un
  // Rolling Cutter passe de un tour à dix sans que l'état tombe une seule
  // fois : comparer les LISTES d'états, comme le reste de ce fichier, laisserait
  // l'anneau du premier tour pour toute la durée.
  //
  // ⚠ ET TOUTE LA FAMILLE PART AVANT QUE LE RANG NE SOIT POSÉ. Le client retire
  // les dix rangs à chaque changement ; sans cela les anneaux s'empileraient, et
  // c'est le genre de défaut qui ne se voit qu'après une minute de combat.
  void PlayStateValueEffects(const ro::sim::World& world, const MapStage& stage,
                             ro::sim::WalkPlanner& planner, std::uint32_t now_ms);

  // LE SECOND CHEMIN DES EFFETS D'ÉTAT : celui du REBÂTISSAGE.
  // Au rebâtissage de son sprite, le client parcourt la liste d'états du JOUEUR
  // et les repose tous d'un coup sur le sprite neuf. Chez le client natif cela arrive
  // à l'entrée, au changement de classe, au changement de costume ; chez nous,
  // le seul instant équivalent est la première image où l'acteur du joueur
  // existe.
  //
  // ⚠ ET LE CLIENT NATIF SE GARDE DE DOUBLER : vingt et une de ses créations
  // sont gardées par un « l'acteur porte-t-il déjà cet effet ? », parce que le message 110 a
  // déjà posé la plupart de ces effets à l'arrivée des paquets d'état. On tient
  // la même garde autrement, et sans lire l'écran : ce que la table de la POSE
  // porte déjà n'est pas reposé ici.
  void PlayRebuildStateEffects(ro::sim::StatusEffectId state, std::uint32_t actor_id,
                               std::optional<std::int32_t> state_value, bool sprite_rebuilt,
                               const std::array<float, 3>& ground, std::uint32_t now_ms);

  // Les effets de CHAPEAU que les acteurs portent : un costume, une monture,
  // une aura d'événement. ⚠ NI UN ÉTAT NI UN EFFET D'OPTION : ce numéro est un
  // numéro de la table des effets de chapeau, `hat_effects.yml`, et le confondre ferait
  // jouer l'effet d'un état homonyme. Comme les effets d'option, ils ne
  // s'annoncent qu'une fois : le compte du temps les fait durer tant que le
  // costume est porté.
  void NoticeHatEffects(const ro::sim::World& world, const MapStage& stage,
                        ro::sim::WalkPlanner& planner, std::uint32_t now_ms);
  // La fiche d'un effet de chapeau, ou nul. La table se lit à la première
  // demande et son échec est RETENU : une installation qui n'a pas ces deux
  // fichiers ne les aura pas davantage au costume suivant.
  [[nodiscard]] const ro::render::HatEffect* HatEffectFor(std::uint16_t hat_effect_id);

  // Ce que les UNITÉS POSÉES entretiennent sous elles, tant qu'elles tiennent
  // leur case : un mur de feu, un sanctuaire, un portail.
  //
  // ⚠ CE N'EST PAS L'EFFET DE LEUR POSE, qui se joue une fois et que
  // `NoticeSkills` lance. Le client sépare les deux moments et les range dans
  // deux fonctions ; une unité peut n'avoir que l'un, que l'autre, ou les deux.
  // C'est ce chemin-ci — et lui seul — qui donne une image à Firewall.
  //
  // Comme les effets d'état, ceux-là ne s'annoncent pas : rien dans le protocole
  // ne dit de les rejouer, et c'est le compte du temps qui les fait durer.
  void NoticeSkillUnitVisuals(const ro::sim::World& world, const MapStage& stage,
                              std::uint32_t now_ms);

  // CE QU'UN WARP MONTRE AU SOL : l'effet 321, une fois par acteur. Voir
  // `effect_warp_view.cpp`, qui dit pourquoi ce n'est pas un sprite.
  void NoticeWarpActors(const ro::sim::World& world, const MapStage& stage, std::uint32_t now_ms);

  // LES TRAITS DE DEVOTION, retendus à chaque liste que le serveur annonce :
  // voir `effect_devotion_view.cpp`.
  void NoticeDevotionLinks(const ro::sim::World& world, const MapStage& stage,
                           ro::sim::WalkPlanner& planner, std::uint32_t now_ms);

  // Les unités dont le DERNIER appel vient de semer l'effet.
  // Le client joue le son d'entretien d'une unité dans le même souffle qu'il en
  // resème l'image, et les deux doivent donc battre ensemble. Mais le son ne
  // sait pas lire un effet, et l'effet ne connaît pas l'audio : c'est `app` qui
  // porte l'un à l'autre, comme il route déjà les intentions de l'interface.
  [[nodiscard]] std::span<const std::uint32_t> JustSownUnits() const noexcept {
    return just_sown_units_;
  }

  // Joue un effet PAR SON NUMÉRO, à un endroit donné.
  //
  // C'est le même chemin que celui d'un effet annoncé par le serveur — fichier
  // d'abord, patron de nœuds ensuite —, mais à une place qu'on choisit. Les
  // empreintes d'un coureur s'en servent : elles se posent là où il ÉTAIT.
  //
  // `owner_id` ATTACHE l'effet à un acteur, fichier et troupe : ses semis à
  // venir suivent alors le porteur, comme la boule de feu de Sight suit le
  // mage qui marche. Zéro le pose là où il naît.
  //
  // 🔴 `endless` EN FAIT UNE AURA. Le client déclare la durée de chaque effet,
  // et `99999999` y est son écriture de l'infini : ces effets-là vivent jusqu'à
  // ce qu'on les retire. Chez nous, cela veut dire REBOUCLER le fichier tant
  // que son porteur est là — un `.str` d'aura fait deux secondes — et
  // s'éteindre avec lui. Il faut alors un `owner_id`, sinon rien ne dirait
  // quand s'arrêter.
  // ⚠ ET `life_floor_ticks` QUAND L'EFFET ACCOMPAGNE UNE ACTION DE DURÉE
  // CONNUE : le patron d'un effet donne la vie d'une bouffée, alors qu'un
  // cercle d'incantation dure ce que dure l'incantation. Zéro laisse le patron
  // décider. Ne vaut que pour la troupe — un fichier a sa propre durée, et
  // c'est `endless` qui la déborde. Voir `EffectNodePool::LiveAtLeast`.
  // ⚠ ET `announce_sound` À FAUX POUR UNE RELANCE. Un effet qui DURE se resème
  // — voir `NoticeSkills` —, et chaque semis réclamerait son bruit : le son
  // d'incantation partait alors en boucle, une fois par tour de cercle. Le
  // client ne le joue qu'à l'entrée en scène.
  // ⚠ ET L'ÉCHELLE DE L'INSTANCE avec le second paramètre de l'objet du RSW —
  // voir `EffectNodesInPlay::PlayArcRibbons` —, le POINT VISÉ et le PARAMÈTRE
  // D'INSTANCE, que la troupe lit — voir `EffectNodesInPlay::PlayProcedural`.
  bool PlayById(std::uint32_t effect_id, glm::vec3 ground_position, std::uint32_t now_ms,
                std::uint32_t owner_id = 0, bool endless = false, std::int32_t life_floor_ticks = 0,
                bool announce_sound = true, std::int32_t life_exact_ticks = 0,
                float instance_scale = 0.0F, float instance_second = 0.0F,
                const std::optional<glm::vec3>& target_position = std::nullopt,
                std::int32_t instance_param = kRawEffectParam);

  // Note qu'un effet vient d'être lancé PAR SON IDENTIFIANT, pour que son bruit
  // parte avec son image.
  //
  // 🔴 LE COUP D'UNE COMPÉTENCE ET UNE UNITÉ POSÉE APPELLENT CECI : le son d'une
  // COMPÉTENCE (table du client ou onde du Lua) ou d'une UNITÉ et celui de
  // l'EFFET qu'elle pose sont deux étages distincts, qui n'ont aucun fichier en
  // commun — rien ne double. Le bâtisseur d'effet sonne à son tic zéro, et le
  // tic d'unité sonne à part.
  //
  // ⚠ L'INCANTATION, ELLE, PASSE BIEN PAR ICI, et c'est la seule voie qu'elle
  // ait : `ActorSounds::PlaySkills` ne connaît que le coup reçu et la compétence
  // sans dégâts. Sans cela, aucune incantation ne s'entendait. `bearer_id` : qui le porte.
  void NoteLaunched(std::uint32_t effect_id, std::array<float, 3> ground,
                    std::uint32_t bearer_id = 0);

  // Ce qui vient d'être lancé, que l'appelant EMPORTE.
  //
  // La liste se vide en partant : un effet ne sonne donc qu'une fois, sans que
  // personne ait à penser à l'oublier. C'est ce qui la distingue de
  // `JustSownUnits`, dont le semeur maîtrise le rythme.
  [[nodiscard]] std::vector<LaunchedEffect> TakeLaunchedEffects() {
    return std::exchange(per_map_.just_launched, {});
  }
  // Et les CHEMINÉES qu'un effet annoncé a demandées : leur place, leur porteur.
  [[nodiscard]] std::vector<std::pair<glm::vec3, std::uint32_t>> TakeChimneys();

  // L'INSTANT JUSQU'OÙ UNE COMPÉTENCE A DEMANDÉ LE TREMBLEMENT D'ÉCRAN depuis
  // le dernier passage, que l'appelant EMPORTE : c'est la caméra qui tremble.
  [[nodiscard]] std::optional<std::uint32_t> TakeScreenQuake() {
    const std::optional<std::uint32_t> classe = classes_.TakeScreenQuake();
    const std::optional<std::uint32_t> suite = std::exchange(screen_quake_until_ms_, std::nullopt);
    return classe.has_value() ? std::max(*classe, suite.value_or(0U)) : suite;
  }

  // UN EFFET QUI CHANGE L'ACTEUR au lieu de se dessiner. L'appelant les EMPORTE,
  // et les inscrit sur l'acteur ou les en RETIRE : voir `SettleActorEffects`.
  struct ActorEffectToRecord {
    std::uint32_t actor_id = 0;
    std::uint32_t effect_id = 0;
    std::uint32_t started_ms = 0;
    std::optional<std::uint16_t> posed_by_state;  // l'état qui le pose, s'il y en a un
    bool withdrawn = false;                       // la levée de cet état le retire
  };
  [[nodiscard]] std::vector<ActorEffectToRecord> TakeActorEffects() {
    return std::exchange(actor_effects_to_record_, {});
  }
  // UN EFFET QUI SE DESSINE ET CHANGE AUSSI SON PORTEUR s'inscrit sur lui en plus,
  // sans que `Notice` le redessine ; `withdrawn` l'en retire. Voir `effect_state_classes.cpp`.
  void AlsoChangeBearer(std::uint32_t effect_id, std::uint32_t bearer_id, std::uint32_t started_ms,
                        bool withdrawn);

  // FAIT NAÎTRE LES EFFETS QUE LES NŒUDS ONT DEMANDÉS en se peignant — voir
  // `ro/render/effect_spawn_request.h`. À appeler après le `Drawn` de la troupe,
  // qui recueille les demandes, et avant `TakeActorEffects`, qui emporte celles
  // qui changent un acteur. Voir `effect_spawn_view.cpp`.
  void SowRequestedEffects(const ro::sim::World& world, const MapStage& stage,
                           ro::sim::WalkPlanner& planner, std::uint32_t now_ms);

  // COMBIEN DE FOIS CE SERVEUR A DEMANDÉ CHAQUE IDENTIFIANT. C'est le relevé
  // que seule une session de jeu peut prendre, et c'est le seul.
  //
  // ⚠ IL NE DIT PAS SI L'EFFET S'EST MONTRÉ, et c'est délibéré. `roc-cli
  // effect juge` le dit déjà pour les 2 042 identifiants du client, hors jeu,
  // de façon reproductible et AVEC SA CAUSE. Ce qu'aucune lecture ne peut
  // donner, c'est la liste de ce que ce serveur-ci ENVOIE ; les deux relevés se
  // croisent après coup, et chacun répond à ce qu'il sait.
  struct EffectAsked {
    std::uint32_t effect_id = 0;
    std::size_t asked = 0;
  };
  // Par identifiant croissant — l'ordre d'une table, pas celui du hasard d'une
  // partie, pour que deux sessions se comparent.
  [[nodiscard]] std::vector<EffectAsked> AskedEffects() const {
    std::vector<EffectAsked> rendered;
    rendered.reserve(asked_.size());
    for (const auto& [each_effect_id, tally] : asked_) {
      rendered.push_back(tally);
    }
    return rendered;
  }

  // Les bruits des effets NOMMÉS semés depuis le dernier passage, que
  // l'appelant EMPORTE.
  //
  // ⚠ UN SON N'ACCOMPAGNE PAS TOUS LES SEMIS d'un effet infini. Il ne se rejoue
  // à chaque tour que si sa fiche le dit — neuf fiches sont infinies, sonnent,
  // et ne bouclent pas leur son. Le rejouer pour elles ferait battre un bruit
  // que le client ne fait entendre qu'une fois.
  [[nodiscard]] std::vector<SownNamedSound> TakeSownNamedSounds() {
    std::vector<SownNamedSound> sounds = std::exchange(per_map_.just_sown_named_sounds, {});
    std::ranges::move(classes_.TakeSounds(), std::back_inserter(sounds));
    return sounds;
  }

  // Joue la SUITE d'une compétence : chaque effet que le client fait naître à
  // ce moment, à son porteur. Ne montre rien de ce que nous ne savons pas
  // dessiner, et le dit. `effect_param` est le paramètre que le client pose
  // dans une instance qui le reçoit — voir `EffectParamOfStrike` et
  // `EffectClock::param` ; `argument` est celui du paquet, le niveau ou le
  // MONTANT, dont une étape peut dépendre (voir `SkillSuiteStep`).
  //
  // 🔴 `moment` DIT DE QUEL CHEMIN L'APPEL VIENT, et ce n'est pas un détail de
  // confort : voir `SkillMoment`. Une compétence sans suite à ce moment ne
  // montre rien, et c'est une réponse.
  void PlaySkillOn(const ro::sim::World& world, const MapStage& stage,
                   ro::sim::WalkPlanner& planner, const ro::sim::Actor& target,
                   std::uint16_t skill_id, std::uint32_t caster_id, std::uint32_t started_ms,
                   std::int32_t effect_param, SkillMoment moment, std::int32_t argument,
                   std::uint8_t damage_type = 0);

  // Lance un effet à un endroit FIXE du monde, que personne ne porte. Faux si
  // le fichier est introuvable ou illisible : c'est à l'appelant de le dire,
  // lui seul sait à qui.
  // `owner_id` ATTACHE l'effet à quelqu'un : il suit alors son porteur d'une
  // image à l'autre. Zéro le laisse où il naît, ce qui est le cas de presque
  // tous les appelants.
  // `loops_with` le fait REBOUCLER tant que cet acteur est là — voir
  // `PlayLooping`, qui est le même geste pour un effet qu'on nomme par son
  // fichier plutôt que par son numéro.
  bool Play(std::string_view path, glm::vec3 ground_position, std::uint32_t now_ms,
            std::uint32_t owner_id = 0, std::uint32_t loops_with = 0,
            std::uint32_t from_effect_id = 0);

  // Le même, pour un effet qui doit DURER PLUS QUE SON FICHIER : il REJOUE le
  // sien tant que `while_actor` est dans le monde, et s'éteint avec lui. Faux
  // si le fichier ne se lit pas.
  //
  // 🔴 UNE BOUCLE, ET NON UN RESEMIS. Relancer un `.str` laisse un trou d'une
  // image entre les deux tours ; recouvrir les deux montre un FLASH, le mélange
  // des effets étant additif. Les deux se lisent comme un clignotement
  // régulier, et c'est ce que la colonne d'un butin faisait. Le client, lui,
  // reboucle : voir `ro::render::EffectLoopFrameAt`.
  //
  // `lift_pixels` est le relèvement d'ancre de CET effet — voir
  // `ro::render::DrawnEffect::lift_pixels`. Un effet qui touche le sol en
  // demande zéro.
  //
  // ⚠ IL NE SUIT PAS SON ACTEUR DE LUI-MÊME, à la différence d'un effet PORTÉ :
  // `while_actor` ne lui sert qu'à savoir quand s'arrêter. Qui veut le déplacer
  // le repose par `MoveLooping`.
  [[nodiscard]] bool PlayLooping(std::string_view path, glm::vec3 ground_position,
                                 std::uint32_t now_ms, float lift_pixels, std::uint32_t while_actor,
                                 bool behind_actors);

  // REPOSE les effets qui bouclent avec cet acteur.
  //
  // ⚠ CE N'EST PAS `actor_id`, QUI FAIT SUIVRE UNE CASE AU SOL. Ce qui passe
  // par ici a une HAUTEUR — un butin qui jaillit vole —, et seul son appelant
  // la connaît.
  void MoveLooping(std::uint32_t while_actor, glm::vec3 ground_position);

  // L'IMPACT D'UN COUP DE COMPÉTENCE, un par frappe et sur la CIBLE.
  //
  // Le client ne le joue pas au geste mais à chaque message de dégâts, à
  // l'instant où le chiffre paraît : dix flèches de Cold Bolt, dix fumées de
  // froid. Quel effet, c'est `ro::render::SkillHitEffect` qui le dit.
  //
  // ⚠ CHAQUE IMPACT NAÎT SUR LA CIBLE TELLE QU'ELLE EST ALORS, et non là où le
  // coup est parti : dix frappes durent deux secondes, et ce qui les reçoit
  // recule pendant ce temps.
  void NoticeHitEffects(const SoundStage& scene);

  // LES GARDES D'AUTO COUNTER QUE LE COUP DÉPENSE. Appelée par la précédente,
  // dont elle est le seul chemin à naître sur le FRAPPEUR — voir
  // `ro/render/auto_counter.h`.
  void SpendAutoCounterGuards(const SoundStage& scene);

  // Ce qu'il faut peindre à cet instant ; les effets terminés s'en vont.
  //
  // Un effet PORTÉ suit son acteur d'une image à l'autre, comme le nombre d'un
  // coup : le serveur le joue sur quelqu'un, et ce quelqu'un peut marcher. Un
  // porteur disparu l'emporte, ou le laisse finir là où il était — voir
  // `Playing::dies_with_actor`.
  [[nodiscard]] std::span<const ro::render::DrawnEffect> Drawn(std::uint32_t now_ms,
                                                               const MapStage& stage,
                                                               const ro::sim::World& world,
                                                               ro::sim::WalkPlanner& planner);

  // CEUX QUI SE PEIGNENT AVANT LES CORPS — puis PAR-DESSUS TOUT, voir
  // `Playing::over_everything` —, rangés par le dernier `Drawn`.
  //
  // ⚠ IL FAUT DONC AVOIR APPELÉ `Drawn` À CETTE IMAGE. L'appelant le fait déjà
  // — il dessine ce lot avant les acteurs et l'autre après —, et un lot vide est
  // ce que rend une image où rien n'est passé derrière.
  [[nodiscard]] std::span<const ro::render::DrawnEffect> DrawnBehindActors() const {
    return behind_;
  }
  [[nodiscard]] std::span<const ro::render::DrawnEffect> DrawnOverEverything() const {
    return over_;
  }
  // ET CEUX QU'UNE MÉTHODE NOMMÉE COUCHE AU SOL, rangés eux aussi par `Drawn` :
  // ce sont des quads du monde, et non des panneaux.
  [[nodiscard]] std::span<const ro::render::DrawnWorldQuads> LaidOnGround() const {
    return laid_lots_;
  }

  using NamedActorView = EffectActorView;
  // LA VUE DE CETTE IMAGE, à poser avant les semis : un angle à l'écran ne se
  // mesure qu'avec elle, et un effet couché tourne avec son cap.
  void SeeScene(const ro::render::Camera& camera, NamedActorView actor_view) {
    camera_ = camera;
    actor_view_ = std::move(actor_view);
    classes_.NoticeHovers(actor_view_.hover_pixels);
  }

  [[nodiscard]] std::size_t playing() const noexcept { return per_map_.playing.size(); }

 private:
  // UNE ÉTAPE de la suite d'une compétence : un effet, à son porteur.
  void PlaySkillEffect(const ro::sim::World& world, const MapStage& stage,
                       ro::sim::WalkPlanner& planner, const ro::sim::Actor& target,
                       std::uint16_t skill_id, std::uint32_t caster_id, std::uint32_t started_ms,
                       std::int32_t effect_param, std::int32_t skill_level, SkillMoment moment,
                       const ro::render::SkillSuiteStep& step);

  // LE GESTE UNIQUE PAR LEQUEL UN IDENTIFIANT DU SERVEUR ENTRE ICI. Les sept
  // chemins qui en reçoivent un commencent tous par résoudre son fichier ; le
  // comptage tient à ce geste et non à une liste de sites tenue à la main.
  //
  // ⚠ LA PREMIÈRE POSE ÉTAIT SUR `PlayById`, QUI N'EST PAS UN ENTONNOIR : deux
  // appelants, les empreintes et les chapeaux, et aucun paquet du serveur. Une
  // partie entière a rendu zéro pendant qu'un effet se semait.
  [[nodiscard]] std::string AskedEffectFile(std::uint32_t effect_id, std::uint32_t now_ms) {
    NoteAsked(effect_id);
    return ro::render::EffectFile(effect_id, now_ms);
  }

  void NoteAsked(std::uint32_t effect_id) {
    EffectAsked& tally = asked_[effect_id];
    tally.effect_id = effect_id;
    ++tally.asked;
  }

  // Sème l'effet entretenu d'une unité et rend l'ATTENTE, en millisecondes,
  // avant de le resemer. ZÉRO quand rien n'a pu être semé : l'appelant cesse
  // alors d'essayer pour cette unité.
  //
  // ⚠ `first_sow` DIT SI L'UNITÉ VIENT D'APPARAÎTRE : ses nœuds procéduraux ne
  // naissent qu'à ce moment-là quand un fichier les accompagne, parce que le
  // client ne les relance jamais — voir le corps.
  //
  // 🔴 `unit_instance_id` ACCROCHE LES NŒUDS À L'UNITÉ, et c'est ce qui les fait
  // PARTIR AVEC ELLE. Un nœud sans ancre ne meurt que de sa vie, et celle d'un
  // mur vaut dix mille tics : le mur restait à l'écran bien après que le serveur
  // l'avait levé — que ce soit sa durée qui s'achève ou son compte de coups qui
  // s'épuise, le client natif l'efface, et nous le laissions.
  [[nodiscard]] std::uint32_t SowUnitVisual(const ro::render::SkillUnitVisualEntry& visual,
                                            glm::vec3 ground_position, std::uint32_t now_ms,
                                            bool first_sow, std::uint32_t unit_instance_id);

  // La fiche d'un effet NOMMÉ, ou nul. La table se lit à la première demande,
  // et son échec est RETENU comme son succès : une installation qui n'a pas le
  // fichier ne l'aura pas davantage à la prochaine unité posée.
  [[nodiscard]] const ro::formats::NamedEffect* NamedEffectFor(std::string_view name);

  // Joue les effets d'une unité pour un MOMENT — pose ou levée : les nommés, que
  // leur fiche retarde, et à la pose ceux par numéro, des classes nées sur sa case.
  void PlayNamedUnit(std::uint32_t unit_instance_id, std::uint16_t unit_id,
                     const ro::sim::CellPosition& cell, ro::render::SkillUnitNamedMoment moment,
                     std::uint32_t now_ms, std::int32_t level = -1,
                     const MapStage* stage = nullptr);

  // Joue les effets qu'une COMPÉTENCE désigne par un nom, pour le moment que dit
  // le slot. Rend VRAI si cette compétence en a pour ce moment-là — auquel cas
  // l'autre chemin ne doit pas jouer, le client ne consultant le registre qu'au
  // DÉFAUT de son répartiteur.
  //
  // `source` est le lanceur, pour les effets que la table fait naître sur lui ;
  // sans lui, ceux-là ne naissent pas. `level` remplit le trou des noms qui le
  // demandent.
  [[nodiscard]] bool PlayNamedSkill(const MapStage& stage, ro::sim::WalkPlanner& planner,
                                    std::uint32_t actor_id, const ro::sim::Actor& actor,
                                    std::uint16_t skill_id, std::uint8_t slot,
                                    std::uint32_t started_ms,
                                    const ro::sim::Actor* source = nullptr, std::int32_t level = 0);

  // Le même, pour un moment qui reçoit la SOURCE et la CIBLE : chaque effet naît sur celle que la
  // table lui donne. `fallback_unit` : l'unité au sol frappée quand la cible n'est pas un acteur.
  void PlayNamedSkillOnPair(const MapStage& stage, ro::sim::WalkPlanner& planner,
                            std::uint16_t skill_id, std::uint8_t slot, const ro::sim::Actor* source,
                            const ro::sim::Actor* target, std::uint32_t started_ms,
                            std::uint32_t attack_motion_ms,
                            const ro::sim::SkillUnit* fallback_unit = nullptr);

  // L'IMPACT D'UNE FRAPPE, rang `hit_index` de son coup : ce que la compétence nomme à la place
  // de l'effet du message. Vrai si elle en nomme. Sans `target`, l'unité au sol de `unit_cell`.
  [[nodiscard]] bool PlayNamedSkillOnHit(const MapStage& stage, ro::sim::WalkPlanner& planner,
                                         std::uint16_t skill_id, const ro::sim::Actor* source,
                                         const ro::sim::Actor* target, std::uint32_t hit_index,
                                         std::uint32_t attack_motion_ms, std::uint32_t landed_ms,
                                         const std::optional<ro::sim::CellPosition>& unit_cell);

  // LA COMPÉTENCE POSÉE SUR UNE CASE par `caster`, nul quand il est hors de vue :
  // voir `effect_ground_skill_view.cpp`.
  void PlayGroundSkillPacket(const MapStage& stage, ro::sim::WalkPlanner& planner,
                             const ro::sim::Actor* caster, std::uint32_t caster_id,
                             const ro::sim::GroundSkillUse& use);

  // LE MOMENT D'UNE ACTION NOMMÉE, et ce que ses méthodes en lisent. Voir
  // `effect_named_skill.cpp`.
  struct NamedSkillMoment {
    std::uint16_t skill_id = 0;
    std::uint8_t slot = 0;
    const ro::sim::Actor* source = nullptr;
    // Celui qui reçoit : l'acteur du moment pour une incantation.
    const ro::sim::Actor* target = nullptr;
    // La CASE VISÉE d'une compétence posée sur une case.
    std::optional<ro::sim::CellPosition> target_cell;
    const ro::sim::SkillUnit* fallback_unit = nullptr;
    std::int32_t level = 0;
    std::uint32_t hit_index = 0;
    // La cadence du coup que le paquet annonce ; aucune pour l'emploi d'une
    // compétence, que personne n'accélère.
    std::optional<std::uint32_t> attack_motion_ms;
    std::uint32_t started_ms = 0;
  };
  bool PlayNamedMoment(const MapStage& stage, ro::sim::WalkPlanner& planner,
                       const NamedSkillMoment& moment);

  // Inscrit un semis nommé, ou coupe ce qui joue sous ce nom. Le geste est le
  // même pour une unité et pour une compétence : seul le propriétaire change.
  // ⚠ L'ANCRAGE DÉCIDE SI L'EFFET SUIT SON PORTEUR. Une boucle d'état attachée
  // à quelqu'un marche avec lui ; un effet laissé à une case y reste. Les
  // confondre laisserait un buff au sol dès que le joueur bouge.
  // `start_delay_ms` remplace le délai de la fiche quand il n'est pas négatif.
  // `sowing` porte ce que la méthode fait de l'image — son nom et son délai
  // n'y sont pas lus.
  void PlayNamedEntry(std::string_view name, ro::render::NamedEffectVerb verb,
                      std::uint32_t owner_id, bool from_unit, const ro::sim::CellPosition& cell,
                      ro::render::NamedEffectAnchor anchor, bool after_vanish, std::uint32_t now_ms,
                      std::int32_t start_delay_ms = ro::render::kRecordStartDelay,
                      const glm::vec3& offset = glm::vec3(0.0F),
                      const ro::render::NamedSowing& sowing = {});

  // UN SEMIS D'UNE MÉTHODE DE COMPÉTENCE, tirages faits : `name` est celui de
  // la fiche, `cell` et `offset` le point où il naît.
  void PlayNamedSkillEntry(const ro::render::SkillNamedEffectEntry& entry, std::string_view name,
                           std::uint32_t owner_id, const ro::sim::CellPosition& cell,
                           ro::render::NamedEffectAnchor anchor, std::int32_t level,
                           std::uint32_t started_ms, const glm::vec3& offset,
                           const ro::render::NamedSowing& sowing);

  // LES ÉTATS NOMMÉS QUI LISENT LEUR VALEUR : leur moment de pose se rejoue à
  // chaque valeur nouvelle, l'état tenant toujours. Voir
  // `ro::render::StateNamedEntryReadsValue`.
  void PlayStateNamedValues(const ro::sim::World& world, const MapStage& stage,
                            ro::sim::WalkPlanner& planner, std::uint32_t now_ms);

  // LES CHARGES qu'un état compte au-dessus de la tête de son porteur — voir
  // `ro/render/charge_count_effect.h`.
  void PlayChargeCount(std::uint32_t actor_id, std::int32_t count, std::uint32_t now_ms);

  // UN DÉCALAGE DANS LE PLAN DE LA VUE, dans le monde : voir
  // `Playing::view_offset`. Nul sans vue posée.
  [[nodiscard]] glm::vec3 ViewOffsetInWorld(glm::vec2 view_offset) const;
  // Couche un effet au sol : ses quads du monde rejoignent `laid_quads_`, un
  // lot par quad. `BindLaidLots` rattache ensuite chaque lot à son quad, une
  // fois le tableau fini de grandir.
  void LayOnGround(const EffectMapMemory::Playing& playing, std::int32_t frame,
                   const MapStage& stage);
  void BindLaidLots();

  // Vrai quand le bras de cet effet est celui que la méthode prend : la cible
  // porte, ou non, l'effet nommé de la porte ; ou le tirage tombe du bon côté.
  [[nodiscard]] bool NamedGateOpens(const ro::render::SkillNamedEffectEntry& entry,
                                    std::uint32_t target_id);

  // Sème ceux dont l'heure est venue, et réinscrit les INFINIS pour leur tour
  // suivant. Rend les instances qui viennent de semer.
  void SowDueNamed(const MapStage& stage, std::uint32_t now_ms);

  // Les types des mémoires de la carte, sous leur nom court.
  using Playing = EffectMapMemory::Playing;
  using CastRing = EffectMapMemory::CastRing;
  using PendingHit = EffectMapMemory::PendingHit;
  using SownUnit = EffectMapMemory::SownUnit;
  using PendingNamed = EffectMapMemory::PendingNamed;

  // Le fichier qu'une fiche montre vraiment, ou vide. Un seul endroit en
  // décide : la période et le semis le liraient sinon chacun de leur côté.
  [[nodiscard]] std::string HatFileOf(const ro::render::HatEffect& record);
  // Combien de temps une fiche tient avant d'être resemée. Zéro pour une fiche
  // dont rien ne se joue.
  [[nodiscard]] std::uint32_t HatPeriodMs(const ro::render::HatEffect& record);

  // LES DÉFAUTS SONT CEUX DU CLIENT : les effets s'affichent, l'allègement est
  // coupé. Voir `gamesettings.lub`, entrées [3] et [36].
  bool effects_shown_ = true;
  bool minimal_effects_ = false;
  std::uint32_t player_id_ = 0;
  ro::render::EffectCatalog* catalog_ = nullptr;
  // Les effets PROCÉDURAUX, qui n'ont pas de fichier. Le même répartiteur
  // sert les deux familles : une compétence n'a pas à savoir de laquelle
  // relève son effet.
  EffectNodesInPlay* nodes_ = nullptr;
  // Les tampons de `Drawn`, refaits à chaque image : ils gardent leur capacité.
  std::vector<ro::render::DrawnEffect> drawn_;
  std::vector<ro::render::DrawnEffect> behind_, over_;
  // 🔴 LE NIVEAU DE JOURNAL SUIT LE VERDICT, PAS L'ÉVÈNEMENT, et c'est la seule
  // règle de ce fichier : `debug` pour ce qui MARCHE — l'effet naît, il est
  // semé, l'impact est posé —, `warning` pour ce qui pourrait ne RIEN montrer —
  // rien en table, en table mais rien semé, ni fichier ni patron, cible hors de
  // la scène. Qui juge un effet côte à côte filtre sur `warning`, et doit n'y
  // trouver que des questions.

  // Les numéros sans fichier déjà signalés, pour ne les dire qu'une fois.
  std::vector<std::uint32_t> reported_;
  // ⚠ ET CELLES QUE LA TABLE NE NOMME PAS À CE MOMENT-LÀ, qui sortaient EN
  // SILENCE. « Je ne vois rien » ne distinguait pas une compétence absente de
  // la table d'un paquet qui n'arrive jamais — et les deux se corrigent à des
  // endroits opposés. La paire est (compétence, moment).
  std::vector<std::pair<std::uint16_t, std::uint8_t>> reported_without_table_;
  // Les compétences dont on a déjà dit quel IMPACT elles posent. Ce chemin-là
  // ne disait rien du tout, et c'est ce qui a rendu Bash impossible à juger sur
  // un journal : son visuel ne passe QUE par lui.
  std::vector<std::uint16_t> reported_hits_;
  // Et celles dont on a déjà dit D'OÙ leur effet naît, par compétence et par
  // moment : un sort de zone frappe dix fois par seconde, et la même ligne dix
  // fois par seconde ne dit rien de plus que la première.
  std::set<std::pair<std::uint16_t, std::uint8_t>> reported_births_;
  // Le tirage des points visés sur un anneau : chaque éclat de Jack Frost tire
  // son angle, et deux étapes du même instant ne doivent pas tirer le même.
  std::uint32_t aim_generator_ = 1;
  // Le dernier tirage pair ou impair d'une méthode nommée, que son second bras
  // relit.
  bool named_draw_is_odd_ = false;
  // La vue de l'image, posée par `SeeScene`, et ce que les semis nommés
  // demandent aux acteurs.
  std::optional<ro::render::Camera> camera_;
  NamedActorView actor_view_;
  // Les effets couchés de la dernière image, et les lots qui les peignent.
  std::vector<ro::render::EffectWorldQuad> laid_quads_;
  std::vector<ro::render::DrawnWorldQuads> laid_lots_;
  // Le dernier point tiré par une étape dispersée, que la suivante peut
  // reprendre : les deux éclats d'une paire de Judex tombent ensemble.
  ro::render::SkillSuiteScatter last_scatter_;
  // Le tremblement demandé et pas encore emporté : voir `TakeScreenQuake`.
  std::optional<std::uint32_t> screen_quake_until_ms_;
  // Les effets d'acteur posés par une suite : voir `TakeActorEffects`.
  std::vector<ActorEffectToRecord> actor_effects_to_record_;
  // Et les compétences dont l'effet d'INCANTATION ne s'est pas dessiné. Une
  // seule ligne par compétence : une incantation muette se répète à chaque
  // lancer, et la deuxième ligne n'apprend rien.
  std::set<std::uint16_t> reported_casts_;
  // Les compétences dont le CHEMIN d'incantation a déjà été tracé. Jumelle de
  // la précédente, et séparée d'elle : l'une dit qu'on n'a rien su montrer,
  // l'autre dit par où l'on est passé pour le découvrir.
  std::set<std::uint16_t> reported_cast_paths_;
  // Et les EFFETS D'IMPACT qui ne se sont pas dessinés. Par effet et non par
  // compétence : plusieurs compétences partagent la gerbe ordinaire, et le
  // trou est dans l'effet.
  std::set<std::uint32_t> reported_hit_effects_;
  // Les coups lus par le chemin Lua — voir `EffectMapMemory::lua_used`.
  ro::sim::StrikeCursor lua_strikes_;
  // Les coups de compétence déjà joués. Voir `ro::sim::StrikeLog`.
  ro::sim::StrikeCursor skill_strikes_;
  // Les compétences posées par un lanceur hors de vue, déjà jouées.
  ro::sim::UnseenGroundSkillCursor unseen_ground_skills_;
  // Les coups dont les impacts sont déjà inscrits : un curseur à part de
  // `skill_strikes_`, parce que l'effet de la compétence et celui de ses
  // frappes ne partent pas au même moment.
  ro::sim::StrikeCursor hit_strikes_;
  // Les coups vus par le tir. Une frappe ne tire qu'une fois.
  ro::sim::StrikeCursor arrow_strikes_;
  // La table des effets de chapeau, lue une fois.
  std::optional<ro::render::HatEffectTable> hat_effects_;
  bool hat_effects_read_ = false;
  EffectDrawGate draw_gate_;

  // La table des effets nommés, lue une fois, et l'échec retenu comme le
  // succès : `named_effects_read_` distingue « pas encore lue » de « lue et
  // absente ».
  std::optional<ro::formats::NamedEffectTable> named_effects_;
  bool named_effects_read_ = false;
  // Les noms demandés en vain, pour ne les dire qu'une fois.
  std::vector<std::string_view> reported_names_;
  ro::vfs::Vfs* vfs_ = nullptr;
  // Celles que le dernier passage vient de semer, pour que leur son parte avec
  // leur image. Un tampon d'image : il garde sa capacité.
  std::vector<std::uint32_t> just_sown_units_;
  // Un `map` et non un `unordered_map` : la sortie se lit triée, et une partie
  // demande quelques centaines d'identifiants — le coût ne se mesure pas.
  std::map<std::uint32_t, EffectAsked> asked_;

  EffectMapMemory per_map_;
  EffectClassesInPlay classes_;
};

}  // namespace moonclient
