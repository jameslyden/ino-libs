/* Stripped-down version of Arduino SD Library
 * James Lyden <james@lyden.org>
 */

#ifndef SDlite_h
#define SDlite_h
#include <SDlite-file.h>
//------------------------------------------------------------------------------
/** SD version YYYYMMDD */
#define SD_FAT_VERSION 20121219
//------------------------------------------------------------------------------
/** error if old IDE */
#if !defined(ARDUINO) || ARDUINO < 100
#error Arduino IDE must be 1.0 or greater
#endif  // ARDUINO < 100
//------------------------------------------------------------------------------

class SD {
 public:
  SD() {}
  bool chdir(bool set_cwd = false);
  bool chdir(const char* path, bool set_cwd = false);

  bool begin(uint8_t chipSelectPin = SD_CHIP_SELECT_PIN,
    uint8_t sckRateID = SPI_FULL_SPEED);

  /** \return a pointer to the SDspi object. */
  SDspi* card() {return &card_;}
  /** \return a pointer to the SDvol object. */
  SDvol* vol() {return &vol_;}
  /** \return a pointer to the volume working directory. */
  SDfile* vwd() {return &vwd_;}

 private:
  SDspi card_;
  SDvol vol_;
  SDfile vwd_;
};

#endif  // SDlite_h

