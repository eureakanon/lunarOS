#include "fs.h"
#include "ata.h"
#include "output.h"
#include "proc.h"
#include "klibc.h"

/* ============================================================
 * 全局状态
 * ============================================================ */
static struct fs_superblock sb;
static struct ofile ofile_table[OFILE_TABLE_SIZE];

/* ============================================================
 * 字符串工具
 * ============================================================ */
static int str_eq(const char *a, const char *b)
{
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static void str_copy(char *dst, const char *src, int max)
{
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/* ============================================================
 * 位图操作
 * ============================================================ */
static int bitmap_get(int lba, int idx)
{
    uint8 buf[FS_BLOCK_SIZE];
    if (ata_read_sector(lba, buf) < 0) return 0;
    return (buf[idx >> 3] >> (idx & 7)) & 1;
}

static int bitmap_set(int lba, int idx, int v)
{
    uint8 buf[FS_BLOCK_SIZE];
    if (ata_read_sector(lba, buf) < 0) return -1;
    if (v) buf[idx >> 3] |=  (1 << (idx & 7));
    else   buf[idx >> 3] &= ~(1 << (idx & 7));
    return ata_write_sector(lba, buf);
}

static int bitmap_alloc(int lba, int max)
{
    for (int i = 0; i < max; i++) {
        if (!bitmap_get(lba, i)) {
            if (bitmap_set(lba, i, 1) < 0) return -1;
            return i;
        }
    }
    return -1;
}

static int  inode_bitmap_alloc(void)  { return bitmap_alloc(FS_IBITMAP_LBA, FS_MAX_INODES); }
static void inode_bitmap_free(int i)  { bitmap_set(FS_IBITMAP_LBA, i, 0); }
static int  block_bitmap_alloc(void)  { return bitmap_alloc(FS_BBITMAP_LBA, FS_MAX_BLOCKS); }
static void block_bitmap_free(int i)  { bitmap_set(FS_BBITMAP_LBA, i, 0); }

/* ============================================================
 * inode 读写
 * ============================================================ */
static int inode_read(int idx, struct fs_inode *ino)
{
    int per = FS_BLOCK_SIZE / sizeof(struct fs_inode);   /* 8 */
    int sec = FS_INODE_LBA + idx / per;
    int off = (idx % per) * sizeof(struct fs_inode);

    uint8 buf[FS_BLOCK_SIZE];
    if (ata_read_sector(sec, buf) < 0) return -1;
    memcpy(ino, buf + off, sizeof(struct fs_inode));
    return 0;
}

static int inode_write(int idx, const struct fs_inode *ino)
{
    int per = FS_BLOCK_SIZE / sizeof(struct fs_inode);
    int sec = FS_INODE_LBA + idx / per;
    int off = (idx % per) * sizeof(struct fs_inode);

    uint8 buf[FS_BLOCK_SIZE];
    if (ata_read_sector(sec, buf) < 0) return -1;
    memcpy(buf + off, ino, sizeof(struct fs_inode));
    return ata_write_sector(sec, buf);
}

/* ============================================================
 * 目录操作
 * ============================================================ */

/* 在目录 dir_ino 的数据块里，按名字查找 */
static int dir_lookup(int dir_ino, const char *name)
{
    struct fs_inode dir;
    if (inode_read(dir_ino, &dir) < 0) return -1;
    if (dir.type != FS_TYPE_DIR) return -1;

    int per = FS_BLOCK_SIZE / sizeof(struct fs_dirent);
    for (int b = 0; b < FS_DIRECT_BLOCKS; b++) {
        if (dir.blocks[b] == 0) continue;

        uint8 buf[FS_BLOCK_SIZE];
        if (ata_read_sector(FS_DATA_LBA + dir.blocks[b], buf) < 0) return -1;

        struct fs_dirent *de = (struct fs_dirent *)buf;
        for (int i = 0; i < per; i++) {
            if (de[i].name[0] == 0) continue;
            if (str_eq(de[i].name, name)) return de[i].inode;
        }
    }
    return -1;
}

/* 绝对路径 → inode 号 */
int namei(const char *path)
{
    if (!path || path[0] != '/') return -1;

    int cur = FS_ROOT_INODE;
    const char *p = path;

    while (*p) {
        while (*p == '/') p++;           /* 跳过连续 '/' */
        if (!*p) return cur;             /* 路径结束 */

        char comp[FS_MAX_NAME];
        int n = 0;
        while (*p && *p != '/' && n < FS_MAX_NAME - 1)
            comp[n++] = *p++;
        comp[n] = 0;

        cur = dir_lookup(cur, comp);
        if (cur < 0) return -1;
    }
    return cur;
}

/* 把 "/a/b/c" 拆成 parent="/a/b", name="c" */
static int split_path(const char *path, char *parent, const char **name)
{
    if (!path || path[0] != '/') return -1;

    int last = -1;
    for (int i = 0; path[i]; i++)
        if (path[i] == '/') last = i;

    if (last < 0) return -1;

    if (last == 0) {
        parent[0] = '/';
        parent[1] = 0;
    } else {
        for (int i = 0; i < last; i++) parent[i] = path[i];
        parent[last] = 0;
    }

    *name = path + last + 1;
    if (!**name) return -1;
    return 0;
}

/* ============================================================
 * 打开文件表
 * ============================================================ */
static struct ofile *ofile_alloc(int inode, int rd, int wr)
{
    for (int i = 0; i < OFILE_TABLE_SIZE; i++) {
        if (!ofile_table[i].used) {
            ofile_table[i].used     = 1;
            ofile_table[i].ref      = 1;
            ofile_table[i].inode    = inode;
            ofile_table[i].offset   = 0;
            ofile_table[i].readable = rd;
            ofile_table[i].writable = wr;
            return &ofile_table[i];
        }
    }
    return 0;
}

static void ofile_close(struct ofile *f)
{
    if (!f || !f->used) return;
    if (--f->ref == 0) f->used = 0;
}

/* ============================================================
 * 格式化 / 初始化
 * ============================================================ */
int fs_format(void)
{
    uint8 buf[FS_BLOCK_SIZE];

    /* 1. superblock */
    memset(buf, 0, FS_BLOCK_SIZE);
    struct fs_superblock *p = (struct fs_superblock *)buf;
    p->magic      = FS_MAGIC;
    p->ninodes    = FS_MAX_INODES;
    p->nblocks    = FS_MAX_BLOCKS;
    p->dir_blocks = FS_ROOT_DIR_BLOCKS;
    if (ata_write_sector(FS_SB_LBA, buf) < 0) return -1;

    /* 2. 两个位图清零 */
    memset(buf, 0, FS_BLOCK_SIZE);
    if (ata_write_sector(FS_IBITMAP_LBA, buf) < 0) return -1;
    memset(buf, 0, FS_BLOCK_SIZE);
    if (ata_write_sector(FS_BBITMAP_LBA, buf) < 0) return -1;

    /* 3. inode 表清零 */
    memset(buf, 0, FS_BLOCK_SIZE);
    for (int i = 0; i < FS_INODE_SECTORS; i++)
        if (ata_write_sector(FS_INODE_LBA + i, buf) < 0) return -1;

    /* 4. 根目录数据块清零 */
    memset(buf, 0, FS_BLOCK_SIZE);
    for (int i = 0; i < FS_ROOT_DIR_BLOCKS; i++)
        if (ata_write_sector(FS_DATA_LBA + i, buf) < 0) return -1;

    /* 5. 建根目录 inode（inode 0） */
    struct fs_inode root;
    memset(&root, 0, sizeof(root));
    root.type    = FS_TYPE_DIR;
    root.mode    = 0x1FF;
    root.nblocks = FS_ROOT_DIR_BLOCKS;
    for (int i = 0; i < FS_ROOT_DIR_BLOCKS; i++) {
        root.blocks[i] = (uint32)i;               /* 数据块 0..3 给根 */
        bitmap_set(FS_BBITMAP_LBA, i, 1);         /* 标记占用 */
    }
    inode_write(FS_ROOT_INODE, &root);
    bitmap_set(FS_IBITMAP_LBA, FS_ROOT_INODE, 1); /* 标记 inode 0 占用 */

    sb.magic      = FS_MAGIC;
    sb.ninodes    = FS_MAX_INODES;
    sb.nblocks    = FS_MAX_BLOCKS;
    sb.dir_blocks = FS_ROOT_DIR_BLOCKS;

    puts("[fs] formatted\n");
    return 0;
}

void fs_init(void)
{
    for (int i = 0; i < OFILE_TABLE_SIZE; i++)
        ofile_table[i].used = 0;

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
    put_dec(sb.ninodes); puts(" inodes, ");
    put_dec(sb.nblocks); puts(" blocks\n");
}

/* ============================================================
 * 文件创建 / 删除
 * ============================================================ */
int fs_create(const char *path, int type)
{
    char parent[128];
    const char *name;
    if (split_path(path, parent, &name) < 0) return -1;

    int pino = namei(parent);
    if (pino < 0) return -1;

    struct fs_inode pdir;
    if (inode_read(pino, &pdir) < 0) return -1;
    if (pdir.type != FS_TYPE_DIR) return -1;

    if (dir_lookup(pino, name) >= 0) return -1;   /* 已存在 */

    /* 分配新 inode */
    int new_ino = inode_bitmap_alloc();
    if (new_ino < 0) return -1;

    struct fs_inode ni;
    memset(&ni, 0, sizeof(ni));
    ni.type = (uint16)type;
    ni.mode = 0x1FF;
    if (inode_write(new_ino, &ni) < 0) {
        inode_bitmap_free(new_ino);
        return -1;
    }

    /* 在父目录里找空 dirent 槽 */
    int per = FS_BLOCK_SIZE / sizeof(struct fs_dirent);
    for (int b = 0; b < FS_DIRECT_BLOCKS; b++) {
        int need_alloc = 0;
        if (pdir.blocks[b] == 0) {
            int nb = block_bitmap_alloc();
            if (nb < 0) { inode_bitmap_free(new_ino); return -1; }
            pdir.blocks[b] = (uint32)nb;
            pdir.nblocks++;
            inode_write(pino, &pdir);
            need_alloc = 1;
        }

        uint8 buf[FS_BLOCK_SIZE];
        if (ata_read_sector(FS_DATA_LBA + pdir.blocks[b], buf) < 0) {
            inode_bitmap_free(new_ino);
            return -1;
        }
        if (need_alloc) memset(buf, 0, FS_BLOCK_SIZE);

        struct fs_dirent *de = (struct fs_dirent *)buf;
        for (int i = 0; i < per; i++) {
            if (de[i].name[0] != 0) continue;

            str_copy(de[i].name, name, FS_MAX_NAME);
            de[i].inode = (uint16)new_ino;
            de[i].type  = (uint16)type;

            if (ata_write_sector(FS_DATA_LBA + pdir.blocks[b], buf) < 0) {
                inode_bitmap_free(new_ino);
                return -1;
            }
            return 0;
        }
    }
    inode_bitmap_free(new_ino);
    return -1;   /* 父目录满了 */
}

int fs_unlink(const char *path)
{
    char parent[128];
    const char *name;
    if (split_path(path, parent, &name) < 0) return -1;

    int pino = namei(parent);
    if (pino < 0) return -1;

    struct fs_inode pdir;
    if (inode_read(pino, &pdir) < 0) return -1;
    if (pdir.type != FS_TYPE_DIR) return -1;

    int per = FS_BLOCK_SIZE / sizeof(struct fs_dirent);
    for (int b = 0; b < FS_DIRECT_BLOCKS; b++) {
        if (pdir.blocks[b] == 0) continue;

        uint8 buf[FS_BLOCK_SIZE];
        if (ata_read_sector(FS_DATA_LBA + pdir.blocks[b], buf) < 0) return -1;
        struct fs_dirent *de = (struct fs_dirent *)buf;

        for (int i = 0; i < per; i++) {
            if (de[i].name[0] == 0) continue;
            if (!str_eq(de[i].name, name)) continue;

            /* 释放目标 inode 的数据块 */
            struct fs_inode t;
            if (inode_read(de[i].inode, &t) == 0) {
                for (int k = 0; k < FS_DIRECT_BLOCKS; k++)
                    if (t.blocks[k]) block_bitmap_free(t.blocks[k]);
            }
            inode_bitmap_free(de[i].inode);

            de[i].name[0] = 0;
            de[i].inode   = 0;
            de[i].type    = 0;
            return ata_write_sector(FS_DATA_LBA + pdir.blocks[b], buf);
        }
    }
    return -1;
}

/* ============================================================
 * 文件打开 / 读写 / 关闭（走 proc->ofile）
 * ============================================================ */
int fs_open(const char *path)
{
    int ino = namei(path);
    if (ino < 0) return -1;

    struct fs_inode i;
    if (inode_read(ino, &i) < 0) return -1;
    if (i.type != FS_TYPE_FILE) return -1;   /* 目录暂时不能 open */

    struct ofile *f = ofile_alloc(ino, 1, 1);
    if (!f) return -1;

    struct proc *p = myproc();
    if (!p) { ofile_close(f); return -1; }

    /* ★ fd 0/1/2 保留给 stdin/stdout/stderr，从 3 开始分配 */
    for (int fd = 3; fd < NOFILE; fd++) {
        if (!p->ofile[fd]) {
            p->ofile[fd] = f;
            return fd;
        }
    }
    ofile_close(f);
    return -1;
}

int fs_read(int fd, void *buf, uint32 count)
{
    struct proc *p = myproc();
    if (!p || fd < 0 || fd >= NOFILE) return -1;
    struct ofile *f = p->ofile[fd];
    if (!f || !f->readable) return -1;

    struct fs_inode ino;
    if (inode_read(f->inode, &ino) < 0) return -1;

    if (f->offset >= ino.size) return 0;
    uint32 avail = ino.size - f->offset;
    if (count > avail) count = avail;

    uint8 *dst = (uint8 *)buf;
    uint32 off = f->offset, done = 0;
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
    f->offset = off;
    return (int)done;
}

int fs_write(int fd, const void *buf, uint32 count)
{
    struct proc *p = myproc();
    if (!p || fd < 0 || fd >= NOFILE) return -1;
    struct ofile *f = p->ofile[fd];
    if (!f || !f->writable) return -1;

    struct fs_inode ino;
    if (inode_read(f->inode, &ino) < 0) return -1;

    const uint8 *src = (const uint8 *)buf;
    uint32 off = f->offset, done = 0;
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
    f->offset = off;
    inode_write(f->inode, &ino);
    return (int)done;
}

int fs_close(int fd)
{
    struct proc *p = myproc();
    if (!p || fd < 0 || fd >= NOFILE) return -1;
    if (!p->ofile[fd]) return -1;
    ofile_close(p->ofile[fd]);
    p->ofile[fd] = 0;
    return 0;
}

/* ============================================================
 * 给 fork / exec 用
 * ============================================================ */
void fs_dup_ofile(struct ofile **dst, struct ofile **src)
{
    for (int i = 0; i < NOFILE; i++) {
        dst[i] = src[i];
        if (src[i]) src[i]->ref++;
    }
}

void fs_close_all(struct ofile **ofile)
{
    for (int i = 0; i < NOFILE; i++) {
        if (ofile[i]) {
            ofile_close(ofile[i]);
            ofile[i] = 0;
        }
    }
}

/* ============================================================
 * 列出根目录
 * ============================================================ */
void fs_list(void)
{
    puts("[fs] root:\n");

    struct fs_inode root;
    if (inode_read(FS_ROOT_INODE, &root) < 0) {
        puts("  (cannot read root inode)\n");
        return;
    }

    int per = FS_BLOCK_SIZE / sizeof(struct fs_dirent);
    int cnt = 0;
    for (int b = 0; b < FS_DIRECT_BLOCKS; b++) {
        if (root.blocks[b] == 0) continue;

        uint8 buf[FS_BLOCK_SIZE];
        if (ata_read_sector(FS_DATA_LBA + root.blocks[b], buf) < 0) continue;

        struct fs_dirent *de = (struct fs_dirent *)buf;
        for (int i = 0; i < per; i++) {
            if (de[i].name[0] == 0) continue;

            puts("  /"); puts(de[i].name);
            if (de[i].type == FS_TYPE_DIR) {
                puts("/");
            } else {
                struct fs_inode ino;
                if (inode_read(de[i].inode, &ino) == 0) {
                    puts("  size=");   put_dec(ino.size);
                    puts("  blocks="); put_dec(ino.nblocks);
                }
            }
            putchar('\n');
            cnt++;
        }
    }
    if (cnt == 0) puts("  (empty)\n");
}