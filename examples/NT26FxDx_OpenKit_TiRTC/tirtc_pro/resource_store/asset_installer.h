#ifndef TIRTC_ASSET_INSTALLER_H
#define TIRTC_ASSET_INSTALLER_H
/* Installer firmware only. Storage must already be initialized by its sole
 * owner. No retries, erases, formatting, renames or unrelated-file writes. */
int tirtc_assets_install(void);
#endif
