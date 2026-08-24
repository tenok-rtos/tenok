#include <ctype.h>
#include <fnmatch.h>
#include <stddef.h>
#include <string.h>

/* Case folding, which FNM_CASEFOLD asks for */
static int fold(int c, int flags)
{
    return (flags & FNM_CASEFOLD) ? tolower((unsigned char) c) : c;
}

/* A slash is matched by nothing but a slash when FNM_PATHNAME is given */
static int is_slash(int c, int flags)
{
    return (flags & FNM_PATHNAME) && c == '/';
}

/* A period that FNM_PERIOD asks to be matched by a period and nothing else */
static int is_lead(const char *s, const char *start, int flags)
{
    if (!(flags & FNM_PERIOD) || *s != '.')
        return 0;

    return s == start || ((flags & FNM_PATHNAME) && s[-1] == '/');
}

/* The character classes POSIX names inside a bracket expression */
static int match_class(const char *name, size_t len, int c)
{
    static const struct {
        const char *name;
        int (*is)(int);
    } classes[] = {
        {"alnum", isalnum}, {"alpha", isalpha}, {"blank", isblank},
        {"cntrl", iscntrl}, {"digit", isdigit}, {"graph", isgraph},
        {"lower", islower}, {"print", isprint}, {"punct", ispunct},
        {"space", isspace}, {"upper", isupper}, {"xdigit", isxdigit},
    };

    for (unsigned i = 0; i < sizeof(classes) / sizeof(*classes); i++) {
        if (strlen(classes[i].name) == len &&
            !strncmp(classes[i].name, name, len))
            return classes[i].is((unsigned char) c) != 0;
    }

    return 0;
}

/* Match a bracket expression. One that never ends is an ordinary '[' */
static int match_bracket(const char **pattern, char c, int flags)
{
    const char *p = *pattern + 1;
    int negate = 0;
    int matched = 0;

    if (*p == '!' || *p == '^') {
        negate = 1;
        p++;
    }

    /* A ']' in the first position stands for itself */
    if (*p == ']') {
        if (fold(*p, flags) == fold(c, flags))
            matched = 1;
        p++;
    }

    for (; *p && *p != ']'; p++) {
        if (p[0] == '[' && p[1] == ':') {
            const char *end = strstr(p + 2, ":]");

            if (end) {
                if (match_class(p + 2, end - (p + 2), c))
                    matched = 1;
                p = end + 1;
                continue;
            }
        }

        if (!(flags & FNM_NOESCAPE) && *p == '\\' && p[1] && p[1] != ']') {
            if (fold(p[1], flags) == fold(c, flags))
                matched = 1;
            p++;
        } else if (p[1] == '-' && p[2] && p[2] != ']') {
            if (fold(c, flags) >= fold(p[0], flags) &&
                fold(c, flags) <= fold(p[2], flags))
                matched = 1;
            p += 2;
        } else if (fold(*p, flags) == fold(c, flags)) {
            matched = 1;
        }
    }

    /* Unterminated, the bracket was not one after all */
    if (*p != ']')
        return -1;

    *pattern = p + 1;

    return negate ? !matched : matched;
}

/* The star is backtracked, so a pattern of stars cannot run the stack out */
int fnmatch(const char *pattern, const char *string, int flags)
{
    const char *start = string;
    const char *star = NULL, *retry = string;

    while (*string) {
        /* A leading period is matched by a period and by nothing else */
        if (is_lead(string, start, flags)) {
            if (*pattern != '.')
                return FNM_NOMATCH;

            pattern++;
            string++;
            continue;
        }

        int taken = 0;

        if (!(flags & FNM_NOESCAPE) && *pattern == '\\' && pattern[1]) {
            if (fold(pattern[1], flags) == fold(*string, flags)) {
                pattern += 2;
                string++;
                continue;
            }
            taken = -1;
        } else if (*pattern == '[') {
            const char *after = pattern;
            int matched = match_bracket(&after, *string, flags);

            if (matched >= 0) {
                if (matched && !is_slash(*string, flags)) {
                    pattern = after;
                    string++;
                    continue;
                }
                taken = -1;
            }
        }

        if (taken == 0 && *pattern == '?' && !is_slash(*string, flags)) {
            pattern++;
            string++;
        } else if (taken == 0 && *pattern != '*' && *pattern != '?' &&
                   fold(*pattern, flags) == fold(*string, flags)) {
            pattern++;
            string++;
        } else if (*pattern == '*') {
            star = pattern++;
            retry = string;
        } else if (star && !is_slash(*retry, flags)) {
            pattern = star + 1;
            string = ++retry;
        } else {
            return FNM_NOMATCH;
        }
    }

    while (*pattern == '*')
        pattern++;

    return (*pattern == '\0') ? 0 : FNM_NOMATCH;
}
