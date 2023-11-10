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
 *         memory
 * @param  size: The number of bytes to allocate for the memory.
 * @retval void *: The pointer to the allocated memory. If the allocation
 *         failed, the function returns NULL.
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
 *         failed, the function returns NULL.
 */
void *calloc(size_t nmemb, size_t size);

/**
 * currently not implemented yet:
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

/**
 * non-standard extensions:
 */

char *itoa(int value, char *buffer, int radix);
char *utoa(unsigned int value, char *buffer, int radix);
char *ltoa(long value, char *buffer, int radix);
char *ultoa(unsigned long value, char *buffer, int radix);

/**
 * functions provided by the compiler:
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
