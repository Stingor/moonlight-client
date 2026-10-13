#include "ro/ui/character_window.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/chat_link_offer.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "character_panes.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// La hauteur d'ouverture, et le PLANCHER sous lequel la fenêtre ne descend pas.
//
// ⚠ CE SONT DEUX CHOSES DIFFÉRENTES : l'une se pose une fois, l'autre à chaque
// image. Sous le plancher, les quatre rangées du mannequin ne tiennent plus et
// la dernière se ferait couper — ce qui ne se voit pas comme un manque de place
// mais comme une case disparue.
//
// ⚠ LA LARGEUR D'OUVERTURE, ELLE, NE S'ÉCRIT PAS ICI : c'est le cran « large »,
// mesuré à la police courante. Un nombre en dur serait juste à une seule taille
// d'interface.
constexpr float kFirstHeightPixels = 490.0F;
constexpr float kMinimumHeightPixels = 450.0F;
// Les compagnons que ce plancher loge sans grandir : le premier prend la place
// libre de la rangée des mains, le second la rangée qu'il ajoute dessous.
// Chacun de plus empile une rangée de cases sous la colonne de gauche, et le
// plancher la gagne.
constexpr std::size_t kCompanionsWithinFloor = 2;
// L'arrondi de la fenêtre et celui des cadres de ses onglets, plus ronds que la
// peau commune.
constexpr float kWindowRoundingPixels = 8.0F;
constexpr float kChildRoundingPixels = 6.0F;
// Ce que la hauteur ne dépasse pas. Aucune fenêtre n'est aussi haute ; c'est la
// valeur d'« absence de plafond » d'ImGui, qui exige un nombre.
constexpr float kNoHeightCeiling = 10000.0F;

// ⚠ LA TOLÉRANCE DU REPLI, et elle n'est pas cosmétique. Sans ces quelques
// pixels, le volet des nombres clignote : il apparaît, sa présence change la
// largeur disponible d'une fraction, il disparaît, et ainsi de suite.
//
// ⚠ EN PIXELS D'ÉCRAN, JAMAIS MIS À L'ÉCHELLE. C'est une marge d'arrondi, pas
// une longueur d'interface : à l'échelle elle deviendrait assez large pour
// laisser passer un volet réellement trop serré. Même raison que la tolérance de
// bord d'écran de la chatbox.
constexpr float kStatsPaneSlack = 6.0F;

// Le seul bouton de la barre de titre, au rang de sa pose.
constexpr int kBugReportRank = 0;

// LE PLANCHER DE LA FENÊTRE : une rangée de cases de plus par compagnon au-delà
// de ceux qu'il loge. Les compagnons restent EN COLONNE, à la place que le
// client leur donne ; c'est la fenêtre qui leur cède la hauteur.
//
// ⚠ IL VAUT POUR TOUS LES ONGLETS, et non pour le seul onglet Équipement qui
// montre les compagnons : un plancher propre à l'onglet ferait sauter la
// hauteur de la fenêtre à chaque passage de l'un à l'autre.
//
// ⚠ PLUS CE QUE LE GIF AJOUTE SOUS LES POSES, tel que le mannequin l'a MESURÉ
// à son dernier dessin — voir `CharacterAvatarState::gif_rows_height`. Déjà en
// pixels d'écran : il ne se remet pas à l'échelle.
[[nodiscard]] float MinimumHeight(std::size_t companion_count, float gif_rows_height) {
  const std::size_t extra_rows =
      companion_count > kCompanionsWithinFloor ? companion_count - kCompanionsWithinFloor : 0;
  const float row_step = kEquipSlotPixels + kEquipSlotGapPixels;
  return (kMinimumHeightPixels + static_cast<float>(extra_rows) * row_step) * Scale() +
         gif_rows_height;
}

// La largeur du volet du mannequin : le plus large de son bloc et des cases
// posées sous les poses, plus ce que l'enfant prend de marge. ⚠ MESURÉE À LA
// POLICE COURANTE et non figée : le plancher ne fait que l'empêcher de
// descendre sous ce qui reste lisible.
[[nodiscard]] float DollPaneWidth(const SkinArt& art) {
  const float scale = Scale();
  const float frame = ImGui::GetStyle().WindowPadding.x * 2.0F;
  const float content = std::max(kCharacterDollBlockPixels * scale, DollSettingWidth(art));
  return std::max(kCharacterDollPaneMinimum * scale, std::ceil(content + frame));
}

// CALE LA LARGEUR SUR LE CRAN LE PLUS PROCHE.
//
// 🔴 C'EST CE QUI FAIT APPARAÎTRE LE VOLET DES NOMBRES D'UN SEUL GESTE. Sans
// lui, élargir la fenêtre la laisse à une largeur quelconque : le volet reste
// caché tant qu'on n'a pas franchi son seuil au pixel près, et le joueur tire la
// poignée sans comprendre ce qu'il lui manque.
//
// ⚠ LA MESURE VIENT DE L'IMAGE PRÉCÉDENTE, comme pour la grille du sac. Au tout
// premier redimensionnement les crans ne sont pas encore connus, et la fenêtre
// se cale une image plus tard — ce qui ne se voit pas.
void SnapToPaneSteps(ImGuiSizeCallbackData* asked) {
  const auto* steps = static_cast<const CharacterWidthSteps*>(asked->UserData);
  if (steps == nullptr) {
    return;
  }
  asked->DesiredSize.x = SnappedCharacterWidth(*steps, asked->DesiredSize.x);
}

// Le corps commun aux onglets Équipement et Costume : le mannequin, et les
// nombres à côté de lui quand la place le permet.
void DrawDollAndStats(CharacterWindowState& state, const CharacterWindowView& view,
                      const CharacterHeaderView& header, std::span<const EquipSlotView> slots,
                      const CharacterStatsView& stats, const SkinArt& art,
                      CharacterWindowAction& action) {
  // ⚠ LES DEUX RELEVÉS DE LARGEUR SONT DÉJÀ FAITS, une fois pour la fenêtre
  // entière : `stats_shown` et `staff_shown` valent pour TOUS les onglets, et
  // les refaire ici les rendrait faux dès qu'un autre onglet serait devant.
  const float available = ImGui::GetContentRegionAvail().x;
  const float doll_pane = DollPaneWidth(art);
  const float stats_pane = CharacterStatsPaneWidth(art);
  const float staff_pane = view.staff_allowed ? CharacterStaffPaneWidth(art) : 0.0F;
  const float doll_width = state.stats_shown ? doll_pane : available;

  // La peau rend l'ascenseur d'ImGui transparent : chaque cadre qui peut défiler
  // peint le sien.
  if (ImGui::BeginChild("mannequin", ImVec2(doll_width, 0.0F), ImGuiChildFlags_Borders)) {
    DrawCharacterDoll(header, slots, view.companions, state.avatar, view.avatar_image,
                      view.equipment_shown, view.costumes_shown, view.doll_gif,
                      state.tab == CharacterTab::kCostume, state.hover_preview,
                      ImGui::GetContentRegionAvail().x, art, action);
    PaintWindowScrollbar(art);
  }
  ImGui::EndChild();

  if (!state.stats_shown) {
    return;
  }
  ImGui::SameLine();
  // ⚠ LE RELEVÉ NE VIT QUE POUR L'IMAGE, et il n'a pas à vivre plus longtemps :
  // les deux volets se dessinent l'un après l'autre, dans cet ordre. Le garder
  // d'une image à l'autre servirait un alignement mesuré sur une police qui a pu
  // changer entre-temps.
  CharacterStatRows rows;
  if (ImGui::BeginChild("nombres", ImVec2(stats_pane, 0.0F), ImGuiChildFlags_Borders)) {
    DrawCharacterStats(stats, art, action, rows);
    PaintWindowScrollbar(art);
  }
  ImGui::EndChild();

  if (!state.staff_shown) {
    return;
  }
  ImGui::SameLine();
  if (ImGui::BeginChild("staff", ImVec2(staff_pane, 0.0F), ImGuiChildFlags_Borders)) {
    DrawCharacterStaff(state.staff, view.staff, rows, art, action.staff);
    PaintWindowScrollbar(art);
  }
  ImGui::EndChild();
}

// LE GRIMOIRE, et le volet du staff à sa droite quand il est ouvert.
//
// 🔴 LA GRILLE VIT TOUJOURS DANS SON ENFANT, volet ou non. L'enfant entre dans
// l'identité de tout ce qu'elle contient : ne l'ouvrir qu'avec le volet ferait
// oublier à ImGui l'onglet de métier choisi, et le grimoire retomberait sur le
// premier chaque fois que le volet paraît.
void DrawGrimoireAndStaff(CharacterWindowState& state, const CharacterWindowView& view,
                          std::span<const SkillGroupView> groups, SkillPointTally points,
                          const SkinArt& art, CharacterWindowAction& action) {
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float staff_pane = view.staff_allowed ? CharacterStaffPaneWidth(art) : 0.0F;
  const float available = ImGui::GetContentRegionAvail().x;
  // ⚠ LA PLACE SE REVÉRIFIE ICI : `staff_shown` a été relevé avec les crans de
  // l'onglet d'AVANT à l'image où l'on change d'onglet, et le volet y écraserait
  // la grille le temps que la fenêtre s'élargisse.
  const bool staff_here =
      state.staff_shown && available >= SkillGridWidth() + gap + staff_pane - kStatsPaneSlack;
  const float grid_width = staff_here ? available - gap - staff_pane : 0.0F;
  if (ImGui::BeginChild("grimoire", ImVec2(grid_width, 0.0F), ImGuiChildFlags_None,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    action.grimoire = DrawGrimoireTab(state.grimoire, groups, points, view.staff_allowed, art,
                                      view.grimoire_staff);
  }
  ImGui::EndChild();
  if (!staff_here) {
    return;
  }
  ImGui::SameLine();
  if (ImGui::BeginChild("staff", ImVec2(staff_pane, 0.0F), ImGuiChildFlags_Borders)) {
    // Sans nombres en face, le volet suit son flux naturel : un relevé vide le
    // lui dit.
    DrawCharacterStaff(state.staff, view.staff, CharacterStatRows{}, art, action.staff);
    PaintWindowScrollbar(art);
  }
  ImGui::EndChild();
}

}  // namespace

float SnappedCharacterWidth(const CharacterWidthSteps& steps, float wanted) {
  if (!steps.measured) {
    return wanted;
  }
  // ⚠ AVANT LE CRAN LARGE, et non après : les deux ordres peuvent tomber sur la
  // même image — le grimoire interdit l'étroit pendant que le droit du staff
  // repose son volet —, et c'est le plus large qui doit gagner.
  if (steps.force_extra) {
    return steps.extra;
  }
  // ⚠ UN ONGLET PLEINE LARGEUR INTERDIT L'ÉTROIT, ET RIEN DE PLUS : il part du
  // cran large et laisse le troisième concourir. Le rabattre sur le large
  // refermerait le volet du staff au simple passage par le grimoire.
  float best = steps.force_wide ? steps.wide : steps.narrow;
  if (std::abs(wanted - steps.wide) < std::abs(wanted - best)) {
    best = steps.wide;
  }
  // ⚠ QUAND AUCUN VOLET DE PLUS N'EXISTE, CE CRAN VAUT LE PRÉCÉDENT et le test
  // s'annule de lui-même : la bascule redevient à deux crans sans que rien ici
  // ait à savoir ce qui existe. C'est ce qui rendra le volet du staff additif.
  if (std::abs(wanted - steps.extra) < std::abs(wanted - best)) {
    best = steps.extra;
  }
  return best;
}

bool CharacterPoseAnimates(CharacterPose pose) {
  return pose == CharacterPose::kWalk || pose == CharacterPose::kReady;
}

bool CharacterPoseIsCardinal(CharacterPose pose) { return pose == CharacterPose::kReady; }

const char* CharacterPoseLabel(CharacterPose pose, bool animated) {
  // ⚠ UN AIGUILLAGE DE LITTÉRAUX, pour la même raison que les libellés de
  // cases : `core::Tr` veut voir sa chaîne à l'appel, et une table rendrait des
  // libellés que le relevé des traductions ne saurait pas suivre.
  switch (pose) {
    case CharacterPose::kIdle:
      return core::Tr("Repos");
    case CharacterPose::kWalk:
      return animated ? core::Tr("Marche (animé)") : core::Tr("Marche");
    case CharacterPose::kSit:
      return core::Tr("Assis");
    case CharacterPose::kReady:
      return animated ? core::Tr("Combat (animé)") : core::Tr("Combat");
  }
  return core::Tr("Repos");
}

const char* EquipSlotLabel(EquipSlot slot) {
  // ⚠ UN AIGUILLAGE DE LITTÉRAUX, et non une table indexée. `core::Tr` veut voir
  // sa chaîne à l'appel : une table rendrait un `const char*` que le relevé des
  // traductions ne saurait pas suivre, et les dix libellés disparaîtraient du
  // catalogue sans que rien ne le dise.
  //
  // ⚠ ET « Acc. R » EST À GAUCHE. Le personnage se montre de face, donc sa main
  // droite est de notre côté gauche. Échanger les deux pour « réparer » ce qui
  // n'en a pas l'air retournerait l'écran que les joueurs connaissent.
  switch (slot) {
    case EquipSlot::kHeadTop:
      return core::Tr("Head\ntop");
    case EquipSlot::kHeadMid:
      return core::Tr("Head\nmid");
    case EquipSlot::kHeadLow:
      return core::Tr("Head\nbot");
    case EquipSlot::kArmor:
      return core::Tr("Armor");
    case EquipSlot::kGarment:
      return core::Tr("Garment");
    case EquipSlot::kShoes:
      return core::Tr("Shoes");
    case EquipSlot::kWeapon:
      return core::Tr("Weapon");
    case EquipSlot::kShield:
      return core::Tr("Shield");
    case EquipSlot::kAccessoryLeft:
      return core::Tr("Acc. R");
    case EquipSlot::kAccessoryRight:
      return core::Tr("Acc. L");
    case EquipSlot::kAmmo:
      return core::Tr("Ammo");
  }
  return "";
}

std::array<EquipSlot, kCostumeSlotCount> CostumeSlots() {
  // Deux rangées de deux, dans l'ordre du dessin : la colonne gauche, puis la
  // droite. Ce sont les quatre places d'apparat, et il n'y en a pas d'autres.
  return {EquipSlot::kHeadTop, EquipSlot::kHeadLow, EquipSlot::kHeadMid, EquipSlot::kGarment};
}

CharacterWindowAction DrawCharacterWindow(
    CharacterWindowState& state, const CharacterWindowView& view, const CharacterHeaderView& header,
    std::span<const EquipSlotView> slots, const CharacterStatsView& stats,
    const PresetTabView& presets, const CharacterTitlesView& titles, const GuildView& guild,
    std::span<const SkillGroupView> groups, SkillPointTally points, const SkinArt& art,
    const CompanionTabsView& companions) {
  CharacterWindowAction action;
  // L'attente d'une touche de preset avale tout le clavier : elle s'annule dès
  // que son onglet n'est plus dessiné.
  bool presets_drawn = false;
  if (!state.open) {
    state.presets.capturing = -1;
    return action;
  }

  static WindowName name(kCharacterWindowTitle, kCharacterWindowId);

  // ── LES TROIS CRANS, MESURÉS À LA POLICE COURANTE ─────────────────────────
  //
  // ⚠ ILS S'EXPRIMENT EN LARGEUR DE FENÊTRE : les volets se mesurent en
  // contenu, et c'est le chrome relevé dedans à l'image précédente qui fait la
  // différence. L'oublier laisserait la fenêtre trop étroite d'un padding — le
  // cran serait atteint et le volet resterait caché, ce qui est exactement le
  // défaut que le calage existe pour empêcher.
  const float scale = Scale();
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  state.width.narrow = DollPaneWidth(art) + state.chrome_width;
  state.width.wide = DollPaneWidth(art) + gap + CharacterStatsPaneWidth(art) + state.chrome_width;
  // 🔴 LE TROISIÈME CRAN N'EXISTE QUE POUR QUI A LE DROIT. Sans lui il vaut le
  // second, et le calage redevient de lui-même une bascule à deux crans — voir
  // `SnappedCharacterWidth`, qui n'a jamais eu à savoir ce qui existe.
  state.width.extra =
      view.staff_allowed ? state.width.wide + gap + CharacterStaffPaneWidth(art) : state.width.wide;
  // ⚠ LE GRIMOIRE PREND TOUTE LA LARGEUR — sept colonnes d'icônes — et le cran
  // étroit y est INTERDIT plutôt que de laisser sa grille déborder. La guilde
  // aussi : sa table des membres a six colonnes.
  // Les presets de même : leurs trois boutons du haut n'y tiennent pas.
  state.width.force_wide = state.tab == CharacterTab::kGrimoire ||
                           state.tab == CharacterTab::kGuild || state.tab == CharacterTab::kPresets;
  // 🔴 ET LE CRAN LARGE LUI-MÊME PEUT NE PAS SUFFIRE AU GRIMOIRE. Ses icônes se
  // dessinent au double exact de leur bitmap, donc dans des cases plus larges
  // que celles qui tenaient tout juste dans « mannequin + nombres ». Les DEUX
  // crans du haut se relèvent, et pas seulement le plancher : le plafond
  // passerait sinon sous lui, et une contrainte inversée laisse ImGui choisir.
  //
  // ⚠ ET SON TROISIÈME CRAN PORTE LE VOLET DU STAFF À DROITE DE LA GRILLE : le
  // grimoire est l'autre onglet où l'on a les mains dans le personnage.
  if (state.tab == CharacterTab::kGrimoire) {
    const float grid = SkillGridWidth() + state.chrome_width;
    state.width.wide = std::max(state.width.wide, grid);
    state.width.extra = std::max(
        state.width.extra, view.staff_allowed ? grid + gap + CharacterStaffPaneWidth(art) : grid);
  }
  // 🔴 LE DROIT ARRIVE APRÈS LA FENÊTRE, et le volet se reperdrait à chaque
  // session : voir `CharacterWindowState::staff_wanted`. Le cran se repose donc
  // à l'instant où le droit paraît, et pour UNE image — le joueur doit pouvoir
  // rétrécir juste après.
  if (!view.staff_allowed) {
    // ⚠ LE DROIT PARTI, LE SOUVENIR REDEVIENT DÛ : une reconnexion doit reposer
    // le volet, sans quoi il ne reviendrait qu'au prochain démarrage.
    state.staff_restored = false;
  }
  state.width.force_extra = view.staff_allowed && state.staff_wanted && !state.staff_restored;
  state.staff_restored = state.staff_restored || view.staff_allowed;
  // 🔴 LE VOLET OUVERT SUIT LE CHANGEMENT D'ONGLET. Les onglets n'ont pas le même
  // troisième cran — la grille est plus large que « mannequin + nombres » —, et
  // la largeur de l'un tombe entre deux crans de l'autre : le calage au plus
  // proche refermerait le volet au simple passage par le grimoire.
  const bool tab_changed = state.tab != state.steps_tab;
  state.steps_tab = state.tab;
  state.width.force_extra =
      state.width.force_extra || (tab_changed && view.staff_allowed && state.staff_shown);
  state.width.measured = true;

  // 🔴 LE PLAFOND EST LE CRAN LE PLUS LARGE QUI EXISTE, et ce n'est pas une
  // avarice : au-delà il n'y aurait rien de plus à montrer, et la place gagnée
  // ne ferait qu'étirer le vide entre deux volets.
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(state.width.force_wide ? state.width.wide : state.width.narrow,
             MinimumHeight(view.companions.size(), state.avatar.gif_rows_height)),
      ImVec2(state.width.extra, kNoHeightCeiling), SnapToPaneSteps, &state.width);
  // La fenêtre s'ouvre sur le mannequin ET ses nombres : c'est la vue que le
  // joueur cherche, et la replier tout de suite serait lui faire élargir à
  // chaque ouverture.
  ImGui::SetNextWindowSize(ImVec2(state.width.wide, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);

  SetNextWindowPinnable(&state.pinned);
  SetNextWindowRounding(kWindowRoundingPixels);
  PoseBugReportButton(state.bug_report);
  // ⚠ PAS DE PUCE ICI : cette fenêtre n'a pas de section de réglages à elle.
  // Le jour où elle en aura une, sa lecture ira AVANT ce demi-tour — voir le
  // sac, dont la puce ne répondrait plus sinon une fois la fenêtre repliée.
  if (!BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar)) {
    EndSkinnedWindow();
    state.presets.capturing = -1;
    return action;
  }
  action.report_bug = state.bug_report && TitleActionClicked(kBugReportRank);

  // Ce que la fenêtre prend de marge, pour les crans de l'image suivante.
  state.chrome_width = ImGui::GetWindowWidth() - ImGui::GetContentRegionAvail().x;

  // LES CADRES DES ONGLETS S'ARRONDISSENT : la peau commune ne les arrondit
  // pas, et un cadre à angles vifs jure dans ce corps aux coins adoucis.
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kChildRoundingPixels * scale);

  // ── CE QUE LA LARGEUR PERMET, DIT UNE FOIS POUR TOUTE LA FENÊTRE ───────────
  //
  // ⚠ ICI ET NON DANS L'ONGLET. Ce sont des relevés de LARGEUR : les calculer
  // sous le mannequin les laisserait périmés dès que le grimoire est montré,
  // et l'étage, qui s'en sert pour retenir le volet du staff, enregistrerait
  // « fermé » pour la seule raison qu'un autre onglet est devant.
  //
  // ⚠ LE VOLET SE CACHE ENTIER OU NE SE CACHE PAS. Le laisser se faire rogner
  // donnerait un panneau de nombres coupés et un ascenseur pour aller les
  // chercher, ce qui est pire que de ne pas les montrer du tout.
  {
    const float room = ImGui::GetContentRegionAvail().x;
    const float gaps = ImGui::GetStyle().ItemSpacing.x;
    const float doll_and_stats = DollPaneWidth(art) + gaps + CharacterStatsPaneWidth(art);
    state.stats_shown = room >= doll_and_stats - kStatsPaneSlack;
    // 🔴 ET LE TROISIÈME DEMANDE LE SECOND. Ses commandes se posent EN FACE de
    // la valeur qu'elles changent : sans les rangées des nombres, elles se
    // poseraient en face de rien, et le volet perdrait ce qui le rend lisible.
    // ⚠ SAUF SUR LE GRIMOIRE, qui n'a pas de nombres : le volet s'y pose à côté
    // de la grille et suit son propre flux.
    const bool grimoire = state.tab == CharacterTab::kGrimoire;
    const float body = grimoire ? SkillGridWidth() : doll_and_stats;
    state.staff_shown = (grimoire || state.stats_shown) && view.staff_allowed &&
                        room >= body + gaps + CharacterStaffPaneWidth(art) - kStatsPaneSlack;
  }

  if (BeginSkinTabBar("fiche", art)) {
    // ⚠ LA DEMANDE D'ONGLET SE POSE ICI, ET NULLE PART AILLEURS : ImGui choisit
    // au moment où il dessine, et seul ce drapeau le lui impose. Voir
    // `CharacterWindowState::tab_request`.
    const auto asked_for = [&state](CharacterTab which) {
      return state.tab_request == which ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    };
    // MAJ + CLIC SUR UNE LANGUETTE : le lien de l'onglet dans le chat. ⚠ LU JUSTE
    // APRÈS `BeginTabItem`, quand la languette est encore le dernier élément —
    // le contenu de l'onglet ouvert la remplacerait.
    const auto linked = [&action](bool opened, CharacterTab which) {
      if (OfferChatLink(true)) {
        action.link_tab = which;
      }
      return opened;
    };
    // ⚠ LES DEUX PREMIERS ONGLETS PARTAGENT LEUR CORPS. Seules les cases
    // montrées changent ; le volet des nombres reste à côté des deux, parce
    // qu'un costume ne dispense pas de savoir sa force.
    if (linked(ImGui::BeginTabItem(core::Tr("Équipement"), nullptr,
                                   asked_for(CharacterTab::kEquipment)),
               CharacterTab::kEquipment)) {
      state.tab = CharacterTab::kEquipment;
      DrawDollAndStats(state, view, header, slots, stats, art, action);
      ImGui::EndTabItem();
    }
    if (linked(ImGui::BeginTabItem(core::Tr("Costume"), nullptr, asked_for(CharacterTab::kCostume)),
               CharacterTab::kCostume)) {
      state.tab = CharacterTab::kCostume;
      DrawDollAndStats(state, view, header, slots, stats, art, action);
      ImGui::EndTabItem();
    }
    // Pleine largeur, sans mannequin.
    if (linked(ImGui::BeginTabItem(core::Tr("Presets"), nullptr, asked_for(CharacterTab::kPresets)),
               CharacterTab::kPresets)) {
      state.tab = CharacterTab::kPresets;
      presets_drawn = true;
      if (ImGui::BeginChild("presets", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders)) {
        action.presets = DrawPresetTab(state.presets, presets, art);
        PaintWindowScrollbar(art);
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    // ⚠ PLEINE LARGEUR, SANS MANNEQUIN : c'est une liste, et la fenêtre garde
    // le cran que le joueur lui a donné.
    if (linked(ImGui::BeginTabItem(core::Tr("Titres"), nullptr, asked_for(CharacterTab::kTitles)),
               CharacterTab::kTitles)) {
      state.tab = CharacterTab::kTitles;
      if (ImGui::BeginChild("titres", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders)) {
        DrawCharacterTitles(state.title_filter, titles, action);
        PaintWindowScrollbar(art);
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    // ⚠ PLEINE LARGEUR, et les deux fenêtres de guilde du client y mènent : celle
    // du membre et celle de qui n'en a pas.
    if (linked(ImGui::BeginTabItem(core::Tr("Guilde"), nullptr, asked_for(CharacterTab::kGuild)),
               CharacterTab::kGuild)) {
      state.tab = CharacterTab::kGuild;
      if (ImGui::BeginChild("guilde", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders)) {
        action.guild =
            DrawGuildTab(state.guild, guild, state.grimoire.smooth_icons, art, view.staff_allowed);
        PaintWindowScrollbar(art);
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (linked(
            ImGui::BeginTabItem(core::Tr("Grimoire"), nullptr, asked_for(CharacterTab::kGrimoire)),
            CharacterTab::kGrimoire)) {
      state.tab = CharacterTab::kGrimoire;
      DrawGrimoireAndStaff(state, view, groups, points, art, action);
      ImGui::EndTabItem();
    }
    // ⚠ PLEINE LARGEUR, ET SEULEMENT QUAND IL EXISTE.
    if (companions.homunculus.present &&
        linked(ImGui::BeginTabItem(core::Tr("Homoncule"), nullptr,
                                   asked_for(CharacterTab::kHomunculus)),
               CharacterTab::kHomunculus)) {
      state.tab = CharacterTab::kHomunculus;
      if (ImGui::BeginChild("homoncule", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders)) {
        action.homunculus = DrawHomunculusTab(state.homunculus, companions.homunculus, art);
        PaintWindowScrollbar(art);
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (companions.mercenary.present &&
        linked(ImGui::BeginTabItem(core::Tr("Mercenaire"), nullptr,
                                   asked_for(CharacterTab::kMercenary)),
               CharacterTab::kMercenary)) {
      state.tab = CharacterTab::kMercenary;
      if (ImGui::BeginChild("mercenaire", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders)) {
        action.mercenary = DrawMercenaryTab(state.mercenary, companions.mercenary, art);
        PaintWindowScrollbar(art);
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    EndSkinTabBar();
  }
  // LE COMPAGNON A DISPARU pendant qu'on regardait son onglet : retour au
  // mannequin plutôt qu'une page sans onglet.
  if ((state.tab == CharacterTab::kHomunculus && !companions.homunculus.present) ||
      (state.tab == CharacterTab::kMercenary && !companions.mercenary.present)) {
    state.tab = CharacterTab::kEquipment;
  }
  // ⚠ APRÈS LA BARRE, ET TOUJOURS : elle ne vaut qu'une image, et une demande
  // qui survit rendrait l'onglet impossible à quitter au clic.
  state.tab_request.reset();
  if (!presets_drawn) {
    state.presets.capturing = -1;
  }

  ImGui::PopStyleVar();
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
