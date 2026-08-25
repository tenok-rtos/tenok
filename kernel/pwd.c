#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Tenok has one user and one group, both of them root */
static char *const no_members[] = {NULL};

static struct passwd root_user = {
    .pw_name = "root",
    .pw_passwd = "",
    .pw_uid = 0,
    .pw_gid = 0,
    .pw_gecos = "",
    .pw_dir = "/",
    .pw_shell = "",
};

static struct group root_group = {
    .gr_name = "root",
    .gr_passwd = "",
    .gr_gid = 0,
    .gr_mem = (char **) no_members,
};

struct passwd *getpwuid(uid_t uid)
{
    return (uid == 0) ? &root_user : NULL;
}

struct passwd *getpwnam(const char *name)
{
    return (name && !strcmp(name, "root")) ? &root_user : NULL;
}

struct group *getgrgid(gid_t gid)
{
    return (gid == 0) ? &root_group : NULL;
}

struct group *getgrnam(const char *name)
{
    return (name && !strcmp(name, "root")) ? &root_group : NULL;
}

/* Everything runs as root, and nothing can change that */
uid_t getuid(void)
{
    return 0;
}

uid_t geteuid(void)
{
    return 0;
}

gid_t getgid(void)
{
    return 0;
}

gid_t getegid(void)
{
    return 0;
}

/* Root already owns every file, so asking for that is already met */
int chown(const char *pathname, uid_t owner, gid_t group)
{
    struct stat statbuf;

    if (stat(pathname, &statbuf) != 0)
        return -1;

    if ((owner != 0 && owner != (uid_t) -1) ||
        (group != 0 && group != (gid_t) -1)) {
        errno = EPERM;
        return -1;
    }

    return 0;
}

/* Tenok has no symbolic link for this to differ on */
int lchown(const char *pathname, uid_t owner, gid_t group)
{
    return chown(pathname, owner, group);
}

/* Root is in the one group there is, never a supplementary one */
int getgroups(int size, gid_t list[])
{
    return 0;
}
