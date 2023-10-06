#ifndef __PRINTK_H__
#define __PRINTK_H__

enum {
    PRINTK_IDLE,
    PRINTK_BUSY
};

void printk_init(void);
void printk_handler(void);

void printk(char *format,  ...);

#endif
