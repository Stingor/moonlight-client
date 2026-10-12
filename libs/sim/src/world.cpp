#include "ro/sim/world.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "ro/sim/actor_traces.h"
#include "ro/sim/motion_events.h"
#include "ro/sim/movement.h"
#include "ro/sim/skill_banner.h"
#include "ro/sim/skill_side_effects.h"
#include "ro/sim/walk_path.h"

#include "facing_rules.h"

namespace ro::sim {
namespace {

// La distance entre deux cases, en cases.
float CellDistance(CellPosition from, CellPosition to) {
  const auto dx = static_cast<float>(static_cast<int>(to.x) - static_cast<int>(from.x));
  const auto dy = static_cast<float>(static_cast<int>(to.y) - static_cast<int>(from.y));
  return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

void World::EnterMap(std::string map_name, std::uint16_t x, std::uint16_t y) {
  std::optional<Actor> player;
  if (const auto found = actors_.find(player_id_); found != actors_.end()) {
    player = std::move(found->second);
  }
  actors_.clear();
  // La désignation se purge au changement de carte, et c'est la SEULE chose qui
  // la purge sans qu'une entité s'en aille. Elle survit au combat.
  Designate(0);
  // LES UNITÉS DE COMPÉTENCE AU SOL restent sur la carte quittée : le serveur
  // n'envoie aucune disparition pour celles qu'on laisse derrière soi.
  skill_units_.clear();
  strikes_.Clear();
  supports_.Clear();
  state_arrivals_.Clear();
  unseen_ground_skills_.Clear();
  // 🔴 LE CHAMP DE BATAILLE NE S'OUBLIE QU'AVEC SA CARTE. Une réapparition sur
  // la même garde coéquipiers et score : le serveur ne redit un coéquipier que
  // quand il bouge, et le score, selon la carte, qu'au point suivant.
  if (map_name != map_name_) {
    battlefield_.Clear();
  }
  map_name_ = std::move(map_name);
  ++map_entries_;
  // 🔴 LE MODE DE COMBAT S'ÉTEINT AVEC LA CARTE, ET LE SERVEUR NE LE REDIT
  // QU'APRÈS L'ENTRÉE. `clif_parse_LoadEndAck` n'envoie `ZC_MAPPROPERTY_R2`
  // qu'une fois le chargement confirmé : entre notre arrivée et sa réponse, un
  // mode conservé ferait viser les passants de la carte suivante.
  //
  // ⚠ UNE AFFECTATION ET NON UN `memset` : `MapMode` porte des champs dont le
  // défaut n'est pas l'octet nul — les deux permissions s'écrivent à l'envers.
  map_mode_ = MapMode();
  map_type_ = 0;
  // LE CLASSEMENT AUSSI : il appartient à la carte qu'on quitte. ⚠ LE SERVEUR
  // L'ÉTEINT DÉJÀ — `map_mapflag_pvp_stop_sub` envoie un rang et un total nuls
  // en quittant la zone — mais seulement quand la zone s'éteint sous nos pieds,
  // pas quand c'est nous qui partons. `unit_remove_map_` remet alors son rang à
  // zéro SANS rien annoncer.
  pvp_standing_.Forget();
  // 🔴 LE COFFRE SE FERME AU CHANGEMENT DE CARTE, ET LE SERVEUR NE LE DIT PAS.
  // `unit_remove_map_` appelle `storage_storage_quit`, qui sauvegarde et remet
  // `storage_flag` à zéro SANS envoyer sa trame de fermeture : il suppose que le
  // client ferme de lui-même. Sans cette ligne, la fenêtre resterait ouverte sur
  // un coffre qui n'existe plus, et tous ses gestes partiraient dans le vide.
  CloseStorage();
  // 🔴 ET LE DIALOGUE D'UN PNJ S'EN VA AVEC LA CARTE, comme au client
  // d'origine : le PNJ est resté là-bas, et aucun clic n'a plus de sens.
  //
  // ⚠ LE SCRIPT, LUI, RESTE ATTACHÉ AU PERSONNAGE, et fermer la fenêtre ne
  // suffit donc pas — mais ce qu'il reste à dire au serveur n'est pas l'affaire
  // du monde. Voir `net::NpcFarewellDue`, qui se lit AVANT d'appliquer la
  // trame.
  npc_dialog_.reset();
  // LES MARQUES DE QUÊTE appartiennent à la carte quittée : le serveur redit
  // celles de la nouvelle une fois le chargement confirmé.
  quest_markers_.Clear();
  if (!player.has_value()) {
    return;
  }
  player->position.x = x;
  player->position.y = y;
  // 🔴 ET TOUTES SES TRACES S'EFFACENT — voir `ForgetLastMapTraces`, qui porte la
  // liste et la raison. Le joueur est le SEUL acteur que ce changement de carte
  // transporte, et c'est lui qui rejouait son dernier sort à chaque `@load`.
  ForgetLastMapTraces(*player);
  actors_.emplace(player_id_, std::move(*player));
}

void World::Spawn(Actor actor) {
  const std::uint32_t id = actor.id;
  // Le serveur RÉANNONCE une unité à chaque fois qu'elle se met en marche, et
  // son annonce ne porte ni la vie ni la taille : celles-là sont arrivées à
  // part, par des paquets à elles. Tout écraser les effacerait à chaque pas —
  // la jauge d'un monstre clignoterait, et un corps rapetissé rejouerait son
  // rétrécissement en marchant.
  //
  // ⚠ CE QUI SE GARDE SE DÉPLACE, sans se copier : l'ancienne fiche est écrasée
  // juste après, et une réannonce arrive à chaque pas de chaque passant.
  if (const auto found = actors_.find(id); found != actors_.end()) {
    // ⚠ LA VIE NE SE GARDE QUE SI LA NEUVE EST MUETTE. L'annonce la PORTE quand
    // le serveur veut bien la dire (voir `Actor::health`) : écraser une valeur
    // fraîche par l'ancienne figerait la jauge d'un monstre qui marche, et la
    // reculerait d'un coup à chaque pas.
    if (actor.max_health <= 0 || actor.health == kUnknownHealth) {
      actor.health = found->second.health;
      actor.max_health = found->second.max_health;
    }
    actor.emotion = found->second.emotion;
    actor.emotion_since_ms = found->second.emotion_since_ms;
    actor.effects = std::move(found->second.effects);
    actor.skill_scale = found->second.skill_scale;
    actor.chat_text = std::move(found->second.chat_text);
    actor.chat_since_ms = found->second.chat_since_ms;
    // LA JAUGE aussi : la barre de progression d'un PNJ n'est redite qu'à
    // l'entrée en vue.
    actor.gauge = found->second.gauge;
    // Même raison, et elle mord plus fort ici : le serveur n'annonce l'enseigne
    // qu'À L'ENTRÉE EN VUE (clif_getareachar_pc), jamais à une réannonce. La
    // perdre serait définitif — tant que le vendeur reste en vue, rien ne la
    // redirait. Sa fermeture a son propre paquet.
    actor.signboard = std::move(found->second.signboard);
    // ET LA CARTE DE NOM, pour la même raison : elle vient d'un paquet à elle,
    // qu'une réannonce ne redit pas. Un joueur qui fait un pas perdrait sa
    // guilde, et plus rien ne la redirait tant qu'il reste en vue.
    actor.card = std::move(found->second.card);
    // ET SON RANG PVP, pour la même raison : il vient d'un paquet à lui, que la
    // réannonce ne redit pas. Le perdre ferait tomber l'anneau des dix premiers
    // au premier pas de celui qui le porte, et rien ne le remettrait avant que
    // le classement ne change.
    actor.pvp_rank = found->second.pvp_rank;
    // ET SES ÉTATS, pour la même raison : le serveur ne les redit qu'à
    // l'entrée en vue, et sa réannonce de marche n'en porte aucun. Les perdre
    // descendrait de sa monture un joueur qui fait un pas, et éteindrait son
    // aura.
    actor.status_effects = std::move(found->second.status_effects);
    actor.status_values = std::move(found->second.status_values);
    actor.status_second_values = std::move(found->second.status_second_values);
    actor.status_since_ms = std::move(found->second.status_since_ms);
    // ET SES PROTÉGÉS : une réannonce ne défait aucun trait de DEVOTION, et le
    // numéro perdu ferait prendre la liste gardée pour une annonce neuve.
    actor.devotion = found->second.devotion;
    // ET SON MAÎTRE, que seul son propre paquet dit : voir `TellSummonMaster`.
    if (!actor.summon_master_id) actor.summon_master_id = found->second.summon_master_id;
    KeepStateEdges(actor, found->second);
    // Ce qu'il avait déjà parcouru, plus le chemin fait depuis sa dernière
    // annonce : le serveur donne la case d'où le nouveau déplacement part, et
    // l'écart avec la précédente est ce qu'il a marché entre les deux.
    actor.travelled_before_cells =
        found->second.travelled_before_cells + CellDistance(found->second.position, actor.position);
  } else {
    // 🔴 UN ACTEUR NEUF REPREND SON RANG AU CARNET, et c'est ce qui fait revenir
    // l'anneau. Le serveur ne redit un rang que lorsqu'il CHANGE — jamais à
    // l'entrée en vue —, si bien qu'un joueur qui meurt et revient, ou qui sort
    // de la vue et y rentre, n'en recevrait plus jamais. Voir `PvpStanding`.
    actor.pvp_rank = pvp_standing_.RankOf(id);
  }
  actors_.insert_or_assign(id, std::move(actor));
}

bool World::Remove(std::uint32_t id) {
  // 🔴 CE QUI S'EN VA CESSE D'ÊTRE DÉSIGNÉ, ET NE LE REDEVIENT PAS TOUT SEUL.
  // C'est ici que la règle se tient — un seul endroit, quelle que soit la raison
  // du départ : sortie de vue, Hiding, mort.
  if (id == selected_id_) {
    Designate(0);
  }
  return actors_.erase(id) != 0;
}

// Change la désignation, dossier compris.
//
// 🔴 UN SEUL PASSAGE, PARCE QUE LES DEUX NE PEUVENT PAS DIVERGER. Un dossier
// laissé derrière une désignation qui change montrerait les points de vie de
// l'entité d'avant sous le nom de la nouvelle — et rien ne le signalerait, les
// deux nombres étant également plausibles.
void World::Designate(std::uint32_t id) {
  selected_id_ = id;
  target_detail_ = TargetDetail();
}

bool World::Select(std::uint32_t id) {
  // ⚠ ON NE DÉSIGNE QUE CE QUI EXISTE : sinon `selected_id()` et `selected()`
  // diraient deux choses, et l'appelant croirait l'une ou l'autre.
  if (id == 0 || !actors_.contains(id)) {
    return false;
  }
  // Redésigner la même entité ne change rien à ce que le serveur en a dit.
  if (id != selected_id_) {
    Designate(id);
  }
  return true;
}

bool World::TellTarget(TargetDetail told) {
  if (told.id == 0 || told.id != selected_id_) {
    return false;
  }
  // Les états viennent d'un autre paquet : ils traversent, tant qu'il s'agit de
  // la même entité — ce que la garde ci-dessus vient d'établir.
  told.statuses_told = target_detail_.statuses_told;
  told.statuses = std::move(target_detail_.statuses);
  told.statuses_refusals = target_detail_.statuses_refusals;
  target_detail_ = std::move(told);
  return true;
}

bool World::TellTargetStatuses(std::uint32_t id, std::vector<ActiveStatus> statuses) {
  if (id == 0 || id != selected_id_) {
    return false;
  }
  target_detail_.id = id;
  target_detail_.statuses_told = true;
  target_detail_.statuses = std::move(statuses);
  return true;
}

bool World::RefuseTargetStatuses(std::uint32_t id) {
  if (id == 0 || id != selected_id_) {
    return false;
  }
  target_detail_.id = id;
  ++target_detail_.statuses_refusals;
  return true;
}

void World::Halt(Actor& actor, CellPosition where) {
  // Ce qu'il a marché depuis l'annonce compte, sans quoi le cycle de marche
  // repartirait de sa première image au prochain pas.
  actor.travelled_before_cells += CellDistance(actor.position, where);
  // Le regard qu'on garde à l'arrêt est celui du pas en cours, si aucun
  // demi-tour ne l'a repris : c'est la marche qui l'a donné.
  if (actor.movement.has_value() && !actor.turned_since_walk_began) {
    actor.facing_origin = FacingOrigin::kWalk;
  }
  actor.position = where;
  // Le déplacement tombe avec la position : c'est le sens même d'un arrêt.
  actor.movement.reset();
}

bool World::PlaceAt(std::uint32_t id, CellPosition where, std::optional<std::uint32_t> arrived_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  if (arrived_ms.has_value()) {
    NoteStop(found->second, where, *arrived_ms);
  }
  Halt(found->second, where);
  if (arrived_ms.has_value()) {
    NoteArrival(found->second, *arrived_ms);
  }
  return true;
}

bool World::StartMoving(std::uint32_t id, const CellMovement& path, std::uint32_t started_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  // 🔴 UN DÉPLACEMENT NE PORTE AUCUNE DIRECTION. Les six octets de `MoveData`
  // sont deux cases et deux sous-cases, rien d'autre : `path.from.direction`
  // vaut donc ZÉRO — le NORD —, et l'affecter avec le reste met l'acteur dos à
  // la caméra partout où le chemin ne recouvre pas la valeur. Deux endroits ne
  // le recouvrent pas, et tous deux prétendaient « garder celle d'avant » :
  // l'arrêt annoncé, qui relit ce champ, et un ordre de marche vers la case
  // qu'on OCCUPE DÉJÀ — le chemin est alors vide et le regard se replie sur
  // lui. Ce second cas arrive tout seul : la marche tenue se redemande toutes
  // les cent millisecondes vers la case sous la souris, et sur un pas d'UNE
  // case on y est déjà arrivé.
  //
  // Ce qu'on garde est donc ce que l'acteur MONTRE à cet instant : le dernier
  // pas de la marche précédente, ou son demi-tour. ⚠ Lu AVANT d'écrire, et par
  // la ligne droite — le monde ne connaît pas la carte, et un chemin qui
  // contournait un obstacle peut finir sur un autre pas que sa corde.
  const std::uint8_t facing = FacingAt(found->second, started_ms);
  // Le compteur de distance ne repart pas : voir Actor::travelled_before_cells.
  found->second.travelled_before_cells += CellDistance(found->second.position, path.from);
  // La position devient celle du DÉPART annoncé, pas celle qu'on croyait : le
  // serveur fait autorité, et il corrige au passage un client qui aurait
  // dérivé.
  found->second.position = path.from;
  found->second.position.direction = facing;
  found->second.movement = path;
  found->second.movement_started_ms = started_ms;
  found->second.glide_ms_per_cell = 0;
  // Une marche neuve reprend la main sur le regard, et sur la vitesse.
  found->second.turned_since_walk_began = false;
  // 🔴 ET PARTIR EN MARCHE REDRESSE LA TÊTE. Le serveur le fait à l'insu de
  // tous : `unit_walktoxy_sub` écrit `sd->head_dir = DIR_NORTH` (src/map/unit.cpp)
  // et n'en avertit personne — aucun paquet de marche ne porte le regard. Un
  // client qui garderait le sien resterait la tête de biais jusqu'au prochain
  // demi-tour, et surtout il MENTIRAIT au serveur : sa prochaine demande de
  // direction partirait d'un regard que le serveur a déjà remis à zéro, et le
  // tour s'y compterait de travers.
  found->second.head_direction = kGazeAhead;
  return true;
}

const Actor* World::Find(std::uint32_t id) const {
  const auto found = actors_.find(id);
  return found == actors_.end() ? nullptr : &found->second;
}

bool World::SetSpeed(std::uint32_t id, std::uint16_t ms_per_cell) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.speed_ms_per_cell = ms_per_cell;
  return true;
}

bool World::SetStatusEffect(std::uint32_t id, StatusEffectId effect, bool active,
                            std::optional<std::int32_t> value,
                            std::optional<std::int32_t> second_value,
                            std::optional<std::uint32_t> since_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  if (active) {
    // L'instant de la POSE, et non d'une redite : un état redit tient depuis
    // la première fois.
    if (found->second.status_effects.insert(effect).second && since_ms.has_value()) {
      found->second.status_since_ms.insert_or_assign(effect, *since_ms);
    }
    if (value.has_value()) {
      found->second.status_values.insert_or_assign(effect, *value);
    }
    if (second_value.has_value()) {
      found->second.status_second_values.insert_or_assign(effect, *second_value);
    }
  } else {
    found->second.status_second_values.erase(effect);
    found->second.status_since_ms.erase(effect);
    found->second.status_effects.erase(effect);
    // ⚠ LA VALEUR PART AVEC L'ÉTAT. La garder ferait montrer l'anneau du tour
    // précédent au prochain Rolling Cutter, avant même que le serveur ait
    // annoncé le nouveau compte.
    found->second.status_values.erase(effect);
  }
  if (since_ms.has_value()) {
    NoteStatusSleep(found->second, effect, active, *since_ms);
  }
  state_arrivals_.Record(id, effect, active);
  return true;
}

bool World::SetHatEffect(std::uint32_t id, HatEffectId effect, bool active) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  if (active) {
    found->second.hat_effects.insert(effect);
  } else {
    found->second.hat_effects.erase(effect);
  }
  return true;
}

bool World::SetLook(std::uint32_t id, const Appearance& look) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.look = look;
  return true;
}

bool World::SetPosture(std::uint32_t id, Posture posture, std::uint32_t since_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  if (found->second.posture != posture) {
    found->second.posture = posture;
    found->second.posture_since_ms = since_ms;
  }
  return true;
}

bool World::SetSignboard(std::uint32_t id, Signboard board) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.signboard = std::move(board);
  return true;
}

bool World::ClearSignboard(std::uint32_t id) {
  const auto found = actors_.find(id);
  if (found == actors_.end() || !found->second.signboard.has_value()) {
    return false;
  }
  found->second.signboard.reset();
  return true;
}

bool World::ClearRoom(std::uint32_t room_id) {
  // ⚠ ON CHERCHE PAR L'IDENTIFIANT DU SALON, et il n'indexe rien : c'est
  // pourquoi ce parcours existe. Les acteurs sont indexés par LEUR identifiant,
  // et un salon porte le sien. Ils sont quelques dizaines à l'écran, et un
  // salon se ferme rarement.
  for (auto& [id, actor] : actors_) {
    if (actor.signboard.has_value() && actor.signboard->kind == SignboardKind::kChatRoom &&
        actor.signboard->room_id == room_id) {
      actor.signboard.reset();
      return true;
    }
  }
  return false;
}

ParameterOutcome World::SetPlayerParameter(std::uint16_t parameter, std::int64_t value) {
  const auto named = static_cast<ServerParameter>(parameter);

  // LA VITESSE VIT SUR L'ACTEUR, parce que tout le monde en a une.
  if (named == ServerParameter::kSpeed) {
    // ⚠ SANS JOUEUR POSÉ, C'EST « RECONNU, RIEN À FAIRE » et non « inconnu » :
    // le serveur annonce des paramètres avant l'entrée en jeu, ce qui est
    // normal, et les compter comme du travail restant ferait croire à un paquet
    // non porté.
    return SetSpeed(player_id_, static_cast<std::uint16_t>(value))
               ? ParameterOutcome::kStored
               : ParameterOutcome::kKnownButUnused;
  }
  // LA VIE AUSSI, et c'est la même raison : le joueur n'est pas un cas à part,
  // et c'est ce qui fait que sa barre se dessine comme celle des autres.
  if (named == ServerParameter::kHp || named == ServerParameter::kMaxHp) {
    const Actor* const self = Find(player_id_);
    if (self == nullptr) {
      return ParameterOutcome::kKnownButUnused;
    }
    // ⚠ LE SERVEUR LES ANNONCE EN DEUX PARAMÈTRES, et poser l'un en oubliant
    // l'autre effacerait la barre : `SetHealth` rend la vie INCONNUE dès que le
    // maximum est nul ou négatif, ce qu'il vaut justement tant que le second
    // paramètre n'est pas arrivé. On garde donc celui qu'on tient déjà.
    //
    // ⚠ ET LA VIE QUI PRÉCÈDE SON MAXIMUM SE MET DE CÔTÉ : `SetHealth` la
    // rendrait inconnue, et le maximum, à son arrivée, partirait alors de rien
    // — une barre à zéro jusqu'au prochain coup, que la régénération d'un
    // joueur plein n'envoie jamais.
    const auto given = static_cast<std::int32_t>(value);
    if (named == ServerParameter::kHp && self->max_health <= 0) {
      pending_player_hp_ = given;
      return ParameterOutcome::kStored;
    }
    const std::int32_t held_hp =
        self->max_health > 0 ? self->health : pending_player_hp_.value_or(kUnknownHealth);
    const std::int32_t hp = named == ServerParameter::kHp ? given : held_hp;
    const std::int32_t max_hp = named == ServerParameter::kMaxHp ? given : self->max_health;
    pending_player_hp_.reset();
    return SetHealth(player_id_, hp, max_hp) ? ParameterOutcome::kStored
                                             : ParameterOutcome::kKnownButUnused;
  }
  return ApplyParameter(player_status_, parameter, value);
}

bool World::SetPlayerAttribute(std::uint16_t parameter, std::int64_t base, std::int64_t bonus) {
  // ⚠ `ZC_COUPLESTATUS` NUMÉROTE COMME `enum _sp`, et non de zéro à cinq : son
  // `statusType` vaut SP_STR pour la force. La base passe donc par la même
  // table que les paramètres ordinaires, et se range où elle irait si elle
  // était arrivée seule.
  const bool ranged = ApplyParameter(player_status_, parameter, base) == ParameterOutcome::kStored;
  return ApplyAttributeBonus(player_status_, parameter, bonus) && ranged;
}

bool World::SetHealth(std::uint32_t id, std::int32_t health, std::int32_t max_health) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  if (max_health <= 0) {
    found->second.health = kUnknownHealth;
    found->second.max_health = kUnknownHealth;
    return true;
  }
  found->second.health = std::clamp(health, 0, max_health);
  found->second.max_health = max_health;
  return true;
}

bool World::RecordStrike(const Strike& arrived) {
  // L'UNITÉ AU SOL QUI FRAPPE se relève ICI : voir `Strike::by_ground_unit`.
  const auto striking_unit = skill_units_.find(arrived.attacker);
  Strike strike = arrived;
  strike.by_ground_unit = striking_unit != skill_units_.end();
  const bool by_unit = strike.by_ground_unit;
  const auto attacker = actors_.find(strike.attacker);
  const auto target = actors_.find(strike.target);
  // LE COMPTEUR SE NOURRIT ICI, et d'un seul endroit : trois paquets amènent
  // un coup ; une unité au sol est créditée à son lanceur — voir `DamageMeter`.
  damage_meter_.SetSelf(player_id_);
  const auto author = actors_.find(by_unit ? striking_unit->second.creator_id : strike.attacker);
  damage_meter_.NoteStrike(strike, by_unit ? striking_unit->second.creator_id : strike.attacker,
                           by_unit, author != actors_.end() ? &author->second : nullptr,
                           target != actors_.end() ? &target->second : nullptr);
  // OÙ LA CIBLE SE TROUVE AU COUP, lu AVANT que le coup ne l'arrête : c'est de
  // là que son auteur la vise, et non de la case où on la repose.
  std::optional<FractionalCell> target_at_strike;
  if (target != actors_.end()) {
    target_at_strike = PositionAt(target->second, walk_map_, strike.started_ms);
    target->second.hit = strike;
    // Broncher COUPE la marche (voir SetWalkMap) ; l'endurance ou zéro dégât la
    // laisse courir, comme le serveur, qui ne retarde qu'une vraie secousse.
    if (strike.flinches && strike.hurt_motion_ms > 0 && target->second.movement.has_value()) {
      const CellMovement& movement = *target->second.movement;
      const std::optional<WalkPath> found =
          walk_map_ != nullptr ? FindWalkPath(*walk_map_, movement.from, movement.to)
                               : std::nullopt;
      const WalkPath path =
          found.has_value() ? *found : StraightWalkPath(movement.from, movement.to);
      // L'instant du coup est celui où la trame nous parvient : la latence
      // décale l'arrêt de moins d'un demi-pas, et la prochaine annonce recale.
      Halt(target->second, StoppedCell(target->second, path, strike.started_ms));
    }
  }
  if (attacker != actors_.end() || target != actors_.end()) {
    // UNE UNITÉ AU SOL FRAPPÉE se retient à sa case : voir `Strike::struck_unit_cell`.
    const auto hit = skill_units_.find(strike.target);
    strikes_.Record(hit != skill_units_.end() ? StrikeOnUnitAt(strike, hit->second.cell) : strike);
  }
  // UNE UNITÉ AU SOL QUI FRAPPE fait jouer son modèle une fois : le piège se
  // referme. Un second coup ne le relance pas, il est déjà parti.
  if (const auto unit = skill_units_.find(strike.attacker);
      unit != skill_units_.end() && !unit->second.sprung_ms.has_value()) {
    unit->second.sprung_ms = strike.started_ms;
  }
  if (attacker != actors_.end() && strike.attacker != strike.target) {
    attacker->second.attack = strike;
    ArmArrivalPose(attacker->second, strike);
    // L'auteur se tourne vers ce qu'il frappe — règles dans `facing_rules.h`.
    facing::FaceAttacker(facing::Scene{actors_, skill_units_, walk_map_, player_id_},
                         attacker->second, strike, target_at_strike);
  }
  return attacker != actors_.end() || target != actors_.end();
}

bool World::RecordPickup(std::uint32_t id, std::uint32_t since_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.pickup_since_ms = since_ms;
  return true;
}

bool World::RecordGroundSkill(std::uint32_t caster_id, const GroundSkillUse& use) {
  const auto found = actors_.find(caster_id);
  if (found == actors_.end()) {
    unseen_ground_skills_.Record(caster_id, use);
    return false;
  }
  found->second.ground_skill = use;
  found->second.turns_received_at_ground_skill = found->second.turns_received;
  // ONZE COMPÉTENCES NE POSENT PAS LA MOTION D'EMPLOI : le lanceur garde la
  // sienne — voir `GroundSkillKeepsCasterMotion`.
  if (!GroundSkillKeepsCasterMotion(use.skill_id)) {
    found->second.skill_use = SkillUse{use.skill_id, use.started_ms};
  }
  facing::FaceGroundSkillCaster(facing::Scene{actors_, skill_units_, walk_map_, player_id_},
                                found->second, use);
  return true;
}

bool World::AnnounceSkill(std::uint32_t caster_id, std::uint16_t skill_id, std::uint32_t now_ms,
                          std::uint32_t target_id, std::uint32_t creature_argument) {
  const auto found = actors_.find(caster_id);
  if (found == actors_.end()) {
    return false;
  }
  // ⚠ UN MONSTRE ET UN PNJ ANNONCENT CE QU'UN JOUEUR TAIT : le client ne
  // consulte sa liste d'exceptions que pour un joueur.
  const bool monster_or_npc =
      found->second.kind == ActorKind::kMonster || found->second.kind == ActorKind::kNonPlayer;
  if (!SkillBannerShows(skill_id, monster_or_npc)) {
    return false;
  }
  found->second.announced_skill_id = skill_id;
  found->second.announced_skill_since_ms = now_ms;
  found->second.announced_target_id = monster_or_npc ? creature_argument : target_id;
  return true;
}

void World::RecordWhisper(WhisperFacts facts, std::string text, std::uint32_t said_ms) {
  RecordChat(ChatKind::kWhisper, 0, std::move(text), said_ms);
  chat_lines_.back().whisper = std::move(facts);
}

void World::RecordChat(ChatKind kind, ChatType type, std::uint32_t speaker_id, std::string text,
                       std::uint32_t said_ms, std::uint32_t color,
                       std::optional<ChatAuthor> author) {
  if (const auto found = actors_.find(speaker_id); found != actors_.end()) {
    found->second.chat_text = text;
    found->second.chat_since_ms = said_ms;
  }
  // ÉVICTION PAR GENRE : les plus anciennes de CE genre-là s'en vont.
  //
  // ⚠ DEUX AU PLUS PAR LIGNE ARRIVÉE quand la borne est dépassée, et non une :
  // une seule retirée pour une ajoutée laisserait le compte où il est, et une
  // borne abaissée ne prendrait jamais effet. Deux, et non tout l'excédent :
  // le trop-plein s'écoule au fil des lignes, sans vider d'un coup ce que le
  // joueur est en train de lire.
  constexpr int kMostEvictedPerLine = 2;
  const auto of_kind = static_cast<int>(std::ranges::count_if(
      chat_lines_, [kind](const ChatLine& line) { return line.kind == kind; }));
  int evicted = std::min(of_kind - kept_per_kind_ + 1, kMostEvictedPerLine);
  if (evicted > 0) {
    std::erase_if(chat_lines_, [kind, &evicted](const ChatLine& line) {
      if (evicted == 0 || line.kind != kind) {
        return false;
      }
      --evicted;
      return true;
    });
  }
  // Le rang part à UN : une marque de vidage à zéro ne désigne aucune ligne.
  ++chat_lines_seen_;
  chat_lines_.push_back(ChatLine{.seq = chat_lines_seen_,
                                 .kind = kind,
                                 .type = type,
                                 .speaker_id = speaker_id,
                                 .text = std::move(text),
                                 .said_ms = said_ms,
                                 .color = color,
                                 .author = std::move(author)});
}

Inventory& World::HolderFor(ItemHolder holder) noexcept {
  switch (holder) {
    case ItemHolder::kCart:
      return cart_;
    case ItemHolder::kStorage:
      return storage_;
    case ItemHolder::kInventory:
      break;
  }
  return inventory_;
}

void World::PlaceSkillUnit(const SkillUnit& unit) { skill_units_[unit.id] = unit; }

bool World::RemoveSkillUnit(std::uint32_t id) { return skill_units_.erase(id) != 0; }
bool World::RecordSkillScale(std::uint32_t id, const SkillScale& scale) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.skill_scale = scale;
  return true;
}

bool World::TellActorName(std::uint32_t id, std::string name, NameCard card) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  if (!name.empty()) {
    found->second.name = std::move(name);
  }
  found->second.card = std::move(card);
  return true;
}

bool World::RecordEmotion(std::uint32_t id, Emotion emotion, std::uint32_t since_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.emotion = emotion;
  found->second.emotion_since_ms = since_ms;
  return true;
}

bool World::SetStates(std::uint32_t id, std::uint16_t body_state, std::uint16_t health_state,
                      std::uint32_t option, std::uint32_t now_ms, bool pk_mode) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  Actor& actor = found->second;
  NoteBodyState(actor, body_state, now_ms);
  actor.health_state = health_state;
  actor.pk_mode = pk_mode;
  // LES BITS QUI S'ALLUMENT, avant d'écrire le mot : c'est le seul endroit où
  // l'ancien et le nouveau se croisent. Voir `Actor::option_risen`.
  if (const std::uint32_t risen = option & ~actor.option; risen != 0) {
    actor.option_risen = risen;
    ++actor.option_rise_count;
  }
  if (const std::uint32_t fallen = actor.option & ~option; fallen != 0) {
    actor.option_fallen = fallen;
    ++actor.option_fall_count;
  }
  actor.option = option;
  ++actor.state_change_count;
  NoteHealthMarks(actor, now_ms);
  return true;
}

void World::NotePlayerStatus(StatusEffectId effect, bool active, std::uint32_t total_ms,
                             std::uint32_t now_ms) {
  const auto same = [effect](const ActiveStatus& kept) { return kept.effect == effect; };
  const auto found = std::find_if(player_statuses_.begin(), player_statuses_.end(), same);
  if (!active) {
    if (found != player_statuses_.end()) {
      player_statuses_.erase(found);
    }
    return;
  }

  // 🔴 « 9999 » N'EST PAS UNE DURÉE : c'est ce que le serveur écrit quand il n'y
  // en a pas. Le prendre au mot ferait disparaître un buff permanent au bout de
  // dix secondes.
  const bool timed = total_ms != 0 && total_ms != kUnknownDurationMs;
  ActiveStatus posed;
  posed.effect = effect;
  posed.total_ms = timed ? total_ms : 0;
  posed.expires_ms = timed ? now_ms + total_ms : 0;

  if (found != player_statuses_.end()) {
    *found = posed;
    return;
  }
  player_statuses_.push_back(posed);
}

void World::ExpirePlayerStatuses(std::uint32_t now_ms) {
  const auto over = [now_ms](const ActiveStatus& kept) {
    // ⚠ SANS ÉCHÉANCE N'EST PAS ÉCHU. Comparer zéro à l'horloge retirerait tous
    // les états permanents à la première image.
    return kept.expires_ms != 0 && now_ms >= kept.expires_ms;
  };
  std::erase_if(player_statuses_, over);
  for (auto& [id, actor] : actors_) {
    std::erase_if(actor.effects,
                  [now_ms](const PlayedEffect& e) { return e.ends_ms && now_ms >= *e.ends_ms; });
  }
}

void World::Clear() {
  actors_.clear();
  skill_units_.clear();
  strikes_.Clear();
  supports_.Clear();
  state_arrivals_.Clear();
  unseen_ground_skills_.Clear();
  // ⚠ LA DÉSIGNATION NE SURVIT PAS, elle : elle nomme un acteur, et il n'y en a
  // plus. C'est ce qui la sépare de l'identifiant du joueur, juste en dessous.
  Designate(0);
  // L'identifiant du joueur SURVIT : il vient de la connexion, pas de la carte.
  // Sa vie en attente, non : elle attendait un maximum posé sur l'acteur
  // qu'on vient d'effacer.
  pending_player_hp_.reset();
}

}  // namespace ro::sim
