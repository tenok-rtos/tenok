#include <libgen.h>
#include <string.h>

/* Newlib ships neither, and a path is a string and nothing else */
char *dirname(char *path)
{
    if (!path || !*path)
        return ".";

    char *end = path + strlen(path) - 1;

    /* A trailing slash is not part of the name */
    while (end > path && *end == '/')
        end--;

    /* Walk back over the name itself */
    while (end > path && *end != '/')
        end--;

    /* No slash at all: the directory is the current one */
    if (*end != '/')
        return ".";

    /* Cutting at the slash would make the parent of "/a" the empty string */
    while (end > path && *end == '/')
        end--;

    end[1] = '\0';

    return path;
}

/* The form newlib declares: it answers with a pointer into the string */
char *basename(const char *path)
{
    const char *slash = strrchr(path, '/');

    return (char *) (slash ? slash + 1 : path);
}
