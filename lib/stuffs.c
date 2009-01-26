#include <bbs.h>

// Returns the path of 'filename' under the home directory of 'userid'.
char *sethomefile(char *buf, const char *userid, const char *filename)
{
	sprintf(buf, "home/%c/%s/%s", toupper(userid[0]), userid, filename);
	return buf;
}

// Returns the path of board 'boardname'.
char *setbpath(char *buf, const char *boardname)
{
	strcpy(buf, "boards/");
	strcat(buf, boardname);
	return buf;
}

// Returns the path of DOT_DIR file under the directory of 'boardname'.
char *setwbdir(char *buf, const char *boardname)
{
	sprintf (buf, "boards/%s/" DOT_DIR, boardname);
	return buf;
}

// Returns the path of 'filename' under the directory of 'boardname'.
char *setbfile(char *buf, const char *boardname, const char *filename)
{
	sprintf(buf, "boards/%s/%s", boardname, filename);
	return buf;
}

// Returns the path of 'filename' under the mail directory of 'userid'.
char *setmfile(char *buf, const char *userid, const char *filename)
{
	sprintf(buf, "mail/%c/%s/%s", toupper(userid[0]), userid, filename);
	return buf;
}

// Returns the path of '.DIR' under the mail directory of 'userid'.
char *setmdir(char *buf, const char *userid)
{
	sprintf(buf, "mail/%c/%s/" DOT_DIR, toupper(userid[0]), userid);
	return buf;
}

// Maps whole file 'filename' to memory and locks it.
// File descriptor of the file is returned via 'ret_fd'.
// The size of the file is returned via 'size'.
// 'openflag' is passed to open().
// 'prot', 'flag' are passed to mmap().
// 'ret_ptr' is the starting address of mapped region.
// Returns 1 if OK, 0 on error.
int safe_mmapfile(const char *filename, int openflag, int prot, int flag,
		void **ret_ptr, size_t *size, int *ret_fd)
{
	int fd;
	struct stat st;

	fd = open(filename, openflag, 0600);
	if (fd < 0)
		return 0;
	// Return error if not a regular file or wrong size.
	if ((fstat(fd, &st) < 0)
			|| (!S_ISREG(st.st_mode))
			|| (st.st_size <= 0)) {
		close(fd);
		return 0;
	}

	// Map whole file to memory.
	*ret_ptr = mmap(NULL, st.st_size, prot, flag, fd, 0);
	if (!ret_fd) {
		close(fd);
	} else {
		*ret_fd = fd;
		flock(fd, LOCK_EX);
	}
	if (*ret_ptr == NULL)
		return 0;
	*size = st.st_size;
	return 1;
}

sigjmp_buf bus_jump;
void sigbus(int signo)
{
	siglongjmp(bus_jump, 1);
}
