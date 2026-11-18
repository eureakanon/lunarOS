#include "fs.h"
#include "ata.h"
#include "output.h"

static struct fs_superblock sb;
static struct fs_file fds[FS_MAX_FILES];

/* ============================================================
 * 位图
 * ============================================================ */
static int bm_read(int lba, uint8 *buf) { return ata_read_sector(lba, buf); }
static int bm_write(int lba, const uint8 *buf) { return ata_write_sector(lba, buf); }

static int bitmap_get(int lba, int idx) {
    uint8 buf[FS_BLOCK_SIZE];
    if (bm_read(lba, buf) < 0) return 0;
    return (buf[idx >> 3] >> (idx & 7)) & 1;
}
static int bitmap_set(int lba, int idx, int v) {
    uint8 buf[FS_BLOCK_SIZE];
    if (bm_read(lba, buf) < 0) return -1;
    if (v) buf[idx >> 3] |=  (1 << (idx & 7));
    else   buf[idx >> 3] &= ~(1 << (idx & 7));
    return bm_write(lba, buf);
}
static int bitmap_alloc(int lba, int max) {
    for (int i = 0; i < max; i++) {
        if (!bitmap_get(lba, i)) {
            if (bitmap_set(lba, i, 1) < 0) return -1;
            return i;
        }
    }
    return -1;
}

static int inode_bitmap_alloc(void)  { return bitmap_alloc(FS_IBITMAP_LBA, FS_MAX_INODES); }
static void inode_bitmap_free(int i) { bitmap_set(FS_IBITMAP_LBA, i, 0); }
static int  block_bitmap_alloc(void) { return bitmap_alloc(FS_BBITMAP_LBA, FS_MAX_BLOCKS); }
static void block_bitmap_free(int i){ bitmap_set(FS_BBITMAP_LBA, i, 0); }

/* ============================================================
 * inode
 * ============================================================ */
static int inode_read(int idx, struct fs_inode *ino) {
    int per = FS_BLOCK_SIZE / sizeof(struct fs_inode);   /* 8 */
    int sec = FS_INODE_LBA + idx / per;
    int off = (idx % per) * sizeof(struct fs_inode);
    uint8 buf[FS_BLOCK_SIZE];
    if (ata_read_sector(sec, buf) < 0) return -1;
    *ino = *(struct fs_inode *)(buf + off);
    return 0;
}

static int inode_write(int idx, const struct fs_inode *ino) {
    int per = FS_BLOCK_SIZE / sizeof(struct fs_inode);
    int sec = FS_INODE_LBA + idx / per;
    int off = (idx % per) * sizeof(struct fs_inode);
    uint8 buf[FS_BLOCK_SIZE];
    if (ata_read_sector(sec, buf) < 0) return -1;
    *(struct fs_inode *)(buf + off) = *ino;
    return ata_write_sector(sec, buf);
}

/* ============================================================
 * 目录
 * ============================================================ */
static int dirent_read(int idx, struct fs_dirent *de) {
    int per = FS_DIRENT_PER_SECT;
    int sec = FS_DATA_LBA + idx / per;
    int off = (idx % per) * sizeof(struct fs_dirent);
    uint8 buf[FS_BLOCK_SIZE];
    if (ata_read_sector(sec, buf) < 0) return -1;
    *de = *(struct fs_dirent *)(buf + off);
    return 0;
}

static int dirent_write(int idx, const struct fs_dirent *de) {
    int per = FS_DIRENT_PER_SECT;
    int sec = FS_DATA_LBA + idx / per;
    int off = (idx % per) * sizeof(struct fs_dirent);
    uint8 buf[FS_BLOCK_SIZE];
    if (ata_read_sector(sec, buf) < 0) return -1;
    *(struct fs_dirent *)(buf + off) = *de;
    return ata_write_sector(sec, buf);
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static int dirent_find(const char *name) {
    int n = FS_ROOT_DIR_BLOCKS * FS_DIRENT_PER_SECT;
    for (int i = 0; i < n; i++) {
        struct fs_dirent de;
        if (dirent_read(i, &de) < 0) continue;
        if (de.name[0] == 0) continue;
        if (str_eq(de.name, name)) return i;
    }
    return -1;
}

static int dirent_alloc(void) {
    int n = FS_ROOT_DIR_BLOCKS * FS_DIRENT_PER_SECT;
    for (int i = 0; i < n; i++) {
        struct fs_dirent de;
        if (dirent_read(i, &de) < 0) continue;
        if (de.name[0] == 0) return i;
    }
    return -1;
}

/* ============================================================
 * 格式化
 * ============================================================ */
int fs_format(void) {
    uint8 buf[FS_BLOCK_SIZE];

    /* 超级块 */
    for (int i = 0; i < FS_BLOCK_SIZE; i++) buf[i] = 0;
    struct fs_superblock *p = (struct fs_superblock *)buf;
    p->magic      = FS_MAGIC;
    p->ninodes    = FS_MAX_INODES;
    p->nblocks    = FS_MAX_BLOCKS;
    p->dir_blocks = FS_ROOT_DIR_BLOCKS;
    if (ata_write_sector(FS_SB_LBA, buf) < 0) return -1;

    /* inode 位图清零 */
    for (int i = 0; i < FS_BLOCK_SIZE; i++) buf[i] = 0;
    if (ata_write_sector(FS_IBITMAP_LBA, buf) < 0) return -1;

    /* 块位图清零 */
    for (int i = 0; i < FS_BLOCK_SIZE; i++) buf[i] = 0;
    if (ata_write_sector(FS_BBITMAP_LBA, buf) < 0) return -1;

    /* inode 表清零 */
    for (int i = 0; i < FS_INODE_SECTORS; i++) {
        for (int j = 0; j < FS_BLOCK_SIZE; j++) buf[j] = 0;
        if (ata_write_sector(FS_INODE_LBA + i, buf) < 0) return -1;
    }

    /* 根目录数据清零 */
    for (int i = 0; i < FS_ROOT_DIR_BLOCKS; i++) {
        for (int j = 0; j < FS_BLOCK_SIZE; j++) buf[j] = 0;
        if (ata_write_sector(FS_DATA_LBA + i, buf) < 0) return -1;
    }

    /* 标记根目录占用的 4 个块 */
    for (int i = 0; i < FS_ROOT_DIR_BLOCKS; i++)
        bitmap_set(FS_BBITMAP_LBA, i, 1);

    sb.magic      = FS_MAGIC;
    sb.ninodes    = FS_MAX_INODES;
    sb.nblocks    = FS_MAX_BLOCKS;
    sb.dir_blocks = FS_ROOT_DIR_BLOCKS;
    puts("[fs] formatted\n");
    return 0;
}

/* ============================================================
 * 初始化
 * ============================================================ */
void fs_init(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) fds[i].used = 0;

    uint8 buf[FS_BLOCK_SIZE];
    if (ata_read_sector(FS_SB_LBA, buf) < 0) {
        puts("[fs] ATA read failed -> format\n");
        fs_format();
        return;
    }
    struct fs_superblock *p = (struct fs_superblock *)buf;
    if (p->magic != FS_MAGIC) {
        puts("[fs] no filesystem -> format\n");
        fs_format();
        return;
    }
    sb = *p;
    puts("[fs] mounted: ");
    put_dec(sb.ninodes);  puts(" inodes, ");
    put_dec(sb.nblocks);  puts(" blocks\n");
}

/* ============================================================
 * 文件 API
 * ============================================================ */
int fs_create(const char *name) {
    if (dirent_find(name) >= 0) return -1;   /* 已存在 */

    int ino_idx = inode_bitmap_alloc();
    if (ino_idx < 0) return -1;

    struct fs_inode ino;
    uint8 *pi = (uint8 *)&ino;
    for (uint32 i = 0; i < sizeof(ino); i++) pi[i] = 0;
    ino.type    = FS_TYPE_FILE;
    ino.mode    = 0x1FF;
    ino.size    = 0;
    ino.nblocks = 0;
    if (inode_write(ino_idx, &ino) < 0) {
        inode_bitmap_free(ino_idx);
        return -1;
    }

    int de_idx = dirent_alloc();
    if (de_idx < 0) {
        inode_bitmap_free(ino_idx);
        return -1;
    }

    struct fs_dirent de;
    uint8 *pd = (uint8 *)&de;
    for (uint32 i = 0; i < sizeof(de); i++) pd[i] = 0;
    int k;
    for (k = 0; k < FS_MAX_NAME - 1 && name[k]; k++) de.name[k] = name[k];
    de.name[k] = 0;
    de.inode = ino_idx;
    de.type  = FS_TYPE_FILE;
    if (dirent_write(de_idx, &de) < 0) {
        inode_bitmap_free(ino_idx);
        return -1;
    }
    return 0;
}

int fs_unlink(const char *name) {
    int de_idx = dirent_find(name);
    if (de_idx < 0) return -1;

    struct fs_dirent de;
    if (dirent_read(de_idx, &de) < 0) return -1;

    struct fs_inode ino;
    if (inode_read(de.inode, &ino) < 0) return -1;

    /* 释放数据块 */
    for (int i = 0; i < FS_DIRECT_BLOCKS; i++) {
        if (ino.blocks[i] != 0) block_bitmap_free(ino.blocks[i]);
    }
    inode_bitmap_free(de.inode);

    /* 清空目录项 */
    de.name[0] = 0;
    de.inode   = 0;
    de.type    = FS_TYPE_FREE;
    return dirent_write(de_idx, &de);
}

int fs_open(const char *name) {
    int de_idx = dirent_find(name);
    if (de_idx < 0) return -1;

    struct fs_dirent de;
    if (dirent_read(de_idx, &de) < 0) return -1;

    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!fds[i].used) {
            fds[i].used   = 1;
            fds[i].inode  = de.inode;
            fds[i].offset = 0;
            return i;
        }
    }
    return -1;
}

int fs_read(int fd, void *buf, uint32 count) {
    if (fd < 0 || fd >= FS_MAX_FILES || !fds[fd].used) return -1;

    struct fs_inode ino;
    if (inode_read(fds[fd].inode, &ino) < 0) return -1;

    uint32 off = fds[fd].offset;
    if (off >= ino.size) return 0;
    uint32 avail = ino.size - off;
    if (count > avail) count = avail;

    uint8 *dst = (uint8 *)buf;
    uint32 done = 0;
    while (done < count) {
        uint32 bi = off / FS_BLOCK_SIZE;
        uint32 bo = off % FS_BLOCK_SIZE;
        if (bi >= FS_DIRECT_BLOCKS) break;
        if (ino.blocks[bi] == 0) break;

        uint8 tmp[FS_BLOCK_SIZE];
        if (ata_read_sector(FS_DATA_LBA + ino.blocks[bi], tmp) < 0) break;

        uint32 chunk = FS_BLOCK_SIZE - bo;
        if (chunk > count - done) chunk = count - done;
        for (uint32 i = 0; i < chunk; i++) dst[done + i] = tmp[bo + i];

        done += chunk;
        off  += chunk;
    }
    fds[fd].offset = off;
    return (int)done;
}

int fs_write(int fd, const void *buf, uint32 count) {
    if (fd < 0 || fd >= FS_MAX_FILES || !fds[fd].used) return -1;

    struct fs_inode ino;
    if (inode_read(fds[fd].inode, &ino) < 0) return -1;

    uint32 off = fds[fd].offset;
    const uint8 *src = (const uint8 *)buf;
    uint32 done = 0;

    while (done < count) {
        uint32 bi = off / FS_BLOCK_SIZE;
        uint32 bo = off % FS_BLOCK_SIZE;
        if (bi >= FS_DIRECT_BLOCKS) break;

        if (ino.blocks[bi] == 0) {
            int nb = block_bitmap_alloc();
            if (nb < 0) break;
            ino.blocks[bi] = (uint32)nb;
            ino.nblocks++;
        }

        uint8 tmp[FS_BLOCK_SIZE];
        if (ata_read_sector(FS_DATA_LBA + ino.blocks[bi], tmp) < 0) break;

        uint32 chunk = FS_BLOCK_SIZE - bo;
        if (chunk > count - done) chunk = count - done;
        for (uint32 i = 0; i < chunk; i++) tmp[bo + i] = src[done + i];

        if (ata_write_sector(FS_DATA_LBA + ino.blocks[bi], tmp) < 0) break;

        done += chunk;
        off  += chunk;
    }

    if (off > ino.size) ino.size = off;
    fds[fd].offset = off;
    inode_write(fds[fd].inode, &ino);
    return (int)done;
}

int fs_close(int fd) {
    if (fd < 0 || fd >= FS_MAX_FILES || !fds[fd].used) return -1;
    fds[fd].used = 0;
    return 0;
}

void fs_list(void) {
    int n = FS_ROOT_DIR_BLOCKS * FS_DIRENT_PER_SECT;
    puts("[fs] root:\n");
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        struct fs_dirent de;
        if (dirent_read(i, &de) < 0) continue;
        if (de.name[0] == 0) continue;
        if (de.type == FS_TYPE_FREE) continue;

        puts("  "); puts(de.name);
        if (de.type == FS_TYPE_FILE) {
            struct fs_inode ino;
            if (inode_read(de.inode, &ino) == 0) {
                puts("  size="); put_dec(ino.size);
                puts("  blocks="); put_dec(ino.nblocks);
            }
        }
        putchar('\n');
        cnt++;
    }
    if (cnt == 0) puts("  (empty)\n");
}
