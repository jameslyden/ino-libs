/* Stripped-down version of Arduino SD Library
 * James Lyden <james@lyden.org>
 */

#ifndef SDlitevol_h
#define SDlitevol_h

#include <SDlite-config.h>
#include <SDlite-info.h>
#include <SDlite-SPI.h>

// Cache for an SD data block
union cache_t {
           /** Used to access cached file data blocks. */
  uint8_t  data[512];
           /** Used to access cached FAT16 entries. */
  uint16_t fat16[256];
           /** Used to access cached FAT32 entries. */
  uint32_t fat32[128];
           /** Used to access cached directory entries. */
  dir_t    dir[16];
           /** Used to access a cached Master Boot Record. */
  mbr_t    mbr;
           /** Used to access to a cached FAT boot sector. */
  fat_boot_t fbs;
           /** Used to access to a cached FAT32 boot sector. */
  fat32_boot_t fbs32;
           /** Used to access to a cached FAT32 FSINFO sector. */
  fat32_fsinfo_t fsinfo;
};

class SDvol {
 public:
  /** Create an instance of SDvol */
  SDvol() : fatType_(0) {}
  /** Initialize a FAT volume.  Try partition one first then try super
   * floppy format.
   */
  bool init(SDspi* dev) { return init(dev, 1) ? true : init(dev, 0);}
  bool init(SDspi* dev, uint8_t part);

  // inline functions that return volume info
  /** The volume's cluster size in blocks. */
  uint8_t blocksPerCluster() const {return blocksPerCluster_;}
  /** The FAT type of the volume. Values are 12, 16 or 32. */
  uint8_t fatType() const {return fatType_;}
  /** The number of entries in the root directory for FAT16 volumes. */
  uint32_t rootDirEntryCount() const {return rootDirEntryCount_;}
  uint32_t rootDirStart() const {return rootDirStart_;}
  /** SDspi object for this volume
   */
  SDspi* sdCard() {return sdCard_;}
//------------------------------------------------------------------------------
 private:
  // Allow SDfile access to SDvol private data.
  friend class SDfile;
//------------------------------------------------------------------------------
  uint32_t allocSearchStart_;   // start cluster for alloc search
  uint8_t blocksPerCluster_;    // cluster size in blocks
  uint32_t blocksPerFat_;       // FAT size in blocks
  uint32_t clusterCount_;       // clusters in one FAT
  uint8_t clusterSizeShift_;    // shift to convert cluster count to block count
  uint32_t dataStartBlock_;     // first data block number
  uint8_t fatCount_;            // number of FATs on volume
  uint32_t fatStartBlock_;      // start block for first FAT
  uint8_t fatType_;             // volume type (12, 16, OR 32)
  uint16_t rootDirEntryCount_;  // number of entries in FAT16 root dir
  uint32_t rootDirStart_;       // root start block for FAT16, cluster for FAT32
//------------------------------------------------------------------------------
// block caches
// use of static functions save a bit of flash - maybe not worth complexity
//
  static const uint8_t CACHE_STATUS_DIRTY = 1;
  static const uint8_t CACHE_STATUS_FAT_BLOCK = 2;
  static const uint8_t CACHE_STATUS_MASK
     = CACHE_STATUS_DIRTY | CACHE_STATUS_FAT_BLOCK;
  static const uint8_t CACHE_OPTION_NO_READ = 4;
  // value for option argument in cacheFetch to indicate read from cache
  static uint8_t const CACHE_FOR_READ = 0;
  // value for option argument in cacheFetch to indicate write to cache
  static uint8_t const CACHE_FOR_WRITE = CACHE_STATUS_DIRTY;
  // reserve cache block with no read
  static uint8_t const CACHE_RESERVE_FOR_WRITE
     = CACHE_STATUS_DIRTY | CACHE_OPTION_NO_READ;
  static cache_t cacheBuffer_;        // 512 byte cache for device blocks
  static uint32_t cacheBlockNumber_;  // Logical number of block in the cache
  static uint32_t cacheFatOffset_;    // offset for mirrored FAT
  static uint8_t cacheStatus_;        // status of cache block
#if USE_SEPARATE_FAT_CACHE
  static cache_t cacheFatBuffer_;       // 512 byte cache for FAT
  static uint32_t cacheFatBlockNumber_;  // current Fat block number
  static uint8_t  cacheFatStatus_;       // status of cache Fatblock
#endif  // USE_SEPARATE_FAT_CACHE
  static SDspi* sdCard_;            // SDspi object for cache

  cache_t *cacheAddress() {return &cacheBuffer_;}
  uint32_t cacheBlockNumber() {return cacheBlockNumber_;}

  static cache_t* cacheFetch(uint32_t blockNumber, uint8_t options);
  static cache_t* cacheFetchData(uint32_t blockNumber, uint8_t options);
  static cache_t* cacheFetchFat(uint32_t blockNumber, uint8_t options);
  static bool cacheSync();
  static bool cacheWriteData();
  static bool cacheWriteFat();
//------------------------------------------------------------------------------
  bool allocContiguous(uint32_t count, uint32_t* curCluster);
  uint8_t blockOfCluster(uint32_t position) const {
          return (position >> 9) & (blocksPerCluster_ - 1);}
  uint32_t clusterStartBlock(uint32_t cluster) const;
  bool fatGet(uint32_t cluster, uint32_t* value);
  bool fatPut(uint32_t cluster, uint32_t value);
  bool fatPutEOC(uint32_t cluster) {
    return fatPut(cluster, 0x0FFFFFFF);
  }
  bool isEOC(uint32_t cluster) const {
    if (FAT12_SUPPORT && fatType_ == 12) return  cluster >= FAT12EOC_MIN;
    if (fatType_ == 16) return cluster >= FAT16EOC_MIN;
    return  cluster >= FAT32EOC_MIN;
  }
  bool readBlock(uint32_t block, uint8_t* dst) {
    return sdCard_->readBlock(block, dst);}
  bool writeBlock(uint32_t block, const uint8_t* dst) {
    return sdCard_->writeBlock(block, dst);
  }
};
#endif  // SDlite-vol_h
