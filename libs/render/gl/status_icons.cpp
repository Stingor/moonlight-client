#include "ro/render/status_icons.h"

#include <string>
#include <string_view>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/formats/image.h"
#include "ro/formats/tga.h"
#include "ro/render/status_icon_table.h"
#include "ro/vfs/encoding.h"

namespace ro::render {

StatusIcons::StatusIcons(vfs::Vfs& vfs, const vfs::StatusTable& table)
    : vfs_(&vfs), table_(&table) {}

void StatusIcons::SetJob(std::uint32_t job_id) {
  if (job_id == job_id_) {
    return;
  }
  job_id_ = job_id;
  icons_.clear();
}

std::string StatusIconPath(const vfs::StatusTable& table, std::uint32_t effect_id,
                           std::uint32_t job_id) {
  const vfs::StatusInfo* info = table.Find(effect_id);
  std::string utf8;
  if (info != nullptr && !info->icon_file.empty()) {
    // ⚠ UN NOM NU : la table rend « BD_Sleep.tga », sans dossier. Un nom qui porterait déjà un
    // séparateur est pris tel quel.
    if (info->icon_file.find('\\') == std::string::npos) {
      utf8 = std::string(kStatusIconFolder);
    }
    utf8 += info->icon_file;
  } else {
    // 🔴 LA SECONDE SOURCE, quand le Lua se tait. Le client fait exactement
    // cela : sa fonction Lua d'abord, puis le `switch` de son propre code. Ne
    // lire que la première laisse quatre-vingt-sept états sans image.
    //
    // ⚠ CE CHEMIN-LÀ EST DÉJÀ RELATIF AU DOSSIER DES TEXTURES — « effect\… » —,
    // et non au dossier des effets : le préfixe n'est donc pas le même.
    const std::string_view built_in = BuiltInStatusIconPath(effect_id, job_id);
    if (built_in.empty()) {
      return {};
    }
    utf8 = std::string(kTextureFolder);
    utf8 += built_in;
  }
  // ⚠ TRANSCRIT, ET PAS SEULEMENT PAR PRUDENCE : soixante-sept de ces noms sont
  // en coréen. L'archive indexe en CP949, et un nom laissé en UTF-8 n'y ouvre
  // rien.
  auto encoded = vfs::Utf8ToCp949(utf8);
  return encoded.has_value() ? std::move(*encoded) : std::string();
}

const IconTexture& StatusIcons::Get(std::uint32_t effect_id) {
  const auto known = icons_.find(effect_id);
  if (known != icons_.end()) {
    return known->second;
  }
  IconTexture& icon = icons_[effect_id];

  const std::string path = StatusIconPath(*table_, effect_id, job_id_);
  // 🔴 UNE ABSENCE SE DIT UNE FOIS, ET C'EST TOUT CE QUI LA REND EXPLICABLE. Une
  // case qui se replie sur son abréviation se lit « les icônes sont en panne »
  // tant que rien ne distingue « pas d'image dans cette installation » de « je
  // n'ai pas su la lire ». Le cache garantit un seul message par état.
  if (path.empty()) {
    // ⚠ LE NOM AVEC LE NUMÉRO : un numéro seul oblige à aller le chercher dans
    // une table, et c'est le nom qu'on lit à l'écran quand on se demande
    // pourquoi cette case-là montre des lettres.
    const std::string_view named = table_->NameOf(effect_id);
    // AU DÉTAIL : un état dont la table ne nomme AUCUNE image est une donnée
    // normale du jeu, pas une installation incomplète, et le jeu en compte des
    // dizaines.
    core::LogDebug("icône d'état {} ({}) : aucune image nommée", effect_id,
                   named.empty() ? std::string_view("sans nom") : named);
    return icon;
  }
  auto bytes = vfs_->ReadRaw(path);
  if (!bytes) {
    // ⚠ EN ALERTE COMME SES DEUX SŒURS CI-DESSOUS : c'est le même fichier
    // manquant et le même repli, et les classer différemment faisait qu'une
    // seule des trois causes se voyait à travers le filtre.
    core::LogWarning("icône d'état {} : {} introuvable — la case montrera son abréviation",
                     effect_id, vfs::Cp949ToUtf8Lossy(path));
    return icon;
  }
  // ⚠ UN TGA, DONC UN VRAI ALPHA : pas de clé de couleur magenta, contrairement
  // aux bitmaps d'objet. L'appliquer mangerait les pixels roses d'une icône.
  auto image = formats::DecodeTga(*bytes);
  if (!image) {
    core::LogWarning(
        "icône d'état {} : {} refusé au décodage ({}) — la case montrera son "
        "abréviation",
        effect_id, vfs::Cp949ToUtf8Lossy(path), core::Describe(image.error()));
    return icon;
  }
  // 🔴 CES DEUX SORTIES PARLENT, sans quoi une icône absente serait
  // indiscernable d'une icône refusée. Un chemin juste, un fichier ouvert, une
  // image décodée — et rien à l'écran : sans ces lignes, il ne resterait qu'à
  // deviner laquelle des quatre étapes a cédé.
  auto texture = GlTexture::Upload(*image, GlTexture::Filtering::kPixelArt);
  if (!texture) {
    core::LogWarning(
        "icône d'état {} : {} refusé au téléversement ({}) — la case montrera son "
        "abréviation",
        effect_id, vfs::Cp949ToUtf8Lossy(path), core::Describe(texture.error()));
    return icon;
  }
  icon.texture = std::move(*texture);
  icon.width_px = image->width;
  icon.height_px = image->height;
  return icon;
}

}  // namespace ro::render
