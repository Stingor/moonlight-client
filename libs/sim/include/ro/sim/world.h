#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/bank.h"
#include "ro/sim/battlefield.h"
#include "ro/sim/card_composition.h"
#include "ro/sim/chat_channel.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/companion.h"
#include "ro/sim/craft_session.h"
#include "ro/sim/damage_meter.h"
#include "ro/sim/departures.h"
#include "ro/sim/ground_skill_log.h"
#include "ro/sim/inventory.h"
#include "ro/sim/map_mode.h"
#include "ro/sim/npc_dialog.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/player_bonuses.h"
#include "ro/sim/player_status.h"
#include "ro/sim/pvp_rank.h"
#include "ro/sim/quest_markers.h"
#include "ro/sim/server_clock.h"
#include "ro/sim/session_settings.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/skill_facts.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/storage_facts.h"
#include "ro/sim/storage_shelf.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/style.h"
#include "ro/sim/target_detail.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/warp_menu.h"

namespace ro::sim {

// Le monde, tel que le SERVEUR le décrit.
//
// ⚠ Cette classe ne connaît ni le temps ni le protocole.
//
// Pas le temps : elle porte l'état que le serveur a annoncé, jamais une
// interpolation. Interpoler est l'affaire de qui dessine, et une classe qui
// regarde l'horloge ne se rejoue pas à l'identique — or rejouer une capture et
// comparer l'état final est le seul contrôle qui rende le jeu non-régressable.
//
// Pas le protocole : c'est ro_net qui traduit les trames en appels d'ici. Un
// essai peut donc bâtir un monde à la main, sans socket ni paquet.
class World {
 public:
  // Fait entrer un acteur, ou REMPLACE celui qui portait déjà cet identifiant.
  //
  // Le serveur réannonce un acteur déjà présent quand il change de carte ou
  // sort puis rentre dans le champ de vision. Refuser le doublon laisserait
  // l'ancien état, donc une apparence périmée.
  //
  // ⚠ Ce que le paquet d'unité NE PORTE PAS survit au remplacement : la vie et
  // la taille arrivent par des paquets à elles, et le serveur réannonce l'unité
  // entière à chaque mise en marche.
  void Spawn(Actor actor);

  // Rend faux si personne ne portait cet identifiant.
  bool Remove(std::uint32_t id);

  // Repose un acteur, et ANNULE son déplacement. C'est ce que le serveur fait
  // quand il arrête quelqu'un : garder le
  // déplacement ferait glisser le personnage vers une destination abandonnée.
  // Avec `arrived_ms`, c'est l'ARRÊT du serveur : voir `NoteStop` et
  // `NoteArrival`.
  bool PlaceAt(std::uint32_t id, CellPosition where, std::optional<std::uint32_t> arrived_ms = {});

  // RECALE un acteur sur la case que le serveur nomme, sans rien changer à sa
  // motion : ni chemin bâti, ni arrêt, ni regard. Qui marche encore garde sa
  // marche, qui reprend la position au pas suivant ; qui se tient là est
  // déplacé tel quel, et sa marche achevée reste ce que son repos quitte.
  bool Reposition(std::uint32_t id, std::uint16_t x, std::uint16_t y, std::uint32_t now_ms);

  // Met un acteur en marche, depuis le départ annoncé (voir movement_started_ms).
  bool StartMoving(std::uint32_t id, const CellMovement& path, std::uint32_t started_ms);
  // Le fait GLISSER de sa case vers `to`, à la vitesse imposée et non à la sienne.
  bool StartGliding(std::uint32_t id, CellPosition to, std::uint16_t ms_per_cell,
                    std::uint32_t now_ms);

  // Tourne un acteur SUR PLACE : le corps et le regard, sans toucher à sa case.
  //
  // ⚠ CELA N'ANNULE PAS UNE MARCHE EN COURS — on peut regarder ailleurs en
  // marchant, et le serveur l'accepte (pc_setdir est inconditionnel). Ce qui
  // change, c'est qu'une fois la marche finie c'est ce demi-tour qui décide du
  // regard, et non plus le dernier pas : voir Actor::turned_since_walk_began.
  // Faux pour un inconnu.
  bool Turn(std::uint32_t id, std::uint8_t direction, std::uint8_t head_direction);

  // Change la vitesse de marche d'un acteur. Faux pour un inconnu.
  //
  // Le serveur l'annonce avec chaque unité qu'il montre, mais JAMAIS avec le
  // joueur, qui n'est annoncé par aucun paquet d'unité : la sienne arrive à
  // part, par le paramètre SP_SPEED.
  bool SetSpeed(std::uint32_t id, std::uint16_t ms_per_cell);

  // Pose ou retire un état. Rend faux si personne ne portait cet identifiant.
  //
  // `value` est ce que le serveur annonce avec l'état — le `val1` du paquet
  // 0x043F —, `second_value` son `val2`. Elles ne sont RETENUES que pour les
  // états qui en montrent quelque chose, et le monde ne sait pas lesquels :
  // c'est l'appelant qui le dit, en ne les passant que pour eux. `since_ms`
  // date la pose : voir `Actor::status_since_ms`.
  bool SetStatusEffect(std::uint32_t id, StatusEffectId effect, bool active,
                       std::optional<std::int32_t> value = std::nullopt,
                       std::optional<std::int32_t> second_value = std::nullopt,
                       std::optional<std::uint32_t> since_ms = std::nullopt);
  // Pose ou retire un effet de chapeau. Faux pour un acteur inconnu.
  bool SetHatEffect(std::uint32_t id, HatEffectId effect, bool active);

  // LE DIALOGUE EN COURS, s'il y en a un. Il n'appartient à aucun acteur : le
  // PNJ peut disparaître de la vue sans que sa fenêtre se ferme, et le serveur
  // en ouvre même au nom d'un PNJ FEINT qui n'est nulle part.
  [[nodiscard]] const std::optional<NpcDialog>& npc_dialog() const noexcept { return npc_dialog_; }

  // Ce que le PNJ dit de plus. Ouvre le dialogue s'il n'y en avait pas.
  //
  // ⚠ LA LIGNE NE S'AFFICHE PAS TOUT DE SUITE : elle rejoint la page en
  // RÉCEPTION, publiée d'un seul tenant par le paquet qui la termine. Voir
  // `NpcDialog::pending_lines`.
  //
  // ⚠ UN AUTRE PNJ REMPLACE LE PRÉCÉDENT. Deux dialogues ne coexistent pas : le
  // serveur n'en tient qu'un par joueur, et en garder deux montrerait une
  // fenêtre que plus rien ne ferme.
  void SayNpcDialog(std::uint32_t npc_id, std::string line, std::uint32_t now_ms);

  // Ce que le dialogue attend désormais, et la PUBLICATION de la page reçue :
  // le paquet qui pose un bouton est celui qui la termine. Rend faux s'il n'y a
  // rien à quoi l'attacher — le serveur envoie « fermer » à la fin d'un script
  // même quand rien n'est ouvert, et ce n'est pas une anomalie.
  bool PromptNpcDialog(std::uint32_t npc_id, NpcPrompt prompt, std::uint32_t now_ms);

  // Un menu de choix. Il OUVRE un dialogue au besoin — un script peut appeler
  // `select` sans avoir rien dit — et PUBLIE la page, comme tout terminateur.
  void OfferNpcMenu(std::uint32_t npc_id, std::vector<NpcMenuOption> options, std::uint32_t now_ms);

  // Le `clear` d'un script : la prochaine ligne remplacera la page.
  //
  // 🔴 IL NE PUBLIE RIEN, ET SURTOUT PAS. `clear` est le PREMIER paquet de la
  // réponse du serveur : y publier ferait tomber le menu de la page précédente à
  // cet instant, le texte s'étalerait dans la place libérée pendant toute la
  // rafale, puis se ferait raboter à l'arrivée du nouveau menu. Deux
  // réarrangements au lieu de zéro.
  //
  // ⚠ ET IL N'EFFACE PAS NON PLUS TOUT DE SUITE : vider ici laisse la fenêtre
  // sans texte ni bouton le temps que le `mes` suivant arrive — un clignotement
  // à chaque écran un peu bavard. Écart assumé avec le client officiel, qui vide
  // aussitôt : un `clear` non suivi d'un `mes` est introuvable dans les scripts,
  // et l'ancienne page est de toute façon plus lisible qu'un cadre vide.
  //
  // Rend faux si le paquet ne désigne pas le dialogue en cours.
  bool ClearNpcDialogText(std::uint32_t npc_id, std::uint32_t now_ms);

  // LE FILET DE LA MISE EN ATTENTE : une page dont le terminateur n'arrive
  // jamais finit par s'afficher — un script qui écrit puis dort, ou qui s'arrête
  // sur `end`. Rend vrai s'il a publié quelque chose. Voir `kNpcPageIdleMs`.
  bool CommitNpcPageIfIdle(std::uint32_t now_ms);

  // Le joueur a cliqué « suivant » : la prochaine page remplacera celle-ci, et
  // en attendant la réponse la page reste affichée, INERTE.
  void AdvanceNpcDialog();

  // Le joueur a répondu au menu. La liste RESTE affichée, inerte, jusqu'à ce que
  // la réponse arrive : la vider tout de suite escamote la question et fait
  // sauter le pied de fenêtre. Une seconde réponse à la même liste est dès lors
  // impossible — voir `NpcDialog::menu_answered`.
  void AnswerNpcMenu();

  // Le joueur a soumis une saisie. Le champ reste en place, inerte.
  //
  // ⚠ IL N'ARME PAS L'EFFACEMENT, contrairement au « suivant » : le serveur ne
  // documente cet effacement qu'au-dessus de ZC_WAIT_DIALOG, et l'armer ici
  // ferait disparaître la question à laquelle on vient de répondre.
  void SubmitNpcInput();

  // Le dialogue se ferme, quelle qu'en soit la raison.
  void CloseNpcDialog();

  // Pose l'enseigne d'un acteur, en remplaçant celle qu'il avait. Faux pour un inconnu.
  bool SetSignboard(std::uint32_t id, Signboard board);
  // Retire l'enseigne d'un acteur. Faux s'il n'en avait pas — le serveur
  // annonce la fermeture d'une échoppe à qui n'en avait jamais vu l'ouverture.
  bool ClearSignboard(std::uint32_t id);
  // Ferme LE SALON qui porte cet identifiant, chez celui qui le tient. Faux si
  // aucun acteur visible ne l'avait — voir Signboard : ce n'est pas un
  // identifiant d'acteur.
  bool ClearRoom(std::uint32_t room_id);

  // Change l'apparence d'un acteur, et rien d'autre : ni sa place, ni sa
  // marche, ni ses états. Rend faux si personne ne portait cet identifiant.
  bool SetLook(std::uint32_t id, const Appearance& look);

  // Change la posture d'un acteur, et note l'instant. La même posture redite
  // garde son instant : l'animation ne repart pas. Rend faux si personne ne
  // portait cet identifiant.
  bool SetPosture(std::uint32_t id, Posture posture, std::uint32_t since_ms);

  // L'HORLOGE DU SERVEUR, calée sur celle de l'appelant, et l'aller-retour
  // qu'elle mesure au passage.
  //
  // POURQUOI ELLE EXISTE. Le serveur date le départ d'une marche dans SA propre
  // horloge. Y substituer l'instant de réception, c'est ajouter la latence à
  // chaque départ : l'acteur marche en retard, et le moindre recalage — un
  // arrêt annoncé, un coup porté — le fait sauter à sa vraie place. Voir
  // `ServerClock` pour le reste, calage et mesure compris.
  [[nodiscard]] ServerClock& server_clock() noexcept { return server_clock_; }
  [[nodiscard]] const ServerClock& server_clock() const noexcept { return server_clock_; }

  // Pose la vie d'un acteur. Une vie maximale nulle ou négative rend la vie
  // INCONNUE plutôt qu'une barre vide. Rend faux si personne ne portait cet
  // identifiant.
  bool SetHealth(std::uint32_t id, std::int32_t health, std::int32_t max_health);

  // CE QUE LE SERVEUR DIT DU PERSONNAGE QU'ON JOUE.
  //
  // ⚠ « pas encore dit » y vaut moins un, jamais zéro. Voir `PlayerStatus`.
  [[nodiscard]] const PlayerStatus& player_status() const noexcept { return player_status_; }

  // LE DÉTAIL DE CE QUE L'ÉQUIPEMENT APPORTE, quand le serveur a bien voulu le
  // dire — il ne le dit qu'à un client qui a donné sa poignée de main.
  //
  // ⚠ « pas encore dit » y vaut `told == false`, et non « tout à zéro » : un
  // personnage nu a bien quarante-neuf zéros à montrer.
  [[nodiscard]] const PlayerBonuses& player_bonuses() const noexcept { return player_bonuses_; }

  // Remplace le relevé ENTIER. Le serveur le recompose à chaque recalcul et
  // n'envoie jamais de différence : le garder par morceaux laisserait traîner
  // un bonus que l'équipement retiré ne donne plus.
  void SetPlayerBonuses(PlayerBonuses bonuses) { player_bonuses_ = std::move(bonuses); }

  // LES ÉTATS DU JOUEUR, AVEC LEUR DURÉE, dans l'ordre où ils sont tombés.
  [[nodiscard]] std::span<const ActiveStatus> player_statuses() const noexcept {
    return player_statuses_;
  }

  // Pose ou retire un état du joueur, en gardant ce que le paquet dit de sa
  // durée. `total_ms` à zéro — ou à la sentinelle — vaut « sans échéance ».
  //
  // ⚠ LE MÊME ÉTAT QUI REVIENT RENOUVELLE LE SIEN, il ne s'ajoute pas : un buff
  // relancé se voit à sa durée qui repart, pas à une deuxième icône.
  void NotePlayerStatus(StatusEffectId effect, bool active, std::uint32_t total_ms,
                        std::uint32_t now_ms);

  // Retire ce qui est échu — les états du joueur, et les effets d'acteur qu'une
  // échéance borne. ⚠ APPELÉ PAR LA BOUCLE : ce qui finit de lui-même n'est
  // annoncé par personne.
  void ExpirePlayerStatuses(std::uint32_t now_ms);

  // Range un paramètre annoncé par le serveur, sous le numéro qu'il emploie.
  //
  // ⚠ C'EST LE SEUL ENDROIT OÙ LES DEUX VOIES SE REJOIGNENT, et c'est tout
  // l'objet de cette méthode. La vie et la vitesse vivent sur l'ACTEUR, parce
  // que tout le monde en a ; le reste vit sur le personnage, parce qu'un monstre
  // n'a ni bourse ni points de compétence. Laisser l'appelant trancher
  // l'obligerait à connaître cette répartition, et deux appelants la
  // connaîtraient bientôt différemment.
  //
  // La VIE MAXIMALE déjà connue est conservée quand seule la vie change, et
  // réciproquement : le serveur les annonce en DEUX paramètres, et poser l'un
  // en oubliant l'autre effacerait la barre à chaque coup reçu.
  [[nodiscard]] ParameterOutcome SetPlayerParameter(std::uint16_t parameter, std::int64_t value);

  // Pose le BONUS d'une caractéristique, celui que l'équipement ajoute, avec sa
  // valeur de base. C'est ce que `ZC_COUPLESTATUS` annonce, et il les donne
  // ensemble.
  //
  // Rend faux pour un numéro de caractéristique que le serveur ne connaît pas.
  bool SetPlayerAttribute(std::uint16_t parameter, std::int64_t base, std::int64_t bonus);

  // La portée d'attaque annoncée, en cases.
  void SetPlayerAttackRange(std::int64_t cells) { player_status_.attack_range = cells; }

  // Le seuil de surcharge, en pour cent, hors de `SetPlayerParameter` : à part.
  void SetOverweightPercent(std::int64_t percent) { player_status_.overweight_percent = percent; }

  // Note un coup sur qui le porte et sur qui le reçoit, tourne le premier vers
  // le second, et l'inscrit au journal `strikes()`. Qui se frappe lui-même ne
  // porte pas de coup : il en reçoit un. Rend faux si aucun des deux n'est là.
  //
  // ⚠ Un coup qui fait BRONCHER sa cible lui coupe la marche, et le serveur ne
  // l'annonce par aucun paquet : voir SetWalkMap.
  bool RecordStrike(const Strike& strike);
  [[nodiscard]] const StrikeLog& strikes() const noexcept { return strikes_; }

  // Note qu'un acteur se baisse pour ramasser. Faux pour un inconnu.
  bool RecordPickup(std::uint32_t id, std::uint32_t since_ms);

  // Note une compétence lancée sur une CASE, sur son lanceur, et le tourne vers
  // elle — règles dans `facing_rules.h`. Rend faux pour un lanceur hors de vue :
  // la compétence rejoint alors `unseen_ground_skills()`, car la case se montre
  // sans lui.
  bool RecordGroundSkill(std::uint32_t caster_id, const GroundSkillUse& use);
  [[nodiscard]] const UnseenGroundSkillLog& unseen_ground_skills() const noexcept {
    return unseen_ground_skills_;
  }

  // Note une compétence SANS DÉGÂTS sur qui la reçoit, et l'inscrit au journal
  // `supports()`. Faux pour un acteur qu'on ne voit pas.
  bool RecordSupport(std::uint32_t target_id, const SupportHit& hit);
  [[nodiscard]] const SupportLog& supports() const noexcept { return supports_; }
  // Le délai de combo, et l'instant ; ses sphères ÉLÉMENTAIRES (`spirit_charm_element`),
  // puis SPIRITUELLES — l'effet de sa classe, au nouveau nombre. Faux : inconnu.
  bool RecordComboDelay(std::uint32_t id, std::uint32_t delay_ms, std::uint32_t now_ms);
  bool RecordSpiritCharm(std::uint32_t id, std::int16_t element, std::int16_t count,
                         std::uint32_t now_ms);
  bool RecordSpiritSpheres(std::uint32_t id, std::int16_t count, std::uint32_t now_ms);

  // La bannière du nom d'une compétence — règle dans `ro/sim/skill_banner.h`.
  bool AnnounceSkill(std::uint32_t caster_id, std::uint16_t skill_id, std::uint32_t now_ms,
                     std::uint32_t target_id = 0, std::uint32_t creature_argument = 0);

  // LE LANCEUR SUIT SA CIBLE DES YEUX pendant son incantation — à chaque battement.
  void TrackCastTargets(std::uint32_t now_ms);

  // Note qu'un acteur COMMENCE une incantation, et tourne le lanceur vers sa
  // cible quand il en vise une. Une incantation en remplace une autre : le
  // serveur n'annonce pas la fin de la précédente. Rend faux si personne ne
  // portait cet identifiant.
  bool RecordCast(std::uint32_t id, const Cast& cast);

  // ARRÊTE l'incantation d'un acteur à cet instant : le serveur dit qu'elle est
  // INTERROMPUE. Elle reste connue, sa durée ramenée à ce qui en a été joué : le
  // repos qui suit sait qu'on la quitte. Rend faux si cet acteur n'en avait pas.
  bool CancelCast(std::uint32_t id, std::uint32_t now_ms);
  // Pose la jauge d'un acteur, ou la retire si elle est vide. Faux pour un acteur hors de vue.
  bool SetGauge(std::uint32_t id, std::optional<SkillGauge> gauge);

  // Note ce qu'un acteur DIT, et l'instant. Un locuteur inconnu — zéro, ou
  // quelqu'un qu'on n'a jamais vu — ne fait qu'entrer dans la conversation :
  // les réponses du serveur arrivent par le même paquet que sa propre parole.
  // ⚠ LE GENRE EST LE PREMIER ARGUMENT, SANS DÉFAUT : un défaut rangerait sous
  // « serveur » ce qu'un appelant pressé oublierait de qualifier, et la ligne
  // paraîtrait dans le mauvais onglet sans rien casser. Le TYPE se déduit du
  // genre (`DefaultChatType`), ou se donne quand l'appelant en sait plus ;
  // l'AUTEUR, quand le serveur l'a décrit à part du texte.
  void RecordChat(ChatKind kind, ChatType type, std::uint32_t speaker_id, std::string text,
                  std::uint32_t said_ms, std::uint32_t color = 0,
                  std::optional<ChatAuthor> author = std::nullopt);
  void RecordChat(ChatKind kind, std::uint32_t speaker_id, std::string text, std::uint32_t said_ms,
                  std::uint32_t color = 0) {
    RecordChat(kind, DefaultChatType(kind), speaker_id, std::move(text), said_ms, color);
  }
  // Un CHUCHOTEMENT, reçu, renvoyé en écho ou refusé : il nomme l'autre.
  void RecordWhisper(WhisperFacts facts, std::string text, std::uint32_t said_ms);

  // La conversation, de la plus ANCIENNE ligne à la plus récente. Elle traverse
  // les changements de carte : ce qui a été dit reste dit.
  [[nodiscard]] std::span<const ChatLine> chat_lines() const noexcept { return chat_lines_; }
  // Le rang de la DERNIÈRE ligne dite, évictions comprises. Zéro tant que
  // personne n'a rien dit.
  [[nodiscard]] std::uint64_t last_chat_seq() const noexcept { return chat_lines_seen_; }

  // Combien de lignes garder PAR GENRE. ⚠ N'ÉLAGUE PAS CE QUI EST DÉJÀ LÀ :
  // abaisser la borne ne doit pas faire disparaître sous les yeux du joueur ce
  // qu'il est en train de lire. Le trop-plein part au fil des lignes qui
  // arrivent, comme d'habitude.
  void SetChatLinesKeptPerKind(int kept) noexcept {
    kept_per_kind_ = std::clamp(kept, kFewestChatLinesKept, kMostChatLinesKept);
  }
  [[nodiscard]] int chat_lines_kept_per_kind() const noexcept { return kept_per_kind_; }

  // LES CANAUX DU SERVEUR, dans l'ordre où il les envoie : la carte, l'alliance,
  // puis ceux de sa configuration — le même ordre que « @channel list ».
  //
  // 🔴 REMPLACE, ET NE S'AJOUTE PAS. Le serveur envoie la liste ENTIÈRE, déjà
  // filtrée par les droits du joueur ; l'accumuler garderait un canal qu'un
  // changement de groupe vient de retirer.
  //
  // ⚠ ELLE SURVIT AU CHANGEMENT DE CARTE, et c'est voulu : elle ne part qu'au
  // login, et la vider laisserait un sélecteur de conversation nu pour le reste
  // de la séance. Le nom du canal de carte est celui du gabarit — `#map` — le
  // même partout ; seule son EXISTENCE varierait, et elle ne varie pas.
  void TellChatChannels(std::vector<ChatChannel> channels) { chat_channels_ = std::move(channels); }
  [[nodiscard]] std::span<const ChatChannel> chat_channels() const noexcept {
    return chat_channels_;
  }

  // Le nom du personnage joué. Il ne vient d'aucun paquet du monde mais de la
  // phase de connexion, et le serveur l'exige en tête de chaque parole.
  void set_player_name(std::string name) { player_name_ = std::move(name); }
  [[nodiscard]] const std::string& player_name() const noexcept { return player_name_; }

  // CE QUE LE SERVEUR DIT DE LA SESSION : niveau de groupe, vitesse, états de
  // staff. Voir `SessionSettings`.
  [[nodiscard]] SessionSettings& session() noexcept { return session_; }
  [[nodiscard]] const SessionSettings& session() const noexcept { return session_; }
  [[nodiscard]] std::uint8_t group_level() const noexcept { return session_.group_level; }

  // Le GRIMOIRE du joueur : ce qu'il sait lancer.
  //
  // Il ne suit pas la carte, et Clear ne l'efface pas : ce que l'on sait ne
  // s'oublie pas en changeant de carte. Le serveur le redit à chaque entrée,
  // mais plus tard que la carte elle-même, et le vider laisserait un grimoire
  // blanc entre les deux annonces.
  //
  // Ouvert en écriture, comme la conversation ne l'est pas : c'est ro_net qui
  // le remplit, à partir de paquets dont le monde ne sait rien.
  [[nodiscard]] const SkillBook& skill_book() const noexcept { return skill_book_; }
  [[nodiscard]] SkillBook& skill_book() noexcept { return skill_book_; }

  // LE SAC, LA CHARRETTE ET LE COFFRE, pour les mêmes raisons que le grimoire :
  // ouverts en écriture parce que c'est ro_net qui les remplit, et non effacés
  // par Clear parce qu'ils ne suivent pas la carte.
  //
  // ⚠ TROIS CONTENANTS SÉPARÉS, et pas un seul indexé : le serveur les envoie
  // par les mêmes paquets, et les mêmes numéros de case s'y répètent. Les mêler
  // ferait qu'une potion du coffre écrase celle du sac.
  //
  // Le coffre est une SESSION : il se vide quand le serveur le ferme, alors que
  // les deux autres durent autant que le personnage.
  [[nodiscard]] const Inventory& inventory() const noexcept { return inventory_; }
  [[nodiscard]] Inventory& inventory() noexcept { return inventory_; }
  [[nodiscard]] const Inventory& cart() const noexcept { return cart_; }
  [[nodiscard]] Inventory& cart() noexcept { return cart_; }
  [[nodiscard]] const Inventory& storage() const noexcept { return storage_; }
  [[nodiscard]] Inventory& storage() noexcept { return storage_; }
  [[nodiscard]] const CardComposition& composition() const noexcept { return composition_; }
  [[nodiscard]] CardComposition& composition() noexcept { return composition_; }
  [[nodiscard]] WarpMenu& warp_menu() noexcept { return warp_menu_; }
  // L'ATELIER : listes de fabrication et d'affinage — voir `CraftSession`.
  [[nodiscard]] const CraftSession& craft() const noexcept { return craft_; }
  [[nodiscard]] CraftSession& craft() noexcept { return craft_; }

  // ── LE COFFRE EST UNE SESSION, PAS UN CONTENANT QUI DORT ──────────────────
  //
  // 🔴 ET CET ÉTAT DÉBORDE DE SA PROPRE FENÊTRE : tant qu'un coffre est ouvert,
  // le serveur REFUSE les échanges entre le sac et la charrette
  // (`sd->state.storage_flag`), et il les refuse SANS RIEN DIRE. Qui ne suit pas
  // l'ouverture propose des gestes qui échouent en silence.
  //
  // Le contenu ne suffit pas à la deviner : un coffre ouvert et vide et un
  // coffre fermé ont tous deux zéro objet.
  [[nodiscard]] bool storage_open() const noexcept { return storage_open_; }

  // Le serveur vient d'ouvrir un coffre. Le nom qu'il en donne est déjà dans le
  // contenant : c'est la seule chose qui sépare le stockage personnel de celui
  // de la guilde.
  void OpenStorage() noexcept { storage_open_ = true; }

  // La session est finie : le contenu s'en va avec elle.
  //
  // ⚠ ON VIDE, et ce n'est pas ce qu'on fait d'une charrette : le coffre existe
  // toujours, mais son contenu ne nous est plus annoncé. Le garder afficherait
  // une liste qui ne bouge plus et que rien ne corrige — pire qu'un coffre vide.
  void CloseStorage();

  // ── LA BANQUE : un coffre à zeny, au compte — voir `BankSession` ──────────
  [[nodiscard]] BankSession& bank() noexcept { return bank_; }
  [[nodiscard]] const BankSession& bank() const noexcept { return bank_; }

  // CE QUE LE PERSONNAGE PEUT APPELER : chariot, monture, faucon.
  //
  // ⚠ IL NE SUIT PAS LA CARTE. Le serveur le pousse au login vérifié, puis à
  // chaque changement — c'est-à-dire aussi quand une Kafra pose le chariot, les
  // scripts passant par les mêmes poseurs. Le vider en changeant de carte
  // laisserait la fiche sans ses cases jusqu'au prochain geste.
  void TellCompanions(CompanionState told) { companions_ = told; }
  [[nodiscard]] const CompanionState& companions() const noexcept { return companions_; }

  // ── LES COFFRES QUE CE PERSONNAGE PEUT OUVRIR ──────────────────────────────
  //
  // 🔴 LA LISTE ET L'OUVERT ARRIVENT ENSEMBLE, dans la même trame, et c'est ce qui les garde
  // d'accord. Le serveur la renvoie À CHAQUE OUVERTURE, avec le numéro du coffre qu'il vient
  // d'ouvrir : les poser séparément laisserait un onglet marqué sur un autre coffre.
  //
  // ⚠ `open_storage_id()` NE REMPLACE PAS `storage_open()` : le premier dit LEQUEL, le
  // second QU'IL Y EN A UN, et c'est lui qui décide de ce que le serveur refuse en silence.
  // Sans cette liste, le client sait donc quand même refermer.
  // CE QUE LE SERVEUR DIT DES OBJETS DU COFFRE — prix de revente, sous-type,
  // emplacement. ⚠ IL EST SEUL À LES CONNAÎTRE : ils vivent dans sa base, et la
  // table de l'installation n'en porte aucun. Voir `ro/sim/storage_facts.h`.
  void TellStorageFacts(std::vector<StorageItemFacts> told) {
    storage_facts_.Tell(std::move(told));
  }
  [[nodiscard]] const StorageFacts& storage_facts() const noexcept { return storage_facts_; }

  // LES COULEURS QUE LES JOUEURS ONT COMPOSÉES. ⚠ À PART DES ACTEURS : un lot
  // arrive à l'entrée sur une carte peuplée, et il peut décrire un joueur dont
  // l'acteur n'existe pas encore chez nous. Voir `ro/sim/style.h`.
  [[nodiscard]] const StyleBook& styles() const noexcept { return styles_; }
  [[nodiscard]] StyleBook& styles() noexcept { return styles_; }

  // CE QUE LE SERVEUR SEUL SAIT D'UNE COMPÉTENCE : son cast, ses dégâts.
  //
  // ⚠ ILS SE DEMANDENT, ILS N'ARRIVENT PAS SEULS — voir `ro/net/skill_facts.h`.
  // Une fenêtre qui les montre doit savoir se dessiner sans eux.
  void TellSkillCastFacts(std::uint32_t skill_id, std::vector<SkillCastFacts> levels) {
    skill_facts_.TellCast(skill_id, std::move(levels));
  }
  void TellSkillDamageFacts(SkillDamageFacts told) { skill_facts_.TellDamage(std::move(told)); }
  [[nodiscard]] const SkillFacts& skill_facts() const noexcept { return skill_facts_; }

  void TellStorages(std::vector<StorageShelf> shelves, std::uint8_t open_id);
  [[nodiscard]] std::span<const StorageShelf> storage_shelves() const noexcept {
    return storage_shelves_;
  }
  [[nodiscard]] std::uint8_t open_storage_id() const noexcept { return open_storage_id_; }

  // Le contenant que désigne un tenant, pour du code qui n'en connaît pas le nom
  // — typiquement le décodeur, qui lit lequel dans le paquet.
  [[nodiscard]] Inventory& HolderFor(ItemHolder holder) noexcept;

  // Pose une unité au sol, ou remplace celle qui portait cet identifiant.
  void PlaceSkillUnit(const SkillUnit& unit);

  // Retire l'unité au sol qui portait cet identifiant. Faux s'il n'y en avait
  // pas : le même paquet sert à retirer d'autres choses.
  bool RemoveSkillUnit(std::uint32_t id);

  // L'unité au sol DIT ce texte : une Talkie Box qui se déclenche. Faux si
  // aucune unité ne porte cet identifiant.
  bool SaySkillUnit(std::uint32_t id, std::string text, std::uint32_t now_ms);

  [[nodiscard]] const std::map<std::uint32_t, SkillUnit>& skill_units() const noexcept {
    return skill_units_;
  }

  // Note l'EFFET joué sur un acteur, et l'instant. Rejoué, il repart de zéro
  // sans s'ajouter une seconde fois. Faux si personne ne porte cet identifiant.
  // La VALEUR est celle du paquet qui en porte une ; l'ÉTAT, celui qui l'a posé.
  // Renoté sans eux, l'effet les perd. Au-delà de kEffectsKeptPerActor, le plus
  // ancien s'en va.
  bool RecordEffect(std::uint32_t id, std::uint32_t effect_id, std::uint32_t since_ms,
                    std::optional<std::int32_t> value = std::nullopt,
                    std::optional<StatusEffectId> posed_by_state = std::nullopt);
  // La même note pour un effet D'OFFICE : le plafond ne le chasse jamais.
  bool RecordLastingEffect(std::uint32_t id, std::uint32_t effect_id, std::uint32_t since_ms);
  // La même note pour un effet que le client règle lui-même : couleur, échéance.
  bool RecordPlayedEffect(std::uint32_t id, const PlayedEffect& played);
  // Un effet LÀ OÙ CET ACTEUR SE TROUVE à cet instant, qu'il s'en aille ou non.
  bool NoteDeparture(std::uint32_t id, std::uint32_t effect_id, std::uint32_t now_ms);
  [[nodiscard]] Departures& departures() noexcept { return departures_; }

  // Retire d'un acteur l'effet que le serveur nomme. Faux si personne ne
  // portait cet identifiant, ou s'il ne portait pas cet effet-là.
  //
  // Le SEUL retrait venu du serveur (removespecialeffect) nomme l'effet, pas
  // son rang : un effet recouvert se retire quand même. Il s'inscrit TOUJOURS
  // au journal de l'acteur, qui coupe l'instance qui joue.
  bool RemoveEffect(std::uint32_t id, std::uint32_t effect_id);

  // Ôte un effet de la liste d'un acteur SANS l'inscrire au journal des
  // retraits. C'est le retrait que les effets règlent eux-mêmes, après avoir
  // coupé leur instance : l'inscrire le ferait relire comme un ordre du
  // serveur, qui couperait l'instance reposée depuis. Faux si personne ne
  // portait cet identifiant, ou s'il ne portait pas cet effet-là.
  bool ForgetEffect(std::uint32_t id, std::uint32_t effect_id);

  // Note la PORTÉE qu'un lanceur annonce, ou la remplace. Faux pour un inconnu.
  bool RecordSkillScale(std::uint32_t id, const SkillScale& scale);

  // Note l'émoticône qu'un acteur montre, et l'instant. La MÊME émoticône
  // redite repart de zéro : on la répète, et elle rejoue. Faux pour un inconnu.
  bool RecordEmotion(std::uint32_t id, Emotion emotion, std::uint32_t since_ms);

  // BLADE STOP : le moine `caster_id` saisit la lame de `target_id`, ou la
  // relâche. Une prise ne se pose que si les deux sont connus ; un relâchement
  // libère chacun de ceux qui le sont. Rend faux si aucun ne l'est.
  bool SetBladeStop(std::uint32_t caster_id, std::uint32_t target_id, bool active,
                    std::uint32_t now_ms);

  // DEVOTION : la liste ENTIÈRE des protégés d'un acteur, sous un numéro
  // d'annonce neuf — voir `DevotionList`. Faux pour un inconnu.
  bool TellDevotion(std::uint32_t devoter_id,
                    const std::array<std::uint32_t, kDevotionSlots>& devotee_ids);

  // Pose les états d'un acteur, et son mode PK, que le même paquet porte.
  // L'instant ne se note que si le corps change. Faux pour un inconnu.
  bool SetStates(std::uint32_t id, std::uint16_t body_state, std::uint16_t health_state,
                 std::uint32_t option, std::uint32_t now_ms, bool pk_mode = false);

  // Pose LE NOM ET SA CARTE. Rend faux si personne ne portait cet identifiant.
  //
  // 🔴 LE NOM DE LA CARTE L'EMPORTE SUR CELUI DE L'APPARITION, et c'est le seul
  // qui soit à jour. Le paquet d'unité porte le nom que la base contient ;
  // celui-ci porte ce que le serveur veut MONTRER — un @fakename, une étiquette
  // d'absence posée devant le pseudo, le nom masqué d'un siège. Garder le
  // premier montrerait quelqu'un qui se cache.
  //
  // ⚠ UN NOM VIDE NE S'ÉCRASE PAS. Le serveur n'envoie pas de carte sans nom,
  // mais un paquet tronqué ou un acteur sans nom en donnerait une : effacer le
  // nom de l'apparition laisserait un acteur anonyme que plus rien ne
  // renommerait.
  bool TellActorName(std::uint32_t id, std::string name, NameCard card);

  [[nodiscard]] const Actor* Find(std::uint32_t id) const;

  // Donne au monde la grille de franchissement de la carte, ou rien.
  //
  // POURQUOI le monde en a besoin. Un coup qui fait broncher sa cible lui COUPE
  // la marche, et le serveur ne l'annonce PAS : il pose l'unité sur la case la
  // plus proche de son chemin et se tait (status_fix_damage appelle
  // unit_set_walkdelay, qui appelle unit_stop_walking avec USW_MOVE_FULL_CELL ;
  // le paquet d'arrêt n'est envoyé que sous un réglage de mise au point).
  // Continuer la marche annoncée montre le monstre plusieurs cases plus loin,
  // jusqu'à ce que la prochaine annonce le ramène d'un coup — c'est la
  // téléportation qu'on voit en attaquant un monstre qui marche. Savoir OÙ il
  // s'arrête demande son chemin, donc la grille.
  //
  // Le monde ne possède pas la grille : il la DÉSIGNE, comme le planificateur
  // de chemins. Elle doit donc lui être redonnée à chaque carte, sous peine de
  // viser celle d'avant.
  void SetWalkMap(const WalkMap* map) noexcept { walk_map_ = map; }

  // Vide le monde. Un changement de carte remplace TOUT : garder les acteurs de
  // l'ancienne les ferait apparaître sur la nouvelle.
  void Clear();

  // Fait entrer le monde sur une carte, à la case donnée. Les acteurs de l'ancienne
  // s'en vont — le serveur réannonce ceux de la nouvelle — mais le JOUEUR traverse :
  // il garde son identifiant et son apparence, que le serveur ne redit pas, ainsi que
  // la direction de son regard, que le paquet ne porte pas. Ce qu'il faisait sur
  // l'ancienne — un coup porté, un coup reçu, un ramassage — s'oublie : ces faits
  // nomment des acteurs qui ne sont plus là.
  //
  // ⚠ SES EFFETS S'ÉTEIGNENT AUSSI : le client natif détruit l'acteur, et ses
  // instances d'effet avec lui. Le serveur s'appuie là-dessus pour rendre sa
  // taille à un corps rapetissé, dont l'effet ne meurt jamais de lui-même.
  void EnterMap(std::string map_name, std::uint16_t x, std::uint16_t y);

  // La carte où le monde se trouve, sans extension. Vide tant que le serveur
  // ne l'a pas dite.
  [[nodiscard]] const std::string& map_name() const noexcept { return map_name_; }

  // Combien de fois le monde est entré sur une carte.
  //
  // Un COMPTEUR et non un nom : le serveur fait aussi sauter d'un bout à
  // l'autre de la MÊME carte, et celui qui doit réagir à chaque saut — le
  // client doit lui redire qu'il a fini de charger — ne peut pas s'en
  // apercevoir en comparant des noms.
  [[nodiscard]] std::uint32_t map_entries() const noexcept { return map_entries_; }

  // CE QUE LE SERVEUR DÉCLARE DE CETTE CARTE : son mode (`MapMode`, qui porte
  // aussi ce que la déclaration NE DIT PAS) et son type
  // (`PartySeesConcealedEffects`). ⚠ REMIS À NEUF PAR `EnterMap` : le serveur ne
  // les redit qu'APRÈS l'entrée, et un mode conservé ferait viser les passants
  // de la carte suivante le temps d'un aller-retour.
  void TellMapMode(const MapMode& mode) noexcept { map_mode_ = mode; }
  [[nodiscard]] const MapMode& map_mode() const noexcept { return map_mode_; }
  void TellMapType(std::uint16_t map_type) noexcept { map_type_ = map_type; }
  [[nodiscard]] std::uint16_t map_type() const noexcept { return map_type_; }

  // LE CLASSEMENT PVP DE CETTE CARTE. Le chronomètre du premier et le carnet des
  // rangs vivent dans `PvpStanding` ; ne reste ici que ce qui touche aux acteurs.
  [[nodiscard]] PvpStanding& pvp_standing() noexcept { return pvp_standing_; }
  [[nodiscard]] const PvpRank& pvp_rank() const noexcept { return pvp_standing_.rank(); }

  // LE RANG D'UN ACTEUR QUELCONQUE, RETENU même si personne ne le porte encore —
  // voir `PvpStanding::NoteActor`. Rend faux quand l'acteur n'est pas en vue.
  bool TellActorPvpRank(std::uint32_t id, std::int32_t rank, std::uint32_t now_ms);

  // L'ANNEAU DES DIX PREMIERS, ACCORDÉ AU RANG DE CHACUN à chaque battement :
  // une CONSÉQUENCE muette tant que rien ne bouge — voir `PvpStanding`.
  void RefreshPvpRings(std::uint32_t now_ms);

  // LE COMPTEUR DE DÉGÂTS, nourri par nos coups — et par le paquet privé du
  // serveur pour les sorts de zone, dont l'unité est la source. Voir `DamageMeter`.
  [[nodiscard]] DamageMeter& damage_meter() noexcept { return damage_meter_; }
  [[nodiscard]] const DamageMeter& damage_meter() const noexcept { return damage_meter_; }

  void set_player_id(std::uint32_t id) noexcept { player_id_ = id; }
  [[nodiscard]] std::uint32_t player_id() const noexcept { return player_id_; }
  // Le personnage du joueur. Nul tant que le serveur ne l'a pas nommé.
  [[nodiscard]] const Actor* player() const { return Find(player_id_); }

  // ── LA SÉLECTION : ce que le joueur a DÉSIGNÉ en dernier ──────────────────
  //
  // 🔴 CE N'EST PAS « CE QU'ON ATTAQUE », ni la sélection que suit la flèche du jeu : celle-là
  // vit hors du monde, avec l'engagement de la souris (ADR 0064). La DÉSIGNATION du mode
  // Ciblage s'allume à un geste et reste quand on cesse de frapper.
  //
  // 🔴 ET UNE CIBLE QUI S'EN VA EST PERDUE. Sortie du champ de vision ou cachée, le serveur la
  // fait disparaître : la désignation s'éteint, et le retour de la même entité ne la rallume
  // pas — il faut la redésigner.
  //
  // ⚠ C'EST POURQUOI C'EST UN CHAMP ET NON UNE RECHERCHE. Déduite de la présence de l'acteur,
  // elle se rallumerait toute seule dès que l'identifiant reparaît. Sur une entité qui se
  // CACHE, ce ne serait pas une gêne mais un détecteur : le Hiding retire l'acteur, et une
  // désignation qui repousse derrière lui dirait où il est.
  // Rend faux si personne ne porte cet identifiant, et la désignation
  // précédente TIENT. Cliquer à côté ne défait pas ce qu'on a désigné : seul
  // `Deselect` le fait. Redésigner la même entité garde son dossier.
  bool Select(std::uint32_t id);

  void Deselect() noexcept { selected_id_ = 0; }

  // Zéro quand rien n'est désigné. ⚠ TANT QU'IL N'EST PAS ZÉRO, L'ACTEUR EXISTE :
  // c'est ce que `Select`, `Remove` et les deux purges tiennent ensemble, et
  // c'est ce qui permet de croire `selected()` sans le recouper.
  [[nodiscard]] std::uint32_t selected_id() const noexcept { return selected_id_; }
  [[nodiscard]] const Actor* selected() const { return Find(selected_id_); }

  // ── LE DOSSIER DE LA CIBLE ────────────────────────────────────────────────
  // Ce que le serveur a dit de la désignation en cours. ⚠ IL SE VIDE À CHAQUE
  // CHANGEMENT DE DÉSIGNATION, et il le faut : les points de vie de l'entité
  // d'avant resteraient à l'écran sous le nom de la nouvelle, le temps qu'une
  // réponse arrive.
  [[nodiscard]] const TargetDetail& target_detail() const noexcept { return target_detail_; }

  // Pose ce que le couple de la cible a répondu.
  // Rend faux si `told.id` n'est pas la désignation en cours : on ne demande
  // qu'elle, et une réponse en retard décrirait l'entité d'avant.
  //
  // ⚠ LES ÉTATS DÉJÀ CONNUS DE LA MÊME ENTITÉ SURVIVENT : ils viennent d'un
  // autre paquet, et les écraser ferait clignoter la rangée à chaque réponse.
  bool TellTarget(TargetDetail told);

  // Même règle, pour le paquet des états. ⚠ UNE LISTE VIDE SE POSE QUAND MÊME :
  // c'est « aucun état », et le dossier le distingue de « pas encore demandé ».
  bool TellTargetStatuses(std::uint32_t id, std::vector<ActiveStatus> statuses);
  // Le serveur a refusé la liste des états : un de plus au compte du dossier.
  bool RefuseTargetStatuses(std::uint32_t id);

  [[nodiscard]] std::size_t size() const noexcept { return actors_.size(); }
  [[nodiscard]] const std::unordered_map<std::uint32_t, Actor>& actors() const noexcept {
    return actors_;
  }
  [[nodiscard]] const QuestMarkers& quest_markers() const noexcept { return quest_markers_; }
  [[nodiscard]] QuestMarkers& quest_markers() noexcept { return quest_markers_; }
  // LE CHAMP DE BATAILLE DE LA CARTE, vidé quand la carte change : `Battlefield`.
  [[nodiscard]] const Battlefield& battlefield() const noexcept { return battlefield_; }
  [[nodiscard]] Battlefield& battlefield() noexcept { return battlefield_; }

 private:
  // Repose un acteur et annule sa marche, en gardant le compte de ce qu'il a
  // parcouru : voir Actor::travelled_before_cells.
  void Halt(Actor& actor, CellPosition where);

  std::unordered_map<std::uint32_t, Actor> actors_;
  const WalkMap* walk_map_ = nullptr;
  Departures departures_;
  std::map<std::uint32_t, SkillUnit> skill_units_;
  std::vector<ChatLine> chat_lines_;
  // Combien de lignes ont été dites depuis le début, évictées comprises : c'est
  // le rang de la dernière.
  std::uint64_t chat_lines_seen_ = 0;
  int kept_per_kind_ = kChatLinesKeptPerKind;
  std::optional<NpcDialog> npc_dialog_;
  SkillBook skill_book_;
  Inventory inventory_;
  Inventory cart_;
  Inventory storage_;
  CardComposition composition_;
  WarpMenu warp_menu_;
  CraftSession craft_;
  // ⚠ ELLE SURVIT À LA FERMETURE D'UN COFFRE ET AU CHANGEMENT DE CARTE : le serveur ne la
  // repousse qu'au login et à chaque ouverture, et la vider priverait la fenêtre de ses onglets.
  CompanionState companions_;
  std::vector<StorageShelf> storage_shelves_;
  StorageFacts storage_facts_;
  StyleBook styles_;
  QuestMarkers quest_markers_;
  Battlefield battlefield_;
  SkillFacts skill_facts_;
  std::uint8_t open_storage_id_ = kNoStorageOpen;
  std::vector<ChatChannel> chat_channels_;
  std::string player_name_;
  SessionSettings session_;
  // Le seul passage qui change la désignation : voir `world.cpp`.
  void Designate(std::uint32_t id);
  void RefreshPvpRing(std::uint32_t id, std::uint32_t now_ms);

  std::uint32_t player_id_ = 0;
  // La dernière annonce de DEVOTION numérotée : voir `DevotionList::told_serial`.
  std::uint32_t devotion_serial_ = 0;
  DamageMeter damage_meter_;
  StrikeLog strikes_;
  SupportLog supports_;
  UnseenGroundSkillLog unseen_ground_skills_;
  std::uint32_t selected_id_ = 0;
  TargetDetail target_detail_;
  PlayerStatus player_status_;
  PlayerBonuses player_bonuses_;
  std::vector<ActiveStatus> player_statuses_;
  std::string map_name_;
  std::uint32_t map_entries_ = 0;
  MapMode map_mode_;
  std::uint16_t map_type_ = 0;
  PvpStanding pvp_standing_;
  ServerClock server_clock_;
  bool storage_open_ = false;
  // ⚠ LE SOLDE SURVIT À LA FERMETURE DE LA FENÊTRE : d'autres écrans le lisent, et il n'est
  // redit qu'à la réouverture.
  BankSession bank_;
};

}  // namespace ro::sim
