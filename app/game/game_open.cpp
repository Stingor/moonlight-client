// L'OUVERTURE D'UNE PARTIE : archives, carte, fenêtre, décor, accessoires.
//
// ⚠ À PART DE LA BOUCLE, et c'est le seul découpage qui vaille ici : ouvrir se
// fait une fois et peut ÉCHOUER à chaque ligne ; une image se refait soixante
// fois par seconde et n'échoue plus. Les mêler donnait une fonction dont on ne
// savait pas, en la lisant, si l'on était encore au démarrage.

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

#include <SDL3/SDL.h>

#include "ro/core/log.h"
#include "ro/formats/assist_address.h"
#include "ro/render/emotion.h"
#include "ro/render/map_scene.h"
#include "ro/ui/chat_emote_names.h"
#include "ro/ui/minimap_window.h"

#include "chat/emoji_catalog.h"
#include "chat/emotion_catalog.h"
#include "game/game.h"
#include "game/system_fonts.h"

namespace moonclient {

Game::Game(ClientShell& shell, ro::net::Connection link, ro::sim::World world,
           const ro::net::ZoneTicket& ticket)
    : shell_(shell),
      client_root_(shell.client_root()),
      vfs_(&shell.vfs()),
      gl_(&shell.gl()),
      settings_(&shell.settings()),
      windows_(shell.windows()),
      interface_(&shell.interface()),
      pieces_(shell.pieces()),
      audio_(shell.audio()),
      link_(std::move(link)),
      world_(std::move(world)),
      ticket_(ticket),
      targeting_(shell.settings().options().targeting().settings()),
      quick_cast_(shell.settings().options().quick_cast().look()),
      music_(shell.music()),
      card_album_(shell.client_root()) {}

ro::core::Result<void> Game::Open(const std::string& map_name) {
  // LE MENU DU CLIC DROIT REPART À BLANC. ⚠ SON ÉTAT SURVIT À LA PARTIE — les
  // fenêtres appartiennent à la coquille —, et sa vue désignait la partie
  // d'avant. Les explications des PNJ bloqués se redisent aussi : c'est une
  // nouvelle session.
  windows_.entity_menu = UiStage::Windows::EntityMenu{};
  settings_->options().context_menu().blocked().ForgetExplanations();
  // LES RÉGLAGES DU SERVEUR REPARTENT DE RIEN : ils sont ceux d'un personnage, et
  // le serveur redira ceux du nouveau à l'entrée en jeu.
  pieces_.commands = CommandsPage{};
  settings_->options().pages().commands_body = pieces_.commands.Body();
  // LA DISPOSITION ET LES RÉGLAGES DE CE PERSONNAGE, posés dès l'entrée. ⚠ LES
  // RÉGLAGES SANS ÊTRE POUSSÉS : la fin de l'ouverture les applique tous.
  shell_.layouts().EnterCharacter(world_.player_name());
  settings_->profiles().EnterCharacter(world_.player_name());
  // UN PROFIL CHARGÉ EN PARTIE redescend ce qu'elle ne lit qu'à l'ouverture.
  settings_->profiles().AfterPose([this]() {
    camera_rig_.Restore(settings_->camera_memory());
    settings_->RestoreChatMacros(windows_);
    windows_.view_equip.state.show_empty = settings_->options().view_equip().show_empty;
    windows_.view_equip.state.compare = settings_->options().view_equip().compare;
  });
  settings_->options().pages().profiles_body =
      [layouts = shell_.layouts().Body(),
       settings = settings_->profiles().Body()](const ro::ui::SkinArt& art) {
        layouts(art);
        settings(art);
      };
  // « VOIR L'ÉQUIPEMENT » AUSSI, pour la même raison, avec ses deux cases
  // retenues : elles ne se relisent qu'ici, et la fenêtre les porte ensuite.
  windows_.view_equip = UiStage::Windows::ViewEquip{};
  windows_.view_equip.state.show_empty = settings_->options().view_equip().show_empty;
  windows_.view_equip.state.compare = settings_->options().view_equip().compare;
  // L'ALBUM DE CARTES SE REFERME, SANS UN MOT AU SERVEUR : la session d'avant
  // est finie, et son verrou avec elle. Sa vue et ses réglages, eux, restent.
  windows_.card_album.open = false;
  windows_.card_album.shown = false;
  // CE QUE CHAQUE ÉTAPE DU DÉMARRAGE COÛTE, en millisecondes.
  //
  // ⚠ MÊME RAISON QUE `MapStage::read_ms` : un chargement se VOIT — le jeu
  // s'arrête pendant qu'il dure —, et le mesurer est le seul moyen de savoir
  // s'il empire. Sans ces lignes, quinze secondes de démarrage passaient pour
  // « la carte est grosse » ; elles ont désigné le téléversement des modèles,
  // qui n'a rien à voir avec la taille de la carte.
  //
  // ⚠ ET ELLE COUVRE TOUT, depuis la première ligne : une étape non mesurée est
  // exactement là où le temps se cache, puisque rien ne la nomme.
  std::uint64_t stage_started_ms = SDL_GetTicks();
  const auto report_stage = [this, &stage_started_ms](const char* what) {
    const std::uint64_t now = SDL_GetTicks();
    ro::core::LogInfo("chargement {} : {} ms", what, now - stage_started_ms);
    stage_started_ms = now;
    // 🔴 ET ON REND LA MAIN AU SYSTÈME. La fenêtre existe DÉJÀ quand cette
    // fonction commence — c'est ce que la coquille a changé —, et une fenêtre
    // dont personne ne vide la file de messages est déclarée « (Ne répond
    // pas) » par Windows. Les accessoires du monde coûtent six à dix secondes
    // à eux seuls. Voir `ClientShell::KeepAlive`.
    shell_.KeepAlive();
  };

  auto first_map =
      LoadMapStage(*vfs_, map_name,
                   ro::render::GreyWorldFlattens(settings_->options().pages().grey_world.look()));
  if (!first_map) {
    return std::unexpected(first_map.error());
  }
  stage_ = std::move(*first_map);
  ReportMapStage((*stage_));
  attempted_map_ = stage_->name;
  background_ = BackgroundOf((*stage_));

  // La grille de franchissement sert à marcher le long du chemin que le serveur
  // calcule, et non en ligne droite.
  planner_ = ro::sim::WalkPlanner(PlannedMapOf((*stage_)));
  // Le monde vise la MÊME grille : elle lui sert à savoir où un coup arrête la
  // marche de sa cible. Voir World::SetWalkMap.
  world_.SetWalkMap(PlannedMapOf((*stage_)));
  report_stage("carte");

  // ⚠ LA FENÊTRE EXISTE DÉJÀ, et elle n'a pas attendu la carte : la coquille
  // l'a ouverte avant nous. Il ne reste qu'à la NOMMER — c'est le seul reste,
  // ici, d'un ordre qui voulait une carte pour avoir une fenêtre.
  gl_->SetTitle(WindowTitle());

  auto terrain = ro::render::TerrainRenderer::Create();
  if (!terrain) {
    return std::unexpected(terrain.error());
  }
  terrain_ = std::move(*terrain);
  if (auto uploaded = terrain_.Upload(stage_->scene.terrain, stage_->scene.terrain_textures,
                                      stage_->scene.atlas.image);
      !uploaded) {
    return std::unexpected(uploaded.error());
  }
  report_stage("terrain");

  auto water = ro::render::WaterRenderer::Create();
  if (!water) {
    return std::unexpected(water.error());
  }
  water_ = std::move(*water);
  if (auto uploaded = water_.Upload(stage_->scene.water, stage_->scene.water_animations);
      !uploaded) {
    return std::unexpected(uploaded.error());
  }
  report_stage("eau");

  auto models = ro::render::ModelRenderer::Create();
  if (!models) {
    return std::unexpected(models.error());
  }
  models_ = std::move(*models);
  if (auto uploaded = models_.Upload(stage_->scene.models); !uploaded) {
    return std::unexpected(uploaded.error());
  }
  report_stage("modèles");

  // Les accessoires du MONDE : garde-robe, tables du client, icônes, coups et
  // humeurs. Leur absence n'empêche pas de jouer — voir game_pieces.h.
  //
  // ⚠ C'EST LA COQUILLE QUI TRANCHE S'IL FAUT LES LIRE : la scène de sélection
  // les a demandés AVANT nous, et les relire ici coûterait une seconde fois le
  // poste le plus long du journal.
  shell_.EnsureWorldPieces();
  report_stage("pièces du monde");

  // Les effets animés : le catalogue les charge une fois chacun, la scène les
  // joue. Sans renderer — un pilote qui refuse le programme —, ils ne
  // paraissent pas, et le reste du jeu tourne quand même.
  camera_rig_.LoadTables(*vfs_);
  effect_catalog_.emplace(*vfs_);
  effect_nodes_.emplace(*vfs_);
  effects_.emplace(*effect_catalog_, *effect_nodes_, *vfs_);
  skill_areas_.emplace(*vfs_);
  graffiti_.emplace(*vfs_);
  if (auto renderer = ro::render::EffectRenderer::Create(); renderer) {
    effect_renderer_ = std::move(*renderer);
  }
  pieces_.doll_effects.emplace(*vfs_);
  report_stage("effets");

  // LE POST-TRAITEMENT DU MONDE. ⚠ MÊME INDULGENCE : un pilote qui refuse ses
  // nuanceurs laisse le monde se peindre droit dans la fenêtre, sans effets
  // d'écran, plutôt que d'empêcher de jouer.
  if (auto post = ro::render::ScenePostProcess::Create(); post) {
    scene_post_ = std::move(*post);
  } else {
    ro::core::LogWarning("graphismes : le post-traitement ne s'est pas monté — {}",
                         ro::core::Describe(post.error()));
  }

  // LA TEINTE DE L'ÉCRAN DE VEILLE. ⚠ MÊME INDULGENCE QUE POUR LES EFFETS : un
  // pilote qui refuse le programme laisse la veille tourner sans vignette —
  // caméra, interface effacée et horloge sont toutes trois ailleurs — plutôt
  // que d'empêcher de jouer.
  if (auto tint = ro::render::ScreenTint::Create(); tint) {
    screen_tint_ = std::move(*tint);
  } else {
    ro::core::LogWarning("écran de veille : la teinte ne s'est pas montée — {}",
                         ro::core::Describe(tint.error()));
  }

  // Le mélangeur ET la musique sont à la coquille ; les BRUITS, eux, sont à la
  // partie : ils portent l'état de ce que chaque acteur a déjà fait entendre.
  actor_sounds_.emplace(*vfs_, kSoundDrawSeed);

  // Les pièges, murs et zones posés au sol. Ce sont des MODÈLES, et ils ont donc
  // leur propre renderer : celui du décor tient la carte, qui ne change qu'au
  // changement de carte, là où une unité se pose et se retire sans cesse.
  ground_units_.emplace(*vfs_);
  auto ground_models = ro::render::ModelRenderer::Create();
  if (!ground_models) {
    return std::unexpected(ground_models.error());
  }
  ground_unit_models_ = std::move(*ground_models);

  if (pieces_.wardrobe.has_value()) {
    auto renderer = ro::render::SpriteRenderer::Create();
    if (!renderer) {
      return std::unexpected(renderer.error());
    }
    sprites_ = std::move(*renderer);
  }

  size_ = gl_->drawable_size();
  aspect_ratio_ = static_cast<float>(size_.width) / static_cast<float>(size_.height);
  camera_ = WholeMapCamera((*stage_), aspect_ratio_);
  {
    // Cadrer la carte entière montrerait un personnage d'un pixel. On vise donc
    // là où le serveur nous a posés, d'assez près pour le voir.
    const ro::sim::Actor* self = world_.player();
    const ro::sim::CellPosition where = self != nullptr ? self->position : ro::sim::CellPosition{};
    const std::array<float, 3> ground = ro::render::CellCenterPosition(
        stage_->scene.gnd, static_cast<std::int32_t>(where.x), static_cast<std::int32_t>(where.y));
    camera_.look_at = glm::vec3(ground[0], ground[1], ground[2]);
    camera_.distance = ro::render::kGameViewDistance;
    camera_.fov_degrees = ro::render::kGameFovDegrees;
    camera_.pitch_degrees = ro::render::kGameElevationDegrees;
    camera_rig_.Restore(settings_->camera_memory());
    camera_rig_.EnterMap(camera_, stage_->name);
  }

  acknowledged_entries_ = world_.map_entries();
  // LES ÉMOTICONES QU'ON PEUT ENVOYER, une fois pour toutes : la grille les
  // montre TOUTES dès qu'elle s'ouvre, donc les étaler n'étalerait rien.
  //
  // 🔴 L'INTERSECTION DE DEUX LISTES, ET IL EN FAUT DEUX. Le CATALOGUE dit
  // ce que le joueur a le droit d'envoyer — une partie des numéros appartient au
  // jeu et aux PNJ. Le SPRITE dit ce que cette installation-ci sait dessiner.
  // Proposer ce qui manque à l'une des deux, c'est promettre une émoticone que
  // personne ne verra — pas même celui qui l'envoie.
  emotion_layers_.clear();
  emotion_frames_.clear();
  emotions_.clear();
  if (pieces_.emotion_icons.has_value()) {
    // ⚠ LA PLACE SE RÉSERVE D'ABORD, et il faut donc compter avant de remplir :
    // les vues pointent DANS ces tableaux, et une réallocation en cours de route
    // les laisserait pendantes. Le premier passage téléverse déjà tout ; le
    // second ne fait que relire ce qui est en cache.
    std::size_t layers = 0;
    std::size_t frames = 0;
    for (std::uint16_t emotion = 0; emotion < kKnownEmotions; ++emotion) {
      if (!EmotionIsOffered(emotion)) {
        continue;
      }
      const std::uint16_t count = pieces_.emotion_icons->FrameCount(emotion);
      for (std::uint16_t frame = 0; frame < count; ++frame) {
        layers += pieces_.emotion_icons->Layers(emotion, frame).size();
      }
      frames += count;
    }
    emotion_layers_.reserve(layers);
    emotion_frames_.reserve(frames);

    for (std::uint16_t emotion = 0; emotion < kKnownEmotions; ++emotion) {
      const std::uint16_t count = pieces_.emotion_icons->FrameCount(emotion);
      const ro::render::EmotionBounds box = pieces_.emotion_icons->ActionBounds(emotion);
      if (!EmotionIsOffered(emotion) || count == 0 || !box.usable()) {
        continue;
      }
      const std::size_t first_frame = emotion_frames_.size();
      for (std::uint16_t frame = 0; frame < count; ++frame) {
        const std::size_t first_layer = emotion_layers_.size();
        for (const ro::render::EmotionLayer& layer :
             pieces_.emotion_icons->Layers(emotion, frame)) {
          ro::ui::ChatEmotionLayer posed{
              .id = static_cast<ImTextureID>(layer.texture),
              .tint = IM_COL32(layer.tint[0], layer.tint[1], layer.tint[2], layer.tint[3])};
          for (std::size_t point = 0; point < posed.corners.size(); ++point) {
            posed.corners[point] = ImVec2(layer.corners[point].x, layer.corners[point].y);
          }
          emotion_layers_.push_back(posed);
        }
        emotion_frames_.push_back(ro::ui::ChatEmotionFrame{
            .layers = std::span<const ro::ui::ChatEmotionLayer>(emotion_layers_)
                          .subspan(first_layer, emotion_layers_.size() - first_layer)});
      }
      // SON NOM `:nom:` SE LIT PAR L'ACTION DU SPRITE, et non par le numéro :
      // c'est ainsi que Discord et les autres clients le rangent.
      const std::optional<std::size_t> action = ro::render::EmotionAction(emotion);
      emotions_.push_back(ro::ui::ChatEmotion{
          .id = emotion,
          .name = EmotionName(emotion),
          // ⚠ LA COMMANDE DU JEU, ET NON LE NOM COURT : le
          // menu des macros écrit ce qui joue la bulle chez
          // TOUS les clients, pas un jeton de cette chatbox.
          .command = EmotionCommandOf(emotion),
          .frames = std::span<const ro::ui::ChatEmotionFrame>(emotion_frames_)
                        .subspan(first_frame, count),
          .frame_ms = pieces_.emotion_icons->FrameMs(emotion),
          .box_min = ImVec2(box.min.x, box.min.y),
          .box_max = ImVec2(box.max.x, box.max.y),
          .chat_token = action.has_value() ? ro::ui::ChatEmoteName(*action) : std::string_view()});
    }
    ro::core::LogInfo("émoticônes : {} proposée(s), {} image(s), {} couche(s)", emotions_.size(),
                      emotion_frames_.size(), emotion_layers_.size());
  }
  // LE FIL LES PEINT AUSSI, rangées par action : le `:nom:` d'une ligne.
  pieces_.chat_web.NoteEmotions(emotions_);
  // LES EMOJI. ⚠ SEULEMENT SI LA POLICE EST LÀ : sans elle, la grille ne
  // rendrait que des losanges « glyphe absent », et l'on chercherait le défaut
  // du côté de la table alors qu'il est dans l'atlas. Un onglet absent se
  // comprend ; un onglet plein de carrés, non.
  emojis_.clear();
  if (!EmojiFontSource().empty()) {
    for (const EmojiCategory& group : EmojiCategories()) {
      // L'intertitre arrive déjà traduit : c'est le catalogue qui le fait, là
      // où les libellés sont des littéraux.
      emojis_.push_back(ro::ui::ChatEmojiGroup{.label = group.label, .items = group.items});
    }
  }

  chat_tabs_.emplace(client_root_);
  // CE QUI S'EST DIT LA PARTIE D'AVANT. ⚠ APRÈS LA LECTURE DES RÉGLAGES : c'est
  // une option, et décochée elle EFFACE le fichier plutôt que de le laisser
  // traîner — il porte les chuchotements en clair.
  chat_tabs_->RestorePast(settings_->options().chat_look().keep_history,
                          settings_->options().chat_look().keep_typed);
  // Les destinataires viennent du MÊME fichier, et donc de la même option :
  // décochée, `RestorePast` l'a effacé et la liste est vide.
  chat_preview_.RestoreWhispers(chat_tabs_->restored_whispers());
  // ── CE QU'ON AVAIT TAPÉ, REMIS SOUS LA FLÈCHE ↑ ─────────────────────────
  // ⚠ SEULE LA FENÊTRE PRINCIPALE A UNE SAISIE — une flottante renvoie le
  // clavier à celle-ci —, il n'y a donc qu'un historique à remplir.
  ro::ui::ChatInputHistory& typed = windows_.chat.main.input.history;
  typed.lines.assign(chat_tabs_->restored_typed().begin(), chat_tabs_->restored_typed().end());
  // ⚠ LE CURSEUR SUR LE PRÉSENT, et non sur la dernière ligne relue : `at`
  // vaut `size()` quand on est sur le brouillon, et c'est de là qu'un premier
  // ↑ doit partir. Posé ailleurs, il rendrait l'avant-dernière ligne.
  typed.at = static_cast<int>(typed.lines.size());
  // LES DIX MACROS, descendues UNE FOIS dans les tampons de leur fenêtre : voir
  // `GameSettings::RestoreChatMacros`.
  settings_->RestoreChatMacros(windows_);
  sheet_.emplace(pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
                 pieces_.skill_icons.has_value() ? &*pieces_.skill_icons : nullptr);
  bag_.emplace(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
               &pieces_.item_words,
               pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr);
  card_insert_.emplace(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                       &pieces_.item_words,
                       pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr);
  character_.emplace(
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
      &pieces_.item_words, pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
      &pieces_.job_names, pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
      &pieces_.status_table, pieces_.skill_icons.has_value() ? &*pieces_.skill_icons : nullptr,
      pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr);
  // Les presets se rangent sous l'identifiant du personnage.
  presets_.emplace(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                   &pieces_.item_words,
                   pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr, client_root_,
                   ticket_.character_id);
  title_view_.emplace(
      pieces_.achievement_titles.has_value() ? &*pieces_.achievement_titles : nullptr,
      pieces_.titles.has_value() ? &*pieces_.titles : nullptr);
  style_view_.emplace(pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr, client_root_);
  style_preview_.emplace(pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr);
  cart_.emplace(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                &pieces_.item_words,
                pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr);
  storage_.emplace(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                   &pieces_.item_words,
                   pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr);
  item_description_.emplace(
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
      &pieces_.item_words, pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr);
  description_preview_.emplace(
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
      &pieces_.item_words, pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr);
  // L'équipement d'un autre joueur se décrit et se survole comme le nôtre.
  item_description_->SeeViewedEquipment(&inspected_);
  item_description_->SeePackages(&pieces_.package_items);
  item_description_->SeeRandomOptions(&pieces_.random_option_names);
  description_preview_->SeeViewedEquipment(&inspected_);
  description_preview_->SeeRandomOptions(&pieces_.random_option_names);
  description_preview_->ShareCreatorsWith(*item_description_);
  item_description_->EnableComparison();
  item_description_->books().See(
      vfs_, &pieces_.books,
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr);
  // Les signets se rangent sous l'identifiant du personnage, comme les presets.
  item_description_->books().RememberFor(client_root_, ticket_.character_id);
  item_description_->SeeWardrobe(pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr);
  // LA BOUTIQUE DE VOTE REPART À ZÉRO : la session d'avant est finie, et le
  // serveur renverra son catalogue à qui le redemande.
  pieces_.cash_shop.See(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                        pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
                        pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr);
  pieces_.cash_shop.StartSession();
  windows_.cash_shop = UiStage::Windows::CashShop{};
  // LES COMMERCES AUSSI : aucune boutique, aucune échoppe ne survit à la partie.
  pieces_.trade.See(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                    pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
                    &pieces_.item_words);
  pieces_.trade.SeeCommands(&pieces_.commands.settings());
  pieces_.trade.deal().SeeApproachClock(&grimoire_.approach_clock());
  pieces_.trade.store_search().SeeCards(
      {.illustrations = &pieces_.card_illustrations, .listed = &pieces_.card_items});
  pieces_.trade.vending().SeeSaveRoot(client_root_);
  pieces_.trade.StartSession();
  windows_.trade = TradeWindows{};
  // LE COURRIER AUSSI : ni boîte, ni écriture ne survivent à la partie.
  pieces_.mail.See(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                   pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
                   &pieces_.item_words, &pieces_.job_names,
                   pieces_.game_images.has_value() ? &*pieces_.game_images : nullptr);
  pieces_.mail.StartSession();
  // NI LA DEMANDE D'ADOPTION REÇUE.
  pieces_.adoption.StartSession();
  windows_.mail = MailWindows{};
  // LES SALONS : on n'est dans aucun. ⚠ LA SAISIE DU FORMULAIRE SURVIT, elle
  // n'appartient pas au personnage.
  pieces_.chat_rooms.See(client_root_);
  // L'IA DES COMPAGNONS : les profils de ce personnage relus, ses compagnons à
  // neuf.
  pieces_.companion_ai.See(client_root_, ticket_.character_id);
  pieces_.chat_rooms.StartSession();
  windows_.chat_room.room = ro::ui::ChatRoomWindowState{};
  windows_.chat_room.password = ro::ui::ChatRoomPasswordState{};
  windows_.chat_room.make.open = false;
  // LES CONVERSATIONS 1:1 : aucune n'est ouverte, aucun envoi n'attend.
  pieces_.whispers.StartSession(world_.last_chat_seq());
  windows_.whispers = WhisperWindows{};
  // LA LISTE DE BLOCAGE DU COMPTE, que le service web garde d'une connexion à
  // l'autre : relue maintenant, rejouée après la première carte.
  pieces_.whispers.RememberBlocks(
      shell_.web().empty() ? std::string() : ro::formats::AssistOrigin(shell_.web()),
      web::UserConfigIdentity{.account_id = ticket_.account.account_id,
                              .token = ticket_.account.web_token,
                              .world_name = ticket_.account.world_name});
  // LES MENTIONS : ce qui a été dit avant d'entrer ne sonne pas.
  pieces_.mentions.StartSession(world_.last_chat_seq());
  // LES SUCCÈS AUSSI : leur progression est celle du personnage qui entre.
  pieces_.achievements.See(
      pieces_.achievement_catalog.has_value() ? &*pieces_.achievement_catalog : nullptr,
      &pieces_.item_words, pieces_.titles.has_value() ? &*pieces_.titles : nullptr,
      &pieces_.status_table,
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
      pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
      pieces_.game_images.has_value() ? &*pieces_.game_images : nullptr);
  pieces_.achievements.SeeSaveRoot(client_root_);
  pieces_.achievements.StartSession();
  windows_.achievements = AchievementWindows{};
  // LES QUÊTES DE MÊME : le journal est celui du personnage qui entre, et le
  // serveur le redit à l'entrée sur la carte. Le suivi reprend ses réglages.
  pieces_.quests.See(
      pieces_.quest_catalog.has_value() ? &*pieces_.quest_catalog : nullptr,
      pieces_.quest_catalog_problem,
      pieces_.recommended_catalog.has_value() ? &*pieces_.recommended_catalog : nullptr,
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
      pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr, &pieces_.item_words,
      pieces_.game_images.has_value() ? &*pieces_.game_images : nullptr);
  pieces_.quests.SeeSaveRoot(client_root_);
  // LES RÉGLAGES DE LA MINIMAP AU PANNEAU D'OPTIONS : ceux que la partie tient,
  // par le dessin du sous-menu de la minimap. Repris à la fin de la partie.
  settings_->options().pages().minimap_panel.Entrust([this](const ro::ui::SkinArt& art) {
    return ro::ui::DrawMinimapSettings(navi_.prefs.minimap, art);
  });
  pieces_.quests.StartSession();
  windows_.quests = QuestWindows{};
  pieces_.tracker.SeeSaveRoot(client_root_);
  pieces_.tracker.StartSession();
  windows_.tracker = TrackerWindows{};
  // LE CARNET DE CHASSE MVP : ses sprites, ses images, les noms lisibles des
  // cartes, et ses lignes détachées.
  pieces_.mvp_tracker.See(MvpTrackerPieces{
      .wardrobe = pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr,
      .images = pieces_.game_images.has_value() ? &*pieces_.game_images : nullptr,
      .map_names = pieces_.map_display_names.has_value() ? &*pieces_.map_display_names : nullptr});
  pieces_.mvp_tracker.SeeSaveRoot(client_root_);
  // 🔴 CE QUE LE SERVEUR EN SAVAIT REPART À ZÉRO : catalogue, groupe, alertes et
  // instantané sont ceux du personnage qui entre. Les lignes détachées, l'épingle
  // et l'ouverture de la fenêtre, elles, appartiennent au joueur.
  pieces_.mvp_tracker.StartSession(windows_.mvp_tracker);
  // LA FILE DU CHAMP DE BATAILLE repart hors file : le serveur redit ce qu'il tient.
  pieces_.battleground.StartSession(windows_.battleground);
  // LA POIGNÉE DE MAIN EST NEUVE : nos surfaces se ré-annoncent, même si le
  // serveur nous reconnaît dès la première image.
  pieces_.surfaces.Rearm();
  viewed_equipment_.emplace(
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
      &pieces_.item_words, pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
      pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr);
  // LES FORGERONS : toutes les surfaces qui nomment un item les lisent dans la
  // description, qui les demande.
  const CreatorNames creators = item_description_->creator_names();
  bag_->SeeCreators(creators);
  bag_->SeeDealGuard([this] { return pieces_.trade.deal().HoldsBagGesture(); });
  bag_->SeeMailDesk(InventoryMailDesk{
      .withheld = [this](std::uint16_t slot) { return pieces_.mail.WithheldAt(slot); },
      .holds_use = [this] { return pieces_.mail.HoldsItemUse(); },
      .attach = [this](std::uint16_t slot,
                       int amount) { pieces_.mail.QueueAttach(slot, amount); }});
  bag_->SeePetDesk(InventoryPetDesk{
      .offers =
          [this](const ro::sim::InventoryItem& item) {
            return pieces_.pet.OffersAsAccessory(item);
          },
      .refuses = [this](ro::sim::World& world) { return pieces_.pet.RefusesAccessory(world); }});
  cart_->SeeCreators(creators);
  storage_->SeeCreators(creators);
  card_insert_->SeeCreators(creators);
  character_->SeeCreators(creators);
  viewed_equipment_->SeeCreators(creators);
  chat_tabs_->SeeCreators(creators);
  monster_sheet_.emplace(pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr,
                         &pieces_.item_words,
                         pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
                         pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
                         pieces_.skill_words.has_value() ? &*pieces_.skill_words : nullptr, vfs_);
  monster_sheet_->ReadsMaps(pieces_.map_display_names.has_value() ? &*pieces_.map_display_names
                                                                  : nullptr);
  // LA NAVIGATION : les préférences du joueur, puis les tables du graphe, lues
  // en arrière-plan.
  navi_.Open(shell_.client_root(), shell_.mount_options());
  if (pieces_.wardrobe.has_value()) navi_.preview_sprite.emplace(*pieces_.wardrobe, vfs_);
  // L'ATLAS DES RECETTES : son livre se lit ici, une fois par partie, et sa
  // page de réglages dit ce qu'il contient — ou qu'il manque.
  craft_atlas_.emplace(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                       pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
                       pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr);
  if (vfs_ != nullptr) {
    craft_atlas_->Load(*vfs_);
  }
  // LES ATELIERS lisent le même livre que l'Atlas, et regardent partir ce que
  // le joueur envoie.
  pieces_.craft.See(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                    &pieces_.item_words,
                    pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
                    craft_atlas_->book(), creators);
  pieces_.repair.See(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                     &pieces_.item_words,
                     pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
                     craft_atlas_->book(), creators);
  pieces_.craft.Watch(link_, world_);
  pieces_.trade.deal().Watch(link_);
  if (const CraftBook* book = craft_atlas_->book(); book != nullptr) {
    settings_->options().craft_atlas().NoteTally(
        CraftAtlasTally{.available = true,
                        .recipes = book->recipes().size(),
                        .materials = book->material_count(),
                        .arrows = book->arrows().size()});
  }
  skill_description_.emplace(pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
                             pieces_.skill_words.has_value() ? &*pieces_.skill_words : nullptr,
                             pieces_.skill_icons.has_value() ? &*pieces_.skill_icons : nullptr);
  dialogue_.emplace(pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
                    pieces_.game_images.has_value() ? &*pieces_.game_images : nullptr,
                    pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr,
                    &pieces_.status_table);
  status_view_.emplace(&pieces_.status_table,
                       pieces_.status_icons.has_value() ? &*pieces_.status_icons : nullptr);
  target_hud_.emplace(
      &pieces_.status_table, pieces_.status_icons.has_value() ? &*pieces_.status_icons : nullptr,
      pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr, pieces_.item_words);
  shortcuts_.emplace(client_root_, pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
                     pieces_.skill_icons.has_value() ? &*pieces_.skill_icons : nullptr);
  // La table des objets et leurs icônes arrivent avec les accessoires, après
  // l'arbre des compétences : la barre les reçoit donc à part.
  shortcuts_->UseItems(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
                       pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr);
  // ⚠ APRÈS LA BARRE : les compétences de guilde s'y posent aussi.
  OpenGuild();
  party_panel_.emplace(PartyPanelPieces{
      .images = pieces_.game_images.has_value() ? &*pieces_.game_images : nullptr,
      .jobs = &pieces_.job_names,
      .map_names = pieces_.map_display_names.has_value() ? &*pieces_.map_display_names : nullptr,
      .wardrobe = pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr,
      .status_table = &pieces_.status_table,
      .status_icons = pieces_.status_icons.has_value() ? &*pieces_.status_icons : nullptr});
  // L'onglet retrouvé du fichier, une fois par partie : ensuite, c'est la
  // fenêtre qui le tient.
  windows_.party.window.tab = settings_->options().pages().party.tab();

  // ⚠ EN DERNIER : il garde l'adresse de presque tout ce qui précède, et rien de
  // tout cela ne doit encore bouger quand il la prend.
  chat_dispatch_.emplace(ChatDispatch{
      .link = &link_,
      .world = &world_,
      .effects = &*effects_,
      .effect_nodes = &*effect_nodes_,
      .stage = &*stage_,
      .planner = &planner_,
      .frames = &frame_tally_,
      .packet_window = &windows_.packets.open,
      .skill_window = &windows_.character.open,
      .inventory_window = &windows_.inventory.open,
      .macro_window = &windows_.macros.open,
      .craft_atlas = &windows_.craft_atlas.state,
      .navi = &navi_,
      .navigation = &windows_.navigation.state,
      .map_names = pieces_.map_display_names.has_value() ? &*pieces_.map_display_names : nullptr,
      .armed = &armed_chat_,
      .arming = &armed_chat_,
      .chat_tabs = &*chat_tabs_,
      .options = &settings_->options(),
      .option_targets = OptionsTargets(),
      .chat_rooms = &pieces_.chat_rooms,
      .whispers = &pieces_.whispers.outbox(),
      .whisper_blocks = &pieces_.whispers.blocks(),
      .line_preview = &chat_preview_});
  // ⚠ LA SECONDE PASSE, et la coquille a déjà fait la première.
  //
  // Elle en fait davantage, et c'est pourquoi les deux existent : la musique,
  // la conversation, les effets, le cadrage et le butin n'existaient pas quand
  // la coquille s'est ouverte, et un réglage qui les vise n'avait alors
  // personne à qui parler. Ce que la première a déjà posé — la langue, le plein
  // écran, la taille, les volumes — se repose ici à l'identique.
  //
  // ⚠ ET LA PREMIÈRE N'EST PAS DE TROP : sans elle, tout ce que le démarrage
  // affiche entre les deux le serait dans la langue par défaut et dans une
  // fenêtre à la mauvaise taille — c'est-à-dire, désormais, l'écran de
  // connexion tout entier.
  settings_->ApplyOptions(OptionsTargets());
  start_ticks_ = SDL_GetTicks();
  return {};
}

}  // namespace moonclient
