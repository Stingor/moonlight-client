#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_catalog.h"
#include "ro/render/effect_renderer.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world_fwd.h"

#include "effects/effect_arrow_shower.h"
#include "world/actor_sounds.h"

namespace moonclient {

// 🔴 CE QUI VAUT POUR UNE SEULE CARTE, que `Forget` remet à neuf d'un seul
// geste. Une mémoire de la carte tenue hors d'ici survit au changement de
// carte, et ce qu'elle a déjà vu empêche l'effet éteint de revenir. Restent
// dehors les tables lues, les avertissements donnés, le compte des
// identifiants demandés, qui vaut pour la séance, les tampons d'image, et
// les curseurs du journal des coups, dont les numéros continuent.
//
// Tenue par `EffectsInPlay`, en un seul membre.
struct EffectMapMemory {
  struct Playing {
    const ro::render::LoadedEffect* effect = nullptr;
    // Qui le porte, ou ZÉRO pour un effet posé à un endroit fixe.
    std::uint32_t actor_id = 0;
    glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
    std::uint32_t started_ms = 0;
    // DE QUI ce semis vient, et sous quel nom, ou zéro et vide. Une levée COUPE
    // certains effets en les désignant par leur nom : sans ces deux champs, on
    // ne saurait pas lesquels vider.
    std::uint32_t named_owner_id = 0;
    std::string_view named_as;
    // L'ACTEUR DONT CET EFFET SUIT LA VIE : il rejoue son fichier tant que
    // celui-ci est là, puis s'éteint. Zéro pour un effet qui joue une fois.
    //
    // ⚠ CE N'EST PAS `actor_id` : celui-là fait SUIVRE l'effet à la case de son
    // porteur, AU SOL. Les confondre clouerait au sol une colonne qui doit
    // monter avec le butin.
    std::uint32_t loops_with_actor = 0;
    // DE COMBIEN CET EFFET-CI SE RELÈVE — voir `DrawnEffect::lift_pixels`.
    //
    // ⚠ EN QUEUE, ET C'EST UNE CONTRAINTE : quatre semis construisent cette
    // structure par liste POSITIONNELLE, et un champ glissé au milieu leur
    // donnerait silencieusement la valeur du voisin. Le compilateur l'a dit ici
    // parce que les types diffèrent ; il se serait tu entre deux entiers.
    float lift_pixels = ro::render::kEffectAnchorLiftPixels;
    // IL SE PEINT AVANT LES CORPS — voir `DrawnEffect::behind_actors`.
    bool behind_actors = false;
    // L'IDENTIFIANT D'EFFET QUI A SEMÉ CETTE INSTANCE, ou zéro.
    //
    // 🔴 SANS LUI, LA COUPE SE TROMPE DE FICHIER. `EffectFile` choisit parmi
    // les variantes d'un effet qui en a plusieurs, et le tirage vient de
    // l'INSTANT : rechercher le fichier au moment de couper ne rend pas celui
    // qu'on a semé. Marquer l'instance supprime la question.
    //
    // ⚠ EN QUEUE, COMME SES DEUX VOISINS, et pour la même raison : quatre semis
    // bâtissent cette structure par liste POSITIONNELLE, la plus longue à six
    // éléments. Un champ glissé avant eux leur donnerait la valeur du voisin.
    std::uint32_t from_effect_id = 0;
    // CE QU'UNE MÉTHODE NOMMÉE FAIT DE L'IMAGE — voir `DrawnEffect::spin_degrees`
    // pour un effet debout, `ro::render::LaidEffectPose` pour un effet couché.
    // ⚠ EN QUEUE, pour la même raison que les trois d'au-dessus.
    float turn_degrees = 0.0F;
    float scale = 1.0F;
    bool flip_x = false;
    bool flip_y = false;
    bool laid_on_ground = false;
    // UN DÉCALAGE DANS LE PLAN DE LA VUE, en unités du monde, autour des pieds
    // du porteur : l'abscisse vers la droite de l'écran, l'ordonnée vers le BAS.
    // Il se recalcule à chaque image, la vue tournant.
    glm::vec2 view_offset{0.0F};
    // L'IMAGE S'ARRÊTE SUR LA DERNIÈRE au lieu de s'effacer, tant que le porteur
    // est là.
    bool holds_last_frame = false;
    // LA ROTATION SUIT LA VUE : chaque degré dont la caméra a tourné depuis la
    // naissance s'ajoute à `turn_degrees` — voir
    // `SkillUnitPoseStep::spin_anchored_to_ground`.
    bool turn_follows_view = false;
    float birth_view_yaw_degrees = 0.0F;
    // IL SE PEINT PAR-DESSUS TOUT, après tous les autres effets — voir
    // `ro::render::StrPaintsOverEverything`.
    bool over_everything = false;
    // IL REBOUCLE SANS FIN, que son porteur soit là ou non : seul le changement
    // de carte l'éteint. Voir `ro::render::StrEffectSetup::loops`.
    bool loops_endlessly = false;
    // Combien de fois le fichier se rejoue à sa fin, et la vie de l'instance en
    // tics, ou zéro pour celle de son fichier.
    std::uint8_t replays = 0;
    std::int32_t life_ticks = 0;
    // IL MEURT QUAND `actor_id` QUITTE LE MONDE, au lieu de finir sa vie à la
    // dernière place de son porteur. Voir `ro::render::StrDiesWithBearer`.
    bool dies_with_actor = false;
    // L'ÂGE, en millisecondes, où il perd le lanceur de case qui le porte, et
    // meurt avec lui où qu'en soit son fichier ; zéro quand rien ne le porte.
    std::uint32_t bearer_dies_after_ms = 0;
    // NÉ D'UN CHAPEAU : la porte de dessin lui retire ses exceptions. Voir
    // `EffectDrawGate::Draws`.
    bool from_hat = false;

    // Vrai une fois ce porteur mort : le lanceur vit tant que son âge
    // n'excède pas sa vie.
    [[nodiscard]] bool LostItsBearerAt(std::uint32_t now_ms) const noexcept {
      return bearer_dies_after_ms != 0 && now_ms - started_ms > bearer_dies_after_ms;
    }
  };

  // UN CERCLE D'INCANTATION EN COURS : quand il a été semé pour la dernière
  // fois, et QUOI resemer. Un lanceur en tient plusieurs — l'aura que la table
  // des incantations donne, et le cercle que l'action de sa compétence pose.
  struct CastRing {
    std::uint32_t sown_ms = 0;
    std::uint32_t effect_id = 0;
    // Faux pour un cercle qui dure de lui-même — un fichier, une gerbe
    // d'écran — : il ne se resème pas, mais une interruption l'éteint aussi.
    bool relaunches = false;
  };

  // Les impacts qui attendent leur frappe. Un coup s'annonce une fois ; ses
  // frappes, elles, se posent l'une après l'autre.
  struct PendingHit {
    std::uint32_t effect_id = 0;
    std::uint32_t target_id = 0;
    std::uint32_t due_ms = 0;
    // L'ACTEUR DONT L'EFFET VISE LA PLACE, quand le client la lui donne ; zéro
    // pour aucune. ⚠ EN QUEUE : les trois d'au-dessus se posent par position.
    std::uint32_t aim_id = 0;
    // LA CASE DE L'UNITÉ AU SOL qui porte l'impact, relevée au coup : le coup
    // qui brise un bloc touche après que le serveur l'a retiré. Vide pour un
    // acteur. Voir `world/struck_place.h`.
    std::optional<ro::sim::CellPosition> unit_cell;
  };

  // Le dernier semis de l'effet ENTRETENU d'une unité posée, et l'attente avant
  // le suivant.
  struct SownUnit {
    std::uint32_t at_ms = 0;
    // ZÉRO veut dire NE PLUS RIEN TENTER : cette unité demande un effet que
    // nous ne savons pas peindre, et le redemander à chaque image coûterait
    // sans jamais rien montrer de plus.
    std::uint32_t period_ms = 0;
    // CE QU'IL FAUT SAVOIR D'ELLE UNE FOIS QU'ELLE N'EST PLUS LÀ : son type dit
    // quels effets sa levée joue, sa case dit où. Le monde ne répond plus.
    std::uint16_t unit_id = 0;
    ro::sim::CellPosition cell{};
  };

  // Un semis d'effet NOMMÉ en attente.
  //
  // Un par NOM, et non un par unité : la fabrique les sème tous à la pose, et
  // c'est la DONNÉE qui les ordonne ensuite — chaque fiche porte son délai de
  // départ et son drapeau d'infini. Un compte par unité les confondrait, et
  // ferait repartir le `_start` à chaque tour du `_loop`.
  struct PendingNamed {
    // QUI L'A SEMÉ : une unité posée, ou un acteur qui a lancé une compétence.
    // Les deux espaces d'identifiants se touchent — une unité au sol EST un
    // acteur chez ce serveur —, et les confondre couperait les effets de l'un
    // en croyant retirer ceux de l'autre.
    std::uint32_t owner_id = 0;
    bool from_unit = true;
    // Pointe dans la table engendrée, dont les chaînes vivent aussi longtemps
    // que le programme.
    std::string_view name;
    // LA CASE EST RETENUE ICI, et non redemandée au monde : les effets de LEVÉE
    // se sèment quand l'unité n'y est plus. Une unité au sol ne bouge pas, si
    // bien que la retenir à la pose ne coûte aucune fidélité.
    ro::sim::CellPosition cell{};
    std::uint32_t at_ms = 0;
    // Faux tant que rien n'a encore été semé : c'est ce qui distingue le
    // PREMIER tour des suivants, et seul le premier fait toujours du bruit.
    bool sown_once = false;
    // Vrai pour un semis de LEVÉE : il ne se purge pas avec son unité,
    // puisqu'il est précisément ce qu'elle laisse derrière elle.
    bool after_vanish = false;
    // Vrai quand l'effet est ATTACHÉ à son porteur et le suit. C'est le cas de
    // 442 des 492 demandes relevées ; les autres sont laissées à une position.
    bool on_actor = false;
    // Ce qui s'ajoute au sol de la case, dans notre repère : le point exact où
    // la méthode l'a posé autour d'elle, tiré une fois pour toutes.
    glm::vec3 offset{0.0F};
    // Ce que la méthode fait de l'image, tiré une fois lui aussi — voir
    // `Playing`.
    float turn_degrees = 0.0F;
    float scale = 1.0F;
    bool flip_x = false;
    bool flip_y = false;
    bool laid_on_ground = false;
  };

  std::vector<Playing> playing;
  // Les effets lancés par leur identifiant depuis le dernier passage du son.
  std::vector<LaunchedEffect> just_launched;
  // Les cheminées demandées, leur place et leur porteur : `TakeChimneys`.
  std::vector<std::pair<glm::vec3, std::uint32_t>> chimneys;
  // Et les bruits des effets NOMMÉS, qui ne se nomment pas par un identifiant.
  std::vector<SownNamedSound> just_sown_named_sounds;
  // L'instant du dernier lancer vu de chaque effet, acteur par acteur : c'est
  // ce qui distingue un effet NOUVEAU de celui qui joue encore.
  //
  // ⚠ LA CLÉ PORTE L'EFFET AUTANT QUE L'ACTEUR. Un acteur en porte plusieurs à
  // la fois ; un témoin par acteur ferait passer le second pour une redite du
  // premier, et le perdrait.
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> noticed;
  // Les cercles d'incantation de chaque lanceur dont l'incantation dure.
  //
  // 🔴 SA PRÉSENCE DIT QUE L'INCANTATION SE SURVEILLE : à sa fin, une
  // interruption éteint ses cercles, ses extras et sa marque, même quand aucun
  // cercle n'a été semé.
  //
  // ⚠ ET QUE LE REPLI S'APPLIQUE. Les cribles qui en
  // décident — un effet nommé, une durée nulle — ne sont calculés qu'à
  // l'apparition du cast ; les refaire à chaque image coûterait deux
  // recherches de table par lanceur, et les oublier relancerait un cercle que
  // le chemin nommé a déjà remplacé.
  std::unordered_map<std::uint32_t, std::vector<CastRing>> cast_ring_sown;
  // LE RÉTICULE QU'UN LANCEUR A POSÉ SUR SA CIBLE, et jusqu'à quand il vit. Le
  // lanceur qui BRONCHE l'éteint sur elle : le client envoie alors à la cible
  // qu'il a mémorisée de détruire son second emplacement, celui du réticule.
  struct LockOnReticle {
    std::uint32_t target_id = 0;
    std::uint32_t ends_ms = 0;
  };
  std::unordered_map<std::uint32_t, LockOnReticle> lock_on_reticles;
  // LE DERNIER COUP POUR LEQUEL CHAQUE ATTAQUANT A DÉJÀ SEMÉ SES CAS
  // PARTICULIERS. C'est notre lecture de la garde d'SR_EARTHSHAKER, qui
  // interroge l'acteur pour savoir s'il porte encore l'effet 888 : ce qu'elle
  // empêche, c'est de resemer à CHAQUE VICTIME d'une même salve.
  std::unordered_map<std::uint32_t, std::uint32_t> hit_extras_sown;
  // Les compétences sans dégâts lues par le chemin Lua ; le coup est dans
  // `lua_strikes_`. Ils sont à lui parce que ce chemin ne remplace aucun des
  // autres — le client consulte son Lua EN PLUS de ce que son switch a fait.
  ro::sim::SupportCursor lua_supports;
  // LES ACTEURS DONT LA GARDE D'AUTO COUNTER EST ARMÉE, et l'instant où elle
  // l'a été. Le client tient ce drapeau sur l'acteur lui-même, que le
  // changement de carte détruit, celui du joueur compris bien qu'il garde son
  // identifiant ; nous le tenons ici parce que `sim` ne connaît pas les
  // effets par leur sens.
  //
  // ⚠ IL SE DÉPENSE, il ne s'oublie pas : une entrée disparaît au premier coup
  // que son acteur porte, jamais par péremption. Voir `ro/render/auto_counter.h`
  // pour les trois gestes du client.
  std::unordered_map<std::uint32_t, std::uint32_t> auto_counter_armed;
  // 🔴 LES FRAPPES EN ATTENTE visent un identifiant, et le joueur garde le
  // sien d'une carte à l'autre : gardées, celles d'une salve reçue au moment
  // du warp rejoueraient leurs impacts sur la carte suivante.
  std::vector<PendingHit> pending_hits;
  // Les coups qui attendent le geste de leur tireur pour partir.
  std::vector<ro::sim::Strike> arrows_waiting;
  // Le même repère pour les compétences lancées sur une case : elles vivent à
  // part, et le même acteur peut frapper et poser dans la même image.
  std::unordered_map<std::uint32_t, std::uint32_t> ground_cast;
  // Les compétences SANS DÉGÂTS déjà jouées, TOUTES : le serveur en envoie
  // souvent deux d'affilée sur la même cible — voir `ro::sim::SupportLog`.
  ro::sim::SupportCursor supports;
  // Les UNITÉS au sol déjà vues : leur effet ne se joue qu'à la pose, et une
  // unité vit bien plus longtemps qu'un effet.
  std::unordered_map<std::uint32_t, std::uint32_t> placed;
  // Et le dernier CHANGEMENT D'APPARENCE vu de chacune : un piège qui se
  // déclenche joue son amorce une fois, à l'instant où il change.
  std::unordered_map<std::uint32_t, std::uint32_t> looked;
  // Et pour les INCANTATIONS annoncées : l'effet naît au début du sort, une
  // seule fois, alors que l'incantation dure.
  std::unordered_map<std::uint32_t, std::uint32_t> casting;
  // Et pour l'ÉTAT D'EMPLOI de chaque lanceur : ce qu'il fait naître en se
  // posant naît une fois par emploi — voir `ro::render::StateEffectsOf`.
  std::unordered_map<std::uint32_t, std::uint32_t> posed_use_states;
  // Le dernier semis d'un effet d'ÉTAT, par acteur : UN compte pour tous ses
  // effets d'état, qui se le partagent. Ceux-là ne s'annoncent pas : c'est ce
  // compte qui les espace.
  //
  // ⚠ IL SURVIT À LA CHUTE D'UN ÉTAT tant que l'acteur garde une option : un
  // état qui tombe et revient avant la durée n'en relance pas un tout de suite.
  // Un mot d'options NUL l'efface — le compteur est alors au plus haut —, et un
  // porteur parti l'emporte. Voir `ro::render::OptionEffectDue`.
  std::unordered_map<std::uint32_t, std::uint32_t> sown_states;
  // Le dernier COMPTE de fronts montants vu sur chaque acteur.
  //
  // ⚠ CE N'EST PAS `sown_states` : celui-là SURVIT à la chute de l'état, par
  // dessein, pour espacer les resemis. Le BRUIT, lui, veut le front montant, et
  // il lui faut donc une mémoire qui suive le bit lui-même.
  std::unordered_map<std::uint32_t, std::uint32_t> seen_options;
  // Les derniers comptes de changements vus sur chaque acteur par
  // `NoticeStateChanges` : fronts montants, fronts descendants, changements
  // d'état. Même règle — un acteur jamais vu ne joue rien.
  struct SeenStateCounts {
    std::uint32_t rises = 0;
    std::uint32_t falls = 0;
    std::uint32_t changes = 0;
    // L'état du corps vu au passage précédent : sortir du gel se lit contre lui.
    std::uint16_t body_state = 0;
  };
  std::unordered_map<std::uint32_t, SeenStateCounts> seen_state_counts;
  // L'AVEUGLEMENT ORDINAIRE A-T-IL ÉTÉ VU SUR LE JOUEUR depuis le dernier noir
  // levé ? Voir `ro::render::AilmentChange::blind_seen`.
  bool blind_seen = false;
  // Le poison, de même : voir `ro::render::AilmentChange::poison_seen`.
  bool poison_seen = false;
  // Les états NOMMÉS déjà vus sur chaque acteur, pour reconnaître la transition.
  //
  // ⚠ LE SERVEUR NE DIT PAS « L'ÉTAT VIENT DE TOMBER » : il donne la liste de ce
  // qui tient. Ce qui est POSÉ et ce qui est RETIRÉ ne se lit qu'en comparant
  // avec le passage précédent, et c'est pourquoi cette mémoire existe. Elle ne
  // retient que les états NOMMÉS : les autres sont des centaines, et les suivre
  // tous coûterait sans rien montrer de plus.
  std::map<std::uint32_t, std::set<ro::sim::StatusEffectId>> named_states;
  // L'INSTANT OÙ UN ÉTAT A POSÉ UN EFFET D'ACTEUR, par porteur et par effet,
  // tant que l'effet peut mourir de lui-même : voir
  // `EffectsInPlay::NoticeStateEffects`.
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> state_actor_effects;
  // CE QU'UN ACTEUR PORTE SANS QU'AUCUN DESSIN LE TIENNE, par porteur et par
  // effet, avec l'instant où l'instance meurt — rien : jusqu'à son retrait. Un
  // effet d'acteur ne vit que dans son mot de rendu, un effet sans figure que
  // dans son instance ; la garde « porte-t-il cet effet ? » d'une suite doit
  // pourtant les voir. Voir `EffectsInPlay::Carries`.
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::optional<std::uint32_t>> carried_marks;
  // LA DERNIÈRE VALEUR vue d'un état qui pose une classe, par porteur ; vide
  // pour un paquet qui ne la portait pas. Voir
  // `EffectsInPlay::RenewStateClasses`.
  std::map<std::pair<std::uint32_t, ro::sim::StatusEffectId>, std::optional<std::int32_t>>
      state_class_values;
  // L'instant du dernier semis de chaque couple porteur/costume.
  std::map<std::pair<std::uint32_t, std::uint16_t>, std::uint32_t> sown_hats;
  // CE QU'UN COSTUME A FAIT NAÎTRE, par couple porteur/identifiant d'effet —
  // l'identifiant concret, ou le rang plus 2442 pour un fichier : ôter le
  // costume l'éteint aussitôt, comme le client qui retire toutes les instances
  // de ce numéro sur l'acteur.
  std::set<std::pair<std::uint32_t, std::uint32_t>> worn_hat_effects;
  // LES CHANGEMENTS D'ACTEUR QU'UN COSTUME A POSÉS — rapetisser, blanchir,
  // dédoubler —, avec l'instant de la pose ; vide une fois leur vie propre
  // écoulée, pour ne pas les reposer tant que le costume reste.
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::optional<std::uint32_t>> hat_actor_changes;
  std::unordered_map<std::uint32_t, SownUnit> sown_units;
  // Le loquet des warps déjà pourvus — voir `effect_warp_view.cpp`. Gardé
  // d'une carte à l'autre, il ferait arriver sans ses anneaux le warp qui
  // reprendrait l'identifiant d'un autre.
  std::set<std::uint32_t> sown_warps;
  std::vector<PendingNamed> pending_named;
  // La dernière valeur vue pour un état à rang, par acteur. Ce qui n'y est pas
  // n'a pas encore de rang posé.
  //
  // ⚠ LE JOUEUR TRAVERSE LA CARTE AVEC SES VALEURS, et le serveur les lui
  // redit à l'identique : c'est l'oubli de cette mémoire qui repose l'effet
  // que le changement de carte a éteint.
  std::map<std::pair<std::uint32_t, ro::sim::StatusEffectId>, std::int32_t> state_values;
  // LES DEUX VALEURS DÉJÀ JOUÉES d'un état nommé qui les lit, par acteur : voir
  // `EffectsInPlay::PlayStateNamedValues`.
  std::map<std::pair<std::uint32_t, ro::sim::StatusEffectId>, std::pair<std::int32_t, std::int32_t>>
      named_state_values;
  // LE COMPTE DE RETRAITS DÉJÀ COUPÉS, par acteur : voir
  // `ro::sim::Actor::removed_effects_total`.
  std::unordered_map<std::uint32_t, std::uint32_t> removals_seen;
  // LE NUMÉRO DE LA DERNIÈRE LISTE DE DEVOTION TENDUE, par acteur : voir
  // `ro::sim::DevotionList::told_serial`. Un acteur absent d'ici n'en a tendu
  // aucune.
  std::unordered_map<std::uint32_t, std::uint32_t> devotion_lists_seen;
  // LES TIREURS DONT LA PROCHAINE FLÈCHE NE PART PAS : voir
  // `ro::render::ImpactSuppressesNextArrow`. Le drapeau se dépense au premier
  // tir, pas à la péremption.
  std::set<std::uint32_t> arrow_suppressed;
  // LES FLÈCHES D'ARROW SHOWER qui attendent leur départ.
  std::vector<ShowerArrow> shower_arrows;
  // UN TREMBLEMENT MIS EN FILE : l'instant où il part, et combien il dure.
  struct PendingQuake {
    std::uint32_t due_ms = 0;
    std::uint32_t lasts_ms = 0;
  };
  // LES TREMBLEMENTS QU'UN EFFET A MIS EN FILE en naissant : ceux de son `.str`
  // — voir `ro::render::StrEffectSetup::quake_ticks` — et ceux que son âge
  // déclenche — voir `ro/render/effect_update_quake.h`.
  std::vector<PendingQuake> pending_quakes;
};

// Note qu'un acteur porte cet effet, jusqu'à la fin de sa vie : voir
// `EffectMapMemory::carried_marks`.
void MarkCarried(EffectMapMemory& memory, std::uint32_t effect_id, std::uint32_t actor_id,
                 std::uint32_t now_ms);

}  // namespace moonclient
