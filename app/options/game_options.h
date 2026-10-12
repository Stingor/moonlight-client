#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/audio/audio_engine.h"
#include "ro/render/armed_skill.h"
#include "ro/render/entity_names.h"
#include "ro/render/gl_context.h"
#include "ro/render/item_drop.h"
#include "ro/render/skill_area.h"
#include "ro/sim/chat_line.h"
#include "ro/ui/align_grid.h"
#include "ro/ui/character_window.h"
#include "ro/ui/chat_bubble.h"
#include "ro/ui/chat_macros.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/entity_bars.h"
#include "ro/ui/item_obtain_toast.h"
#include "ro/ui/options_window.h"
#include "ro/ui/pvp_hud_frames.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/staff_tools.h"
#include "ro/ui/stat_bars.h"
#include "ro/ui/status_bar.h"
#include "ro/ui/zone_capture.h"

#include "game/settings_file.h"
#include "game/ui_stage.h"
#include "options/damage_page_names.h"
#include "options/display_options.h"
#include "options/factory_layout.h"
#include "options/game_option_ids.h"
#include "options/languages.h"
#include "options/option_pages.h"
#include "options/option_sections.h"
#include "options/option_targets.h"
#include "options/sound_options.h"
#include "world/hit_popup_look.h"

namespace moonclient {

// LES OPTIONS DU JOUEUR : ce qu'il règle, et où ça s'applique.
//
// ⚠ ELLES NE SONT PAS LE TABLEAU DE BORD D'UN AUTRE CLIENT. Ici il n'y a pas de
// fenêtre native à arbitrer contre les nôtres : ce qui reste est ce qu'un
// joueur veut vraiment choisir.
//
// ⚠ CHAQUE OPTION EXISTE À UN SEUL ENDROIT — ici — et se déclare au fichier de
// réglages. La commande de chat et la fenêtre passent par les MÊMES méthodes :
// deux chemins qui écriraient chacun leur valeur finiraient par se contredire,
// et c'est celui qu'on n'a pas relu qui gagnerait.

class GameOptions {
 public:
  // Déclare tout au fichier. ⚠ AVANT SA LECTURE : c'est la valeur présente ici
  // qui devient le défaut.
  void Declare(SettingsFile& into);

 private:
  // Les quarante-et-un réglages des huit barres, déclarés en boucle. À part
  // parce que leurs clés se COMPOSENT, là où toutes les autres sont des
  // littéraux.
  void DeclareShortcutBars(SettingsFile& into);
  void DeclareStatBars(SettingsFile& into);
  // Les deux cadres du classement PVP : quatre nombres chacun, plus ce qui vaut
  // pour les deux.
  void DeclarePvpHud(SettingsFile& into);
  // Les vingt-trois réglages des six espèces, déclarés en boucle. ⚠ L'APERÇU
  // N'EN EST PAS : c'est un outil de réglage, et le retrouver allumé à la
  // session suivante ferait chercher d'où viennent six barres au milieu de
  // l'écran.
  void DeclareEntityBars(SettingsFile& into);
  void DeclareEntityNames(SettingsFile& into);
  void DeclareDamage(SettingsFile& into);

 public:
  // Pose TOUT dans le moteur, après lecture ou retour aux origines. Sans la machine (son, écran,
  // langue) pour un profil : la repousser relancerait la musique et retaillerait la fenêtre.
  void ApplyAll(const OptionTargets& to, bool machine_too = true);

  // Refait les lignes des deux panneaux. ⚠ SEULEMENT SI L'UN EST OUVERT :
  // composer sept lignes par image pour des fenêtres fermées serait du temps
  // donné à rien.
  // `shown_bar` est l'onglet ouvert : la composition ne fait que ses lignes —
  // sauf `searching`, où les autres onglets se recopient pour le filtre.
  void Refresh(bool a_panel_is_open, int shown_bar, bool searching = false);

  // ⚠ DEUX GROUPES, ET ILS NE SE MÉLANGENT PAS. Ce que le CLIENT D'ORIGINE
  // sait régler d'un côté — le son, la caméra ; ce que CE client ajoute de
  // l'autre. Un joueur qui cherche un réglage de chat ne doit pas le trouver
  // sous un menu qui promet les options du jeu, ni l'inverse.
  //
  // C'est le second qui grandira : chaque fenêtre qu'on bâtit y pose les
  // siens.
  [[nodiscard]] std::span<const ro::ui::OptionSectionView> sections() const noexcept;

  // Fait ce que le joueur a demandé dans la fenêtre.
  // ⚠ ELLE PREND L'INSTANT : un changement d'affichage arme un filet qui se
  // détend tout seul, et une échéance a besoin d'une horloge.
  void Change(const ro::ui::OptionsWindowAction& asked, const OptionTargets& to,
              std::uint32_t now_ms);

  // ── CE QUE LES COMMANDES DE CHAT APPELLENT ─────────────────────────────────
  // Les mêmes chemins que la fenêtre. Chacune rend l'état obtenu, que la
  // commande annonce dans le chat.
  bool ToggleMusic(const OptionTargets& to);
  bool ToggleSound(const OptionTargets& to);
  // LES DEUX BASCULES D'EFFET : `/effect` crible l'AFFICHAGE, `/mineffect` la RESSOURCE.
  bool ToggleEffectsShown(const OptionTargets& to);
  bool ToggleMinimalEffects(const OptionTargets& to);
  bool ToggleFixedCamera(const OptionTargets& to);
  bool ToggleExpandedZoom(const OptionTargets& to);
  bool ToggleLightmaps(const OptionTargets& to);
  bool ToggleFog(const OptionTargets& to);
  bool ToggleNoShift(const OptionTargets& to);
  bool ToggleNoCtrl(const OptionTargets& to);
  bool ToggleCashShopButton(const OptionTargets& to);
  // LE BATTLE MODE. ⚠ UNE BASCULE, SANS ARGUMENT, comme dans le client
  // d'origine : `/bm on` y est ignoré, et personne ne s'en plaint.
  bool ToggleBattleMode(const OptionTargets& to);
  float SetInterfaceScale(float scale, const OptionTargets& to);
  // Rend faux si la langue est inconnue ou si son catalogue n'a pas pu être lu ;
  // `why` reçoit alors de quoi le dire au joueur.
  bool SetLanguage(std::string_view code, const OptionTargets& to, std::string* why);

  // LA FAMILLE DE POLICE, par son rang dans `SystemFontFamilies`. Rend faux sur
  // un rang qui n'existe pas. ⚠ ELLE S'APPLIQUE TOUT DE SUITE : les douze
  // familles sont bakées à l'ouverture, la bascule ne coûte qu'un pointeur.
  bool SetFontFamily(int family, const OptionTargets& to);

  // LE DÉCOR VIVANT DERRIÈRE L'ÉCRAN DE CONNEXION.
  //
  // 🔴 IL SE RÈGLE SUR LE PREMIER ÉCRAN ET NULLE PART AILLEURS, et c'est
  // pourquoi il n'a pas de rangée dans le panneau du jeu : en jeu, ce réglage
  // ne peut RIEN faire — il n'y a plus de décor à éteindre. L'ADR 0056 le dit :
  // « jamais ici » disparaît, il ne se grise pas.
  //
  // ⚠ IL SE RETIENT, LUI : une machine ou une connexion qui préfèrent s'en
  // passer le préfèrent aussi au lancement suivant.
  [[nodiscard]] bool backdrop_wanted() const noexcept { return backdrop_wanted_; }
  void SetBackdropWanted(bool wanted, const OptionTargets& to);

  // LA TAILLE QUE LE JOUEUR A CHOISIE, relue du fichier. Nulle quand il n'en a
  // jamais choisi.
  //
  // 🔴 ELLE SE LIT AVANT QUE LA FENÊTRE N'EXISTE, et c'est tout son objet :
  // ouvrir la fenêtre à la bonne taille du premier coup. L'ouvrir à une taille
  // d'usine pour la redimensionner ensuite se voit — la fenêtre saute —, et
  // surtout toute disposition calculée entre les deux l'aurait été contre un
  // écran que le joueur n'aura pas.
  [[nodiscard]] ro::render::Resolution saved_resolution() const;
  // L'ÉCRAN CHOISI, relu du fichier pour la même raison : la fenêtre y naît.
  [[nodiscard]] const ScreenChoice& saved_screen() const noexcept { return screen_; }

  // LES DEUX MODIFICATEURS DE CIBLAGE, relus à chaque clic. Voir `no_shift_`
  // pour ce que « permanent » veut dire ici, et pourquoi ce n'est pas une
  // inversion.
  [[nodiscard]] bool no_shift() const noexcept { return no_shift_; }
  [[nodiscard]] bool no_ctrl() const noexcept { return no_ctrl_; }
  [[nodiscard]] bool speak_scream() const noexcept { return speak_scream_; }
  [[nodiscard]] bool speak_frost_joke() const noexcept { return speak_frost_joke_; }
  // `/zoom`, relu à chaque image par les bornes de la caméra.
  [[nodiscard]] bool expanded_zoom() const noexcept { return expanded_zoom_; }

  [[nodiscard]] const ro::ui::ChatAppearance& chat_look() const noexcept { return chat_look_; }
  [[nodiscard]] int font_family() const noexcept { return font_family_; }
  [[nodiscard]] const ro::ui::ChatBubbleLook& bubbles() const noexcept { return bubbles_; }
  [[nodiscard]] const ro::ui::AlignGrid& align_grid() const noexcept { return align_grid_; }
  // LE BANDEAU D'OBTENTION, en lecture seule : l'étage d'interface le relit
  // à chaque image, et rien ne se pousse.
  [[nodiscard]] const ro::ui::ItemObtainToastLook& item_obtain() const noexcept {
    return item_obtain_;
  }
  // La poignée vient d'être posée : c'est le dessin qui l'a déplacée.
  void NoteItemObtainAnchor(int pos_x, int pos_y) noexcept;

  // LES DÉGÂTS FLOTTANTS, en lecture seule : `HitPopups` les relit à chaque
  // image, et leurs trente réglages passent tous par l'aiguillage comme les
  // autres. Rien ne se pousse.
  [[nodiscard]] const HitPopupLook& damage_look() const noexcept { return damage_; }
  // Les polices du dossier du jeu que l'interface a lues, offertes aux dégâts.
  void SetDamageFonts(std::vector<std::string> loose);

  // CE QUE L'ÉTABLI DU STAFF RÈGLE, retenu d'une partie à l'autre. 🔴 PAS
  // D'ENTRÉE DANS `GameOption`, comme les barres de raccourcis : ces réglages
  // vivent dans la fenêtre qui s'en sert, et ici parce que c'est ici qu'est le
  // fichier — la fenêtre n'a que la copie que `GameSettings::Refresh` descend.
  [[nodiscard]] const ro::ui::StaffToolsLook& staff_tools() const noexcept { return staff_tools_; }

  // Ce que la zone des sorts montre, et de quoi elle a l'air.
  [[nodiscard]] const ro::render::SkillAreaLook& skill_area() const noexcept { return skill_area_; }

  // Ce que l'écran de veille fait, et au bout de combien de temps.
  [[nodiscard]] const AfkSettings& afk() const noexcept { return pages_.afk.settings(); }
  // Ce que le moteur fait de l'image : lumière de la carte, post-traitement.
  [[nodiscard]] const GraphicsOptions& graphics() const noexcept { return pages_.graphics; }
  [[nodiscard]] TargetOptions& targeting() noexcept { return pages_.targeting; }
  [[nodiscard]] ContextMenuOptions& context_menu() noexcept { return pages_.context_menu; }
  [[nodiscard]] ViewEquipMemory& view_equip() noexcept { return pages_.view_equip; }
  [[nodiscard]] const MonsterSheetOptions& monster_sheet() const noexcept {
    return pages_.monster_sheet;
  }
  [[nodiscard]] CraftAtlasOptions& craft_atlas() noexcept { return pages_.craft_atlas; }
  // Quick cast : ses cinq réglages, et le droit qui borne la cadence d'item.
  [[nodiscard]] QuickCastOptions& quick_cast() noexcept { return pages_.quick_cast; }
  [[nodiscard]] CardAlbumOptions& card_album() noexcept { return pages_.card_album; }
  [[nodiscard]] DescriptionOptions& descriptions() noexcept { return pages_.descriptions; }
  [[nodiscard]] const BugReportOptions& bug_report() const noexcept { return pages_.bug_report; }
  [[nodiscard]] const ShopOptions& shops() const noexcept { return pages_.shops; }
  bool ToggleShopSetting(ShopRow row, const OptionTargets& to);
  [[nodiscard]] OptionPages& pages() noexcept { return pages_; }

  // Ce qu'un sort qui attend sa cible montre : le rang au curseur, l'étiquette.
  [[nodiscard]] const ro::render::ArmedSkillLook& armed_skill() const noexcept {
    return armed_skill_;
  }

  // L'ENREGISTREUR DE ZONE, tel que le fichier le porte. ⚠ LA ZONE Y EST AUSSI :
  // la tracer est le seul geste qui demande de recouvrir l'écran, et la perdre
  // à chaque lancement obligerait à recommencer avant chaque prise.
  //
  // 🔴 CE QUI SORT D'ICI EST BORNÉ. Le fichier s'ouvre dans un éditeur de texte :
  // une durée de trois cents secondes s'y écrit en une frappe, et réserverait
  // trois mille images avant le premier signe de vie.
  [[nodiscard]] ro::ui::ZoneCaptureSettings capture() const noexcept {
    return ro::ui::SaneCapture(capture_);
  }

  // La zone que le joueur vient de tracer. Rend vrai si elle a changé.
  [[nodiscard]] bool NoteCaptureZone(const ro::ui::ZoneRect& drawn) noexcept;

  // LES DIX MACROS DE CHAT, telles que le fichier les porte.
  //
  // ⚠ ELLES NE SONT PAS DANS `GameOption`, pour la même raison que le sol uni :
  // elles ne se règlent pas dans le panneau d'options mais dans leur propre
  // fenêtre, qui tient les tampons de frappe pendant la partie. Ce qui vit ici
  // est ce que le fichier écrit.
  [[nodiscard]] std::span<const std::string> chat_macros() const noexcept { return chat_macros_; }

  // Ce que la fenêtre des macros vient d'écrire. Rend vrai si quelque chose a
  // changé — l'appelant n'a alors qu'à toucher le fichier.
  [[nodiscard]] bool NoteChatMacros(std::span<const std::string> seen);

  // CE QUE L'ÉTABLI VIENT DE RÉGLER. Rend vrai si quelque chose a changé —
  // l'appelant n'a alors qu'à toucher le fichier.
  //
  // ⚠ UN RELEVÉ ET NON UN ÉVÉNEMENT, comme pour la place de la fenêtre : les
  // cases et le nuancier sont autant de gestes, et chacun serait un drapeau à ne
  // pas oublier de lever.
  [[nodiscard]] bool NoteStaffTools(const ro::ui::StaffToolsLook& seen) noexcept;

  // LISSER LES ICÔNES DU GRIMOIRE. ⚠ PAS D'ENTRÉE DANS `GameOption`, et pas
  // d'oubli : la case vit dans l'en-tête du grimoire, à côté de la grille
  // qu'elle change, et non dans le panneau des réglages — c'est là qu'on la
  // cherche quand on trouve les icônes trop dures ou trop molles. Le fichier,
  // lui, la garde comme tout le reste.
  [[nodiscard]] bool grimoire_smooth_icons() const noexcept { return grimoire_smooth_icons_; }
  [[nodiscard]] bool NoteGrimoireSmoothIcons(bool seen) noexcept;

  // LE VOLET DU STAFF ÉTAIT-IL OUVERT, ET « IGNORÉ DES MONSTRES » VOULU ? ⚠ PAS
  // D'ENTRÉE DANS `GameOption` : l'un est la POSITION d'une fenêtre, qu'ImGui
  // rabote avant l'arrivée du droit ; l'autre un état que le serveur oublie.
  [[nodiscard]] bool character_staff_pane() const noexcept { return character_staff_pane_; }
  [[nodiscard]] bool NoteCharacterStaffPane(bool seen) noexcept;
  [[nodiscard]] bool staff_battle_ignore() const noexcept { return staff_battle_ignore_; }
  [[nodiscard]] bool NoteStaffBattleIgnore(bool seen) noexcept;

  // LA POSE DE L'AVATAR, sa direction, et si elle défile. ⚠ PAS D'ENTRÉE DANS
  // `GameOption` NON PLUS, et pour la même raison que le volet : ce n'est pas
  // un réglage qu'on va chercher dans un panneau, c'est ce qu'on regardait la
  // dernière fois — on le change à la molette et par la liste, sur la fiche.
  //
  // La pose est retenue par son RANG dans `ro::ui::CharacterPose`, et il se
  // borne à la relecture : un fichier écrit par une version qui en portait
  // davantage ne doit pas rendre une pose qui n'existe plus.
  [[nodiscard]] ro::ui::CharacterPose character_avatar_pose() const noexcept;
  [[nodiscard]] bool character_avatar_animated() const noexcept {
    return character_avatar_animated_;
  }
  [[nodiscard]] std::uint8_t character_avatar_direction() const noexcept;
  [[nodiscard]] bool NoteCharacterAvatar(ro::ui::CharacterPose pose, bool animated,
                                         std::uint8_t direction) noexcept;

  // CE QUE LES HUIT BARRES DE RACCOURCIS MONTRENT, ET COMMENT.
  //
  // 🔴 PAS D'ENTRÉE DANS `GameOption`, ET C'EST LA FICHE 0038 QUI LE DIT. Huit
  // barres fois cinq réglages feraient quarante entrées dans une énumération qui
  // en porte une par réglage nommé, et quarante `case` dans son aiguillage. Les
  // barres ont leur propre panneau, qui écrit ici puis demande l'écriture du
  // fichier — le même chemin qu'`AlignGrid`, sans passer par la fenêtre des
  // options.
  //
  // ⚠ EN ÉCRITURE, parce que c'est le panneau qui l'édite. Ce qui suit la
  // modification — poser le fichier comme « à écrire » — reste au demandeur :
  // voir `GameSettings::NoteShortcutBarsChanged`.
  [[nodiscard]] ro::ui::ShortcutBarsLayout& shortcut_bars() noexcept { return shortcut_bars_; }
  [[nodiscard]] const ro::ui::ShortcutBarsLayout& shortcut_bars() const noexcept {
    return shortcut_bars_;
  }

  // LA BARRE D'ÉTATS, en lecture seule : elle n'a pas de panneau à elle, et ses
  // treize réglages passent tous par l'aiguillage comme les autres.
  [[nodiscard]] const ro::ui::StatusBarLayout& status_bar() const noexcept { return status_bar_; }

  // LES SIX JAUGES LIBRES, en lecture seule pour les réglages — et en écriture
  // pour qui les dessine, puisque le joueur les déplace à la souris.
  [[nodiscard]] const ro::ui::StatBarsLook& stat_bars() const noexcept { return stat_bars_; }
  [[nodiscard]] ro::ui::StatBarsLook& stat_bars() noexcept { return stat_bars_; }

  // LA PLACE DES DEUX PIÈCES DU CLASSEMENT PVP. Voir `ro/ui/pvp_hud_frames.h` :
  // le cadre porte la taille autant que la position.
  [[nodiscard]] const ro::ui::PvpHudLook& pvp_hud() const noexcept { return pvp_hud_; }
  [[nodiscard]] ro::ui::PvpHudLook& pvp_hud() noexcept { return pvp_hud_; }

  // LES BARRES AU-DESSUS DES ENTITÉS, en lecture seule : personne ne les
  // déplace à la souris — elles suivent ce qu'elles surmontent — et leurs
  // réglages passent tous par l'aiguillage comme les autres.
  [[nodiscard]] const ro::ui::EntityBarsLook& entity_bars() const noexcept { return entity_bars_; }

  // LES ÉTIQUETTES DES ENTITÉS, en lecture seule et pour la même raison : elles
  // suivent ce qu'elles nomment, et tous leurs réglages passent par
  // l'aiguillage.
  [[nodiscard]] const ro::render::EntityNamesLook& entity_names() const noexcept {
    return entity_names_;
  }

  // COMBIEN DE FAUX ÉTATS MONTRER, zéro quand l'aperçu est éteint. ⚠ LE PLAFOND
  // DE LA BARRE, et non un nombre à part : l'aperçu sert à voir la barre PLEINE.
  [[nodiscard]] int status_preview() const noexcept {
    return status_preview_ ? status_bar_.most : 0;
  }

  // Les effets, la recherche des longs menus de PNJ, et l'avis de pose.
  [[nodiscard]] bool effects_shown() const noexcept { return effects_shown_; }
  [[nodiscard]] bool minimal_effects() const noexcept { return minimal_effects_; }
  [[nodiscard]] bool npc_menu_search() const noexcept { return npc_menu_search_; }
  [[nodiscard]] bool unlock_notice() const noexcept { return unlock_notice_; }
  [[nodiscard]] bool skill_fail_shown() const noexcept { return skill_fail_shown_; }

  // Ce que le joueur a demande de son sac. Pousse dans l'etat de la fenetre
  // par `GameSettings::Refresh` : `ui` ne lit aucun reglage elle-meme.
  [[nodiscard]] bool inventory_filter_shown() const noexcept { return inventory_filter_; }
  [[nodiscard]] bool cart_filter_shown() const noexcept { return cart_filter_; }
  [[nodiscard]] bool cart_size_locked() const noexcept { return cart_locked_; }
  [[nodiscard]] bool storage_filter_shown() const noexcept { return storage_filter_; }
  [[nodiscard]] bool inventory_storage_button() const noexcept { return inventory_storage_button_; }
  [[nodiscard]] bool cart_storage_button() const noexcept { return cart_storage_button_; }
  [[nodiscard]] bool storage_peer_buttons() const noexcept { return storage_peer_buttons_; }
  [[nodiscard]] bool inventory_hover_description() const noexcept {
    return inventory_hover_description_;
  }
  [[nodiscard]] bool cart_hover_description() const noexcept { return cart_hover_description_; }
  [[nodiscard]] bool character_hover_description() const noexcept {
    return character_hover_description_;
  }
  [[nodiscard]] bool storage_hover_description() const noexcept {
    return storage_hover_description_;
  }
  [[nodiscard]] bool inventory_tabs_vertical() const noexcept { return inventory_tabs_vertical_; }
  // ⚠ ET LA TAILLE AVEC LUI : une case ne veut rien dire tant que la largeur
  // peut changer, donc le réglage seul ne suffit pas à l'allumer.
  [[nodiscard]] bool inventory_free_layout() const noexcept {
    return inventory_free_layout_ && inventory_locked_;
  }
  [[nodiscard]] bool cart_tabs_vertical() const noexcept { return cart_tabs_vertical_; }
  [[nodiscard]] bool storage_tabs_vertical() const noexcept { return storage_tabs_vertical_; }
  [[nodiscard]] bool storage_type_tabs() const noexcept { return storage_type_tabs_; }
  [[nodiscard]] bool storage_tab_images() const noexcept { return storage_tab_images_; }
  [[nodiscard]] bool storage_shelf_row() const noexcept { return storage_shelf_row_; }
  [[nodiscard]] bool storage_slot_column() const noexcept { return storage_slot_column_; }
  [[nodiscard]] bool storage_item_id_column() const noexcept { return storage_item_id_column_; }
  [[nodiscard]] bool storage_card_slots_column() const noexcept {
    return storage_card_slots_column_;
  }
  [[nodiscard]] bool storage_value_column() const noexcept { return storage_value_column_; }
  [[nodiscard]] bool storage_total_value() const noexcept { return storage_total_value_; }
  [[nodiscard]] bool inventory_size_locked() const noexcept { return inventory_locked_; }
  [[nodiscard]] bool inventory_drop_locked() const noexcept { return inventory_drop_lock_; }
  [[nodiscard]] bool inventory_sell_locked() const noexcept { return inventory_sell_lock_; }
  [[nodiscard]] bool inventory_sorted() const noexcept { return inventory_sort_; }
  [[nodiscard]] float interface_scale() const noexcept { return interface_scale_; }
  [[nodiscard]] int frame_limit() const noexcept { return frame_limit_; }
  // Le rang du mode d'affichage. Zéro = fenêtré, le seul où une position ait un
  // sens à retenir.
  [[nodiscard]] int display_mode() const noexcept { return display_mode_; }
  [[nodiscard]] bool frame_meter() const noexcept { return frame_meter_; }
  [[nodiscard]] bool frame_meter_locked() const noexcept { return frame_meter_locked_; }
  [[nodiscard]] bool frame_meter_graph() const noexcept { return frame_meter_graph_; }
  [[nodiscard]] bool frame_meter_ping() const noexcept { return frame_meter_ping_; }

  // Combien de secondes avant le retour en arrière, ou moins un si rien
  // n'attend. C'est ce que la fenêtre montre.
  [[nodiscard]] int revert_seconds(std::uint32_t now_ms) const noexcept;
  // Le joueur a dit qu'il voyait : on désarme.
  void KeepDisplay() noexcept;
  // Il n'a rien dit, ou il a refusé : on remet ce qui marchait.
  void RevertDisplay(const OptionTargets& to);
  // Arme le filet si le changement qu'on vient de poser, depuis `before`, peut
  // cacher la question.
  void ArmDisplayRevert(std::uint32_t now_ms, const DisplayRevert& before, const OptionTargets& to);

  // LA FENÊTRE VIENT DE BOUGER. ⚠ APPELÉE À CHAQUE DÉPLACEMENT, donc plusieurs
  // fois par seconde pendant un glisser : c'est l'anti-rebond du fichier qui
  // encaisse, et c'est exactement ce pour quoi il existe.
  //
  // Rend vrai si la place a changé — l'appelant n'a alors qu'à toucher le
  // fichier. ⚠ L'ÉCRAN SUIT : glisser la fenêtre sur un autre écran le choisit.
  [[nodiscard]] bool NoteWindowMoved(const ro::render::GlContext& display);

  // Rend à la fenêtre la place qu'elle avait. ⚠ TOUJOURS RABATTUE DANS L'ÉCRAN :
  // l'écran où elle était a pu être débranché, et une place relue telle quelle
  // la mettrait alors nulle part.
  void PlaceWindow(const OptionTargets& to) const;

  // Les tailles que l'écran offre, relevées au premier besoin. ⚠ ELLES NE
  // SERVENT QU'À LA LISTE DÉROULANTE : ce qu'on enregistre est la taille, pas
  // son rang.
  void ReadResolutions(const OptionTargets& to);
  [[nodiscard]] std::string_view language() const noexcept { return language_; }

  // LE COMPTE QUE L'ÉCRAN DE CONNEXION REPROPOSE, ou vide. 🔴 LE COMPTE, ET LUI
  // SEUL : ce fichier est en clair, à côté du jeu. ⚠ Et il n'a pas d'entrée
  // dans `GameOption` — c'est une case de l'écran de connexion, pas un réglage
  // qu'on vient chercher dans une fenêtre.
  [[nodiscard]] const std::string& login_account() const noexcept { return login_account_; }
  void SetLoginAccount(std::string account) { login_account_ = std::move(account); }

 private:
  void Apply(GameOption which, const OptionTargets& to);
  // Lit le catalogue d'une langue et la pose. ⚠ N'ENREGISTRE PAS : c'est ce
  // qui permet à la pose initiale de ne pas compter comme un changement.
  bool PoseLanguage(std::string_view code, std::string* why);
  // Enregistre — ou plutôt : demande l'enregistrement, qui attend que le joueur
  // s'arrête. Sans effet si l'appelant n'a pas donné de fichier.
  static void Record(const OptionTargets& to);

  // Les valeurs, et LEURS DÉFAUTS : l'initialisateur écrit ici est la seule
  // source, le fichier de réglages le lit à la déclaration.
  SoundOptions sound_options_;
  // ⚠ LES DÉFAUTS SONT CEUX DU CLIENT, lus dans `gamesettings.lub` et non
  // choisis : les effets s'affichent, leur version allégée est coupée.
  bool effects_shown_ = true;
  bool minimal_effects_ = false;
  std::string language_{kDefaultLanguage};
  // Le champ de filtre est MASQUE par defaut : il coute une ligne a une fenetre
  // qui en a peu.
  bool inventory_filter_ = false;
  bool cart_filter_ = false;
  bool cart_locked_ = false;
  // ⚠ VRAI PAR DÉFAUT, seul des trois : c'est la fenêtre où l'on cherche, et
  // celle dont les lignes sont les plus nombreuses.
  bool storage_filter_ = true;
  bool inventory_storage_button_ = false;
  bool cart_storage_button_ = false;
  bool storage_peer_buttons_ = false;
  bool inventory_hover_description_ = false;
  bool cart_hover_description_ = false;
  bool storage_hover_description_ = false;
  bool character_hover_description_ = false;
  // ⚠ NET PAR DÉFAUT, comme le client : il ne filtre rien, et la grille dessine
  // au double exact du bitmap.
  bool grimoire_smooth_icons_ = false;
  // ⚠ FERMÉ ET NON IGNORÉ PAR DÉFAUT : des gestes de staff qu'on ne cherche pas.
  bool character_staff_pane_ = false;
  bool staff_battle_ignore_ = false;
  // ⚠ LA GARDE ANIMÉE, DE FACE : ce sont ces défauts qui décident de ce que
  // voit quelqu'un qui ouvre sa fiche pour la première fois.
  int character_avatar_pose_ = static_cast<int>(ro::ui::CharacterPose::kReady);
  bool character_avatar_animated_ = true;
  int character_avatar_direction_ = 0;
  // LA DISPOSITION DES ONGLETS : debout partout, le coffre compris.
  bool inventory_tabs_vertical_ = true;
  bool inventory_free_layout_ = false;
  bool cart_tabs_vertical_ = true;
  bool storage_tabs_vertical_ = true;
  bool storage_type_tabs_ = true;
  bool storage_tab_images_ = true;
  bool storage_shelf_row_ = false;
  // LES TROIS COLONNES QUI NE VONT PAS DE SOI, éteintes d'usine.
  bool storage_slot_column_ = false;
  bool storage_item_id_column_ = false;
  bool storage_card_slots_column_ = false;
  bool storage_value_column_ = true;
  bool storage_total_value_ = true;
  bool inventory_locked_ = false;
  // ⚠ LE LÂCHER EST LIBRE PAR DÉFAUT : un verrou qu'on n'a pas demandé et qui
  // refuse un geste ordinaire se prend pour une panne. La vente, elle, est
  // verrouillée d'usine — c'est le choix du joueur.
  bool inventory_drop_lock_ = false;
  bool inventory_sell_lock_ = true;
  bool inventory_sort_ = false;
  // Ce que la conversation garde en mémoire, par genre. ⚠ PAS DANS
  // `chat_look_` : ce n'est pas une apparence, et `ui` ne connaît pas `sim`.
  int chat_kept_per_kind_ = ro::sim::kChatLinesKeptPerKind;
  ro::ui::ChatBubbleLook bubbles_;
  ro::ui::ItemObtainToastLook item_obtain_ = FactoryItemObtain();
  ro::ui::AlignGrid align_grid_ = FactoryAlignGrid();
  ro::ui::ShortcutBarsLayout shortcut_bars_ = FactoryShortcutBars();
  ro::ui::StatusBarLayout status_bar_ = FactoryStatusBar();
  ro::ui::StatBarsLook stat_bars_ = FactoryStatBars();
  ro::ui::PvpHudLook pvp_hud_;
  // ⚠ COULEURS ET SEUILS DU CLIENT — vert, rouge sous un quart —, place de la
  // disposition d'usine : voir `factory_layout.h`.
  ro::ui::EntityBarsLook entity_bars_ = FactoryEntityBars();
  // ⚠ MÊME REMARQUE : blanc, rose clair, l'étiquette au survol.
  ro::render::EntityNamesLook entity_names_ = FactoryEntityNames();
  ro::ui::StaffToolsLook staff_tools_;
  // ⚠ SES DÉFAUTS SONT CEUX DE `HitPopupLook`, qui sont le client : le fichier
  // de réglages lit la valeur présente à la déclaration.
  HitPopupLook damage_;
  // ⚠ BRUT, TEL QUE LE FICHIER L'A RENDU : c'est `capture()` qui borne, et non
  // ce champ. Le déclarer déjà borné ferait mentir la valeur qu'on réécrit.
  ro::ui::ZoneCaptureSettings capture_;
  ro::render::SkillAreaLook skill_area_;
  // ⚠ SES DÉFAUTS SONT CEUX DU CLIENT — le rang au curseur, à l'échelle deux,
  // vingt-quatre pixels à droite —, lus par le fichier à la déclaration.
  ro::render::ArmedSkillLook armed_skill_;
  std::array<std::string, ro::ui::kChatMacroCount> chat_macros_;
  // ⚠ NON PERSISTÉ : voir `GameOption::kStatusBarPreview`.
  bool status_preview_ = false;
  // ⚠ ALLUMÉ PAR DÉFAUT : un menu de téléportation en aligne trente, et les
  // parcourir à l'œil coûte plus que de taper trois lettres.
  bool npc_menu_search_ = true;
  bool unlock_notice_ = true;
  bool skill_fail_shown_ = true;
  float interface_scale_ = 1.0F;

  // ── L'AFFICHAGE ───────────────────────────────────────────────────────────
  //
  // 🔴 LA RÉSOLUTION SE GARDE EN TOUTES LETTRES — « 1280x720 » — ET NON PAR SON
  // RANG DANS LA LISTE. La liste vient de l'ÉCRAN : brancher un autre moniteur,
  // changer de carte graphique ou mettre à jour un pilote la réordonne, et un
  // rang relu d'un fichier désignerait alors autre chose. Le joueur retrouverait
  // une résolution qu'il n'a jamais choisie, sans rien pour l'expliquer.
  //
  // Vide = « celle de la fenêtre au démarrage », c'est-à-dire aucun choix encore
  // fait. C'est ce qui distingue « jamais réglé » de « réglé sur cette valeur ».
  std::string resolution_;
  int display_mode_ = 0;
  ScreenChoice screen_;
  bool lock_window_size_ = false;
  // ⚠ ALLUMÉE PAR DÉFAUT. Sans elle la carte dessine aussi vite qu'elle peut,
  // chauffe, et l'écran n'en montre qu'une image sur trois.
  bool vertical_sync_ = true;
  // Zéro = aucune limite. Le frein n'a de sens que la synchronisation éteinte.
  int frame_limit_ = 0;
  bool frame_meter_ = true;
  bool frame_meter_locked_ = true;
  bool frame_meter_graph_ = false;
  bool frame_meter_ping_ = false;

  // Le filet de l'affichage. Voir `DisplayRevert`.
  DisplayRevert revert_;

  // ── OÙ LA FENÊTRE ÉTAIT LA DERNIÈRE FOIS ──────────────────────────────────
  //
  // ⚠ AUCUNE LIGNE NE LE MONTRE, et c'est voulu : ce n'est pas un choix qu'on
  // présente mais un souvenir qu'on rend. Le joueur place sa fenêtre à la
  // souris ; le panneau n'aurait rien à lui demander.
  //
  // Vide = jamais placée. Le système choisit alors, comme au tout premier
  // démarrage.
  std::string window_place_;
  // ⚠ ZÉRO EST MALGUN GOTHIC, le seul choix complet : latin accentué,
  // ponctuation typographique ET coréen. Voir `system_fonts.h`.
  int font_family_ = 0;
  // ⚠ ALLUMÉ PAR DÉFAUT : c'est la mise en scène du client, et celui qui n'en
  // veut pas le dira. Voir `backdrop_wanted`.
  bool backdrop_wanted_ = true;
  std::string login_account_;
  bool fixed_camera_ = false;
  bool expanded_zoom_ = false;
  // ── LES DEUX MODIFICATEURS DE CIBLAGE ─────────────────────────────────────
  //
  // 🔴 ILS RENDENT PERMANENT, ILS N'INVERSENT PAS, comme chez le client
  // d'origine : le drapeau entre en OU PUR avec l'état
  // de la touche. MAJ actif ou `/noshift` actif ; CTRL actif ou `/noctrl`
  // actif. Les prendre pour des inverseurs — « la commande échange le sens de
  // la touche » — rendrait le geste impossible une fois la commande allumée.
  //
  // ⚠ ET RIEN N'EST POUSSÉ À L'APPLICATION : le ciblage les RELIT à chaque
  // clic, comme la fenêtre de dialogue relit les siens à chaque image. Un
  // réglage recopié à côté serait un second exemplaire à tenir d'accord.
  //
  // MAJ FORCE LA CIBLE. Sur un autre joueur, il le rend hostile même là où la
  // carte ne le fait pas ; sur un monstre, il laisse une compétence de soutien
  // l'atteindre au lieu de le frapper. Le client d'origine n'en fait qu'un
  // réglage, sous le nom « No Shift », et sa description ne cite que le second
  // usage — le premier est pourtant dans le même drapeau.
  bool no_shift_ = false;
  // ⚠ ALLUMÉ PAR DÉFAUT, comme au client d'origine (`GameSettings.lub`,
  // OptionTbl[15], `Default = true`). Éteint, un clic sur un monstre ne porte
  // qu'UN coup ; c'est CTRL, ou ce réglage, qui fait poursuivre et frapper
  // jusqu'à la chute.
  bool no_ctrl_ = true;
  // ⚠ ÉTEINTS PAR DÉFAUT, comme au client du joueur, qui ne les dit jamais.
  bool speak_scream_ = false;
  bool speak_frost_joke_ = false;
  // ⚠ C'EST LA VALEUR PRÉSENTE ICI qui devient le défaut du fichier de
  // réglages.
  ro::render::ItemDropSettings item_drop_;
  // ⚠ LES DÉFAUTS DE `ChatAppearance`, retouchés par la disposition d'usine.
  ro::ui::ChatAppearance chat_look_ = FactoryChatAppearance();

  // Les lignes montrées, par section. Refaites à chaque image de fenêtre
  // ouverte : une vue ne se garde pas au travers d'un changement de langue.
  std::vector<ro::ui::OptionRowView> sound_rows_;
  std::vector<ro::ui::OptionRowView> display_rows_;
  std::vector<ro::ui::OptionRowView> game_rows_;
  std::vector<ro::ui::OptionRowView> chat_rows_;
  std::vector<ro::ui::OptionRowView> bubble_rows_;
  std::vector<ro::ui::OptionRowView> item_obtain_rows_;
  // Les noms de langue, dans un tampon stable : la ligne n'en garde qu'une vue.
  std::vector<const char*> language_names_;
  // Les noms de police, même règle : la ligne n'en garde qu'une vue.
  std::vector<const char*> font_names_;
  std::vector<ro::ui::OptionRowView> inventory_rows_;
  std::vector<ro::ui::OptionRowView> cart_rows_;
  std::vector<ro::ui::OptionRowView> screen_rows_;
  std::vector<ro::ui::OptionRowView> capture_rows_;
  // Ce que la section de capture écrit à la volée : le libellé du bouton de
  // tracé — il dit s'il DÉFINIT ou REDÉFINIT —, et la conséquence chiffrée des
  // quatre curseurs. ⚠ DES TAMPONS QUI SURVIVENT À LA COMPOSITION : la rangée
  // n'en garde qu'un pointeur.
  std::string capture_zone_label_;
  std::string capture_note_;
  // Les deux infobulles longues, recollées de leurs paragraphes : le catalogue
  // de traduction range une entrée par LIGNE, et une clé multiligne y perdrait
  // sa moitié du bas.
  std::string capture_record_help_;
  std::string capture_picture_help_;
  // ── CE QUE L'ÉCRAN OFFRE, relu à chaque composition des lignes ────────────
  //
  // ⚠ CE N'EST PAS UN RÉGLAGE, c'est un FAIT de la machine : brancher un autre
  // moniteur change la liste, et rien de ce qu'on a enregistré ne doit en
  // dépendre. Voir `resolution_`, qui garde une taille et non un rang.
  std::vector<ro::render::Resolution> resolutions_;
  std::vector<std::string> resolution_labels_;
  std::vector<const char*> resolution_choices_;
  ScreenOffer screens_;
  // LES PALIERS D'ÉCHELLE OFFERTS, en pour cent. Voir `kInterfaceScalePercents`.
  //
  // ⚠ REFAITS À CHAQUE COMPOSITION, comme les noms de modes : le facteur courant
  // s'y ajoute quand il ne tombe sur aucun palier — `/echelle` accepte n'importe
  // quelle valeur — et il change sous nos pieds.
  std::vector<int> interface_percents_;
  std::vector<std::string> interface_scale_labels_;
  std::vector<const char*> interface_scale_choices_;
  // Les noms des trois modes, refaits à chaque composition : ils se traduisent.
  std::vector<const char*> display_mode_names_;
  std::vector<const char*> frame_limit_names_;

  // Le rang de la valeur enregistrée dans la liste offerte ; zéro si absente.
  [[nodiscard]] int ResolutionRank() const;
  [[nodiscard]] int FrameLimitRank() const;
  [[nodiscard]] int InterfaceScaleRank() const;
  // Et l'inverse : ce que le rang choisi désigne.
  [[nodiscard]] std::string ResolutionTextAt(int rank) const;
  [[nodiscard]] static int FrameLimitAt(int rank);
  [[nodiscard]] float InterfaceScaleAt(int rank) const;
  // Compose les paliers offerts. ⚠ AVANT DE BÂTIR LA RANGÉE qui les montre : le
  // facteur courant en fait partie, et il vient peut-être de changer.
  void ReadInterfaceScales();
  std::vector<ro::ui::OptionRowView> storage_rows_;
  std::vector<ro::ui::OptionRowView> status_rows_;
  std::vector<ro::ui::OptionRowView> stat_bar_rows_;
  // CE QUI VAUT POUR LES SIX, dessiné AU-DESSUS des onglets. ⚠ C'EST LA PLACE
  // QUI LE DIT, rien d'autre ne le peut.
  std::vector<ro::ui::OptionRowView> stat_bar_shared_rows_;
  // Les six onglets. ⚠ REFAITS À CHAQUE IMAGE : ces noms se traduisent.
  std::vector<const char*> stat_bar_tabs_;
  // 🔴 LES LIBELLÉS D'UNE LISTE DÉROULANTE VIVENT ICI, ET C'EST UN PLANTAGE QUI
  // L'A DIT. Une rangée ne COPIE pas ses choix : elle en garde un `span`. Les
  // deux façons de se tromper mènent au même écran noir —
  //
  //   · un `static` les capture UNE fois, et le catalogue qui les porte est
  //     remplacé au premier changement de langue ;
  //   · un tableau LOCAL meurt à la fin de la composition, donc avant le dessin.
  //
  // Un tampon de l'objet, rempli à chaque image, n'a ni l'un ni l'autre défaut :
  // les pointeurs sont frais, et ils vivent jusqu'au dessin. C'est ce que font
  // déjà `status_anchor_names_` et ses voisines.
  std::vector<const char*> stat_bar_text_names_;
  std::vector<const char*> stat_bar_shape_names_;
  // Les trois listes déroulantes de la barre d'états. ⚠ REFAITES À CHAQUE IMAGE
  // comme les noms de langue : un texte traduit ne se garde pas d'une image à
  // l'autre, son pointeur meurt au changement de langue.
  std::vector<const char*> status_anchor_names_;
  std::vector<const char*> status_sort_names_;
  std::vector<const char*> status_sweep_names_;
  std::vector<ro::ui::OptionRowView> shortcut_rows_;
  // Ce qui vaut pour les HUIT barres, dessine au-dessus des onglets : voir
  // `OptionSectionView::shared_rows`.
  std::vector<ro::ui::OptionRowView> shortcut_shared_rows_;
  // Les libellés COMPOSÉS des barres — « Barre 3 · colonnes ». ⚠ DANS UN TAMPON
  // STABLE, et rempli EN ENTIER avant qu'une seule ligne ne le pointe : une vue
  // prise en cours de remplissage pendrait à la première réallocation. Même
  // discipline que les libellés de touche.
  std::vector<std::string> shortcut_labels_;
  // Les noms des huit onglets — « Barre 1 » … —, et les vues que la section leur
  // emprunte. Même discipline de tampon que ci-dessus.
  std::vector<std::string> shortcut_tab_names_;
  std::vector<const char*> shortcut_tabs_;
  // Les noms de la liste déroulante de l'ombre des textes. ⚠ REFAITS À CHAQUE
  // IMAGE comme les trois listes de la barre d'états : le pointeur d'un texte
  // traduit meurt au changement de langue et ne se garde pas d'une image à
  // l'autre.
  std::vector<const char*> shortcut_shadow_names_;
  // Les deux moitiés du panneau des barres, dans `game_option_bars.cpp` : ce
  // qu'il MONTRE, et ce qu'une de ses lignes CHANGE.
  void ComposeShortcutRows(int shown_bar);
  void ComposeScreenRows();
  // LE SORT EN MAIN : sept lignes COMMUNES aux barres, dans
  // `game_option_armed.cpp`. ⚠ AU-DESSUS DES ONGLETS comme les deux autres
  // réglages communs : posées dessous, elles se liraient comme celles de la
  // barre ouverte. `ChangeArmedSkill` rend faux quand la ligne n'est pas l'une
  // des siennes.
  void ComposeArmedSkillRows();
  [[nodiscard]] bool ChangeArmedSkill(const ro::ui::OptionsWindowAction& asked);
  void DeclareArmedSkill(SettingsFile& into);
  // LES TROIS FENÊTRES D'OBJETS — sac, charrette, coffre. Voir
  // `game_option_items.cpp`, qui les porte pour la même raison que les barres :
  // le catalogue des rangées passerait son plafond.
  void ComposeItemWindowRows();
  // LE BANDEAU D'OBTENTION : son panneau vit dans `game_option_toast.cpp`,
  // pour la même raison que les deux précédents — le catalogue des rangées
  // passerait son plafond.
  void ComposeItemObtainRows();
  [[nodiscard]] bool ChangeShortcutBar(const ro::ui::OptionsWindowAction& asked);
  [[nodiscard]] bool ChangeStatBar(const ro::ui::OptionsWindowAction& asked);
  // `shown_bar` est l'onglet ouvert : on ne compose que ses seize lignes.
  void BuildStatBarRows(int shown_bar);
  // LES BARRES DES ENTITÉS : six onglets, un par espèce, dans
  // `game_option_entities.cpp`. Même dispositif que les jauges, et pour la même
  // raison — cent quarante-quatre lignes qui se ressemblent toutes.
  void BuildEntityBarRows(int shown_kind);
  void BuildEntityBarShared();
  [[nodiscard]] bool ChangeEntityBar(const ro::ui::OptionsWindowAction& asked);
  // LES ÉTIQUETTES DES ENTITÉS : six onglets, un par espèce, dans
  // `game_option_names.cpp`. ⚠ LEURS ESPÈCES NE SONT PAS CELLES DES BARRES —
  // pas de groupe, mais un PNJ et un objet au sol : voir `NamePlateKind`.
  void BuildEntityNameRows(int shown_kind);
  void BuildEntityNameShared();
  [[nodiscard]] bool ChangeEntityName(const ro::ui::OptionsWindowAction& asked);
  // Ce que le SERVEUR met dans l'étiquette de cette espèce-là, pour l'infobulle.
  // ⚠ STATIQUE : elle ne lit aucun réglage, elle dit le protocole.
  [[nodiscard]] static const char* EntityNameSourceHelp(std::size_t kind);
  std::vector<ro::ui::OptionRowView> entity_name_rows_;
  std::vector<ro::ui::OptionRowView> entity_name_shared_rows_;
  std::vector<const char*> entity_name_tabs_;
  std::vector<const char*> entity_name_when_names_;
  // Ce que le SERVEUR dit de la vie de cette espèce-là, pour l'infobulle.
  // ⚠ STATIQUE : elle ne lit aucun réglage, elle dit le protocole.
  [[nodiscard]] static const char* EntityBarSourceHelp(std::size_t kind);
  std::vector<ro::ui::OptionRowView> entity_bar_rows_;
  std::vector<ro::ui::OptionRowView> entity_bar_shared_rows_;
  // Les six onglets et les deux listes déroulantes. ⚠ REFAITS À CHAQUE IMAGE :
  // le pointeur d'un texte traduit meurt au changement de langue.
  std::vector<const char*> entity_bar_tabs_;
  std::vector<const char*> entity_bar_when_names_;
  std::vector<const char*> skill_area_pattern_names_;
  std::vector<const char*> entity_bar_text_names_;
  // Les lignes de l'enregistreur de zone. ⚠ DANS SON PROPRE FICHIER : celui des
  // lignes touche déjà les sept cents, et le plafond de ce dépôt est à huit
  // cents.
  void BuildCaptureRows();
  void BuildSkillAreaRows();
  void DeclareSkillArea(SettingsFile& into);
  // LA PARTIE PVP de la section Interface : voir `game_option_pvp.cpp`.
  void ComposePvpHudRows();
  // LA BARRE D'ÉTATS et ses trois listes : voir `game_option_statuses.cpp`.
  void ComposeStatusBarRows();
  void BuildStatBarShared();
  // LES DÉGÂTS FLOTTANTS : trois onglets et deux réglages communs, dans
  // `game_option_damage.cpp`. `shown_tab` est l'onglet ouvert : on ne compose
  // que ses lignes.
  void ComposeDamageRows(int shown_tab);
  // Ce qu'une ligne de dégâts change. L'aiguillage général l'appelle pour
  // toute entrée `kDamage*`, puis enregistre.
  void ChangeDamage(GameOption which, const ro::ui::OptionsWindowAction& asked);
  std::vector<ro::ui::OptionRowView> damage_rows_;
  std::vector<ro::ui::OptionRowView> damage_shared_rows_;
  DamagePageNames damage_names_;
  // Les pages qui tiennent leur propre classe : voir `option_pages.h`.
  OptionPages pages_;

  // En deux familles contiguës : le titre d'une famille est porté par son
  // premier membre.
  std::array<ro::ui::OptionSectionView, Rank(OptionSection::kCount)> sections_{};
};

}  // namespace moonclient
