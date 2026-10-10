#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/emblem_canvas.h"
#include "ro/ui/skill_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── L'ONGLET GUILDE DE LA FICHE ─────────────────────────────────────────────
//
// La fenêtre de guilde du client d'origine, refaite dans la fiche : l'en-tête
// et son emblème, l'annonce, cinq sous-onglets — membres, postes, compétences,
// relations, expulsions —, la barre d'actions, et l'éditeur d'emblème.
//
// ⚠ ELLE NE DÉCIDE D'AUCUN DROIT. `app` dit ce que le poste du joueur permet,
// et la fenêtre ne montre que les gestes qu'il autorise ; le serveur, de toute
// façon, revalide chacun.
//
// ⚠ TOUT TEXTE ARRIVE EN UTF-8, et tout ce que le joueur tape repart en UTF-8 :
// la conversion vers le fil est l'affaire de `net`.

// Les vingt postes d'une guilde, et la part d'expérience qu'un poste peut céder.
inline constexpr std::size_t kGuildTabPositions = 20;
inline constexpr int kGuildTabPayRateMost = 50;

struct GuildMemberView {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::string_view name;
  std::string_view job;
  int level = 0;
  std::int32_t position = 0;
  // Vide quand aucun nom n'est encore arrivé pour ce poste.
  std::string_view position_label;
  std::int32_t contribution = 0;
  bool online = false;
  // La dernière connexion, déjà écrite ; vide quand le serveur ne l'a pas.
  std::string_view last_seen;
  // Pour le tri seulement.
  std::uint32_t last_login = 0;
  // La miniature de tête, pour un membre CONNECTÉ seulement : c'est le client.
  SkinImage head;
  bool self = false;
  // C'est le maître de la guilde.
  bool master = false;
  // « Inviter dans mon carnet MVP » : la raison, déjà traduite, qui grise
  // l'entrée du menu ; nulle, elle est offerte.
  const char* mvp_refusal = nullptr;
};

struct GuildPositionView {
  std::string_view name;
  bool may_invite = false;
  bool may_expel = false;
  bool may_storage = false;
  int pay_rate = 0;
  bool has_name = false;
  // ⚠ UNE LIGNE NE S'ÉDITE QUE SI SES DROITS SONT CONNUS : la renvoyer sans eux
  // remettrait son masque à zéro chez le serveur.
  bool has_rights = false;
};

struct GuildRelationView {
  std::int32_t guild_id = 0;
  bool enemy = false;
  std::string_view name;
};

struct GuildBanView {
  std::string_view name;
  std::string_view reason;
};

// UNE COMPÉTENCE DE GUILDE : la case du grimoire, et ce que la guilde y ajoute.
struct GuildSkillView {
  // `learned` est le niveau, `raisable` dit que le serveur accepte d'en monter
  // un, `castable` qu'elle se lance sur soi.
  SkillCellView cell;
  // Le nom technique (« GD_APPROVAL »), montré à côté du libellé.
  std::string_view technical;
  // 🔴 LE SERVEUR N'ENVOIE PAS UNE COMPÉTENCE DONT LES PRÉREQUIS MANQUENT :
  // celle-ci vient de l'arbre de l'installation seulement.
  bool locked = false;
  // La profondeur dans l'arbre : sa ligne dans la grille, son retrait en liste.
  int depth = 0;
  // Zéro pour une passive — c'est le masque de nature du serveur. ⚠ SANS SENS
  // SUR UNE VERROUILLÉE, dont aucun paquet n'a dit la nature.
  bool active = false;
  // LE JOUEUR PEUT LA LANCER ET LA POSER DANS UNE BARRE — le maître, en somme.
  // Faux, l'arbre ne se consulte qu'en lecture : la description, rien d'autre.
  bool wieldable = false;
  // « Battle Orders Niv 1, Guild Extension Niv 2 », déjà composé.
  std::string_view requirements;
};

// UN FICHIER DU DOSSIER `emblem\` de l'installation.
struct EmblemFileView {
  std::string_view name;
  SkinImage preview;
  bool usable = false;
  // Le motif du refus, en clair, quand il n'est pas utilisable.
  std::string_view why;
  std::size_t bytes = 0;
  int transparency = 0;
};

// UNE ICÔNE DU SAC, pour partir d'un dessin existant.
struct EmblemItemView {
  std::uint32_t item_id = 0;
  SkinImage icon;
  std::string_view name;
};

struct GuildView {
  bool in_guild = false;

  // ── SANS GUILDE ───────────────────────────────────────────────────────────
  // La dernière réponse à une création, déjà écrite ; vide sans réponse.
  std::string_view creation_answer;
  bool creation_succeeded = false;

  // ── L'EN-TÊTE ─────────────────────────────────────────────────────────────
  std::int32_t guild_id = 0;
  std::string_view name;
  std::string_view master_name;
  std::string_view land;
  int level = 0;
  // Le nombre de membres vient de la liste, les connectés de l'en-tête.
  int member_count = 0;
  int member_most = 0;
  int online = 0;
  int average_level = 0;
  std::int64_t exp = 0;
  std::int64_t next_exp = 0;
  SkinImage emblem;

  std::string_view notice_subject;
  std::string_view notice_body;

  // ── CE QUE LE POSTE DU JOUEUR PERMET ──────────────────────────────────────
  bool master = false;
  bool position_found = false;
  std::string_view my_position;
  bool rights_known = false;
  bool may_invite = false;
  bool may_expel = false;
  bool may_storage = false;

  // ── LES SOUS-ONGLETS ──────────────────────────────────────────────────────
  // Ceux que le serveur ouvre au personnage ; un onglet fermé ne se montre pas.
  // Les compétences et les relations ne se ferment jamais.
  bool members_tab = true;
  bool positions_tab = true;
  bool bans_tab = true;
  std::span<const GuildMemberView> members;
  // Le carnet de chasse MVP est éteint : « Inviter dans mon carnet MVP » n'est
  // dans le menu d'aucun membre.
  bool mvp_invites_withdrawn = false;
  std::span<const GuildPositionView> positions;
  std::span<const GuildRelationView> relations;
  std::span<const GuildSkillView> skills;
  int skill_points = 0;
  bool skills_known = false;
  // L'arbre de l'installation a été lu : niveaux maximaux et verrouillées.
  bool skill_tree_known = false;
  std::span<const GuildBanView> bans;
  bool bans_known = false;

  // ── L'ÉDITEUR D'EMBLÈME ───────────────────────────────────────────────────
  std::span<const EmblemFileView> emblem_files;
  // Le dossier, écrit avec des « / ».
  std::string_view emblem_folder;
  std::span<const EmblemItemView> gallery;
  // L'icône de l'objet dont on a tapé le numéro, ou rien.
  SkinImage typed_item_icon;
  // Ce que le dessin courant pèserait, et son taux de transparence, au sens du
  // serveur. ⚠ CALCULÉS PAR `app` : le BMP est un format, et la fenêtre n'y a
  // pas accès.
  std::size_t canvas_bytes = 0;
  int canvas_transparency = 0;
  int transparency_limit = 0;
};

// ── CE QUE L'ONGLET GARDE D'UNE IMAGE À L'AUTRE ─────────────────────────────

// Un poste en cours d'édition.
struct GuildPositionDraft {
  std::array<char, 64> name = {};
  bool may_invite = false;
  bool may_expel = false;
  bool may_storage = false;
  int pay_rate = 0;
};

struct EmblemEditorState {
  // Ouvrir la boîte à l'image suivante — en sortant d'un menu, la pile
  // d'identifiants n'est pas celle de la boîte.
  bool ask_open = false;
  // Ouverte par un clic sur l'emblème : l'éditeur part de celui en place.
  bool load_current = false;
  bool goto_paint = false;
  int chosen_file = -1;
  std::string error;
  // Le compte rendu du dernier envoi, écrit par `app`.
  std::string report;
  int item_id = 0;
  bool gallery_open = false;
  std::array<char, 32> save_name = {};
  EmblemCanvas canvas;
};

struct GuildTabState {
  std::uint32_t selected_char = 0;
  // Le dernier compte rendu d'un geste, sous la barre d'actions.
  std::string status;

  bool notice_editing = false;
  std::array<char, 128> notice_subject = {};
  std::array<char, 256> notice_body = {};
  std::array<char, 64> invite_name = {};
  std::array<char, 64> create_name = {};
  std::array<char, 128> reason = {};

  // Les confirmations, ouvertes à l'image suivante.
  bool ask_expel = false;
  bool ask_transfer = false;
  bool ask_relation = false;
  std::uint32_t target_account = 0;
  std::uint32_t target_char = 0;
  std::string target_name;
  std::int32_t target_guild = 0;
  bool target_enemy = false;
  std::array<char, 64> disband_confirm = {};
  std::array<char, 64> transfer_confirm = {};

  // Les postes : tant qu'une saisie est en cours, la copie ne se resynchronise
  // plus, sinon la frappe serait écrasée par le rafraîchissement suivant.
  bool positions_editing = false;
  std::array<GuildPositionDraft, kGuildTabPositions> positions = {};

  // Les compétences : grille ou liste, et le survol de l'image précédente.
  bool skill_grid = true;
  std::uint16_t skill_hover = 0;
  // Un clic se départage d'un double-clic : l'identifiant pressé, et s'il est
  // resté propre — ni glissé, ni tiré.
  std::uint16_t skill_pressed = 0;
  bool skill_press_clean = false;

  // L'ordre du tableau des membres.
  std::vector<int> member_order;

  EmblemEditorState emblem;
};

// ── CE QUE LE JOUEUR A DEMANDÉ ──────────────────────────────────────────────

enum class GuildVerb : std::uint8_t {
  kNothing,
  kRefresh,
  kCreate,
  kSaveNotice,
  kOpenStorage,
  kWhisper,
  // La conversation 1:1 à part, avec le membre nommé dans `text`.
  kWhisperWindow,
  // « Envoyer un courrier… » : une écriture adressée au membre (`text`).
  kSendMail,
  kChangePosition,
  kTransferMaster,
  kExpel,
  kLeave,
  kDisband,
  kInvite,
  kEndRelation,
  kSavePositions,
  kSkillUp,
  kUseSkill,
  kDescribeSkill,
  // L'éditeur d'emblème.
  kOpenEmblemEditor,
  kScanEmblemFolder,
  kUploadEmblemFile,
  kUploadCanvas,
  kSaveCanvas,
  kLoadCurrentEmblem,
  kLoadFileIntoCanvas,
  kImportItemIcon,
  // « Inviter dans mon carnet MVP » : le membre nommé dans `text`.
  kInviteMvp,
};

struct GuildPositionEditView {
  std::int32_t position = 0;
  std::string name;
  bool may_invite = false;
  bool may_expel = false;
  bool may_storage = false;
  int pay_rate = 0;
};

struct GuildAction {
  GuildVerb verb = GuildVerb::kNothing;
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::int32_t position = 0;
  std::int32_t guild_id = 0;
  bool enemy = false;
  // Le nom, le motif ou le fichier, selon le verbe.
  std::string text;
  std::string subject;
  std::string body;
  std::vector<GuildPositionEditView> positions;
  std::uint16_t skill_id = 0;
  int skill_level = 0;
  std::uint32_t item_id = 0;
  int file = -1;
  // `kOpenEmblemEditor` : l'éditeur part de l'emblème en place.
  bool from_current = false;
};

// ⚠ `smooth_icons` EST LE RÉGLAGE DU GRIMOIRE, partagé : une seule préférence
// « icônes de compétence ».
//
// `staff_allowed` est le droit de la fiche, relu à chaque image : il ajoute
// l'identifiant au survol d'une compétence, voir `ro/ui/staff_skill_id.h`.
[[nodiscard]] GuildAction DrawGuildTab(GuildTabState& state, const GuildView& view,
                                       bool& smooth_icons, const SkinArt& art,
                                       bool staff_allowed = false);

// L'invitation reçue : « Untel t'invite dans sa guilde ». Rend vrai si le
// joueur a répondu, et `accepted` dit comment.
//
// ⚠ HORS DE LA FICHE : l'invitation arrive fenêtre fermée, et doit se voir.
[[nodiscard]] bool DrawGuildInvitation(std::string_view guild_name, bool& accepted,
                                       const SkinArt& art);

}  // namespace ro::ui
