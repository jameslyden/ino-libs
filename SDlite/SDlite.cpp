/* Stripped-down version of Arduino SD Library
 * James Lyden <james@lyden.org>
 */

#include <SDlite.h>
/**
 * Initialize an SD object.
 */
bool SD::begin(uint8_t chipSelectPin, uint8_t sckRateID) {
  return card_.init(sckRateID, chipSelectPin) && vol_.init(&card_) && chdir(1);
}
/** Change a volume's working directory to root
 */
bool SD::chdir(bool set_cwd) {
  if (set_cwd) SDfile::cwd_ = &vwd_;
  if (vwd_.isOpen()) vwd_.close();
  return vwd_.openRoot(&vol_);
}
/** Change a volume's working directory
 */
bool SD::chdir(const char *path, bool set_cwd) {
  SDfile dir;
  if (path[0] == '/' && path[1] == '\0') return chdir(set_cwd);
  if (!dir.open(&vwd_, path, O_READ)) goto fail;
  if (!dir.isDir()) goto fail;
  vwd_ = dir;
  if (set_cwd) SDfile::cwd_ = &vwd_;
  return true;

 fail:
  return false;
}

