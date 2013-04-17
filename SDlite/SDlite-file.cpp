/* Stripped-down version of Arduino SD Library
 * James Lyden <james@lyden.org>
 */

#include <SDlite-file.h>
// macro for debug
#define DBG_FAIL_MACRO  //  Serial.print(__FILE__);Serial.println(__LINE__)
//------------------------------------------------------------------------------
// pointer to cwd directory
SDfile* SDfile::cwd_ = 0;
//------------------------------------------------------------------------------
// add a cluster to a file
bool SDfile::addCluster() {
  if (!vol_->allocContiguous(1, &curCluster_)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // if first cluster of file link to directory entry
  if (firstCluster_ == 0) {
    firstCluster_ = curCluster_;
    flags_ |= F_FILE_DIR_DIRTY;
  }
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
// Add a cluster to a directory file and zero the cluster.
// return with first block of cluster in the cache
cache_t* SDfile::addDirCluster() {
  uint32_t block;
  cache_t* pc;
  // max folder size
  if (fileSize_/sizeof(dir_t) >= 0XFFFF) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!addCluster()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  block = vol_->clusterStartBlock(curCluster_);
  pc = vol_->cacheFetch(block, SDvol::CACHE_RESERVE_FOR_WRITE);
  if (!pc) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  memset(pc, 0, 512);
  // zero rest of clusters
  for (uint8_t i = 1; i < vol_->blocksPerCluster_; i++) {
    if (!vol_->writeBlock(block + i, pc->data)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // Increase directory file size by cluster size
  fileSize_ += 512UL*vol_->blocksPerCluster_;
  return pc;

 fail:
  return 0;
}
//------------------------------------------------------------------------------
// cache a file's directory entry
// return pointer to cached entry or null for failure
dir_t* SDfile::cacheDirEntry(uint8_t action) {
  cache_t* pc;
  pc = vol_->cacheFetch(dirBlock_, action);
  if (!pc) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return pc->dir + dirIndex_;

 fail:
  return 0;
}
//------------------------------------------------------------------------------
/** Close a file and force cached data and directory information
 *  to be written to the storage device.
 */
bool SDfile::close() {
  bool rtn = sync();
  type_ = FAT_FILE_TYPE_CLOSED;
  return rtn;
}
//------------------------------------------------------------------------------
// format directory name field from a 8.3 name string
bool SDfile::make83Name(const char* str, uint8_t* name, const char** ptr) {
  uint8_t c;
  uint8_t n = 7;  // max index for part before dot
  uint8_t i = 0;
  // blank fill name and extension
  while (i < 11) name[i++] = ' ';
  i = 0;
  while (*str != '\0' && *str != '/') {
    c = *str++;
    if (c == '.') {
      if (n == 10) {
        // only one dot allowed
        DBG_FAIL_MACRO;
        goto fail;
      }
      n = 10;  // max index for full 8.3 name
      i = 8;   // place for extension
    } else {
      // illegal FAT characters
#ifdef __AVR__
      // store chars in flash
      PGM_P p = PSTR("|<>^+=?/[];,*\"\\");
      uint8_t b;
      while ((b = pgm_read_byte(p++))) if (b == c) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#else  // __AVR__
      // store chars in RAM
      if (strchr("|<>^+=?/[];,*\"\\", c)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#endif  // __AVR__

      // check size and only allow ASCII printable characters
      if (i > n || c < 0X21 || c > 0X7E) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // only upper case allowed in 8.3 names - convert lower to upper
      name[i++] = c < 'a' || c > 'z' ?  c : c + ('A' - 'a');
    }
  }
  *ptr = str;
  // must have a file name, extension is optional
  return name[0] != ' ';

 fail:
  return false;
}
//------------------------------------------------------------------------------
 /** Open a file in the current working directory.  */
  bool SDfile::open(const char* path, uint8_t oflag) {
    return open(cwd_, path, oflag);
  }
/** Open a file or directory by name.  */
bool SDfile::open(SDfile* dirFile, const char* path, uint8_t oflag) {
  uint8_t dname[11];
  SDfile dir1, dir2;
  SDfile *parent = dirFile;
  SDfile *sub = &dir1;

  if (!dirFile) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // error if already open
  if (isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (*path == '/') {
    while (*path == '/') path++;
    if (!dirFile->isRoot()) {
      if (!dir2.openRoot(dirFile->vol_)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      parent = &dir2;
    }
  }
  while (1) {
    if (!make83Name(path, dname, &path)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    while (*path == '/') path++;
    if (!*path) break;
    if (!sub->open(parent, dname, O_READ)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (parent != dirFile) parent->close();
    parent = sub;
    sub = parent != &dir1 ? &dir1 : &dir2;
  }
  return open(parent, dname, oflag);

 fail:
  return false;
}
// open with filename in dname
bool SDfile::open(SDfile* dirFile,
  const uint8_t dname[11], uint8_t oflag) {
  cache_t* pc;
  bool emptyFound = false;
  bool fileFound = false;
  uint8_t index;
  dir_t* p;

  vol_ = dirFile->vol_;

  dirFile->rewind();
  // search for file

  while (dirFile->curPosition_ < dirFile->fileSize_) {
    index = 0XF & (dirFile->curPosition_ >> 5);
    p = dirFile->readDirCache();
    if (!p) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (p->name[0] == DIR_NAME_FREE || p->name[0] == DIR_NAME_DELETED) {
      // remember first empty slot
      if (!emptyFound) {
        dirBlock_ = vol_->cacheBlockNumber();
        dirIndex_ = index;
        emptyFound = true;
      }
      // done if no entries follow
      if (p->name[0] == DIR_NAME_FREE) break;
    } else if (!memcmp(dname, p->name, 11)) {
      fileFound = true;
      break;
    }
  }
  if (fileFound) {
    // don't open existing file if O_EXCL
    if (oflag & O_EXCL) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else {
    // don't create unless O_CREAT and O_WRITE
    if (!(oflag & O_CREAT) || !(oflag & O_WRITE)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (emptyFound) {
      index = dirIndex_;
      p = cacheDirEntry(SDvol::CACHE_FOR_WRITE);
      if (!p) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    } else {
      if (dirFile->type_ == FAT_FILE_TYPE_ROOT_FIXED) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // add and zero cluster for dirFile - first cluster is in cache for write
      pc = dirFile->addDirCluster();
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // use first entry in cluster
      p = pc->dir;
      index = 0;
    }
    // initialize as empty file
    memset(p, 0, sizeof(dir_t));
    memcpy(p->name, dname, 11);

    // use default date/time
    p->creationDate = FAT_DEFAULT_DATE;
    p->creationTime = FAT_DEFAULT_TIME;
    p->lastAccessDate = p->creationDate;
    p->lastWriteDate = p->creationDate;
    p->lastWriteTime = p->creationTime;

    // write entry to SD
    if (!dirFile->vol_->cacheSync()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // open entry in cache
  return openCachedEntry(index, oflag);

 fail:
  return false;
}
/** Open a file by index.  */
bool SDfile::open(SDfile* dirFile, uint16_t index, uint8_t oflag) {
  dir_t* p;

  vol_ = dirFile->vol_;

  // error if already open
  if (isOpen() || !dirFile) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  // don't open existing file if O_EXCL - user call error
  if (oflag & O_EXCL) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // seek to location of entry
  if (!dirFile->seekSet(32 * index)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // read entry into cache
  p = dirFile->readDirCache();
  if (!p) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // error if empty slot or '.' or '..'
  if (p->name[0] == DIR_NAME_FREE ||
      p->name[0] == DIR_NAME_DELETED || p->name[0] == '.') {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // open cached entry
  return openCachedEntry(index & 0XF, oflag);

 fail:
  return false;
}
// open a cached directory entry. Assumes vol_ is initialized
bool SDfile::openCachedEntry(uint8_t dirIndex, uint8_t oflag) {
  // location of entry in cache
  dir_t* p = &vol_->cacheAddress()->dir[dirIndex];

  // write or truncate is an error for a directory or read-only file
  if (p->attributes & (DIR_ATT_READ_ONLY | DIR_ATT_DIRECTORY)) {
    if (oflag & (O_WRITE | O_TRUNC)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // remember location of directory entry on SD
  dirBlock_ = vol_->cacheBlockNumber();
  dirIndex_ = dirIndex;

  // copy first cluster number for directory fields
  firstCluster_ = (uint32_t)p->firstClusterHigh << 16;
  firstCluster_ |= p->firstClusterLow;

  // make sure it is a normal file or subdirectory
  if (DIR_IS_FILE(p)) {
    fileSize_ = p->fileSize;
    type_ = FAT_FILE_TYPE_NORMAL;
  } else if (DIR_IS_SUBDIR(p)) {
    if (!setDirSize()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    type_ = FAT_FILE_TYPE_SUBDIR;
  } else {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // save open flags for read/write
  flags_ = oflag & F_OFLAG;

  // set to start of file
  curCluster_ = 0;
  curPosition_ = 0;

  return oflag & O_AT_END ? seekEnd(0) : true;

 fail:
  type_ = FAT_FILE_TYPE_CLOSED;
  return false;
}
/** Open a volume's root directory.  */
bool SDfile::openRoot(SDvol* vol) {
  // error if file is already open
  if (isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  vol_ = vol;
  if (vol->fatType() == 16 || (FAT12_SUPPORT && vol->fatType() == 12)) {
    type_ = FAT_FILE_TYPE_ROOT_FIXED;
    firstCluster_ = 0;
    fileSize_ = 32 * vol->rootDirEntryCount();
  } else if (vol->fatType() == 32) {
    type_ = FAT_FILE_TYPE_ROOT32;
    firstCluster_ = vol->rootDirStart();
    if (!setDirSize()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else {
    // volume is not initialized, invalid, or FAT12 without support
    DBG_FAIL_MACRO;
    goto fail;
  }
  // read only
  flags_ = O_READ;

  // set to start of file
  curCluster_ = 0;
  curPosition_ = 0;

  // root has no directory entry
  dirBlock_ = 0;
  dirIndex_ = 0;
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
/** Read the next byte from a file.  */
int16_t SDfile::read() {
  uint8_t b;
  return read(&b, 1) == 1 ? b : -1;
}
/** Read data from a file starting at the current position. */
int SDfile::read(void* buf, size_t nbyte) {
  uint8_t blockOfCluster;
  uint8_t* dst = reinterpret_cast<uint8_t*>(buf);
  uint16_t offset;
  size_t toRead;
  uint32_t block;  // raw device block number
  cache_t* pc;

  // error if not open or write only
  if (!isOpen() || !(flags_ & O_READ)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // max bytes left in file
  if (nbyte >= (fileSize_ - curPosition_)) {
    nbyte = fileSize_ - curPosition_;
  }
  // amount left to read
  toRead = nbyte;
  while (toRead > 0) {
    size_t n;
    offset = curPosition_ & 0X1FF;  // offset in block
    blockOfCluster = vol_->blockOfCluster(curPosition_);
    if (type_ == FAT_FILE_TYPE_ROOT_FIXED) {
      block = vol_->rootDirStart() + (curPosition_ >> 9);
    } else {
      if (offset == 0 && blockOfCluster == 0) {
        // start of new cluster
        if (curPosition_ == 0) {
          // use first cluster in file
          curCluster_ = firstCluster_;
        } else {
          // get next cluster from FAT
          if (!vol_->fatGet(curCluster_, &curCluster_)) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
      }
      block = vol_->clusterStartBlock(curCluster_) + blockOfCluster;
    }
    if (offset != 0 || toRead < 512 || block == vol_->cacheBlockNumber()) {
      // amount to be read from current block
      n = 512 - offset;
      if (n > toRead) n = toRead;
      // read block to cache and copy data to caller
      pc = vol_->cacheFetch(block, SDvol::CACHE_FOR_READ);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      uint8_t* src = pc->data + offset;
      memcpy(dst, src, n);
    } else if (!USE_MULTI_BLOCK_SD_IO || toRead < 1024) {
      // read single block
      n = 512;
      if (!vol_->readBlock(block, dst)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    } else {
      uint8_t nb = toRead >> 9;
      if (type_ != FAT_FILE_TYPE_ROOT_FIXED) {
        uint8_t mb = vol_->blocksPerCluster() - blockOfCluster;
        if (mb < nb) nb = mb;
      }
      n = 512*nb;
      if (vol_->cacheBlockNumber() <= block
        && block < (vol_->cacheBlockNumber() + nb)) {
        // flush cache if a block is in the cache
        if (!vol_->cacheSync()) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
      if (!vol_->sdCard()->readStart(block)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      for (uint8_t b = 0; b < nb; b++) {
        if (!vol_->sdCard()->readData(dst + b*512)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
      if (!vol_->sdCard()->readStop()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    dst += n;
    curPosition_ += n;
    toRead -= n;
  }
  return nbyte;

 fail:
  return -1;
}
// Read next directory entry into the cache
// Assumes file is correctly positioned
dir_t* SDfile::readDirCache() {
  uint8_t i;
  // error if not directory
  if (!isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // index of entry in cache
  i = (curPosition_ >> 5) & 0XF;

  // use read to locate and cache block
  if (read() < 0) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // advance to next entry
  curPosition_ += 31;

  // return pointer to entry
  return vol_->cacheAddress()->dir + i;

 fail:
  return 0;
}
//------------------------------------------------------------------------------
/**  Create a file object and open it in the current working directory.  */
SDfile::SDfile(const char* path, uint8_t oflag) {
  type_ = FAT_FILE_TYPE_CLOSED;
  writeError = false;
  open(path, oflag);
}
//------------------------------------------------------------------------------
/** Sets a file's position.  */
bool SDfile::seekSet(uint32_t pos) {
  uint32_t nCur;
  uint32_t nNew;
  // error if file not open or seek past end of file
  if (!isOpen() || pos > fileSize_) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (type_ == FAT_FILE_TYPE_ROOT_FIXED) {
    curPosition_ = pos;
    goto done;
  }
  if (pos == 0) {
    // set position to start of file
    curCluster_ = 0;
    curPosition_ = 0;
    goto done;
  }
  // calculate cluster index for cur and new position
  nCur = (curPosition_ - 1) >> (vol_->clusterSizeShift_ + 9);
  nNew = (pos - 1) >> (vol_->clusterSizeShift_ + 9);

  if (nNew < nCur || curPosition_ == 0) {
    // must follow chain from first cluster
    curCluster_ = firstCluster_;
  } else {
    // advance from curPosition
    nNew -= nCur;
  }
  while (nNew--) {
    if (!vol_->fatGet(curCluster_, &curCluster_)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  curPosition_ = pos;

 done:
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
// set fileSize_ for a directory
bool SDfile::setDirSize() {
  uint16_t s = 0;
  uint32_t cluster = firstCluster_;
  do {
    if (!vol_->fatGet(cluster, &cluster)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    s += vol_->blocksPerCluster();
    // max size if a directory file is 4096 blocks
    if (s >= 4096) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } while (!vol_->isEOC(cluster));
  fileSize_ = 512L*s;
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
/** The sync() call causes all modified data and directory fields
 * to be written to the storage device.
 */
bool SDfile::sync() {
  // only allow open files and directories
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (flags_ & F_FILE_DIR_DIRTY) {
    dir_t* d = cacheDirEntry(SDvol::CACHE_FOR_WRITE);
    // check for deleted by another open file object
    if (!d || d->name[0] == DIR_NAME_DELETED) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // do not set filesize for dir files
    if (!isDir()) d->fileSize = fileSize_;

    // update first cluster fields
    d->firstClusterLow = firstCluster_ & 0XFFFF;
    d->firstClusterHigh = firstCluster_ >> 16;

    // clear directory dirty
    flags_ &= ~F_FILE_DIR_DIRTY;
  }
  return vol_->cacheSync();

 fail:
  writeError = true;
  return false;
}
