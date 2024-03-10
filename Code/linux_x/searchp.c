/*
 * Written by Udo Munk <udo@umserver.umnet.de>
 *
 * I don't care what you do with this and I'm not responsible for
 * anything which might happen if you use this. This code is provided
 * AS IS and there are no guarantees, none.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * See if file is in any directory in the PATH environment variable.
 * If yes return a complete pathname, if not found just return the filename.
 */
char *searchpath(char *file) {
	char		*path;
	char		*dir;
	static char	b[2048];
	struct stat	s;
	char		pb[2048];

	/* get PATH, if not set just return filename, might be in cwd */
	if ((path = getenv("PATH")) == NULL)
		return(file);

	/* we have to do this because strtok() is destructive */
	strcpy(pb, path);

	/* get first directory */
	dir = strtok(pb, ":");

	/* loop over the directories in PATH and see if the file is there */
	while (dir) {
		/* build filename from directory/filename */
		strcpy(b, dir);
		strcat(b, "/");
		strcat(b, file);
		if (stat(b, &s) == 0) {
			return(b);	/* yep, there it is */
		}

		/* get next directory */
		dir = strtok(NULL, ":");
	}

	/* hm, not found, just return filename, again, might be in cwd */
	return(file);
}
