# moonclient — l'exécutable à fenêtre.
#
# C'est la couche de composition : elle est la SEULE à connaître toutes les
# autres, et la seule à posséder la boucle. Une bibliothèque qui possède la
# boucle impose son rythme au reste du programme.
#
# SDL apparaît ici et pas plus bas : `ro_render_gl` masque délibérément SDL
# derrière GlContext pour que le rendu reste portable vers un autre back-end,
# mais la lecture des événements d'entrée est le travail de `app`.

# ── LE RANGEMENT : UN DOSSIER PAR FAMILLE ────────────────────────────────────
#
# `game/` la boucle et la session · `world/` acteurs, carte et clics ·
# `effects/` · `chat/` · `options/` · `character/` fiche et compétences ·
# `items/` sac, charrette et coffre · `input/` touches et caméra · `capture/`.
#
# 🔴 L'INCLUSION PORTE LE DOSSIER : `#include "options/game_options.h"`. Ce n'est
# pas une décoration. Ajouter chaque sous-dossier aux chemins d'inclusion aurait
# laissé les inclusions plates, et le rangement n'aurait plus rien dit au
# lecteur — ni empêché deux familles d'avoir chacune son `view.h` de se
# confondre. C'est la même règle que `libs/`, où l'on écrit `ro/ui/...`.
#
# ⚠ LE FICHIER QUI SAUVEGARDE VIT AVEC CE QU'IL SAUVEGARDE — `chat/chat_log_file`,
# `input/hotkey_file`, `items/item_layout_file`. Un dossier « fichiers » aurait
# coupé chaque famille en deux, alors que c'est la famille qu'on cherche.
#
# ⚠ LES CIBLES NE SUIVENT PAS LES DOSSIERS, et c'est voulu : une bibliothèque dit
# ce qu'un groupe de fichiers ne CONNAÎT pas — l'éditeur de liens le vérifie —,
# un dossier dit de quoi il parle. `moonclient_equip` pioche ainsi dans
# `character/` et `world/`, et les deux découpages restent vrais.

find_package(SDL3 CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

# LES NŒUDS PROCÉDURAUX, EN BIBLIOTHÈQUE, et ce n'est pas un rangement.
#
# Ce groupe de fichiers ne connaît QUE `core`, `formats`, `vfs` et `render` — pas
# le monde, pas la carte, pas la boucle, pas SDL. Compilé dans l'exécutable,
# cette propriété ne serait qu'une intention : rien n'empêcherait d'y inclure
# `map_stage.h` un jour de fatigue.
#
# En faire une cible rend la propriété VÉRIFIÉE PAR L'ÉDITEUR DE LIENS, comme
# pour les couches de `libs/`. Et cela la rend UTILISABLE : le juge des effets
# tourne hors jeu, sous `roc-cli`, ce qui permet de produire son
# rapport sans fenêtre — et c'est tout l'objet d'un instrument de mesure.
#
# ⚠ LES FICHIERS RESTENT SOUS `app/`, et c'est un état de transit assumé : leur
# place est `libs/`, avec le changement de portée qui va avec. La contrainte de
# lien est la moitié qui vaut tout de suite ; le déménagement se fera à part.
add_library(moonclient_effect_nodes STATIC
    effects/effect_draw_gate.cpp
    effects/effect_judge.cpp
    effects/effect_node_compose.cpp
    effects/effect_node_compose_multi.cpp
    effects/effect_node_compose_shapes.cpp
    effects/effect_node_sow.cpp
    effects/effect_node_sow_clocked.cpp
    effects/effect_node_sow_waves.cpp
    effects/effect_node_sown_figures.cpp
    effects/effect_node_spawn.cpp
    effects/effect_node_sprite_sow.cpp
    effects/effect_node_view.cpp
    effects/effect_ring_sow.cpp
    effects/effect_rising_quad_sow.cpp
    effects/effect_screen_node_view.cpp
    effects/effect_spinning_box_sow.cpp)

target_link_libraries(moonclient_effect_nodes
    PUBLIC ro_core ro_formats ro_render ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_effect_nodes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA CARTE CHARGÉE, puis LES EFFETS EN PLACE, en bibliothèques pour qu'un banc
# les instancie : ce qu'`EffectsInPlay` oublie au changement de carte ne se
# vérifie pas autrement qu'en jouant, et le défaut est de ceux qu'on ne voit
# qu'après un warp.
#
# ⚠ LA CARTE DEMANDE GL, parce qu'elle téléverse son décor.
#
# ⚠ LES IMPACTS ET LES TIRS (`effect_hit_view.cpp`) RESTENT DANS L'EXÉCUTABLE,
# avec leur seul appelant : ils lisent la pose des acteurs, qui y vit encore.
# Les ranger ici ferait une bibliothèque qui ne se lie qu'avec lui.
add_library(moonclient_stage STATIC
    world/decor_veil_view.cpp
    world/night_light_view.cpp
    world/map_stage.cpp)

target_link_libraries(moonclient_stage
    PUBLIC ro_core ro_formats ro_render ro_render_gl ro_sim ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_stage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Le bandeau du nom de la carte, entre la table des cartes et l'écran.
add_library(moonclient_map_sign STATIC
    world/map_sign_stage.cpp)

target_link_libraries(moonclient_map_sign
    PUBLIC ro_core ro_render ro_ui ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_map_sign PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(moonclient_effects STATIC
    effects/doll_camera.cpp
    effects/effect_arrow_shower.cpp
    effects/effect_bearer_gate.cpp
    effects/effect_class_view.cpp
    effects/effect_devotion_view.cpp
    effects/effect_ground_skill_view.cpp
    effects/effect_hat.cpp
    effects/effect_looping.cpp
    effects/effect_lua_view.cpp
    effects/effect_named.cpp
    effects/effect_named_look.cpp
    effects/effect_named_skill.cpp
    effects/effect_named_sounds.cpp
    effects/effect_skill_step.cpp
    effects/effect_skill_view.cpp
    effects/effect_spawn_view.cpp
    effects/effect_state_change_view.cpp
    effects/effect_state_classes.cpp
    effects/effect_str_look.cpp
    effects/effect_switches.cpp
    effects/effect_view.cpp
    effects/effect_warp_view.cpp
    world/actor_effect_finish.cpp
    world/actor_ghosts.cpp
    world/actor_tint.cpp
    world/ailment_tints.cpp)

target_link_libraries(moonclient_effects
    PUBLIC moonclient_effect_nodes moonclient_stage ro_audio ro_core ro_formats ro_render ro_sim
           ro_vfs
    PRIVATE moonclient_damage moonclient_visibility ro_warnings)

target_include_directories(moonclient_effects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# OU LE SERVEUR RANGE CE QU'UNE CASE DU MANNEQUIN MONTRE, en bibliothèque, pour
# la même raison que les deux au-dessus : ce fichier ne connaît que `sim` et
# `ui` — ni la carte, ni le réseau, ni la table des objets. En faire une cible
# rend cette propriété VÉRIFIÉE PAR L'ÉDITEUR DE LIENS, et rend la
# correspondance ESSAYABLE : l'échange de deux emplacements voisins est muet en
# jeu tant que le personnage n'en porte qu'un.
#
# La table des APPORTS y vit aussi, et pour la même raison : elle ne connaît que
# `sim` et `ui`, et l'ordre dans lequel elle range quarante-cinq rangées se
# vérifie sur un banc, jamais en jeu.
add_library(moonclient_equip STATIC
    character/character_bonus_rows.cpp
    world/entity_traits.cpp
    character/stat_bar_values.cpp
    character/equip_slot_map.cpp)

target_link_libraries(moonclient_equip
    PUBLIC ro_sim ro_ui ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_equip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# CE QUE LA BARRE D'ÉTATS MONTRE, en bibliothèque et pour la même raison que
# celle du dessus : ce fichier croise le monde, les tables de l'installation et
# les textures — mais il ne connaît ni la fenêtre, ni la boucle, ni SDL. En faire
# une cible rend cette propriété VÉRIFIÉE PAR L'ÉDITEUR DE LIENS, et rend le
# croisement ESSAYABLE : les altérations qui arrivent en BITS, le doublon qu'elles
# feraient avec un EFST de même sens, et le repli sur une abréviation sont trois
# pannes muettes en jeu.
add_library(moonclient_statuses STATIC
    character/status_view.cpp)

target_link_libraries(moonclient_statuses
    PUBLIC ro_render_gl ro_sim ro_ui ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_statuses PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES TITRES QU'ON POSSÈDE, en bibliothèque et pour la même raison : ce fichier
# croise ce que le serveur dit avec deux tables de l'installation, sans connaître
# ni la fenêtre ni la boucle. Le croisement se vérifie sur un banc — le
# dédoublonnage et l'ordre d'une liste sont muets en jeu tant qu'on n'a qu'un
# titre.
add_library(moonclient_titles STATIC
    character/title_view.cpp)

target_link_libraries(moonclient_titles
    PUBLIC ro_net ro_render ro_sim ro_ui ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_titles PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# CE QUE LE CLIENT ENREGISTRE, EN BIBLIOTHÈQUE, pour la même raison que les
# nœuds ci-dessus : ces fichiers ne connaissent que `core`, `sim` et yaml-cpp
# — ni la carte, ni le rendu, ni SDL, ni la boucle. En faire une cible rend
# cette propriété VÉRIFIÉE PAR L'ÉDITEUR DE LIENS, et les rend surtout
# ESSAYABLES : les pannes qu'ils existent pour empêcher — une clé lue mais
# jamais écrite, un défaut qui diverge, un filtre perdu à la relecture — sont
# muettes en jeu, et ne se voient que sur un banc.
# LES QUATRE AIDES DE TEXTE DE L'AFFICHAGE, a part : elles ne connaissent que des
# chaines et des nombres, donc un essai les atteint sans monter tout le jeu.
add_library(moonclient_display STATIC
    options/display_options.cpp)

target_link_libraries(moonclient_display
    PUBLIC ro_core ro_render_gl
    PRIVATE ro_warnings)

target_include_directories(moonclient_display PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# QUI REÇOIT LA TOUCHE, EN BIBLIOTHÈQUE, et pour la même raison que les trois
# au-dessus : ce fichier ne connaît que SDL — ni le monde, ni les fenêtres, ni
# la boucle. En faire une cible rend cette propriété VÉRIFIÉE PAR L'ÉDITEUR DE
# LIENS, et rend la règle ESSAYABLE. Elle en a besoin : une touche avalée à tort
# ne se voit qu'en jeu, et s'y lit « le client a bloqué mon clavier ».
add_library(moonclient_keys STATIC
    input/key_gate.cpp)

target_link_libraries(moonclient_keys
    PUBLIC SDL3::SDL3
    PRIVATE ro_warnings)

target_include_directories(moonclient_keys PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA TRADUCTION DES BALISES D'OBJET DU FIL, en bibliotheque et pour la meme
# raison que celle du dessus : ce fichier ne connait que la table des objets et
# la forme d'une ligne de chat -- ni le monde, ni les fenetres, ni la boucle. En
# faire une cible rend cette propriete VERIFIEE PAR L'EDITEUR DE LIENS, et rend
# la regle essayable : une balise mal relue remplit le fil de charabia chez tous
# les joueurs, et cela ne se voit qu'en jeu.
add_library(moonclient_chatlinks STATIC
    chat/chat_item_link.cpp
    chat/chat_monster_link.cpp
    chat/chat_mvp_link.cpp
    chat/chat_navi_search_link.cpp
    chat/chat_place_link.cpp
    chat/chat_recipe_link.cpp
    chat/chat_setting_link.cpp
    chat/chat_mention_piece.cpp
    chat/chat_rich_decor.cpp
    chat/chat_search_gestures.cpp
    chat/navigation_command.cpp
    chat/staff_item_command.cpp
    chat/announce_command.cpp
    chat/whisper_block_command.cpp)

# ⚠ `ro_sim` EST PUBLIC : une ligne de conversation porte deux
# espèces de balise, et celle d'un STYLE se lit — donc se valide — avec
# `ro/sim/style_code.h`. Le genre et le code voyagent dans son en-tête.
# ⚠ `ro_net` EN PRIVÉ : l'auteur d'une ligne relayée de Discord se lit avec
# `ro/net/discord_relay.h`, et rien de `net` ne paraît dans les en-têtes d'ici.
target_link_libraries(moonclient_chatlinks
    PUBLIC ro_sim ro_ui ro_vfs moonclient_itemfacts moonclient_settinglinks
    PRIVATE ro_net ro_warnings)

target_include_directories(moonclient_chatlinks PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES REFUS DE COMPETENCE DANS LA CONVERSATION, en bibliotheque pour que leurs
# phrases s'essaient sur un banc : un motif mal range dit au joueur autre chose
# que ce que le serveur a refuse, et cela ne se voit qu'en jeu.
add_library(moonclient_skillrefusal STATIC
    world/skill_refusal_report.cpp
    world/skill_refusal_texts.cpp
    world/twilight_shortfall.cpp)

target_link_libraries(moonclient_skillrefusal
    PUBLIC ro_sim ro_vfs
    PRIVATE ro_core ro_net ro_warnings)

target_include_directories(moonclient_skillrefusal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA BARRE DE SAISIE VUE DE LA BOUCLE : où part la ligne, ce qu'il en reste, et
# la couleur de chaque destination — la même table que celle du fil. En
# bibliothèque pour que le sélecteur se juge sur un banc, sans fenêtre : un
# rang qui armerait une autre destination que celle qu'il montre ne se voit
# qu'à l'envoi.
add_library(moonclient_chatpreview STATIC
    chat/chat_line_color.cpp
    chat/chat_line_preview.cpp)

target_link_libraries(moonclient_chatpreview
    # La lecture d'une annonce (/b, /lb…) pour mesurer sa ligne, et les liens
    # posés qu'une ligne validée emporte ou garde : PUBLIC, l'en-tête les nomme.
    PUBLIC ro_core ro_net ro_proto ro_sim ro_ui moonclient_chatlinks
    PRIVATE ro_warnings)

target_include_directories(moonclient_chatpreview PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA CONFIGURATION DU CLIENT, `moonclient.yml` : à qui il parle. En bibliothèque
# parce qu'elle ne connaît que `core` et yaml-cpp, et qu'un lecteur qui vit dans
# l'exécutable ne s'éprouve pas.
add_library(moonclient_config STATIC
    game/client_config.cpp)

target_link_libraries(moonclient_config
    PUBLIC ro_core
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA TABLE DES RACCOURCIS ET SON FICHIER, en bibliothèque et pour la même raison
# que les autres : ces deux fichiers ne connaissent que `core`, `sim`, SDL et
# yaml-cpp — ni le monde, ni les fenêtres, ni la boucle.
#
# 🔴 ET ILS EN ONT PLUS BESOIN QUE LES AUTRES. La table compte cent huit lignes
# dont quatre-vingt-seize cases de barre, et son fichier doit relire les clés de
# la version d'avant. Une migration qui échoue ne casse rien : elle rend
# simplement les touches d'origine, et le joueur croit s'être trompé.
add_library(moonclient_hotkeys STATIC
    input/hotkey_file.cpp
    input/hotkey_gestures.cpp
    input/hotkeys.cpp)

target_link_libraries(moonclient_hotkeys
    PUBLIC ro_core ro_sim SDL3::SDL3
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_hotkeys PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA GRILLE D'ICÔNES DE MENU, en bibliothèque pour la raison habituelle : ce
# qu'elle montre — icônes cachées, enfoncées, allumées, la pastille de la file,
# les raccourcis — se vérifie sur un banc, sans art ni fenêtres.
add_library(moonclient_menuicons STATIC
    game/menu_icon_compose.cpp)

target_link_libraries(moonclient_menuicons
    PUBLIC ro_core ro_render ro_render_gl ro_ui moonclient_hotkeys
    PRIVATE ro_warnings)

target_include_directories(moonclient_menuicons PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE FICHIER DE L'IA DES COMPAGNONS, en bibliothèque pour la raison habituelle :
# il ne connaît que `core`, `sim`, les champs des fenêtres « Avancé… » et « IA
# du compagnon » et yaml-cpp, et une ligne écartée, une fusion, une version
# inconnue ou la phrase d'une règle se vérifient sur un banc, jamais en jeu.
add_library(moonclient_companionai STATIC
    game/companion_ai_editor.cpp
    game/companion_ai_fields.cpp
    game/companion_ai_file.cpp
    game/companion_ai_merge.cpp
    game/companion_ai_characters_yaml.cpp
    game/companion_ai_players_yaml.cpp
    game/companion_ai_profiles.cpp
    game/companion_ai_state_file.cpp
    game/companion_ai_session.cpp
    game/companion_ai_names.cpp
    game/homunculus_staff_words.cpp
    game/companion_ai_profile_gestures.cpp
    game/companion_ai_window_compose.cpp
    game/companion_ai_profile_yaml.cpp
    game/companion_ai_settings_yaml.cpp
    game/companion_rule_monsters.cpp
    game/companion_rule_words.cpp
    game/companion_tactic_fields.cpp
    game/companion_target_marks.cpp)

target_link_libraries(moonclient_companionai
    PUBLIC ro_core ro_sim ro_ui
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_companionai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES PRESETS D'ÉQUIPEMENT ET LEUR FICHIER, en bibliothèque pour la raison
# habituelle : ils ne connaissent que le sac, la table des raccourcis et
# yaml-cpp. Ce qu'un preset retire et remet se décide ici, et une pièce
# désignée deux fois ne se verrait sinon qu'en jeu, le jour où un accessoire
# disparaît au lieu de revenir.
add_library(moonclient_presets STATIC
    character/equip_preset_file.cpp
    character/equip_presets.cpp)

target_link_libraries(moonclient_presets
    PUBLIC moonclient_hotkeys ro_sim
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_presets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# CE QUE LE SERVEUR SEUL SAIT D'UN OBJET, et le rythme de ses demandes : une
# bibliothèque pour qu'il s'éprouve contre un serveur en boucle locale.
add_library(moonclient_itemfacts STATIC
    items/creator_names.cpp
    items/item_facts_asker.cpp
    items/repair_piece_name.cpp)

target_link_libraries(moonclient_itemfacts
    PUBLIC ro_net ro_sim ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_itemfacts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE CONTEXTE D'UN RAPPORT DE BUG, à part pour qu'un essai vérifie ce que le
# serveur recevra : un JSON valable, des noms échappés, la révision du client.
add_library(moonclient_bugreport STATIC
    game/bug_report_context.cpp)

target_link_libraries(moonclient_bugreport
    PUBLIC ro_core ro_net ro_ui
    PRIVATE ro_warnings)

target_include_directories(moonclient_bugreport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES LIVRES : leur fenêtre et la lecture à voix haute, à part pour que la
# récitation s'essaie contre un serveur en boucle locale.
add_library(moonclient_books STATIC
    items/book_bookmark_file.cpp
    items/book_reader.cpp)

target_link_libraries(moonclient_books
    PUBLIC ro_net ro_sim ro_ui ro_vfs
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_books PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE TROC VU PAR SA FENÊTRE, à part de son étage : le troc est éteint sur le
# serveur, et ce qu'il montre, ce qu'il dit et jusqu'où il laisse ajouter ne
# s'éprouvent que sur un banc.
add_library(moonclient_barter STATIC
    game/barter_view.cpp)

target_link_libraries(moonclient_barter
    PUBLIC ro_core ro_sim ro_ui ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_barter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# L'ÉCHOPPE D'ACHAT VUE DE L'ÉCRAN, à part de son étage : ce qu'on propose, les
# mots que dit un code du serveur, la ligne d'une vente. Une ligne fausse ne se
# verrait qu'au moment de vendre ; un banc la voit avant.
add_library(moonclient_buyingstore STATIC
    game/buying_store_rules.cpp)

target_link_libraries(moonclient_buyingstore
    PUBLIC ro_core ro_sim ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_buyingstore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA RECHERCHE D'ÉCHOPPES VUE DE L'ÉCRAN, à part de son étage : le catalogue
# qu'elle parcourt, et les tables qui y disent qui est une carte.
add_library(moonclient_storesearch STATIC
    game/store_search_catalogue.cpp)

target_link_libraries(moonclient_storesearch
    PUBLIC ro_core ro_sim ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_storesearch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# L'ÉCHOPPE DE VENTE VUE DE L'ÉCRAN, à part de son étage : les mots d'un code du
# serveur, la dernière échoppe retenue sur le disque et reposée sur le cart, et
# l'enseigne qu'un clic vise, et les récapitulatifs qu'écrit l'ouverture d'une
# échoppe, de vente comme d'achat. Chacun se juge sur un banc, sans fenêtre.
add_library(moonclient_vending STATIC
    game/trade_recap.cpp
    game/vending_memory.cpp
    game/vending_said.cpp
    world/signboard_pick.cpp)

target_link_libraries(moonclient_vending
    PUBLIC ro_core ro_render ro_sim ro_ui ro_vfs
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_vending PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES SALONS VUS DE L'ÉCRAN, à part de leur étage : ce qui empêche d'en ouvrir
# un, les salons que la zone montre, et le formulaire retenu sur le disque.
# Chacun se juge sur un banc, sans fenêtre.
add_library(moonclient_chatroom STATIC
    game/chat_room_memory.cpp
    game/chat_room_rules.cpp)

target_link_libraries(moonclient_chatroom
    PUBLIC ro_core ro_sim
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_chatroom PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES CHUCHOTEMENTS VUS DE L'ÉCRAN : l'en-tête d'une ligne selon l'endroit où
# elle se lit, et les conversations 1:1 — ce qui les ouvre, ce qu'elles prennent
# au fil, leur non-lu. Règles pures, jugées sur un banc sans fenêtre. La liste
# de blocage rangée au service web aussi : `moonclient_web` ne connaît aucune
# plateforme, et le transport se donne au constructeur.
add_library(moonclient_whispers STATIC
    chat/player_link_judge.cpp
    chat/whisper_desk.cpp
    chat/whisper_block_memory.cpp
    chat/whisper_lines.cpp)

target_link_libraries(moonclient_whispers
    PUBLIC ro_core ro_sim ro_vfs ro_ui moonclient_web
    PRIVATE ro_warnings)

target_include_directories(moonclient_whispers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES SUCCÈS VUS DE L'ÉCRAN, à part de leur étage : ce qu'un onglet montre et
# dans quel ordre, ce qu'il compte, comment un objectif et une ligne de suivi
# s'écrivent, et ce qui s'en retient sur le disque. Un ordre faux, une coupe au
# milieu d'un caractère ou un fichier mal relu se voient
# sur un banc avant le jeu. Le rapport de bug de la fenêtre aussi.
add_library(moonclient_achievements STATIC
    game/achievement_bug_report.cpp
    game/achievement_memory.cpp
    game/achievement_rows.cpp)

target_link_libraries(moonclient_achievements
    PUBLIC ro_core ro_sim ro_ui ro_vfs moonclient_bugreport
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_achievements PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# UN TEXTE DE NOS TABLES YAML — une chaîne nue, ou { fr, en, es } —, toutes
# ses langues gardées : le champ de bataille et les quêtes le lisent de la même
# façon, et le choisissent dans la langue du moment.
add_library(moonclient_yamltext STATIC
    game/yaml_text.cpp)

target_link_libraries(moonclient_yamltext
    PUBLIC yaml-cpp::yaml-cpp
    PRIVATE ro_warnings)

target_include_directories(moonclient_yamltext PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE CATALOGUE DES QUÊTES, lu dans data\yaml_files\quests.yml : titre, résumé,
# description, PNJ, récompenses, icônes et catégories ; et celui des quêtes
# recommandées, dans recommended_quests.yml. Une langue mal résolue
# ou une quête mal formée se voit sur un banc, pas devant le journal.
#
# Et ce qui s'y range sans fenêtre : les règles du journal (temps restant,
# recherche, tri, suivi) et ce que le client retient des quêtes et du panneau
# de suivi, sur le disque.
add_library(moonclient_quests STATIC
    quest/quest_catalog.cpp
    quest/quest_memory.cpp
    quest/quest_rules.cpp
    quest/recommended_catalog.cpp
    quest/tracker_memory.cpp)

target_link_libraries(moonclient_quests
    PUBLIC ro_core ro_sim ro_ui
    PRIVATE moonclient_yamltext ro_vfs yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_quests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES PROFILS DE DISPOSITION ET DE RÉGLAGES, sur le disque, en bibliothèque :
# un aller-retour infidèle perdrait la place de toutes les fenêtres, ou les
# réglages d'un personnage, sans un mot.
add_library(moonclient_profiles STATIC
    game/profile_yaml.cpp
    game/settings_profile_file.cpp
    game/ui_layout_file.cpp)

target_link_libraries(moonclient_profiles
    PUBLIC ro_core ro_ui yaml-cpp::yaml-cpp
    PRIVATE ro_warnings)

target_include_directories(moonclient_profiles PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE COURRIER VU DE L'ÉCRAN, à part de son étage : le sac montré pendant
# l'écriture, et la suite qu'impose chaque réponse du serveur à la fenêtre
# d'écriture. Une annulation oubliée laisse le joueur figé, une saisie effacée
# après un échec se refait à la main : deux pannes qu'un banc voit avant le jeu.
add_library(moonclient_mail STATIC
    game/mail_write_flow.cpp
    items/mail_withheld.cpp)

target_link_libraries(moonclient_mail
    PUBLIC ro_core ro_sim ro_ui
    PRIVATE ro_warnings)

target_include_directories(moonclient_mail PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# L'ALBUM DE CARTES, en bibliothèque pour la raison habituelle : ce qu'on dit
# d'une réponse, la suite d'un sacrifice et le bon de travail du staff se
# vérifient sur un banc, contre un serveur en boucle locale.
add_library(moonclient_cardalbum STATIC
    items/card_album_delta.cpp
    items/card_album_view.cpp)

target_link_libraries(moonclient_cardalbum
    PUBLIC ro_net ro_render ro_render_gl ro_sim ro_ui ro_vfs
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_cardalbum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE CARNET DE CHASSE MVP, en bibliothèque pour la même raison que l'album :
# l'instantané demandé au bon moment, les alertes, les lignes détachées et leur
# fichier, la page de réglages et le masque des surfaces annoncé au serveur se
# vérifient sur un banc, contre un serveur en boucle locale. Le son de l'alerte
# et les sons d'interface qu'il partage avec l'alarme des chuchotements aussi,
# sur un mélangeur sans périphérique.
add_library(moonclient_mvptracker STATIC
    game/interface_sound.cpp
    game/mvp_alert_sound.cpp
    game/surface_announcer.cpp
    mvp/mvp_line_file.cpp
    mvp/mvp_tracker_compose.cpp
    mvp/mvp_tracker_results.cpp
    mvp/mvp_tracker_stage.cpp
    options/mvp_tracker_options.cpp)

target_link_libraries(moonclient_mvptracker
    PUBLIC ro_audio ro_net ro_render ro_render_gl ro_sim ro_ui ro_vfs moonclient_chatlinks
           moonclient_monstersheet moonclient_settings
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_mvptracker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA FILE DU CHAMP DE BATAILLE, en bibliothèque pour la raison habituelle :
# l'inscription refusée sans rien envoyer, les fenêtres qui suivent la phase de
# la file et l'effectif redemandé à son rythme se vérifient sur un banc, contre
# un serveur en boucle locale.
add_library(moonclient_battleground STATIC
    game/battleground_bug_report.cpp
    game/battleground_compose.cpp
    game/battleground_file.cpp
    game/battleground_rules.cpp
    game/battleground_stage.cpp)

target_link_libraries(moonclient_battleground
    PUBLIC ro_audio ro_net ro_sim ro_ui ro_vfs moonclient_bugreport moonclient_yamltext
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_battleground PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# CE QUE LE COMBAT DU CHAMP DE BATAILLE MONTRE, et sa page de réglages. En
# bibliothèque parce que ces règles ne connaissent que `sim` et `render` : la
# marque d'un ennemi caché, le score d'un camp inconnu ne se voient pas en
# jouant.
add_library(moonclient_battlefield STATIC
    options/battleground_options.cpp
    world/battlefield_view.cpp)

target_link_libraries(moonclient_battlefield
    PUBLIC ro_sim ro_render ro_ui moonclient_visibility moonclient_settings
    PRIVATE ro_warnings)

target_include_directories(moonclient_battlefield PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES RÈGLES DE LA CAMÉRA, en bibliothèque pour la raison habituelle : bornes,
# pas des gestes et tables par carte se vérifient sur un banc. Une borne
# comparée dans le mauvais ordre fige l'inclinaison sans une erreur.
add_library(moonclient_camera_rules STATIC
    input/camera_rules.cpp)

target_link_libraries(moonclient_camera_rules
    PUBLIC ro_render
    PRIVATE ro_warnings)

target_include_directories(moonclient_camera_rules PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE PLACEMENT LIBRE DES OBJETS DU SAC, en bibliothèque et pour la raison
# habituelle : l'ordre des cases et leurs trous se vérifient sur un banc, jamais
# en jeu. Une case prise deux fois, un objet qui ne trouve plus de trou, un
# échange qui écrase au lieu d'échanger — trois pannes muettes, dont la dernière
# se découvrirait un objet perdu à la fois.
add_library(moonclient_items STATIC
    items/item_layout.cpp
    items/item_subcategory.cpp
    items/item_layout_file.cpp
    items/storage_favorites.cpp
    items/storage_tab_names.cpp
    items/pickup_refusal.cpp
    items/transfer_hold.cpp
    items/warp_memo_report.cpp
    items/view_equip_rows.cpp)

target_link_libraries(moonclient_items
    PUBLIC ro_core ro_sim ro_ui
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_items PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# CE QUE LE RÉGLAGE FAIT D'UN COUP, en bibliothèque et pour la raison habituelle :
# ces règles ne connaissent que `sim` et `render` — ni le monde vivant, ni les
# sprites, ni la boucle. En faire une cible rend cette propriété VÉRIFIÉE PAR
# L'ÉDITEUR DE LIENS, et les rend ESSAYABLES : la priorité des couleurs et
# l'interrupteur qui tait les coups des autres sont muets en jeu — un nombre de
# la mauvaise couleur ne se voit qu'à l'œil, un nombre absent pas du tout.
# CE QUE LE CURSEUR ANNONCE QUAND UNE COMPÉTENCE ATTEND SA CIBLE, en
# bibliothèque pour la même raison : la règle ne connaît que `sim` et `render`,
# et elle est INVISIBLE hors du jeu. Une forme fausse ne se voit qu'à la souris
# — une épée promettrait le coup ordinaire que l'armement suspend.
# LA PORTE DU CIBLAGE : ce qui fait d'un autre joueur une cible.
#
# Sa propre bibliotheque pour la meme raison que le curseur arme : c'est la
# regle centrale de la fiche 0055, et une regle qui vit dans l'executable ne
# s'eprouve pas.
add_library(moonclient_versus STATIC
    world/versus_gate.cpp)
target_link_libraries(moonclient_versus
    PUBLIC ro_sim
    PRIVATE ro_warnings)
target_include_directories(moonclient_versus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# CE QUI SE DESSINE D'UN ACTEUR que ses options cachent : rien, son ombre, ou
# une place sans corps pour le joueur lui-même. En bibliothèque parce que la
# règle ne connaît que `sim` et `render`, et qu'elle peut être fausse sans que
# rien ne le dise : le joueur caché resterait à l'écran. Et les effets d'office
# d'une créature, qui se posent et s'ôtent selon cette même règle.
add_library(moonclient_visibility STATIC
    world/actor_visibility.cpp
    world/monster_auras.cpp)
target_link_libraries(moonclient_visibility
    PUBLIC ro_sim ro_render ro_vfs
    PRIVATE ro_warnings)
target_include_directories(moonclient_visibility PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE MODE CIBLAGE : ce qui décide quand la désignation change, ce que le clic
# veut dire sous lui, et ce que le HUD sait de la cible. Et la flèche du jeu, que
# le mode détourne : l'engagement de la souris qui la gouverne, et la décision de
# chaque image. En bibliothèque parce que ces règles ne connaissent que `sim`, et
# qu'une règle qui vit dans l'exécutable ne s'éprouve pas.
add_library(moonclient_targeting STATIC
    world/deferred_walk.cpp
    world/double_click_rule.cpp
    world/mouse_engagement.cpp
    world/native_marker.cpp
    world/target_facts.cpp
    world/targeting.cpp)
target_link_libraries(moonclient_targeting
    PUBLIC moonclient_equip ro_render ro_sim ro_vfs
    PRIVATE ro_warnings)
target_include_directories(moonclient_targeting PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# QUICK CAST : le sort qui part dès qu'il s'arme, sa relance tant que la touche
# est tenue, et celle d'une case d'item. En bibliothèque pour la raison
# habituelle : la machine ne voit le jeu qu'à travers une interface, et ses
# règles — la souris avant la cible du HUD, le cercle retiré au relâchement, le
# plancher du staff — ne se vérifient pas en jouant.
add_library(moonclient_quickcast STATIC
    world/quick_cast.cpp)
target_link_libraries(moonclient_quickcast
    PUBLIC ro_sim ro_ui
    PRIVATE ro_warnings)
target_include_directories(moonclient_quickcast PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA CONSOLE DE MISE AU POINT (ADR 0095) : une écoute sur 127.0.0.1 que le pont
# MCP de `tools/moonclient_mcp` interroge. En bibliothèque pour la raison
# habituelle : ses requêtes et son écrivain JSON se vérifient sur un monde
# construit dans un essai, sans fenêtre.
add_library(moonclient_debug STATIC
    debug/debug_console.cpp
    debug/debug_json.cpp
    debug/debug_queries.cpp
    debug/debug_request.cpp)
target_link_libraries(moonclient_debug
    PUBLIC ro_core ro_sim ro_net
    PRIVATE ro_warnings)
target_include_directories(moonclient_debug PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# L'APPEL D'URGENCE DANS « GAMEPLAY » : une case qui montre ce que le serveur
# tient et ne fait que DEMANDER. En bibliothèque pour la raison habituelle :
# grisée tant que le serveur se tait, un clic qui ne part qu'une fois — rien de
# cela ne se voit en jouant.
add_library(moonclient_calloptions STATIC
    options/call_options.cpp)
target_link_libraries(moonclient_calloptions
    PUBLIC ro_ui
    PRIVATE ro_warnings)
target_include_directories(moonclient_calloptions PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES LIENS, LES IMAGES ET LES MENTIONS DU CHAT, dans « Conversation ». En
# bibliothèque pour que leurs réglages s'éprouvent : une liste de sites mal
# relue, ou une taille hors bornes, ne se voit qu'en jeu.
add_library(moonclient_chatweboptions STATIC
    options/chat_web_options.cpp)
target_link_libraries(moonclient_chatweboptions
    PUBLIC ro_core ro_ui moonclient_settings
    PRIVATE ro_warnings)
target_include_directories(moonclient_chatweboptions PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA MUSIQUE D'UNE CHANSON QUI S'ARRÊTE AVEC ELLE : quelle unité porte quelle
# chanson, et quels bruits de son annonce se taisent. En bibliothèque pour que
# l'essai vérifie que chaque chanson a bien une musique à couper.
add_library(moonclient_songmusic STATIC
    world/song_music.cpp)
target_link_libraries(moonclient_songmusic
    PUBLIC ro_audio
    PRIVATE ro_render ro_warnings)
target_include_directories(moonclient_songmusic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES CLASSEMENTS ET LES AVIS D'ÉTOILE DANS LA CONVERSATION, et les trois
# commandes qui demandent un classement. En bibliothèque pour que leurs lignes
# s'éprouvent : un rang sans nom, une mission sans lien, une étoile mal lue ne
# se voient qu'en jeu.
add_library(moonclient_ranknotices STATIC
    world/rank_notices.cpp
    chat/rank_command.cpp)
target_link_libraries(moonclient_ranknotices
    PUBLIC ro_net ro_sim
    PRIVATE moonclient_chatlinks ro_core ro_vfs ro_warnings)
target_include_directories(moonclient_ranknotices PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES SONS DES IMAGES QUE LE JOUEUR FAIT TAIRE, et la page qui les règle. En
# bibliothèque pour que la clé d'un fichier — son nom seul, en minuscules
# ASCII — s'éprouve : une clé mal formée laisse jouer le son qu'on a tu.
add_library(moonclient_spritesounds STATIC
    options/sprite_sound_options.cpp
    world/sprite_sound_mutes.cpp)
target_link_libraries(moonclient_spritesounds
    PUBLIC ro_ui moonclient_settings
    PRIVATE ro_warnings)
target_include_directories(moonclient_spritesounds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE MENU DU CLIC DROIT SUR UNE ENTITÉ : sur quoi il s'ouvre, ce qu'il propose,
# les PNJ qu'on rend sourds et la page qui les règle. En bibliothèque pour la
# raison habituelle : chaque ligne tient à une règle — griser plutôt
# que retirer, un trait par bloc, le staff en fin de liste —, et aucune ne se
# vérifie en jouant.
add_library(moonclient_entitymenu STATIC
    options/context_menu_options.cpp
    world/blocked_npcs.cpp
    world/entity_menu.cpp
    world/entity_menu_tactics.cpp)
# ⚠ LES LIENS DU CHAT EN PRIVÉ : « Linker ce NPC » ne s'offre que pour un nom
# que la balise `<NAVS>` peut porter, et cette règle est la leur.
target_link_libraries(moonclient_entitymenu
    PUBLIC ro_sim ro_ui moonclient_settings
    PRIVATE moonclient_chatlinks yaml-cpp::yaml-cpp ro_warnings)
target_include_directories(moonclient_entitymenu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE GROUPE ET LES AMIS : ce que la fenêtre et la grille montrent, le rythme
# auquel on demande le PS des membres, les deux pages de réglages, et la
# demande d'adoption reçue. En
# bibliothèque pour la raison habituelle : la composition ne voit le jeu qu'à
# travers une interface, et ses règles — ma tuile hors groupe, un PS périmé qui
# se tait, une demande toutes les 250 ms — ne se vérifient pas en jouant.
add_library(moonclient_party STATIC
    game/adoption_stage.cpp
    party/party_compose.cpp
    party/party_option_rows.cpp
    party/party_options.cpp
    party/party_poll.cpp)
target_link_libraries(moonclient_party
    PUBLIC ro_net ro_sim ro_ui moonclient_settings
    PRIVATE ro_warnings)
target_include_directories(moonclient_party PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA FICHE D'UN MONSTRE : ce qu'on demande au serveur et quand, le relevé de
# Sense, le sprite qu'on titille. À part pour que la politique de demande et le
# filtre des Gardiens s'essaient sans monter le jeu.
add_library(moonclient_monstersheet STATIC
    world/monster_model.cpp
    world/monster_sheet.cpp
    world/monster_sprite.cpp)

target_link_libraries(moonclient_monstersheet
    PUBLIC ro_render ro_render_gl ro_sim ro_ui ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_monstersheet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# L'ATLAS DES RECETTES : le livre lu dans l'installation, la vue qu'on en
# compose, et sa page de réglages. À part pour que la lecture du fichier — et
# l'index inverse qu'elle bâtit — s'essaient sur un texte inventé : un index
# mal bâti ne se voit en jeu que comme une recette qui manque.
add_library(moonclient_craft STATIC
    items/craft_atlas_view.cpp
    items/craft_book.cpp
    items/make_item_compose.cpp
    items/make_item_flow.cpp
    items/make_item_odds.cpp
    items/make_item_recipes.cpp
    items/weapon_refine_flow.cpp
    options/craft_atlas_options.cpp
    options/make_item_options.cpp
    options/weapon_refine_options.cpp)

target_link_libraries(moonclient_craft
    PUBLIC ro_net ro_render ro_sim ro_ui ro_vfs moonclient_itemfacts moonclient_settings
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_craft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA NAVIGATION ET LA MINIMAP : les préférences du joueur, la session qui lit
# le graphe en arrière-plan et suit le guidage, et la composition des deux vues.
# À part pour que le fichier et les compositions s'essaient sans monter le jeu.
add_library(moonclient_navigation STATIC
    world/minimap_compose.cpp
    world/world_map_compose.cpp
    world/world_map_kafra.cpp
    world/world_map_routes.cpp
    world/navi_bug_report.cpp
    world/navi_file.cpp
    world/navi_session.cpp
    world/navi_trail_view.cpp
    world/navigation_compose.cpp)

target_link_libraries(moonclient_navigation
    PUBLIC ro_core ro_formats ro_render ro_sim ro_ui ro_vfs moonclient_bugreport
           moonclient_monstersheet
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_navigation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(moonclient_cursor STATIC
    world/armed_cursor.cpp
    world/ground_unit_pick.cpp
    world/pick_box_view.cpp
    world/talkbox_bubbles.cpp)

target_link_libraries(moonclient_cursor
    PUBLIC ro_render ro_sim ro_ui
    PRIVATE ro_warnings)

target_include_directories(moonclient_cursor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# QUI A LÂCHÉ UN BUTIN : la règle qui désigne l'entité dont l'objet jaillit.
#
# 🔴 EN BIBLIOTHÈQUE POUR LA MÊME RAISON QUE LES DÉGÂTS : elle ne connaît ni
# carte ni rendu — rien qu'un monde et une case —, et la sortir du module qui
# tient les arcs est ce qui permet de l'éprouver sans charger une carte. Le
# souvenir des piliers y loge pour la même raison.
add_library(moonclient_itemdrop STATIC
    world/item_dropper.cpp
    world/item_pillar_memory.cpp)

target_link_libraries(moonclient_itemdrop
    PUBLIC ro_render ro_sim
    PRIVATE ro_warnings)

target_include_directories(moonclient_itemdrop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA PILE DU BANDEAU D'OBTENTION : quand deux ramassages n'en font qu'un, quand
# une ligne meurt, et laquelle tombe quand la pile deborde.
#
# 🔴 EN BIBLIOTHEQUE POUR LA MEME RAISON QUE LES DEGATS : ces trois regles sont
# MUETTES en jeu. Une ligne qui ne fusionne pas ressemble a un second ramassage,
# une ligne qui expire trop tot ressemble a une ligne qu'on n'a pas vue passer,
# et l'ecretage par le mauvais bout jette la plus RECENTE sans que rien ne le
# dise. Elle ne connait ni monde, ni table d'objets, ni texture.
add_library(moonclient_itemtoast STATIC
    world/item_obtain_rules.cpp)

target_link_libraries(moonclient_itemtoast
    PUBLIC ro_ui
    PRIVATE ro_warnings)

target_include_directories(moonclient_itemtoast PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(moonclient_damage STATIC
    options/damage_path_anchors.cpp
    world/battle_log_rules.cpp
    world/damage_text_view.cpp
    world/hit_popup_rules.cpp
    world/number_paths.cpp
    world/struck_place.cpp)

target_link_libraries(moonclient_damage
    PUBLIC ro_render ro_sim ro_ui ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_damage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES BARRES DE VIE ET DE MAGIE DES ENTITÉS : ce qui traduit le MONDE en espèce
# réglable, et projette chaque acteur en pixels.
#
# 🔴 EN BIBLIOTHÈQUE POUR LA MÊME RAISON QUE LES DÉGÂTS : la règle qui décide
# « celui-ci est de mon groupe » tient à un détail de protocole — un seul paquet
# donne la vie d'un autre joueur —, et elle doit se vérifier sans fenêtre, sans
# carte et sans serveur. Sinon, personne ne la vérifie.
#
# Les BARRES D'INCANTATION y logent aussi : même projection, même frontière, et
# la règle « quelle barre cet acteur montre » se vérifie de la même façon.
add_library(moonclient_entitybars STATIC
    world/entity_bar_view.cpp
    world/cast_bar_view.cpp)

target_link_libraries(moonclient_entitybars
    PUBLIC ro_render ro_sim ro_ui
    PRIVATE ro_warnings)

target_include_directories(moonclient_entitybars PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# CE QUI TRADUIT LE MONDE EN RÉGLAGE D'ÉTIQUETTE : l'espèce d'un acteur, et la
# couleur de son nom.
#
# 🔴 EN BIBLIOTHÈQUE POUR LA MÊME RAISON QUE LES BARRES, ET AVEC UN DANGER DE
# PLUS : la couleur d'un nom se décide sur un ÉCART DE NIVEAU, donc sur deux
# nombres dont l'un manque souvent. Une règle qui traiterait « inconnu » comme
# « zéro » peindrait tous les monstres en gris, et personne ne saurait dire si
# c'est le réglage ou le protocole.
add_library(moonclient_names STATIC
    guild/emblem_plates.cpp
    world/name_card_asker.cpp
    world/name_plate_view.cpp)

target_link_libraries(moonclient_names
    PUBLIC ro_formats ro_render ro_sim
    PRIVATE ro_warnings)

target_include_directories(moonclient_names PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA TRADUCTION DES COULEURS D'UN JOUEUR EN APPARENCE, en bibliothèque pour la
# même raison que les précédentes : elle ne connaît que `sim` et `render`, et
# elle est INVISIBLE hors du jeu.
#
# 🔴 ET CELLE-CI SE VOIT MOINS QUE TOUTE AUTRE. Une recette mal traduite ne
# plante pas et ne laisse pas de trou : elle donne des couleurs PLAUSIBLES,
# mais AUTRES que celles que les autres joueurs voient sur le même personnage.
# Personne, devant son propre écran, ne peut s'en apercevoir.
#
# Ce que l'éditeur RANGE y vit aussi : ses préréglages et son brouillon ne
# connaissent que `sim` et un fichier YAML, et les pannes qu'ils existent pour
# empêcher sont muettes en jeu — un code d'une version périmée qui reste dans
# la liste, un préréglage mis à jour qui saute en fin de liste, un brouillon
# réécrit à chaque image.
add_library(moonclient_styles STATIC
    character/style_recipe.cpp
    character/style_store.cpp
    world/actor_styles.cpp)

target_link_libraries(moonclient_styles
    PUBLIC ro_render ro_sim
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_styles PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES POSTURES DE COMPETENCE ET LES POSES MENEES, en bibliotheque pour etre
# essayees : elles ne lisent que l'acteur, le catalogue et le rendu, et c'est
# la que se decide quand une pose finit et quand une blessure n'a pas lieu.
add_library(moonclient_postures STATIC
    world/actor_held_image.cpp
    world/actor_led_pose.cpp
    world/actor_look.cpp
    world/actor_skill_posture.cpp
    world/frozen_poses.cpp)

target_link_libraries(moonclient_postures
    PUBLIC ro_audio ro_render ro_render_gl ro_sim ro_vfs
    PRIVATE ro_warnings)

target_include_directories(moonclient_postures PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES DESTINATIONS D'UN LIEN DE REGLAGE, en bibliotheque et pour la meme raison
# que les autres : ce fichier ne connait qu'une table de cles et le rang d'une
# section. En faire une cible rend essayable la seule chose qui compte ici --
# qu'une cle deja ecrite dans un script mene encore quelque part. Une cle perdue
# ne casse rien de visible : le lien cesse de se former, et personne ne sait
# pourquoi.
#
# ⚠ `ro_ui` ET `ro_sim` EN PRIVÉ : la table nomme les onglets des fenêtres par
# leurs énumérations, et rien d'elles ne paraît dans l'en-tête.
add_library(moonclient_settinglinks STATIC
    options/setting_destinations.cpp
    options/setting_links.cpp)

target_link_libraries(moonclient_settinglinks
    PUBLIC ro_core
    PRIVATE ro_sim ro_ui ro_warnings)

target_include_directories(moonclient_settinglinks PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(moonclient_settings STATIC
    chat/chat_history_file.cpp
    chat/chat_log_file.cpp
    chat/chat_tab_file.cpp
    chat/chat_tabs.cpp
    chat/chat_type_label.cpp
    game/settings_file.cpp
    character/character_styles.cpp
    character/recent_characters.cpp
    character/shortcut_file.cpp)

# ⚠ `moonclient_whispers` EN PRIVÉ : un onglet ne compte pas pour non lue une
# ligne qu'une conversation 1:1 a prise au fil.
target_link_libraries(moonclient_settings
    PUBLIC ro_core ro_sim
    PRIVATE moonclient_whispers yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES SKINS DU JOUEUR, sur le disque : où se range une pièce d'interface, et
# ce que le skin d'origine doit porter pour se lire SANS ARCHIVE. En
# bibliothèque pour qu'une garde contre les chemins qui sortiraient de
# l'installation s'éprouve.
add_library(moonclient_skins STATIC
    shell/skin_folder.cpp)

target_link_libraries(moonclient_skins
    PUBLIC ro_core ro_vfs
    PRIVATE ro_render ro_ui ro_warnings)

target_include_directories(moonclient_skins PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# L'HABILLAGE DU SKIN que le joueur règle, ses thèmes et ses profils. En
# bibliothèque pour qu'un thème, la réinitialisation et le fichier s'éprouvent.
add_library(moonclient_skinlook STATIC
    options/skin_look_code.cpp
    options/skin_look_options.cpp
    options/skin_look_profiles.cpp)

target_link_libraries(moonclient_skinlook
    PUBLIC ro_ui moonclient_settings moonclient_profiles
    PRIVATE ro_warnings)

target_include_directories(moonclient_skinlook PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA MISE EN PAGE DE LA TABLÉE, sur le disque, en bibliothèque.
#
# ⚠ À PART DES AUTRES RÉGLAGES parce qu'elle seule parle en types de `ro_ui` :
# la mise en page EST l'état de la scène, et la lier à `moonclient_settings`
# ferait dépendre tous ses fichiers d'ImGui. Ce qui s'achète ici est un banc —
# lire, écrire et relire la mise en page sans fenêtre.
add_library(moonclient_seatlayout STATIC
    shell/seat_layout_file.cpp)

target_link_libraries(moonclient_seatlayout
    PUBLIC ro_core ro_ui
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_seatlayout PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA POSE DU DÉCOR DE L'ÉCRAN DE CONNEXION, en bibliothèque.
#
# 🔴 `ro_render` ET RIEN D'AUTRE : ce sont des matrices, pas une scène. Ce que
# la contrainte de lien achète ici est un BANC — le signe de l'élévation est un
# défaut qui ne se voit pas dans le code et coûte cher à l'écran, et une pose
# qui ne dépend que de trois nombres se vérifie par assertion plutôt qu'en
# lançant le client.
add_library(moonclient_backdrop STATIC
    shell/backdrop_camera.cpp)

target_link_libraries(moonclient_backdrop
    PUBLIC ro_render
    PRIVATE ro_warnings)

target_include_directories(moonclient_backdrop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# L'ÉCRAN DE VEILLE, en bibliothèque, et pour les deux raisons habituelles.
#
# 🔴 `ro_render` ET RIEN D'AUTRE : ni SDL, ni le monde, ni la fenêtre. Ce module
# décide — s'endormir, se réveiller, avaler un clic — et pose trois nombres sur
# une caméra ; il ne dessine rien et ne lit aucun événement du système. La
# boucle lui TRADUIT ses événements, ce qui rend le filtre d'entrées essayable
# sans fenêtre : ses cas intéressants sont justement ceux qu'on ne peut pas
# rejouer à la main — un relâchement dont l'appui a été confisqué, une touche
# qui n'a pas le droit de réveiller, une orbite de cinq tours qui doit rentrer
# par le plus court chemin.
#
# ⚠ ET LA RAMPE SE VÉRIFIE PAR ASSERTION. Une transition de deux secondes et
# demie ne se juge pas à l'œil : c'est le même argument que pour la pose du
# décor, juste au-dessus.
add_library(moonclient_afk STATIC
    afk/afk_input.cpp
    afk/afk_screen.cpp)

target_link_libraries(moonclient_afk
    PUBLIC ro_render
    PRIVATE ro_warnings)

target_include_directories(moonclient_afk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LA MACHINE À ÉTATS DU DÉMARRAGE, en bibliothèque, et la contrainte de lien
# EST la raison d'être de cette cible.
#
# 🔴 `ro_core` ET `ro_net`, RIEN D'AUTRE. Pas de SDL, pas de `ro_render`, pas de
# `ro_ui` : l'éditeur de liens refuse tout ce qui toucherait la fenêtre. Une
# machine à états qui pourrait dessiner finirait par le faire, et l'écran de
# connexion deviendrait le panneau de trois mille lignes qu'on connaît.
#
# ⚠ ET C'EST CE QUI LA REND ESSAYABLE. Tout ce qui vit dans l'EXÉCUTABLE n'a
# aucun essai : `tests/CMakeLists.txt` ne se lie qu'à des bibliothèques, si bien
# que `connected_session.cpp` et `main.cpp` n'ont jamais été
# éprouvés par autre chose qu'un lancement à la main. La séquence de démarrage
# est précisément le code qu'on ne peut pas essayer à la main : elle traverse
# trois serveurs, et ses cas intéressants sont des refus.
add_library(moonclient_lobby STATIC
    lobby/backdrop_session.cpp
    lobby/enter_veil.cpp
    lobby/lobby_flow.cpp
    lobby/session_drive.cpp)

target_link_libraries(moonclient_lobby
    PUBLIC ro_core ro_net
    PRIVATE ro_warnings)

target_include_directories(moonclient_lobby PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE LOGIN WEB, EN DEUX CIBLES, et la coupure est celle de `ro_ui` / `ro_ui_gl`.
#
# 🔴 `moonclient_web` NE CONNAÎT AUCUNE PLATEFORME : le lecteur JSON, la
# machine à états du parcours web, les formes de requête et de réponse, et ce
# que la porte retient d'un lancement à l'autre. Aucun appel au système, donc
# elle se bâtit et s'ESSAIE partout — y compris sur le runner Linux sous
# sanitizers, qui est précisément l'endroit où l'on veut éprouver un lecteur
# d'octets venus du réseau.
#
# ⚠ ET C'EST LA MOITIÉ INTÉRESSANTE. Le parcours a six états, trois refus et un
# jeton qui expire ; le transport, lui, envoie une requête et rend un corps.
# Mettre les deux ensemble aurait rendu le premier inessayable à cause du
# second. La mémoire du compte suit la même coupure : ce qu'on retient et
# comment on l'écrit sont ICI, le coffre du système est en face.
#
# ⚠ `yaml-cpp` N'EST PAS UNE PLATEFORME, c'est un format — le format
# d'écriture de ce dépôt. Il se bâtit sur les trois systèmes, et la propriété
# qui compte pour cette cible reste entière.
add_library(moonclient_web STATIC
    web/emblem_web.cpp
    web/json_value.cpp
    web/multipart_body.cpp
    web/user_config_web.cpp
    web/web_credentials.cpp
    web/web_login.cpp)

target_link_libraries(moonclient_web
    PUBLIC ro_core
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

target_include_directories(moonclient_web PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LE PATCHER, SANS PLATEFORME : la liste des patchs du serveur, l'état que
# l'installation en garde, et la marche d'une chaîne — télécharger, vérifier,
# appliquer. Ce sont des octets venus du réseau et des fichiers que rien ne
# protège ; derrière une interface de réseau, tout s'éprouve partout.
add_library(moonclient_patcher STATIC
    patcher/patch_apply.cpp
    patcher/patch_list.cpp
    patcher/patch_run.cpp
    patcher/patcher_state.cpp)

target_link_libraries(moonclient_patcher
    PUBLIC ro_core ro_formats moonclient_config
    PRIVATE ro_vfs moonclient_web ro_warnings)

target_include_directories(moonclient_patcher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES DOS DU RÉSEAU, DU COFFRE ET DES IMAGES : ce que le SYSTÈME fournit.
#
# ⚠ SÉPARÉS POUR LA MÊME RAISON QUE `ro_ui_gl` : ces fichiers seuls touchent à
# la plateforme, et c'est ce qui permet à tout le reste du parcours d'être bâti
# et ESSAYÉ partout. Chaque service a un en-tête neutre et une implémentation
# par plateforme :
#
#   service                 Windows                      Linux
#   login web (fil)         WinHTTP                      libcurl
#   login web (coffre)      DPAPI                        libsecret + OpenSSL
#   patcher (réseau)        WinHTTP                      libcurl
#   patcher (verrou)        fichier sans partage         flock
#   patcher (relance)       CreateProcess                fork + execv
#   aperçus du chat         WinHTTP                      libcurl
#   WebP                    WIC                          libwebp
#
# 🔴 LES BIBLIOTHÈQUES DU SYSTÈME, PAS DES COPIES LIVRÉES : la TLS et le
# magasin de certificats sont ceux que l'administrateur entretient. Sous
# Linux, libcurl, libsecret et OpenSSL viennent donc de la distribution
# (`libcurl4-openssl-dev`, `libsecret-1-dev`, `libssl-dev`) ; seul libwebp,
# qui ne touche ni au réseau ni aux clés, vient de vcpkg.
if(WIN32)
    set(_web_http_sources web/win_http_transport.cpp web/win_secret_store.cpp)
    set(_web_http_libraries crypt32 winhttp)
    set(_patcher_net_sources
        patcher/win_http_patch_fetcher.cpp
        patcher/win_install_lock.cpp
        patcher/win_relaunch.cpp)
    set(_patcher_net_libraries winhttp)
    set(_chatweb_net_sources web/win_http_get.cpp web/wic_picture.cpp)
    set(_chatweb_net_libraries winhttp windowscodecs ole32)
else()
    find_package(CURL REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
    find_package(WebP CONFIG REQUIRED)
    set(_web_http_sources web/curl_http_transport.cpp web/linux_secret_store.cpp)
    set(_web_http_libraries CURL::libcurl OpenSSL::Crypto PkgConfig::LIBSECRET)
    set(_patcher_net_sources
        patcher/curl_patch_fetcher.cpp
        patcher/posix_install_lock.cpp
        patcher/posix_relaunch.cpp)
    set(_patcher_net_libraries CURL::libcurl)
    set(_chatweb_net_sources web/curl_preview_get.cpp web/libwebp_picture.cpp)
    set(_chatweb_net_libraries CURL::libcurl WebP::webp WebP::webpdemux)
endif()

add_library(moonclient_web_http STATIC
    web/http_transport.cpp
    ${_web_http_sources})

target_link_libraries(moonclient_web_http
    PUBLIC moonclient_web
    PRIVATE ${_web_http_libraries} ro_warnings)

target_include_directories(moonclient_web_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(moonclient_patcher_net STATIC ${_patcher_net_sources})

target_link_libraries(moonclient_patcher_net
    PUBLIC moonclient_patcher
    PRIVATE ${_patcher_net_libraries} ro_warnings)

target_include_directories(moonclient_patcher_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# LES APERÇUS DES LIENS DU CHAT : un saut HTTP et le décodage du WebP. La
# boucle des sauts et la liste blanche, elles, sont pures et vivent dans
# `ro_core`.
add_library(moonclient_chatweb_net STATIC ${_chatweb_net_sources})

target_link_libraries(moonclient_chatweb_net
    PUBLIC ro_core ro_formats
    PRIVATE ${_chatweb_net_libraries} ro_warnings)

target_include_directories(moonclient_chatweb_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(moonclient
    world/actor_carts.cpp
    world/actor_census.cpp
    world/actor_footprints.cpp
    world/actor_falcons.cpp
    world/actor_models.cpp
    world/actor_motion.cpp
    world/actor_named_pace.cpp
    world/actor_sounds.cpp
    world/actor_sounds_ambience.cpp
    world/sound_zone_view.cpp
    world/actor_talk.cpp
    world/actor_view.cpp
    world/battle_log.cpp
    world/swing_replay.cpp
    input/camera_aim.cpp
    input/camera_rig.cpp
    chat/chat_image_cache.cpp
    chat/chat_mentions.cpp
    chat/chat_tab_marks.cpp
    chat/chat_tab_view.cpp
    chat/chat_web_stage.cpp
    chat/chat_meter_tab.cpp
    chat/chat_room_lines.cpp
    chat/chat_whisper_lines.cpp
    chat/chat_view.cpp
    chat/dev_commands.cpp
    chat/native_slash_commands.cpp
    items/description_view.cpp
    world/attack_intent.cpp
    world/click_action.cpp
    effects/effect_bench_view.cpp
    chat/emoji_catalog.cpp
    chat/emotion_catalog.cpp
    effects/effect_layers.cpp
    effects/effect_hit_view.cpp
    game/game_chat_links.cpp
    game/map_plan_tip.cpp
    game/client_shell.cpp
    shell/backdrop_effects.cpp
    shell/backdrop_scene.cpp
    shell/banquet_dolls.cpp
    shell/newcomer_preview.cpp
    shell/lobby_banquet.cpp
    shell/lobby_layout.cpp
    shell/lobby_screen.cpp
    shell/lobby_web_door.cpp
    shell/login_settings.cpp
    patcher/patch_stage.cpp
    game/game_pieces.cpp
    game/skin_reload.cpp
    world/ground_unit_view.cpp
    world/item_drops.cpp
    chat/emotion_view.cpp
    world/sleep_bubbles_view.cpp
    world/quest_marker_view.cpp
    game/command_line.cpp
    world/overlay_view.cpp
    world/skill_area_view.cpp
    world/grey_world_view.cpp
    world/graffiti_view.cpp
    game/connected_session.cpp
    world/hit_popups.cpp
    world/damage_path_sample.cpp
    world/item_obtain_toast.cpp
    world/item_news_report.cpp
    world/item_pickup_report.cpp
    items/cart_view.cpp
    items/bank_view.cpp
    items/card_insert_view.cpp
    items/storage_view.cpp
    items/view_equip_view.cpp
    character/character_view.cpp
    character/preset_view.cpp
    character/staff_jobs.cpp
    character/staff_view.cpp
    capture/capture_clipboard.cpp
    capture/zone_recorder.cpp
    game/game_menu_route.cpp
    game/menu_icon_route.cpp
    items/inventory_view.cpp
    input/key_action.cpp
    chat/macro_commands.cpp
    game/main.cpp
    world/map_music.cpp
    world/npc_talk.cpp
    world/npc_illustration_stage.cpp
    world/npc_talk_view.cpp
    game/zone_stream_capture.cpp
    character/skill_book_view.cpp
    world/screen_overlays.cpp
    world/selection_marker.cpp
    world/target_hud_view.cpp
    game/game.cpp
    game/game_afk.cpp
    game/game_description.cpp
    game/game_draw.cpp
    game/game_grey_world.cpp
    game/game_map_effects.cpp
    game/over_world_layer.cpp
    game/game_cursor.cpp
    game/game_entity_menu.cpp
    game/game_network.cpp
    game/handshake_pump.cpp
    game/game_zone_move.cpp
    game/game_view_equip.cpp
    game/game_monster_sheet.cpp
    game/game_craft_atlas.cpp
    game/game_navigation.cpp
    game/game_monster_gestures.cpp
    game/game_mvp_tracker.cpp
    game/game_craft.cpp
    game/craft_stage.cpp
    game/game_npc_links.cpp
    game/game_item_drag.cpp
    game/game_card_insert.cpp
    game/message_language.cpp
    game/warp_list_stage.cpp
    game/cart_choice_stage.cpp
    game/spell_list_stage.cpp
    game/repair_stage.cpp
    game/talkbox_stage.cpp
    game/cash_shop_stage.cpp
    game/npc_shop_stage.cpp
    game/homunculus_stage.cpp
    game/mercenary_stage.cpp
    game/companion_ai_stage.cpp
    game/companion_ai_trace.cpp
    game/debug_console_route.cpp
    game/companion_gestures.cpp
    game/companion_ai_tactics.cpp
    game/companion_ai_window_stage.cpp
    game/companion_ai_profiles_stage.cpp
    game/companion_ai_window_profiles.cpp
    game/pet_stage.cpp
    game/barter_stage.cpp
    game/buying_store_stage.cpp
    game/buying_store_visit.cpp
    game/vending_stage.cpp
    game/vending_views.cpp
    game/vending_visit_stage.cpp
    game/store_search_stage.cpp
    game/deal_stage.cpp
    game/deal_views.cpp
    game/trade_items.cpp
    game/trade_stages.cpp
    game/game_chat.cpp
    game/game_containers.cpp
    game/game_sheet.cpp
    game/game_guild.cpp
    game/game_party.cpp
    party/party_panel.cpp
    guild/guild_emblem_editor.cpp
    guild/guild_emblems.cpp
    guild/guild_panel.cpp
    guild/guild_skill_rows.cpp
    game/game_open.cpp
    game/game_targeting.cpp
    game/hud_lock.cpp
    game/game_quick_cast.cpp
    options/afk_options.cpp
    options/commands_page.cpp
    options/game_option_apply.cpp
    options/game_option_armed.cpp
    options/game_option_bars.cpp
    options/game_option_capture.cpp
    options/game_option_damage.cpp
    options/game_option_damage_keys.cpp
    options/damage_path_rows.cpp
    options/game_option_direct.cpp
    options/game_option_gauges.cpp
    options/game_option_pvp.cpp
    options/game_option_statuses.cpp
    options/game_option_entities.cpp
    options/game_option_names.cpp
    options/game_option_items.cpp
    options/game_option_toast.cpp
    options/game_option_keys.cpp
    options/game_option_rows.cpp
    options/game_option_skill_area.cpp
    options/graphics_options.cpp
    options/monster_sheet_options.cpp
    options/quick_cast_options.cpp
    options/card_album_options.cpp
    options/description_options.cpp
    options/option_pages.cpp
    options/pet_options.cpp
    options/bug_report_options.cpp
    options/menu_icons_options.cpp
    options/map_sign_options.cpp
    options/window_title_options.cpp
    options/minimap_panel_options.cpp
    options/skin_options.cpp
    options/cast_bar_options.cpp
    options/pick_precision_options.cpp
    options/damage_meter_options.cpp
    options/shop_options.cpp
    options/discord_relay_options.cpp
    options/whisper_options.cpp
    options/grey_world_options.cpp
    game/bug_report_route.cpp
    items/item_doll.cpp
    world/entity_sample.cpp
    items/doll_effects.cpp
    options/target_options.cpp
    game/display_settings.cpp
    options/game_options.cpp
    options/sound_options.cpp
    game/game_settings.cpp
    game/setting_destination_open.cpp
    game/settings_profiles.cpp
    game/damage_meter_stage.cpp
    character/shortcut_view.cpp
    character/grimoire_keys.cpp
    character/skill_description_view.cpp
    character/skill_facts_asker.cpp
    character/skill_view.cpp
    character/style_preview.cpp
    character/style_view.cpp
    game/system_fonts.cpp
    game/ui_layouts.cpp
    game/ui_stage.cpp
    game/ui_stage_lobby.cpp
    game/ui_stage_trade.cpp
    game/mail_stage.cpp
    game/mail_views.cpp
    game/ui_stage_mail.cpp
    game/chat_room_stage.cpp
    game/ui_stage_chat.cpp
    game/ui_stage_chat_room.cpp
    game/whisper_stage.cpp
    game/ui_stage_whisper.cpp
    game/achievement_stage.cpp
    game/ui_stage_achievements.cpp
    game/game_quests.cpp
    game/quest_compose.cpp
    game/quest_stage.cpp
    game/tracker_stage.cpp
    game/ui_stage_quests.cpp
    game/ui_stage_mvp_tracker.cpp
    game/ui_stage_battleground.cpp
    game/ui_stage_navigation.cpp
    game/window_events.cpp)

target_link_libraries(moonclient
    PRIVATE moonclient_chatlinks moonclient_skillrefusal moonclient_songmusic moonclient_ranknotices moonclient_chatpreview moonclient_cursor moonclient_targeting moonclient_quickcast moonclient_calloptions moonclient_spritesounds moonclient_debug moonclient_versus moonclient_damage moonclient_display moonclient_entitybars moonclient_names moonclient_equip moonclient_effect_nodes moonclient_effects moonclient_stage moonclient_map_sign moonclient_itemdrop moonclient_items moonclient_itemtoast moonclient_statuses moonclient_titles
            moonclient_visibility moonclient_afk moonclient_backdrop moonclient_camera_rules moonclient_cardalbum moonclient_mvptracker moonclient_battleground moonclient_battlefield moonclient_craft moonclient_itemfacts moonclient_bugreport moonclient_books moonclient_barter moonclient_buyingstore moonclient_storesearch moonclient_vending moonclient_chatroom moonclient_whispers moonclient_mail moonclient_achievements moonclient_profiles moonclient_entitymenu moonclient_party moonclient_hotkeys moonclient_menuicons moonclient_companionai moonclient_keys moonclient_lobby moonclient_monstersheet moonclient_navigation moonclient_quests
            moonclient_config moonclient_patcher_net moonclient_skins moonclient_skinlook moonclient_postures moonclient_presets moonclient_styles moonclient_web_http
            moonclient_chatweboptions moonclient_chatweb_net
            moonclient_seatlayout moonclient_settings moonclient_settinglinks ro_audio ro_core ro_formats
            ro_net ro_sim ro_vfs
            ro_render ro_render_gl ro_ui ro_ui_gl SDL3::SDL3 yaml-cpp::yaml-cpp
            ro_warnings)

# L'ICÔNE, ET SEULEMENT SUR WINDOWS.
#
# ⚠ UN `.rc` N'EST PAS UNE SOURCE PORTABLE : il se compile par `rc.exe`, qui
# n'existe que sur cette plateforme. Le poser dans `add_executable` sans garde
# casserait les presets Linux, où l'exécutable se bâtit aussi.
#
# ⚠ ET IL EST DÉCLARÉ, pas ramassé : c'est la règle du dépôt, et elle vaut
# autant pour une ressource que pour un `.cpp` — un fichier ajouté sans être
# déclaré doit provoquer une erreur, pas disparaître.
#
# ⚠ LES SOURCES PUBLIÉES N'ONT PAS L'ICÔNE (`scripts/publish_mirror.py`) :
# l'exécutable y prend alors celle de Windows par défaut.
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/moonclient.ico")
    target_sources(moonclient PRIVATE resources/moonclient.rc)
endif()

# 🔴 LE FICHIER PORTE LE NOM DU JEU, PAS CELUI DE LA CIBLE, et c'est la raison
# d'être de ce dépôt : `Moonlight-Destiny.exe` est le nom de l'exécutable que
# ce client REMPLACE, celui que le joueur lance et celui sous lequel les
# archives de mise à jour livrent l'exécutable — le patcher intégré ne
# remplace le programme en cours que s'il porte ce nom. La cible, elle, reste
# `moonclient` — c'est un identifiant, et le renommer ne rendrait rien.
set_target_properties(moonclient PROPERTIES OUTPUT_NAME "Moonlight-Destiny")

# PAS DE CONSOLE HORS DEBUG. Un programme du sous-système console en ouvre une à
# son lancement, et un joueur n'a rien à y lire : le journal va dans
# `moonclient.log`, et le staff l'ouvre en jeu. En Debug, elle reste, parce
# qu'on y lit le journal pendant qu'on développe.
#
# ⚠ LE POINT D'ENTRÉE EST NOMMÉ, et c'est ce qui garde `main` : le sous-système
# Windows chercherait sinon un `WinMain`.
if(MSVC)
    target_link_options(moonclient PRIVATE
        "$<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS;/ENTRY:mainCRTStartup>")
endif()

# LE DÉPÔT AUTOMATIQUE DANS UNE INSTALLATION, quand on en désigne une.
#
# ⚠ ÉTEINT PAR DÉFAUT, ET LE CHEMIN N'EST PAS DANS LE DÉPÔT : `E:\Moonlight-Destiny`
# est le poste d'une personne. Il se pose dans `CMakeUserPresets.json`, qui est
# gitignoré, avec `"MOONCLIENT_DEPLOY_DIR": "E:/Moonlight-Destiny"`.
#
# 🔴 CE N'EST PAS UNE LIVRAISON, ET LE SCRIPT LE DIT À CHAQUE FOIS. Une copie
# posée par le build ne peut pas exiger ce qu'exige une livraison — arbre
# propre, tampon accordé, symboles archivés —, puisqu'on bâtit justement pour
# essayer. Elle dépose donc en BROUILLON et énumère ce qui manquerait ; c'est
# `deploy_client.py` sans `--apres-build` qui refuse.
#
# ⚠ ET ELLE NE PEUT PAS FAIRE ÉCHOUER LE BUILD : le script rend toujours zéro
# dans ce mode. Un disque externe endormi ne doit pas casser une compilation.
#
# Les sources publiées n'ont pas `scripts/` : l'option y reste sans effet.
if(MOONCLIENT_DEPLOY_DIR AND EXISTS "${CMAKE_SOURCE_DIR}/scripts/deploy_client.py")
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_custom_command(TARGET moonclient POST_BUILD
            COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_SOURCE_DIR}/scripts/deploy_client.py"
                    --dest "${MOONCLIENT_DEPLOY_DIR}"
                    --build "${CMAKE_BINARY_DIR}"
                    --config "$<CONFIG>"
                    --apres-build
            COMMENT "dépôt dans ${MOONCLIENT_DEPLOY_DIR}"
            VERBATIM)
    else()
        message(WARNING "MOONCLIENT_DEPLOY_DIR est posé mais Python est introuvable : "
                        "aucun dépôt automatique.")
    endif()
endif()

# Les en-têtes que chaque source relit, précompilés : voir cmake/build_speed.cmake.
if(TARGET moonclient)
  moonclient_precompile_headers(moonclient GLM)
endif()
