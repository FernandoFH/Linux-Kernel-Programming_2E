/*
 * ch11/userspc_cpuaffinity/userspc_cpuaffinity.c
 ***************************************************************
 * This program is part of the source code released for the book
 *  "Linux Kernel Programming" 2E
 *  (c) Author: Kaiwan N Billimoria
 *  Publisher:  Packt
 *  GitHub repository:
 *  https://github.com/PacktPublishing/Linux-Kernel-Programming_2E
 *
 * From: Ch 11 : CPU Scheduling, Part 2
 ****************************************************************
 * Brief Description:
 * A small *userspace* app to query and set the CPU affinity mask of any
 * given process or thread (via PID). If no PID is explicitly provided,
 * we just display the CPU mask of the calling process (this app).
 *
 * For details, please refer the book, Ch 11.
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sched.h>
#include "../../convenient.h"

static unsigned int numcores;

static inline void print_ruler_line(unsigned int len)
{
	int i;

	printf("       +");
	for (i=1; i<=len*4; i++) {
		if ((i%4) == 0)
			printf("+");
		else
			printf("-");
	}
	printf("\n");
}

/* 
 * disp_cpumask()
 * Print the provided CPU bitmask @cpumask (along with the 'ruler' lines),
 * for a max of @ncores-1 CPU cores, in (a more intuitive) right-to-left order.
 */
static void disp_cpumask(pid_t pid, cpu_set_t *cpumask, unsigned int ncores)
{
	int i;
	char tmpbuf[128];

	printf("CPU affinity mask for PID %d:\n", pid);
	/* We don't use our snprintf_lkp() wrapper here as that will require
	 * linking into the klib.c file... here, as this is a userspace app,
	 * we pedantically avoid it. Don't have any such qualms in production;
	 * simply use it and link appropriately.
	 */
	snprintf(tmpbuf, 127, "ps -A |awk '$1 == %d {print $0}'", pid);
	//snprintf_lkp(tmpbuf, 127, "ps -A |awk '$1 == %d {print $0}'", pid);
	if (system(tmpbuf) == -1)
		fprintf(stderr, "Warning: %s():system(3) (to show ps output)"
			" failed\n", __func__);

	print_ruler_line(ncores);

	printf("core#  |");
	for (i=ncores-1; i>=0; i--)
		printf("%3d|", i); /* [1-999] cores; should do :-)
		                    * If not ok, you're too high-end for us :D
				    */
	printf("\n");
	print_ruler_line(ncores);

	printf("cpumask|");
	for (i=ncores-1; i>=0; i--)
		printf("%3u|", CPU_ISSET(i, cpumask));
	printf("\n");
	print_ruler_line(ncores);
}

static int query_cpu_affinity(pid_t pid)
{
	cpu_set_t cpumask;

	CPU_ZERO(&cpumask);
	if (sched_getaffinity(pid, sizeof(cpu_set_t), &cpumask) < 0) {
		perror("sched_getaffinity() failed");
		return -1;
	}
	disp_cpumask(pid, &cpumask, numcores);

	return 0;
}

static int set_cpu_affinity(pid_t pid, unsigned long bitmask)
{
	cpu_set_t cpumask;
	int i;

	printf("\nSetting CPU affinity mask for PID %d now...\n", pid);
	CPU_ZERO(&cpumask);

	/* Iterate over the given bitmask, setting CPU bits as required */
	for (i=0; i<sizeof(unsigned long)*8; i++) {
		/* printf("bit %ld: %d\n", i, (bitmask >> i) & 1); */
		if ((bitmask >> i) & 1)
			CPU_SET(i, &cpumask);
	}

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpumask) < 0) {
		perror("sched_setaffinity() failed");
		return -1;
	}
	disp_cpumask(pid, &cpumask, numcores);

	return 0;
}

int main (int argc, char **argv)
{
	pid_t pid = getpid();
	unsigned long new_cpumask = 0x0;

	if (argc > 1 && 
		((!strncmp(argv[1], "--help", 6)) || (!strncmp(argv[1], "-h", 2)))) {
		fprintf(stderr, "Usage: %s [PID] [new-CPU-mask]\n"
		"(If using the optional params, you must at least pass"
		" the process PID;\nwe (attempt to) set CPU affinity only if"
		" new-CPU-mask is passed)\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	numcores = get_nprocs();
	printf("Detected %d CPU cores system-wide\n", numcores);
	/* we say 'for this process ...' as the number of cores available to processes can
	 * change within a container
	 */

	if (argc >= 2)
		pid = atoi(argv[1]);

	query_cpu_affinity(pid);
	if (argc == 3) {
		new_cpumask = strtoul(argv[2], 0, 0);
		set_cpu_affinity(pid, new_cpumask);
	}

	exit(EXIT_SUCCESS);
}
