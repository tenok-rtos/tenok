#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include "list.h"

#define INPUT_DIR         "."
#define OUTPUT            "./romfs.bin"

#define FILE_NAME_LEN_MAX 30
#define PATH_LEN_MAX      128
#define INODE_CNT_MAX     100
#define ROMFS_BLK_SIZE    128
#define ROMFS_BLK_CNT     20

#define S_IFREG 3 //regular file
#define S_IFDIR 4 //directory

struct super_block {
    uint32_t s_blk_cnt;   //number of the used blocks
    uint32_t s_inode_cnt; //number of the used inodes

    bool     s_rd_only;   //read only flag

    uint32_t s_sb_addr;   //start address of the super block
    uint32_t s_ino_addr;  //start address of the inode table
    uint32_t s_blk_addr;  //start address of the blocks region
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
uint8_t romfs_blk[ROMFS_BLK_CNT][ROMFS_BLK_SIZE];

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

int calculate_dentry_block_size(size_t block_size, size_t dentry_cnt)
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
    int dentry_per_blk = ROMFS_BLK_SIZE / sizeof(struct dentry);

    /* calculate how many dentries the directory has */
    int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);

    /* check if current block can fit a new dentry */
    bool fit = ((dentry_cnt + 1) <= (inode_dir->i_blocks * dentry_per_blk)) &&
               (inode_dir->i_size != 0) /* no memory is allocated if size = 0 */;

    /* allocate memory for the new dentry */
    uint8_t *dir_data_p;
    if(fit == true) {
        struct list *list_end = inode_dir->i_dentry.last;
        struct dentry *dir = list_entry(list_end, struct dentry, d_list);
        dir_data_p = (uint8_t *)dir + sizeof(struct dentry);
    } else {
        /* can not fit, requires a new block */
        dir_data_p = (uint8_t *)romfs_blk + (romfs_sb.s_blk_cnt * ROMFS_BLK_SIZE);

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
            /* allocate memory for the new file */
            uint8_t *file_data_p = (uint8_t *)romfs_blk + (romfs_sb.s_blk_cnt * ROMFS_BLK_SIZE);
            romfs_sb.s_blk_cnt++;

            //XXX
            char test_str[] = {'c', 'a', 't', '\n', '\r',-1};

            new_inode->i_mode   = S_IFREG;
            new_inode->i_size   = 6; //XXX
            new_inode->i_blocks = 1;
            new_inode->i_data   = (uint32_t)file_data_p;

            memcpy(file_data_p, test_str, sizeof(char) * 6);

            break;
        }
        case S_IFDIR: {
            new_inode->i_mode   = S_IFDIR;
            new_inode->i_size   = 0;
            new_inode->i_blocks = 0;
            new_inode->i_data   = (uint32_t)NULL; //new directory without any files
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
    inode_dir->i_blocks = calculate_dentry_block_size(ROMFS_BLK_SIZE, dentry_cnt);

    return new_inode;
}

char *split_path(char *entry, char *path)
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

static int create_file(char *pathname, uint8_t file_type)
{
    /* a legal path name must start with '/' */
    if(pathname[0] != '/')
        return -1;

    struct inode *inode_curr = &inodes[0]; //start from the root node
    struct inode *inode;

    char entry[PATH_LEN_MAX];
    char *path = pathname;

    path = split_path(entry, path); //get rid of the first '/'

    while(1) {
        /* split the path and get the entry name at each layer */
        path = split_path(entry, path);

        /* two successive '/' are detected */
        if(entry[0] == '\0')
            continue;

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
                    return -1;
            }

            inode_curr = inode;
        } else {
            /* no more path to be splitted, the remained string should be the file name */

            /* make sure the last char is not equal to '/' */
            int len = strlen(pathname);
            if(pathname[len - 1] == '/')
                return -1;

            /* create new inode for the file */
            inode = fs_add_file(inode_curr, entry, file_type);

            /* failed to create the file */
            if(inode == NULL)
                return -1;

            /* file is created successfully */
            return inode->i_fd;
        }
    }
}

void romfs_export(void)
{
    FILE *file = fopen(OUTPUT, "wb");

    uint32_t sb_size = sizeof(romfs_sb);
    uint32_t inodes_size = sizeof(inodes);
    uint32_t blocks_size = sizeof(romfs_blk);

    /* memory space conversion */
    printf("[romfs memory space conversion]\n");
    int i;
    for(i = 0; i < romfs_sb.s_inode_cnt; i++) {
        /* adjust the inode.i_data (which is point to the block region) */
        inodes[i].i_data = inodes[i].i_data - (uint32_t)romfs_blk + sb_size + inodes_size;

        if(inodes[i].i_mode == S_IFDIR) {
            struct list *list_start = &inodes[i].i_dentry;
            struct list *list_curr = list_start;

            /* adjust dentries */
            do {
                /* preserve the next pointer before modifying */
                struct list *list_next = list_curr->next;

                /* obtain the dentry */
                struct dentry *dentry = list_entry(list_curr, struct dentry, d_list);

                if(dentry->d_list.last == &inodes[i].i_dentry) {
                    /* inode.i_dentry is stored in the inodes region */
                    dentry->d_list.last = (struct list *)((uint8_t *)&inodes[i].i_dentry - (uint32_t)inodes + sb_size);
                } else {
                    /* other are stored in the block region */
                    dentry->d_list.last = (struct list *)((uint8_t *)dentry->d_list.last - (uint32_t)romfs_blk + sb_size + inodes_size);
                }

                if(dentry->d_list.next == &inodes[i].i_dentry) {
                    /* inode.i_dentry is stored in the inodes region */
                    dentry->d_list.next = (struct list *)((uint8_t *)&inodes[i].i_dentry - (uint32_t)inodes + sb_size);
                } else {
                    /* other are stored in the block region */
                    dentry->d_list.next = (struct list *)((uint8_t *)dentry->d_list.next - (uint32_t)romfs_blk + sb_size + inodes_size);
                }

                printf("[file_name:\"%s\", file_inode:%d, last:%d, next:%d]\n",
                       dentry->d_name, dentry->d_inode, (uint32_t)dentry->d_list.last, (uint32_t)dentry->d_list.next);

                list_curr = list_next;
            } while(list_curr != list_start);
        }
    }

    printf("romfs generation report:\n"
           "super block size: %d bytes\n"
           "inode table size: %d bytes\n"
           "block region size: %d bytes\n"
           "used inode count: %d\n"
           "used block count: %d\n",
           sb_size, inodes_size, blocks_size,
           romfs_sb.s_inode_cnt,
           romfs_sb.s_blk_cnt);

    fwrite((uint8_t *)&romfs_sb, sizeof(uint8_t), sb_size, file);
    fwrite((uint8_t *)&inodes, sizeof(uint8_t), inodes_size, file);
    fwrite((uint8_t *)&romfs_blk, sizeof(uint8_t), blocks_size, file);

    fclose(file);
}

char romfs_import_dir(const char *path)
{
    DIR *dir = opendir(path);
    if(dir == NULL)
        exit(1);

    struct dirent* dirent = NULL;
    while ((dirent = readdir(dir)) != NULL) {
        //printf("%s\n", dirent->d_name);
    }

    closedir(dir);
}

int main(int argc, char **argv)
{
    int opt;
    while((opt = getopt(argc, argv, "o:d:")) != -1) {
        switch(opt) {
            case 'v':
                break;
            case 'd':
                printf("d: %s\n", optarg);
                break;
            case 'o':
                printf("o: %s\n", optarg);
                break;
        }
    }

    romfs_init();

    romfs_import_dir(INPUT_DIR);

    create_file("/rom_data/test1.txt", S_IFREG);
    create_file("/rom_data/test2.txt", S_IFREG);

    romfs_export();

    return 0;
}
