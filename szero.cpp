#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define SECTOR_SIZE (128*1024)

int main(int argc, char *argv[]) 
{
	int rc = -1;
	int readErr = 0;

	if ( argc < 2 )
	{
		printf("Must specify device on command line\n");
		return 1;
	}

	setbuf(stdout,NULL);
	printf("Reading device %s\n", argv[1]);
	uint8_t *sector = (uint8_t*) aligned_alloc(SECTOR_SIZE, SECTOR_SIZE);
	uint8_t * zero_sector = (uint8_t*) aligned_alloc(SECTOR_SIZE, SECTOR_SIZE);
	memset(zero_sector, 0, SECTOR_SIZE);

	int fd=open( argv[1], O_RDWR );

	uint64_t length = lseek(fd, 0, SEEK_END);
	uint64_t totalLength = length;
	printf("Total of %ju bytes\n", length );
	lseek(fd,0,SEEK_SET);


	posix_fadvise( fd, 0, length, POSIX_FADV_SEQUENTIAL );

	uint64_t overwrites = 0;
	time_t start = time(NULL);
	uint32_t updateRate = 5;
	time_t nextUpdate = start + updateRate;
	while ( length > 0 )
	{
		int sz = SECTOR_SIZE;
		if ( sz > length) sz = length;

		bool bNonZero;
		do
		{
			bNonZero = false;
			rc = read(fd, sector, sz);
			if (rc < 0)
			{
				++readErr;
				lseek(fd,sz,SEEK_CUR);	
				updateRate = 1;
			  	break;	
				//printf("\n\n\nsector read error: %s\n\n", strerror(errno));
				//return 3;
			}
			for ( int j = 0; j < SECTOR_SIZE; j ++ )
			{
				if ( sector[j] != 0 )
				{
					bNonZero = true;
					break;
				}
			}
			if ( bNonZero )
			{
				lseek( fd, -sz, SEEK_CUR );
				rc = write( fd, zero_sector, sz );
				if (rc < 0)
				{
					printf("\n\n\nsector write error: %s\n\n", strerror(errno));
					//TODO don't fail on this
					return 4;
				}
				overwrites += sz;
				lseek( fd, -sz, SEEK_CUR );
			}
		} while ( bNonZero );
		length -= sz;
		time_t now = time(NULL);
		if ( now >= nextUpdate || length == 0) 
		{
			uint64_t seconds = (now-start);
			uint64_t KBps = (totalLength - length)/(seconds>0?seconds:1)/1024;
			uint64_t secondsLeft = length / 1024 / KBps;	
			printf("%02ju:%02ju elapsed|%02ju:%02ju est. remaining|%3.2f%% complete|%.2f MB/s|%.2f MB overwritten|%d read errors\r",seconds/60,seconds%60,secondsLeft/60,secondsLeft%60,(1.0-length/(float)totalLength)*100.0, KBps/1024.0, overwrites/(1024*1024.0),readErr);
			nextUpdate += updateRate;
		}
	}
	free(sector);
	free(zero_sector);
	printf("\n\nSYNCing\n");
	close(fd);
	sync();
	if (readErr)
		printf("Scan completed with %d read errors\n", readErr);
	else
		printf("Scan completed successfully\n");

	return 0;
}
