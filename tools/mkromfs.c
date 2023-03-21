#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdarg.h>
#include "list.h"
#include "kconfig.h"

#define HOST_INPUT_DIR   "../rom/"
#define ROMFS_OUTPUT_DIR "/rom_data/"
#define OUTPUT_BIN       "./romfs.bin"

#define S_IFREG 3 //regular file
#define S_IFDIR 4 //directory

bool _verbose = false;

struct super_block {
    uint32_t s_blk_cnt;   //number of the used blocks
    uint32_t s_inode_cnt; //number of the used inodes

    bool     s_rd_only;   //read only flag

    uint32_t s_sb_addr;   //start address of the super block
    uint32_t s_ino_addr;  //start address of the inode table
    uint32_t s_blk_addr;  //start address of the blocks region
};

/* block header will be placed to the top of every blocks of the regular file */
struct block_header {
    uint32_t b_next; //virtual address of the next block
};

struct mount {
    struct file *dev_file;        //driver file of the mounted storage device
    struct super_block super_blk; //super block of the mounted storage device
};

/* index node */
struct inode {
    uint8_t  i_mode;      //file type: e.g., S_IFIFO, S_IFCHR, etc.

    uint8_t  i_rdev;      //the device on which this file system is mounted
    bool     i_sync;      //the mounted file is loaded into the rootfs or not

    uint32_t i_ino;       //inode number
    uint32_t i_parent;    //inode number of the parent directory

    uint32_t i_fd;        //file descriptor number

    uint32_t i_size;      //file size (bytes)
    uint32_t i_blocks;    //block_numbers = file_size / block_size
    uint32_t i_data;      //virtual address for accessing the storage

    struct list i_dentry; //list head of the dentry table
};

/* directory entry */
struct dentry {
    char     d_name[FILE_NAME_LEN_MAX]; //file name

    uint32_t d_inode;  //the inode of the file
    uint32_t d_parent; //the inode of the parent directory

    struct list d_list;
};

struct super_block romfs_sb;
struct inode inodes[INODE_CNT_MAX];
uint8_t romfs_blk[FS_BLK_CNT][FS_BLK_SIZE];

int verbose(const char * restrict format, ...)
{
    if(!_verbose)
        return 0;

    va_list args;

    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);

    return ret;
}

void romfs_init(void)
{
    /* configure the root directory inode */
    struct inode *inode_root = &inodes[0];
    inode_root->i_mode   = S_IFDIR;
    inode_root->i_ino    = 0;
    inode_root->i_size   = 0;
    inode_root->i_blocks = 0;
    inode_root->i_data   = (uint32_t)NULL;
    list_init(&inode_root->i_dentry);

    romfs_sb.s_inode_cnt = 1;
    romfs_sb.s_blk_cnt   = 0;
    romfs_sb.s_rd_only   = true;
    romfs_sb.s_sb_addr   = 0;
    romfs_sb.s_ino_addr  = sizeof(struct super_block);
    romfs_sb.s_blk_addr  = romfs_sb.s_ino_addr + (sizeof(struct inode) * INODE_CNT_MAX);
}

struct inode *fs_search_file(struct inode *inode_dir, char *file_name)
{
    /* currently the dentry table is empty */
    if(inode_dir->i_size == 0)
        return NULL;

    /* traversal of the dentry list */
    struct list *list_curr;
    list_for_each(list_curr, &inode_dir->i_dentry) {
        struct dentry *dentry = list_entry(list_curr, struct dentry, d_list);

        /* compare the file name with the dentry */
        if(strcmp(dentry->d_name, file_name) == 0)
            return &inodes[dentry->d_inode];
    }

    return NULL;
}

int fs_calculate_dentry_blocks(size_t block_size, size_t dentry_cnt)
{
    /* calculate how many dentries can a block hold */
    int dentry_per_blk = block_size / sizeof(struct dentry);

    /* calculate how many blocks is required for storing N dentries */
    int blocks = dentry_cnt / dentry_per_blk;
    if(dentry_cnt % dentry_per_blk)
        blocks++;

    return blocks;
}

struct inode *fs_add_file(struct inode *inode_dir, char *file_name, int file_type)
{
    /* inodes numbers is full */
    if(romfs_sb.s_inode_cnt >= INODE_CNT_MAX)
        return NULL;

    /* calculate how many dentries can a block hold */
    int dentry_per_blk = FS_BLK_SIZE / sizeof(struct dentry);

    /* calculate how many dentries the directory has */
    int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);

    /* check if current block can fit a new dentry */
    bool fit = ((dentry_cnt + 1) <= (inode_dir->i_blocks * dentry_per_blk)) &&
               (inode_dir->i_size != 0) /* no memory is allocated if size = 0 */;

    /* allocate memory for the new dentry */
    uint8_t *dir_data_p;
    if(fit == true) {
        struct list *list_end = inode_dir->i_dentry.prev;
        struct dentry *dir = list_entry(list_end, struct dentry, d_list);
        dir_data_p = (uint8_t *)dir + sizeof(struct dentry);
    } else {
        /* can not fit, requires a new block */
        dir_data_p = (uint8_t *)romfs_blk + (romfs_sb.s_blk_cnt * FS_BLK_SIZE);

        romfs_sb.s_blk_cnt++;
    }

    /* configure the new dentry */
    struct dentry *new_dentry = (struct dentry*)dir_data_p;
    new_dentry->d_inode  = romfs_sb.s_inode_cnt;               //assign new inode number for the file
    new_dentry->d_parent = inode_dir->i_ino;                   //save the inode of the parent directory
    strncpy(new_dentry->d_name, file_name, FILE_NAME_LEN_MAX); //copy the file name

    /* configure the new file inode */
    struct inode *new_inode = &inodes[romfs_sb.s_inode_cnt];
    new_inode->i_ino    = romfs_sb.s_inode_cnt;
    new_inode->i_parent = inode_dir->i_ino;
    new_inode->i_fd     = 0;

    /* file instantiation */
    int result = 0;

    switch(file_type) {
        case S_IFREG: {
            new_inode->i_mode   = S_IFREG;
            new_inode->i_size   = 0;
            new_inode->i_blocks = 0;
            new_inode->i_data   = (uint32_t)NULL; //empty file

            break;
        }
        case S_IFDIR: {
            new_inode->i_mode   = S_IFDIR;
            new_inode->i_size   = 0;
            new_inode->i_blocks = 0;
            new_inode->i_data   = (uint32_t)NULL; //empty directory
            list_init(&new_inode->i_dentry);

            break;
        }
        default:
            exit(-1);
    }

    romfs_sb.s_inode_cnt++; //update inode number for the next file

    /* currently no files is under the directory */
    if(list_is_empty(&inode_dir->i_dentry) == true)
        inode_dir->i_data = (uint32_t)new_dentry; //add the first dentry

    /* insert the new file under the current directory */
    list_push(&inode_dir->i_dentry, &new_dentry->d_list);

    /* update size and block information of the inode */
    inode_dir->i_size += sizeof(struct dentry);

    dentry_cnt = inode_dir->i_size / sizeof(struct dentry);
    inode_dir->i_blocks = fs_calculate_dentry_blocks(FS_BLK_SIZE, dentry_cnt);

    return new_inode;
}

char *fs_split_path(char *entry, char *path)
{
    while(1) {
        bool found_dir = (*path == '/');

        /* copy */
        if(found_dir == false) {
            *entry = *path;
            entry++;
        }

        path++;

        if((found_dir == true) || (*path == '\0'))
            break;
    }

    *entry = '\0';

    if(*path == '\0')
        return NULL; //the path can not be splitted anymore

    return path; //return the address of the left path string
}

static struct inode *fs_create_file(char *pathname, uint8_t file_type)
{
    /* the path name must start with '/' */
    if(pathname[0] != '/')
        return NULL;

    struct inode *inode_curr = &inodes[0]; //start from the root node
    struct inode *inode;

    char file_name[FILE_NAME_LEN_MAX];
    char entry[PATH_LEN_MAX];
    char *path = pathname;

    path = fs_split_path(entry, path); //get rid of the first '/'

    while(1) {
        /* split the path and get the entry name at each layer */
        path = fs_split_path(entry, path);

        /* two successive '/' are detected */
        if(entry[0] == '\0')
            continue;

        /* the last non-empty entry string is the file name */
        if(entry[0] != '\0')
            strncpy(file_name, entry, FILE_NAME_LEN_MAX);

        /* search the entry and get the inode */
        inode = fs_search_file(inode_curr, entry);

        if(path != NULL) {
            /* the path can be further splitted, which means it is a directory */

            /* check if the directory exists */
            if(inode == NULL) {
                /* directory does not exist, create one */
                inode = fs_add_file(inode_curr, entry, S_IFDIR);

                /* failed to create the directory */
                if(inode == NULL)
                    return NULL;
            }

            inode_curr = inode;
        } else {
            /* no more path to be splitted, the remained string should be the file name */

            /* file with the same name already exists */
            if(inode != NULL)
                return NULL;

            /* create new inode for the file */
            inode = fs_add_file(inode_curr, file_name, file_type);

            /* failed to create the file */
            if(inode == NULL)
                return NULL;

            /* file is created successfully */
            return inode;
        }
    }
}

void romfs_address_conversion_dir(struct inode *inode)
{
    uint32_t sb_size = sizeof(romfs_sb);
    uint32_t inodes_size = sizeof(inodes);
    uint32_t blocks_size = sizeof(romfs_blk);

    /* adjust the address stored in the inode.i_data (which is in the block region) */
    inode->i_data = inode->i_data - (uint32_t)romfs_blk + sb_size + inodes_size;

    struct list *list_start = &inode->i_dentry;
    struct list *list_curr = list_start;

    /* adjust the dentry list */
    do {
        /* preserve the next pointer before modifying */
        struct list *list_next = list_curr->next;

        /* obtain the dentry */
        struct dentry *dentry = list_entry(list_curr, struct dentry, d_list);

        if(dentry->d_list.prev == &inode->i_dentry) {
            /* the address of the inode.i_dentry (list head) is in the inodes region */
            dentry->d_list.prev = (struct list *)((uint8_t *)&inode->i_dentry - (uint32_t)inodes + sb_size);
        } else {
            /* besides the list head, others in the blocks region */
            dentry->d_list.prev = (struct list *)((uint8_t *)dentry->d_list.prev - (uint32_t)romfs_blk + sb_size + inodes_size);
        }

        if(dentry->d_list.next == &inode->i_dentry) {
            /* the address of the inode.i_dentry (list head) is in the inodes region */
            dentry->d_list.next = (struct list *)((uint8_t *)&inode->i_dentry - (uint32_t)inodes + sb_size);
        } else {
            /* besides the list head, others in the blocks region */
            dentry->d_list.next = (struct list *)((uint8_t *)dentry->d_list.next - (uint32_t)romfs_blk + sb_size + inodes_size);
        }

        if(list_curr != list_start) {
            verbose("[dentry:\"%s\", file_inode:%d, prev:%d, next:%d]\n",
                    dentry->d_name, dentry->d_inode,
                    (uint32_t)dentry->d_list.prev, (uint32_t)dentry->d_list.next);
        }

        list_curr = list_next;
    } while(list_curr != list_start);
}

void romfs_address_conversion_file(struct inode *inode)
{
    if(inode->i_size == 0)
        return;

    uint32_t sb_size = sizeof(romfs_sb);
    uint32_t inodes_size = sizeof(inodes);
    uint32_t blocks_size = sizeof(romfs_blk);

    uint32_t blk_head_size = sizeof(struct block_header);
    uint32_t blk_free_size = FS_BLK_SIZE - blk_head_size;

    /* calculate the blocks count */
    int blocks = inode->i_size / blk_free_size;
    if((inode->i_size % blk_free_size) > 0)
        blocks++;

    struct block_header *blk_head;
    uint32_t next_blk_addr;

    /* adjust the block headers of the file */
    int i;
    for(i = 0; i < blocks; i++) {
        if(i == 0) {
            /* get the address of the firt block from inode.i_data */
            blk_head = (struct block_header *)inode->i_data;
            next_blk_addr = blk_head->b_next;
        } else {
            /* get the address of the rest from the block header */
            blk_head = (struct block_header *)next_blk_addr;
            next_blk_addr = blk_head->b_next;
        }

        /* the last block header requires no adjustment */
        if(blk_head->b_next == (uint32_t)NULL)
            break;

        /* adjust block_head.b_next (the address in in the block region) */
        blk_head->b_next = blk_head->b_next - (uint32_t)romfs_blk + sb_size + inodes_size;

        verbose("[inode: #%d, block #%d, next:%d]\n", inode->i_ino, i+1, (uint32_t)blk_head->b_next);
    }

    /* adjust the address stored in the inode.i_data (which is in the block region) */
    inode->i_data = inode->i_data - (uint32_t)romfs_blk + sb_size + inodes_size;

    verbose("[inode: #%d, block #0, next:%d]\n", inode->i_ino, inode->i_data);
}

void romfs_export(void)
{
    FILE *file = fopen(OUTPUT_BIN, "wb");

    uint32_t sb_size = sizeof(romfs_sb);
    uint32_t inodes_size = sizeof(inodes);
    uint32_t blocks_size = sizeof(romfs_blk);

    /* memory space conversion */
    verbose("================================\n"
            "[romfs memory space conversion]\n");
    int i;
    for(i = 0; i < romfs_sb.s_inode_cnt; i++) {
        if(inodes[i].i_mode == S_IFDIR) {
            romfs_address_conversion_dir(&inodes[i]);
        } else if(inodes[i].i_mode == S_IFREG) {
            romfs_address_conversion_file(&inodes[i]);
        } else {
            printf("mkromfs: error, unknown file type.\n");
            exit(-1);
        }
    }
    verbose("================================\n");

    printf("[romfs generation report]\n"
           "super block size: %d bytes\n"
           "inodes size: %d bytes\n"
           "blocks size: %d bytes\n"
           "inode count: %d\n"
           "block count: %d\n",
           sb_size, inodes_size, blocks_size,
           romfs_sb.s_inode_cnt,
           romfs_sb.s_blk_cnt);

    fwrite((uint8_t *)&romfs_sb, sizeof(uint8_t), sb_size, file);
    fwrite((uint8_t *)&inodes, sizeof(uint8_t), inodes_size, file);
    fwrite((uint8_t *)&romfs_blk, sizeof(uint8_t), blocks_size, file);

    fclose(file);
}

void romfs_import_file(char *host_path, char *romfs_path)
{
    /* create new romfs file */
    struct inode *inode = fs_create_file(romfs_path, S_IFREG);
    if(inode == NULL) {
        printf("[mkromfs] failed to create new file!\n");
        exit(-1);
    }

    /* open the source file */
    FILE *file = fopen(host_path, "r");
    if(file == NULL) {
        printf("%s: failed to open the file!\n", host_path);
        exit(-1);
    }

    /* get the source file size */
    fseek(file, 0, SEEK_END); //get the file size
    long file_size = ftell(file);

    /* nothing to write to the romfs file */
    if(file_size == 0) {
        fclose(file);
        return;
    }

    /* check if the file is too big */
    int left_space = FS_BLK_SIZE * (FS_BLK_CNT - romfs_sb.s_blk_cnt);
    if(file_size > left_space) {
        printf("%s: the space is not enough to fit the file!\n", host_path);
        exit(1);
    }

    /* read and close the source file */
    char *file_content = malloc(file_size + 1); //plus one for the EOF symbol
    fseek(file, 0L, SEEK_SET);
    fread(file_content, sizeof(char), file_size, file);
    fclose(file);

    /* calculate the required blocks number */
    uint32_t blk_head_size = sizeof(struct block_header);
    uint32_t blk_free_size = FS_BLK_SIZE - blk_head_size;
    int blocks = file_size / blk_free_size;
    if((file_size % blk_free_size) > 0)
        blocks++;

    /* update inode information */
    inode->i_size   = file_size;
    inode->i_blocks = blocks;

    printf("import %s => %s (size=%ld, blocks=%d)\n", host_path, romfs_path, file_size, blocks);

    uint8_t *last_blk_addr = NULL;
    int file_size_remained = file_size;
    int file_pos = 0;

    int i;
    for(i = 0; i < blocks; i++) {
        /* new block allocation */
        uint8_t *block_addr = (uint8_t *)romfs_blk + (romfs_sb.s_blk_cnt * FS_BLK_SIZE);
        romfs_sb.s_blk_cnt++;

        /* first block to write */
        if(i == 0) {
            inode->i_data = (uint32_t)block_addr;
        }

        /* update the block header for the last block */
        if(i > 0) {
            struct block_header *blk_head_last =  (struct block_header *)last_blk_addr;
            blk_head_last->b_next = (uint32_t)block_addr;
        }

        int blk_pos = 0;
        int write_size = 0;

        /* calculate the write size for the current block */
        if(file_size_remained > blk_free_size) {
            /* too large to fit all */
            write_size = blk_free_size;
            file_size_remained -= blk_free_size;
        } else {
            /* enough to fit the left data */
            write_size = file_size_remained;
        }

        /* write the block header */
        struct block_header blk_head = {.b_next = (uint32_t)NULL};
        memcpy(&block_addr[blk_pos], &blk_head, blk_head_size);
        blk_pos += blk_head_size;

        /* write the left file content */
        memcpy(&block_addr[blk_pos], &file_content[file_pos], write_size);
        blk_pos += write_size;

        /* update the position of the file content that is written */
        file_pos += write_size;

        /* preserve the current block address */
        last_blk_addr = block_addr;
    }

    free(file_content);
}

#define PATH_BUF_SIZE 500
char romfs_import_dir(const char *host_path, const char *romfs_path)
{
    /* open the directory */
    DIR *dir = opendir(host_path);
    if(dir == NULL)
        exit(1);

    /* enumerate all the files under the directory */
    struct dirent* dirent = NULL;
    while ((dirent = readdir(dir)) != NULL) {
        /* ignore "." and ".." */
        if(!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
            continue;

        char romfs_child_path[PATH_BUF_SIZE] = {0};
        char host_child_path[PATH_BUF_SIZE] = {0};

        if(dirent->d_type == DT_DIR) {
            /* combine the children directory name with the parent path name */
            snprintf(romfs_child_path, PATH_BUF_SIZE, "%s%s/", romfs_path, dirent->d_name);
            snprintf(host_child_path, PATH_BUF_SIZE, "%s%s/", host_path, dirent->d_name);

            /* import the directory recursively */
            romfs_import_dir(host_child_path, romfs_child_path);
        } else if(dirent->d_type == DT_REG) {
            /* combine the children file name with the parent path name */
            snprintf(romfs_child_path, PATH_BUF_SIZE, "%s%s", romfs_path, dirent->d_name);
            snprintf(host_child_path, PATH_BUF_SIZE, "%s%s", host_path, dirent->d_name);

            /* import the files under the directory */
            romfs_import_file(host_child_path, romfs_child_path);
        }
    }

    closedir(dir);
}

int main(int argc, char **argv)
{
    int opt;
    while((opt = getopt(argc, argv, "v")) != -1) {
        switch(opt) {
            case 'v':
                _verbose = true;
                break;
        }
    }

    romfs_init();
    romfs_import_dir(HOST_INPUT_DIR, ROMFS_OUTPUT_DIR);
    romfs_export();

    return 0;
}
