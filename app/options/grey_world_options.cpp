#include "options/grey_world_options.h"

#include <algorithm>
#include <cmath>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les
// joueurs les réglages d'origine, sans que rien ne le dise.
constexpr const char* kEnabledKey = "greyworld";
constexpr const char* kHideModelsKey = "greyworld_hide_models";
constexpr const char* kHideLightmapKey = "greyworld_hide_lightmap";
constexpr const char* kHideMapEffectsKey = "greyworld_hide_map_effects";
constexpr const char* kGridKey = "greyworld_grid";
constexpr const char* kFlatGroundKey = "greyworld_flat_ground";
constexpr const char* kRadiusKey = "greyworld_radius";
constexpr const char* kPatternKey = "greyworld_pattern";
constexpr const char* kFillKey = "greyworld_fill";
constexpr const char* kFlattenKey = "greyworld_flatten";
constexpr const char* kGapKey = "greyworld_gap";
constexpr const char* kGroundColorKey = "greyworld_neck_ground";
constexpr const char* kWalkColorKey = "greyworld_neck_walk";
constexpr const char* kWallColorKey = "greyworld_neck_block";
constexpr const char* kSnipeColorKey = "greyworld_neck_snipe";

[[nodiscard]] int IdOf(GreyWorldRow row) { return kGreyWorldRowBase + static_cast<int>(row); }

}  // namespace

void GreyWorldOptions::Declare(SettingsFile& into) {
  into.Declare(kEnabledKey, &look_.enabled);
  into.Declare(kHideModelsKey, &look_.hide_models);
  into.Declare(kHideLightmapKey, &look_.hide_lightmap);
  into.Declare(kHideMapEffectsKey, &look_.hide_map_effects);
  into.Declare(kGridKey, &look_.grid);
  into.Declare(kFlatGroundKey, &look_.flat_ground);
  into.Declare(kRadiusKey, &look_.radius_cells);
  // ⚠ UN RANG D'ÉNUMÉRATION SE DÉCLARE EN `int*` : le fichier s'ouvre dans un
  // éditeur de texte, et le dessin le relit borné.
  into.Declare(kPatternKey, reinterpret_cast<int*>(&look_.pattern));
  into.Declare(kFillKey, reinterpret_cast<int*>(&look_.fill));
  into.Declare(kFlattenKey, &look_.flatten);
  into.Declare(kGapKey, &look_.gap_percent);
  into.Declare(kGroundColorKey, &look_.ground_color);
  into.Declare(kWalkColorKey, &look_.walk_color);
  into.Declare(kWallColorKey, &look_.wall_color);
  into.Declare(kSnipeColorKey, &look_.snipe_color);
}

void GreyWorldOptions::ComposeInto(std::vector<OptionRowView>& rows) {
  // ⚠ L'ORDRE EST CELUI DE `SkillAreaPattern` ET DE `GreyWorldFill` : une liste
  // déroulante rend un RANG, et deux ordres qui divergent feraient choisir un
  // motif pour un autre.
  pattern_names_.clear();
  pattern_names_.push_back(Tr("Équerres (coins de case)"));
  pattern_names_.push_back(Tr("Carrelage (cadre de case)"));
  pattern_names_.push_back(Tr("Carreau plein (avec joint)"));
  fill_names_.clear();
  fill_names_.push_back(Tr("Toutes les cases"));
  fill_names_.push_back(Tr("Contour des obstacles"));

  // LES LEVIERS RESTENT VISIBLES quand l'interrupteur est levé, simplement
  // grisés : le joueur voit du même coup ce que GreyWorld recouvre, sans avoir
  // à l'allumer pour le découvrir.
  const bool off = !look_.enabled;
  const bool no_grid = off || !look_.grid;

  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kIntro),
      .label = Tr("Le monde dépouillé, comme les GRF « greyworld » que la communauté RO se "
                  "passe pour la WoE — mais réglable, réversible, et sans rien installer : plus "
                  "rien ne cache les monstres, et le sol dit où l'on peut marcher."),
      .kind = OptionKind::kNote});
  rows.back().fold = Tr("GreyWorld");
  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kEnabled),
      .label = Tr("Activer GreyWorld"),
      .help = Tr("Un seul interrupteur pour les quatre leviers ci-dessous.\nRien n'est modifié "
                 "dans tes fichiers : tout se fait à l'exécution, donc sur toutes les cartes, "
                 "même celles ajoutées après coup."),
      .kind = OptionKind::kToggle,
      .on = look_.enabled});

  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kHideModels),
      .label = Tr("Masquer les décors 3D"),
      .help = Tr("Arbres, murs, ponts, rochers, mobilier : tout ce que la carte pose par-dessus "
                 "le sol.\nLes monstres et PNJ en 3D ne sont PAS concernés — ils passent par un "
                 "autre chemin de rendu."),
      .kind = OptionKind::kToggle,
      // ⚠ APLATIR LES EMPORTE : la case le montre, cochée et grisée, au lieu
      // d'en laisser croire le contraire.
      .on = look_.hide_models || look_.flatten,
      .disabled = off || look_.flatten});
  rows.back().heading = Tr("Décors");
  // ESCLAVE DES DÉCORS : grisée tant qu'ils restent visibles.
  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kHideLightmap),
      .label = Tr("Retirer aussi leurs ombres (lightmap)"),
      .help = Tr("L'ombre et la lumière cuites dans le sol, dont celles des décors : sans "
                 "elles, les arbres masqués ne laissent plus leur ombre par terre.\nComme « "
                 "Ombres de la carte » décoché, mais seulement tant que GreyWorld masque les "
                 "décors."),
      .kind = OptionKind::kToggle,
      .on = look_.hide_lightmap,
      .indented = true,
      .disabled = !ro::render::GreyWorldHidesModels(look_)});
  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kHideMapEffects),
      .label = Tr("Masquer les effets de la carte"),
      .help = Tr("Torches, fumées, lueurs, poussières, et le ciel animé — nuages, brume, "
                 "étoiles : les effets que la carte pose d'elle-même.\nLes effets des "
                 "personnages, des monstres et des sorts restent, comme la météo de la carte."),
      .kind = OptionKind::kToggle,
      .on = look_.hide_map_effects,
      .disabled = off});

  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kGrid),
      .label = Tr("Afficher les cellules"),
      .help = Tr("Une case par cellule de la carte, colorée selon ce qu'on peut y faire. Le "
                 "dessin épouse le relief : c'est la fonction dont le client se sert déjà pour "
                 "ton curseur de destination."),
      .kind = OptionKind::kToggle,
      .on = look_.grid,
      .disabled = off});
  rows.back().heading = Tr("Quadrillage du sol");
  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kPattern),
      .label = Tr("Dessin"),
      .help = Tr("Les équerres et le cadre sont des textures du client — celle de ton curseur "
                 "de destination et celle des sorts de zone.\n« Carreau plein » emploie la "
                 "nôtre : la case devient un aplat, un peu plus petit qu'elle, et c'est le sol "
                 "laissé visible tout autour qui trace la bordure."),
      .kind = OptionKind::kChoice,
      .chosen = static_cast<int>(look_.pattern),
      .choices = pattern_names_,
      .indented = true,
      .disabled = no_grid});
  // ⚠ GRISÉ SUR LES DEUX AUTRES MOTIFS : ils dessinent un contour, que creuser
  // rapetisserait au lieu d'espacer.
  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kGap),
      .label = Tr("Joint (%)"),
      .help = Tr("Largeur de la bordure, en pourcentage du côté de la case. À zéro, les "
                 "carreaux se touchent et le quadrillage disparaît."),
      .kind = OptionKind::kSlider,
      .value = look_.gap_percent,
      .lowest = ro::render::kSmallestGreyWorldGapPercent,
      .highest = ro::render::kLargestGreyWorldGapPercent,
      .format = "%.0f %%",
      .indented = true,
      .disabled = no_grid || look_.pattern != ro::render::SkillAreaPattern::kSolid});
  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kFill),
      .label = Tr("Cases dessinées"),
      .help = Tr("« Contour des obstacles » ne garde que les cases qui touchent un terrain "
                 "différent : on lit la SILHOUETTE des murs, et il reste dix fois moins de "
                 "cases à dessiner.\nC'est donc aussi le réglage le moins cher, avant même la "
                 "portée — et il ne peut pas cacher un obstacle, puisqu'il garde toujours les "
                 "bords."),
      .kind = OptionKind::kChoice,
      .chosen = static_cast<int>(look_.fill),
      .choices = fill_names_,
      .indented = true,
      .disabled = no_grid});
  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kRadius),
      .label = Tr("Portée (cellules)"),
      .help = Tr("Demi-côté du carré dessiné autour de toi. C'est LE réglage qui coûte : le "
                 "nombre de cases croît au carré (24 en vaut 2401, 60 en vaut 14 641). "
                 "Baisse-le si tu perds des images par seconde."),
      .kind = OptionKind::kSlider,
      .value = static_cast<float>(look_.radius_cells),
      .lowest = static_cast<float>(ro::render::kSmallestGreyWorldRadiusCells),
      .highest = static_cast<float>(ro::render::kLargestGreyWorldRadiusCells),
      .format = "%.0f",
      .indented = true,
      .disabled = no_grid});
  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kWalkColor),
      .label = Tr("Cellule marchable"),
      .help = Tr("Mets son opacité à zéro pour ne garder que les obstacles : c'est aussi la "
                 "façon la moins chère d'afficher le quadrillage, puisque les cases invisibles "
                 "ne sont plus dessinées du tout."),
      .kind = OptionKind::kColor,
      .color = look_.walk_color,
      .indented = true,
      .disabled = no_grid});
  rows.push_back(OptionRowView{.id = IdOf(GreyWorldRow::kWallColor),
                               .label = Tr("Mur"),
                               .help = Tr("Ni marche, ni tir : la cellule est fermée."),
                               .kind = OptionKind::kColor,
                               .color = look_.wall_color,
                               .indented = true,
                               .disabled = no_grid});
  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kSnipeColor),
      .label = Tr("Infranchissable mais tirable"),
      .help = Tr("Un vide ou une dénivellation : on ne peut pas y aller, mais les sorts et les "
                 "flèches passent."),
      .kind = OptionKind::kColor,
      .color = look_.snipe_color,
      .indented = true,
      .disabled = no_grid});

  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kFlatGround),
      .label = Tr("Sol d'une seule couleur"),
      .help = Tr("Repeint le terrain sans toucher à sa géométrie : l'occlusion par le relief "
                 "reste correcte."),
      .kind = OptionKind::kToggle,
      .on = look_.flat_ground,
      .disabled = off});
  rows.back().heading = Tr("Sol");
  rows.push_back(OptionRowView{.id = IdOf(GreyWorldRow::kGroundColor),
                               .label = Tr("Couleur du sol"),
                               .kind = OptionKind::kColor,
                               .color = look_.ground_color,
                               .indented = true,
                               .disabled = off || !look_.flat_ground});
  if (look_.flat_ground && staff_ground_wins_) {
    rows.push_back(OptionRowView{
        .id = IdOf(GreyWorldRow::kStaffGround),
        .label = Tr("Le « Sol uni » des Staff Tools est actif : c'est SA couleur qui s'affiche."),
        .kind = OptionKind::kNote,
        .indented = true});
  }
  rows.push_back(OptionRowView{
      .id = IdOf(GreyWorldRow::kFlatten),
      .label = Tr("Aplatir le relief"),
      .help = Tr("Met toute la carte à une seule altitude — la moyenne de son relief — comme le "
                 "fait un GRF greyworld, sols et personnages ensemble.\nLes décors ne sont alors "
                 "plus chargés du tout : ils garderaient leur hauteur d'origine et flotteraient "
                 "au-dessus du sol. Les effets de la carte, eux, suivent le sol.\nTout cela est "
                 "figé au chargement de la carte : cocher ou décocher la recharge sur place, "
                 "sans rien demander au serveur. Le jeu marque un temps d'arrêt, tu ne bouges "
                 "pas.\nLe niveau de l'eau n'est pas touché : sur une carte qui en a, le sol "
                 "peut passer dessous."),
      .kind = OptionKind::kToggle,
      .on = look_.flatten,
      .disabled = off});
}

bool GreyWorldOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kGreyWorldRowBase ||
      asked.changed >= kGreyWorldRowBase + static_cast<int>(GreyWorldRow::kCount)) {
    return false;
  }
  switch (static_cast<GreyWorldRow>(asked.changed - kGreyWorldRowBase)) {
    case GreyWorldRow::kEnabled:
      look_.enabled = asked.on;
      break;
    case GreyWorldRow::kHideModels:
      look_.hide_models = asked.on;
      break;
    case GreyWorldRow::kHideLightmap:
      look_.hide_lightmap = asked.on;
      break;
    case GreyWorldRow::kHideMapEffects:
      look_.hide_map_effects = asked.on;
      break;
    case GreyWorldRow::kGrid:
      look_.grid = asked.on;
      break;
    case GreyWorldRow::kPattern:
      // ⚠ BORNÉ À LA LECTURE : le rang vient d'une liste déroulante, mais aussi
      // d'un fichier de réglages qu'on peut avoir écrit à la main.
      look_.pattern = static_cast<ro::render::SkillAreaPattern>(
          std::clamp(asked.chosen, 0, static_cast<int>(ro::render::SkillAreaPattern::kSolid)));
      break;
    case GreyWorldRow::kGap:
      look_.gap_percent = asked.value;
      break;
    case GreyWorldRow::kFill:
      look_.fill = static_cast<ro::render::GreyWorldFill>(
          std::clamp(asked.chosen, 0, static_cast<int>(ro::render::GreyWorldFill::kOutline)));
      break;
    case GreyWorldRow::kRadius:
      look_.radius_cells = static_cast<int>(std::lround(asked.value));
      break;
    case GreyWorldRow::kWalkColor:
      look_.walk_color = asked.color;
      break;
    case GreyWorldRow::kWallColor:
      look_.wall_color = asked.color;
      break;
    case GreyWorldRow::kSnipeColor:
      look_.snipe_color = asked.color;
      break;
    case GreyWorldRow::kFlatGround:
      look_.flat_ground = asked.on;
      break;
    case GreyWorldRow::kGroundColor:
      look_.ground_color = asked.color;
      break;
    case GreyWorldRow::kFlatten:
      look_.flatten = asked.on;
      break;
    case GreyWorldRow::kIntro:
    case GreyWorldRow::kStaffGround:
    case GreyWorldRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
