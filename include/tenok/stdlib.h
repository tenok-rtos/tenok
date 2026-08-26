/**
 * @file
 */
#ifndef __STDLIB_H__
#define __STDLIB_H__

#include <stddef.h>

/* What a program hands exit() to say how it went */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* The largest number rand() returns */
#define RAND_MAX 0x7fffffff

/**
 * @brief  To cause task termination
 * @param  status: Not used.
 * @retval None
 */
void exit(int status);

/**
 * @brief  Allocate a memory space.
 * @param  size: The number of bytes for allocating a memory space.
 * @retval void *: The pointer to the allocated memory. If the allocation
 *         failed, the function returns NULL.
 */
void *malloc(size_t size);

/**
 * @brief  Free a memory space
 * @param  ptr: Pointer to the allocated memory.
 * @retval None
 */
void free(void *ptr);

/**
 * @brief  Allocate and reset a memory space of an array.
 * @param  nmemb: Size of the array element.
 * @param  size: The number of array elements.
 * @retval void *: The pointer to the allocated memory. If the allocation
 *         failed, the function returns NULL.
 */
void *calloc(size_t nmemb, size_t size);

/**
 * @brief  Resize an allocated memory space, preserving its contents.
 * @param  ptr: Pointer to the allocated memory, or NULL to allocate anew.
 * @param  size: The new number of bytes, or zero to free the memory.
 * @retval void *: The pointer to the resized memory, which may differ from
 *         ptr. If the allocation failed, the function returns NULL and the
 *         memory pointed to by ptr is left untouched.
 */
void *realloc(void *ptr, size_t size);

/**
 * @brief  Create a file no other name stands for and open it. The last six
 *         characters of the template must be XXXXXX and are replaced with the
 *         name the file was given.
 * @param  template: The name to make unique, ending in XXXXXX.
 * @retval int: The descriptor of the new file, and -1 on error with the
 *         reason left in errno.
 */
int mkstemp(char *template);

/* Room on the stack of the caller, which goes away when the caller does. The
 * compiler is the one that can take it, so it is the one that is asked
 */
#define alloca(size) __builtin_alloca(size)

/*
 * Currently not implemented:
 */

#if 0
int atexit(void (*function)(void));
void abort(void);
int system(const char *command);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int putenv(char *string);
char *getenv(const char *name);
#endif

/*
 * Non-standard extensions:
 */

char *ltoa(long value, char *buffer, int radix);

/*
 * Functions provided by the compiler:
 */

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

typedef struct {
    long long int quot;
    long long int rem;
} lldiv_t;

int atoi(const char *nptr);
long atol(const char *nptr);
long long atoll(const char *nptr);
long a64l(const char *str64);
int abs(int j);
double atof(const char *nptr);
void *bsearch(const void *key,
              const void *base,
              size_t nmemb,
              size_t size,
              int (*compar)(const void *, const void *));
div_t div(int numerator, int denominator);
int getsubopt(char **optionp, char *const *tokens, char **valuep);
char *l64a(long value);
long labs(long j);
ldiv_t ldiv(long numerator, long denominator);
long long llabs(long long j);
lldiv_t lldiv(long long numerator, long long denominator);
int mblen(const char *s, size_t n);
size_t mbstowcs(wchar_t *dest, const char *src, size_t n);
int mbtowc(wchar_t *pwc, const char *s, size_t n);
void qsort(void *base,
           size_t nmemb,
           size_t size,
           int (*compar)(const void *, const void *));
char *realpath(const char *path, char *resolved_path);
double strtod(const char *nptr, char **endptr);
float strtof(const char *nptr, char **endptr);
long strtol(const char *nptr, char **endptr, int base);
long double strtold(const char *nptr, char **endptr);
long long strtoll(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
size_t wcstombs(char *dest, const wchar_t *src, size_t n);
int wctomb(char *s, wchar_t wc);

#endif
