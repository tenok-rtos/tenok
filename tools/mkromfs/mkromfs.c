/* Resolve confliction between linux/limits.h and kconfig.h */
#define _LINUX_LIMITS_H

#include <dirent.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "kconfig.h"
#define HOST_INPUT_DIR "../../rom/"
#define ROMFS_OUTPUT_DIR "/rom_data/"
#define OUTPUT_BIN "./romfs.bin"

#define NAME_MAX _NAME_MAX

/* The mode is written into the image, so these are the values Tenok reads */
#undef S_IFREG
#undef S_IFDIR
#define S_IFREG 0100000 /* Regular file */
#define S_IFDIR 0040000 /* Directory */

bool _verbose = false;
uint32_t _build_time;

struct super_block {
    bool s_rd_only;       /* Read-only flag */
    uint32_t s_blk_cnt;   /* number of the used blocks */
    uint32_t s_inode_cnt; /* number of the used inodes */
    uint64_t s_sb_addr;   /* Start address of the super block */
    uint64_t s_ino_addr;  /* Start address of the inode table */
    uint64_t s_blk_addr;  /* Start address of the blocks region */
} __attribute__((aligned(4)));

/* Block header will be placed to the top of every blocks of the regular file */
struct block_header {
    uint32_t b_next; /* Virtual address of the next block */
    uint32_t reserved;
} __attribute__((aligned(4)));

struct list_head {
    uint32_t next;
    uint32_t prev;
} __attribute__((aligned(4)));

/* index node */
struct inode {
    uint16_t i_mode;   /* File type and permission bits */
    uint8_t i_rdev;    /* The device on which this file system is mounted */
    bool i_sync;       /* The mounted file is loaded into the rootfs or not */
    uint32_t i_ino;    /* inode number */
    uint32_t i_parent; /* inode number of the parent directory */
    uint32_t i_fd;     /* File descriptor number */
    uint32_t i_size;   /* File size (bytes) */
    uint32_t i_blocks; /* Block_numbers = file_size / block_size */
    uint32_t i_data;   /* Virtual address for accessing the storage */
    struct list_head i_dentry; /* List head of the dentry table */
    uint32_t i_mtime;          /* Seconds since the epoch, last written */
    uint32_t reserved[2];
} __attribute__((aligned(4)));

/* Directory entry */
struct dentry {
    char d_name[NAME_MAX];   /* File name */
    uint32_t d_inode;        /* The inode of the file */
    uint32_t d_parent;       /* The inode of the parent directory */
    struct list_head d_list; /* List head of the dentry */
    uint32_t reserved[2];
} __attribute__((aligned(4)));

struct super_block romfs_sb;
struct inode inodes[INODE_MAX];
uint8_t romfs_blk[FS_BLK_CNT][FS_BLK_SIZE];

static inline uint32_t romfs_sb_size(void)
{
    return sizeof(struct super_block);
}

static inline uint32_t romfs_inodes_size(void)
{
    return sizeof(inodes);
}

static inline uint32_t romfs_ptr_to_off(const void *ptr)
{
    uintptr_t p = (uintptr_t) ptr;
    uintptr_t inodes_start = (uintptr_t) inodes;
    uintptr_t inodes_end = inodes_start + sizeof(inodes);
    uintptr_t blks_start = (uintptr_t) romfs_blk;
    uintptr_t blks_end = blks_start + sizeof(romfs_blk);

    if (p >= inodes_start && p < inodes_end) {
        return romfs_sb_size() + (uint32_t) (p - inodes_start);
    }

    if (p >= blks_start && p < blks_end) {
        return romfs_sb_size() + romfs_inodes_size() +
               (uint32_t) (p - blks_start);
    }

    return 0;
}

static inline void *romfs_off_to_ptr(uint32_t off)
{
    uint32_t sb_size = romfs_sb_size();
    uint32_t ino_size = romfs_inodes_size();

    if (off < sb_size + ino_size) {
        return (uint8_t *) inodes + (off - sb_size);
    }

    return (uint8_t *) romfs_blk + (off - sb_size - ino_size);
}

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    uint32_t off = romfs_ptr_to_off(list);
    list->next = off;
    list->prev = off;
}

static inline int list_empty(struct list_head *head)
{
    return head->next == romfs_ptr_to_off(head);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    uint32_t head_off = romfs_ptr_to_off(head);
    uint32_t prev_off = head->prev;
    struct list_head *prev = (struct list_head *) romfs_off_to_ptr(prev_off);

    new->next = head_off;
    new->prev = prev_off;
    prev->next = romfs_ptr_to_off(new);
    head->prev = romfs_ptr_to_off(new);
}

#define list_entry(ptr, type, member) \
    ((type *) ((uint8_t *) (ptr) -offsetof(type, member)))

#define list_for_each(pos, head)                                         \
    for (uint32_t __off = (head)->next;                                  \
         (pos = (struct list_head *) romfs_off_to_ptr(__off)) != (head); \
         __off = (pos)->next)

int verbose(const char *restrict format, ...)
{
    if (!_verbose)
        return 0;

    va_list args;

    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);

    return ret;
}

void romfs_init(void)
{
    /* Configure the root directory inode */
    struct inode *inode_root = &inodes[0];
    inode_root->i_mode = S_IFDIR | 0755;
    inode_root->i_mtime = _build_time;
    inode_root->i_ino = 0;
    inode_root->i_size = 0;
    inode_root->i_blocks = 0;
    inode_root->i_data = 0;
    INIT_LIST_HEAD(&inode_root->i_dentry);

    romfs_sb.s_inode_cnt = 1;
    romfs_sb.s_blk_cnt = 0;
    romfs_sb.s_rd_only = true;
    romfs_sb.s_sb_addr = 0;
    romfs_sb.s_ino_addr = romfs_sb_size();
    romfs_sb.s_blk_addr = romfs_sb.s_ino_addr + romfs_inodes_size();
}

struct inode *fs_search_file(struct inode *inode_dir, char *file_name)
{
    /* Currently the dentry table is empty */
    if (inode_dir->i_size == 0)
        return NULL;

    /* Traverse the dentry list */
    struct list_head *list_curr;
    list_for_each (list_curr, &inode_dir->i_dentry) {
        struct dentry *dentry = list_entry(list_curr, struct dentry, d_list);

        /* Compare the file name with the dentry */
        if (strcmp(dentry->d_name, file_name) == 0)
            return &inodes[dentry->d_inode];
    }

    return NULL;
}

int fs_calculate_dentry_blocks(size_t block_size, size_t dentry_cnt)
{
    /* Calculate how many dentries a block can hold */
    int dentry_per_blk = block_size / sizeof(struct dentry);

    /* Calculate how many blocks is required for storing N dentries */
    int blocks = dentry_cnt / dentry_per_blk;
    if (dentry_cnt % dentry_per_blk)
        blocks++;

    return blocks;
}

struct inode *fs_add_file(struct inode *inode_dir,
                          char *file_name,
                          uint16_t file_type)
{
    /* inodes numbers is full */
    if (romfs_sb.s_inode_cnt >= INODE_MAX)
        return NULL;

    /* Calculate how many dentries a block can hold */
    int dentry_per_blk = FS_BLK_SIZE / sizeof(struct dentry);

    /* Calculate how many dentries the directory has */
    int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);

    /* Check if current block can fit a new dentry */
    bool fit =
        ((dentry_cnt + 1) <= (inode_dir->i_blocks * dentry_per_blk)) &&
        (inode_dir->i_size != 0) /* No memory is allocated if size = 0 */;

    /* Allocate new dentry */
    uint8_t *dir_data_p;
    if (fit == true) {
        /* Append at the end of the old block */
        struct list_head *list_end =
            (struct list_head *) romfs_off_to_ptr(inode_dir->i_dentry.prev);
        struct dentry *dir = list_entry(list_end, struct dentry, d_list);
        dir_data_p = (uint8_t *) dir + sizeof(struct dentry);
    } else {
        /* The dentry requires a new block */
        if (romfs_sb.s_blk_cnt >= FS_BLK_CNT)
            return NULL;
        dir_data_p = (uint8_t *) romfs_blk + (romfs_sb.s_blk_cnt * FS_BLK_SIZE);

        romfs_sb.s_blk_cnt++;
    }

    /* Configure the new dentry */
    struct dentry *new_dentry = (struct dentry *) dir_data_p;
    new_dentry->d_inode = romfs_sb.s_inode_cnt;           /* File inode */
    new_dentry->d_parent = inode_dir->i_ino;              /* Parent inode */
    strncpy(new_dentry->d_name, file_name, NAME_MAX - 1); /* File name */
    new_dentry->d_name[NAME_MAX - 1] = '\0';

    /* Configure the new file inode */
    struct inode *new_inode = &inodes[romfs_sb.s_inode_cnt];
    new_inode->i_ino = romfs_sb.s_inode_cnt;
    new_inode->i_parent = inode_dir->i_ino;
    new_inode->i_fd = 0;
    new_inode->i_mtime = _build_time;

    /* File instantiation */
    switch (file_type) {
    case S_IFREG: {
        new_inode->i_mode = S_IFREG | 0644;
        new_inode->i_size = 0;
        new_inode->i_blocks = 0;
        new_inode->i_data = 0; /* Empty file */

        break;
    }
    case S_IFDIR: {
        new_inode->i_mode = S_IFDIR | 0755;
        new_inode->i_size = 0;
        new_inode->i_blocks = 0;
        new_inode->i_data = 0; /* Empty directory */
        INIT_LIST_HEAD(&new_inode->i_dentry);

        break;
    }
    default:
        exit(-1);
    }

    /* Update inode count */
    romfs_sb.s_inode_cnt++;

    /* Currently no files is under the directory */
    if (list_empty(&inode_dir->i_dentry) == true) {
        /* Add the first dentry */
        inode_dir->i_data = romfs_ptr_to_off(new_dentry);
    }

    /* Insert the new file under the current directory */
    list_add_tail(&new_dentry->d_list, &inode_dir->i_dentry);

    /* Update inode size and block information */
    inode_dir->i_size += sizeof(struct dentry);

    dentry_cnt = inode_dir->i_size / sizeof(struct dentry);
    inode_dir->i_blocks = fs_calculate_dentry_blocks(FS_BLK_SIZE, dentry_cnt);

    return new_inode;
}

char *fs_split_path(char *entry, char *path)
{
    while (1) {
        bool found_dir = (*path == '/');

        /* Copy */
        if (found_dir == false) {
            *entry = *path;
            entry++;
        }

        path++;

        if ((found_dir == true) || (*path == '\0'))
            break;
    }

    *entry = '\0';

    /* The path can not be splitted anymore */
    if (*path == '\0')
        return NULL;

    /* Return the address of the left path string */
    return path;
}

static struct inode *fs_create_file(char *pathname, uint16_t file_type)
{
    /* The path name must start with '/' */
    if (pathname[0] != '/')
        return NULL;

    /* Iterate from the root inode */
    struct inode *inode_curr = &inodes[0];
    struct inode *inode;

    char file_name[NAME_MAX];
    char entry[NAME_MAX];
    char *path = pathname;

    /* Get rid of the first '/' */
    path = fs_split_path(entry, path);

    while (1) {
        /* Split the path and get the entry name of each layer */
        path = fs_split_path(entry, path);

        /* Two successive '/' are detected */
        if (entry[0] == '\0')
            continue;

        /* The last non-empty entry string is the file name */
        if (entry[0] != '\0') {
            strncpy(file_name, entry, NAME_MAX - 1);
            file_name[NAME_MAX - 1] = '\0';
        }

        /* Search the entry and get the inode */
        inode = fs_search_file(inode_curr, entry);

        if (path != NULL) {
            /* The path can be further splitted, which means it is a directory
             */

            /* Check if the directory exists */
            if (inode == NULL) {
                /* Directory does not exist, create one */
                inode = fs_add_file(inode_curr, entry, S_IFDIR);

                /* Failed to create the directory */
                if (inode == NULL)
                    return NULL;
            }

            inode_curr = inode;
        } else {
            /* No more path to be splitted, the remained string should be the
             * file name */

            /* File with the same name already exists */
            if (inode != NULL)
                return NULL;

            /* Create new inode for the file */
            inode = fs_add_file(inode_curr, file_name, file_type);

            /* Failed to create the file */
            if (inode == NULL)
                return NULL;

            /* File is created successfully */
            return inode;
        }
    }
}

void romfs_export(void)
{
    FILE *file = fopen(OUTPUT_BIN, "wb");

    uint32_t sb_size = sizeof(romfs_sb);
    uint32_t inodes_size = sizeof(inodes);
    uint32_t blocks_size = sizeof(romfs_blk);

    printf(
        "[romfs generation report]\n"
        "super block size: %d bytes\n"
        "inodes size: %d bytes\n"
        "blocks size: %d bytes\n"
        "inode count: %d\n"
        "block count: %d\n",
        sb_size, inodes_size, blocks_size, romfs_sb.s_inode_cnt,
        romfs_sb.s_blk_cnt);

    fwrite((uint8_t *) &romfs_sb, sizeof(uint8_t), sb_size, file);
    fwrite((uint8_t *) &inodes, sizeof(uint8_t), inodes_size, file);
    fwrite((uint8_t *) &romfs_blk, sizeof(uint8_t), blocks_size, file);

    fclose(file);
}

void romfs_import_file(char *host_path, char *romfs_path)
{
    /* Create new romfs file */
    struct inode *inode = fs_create_file(romfs_path, S_IFREG);
    if (inode == NULL) {
        printf("[mkromfs] failed to create new file!\n");
        exit(-1);
    }

    /* Open the source file */
    FILE *file = fopen(host_path, "r");
    if (file == NULL) {
        printf("%s: failed to open the file!\n", host_path);
        exit(-1);
    }

    /* Get the source file size */
    fseek(file, 0, SEEK_END);  // get the file size
    long file_size = ftell(file);

    /* Nothing to write to the romfs file */
    if (file_size == 0) {
        fclose(file);
        return;
    }

    /* Check if the file is too big */
    int left_space = FS_BLK_SIZE * (FS_BLK_CNT - romfs_sb.s_blk_cnt);
    if (file_size > left_space) {
        printf("%s: the space is not enough to fit the file!\n", host_path);
        exit(1);
    }

    /* Read and close the source file */
    char *file_content = malloc(file_size + 1); /* +1 for the EOF symbol */
    fseek(file, 0L, SEEK_SET);
    fread(file_content, sizeof(char), file_size, file);
    fclose(file);

    /* Calculate the required blocks number */
    uint32_t blk_head_size = sizeof(struct block_header);
    uint32_t blk_free_size = FS_BLK_SIZE - blk_head_size;
    int blocks = file_size / blk_free_size;
    if ((file_size % blk_free_size) > 0)
        blocks++;

    /* Update inode information */
    inode->i_size = file_size;
    inode->i_blocks = blocks;

    printf("import %s => %s (size=%ld, blocks=%d)\n", host_path, romfs_path,
           file_size, blocks);

    uint8_t *last_blk_addr = NULL;
    int file_size_remained = file_size;
    int file_pos = 0;

    int i;
    for (i = 0; i < blocks; i++) {
        /* Allocate new blocl */
        uint8_t *block_addr = (uint8_t *) ((uintptr_t) romfs_blk +
                                           (romfs_sb.s_blk_cnt * FS_BLK_SIZE));
        romfs_sb.s_blk_cnt++;

        /* First block to write */
        if (i == 0) {
            inode->i_data = romfs_ptr_to_off(block_addr);
        }

        /* Update the block header for the last block */
        if (i > 0) {
            struct block_header *blk_head_last =
                (struct block_header *) last_blk_addr;
            blk_head_last->b_next = romfs_ptr_to_off(block_addr);
        }

        int blk_pos = 0;
        int write_size = 0;

        /* Calculate the write size for the current block */
        if (file_size_remained > blk_free_size) {
            /* Too large to fit all */
            write_size = blk_free_size;
            file_size_remained -= blk_free_size;
        } else {
            /* Enough to fit the left data */
            write_size = file_size_remained;
        }

        /* Write the block header */
        struct block_header blk_head = {.b_next = 0, .reserved = 0};
        memcpy(&block_addr[blk_pos], &blk_head, blk_head_size);
        blk_pos += blk_head_size;

        /* Write the left file content */
        memcpy(&block_addr[blk_pos], &file_content[file_pos], write_size);
        blk_pos += write_size;

        /* Update the position of the file content that is written */
        file_pos += write_size;

        /* Preserve the current block address */
        last_blk_addr = block_addr;
    }

    free(file_content);
}

#define PATH_BUF_SIZE 500
void romfs_import_dir(const char *host_path, const char *romfs_path)
{
    /* Open the directory */
    DIR *dir = opendir(host_path);
    if (dir == NULL)
        exit(1);

    /* Enumerate all the files under the directory */
    struct dirent *dirent = NULL;
    while ((dirent = readdir(dir)) != NULL) {
        /* Ignore "." and ".." */
        if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..") ||
            !strcmp(dirent->d_name, ".gitkeep"))
            continue;

        char romfs_child_path[PATH_BUF_SIZE] = {0};
        char host_child_path[PATH_BUF_SIZE] = {0};

        if (dirent->d_type == DT_DIR) {
            /* Combine the children directory name with the parent pathname */
            snprintf(romfs_child_path, PATH_BUF_SIZE, "%s%s/", romfs_path,
                     dirent->d_name);
            snprintf(host_child_path, PATH_BUF_SIZE, "%s%s/", host_path,
                     dirent->d_name);

            /* Import the directory recursively */
            romfs_import_dir(host_child_path, romfs_child_path);
        } else if (dirent->d_type == DT_REG) {
            /* Combine the children file name with the parent pathname */
            snprintf(romfs_child_path, PATH_BUF_SIZE, "%s%s", romfs_path,
                     dirent->d_name);
            snprintf(host_child_path, PATH_BUF_SIZE, "%s%s", host_path,
                     dirent->d_name);

            /* Import the files under the directory */
            romfs_import_file(host_child_path, romfs_child_path);
        }
    }

    closedir(dir);
}

int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
        case 'v':
            _verbose = true;
            break;
        }
    }

    /* Every file of the image is as old as the image */
    _build_time = (uint32_t) time(NULL);

    romfs_init();
    romfs_import_dir(HOST_INPUT_DIR, ROMFS_OUTPUT_DIR);
    romfs_export();

    return 0;
}
