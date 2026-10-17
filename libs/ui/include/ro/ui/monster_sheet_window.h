#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {

// ── LA FICHE D'UN MONSTRE ───────────────────────────────────────────────────
//
// Elle remplace la fenêtre « Monster
// Info » que le skill Sense ouvre, et va bien au-delà : ce que le jeu d'origine
// en montre — neuf champs, neuf résistances, une image figée — devient une
// fiche entière, que le serveur remplit sur demande.
//
// C'est un panneau de DESCRIPTION, pas une fenêtre ordinaire : on la lit, on ne
// la manipule pas. Elle en porte donc l'habillage — barre claire, corps blanc.
//
// Cette couche dessine et rend ce qui a été demandé ; `app` compose la vue,
// anime le sprite, joue ses sons et route les gestes.

inline constexpr const char* kMonsterSheetWindowId = "monster_sheet";

// Le nombre de résistances, une par élément d'attaque.
inline constexpr std::size_t kMonsterSheetResistCount = 10;

// Où en est la fiche affichée.
enum class MonsterSheetPhase : std::uint8_t {
  // La demande est partie, rien n'est revenu.
  kAsking,
  // Le serveur ne connaît pas ce monstre.
  kUnknown,
  kReady,
};

struct MonsterSheetDropView {
  std::uint32_t item_id = 0;
  // Le nom venu du serveur, en UTF-8 ; vide s'il n'en a pas.
  std::string name;
  SkinImage icon;
  // En centièmes de pour cent.
  std::uint32_t rate_hundredths = 0;
  bool mvp_reward = false;
};

struct MonsterSheetSpawnView {
  // Le nom interne, celui que les trois gestes de la ligne emportent.
  std::string map_name;
  // Ce que la ligne écrit : « Nom affiché (fichier) », ou le nom interne seul
  // quand l'installation ne nomme pas la carte.
  std::string label;
  std::uint16_t count = 0;
};

struct MonsterSheetSkillView {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;
  // Le nom de l'installation quand elle le connaît, celui du serveur sinon, et
  // le numéro en dernier recours. Jamais vide.
  std::string name;
  // L'installation a une DESCRIPTION pour elle : seule condition pour que la
  // ligne soit un lien, puisque c'est tout ce qu'un clic montrerait.
  bool describable = false;
};

// Ce que ce monstre rapporte au personnage qui regarde, calculé par le serveur.
struct MonsterSheetExpView {
  std::uint32_t base_exp = 0;
  std::uint32_t job_exp = 0;
  std::uint32_t next_base_exp = 0;
  std::uint32_t next_job_exp = 0;
  bool base_level_maxed = false;
  bool job_level_maxed = false;
};

// Les statistiques FINALES du personnage — équipement et états compris —,
// celles de sa fiche. ⚠ LE CRITIQUE ET L'ESQUIVE PARFAITE SONT EN POUR CENT
// ENTIERS, tels que le serveur les envoie.
struct MonsterSheetOwnView {
  int hit = 0;
  int flee = 0;
  int critical_percent = 0;
  int perfect_dodge_percent = 0;
};

// Ce que le skill Sense a relevé sur l'exemplaire croisé.
struct MonsterSheetSenseView {
  std::uint32_t sprite_class = 0;
  int level = 0;
  std::uint32_t hp = 0;
  int def = 0;
  int mdef = 0;
};

// LES MOTS DE LA TABLE DE MESSAGES DU CLIENT, jamais une paraphrase : race,
// élément et taille dans la langue de l'installation, comme la fenêtre du jeu.
struct MonsterSheetWords {
  std::string_view level;
  std::string_view race;
  std::string_view element;
  std::string_view size;
  std::string_view def;
  std::string_view mdef;
  std::string_view race_value;
  std::string_view element_value;
  std::string_view size_value;
  // Le nom de chaque élément d'attaque, dans l'ordre des résistances.
  std::array<std::string_view, kMonsterSheetResistCount> resist;
};

// L'EXP écrite dans la base et les taux du serveur, pour l'infobulle de l'EXP.
struct MonsterSheetWrittenExpView {
  std::uint32_t base_exp = 0;
  std::uint32_t job_exp = 0;
  std::uint32_t mvp_exp = 0;
  std::int64_t base_rate_percent = 0;
  std::int64_t job_rate_percent = 0;
  std::int64_t mvp_rate_percent = 0;
};

struct MonsterSheetView {
  MonsterSheetPhase phase = MonsterSheetPhase::kAsking;
  // Ce que le menu d'un item sait d'avance : vote shop, album.
  ItemLinkFacts link_facts;
  // L'identifiant demandé, celui qu'on montre en « #… ».
  std::uint32_t shown_id = 0;

  // ── L'EN-TÊTE ─────────────────────────────────────────────────────────────
  std::string name;
  // « événement » ou « invoqué », DÉJÀ TRADUIT, ou nul quand rien n'est sûr.
  const char* qualifier = nullptr;
  // 0 ordinaire, 1 mini-boss, 2 MVP.
  std::uint8_t boss_rank = 0;
  // Le sprite, déjà composé à la taille de son cadre. Inutilisable quand il n'y
  // a rien à montrer — ce qui arrive aussi sur l'image vide d'une mort.
  SkinImage sprite;
  // Le sursaut du sprite titillé, en pixels, vers la droite.
  float sprite_shake_px = 0.0F;
  // Ni `.spr` ni `.act` : un monstre sans art, ou un acteur 3D.
  bool sprite_missing = false;
  // 🔴 L'ACTEUR 3D N'EST PAS « SANS SPRITE » : l'Emperium, les gardiens, les
  // coffres ont un modèle, pas d'image. Écrire « pas de sprite » serait exact
  // au mot près et faux sur le fond.
  bool sprite_is_model = false;
  // Poser le monstre dans la barre de chat est possible.
  bool can_post_link = false;
  // « Base de données » : la fiche est seule, et la base peut la reprendre.
  bool database_link = false;
  MonsterSheetWords words;

  // ── LES STATISTIQUES ──────────────────────────────────────────────────────
  int level = 0;
  int element_level = 0;
  std::uint32_t mode = 0;
  std::uint32_t hp = 0;
  std::uint32_t sp = 0;
  std::uint32_t base_exp = 0;
  std::uint32_t job_exp = 0;
  std::uint32_t mvp_exp = 0;
  int atk_min = 0;
  int atk_max = 0;
  int matk_min = 0;
  int matk_max = 0;
  int def = 0;
  int def_bonus = 0;
  int mdef = 0;
  int mdef_bonus = 0;
  int str = 0;
  int agi = 0;
  int vit = 0;
  int int_stat = 0;
  int dex = 0;
  int luk = 0;
  int attack_range_cells = 0;
  // Le temps de marche d'une case ; zéro pour un monstre immobile.
  int walk_ms_per_cell = 0;
  std::array<int, kMonsterSheetResistCount> resist_percent{};
  // FAUX QUAND LA FICHE VIENT DE LA BASE SEULE, que le serveur n'a pas encore
  // complétée : la base ne porte pas les résistances, et des zéros mentiraient.
  // L'onglet n'existe alors pas.
  bool resist_known = true;
  std::optional<MonsterSheetExpView> exp_gain;
  // L'EXP de la base, avant les taux du serveur : l'infobulle de l'EXP réelle.
  std::optional<MonsterSheetWrittenExpView> written_exp;
  // Absentes tant que le serveur n'a rien dit du personnage.
  std::optional<MonsterSheetOwnView> own;

  // ── L'IDENTITÉ ────────────────────────────────────────────────────────────
  std::string aegis_name;
  int namesake_count = 1;
  std::uint32_t namesake_origin = 0;

  // Le dernier relevé de Sense, s'il y en a un.
  std::optional<MonsterSheetSenseView> sense;
  // La classe de sprite de la fiche, que le relevé doit viser pour compter.
  std::uint32_t sprite_class = 0;

  std::span<const MonsterSheetDropView> drops;
  std::span<const MonsterSheetSpawnView> spawns;
  std::span<const MonsterSheetSkillView> skills;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct MonsterSheetWindowState {
  bool open = false;
  // Passer devant à l'image suivante : une fiche vient d'être demandée.
  bool raise = false;
  // Le titre en cache, recomposé quand le nom ou la langue changent.
  std::string title;
  std::string title_for;
  std::size_t title_language = 0;
  // L'orientation du sprite, tournée à la molette : 0 de face, dans le sens des
  // aiguilles d'une montre, comme le .ACT les range. ⚠ ELLE SURVIT AU
  // CHANGEMENT DE MONSTRE : c'est un réglage de lecture.
  int sprite_direction = 0;
  // Le cadre du sprite, relevé au dessin pour que `app` le compose à la bonne
  // taille à l'image suivante : le sprite s'y ajuste.
  float sprite_box_width = 0.0F;
  float sprite_box_height = 0.0F;
  WheelGateState wheel;
  ImGuiTextFilter drop_filter;
  // L'ordre des trois tables, refait à chaque image depuis leur tri.
  std::vector<int> drop_order;
  std::vector<int> spawn_order;
  std::vector<int> skill_order;
  // LE MENU D'UN BUTIN, armé au clic droit et ouvert hors de la table : une
  // cellule change la pile d'ids, et le surgissant ne s'y retrouverait pas.
  ItemLinkMenu menu;
  // « Signaler un bug », posé par `app` d'après le réglage commun : dans la
  // barre de titre de la fenêtre, dans l'en-tête de la fiche quand la base
  // l'héberge.
  bool bug_report = false;
};

struct MonsterSheetAnswer {
  // Un geste sur un butin — description, lien, menu —, ou son survol.
  ChatLinkAsk item;
  // Le sprite vient d'être cliqué.
  bool poked = false;
  // Ouvrir la fiche de ce monstre-là : le plus ancien de ses homonymes.
  std::uint32_t open_monster = 0;
  // La description de cette compétence, dans la fenêtre des compétences.
  std::uint16_t describe_skill = 0;
  // LES GESTES SUR UNE CARTE D'APPARITION, par son nom interne : le clic y
  // guide, le Maj+clic pose son lien de lieu dans la saisie, le survol demande
  // son infobulle.
  std::string guide_map;
  std::string link_map;
  std::string hovered_map;
  // Poser le monstre dans la barre de chat.
  bool post_link = false;
  // Ouvrir ce monstre dans la base de données.
  bool open_database = false;
  // Le formulaire de rapport de bug, pré-rempli de ce monstre.
  bool report_bug = false;
};

// OÙ LA FICHE SE DESSINE. Seul « Signaler un bug » en dépend : la fenêtre le
// porte dans sa barre de titre, la base, qui n'en a pas, dans l'en-tête.
enum class MonsterSheetHost : std::uint8_t { kWindow, kDatabase };

// 🔴 UN SEUL DESSIN DE FICHE, DEUX HÔTES : la fenêtre ci-dessous, quand on
// l'ouvre depuis un monstre en jeu, ou le panneau de droite de la base de
// données (ADR 0116). Le corps se dessine dans la fenêtre en cours.
[[nodiscard]] MonsterSheetAnswer DrawMonsterSheetBody(MonsterSheetWindowState& state,
                                                      const MonsterSheetView& view,
                                                      const SkinArt& art, MonsterSheetHost host);

// Dessine la fenêtre si elle est ouverte.
[[nodiscard]] MonsterSheetAnswer DrawMonsterSheetWindow(MonsterSheetWindowState& state,
                                                        const MonsterSheetView& view,
                                                        const SkinArt& art);

// ── L'APERÇU AU SURVOL D'UN LIEN DE MONSTRE ─────────────────────────────────
//
// Volontairement COURT : il répond à « est-ce que je veux ouvrir la fiche ? »,
// pas à « comment je le tue ? ». Sprite, nom et rang, niveau, taille, élément,
// race, PV — et le SP seulement quand il en a.
//
// ⚠ SURVOLER N'EST PAS CLIQUER : l'aperçu demande la fiche au serveur s'il ne
// la connaît pas, mais n'ouvre ni ne change la fenêtre.
struct MonsterSheetPreview {
  MonsterSheetPhase phase = MonsterSheetPhase::kAsking;
  std::uint32_t shown_id = 0;
  SkinImage sprite;
  std::string name;
  std::uint8_t boss_rank = 0;
  int level = 0;
  int element_level = 0;
  std::uint32_t hp = 0;
  std::uint32_t sp = 0;
  std::string_view race_value;
  std::string_view element_value;
  std::string_view size_value;
  // Une ligne de pied DÉJÀ TRADUITE, posée par celui qui montre le lien : le
  // geste qu'il y offre. Vide, rien ne s'ajoute ; sinon elle se dit à toute
  // phase, la fiche encore attendue comprise.
  std::string hint;
};

// Le cadre du sprite de l'aperçu, relevé au dessin comme celui de la fiche.
struct MonsterSheetPreviewState {
  float sprite_box_width = 0.0F;
  float sprite_box_height = 0.0F;
};

void DrawMonsterSheetPreview(MonsterSheetPreviewState& state, const MonsterSheetPreview& preview,
                             const SkinArt& art);

}  // namespace ro::ui
