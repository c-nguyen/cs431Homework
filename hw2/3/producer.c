#define _GNU_SOURCE
#include <stdio.h>			// printf
#include <stdlib.h>			// exit
#include <string.h>
#include <errno.h>			// errno
#include <fcntl.h>			// 0_CREAT, 0_EXEC
#include <unistd.h>
#include <sys/types.h>		// key_t, sem_t
#include <semaphore.h>		// sem_open, sem_destroy, sem_wait
#include <sys/stat.h>
#include <sys/mman.h>

#define MAX 10

sem_t *sem;  // semaphore
int fd, *map;

int produce() {
	struct stat st;
	off_t len_file, len;
	char src[1] = "p";
	int i;

	fstat(fd, &st);
	len_file = st.st_size;

	/* Produce items in memory mapped file */
	if (len_file < MAX * sizeof(int)) {
		for (i = 0; i < MAX; i++) {
			/* Do down on semaphore */
			sem_wait(sem);

			len = len_file;
			len_file += strlen(src);

			if (ftruncate(fd, len_file) != 0) {
				perror("p - Can't extend file.\n");
				exit(1);
			}

			/* Produce item */
			memcpy(map+len, src, len_file - len);

			//printf("Produce: %d\n", map[i]);
			printf("Produced: %d\n", i);

			/* Do up on semaphore */
			sem_post(sem);
		}
	}

	return 0;
}

int main(void) {
	key_t sem_key;      // semaphore key
	time_t start, end;  // used as timer
	int terminator = 1; // change to 0 in while loop when done
	double elapsed;     // elapsed time

	/* Create semaphore */
	sem = sem_open("semaphoreV", O_CREAT, 0666, 1);
	sem_unlink("semaphoreV");

	/* Open file for reading/writing */
	fd = open("sharedfile.bin", O_RDWR, 0666);
	if (fd == -1) {
		perror("Could not open file.\n");
		exit(1);
	}

	/* Map file */
	map = mmap(0, MAX * sizeof(int), PROT_READ | PROT_WRITE,
		MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		close(fd);
		perror("Couldn't map file.\n");
		exit(1);
	}

	start = time(NULL);
	while(terminator) {
		end = time(NULL);
		elapsed = end - start;
		if (elapsed < 5) {
			produce();
			sleep(1);
		} else {
			terminator = 0;
		}
	}

	/* Sync file changes */
	if (msync(map, MAX * sizeof(int), MS_SYNC) < 0) {
		perror("Can't sync file.\n");
	}

	/* Deallocate mapped memory */
	if (munmap(map, MAX * sizeof(int)) == -1) {
		perror("Can't unmap the file.\n");
	}

	/* Close file */
	close(fd);

	return 0;
}