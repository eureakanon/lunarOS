#ifndef _FS_H
#define _FS_H

#include "types.h"

/* ============================================================
 * 基本参数
 * ============================================================ */
#define FS_BLOCK_SIZE      512
#define FS_MAX_INODES      64
#define FS_MAX_BLOCKS      4096
#define FS_MAX_NAME        24
#define FS_DIRECT_BLOCKS   26

#define NOFILE             16      /* 每进程最多打开的 fd 数（含 0/1/2） */
#define OFILE_TABLE_SIZE   64      /* 全局打开文件表大小 */

#define FS_TYPE_FREE       0
#define FS_TYPE_FILE       1
#define FS_TYPE_DIR        2

#define FS_MAGIC           0x5A5A1235

/* ============================================================
 * 磁盘布局
 * ============================================================ */
#define FS_SB_LBA          100
#define FS_IBITMAP_LBA     101
#define FS_BBITMAP_LBA     102
#define FS_INODE_LBA       103
#define FS_INODE_SECTORS   8
#define FS_DATA_LBA        200
#define FS_ROOT_INODE      0
#define FS_ROOT_DIR_BLOCKS 4

/* ============================================================
 * 磁盘结构
 * ============================================================ */
struct fs_superblock {
    uint32 magic;
    uint32 ninodes;
    uint32 nblocks;
    uint32 dir_blocks;
};

struct fs_inode {
    uint16 type;                       /* FS_TYPE_FILE / FS_TYPE_DIR */
    uint16 mode;
    uint32 size;
    uint32 nblocks;
    uint32 blocks[FS_DIRECT_BLOCKS];   /* 数据块号 */
    uint32 reserved[3];                /* 凑 64 字节 */
} __attribute__((packed));

struct fs_dirent {
    char   name[FS_MAX_NAME];
    uint16 inode;
    uint16 type;
    uint32 reserved;
} __attribute__((packed));

/* ============================================================
 * 打开文件表（全局）
 * ============================================================ */
struct ofile {
    int    used;
    int    ref;          /* fork 时 ++，close 时 --；归零才真正释放 */
    int    inode;
    uint32 offset;
    int    readable, writable;
};

/* ============================================================
 * API
 * ============================================================ */
void fs_init(void);
int  fs_format(void);

/* 路径接口 */
int  namei(const char *path);                        /* 路径 → inode 号 */
int  fs_create(const char *path, int type);          /* type = FS_TYPE_FILE/DIR */
int  fs_unlink(const char *path);
int  fs_open(const char *path);                      /* 返回 fd（>=3） */
int  fs_read(int fd, void *buf, uint32 count);
int  fs_write(int fd, const void *buf, uint32 count);
int  fs_close(int fd);
void fs_list(void);

/* 给 fork / exec 用 */
void fs_dup_ofile(struct ofile **dst, struct ofile **src);
void fs_close_all(struct ofile **ofile);

#endif
