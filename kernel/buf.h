struct buf {
  struct sleeplock lock;
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  uchar data[BSIZE];

  uint refcnt;
  int clkbit;
  struct buf *prev; // LRU cache list
  struct buf *next;
};

