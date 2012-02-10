#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fts.h>
#include <pkg.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "pkgcli.h"

void
usage_clean(void)
{
	fprintf(stderr, "usage: pkg clean [-n]\n");
}

int
exec_clean(int argc, char **argv)
{
	struct pkgdb *db = NULL;
	struct pkgdb_it *it = NULL;
	struct pkg *pkg = NULL;
	struct pkg *p = NULL;
	FTS *fts = NULL;
	FTSENT *ent = NULL;
	const char *cachedir;
	char *paths[2];
	char *repopath;
	bool to_delete;
	int retcode = 1;
	int ret;

	(void)argc;
	(void)argv;

	if (pkg_config_string(PKG_CONFIG_CACHEDIR, &cachedir) != EPKG_OK) {
		warnx("Cant get cachedir config entry");
		return 1;
	}

	paths[0] = __DECONST(char*, cachedir);
	paths[1] = NULL;

	if (pkgdb_open(&db, PKGDB_REMOTE) != EPKG_OK) {
		goto cleanup;
	}

	if ((fts = fts_open(paths, FTS_PHYSICAL, NULL)) == NULL) {
		warn("fts_open(%s)", cachedir);
		goto cleanup;
	}

	while ((ent = fts_read(fts)) != NULL) {
		const char *origin, *pkgrepopath;

		if (ent->fts_info != FTS_F)
			continue;

		repopath = ent->fts_path + strlen(cachedir);
		if (repopath[0] == '/')
			repopath++;

		if (pkg_open(&pkg, ent->fts_path, NULL) != EPKG_OK) {
			warnx("skipping %s", ent->fts_path);
			continue;
		}

		pkg_get(pkg, PKG_ORIGIN, &origin);
		it = pkgdb_rquery(db, origin, MATCH_EXACT, FIELD_ORIGIN, NULL);

		if (it == NULL) {
			warnx("skipping %s", ent->fts_path);
			continue;
		}

		ret = pkgdb_it_next(it, &p, PKG_LOAD_BASIC);
		to_delete = false;
		pkg_get(pkg, PKG_REPOPATH, &pkgrepopath);
		if (ret == EPKG_FATAL) {
			warnx("skipping %s", ent->fts_path);
			continue;
		} else if (ret == EPKG_END) {
			to_delete = true;
			printf("%s does not exist anymore, deleting\n", repopath);
		} else if (strcmp(repopath, pkgrepopath)) {
			printf("%s is out-of-date, deleting\n", repopath);
			to_delete = true;
		}

		if (to_delete == true) {
			if (unlink(ent->fts_path) != 0)
				warn("unlink(%s)", ent->fts_path);
		}

		pkgdb_it_free(it);
	}

	retcode = 0;

	cleanup:
	if (pkg != NULL)
		pkg_free(pkg);
	if (p != NULL)
		pkg_free(p);
	if (fts != NULL)
		fts_close(fts);
	if (db != NULL)
		pkgdb_close(db);

	return (retcode);
}
