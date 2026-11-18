#ifndef _FS_H
#define _FS_H
#include"types.h"

#define FS_BLOCK_SIZE 512 /*一个块512B*/
#define FS_MAX_INODES 64/*维护64个inode*/
#define FS_MAX_BLOCKS 4096 /*总共4096个block*/
#define FS_MAX_NAME 24 /*名字最长24*/
#define FS_MAX_FILES 16/*最多16个文件*/
#define FS_DIRECT_BLOCKS 13 /*总共8个直接块*/

#define FS_MAGIC           0x5A5A1234

/* 磁盘布局 */
#define FS_SB_LBA          100
#define FS_IBITMAP_LBA     101
#define FS_BBITMAP_LBA     102
#define FS_INODE_LBA       103
#define FS_INODE_SECTORS   8       /* 8 × 512 / 64 = 64 inode */
#define FS_DATA_LBA        200     /* 数据块 0 从这里开始 */

#define FS_ROOT_DIR_BLOCKS 4
#define FS_DIRENT_PER_SECT (FS_BLOCK_SIZE / sizeof(struct fs_dirent))

#define FS_TYPE_FREE 0
#define FS_TYPE_FILE 1

struct fs_superblock{
    uint32 magic;
    uint32 ninodes;
    uint32 nblocks;       /* 数据块总数，这里是 4096 */
    uint32 dir_blocks;
};

struct fs_inode
{
    uint16 type;
    uint16 mode;
    uint32 size;
    uint32 nblocks;
    uint32 blocks[FS_DIRECT_BLOCKS];
}__attribute__((packed));

struct fs_dirent {
    char   name[FS_MAX_NAME];  /* 空字符串 = 空槽 */
    uint16 inode;
    uint16 type;
    uint32 reserved;
} __attribute__((packed));

struct fs_file {
    int    inode;
    uint32 offset;
    int    used;
};

void fs_init(void);
int  fs_format(void);

int  fs_create(const char *name);
int  fs_unlink(const char *name);
int  fs_open(const char *name);
int  fs_read(int fd, void *buf, uint32 count);
int  fs_write(int fd, const void *buf, uint32 count);
int  fs_close(int fd);
void fs_list(void);

#endif
