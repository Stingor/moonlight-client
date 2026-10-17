// LA FICHE ET L'ÉDITEUR DE STYLE, VUS DE LA BOUCLE : ce que la fiche montre du
// personnage — ses pièces, ses presets, ses titres, ses compagnons, son avatar,
// le volet du staff —, la petite fenêtre de statut qui lit ses nombres, et les
// pantins de l'éditeur et de l'aperçu d'un style
// reçu, composés avant le dessin ; puis ce que les presets, le GIF du pantin et
// ces deux-là ont demandé.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "ro/net/guild.h"
#include "ro/render/doll_animation.h"

#include "character/character_view.h"
#include "character/doll_gif_export.h"
#include "character/equip_slot_map.h"
#include "character/staff_view.h"
#include "character/status_window_view.h"
#include "game/game.h"
#include "world/actor_view.h"

namespace moonclient {

void Game::StageCharacterSheet(std::uint32_t now_ms) {
  // LES DÉPARTS DE LA GUILDE S'ÉCRIVENT DANS LA CONVERSATION, même l'écran de
  // veille levé. Le nôtre ferme la fenêtre de guilde : la fiche, quand c'est
  // elle qu'elle montre.
  if (ro::net::SayGuildDepartures(guild_book_, world_, &pieces_.banned_words, now_ms) &&
      windows_.character.tab == ro::ui::CharacterTab::kGuild) {
    windows_.character.open = false;
  }
  // LA FICHE SE REFAIT AVANT LE DESSIN : l'onglet ouvert décide des
  // emplacements cherchés, et il a pu changer à l'image précédente.
  //
  // ⚠ LA PETITE FENÊTRE DE STATUT LIT LES MÊMES NOMBRES : ouverte seule, elle
  // ne fait refaire qu'eux.
  StatusWindowRound& status = windows_.status;
  character_->Refresh(windows_.character.open, world_,
                      windows_.character.tab == ro::ui::CharacterTab::kCostume);
  if (status.state.open && !windows_.character.open) {
    character_->RefreshNumbers(world_);
  }
  // LA FLÈCHE CLIQUÉE À L'IMAGE PRÉCÉDENTE part par la route du « + » de la
  // fiche. ⚠ CONSOMMÉE : laissée en place, elle remonterait la caractéristique
  // à chaque image.
  const ro::ui::StatusWindowAction asked = std::exchange(status.answer, {});
  if (asked.verb != ro::ui::StatusVerb::kNothing) {
    character_->Apply(SheetActionOf(asked), link_, world_);
  }
  // LES PRÉRÉGLAGES DE STATS : ce que leur fenêtre a demandé, puis la montée
  // suivante de l'application en cours, s'il y en a une.
  stat_presets_->Apply(asked.presets, world_, now_ms);
  stat_presets_->Tick(link_, world_, now_ms);
  // ⚠ LE NOM SE COPIE DANS LE PORTEUR : la vue de l'onglet de guilde date de
  // l'image précédente, et le réseau a pu réécrire depuis la chaîne qu'elle
  // désigne.
  status.guild_name = GuildNameOf(guild_book_);
  status.view = StatusWindowViewOf(character_->stats(), status.guild_name);
  stat_presets_->Compose(world_, status.view.presets);
  // Fiche ouverte, et pas seulement sur son onglet : un clic sur l'onglet ne le
  // montre qu'à l'image suivante, que ses lignes doivent déjà servir.
  presets_->Refresh(windows_.character.open, settings_->bindings());
  title_view_->Refresh(titles_);
  // LES COMPAGNONS QU'ON PEUT APPELER : la fiche en fait une colonne, et c'est
  // le monde qui les tient.
  windows_.character_view.companions = character_->companions();
  // LES DEUX RÉGLAGES DE LA CASE SOUS LES POSES, tels que le serveur les a dits.
  // ⚠ REPOSÉS À CHAQUE IMAGE : la fenêtre survit à la partie, et un personnage
  // suivant ne doit pas hériter de ceux du précédent.
  windows_.character_view.equipment_shown = own_config_.equipment_shown();
  windows_.character_view.costumes_shown = own_config_.costumes_shown();
  // LA SILHOUETTE DE L'AVATAR, composée à la boîte que la fenêtre a mesurée à
  // l'image précédente — et refaite seulement quand son image change.
  //
  // ⚠ SEULEMENT FENÊTRE OUVERTE : la boîte mesurée survit à la fermeture, et
  // sans cette garde une fiche refermée continuerait de composer un personnage
  // que personne ne regarde, image d'animation après image d'animation.
  if (windows_.character.open) {
    // ⚠ LE MÊME REGISTRE QUE LA CARTE : deux registres donneraient deux
    // identifiants pour la même recette, donc deux tenues et deux textures
    // pour un seul personnage.
    ro::render::ActorLook dressed;
    const ro::sim::Actor* me = world_.player();
    if (me != nullptr) {
      dressed = LookOf(*me);
      // « VOIR LES COSTUMES » DÉCOCHÉE, SUR L'ONGLET ÉQUIPEMENT : les coiffes et
      // la cape PORTÉES, et non ce que la carte montre. ⚠ CE N'EST PAS LA MÊME
      // CHOSE : une apparence que le serveur impose à une coiffe ou à la cape —
      // le styliste, un événement, `@wings` — reste sur la carte, et l'onglet
      // Équipement dit ce que le personnage porte, pas ce à quoi il ressemble.
      if (windows_.character.tab == ro::ui::CharacterTab::kEquipment &&
          !own_config_.costumes_shown().value_or(true)) {
        const WornViews worn = WornPieceViews(world_.inventory());
        dressed.headgears = worn.headgears;
        dressed.robe = worn.robe;
      }
      if (pieces_.wardrobe.has_value()) {
        styles_.Dress(world_.player_id(), dressed, *pieces_.wardrobe, world_.styles());
      }
    }
    character_->RefreshAvatar(world_, windows_.character.avatar,
                              windows_.character_view.avatar_image, now_ms, dressed);
    sheet_look_ = dressed;
  }
  // LE GIF DU PANTIN : ce que l'encodage et le dialogue ont rendu, puis ce que
  // la fiche en montre. ⚠ MÊME FICHE FERMÉE : un enregistrement fini doit se
  // dire dans la conversation, et l'export d'un fil ne s'abandonne pas.
  if (const std::optional<std::string> saved = pieces_.doll_gif->Tick(); saved.has_value()) {
    world_.RecordChat(ro::sim::ChatKind::kServer, 0, *saved, now_ms);
  }
  windows_.character_view.doll_gif = pieces_.doll_gif->view();
  // ⚠ RELU À CHAQUE IMAGE, comme celui de l'établi — voir `StaffToolsAllowed`.
  // Il montre la ligne du staff du grimoire et le volet du mannequin ; le
  // retirer fait disparaître le volet ET son cran de largeur, sans attendre une
  // reconnexion.
  windows_.character_view.staff_allowed = Rights().staff_tools;
  if (windows_.character_view.staff_allowed) {
    FeedStaffPane(world_, staff_.jobs, &pieces_.job_names, windows_.character_view.staff);
    // LA CRÉATION D'UN HOMONCULE, dans la ligne du staff du grimoire : le
    // serveur refuse d'en créer un second. ⚠ `present()` NE DIT QUE CE QUE LE
    // CLIENT A VU : un homoncule accusé reste présent au repos ou mort, mais
    // celui qui l'était déjà à la connexion n'est jamais annoncé, et le serveur
    // le compte quand même. Le bouton reste alors cliquable.
    windows_.character_view.grimoire_staff = ro::ui::GrimoireStaffView{
        .homunculi = staff_.homunculi, .has_homunculus = pieces_.homunculus.homunculus().present()};
    // 🔴 LE SOUHAIT SE REPOSE ICI, SOUS LE DROIT : c'est lui qui dit que le
    // serveur est prêt à l'entendre, et il n'arrive qu'après l'entrée sur la
    // carte. Plus tôt, la commande serait refusée — et notée comme faite.
    const bool ignore_wanted = settings_->options().staff_battle_ignore();
    windows_.character_view.staff.battle_ignored = ignore_wanted;
    KeepBattleIgnore(ignore_wanted, staff_, link_, world_);
  }
}

void Game::StageStyle(std::uint32_t now_ms) {
  // CE QUE LE STYLISTE A DEMANDÉ. ⚠ CONSOMMÉ : une demande laissée en place
  // rouvrirait la fenêtre à chaque image, et le joueur ne pourrait plus la
  // fermer.
  if (const std::optional<ro::sim::StyleEditorRequest> asked = world_.styles().TakeRequest()) {
    switch (*asked) {
      case ro::sim::StyleEditorRequest::kClose:
        windows_.style.open = false;
        break;
      case ro::sim::StyleEditorRequest::kOpen:
        windows_.style.open = true;
        break;
      case ro::sim::StyleEditorRequest::kToggle:
        windows_.style.open = !windows_.style.open;
        break;
    }
  }

  // NOS COULEURS, GARDÉES POUR LA TABLÉE, qui ne les recevra pas du serveur.
  style_view_->KeepForSelection(world_, ticket_.character_id);

  // L'ÉDITEUR DE STYLE : son pantin et ses pièces. ⚠ SEULEMENT FENÊTRE
  // OUVERTE — composer une silhouette que personne ne regarde coûte une
  // palette et une texture à chaque pulsation.
  if (windows_.style.open) {
    style_view_->Refresh(world_, windows_.style, link_.is_open(), now_ms);
  } else {
    // ⚠ RENDU ENTRE DEUX IMAGES : oublier un style détruit les tenues qui le
    // portaient, et une composition en cours les lirait mortes.
    style_view_->Close();
  }
  // L'APERÇU D'UN STYLE REÇU, sous la même règle et pour la même raison : son
  // pantin ne se compose que fenêtre ouverte, et son style retourne à la
  // garde-robe dès qu'elle se ferme.
  if (windows_.style_preview.open) {
    style_preview_->Refresh(world_, windows_.style_preview);
  } else {
    style_preview_->Close();
  }
  // 🔴 LE BROUILLON S'ÉCRIT MÊME FENÊTRE FERMÉE, et c'est ce qui le rend
  // utile : l'accident visé — plantage, coupure, client tué — ne passe pas
  // par une fermeture propre. Refermer la fenêtre juste après un réglage
  // doit quand même laisser au délai le temps d'expirer.
  style_view_->TickDraft(windows_.style, now_ms);
}

void Game::ApplySheetAnswers(const UiStage::Answers& answers) {
  const ro::ui::PresetAction& asked = answers.character.presets;
  if (asked.toggle_style) {
    windows_.style.open = !windows_.style.open;
  }
  presets_->Apply(asked, windows_.character.presets, link_, world_);

  // LE GIF DU PANTIN, figé sur ce que la fiche montrait à ce clic : la pose, la
  // direction, l'échelle et l'apparence habillée avant le dessin.
  const ro::ui::DollGifVerb gif = answers.character.doll_gif;
  if (gif == ro::ui::DollGifVerb::kNothing) {
    return;
  }
  // ⚠ UN CLIC QUI NE PEUT RIEN FAIRE LE DIT : sans sprites ni personnage, la
  // place de l'avatar est vide, et le GIF le serait aussi.
  if (!pieces_.wardrobe.has_value()) {
    pieces_.doll_gif->Refuse("l'installation n'a pas de garde-robe");
  } else if (world_.player() == nullptr) {
    pieces_.doll_gif->Refuse("aucun personnage à photographier");
  } else {
    const ro::ui::CharacterAvatarState& avatar = windows_.character.avatar;
    const bool plays = avatar.animated && ro::ui::CharacterPoseAnimates(avatar.pose);
    DollGifOrder order;
    order.verb = gif;
    order.look = sheet_look_;
    order.shot = ro::render::DollShot{.animation = CharacterPoseAnimation(avatar.pose),
                                      .direction = avatar.direction,
                                      .body_plays = plays,
                                      .scale = avatar.gif_scale,
                                      .margin_px = ro::render::kDollGifMarginPx};
    order.file_name = ro::ui::DollGifFileName(
        world_.player_name(), ro::ui::CharacterPoseLabel(avatar.pose, avatar.animated),
        avatar.direction);
    pieces_.doll_gif->Start(order, *pieces_.wardrobe, gl_->window(), client_root_);
  }
}

void Game::ApplyStyleAnswers(const UiStage::Answers& answers, std::uint32_t now_ms) {
  // ⚠ APRÈS LE DESSIN : le joueur vient de désigner une couleur, et c'est ici
  // qu'elle devient un réglage.
  style_view_->Apply(answers.style, link_, windows_.style, world_, now_ms);
  // ⚠ CONSOMMÉ : laissé en place, le même code se reposerait à chaque image et
  // la saisie se remplirait toute seule.
  if (const StyleView::StyleToShare shared = style_view_->TakeStyleToShare(); !shared.empty()) {
    PostStyleLink(shared.code, shared.label);
  }
  // ── CE QUE L'APERÇU D'UN STYLE REÇU A DEMANDÉ ─────────────────────────────
  //
  // ⚠ « COPIER » A DÉJÀ EU LIEU : la fenêtre seule parle au presse-papiers,
  // comme l'éditeur. Il ne reste que l'essai.
  if (answers.style_preview == ro::ui::StylePreviewVerb::kTry) {
    windows_.style.open = true;
    static_cast<void>(style_view_->TryCode(style_preview_->code(), windows_.style, now_ms));
  }
}

}  // namespace moonclient
