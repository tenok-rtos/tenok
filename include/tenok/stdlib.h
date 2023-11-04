/**
 * @file
 */
#ifndef __STDLIB_H__
#define __STDLIB_H__

/**
 * @brief  To cause task termination
 * @param  status: Not used.
 * @retval None
 */
void exit(int status);

/**
 * @brief  Allocate size bytes and returns a pointer to the allocated
           memory
 * @param  size: The number of bytes to allocate for the memory.
 * @retval void *: The pointer to the allocated memory. If the allocation
 *                 failed, the function returns NULL.
 */
void *malloc(size_t size);

/**
 * @brief  Free the memory space pointed to by ptr, which must have
           been returned by a previous call to malloc()
 * @param  ptr: The memory space to provide.
 * @retval None
 */
void free(void *ptr);

/**
 * @brief  Allocate memory of an array of nmemb elements of size bytes.
 *         The newly allocated memory will be set to zero.
 * @param  nmemb: Size of the array element.
 * @param  size: The number of of the nmemb to allocate.
 * @retval void *: The pointer to the allocated memory. If the allocation
 *                 failed, the function returns NULL.
 */
void *calloc(size_t nmemb, size_t size);

/* non-standard extensions: */
char *itoa(int value, char *buffer, int radix);
char *utoa(unsigned int value, char *buffer, int radix);
char *ltoa(long value, char *buffer, int radix);
char *ultoa(unsigned long value, char *buffer, int radix);

/* rest of the functions are provided by the compiler: */
int atoi(const char *nptr);
long atol(const char *nptr);
long long atoll(const char *nptr);

#endif
