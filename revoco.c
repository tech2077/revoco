/*
 * Simple hack to control the wheel of Logitech's MX-Revolution mouse.
 *
 * Requires hidraw.
 *
 * Written November 2006 by Matthew Skolaut - no copyrights.
 * Modified from revoco written November 2006 by E. Toernig's bonobo - no copyrights.
 *
 * Contact: Matthew Skolaut <tech2077@gmail.com>
 *
 * Discovered commands:
 * (all numbers in hex, FS=free-spinning mode, CC=click-to-click mode):
 *   6 byte commands send with report ID 10:
 *   01 80 56 z1 00 00	immediate FS
 *   01 80 56 z2 00 00	immediate CC
 *   01 80 56 03 00 00	FS when wheel is moved
 *   01 80 56 04 00 00	CC when wheel is moved
 *   01 80 56 z5 xx yy	CC and switch to FS when wheel is rotated at given
 *			speed; xx = up-speed, yy = down-speed
 *			(speed in something like clicks per second, 1-50,
 *			 0 = previously set speed)
 *   01 80 56 06 00 00	?
 *   01 80 56 z7 xy 00	FS with button x, CC with button y.
 *   01 80 56 z8 0x 00	toggle FS/CC with button x; same result as 07 xx 00.
 *
 * If z=0 switch temporary, if z=8 set as default after powerup.
 *
 * Button numbers:
 *   0 previously set button
 *   1 left button	(can't be used for mode changes)
 *   2 right button	(can't be used for mode changes)
 *   3 middle (wheel) button
 *   4 rear thumb button
 *   5 front thumb button
 *   6 find button
 *   7 wheel left tilt
 *   8 wheel right tilt
 *   9 side wheel forward
 *  11 side wheel backward
 *  13 side wheel pressed
 *
 * Many thanks to Andreas Schneider <anschneider@suse.de> who found
 * the codes to query the battery level and to initiate a reconnect.
 *
 * Christophe THOMAS <oxygen77@free.fr> told me, how to make revoco
 * work with the MX-5500 combo.
 *
 * Some minor changes are made by Artem Illarionoff <cz0@mail.ru> to make
 * battery/mode request work for MX-5500 combo.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;

#define streq(a,b)	(strcmp((a), (b)) == 0)
#define strneq(a,b,c)	(strncmp((a), (b), (c)) == 0)

#define LOGITECH		0x046d
#define MX_REVOLUTION	0xc51a	// version RR41.01_B0025
#define MX_REVOLUTION2	0xc525	// version RQR02.00_B0020
#define MX_REVOLUTION3	0xc526	// don't know which version this is
#define MX_REVOLUTION4	0xc52b	// Unifying Receiver (added 2015-05-30)
#define MX_REVOLUTION5	0xb007	// ??? R0019 (added 2015-05-30)
#define MX_5500			0xc71c	// keyboard/mouse combo - experimental

static u8 first_byte;

static void fatal(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "revoco: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);

	exit(1);
}

static int open_dev(char *path)
{
	char buf[128];
	int i, fd;
	struct hidraw_devinfo dinfo;

	for (i = 0; i < 16; ++i)
	{
		sprintf(buf, path, i);
		fd = open(buf, O_RDWR);
		if (fd >= 0)
		{
			if (ioctl(fd, HIDIOCGRAWINFO, &dinfo) == 0)
			{
#ifdef DEBUG
				printf("/dev/hidraw%d 0x%04hx:0x%04hx 0x%04hx:0x%04hx\n", i, dinfo.vendor, dinfo.product, LOGITECH, MX_REVOLUTION5);
#endif
				if (dinfo.vendor == (short)LOGITECH)
				{
					first_byte = 1;
					if (dinfo.product == (short)MX_REVOLUTION)
						return fd;
					if (dinfo.product == (short)MX_REVOLUTION2)
						return fd;
					if (dinfo.product == (short)MX_REVOLUTION3)
						return fd;
					if (dinfo.product == (short)MX_REVOLUTION4)
						return fd;
					if (dinfo.product == (short)MX_REVOLUTION5)
						return fd;
					if (dinfo.product == (short)MX_5500)
					{
						first_byte = 2;
						return fd;
					}
				}
			}
			close(fd);
		}
	}
	return -1;
}

static void init_dev(int fd)
{
	if (fcntl(fd, F_SETFL, O_RDWR) == -1)
		printf("fcntl(O_NONBLOCK): %s\n", strerror(errno));
}

static void close_dev(int fd)
{
	close(fd);
}

static void send_report(int fd, u8 id, const u8 *buf, int n)
{
	int i, res;
	u8 *send_buf = (u8*) malloc(n+1);
	send_buf[0] = id;

	for(i = 1; i < n+1; i++) {
		send_buf[i] = buf[i-1];
	}

	res = write(fd, send_buf, n);
	free(send_buf);

	if (res < 0) {
		printf("Error: %d\n", errno);
		perror("write");
	}
}

static void query_report(int fd, u8 id, u8 *buf, int n)
{
	int res;
	res = read(fd, buf, n);
	if (res < 0) {
		perror("read");
	}
}

static void mx_cmd(int fd, u8 b1, u8 b2, u8 b3)
{
	u8 buf[6] = { first_byte, 0x80, 0x56, b1, b2, b3 };

	send_report(fd, 0x10, buf, 6);
}

static int mx_query(int fd, u8 b1, u8 *res)
{
	u8 buf[6] = { first_byte, 0x81, b1, 0, 0, 0 };
	u8 shifted_res[7];
	int i;

	send_report(fd, 0x10, buf, 6);
	query_report(fd, 0x10, shifted_res, 7);

	for (int i = 0; i < 6; i++)
		res[i] = shifted_res[i+1];

	if ((
		(res[0] != 0x02 && res[0] != 0x01 && res[0] != 0x00) ||
		res[1]  != 0x81 ||
		(res[2] != 0xb1 && res[2] != 0x08)
	) && (
		(res[0] != 0x02 && res[0] != 0x01 && res[0] != 0x00) ||
		res[1]  != 0x81 ||
		(res[2] != 0x0d && res[2] != 0x08)
	))
	{
		printf("bad answer:");
		for (i = 0; i < 6; ++i)
		{
		  printf("%02X ", res[i]);
		}
		printf("\n");
		return 0;
	}
	return 1;
}

static char * onearg(char *str, char prefix, u8 *arg, int def, int min, int max)
{
	char *end;
	long n;

	*arg = def;

	if (*str == '\0')
		return str;

	if (*str != prefix)
		fatal("bad argument `%s': `%c' expected", str, prefix);

	n = strtol(++str, &end, 0);
	if (str != end)
	{
		*arg = n;
		if (n < min || n > max)
			fatal("argument `%.*s' out of range (%d-%d)", end - str, str, min, max);
	}
	return end;
}

static void twoargs(char *str, u8 *arg1, u8 *arg2, int def, int min, int max)
{
	char *p = str;

	p = onearg(p, '=', arg1, def, min, max);
	p = onearg(p, ',', arg2, *arg1, min, max);
	if (*p)
		fatal("malformed argument `%s'", str);
}

static int nargs(char *str, u8 *buf, int n, int def, int min, int max)
{
	char *p = str;
	int i = 0, del = '=';

	while (n--)
	{
		if (*p)
			i++;
		p = onearg(p, del, buf++, def, min, max);
		del = ',';
	}
	if (*p)
		fatal("malformed argument `%s'", str);
	return i;
}

static void configure(int handle, int argc, char **argv)
{
	int i;
	u8 arg1, arg2;

	for (i = 1; i < argc; ++i)
	{
		u8 perm = 0x80;
		char *cmd = argv[i];

		if (strneq(cmd, "temp-", 5))
			perm = 0, cmd += 5;

		if (streq(cmd, "free"))
		{
			mx_cmd(handle, perm + 1, 0, 0);
		}
		else if (streq(cmd, "click"))
		{
			mx_cmd(handle, perm + 2, 0, 0);
		}
		else if (strneq(cmd, "manual", 6))
		{
			twoargs(cmd + 6, &arg1, &arg2, 0, 0, 15);
			if (arg1 != arg2)
				mx_cmd(handle, perm + 7, arg1 * 16 + arg2, 0);
			else
				mx_cmd(handle, perm + 8, arg1, 0);
		}
		else if (strneq(cmd, "auto", 4))
		{
			twoargs(cmd + 4, &arg1, &arg2, 0, 0, 50);
			mx_cmd(handle, perm + 5, arg1, arg2);
		}
		else if (strneq(argv[i], "soft-free", 9))
		{
			twoargs(argv[i] + 9, &arg1, &arg2, 0, 0, 255);
			mx_cmd(handle, 3, arg1, arg2);
		}
		else if (strneq(argv[i], "soft-click", 10))
		{
			twoargs(argv[i] + 10, &arg1, &arg2, 0, 0, 255);
			mx_cmd(handle, 4, arg1, arg2);
		}
		else if (strneq(argv[i], "reconnect", 9))
		{
			static const u8 cmd[] = { 0xff, 0x80, 0xb2, 1, 0, 0 };

			twoargs(argv[i] + 9, &arg1, &arg2, 0, 0, 255);
			send_report(handle, 0x10, cmd, 6);
			printf("Reconnection initiated\n");
			printf(" - Turn off the mouse\n");
			printf(" - Press and hold the left mouse button\n");
			printf(" - Turn on the mouse\n");
			printf(" - Press the right button 5 times\n");
			printf(" - Release the left mouse button\n");
		}
		else if (strneq(argv[i], "mode", 4))
		{
			u8 buf[6];

			if (mx_query(handle, 0x08, buf))
			{
				if (buf[5] & 1)
					printf("click-by-click\n");
			else
				printf("free spinning\n");
			}
		}
		else if (strneq(argv[i], "battery", 7))
		{
			u8 buf[6];

			if (mx_query(handle, 0x0d, buf))
			{
				char str[32], *st;

				switch (buf[5])
				{
					case 0x30:	st = "running on battery";	break;
					case 0x50:	st = "charging";		break;
					case 0x90:	st = "fully charged";		break;
					default:	sprintf(st = str, "status %02x", buf[5]);
				}
				printf("battery level %d%%, %s\n", buf[3], st);
			}
		}

		/*** debug commands ***/
		else if (strneq(argv[i], "raw", 3))
		{
			u8 buf[256], n;

			n = nargs(argv[i] + 3, buf, 256, 0, 0, 255);
			send_report(handle, buf[0], buf+1, n-1);
		}
		else if (strneq(argv[i], "query", 5))
		{
			u8 buf[256], j;

			twoargs(argv[i] + 5, &arg1, &arg2, -1, 0, 255);
			if (arg1 == -1)
				arg1 = 0x10, arg2 = 6;
			query_report(handle, arg1, buf, arg2);

			printf("report %02x:", arg1);
			for (j = 0; j < arg2; ++j)
				printf(" %02x", buf[j]);
			printf("\n");
		}
		else if (strneq(argv[i], "sleep", 5))
		{
			twoargs(argv[i] + 5, &arg1, &arg2, 1, 0, 255);
			sleep(arg1);
		}
		else
			fatal("unknown option `%s'", argv[i]);
	}
}

static void usage(void)
{
	printf("Revoco v"VERSION" - Change the wheel behaviour of "
				    "Logitech's MX-Revolution mouse.\n\n");
	printf("Usage:\n");
	printf("  revoco free                      free spinning mode\n");
	printf("  revoco click                     click-to-click mode\n");
	printf("  revoco manual[=button[,button]]  manual mode change via button\n");
	printf("  revoco auto[=speed[,speed]]      automatic mode change (up, down)\n");
	printf("  revoco battery                   query battery status\n");
	printf("  revoco mode                      query scroll wheel mode\n");
	printf("  revoco reconnect                 initiate reconnection\n");
	printf("\n");
	printf("Prefixing the mode with 'temp-' (i.e. temp-free) switches the mode\n");
	printf("temporarily, otherwise it becomes the default mode after power up.\n");
	printf("\n");
	printf("Button numbers:\n");
	printf("  0 previously set button   7 wheel left tilt\n");
	printf("  3 middle (wheel button)   8 wheel right tilt\n");
	printf("  4 rear thumb button       9 thumb wheel forward\n");
	printf("  5 front thumb button     11 thumb wheel backward\n");
	printf("  6 find button            13 thumb wheel pressed\n");
	printf("\n");
	exit(0);
}

static void trouble_shooting(void)
{
	char *path;
	int fd;

	fd = open(path = "/dev/hidraw0", O_RDWR);
	if (fd == -1 && errno == ENOENT)
		fd = open(path = "/dev/usb/hidraw0", O_RDWR);

	if (fd != -1)
		fatal("No Logitech MX-Revolution"
		      "(%04x:%04x, %04x:%04x, %04x:%04x, %04x:%04x, or %04x:%04x) found.",
		      LOGITECH, MX_REVOLUTION,
		      LOGITECH, MX_REVOLUTION2,
		      LOGITECH, MX_REVOLUTION3,
		      LOGITECH, MX_REVOLUTION4,
		      LOGITECH, MX_REVOLUTION5
			  );

	if (errno == EPERM || errno == EACCES)
		fatal("No permission to access hidraw (%s-15)\n"
		"Try 'sudo revoco ...'", path);

	fatal("Device not found.'\n");
}

int main(int argc, char **argv)
{
	int handle;

	if (argc < 2)
		usage();
	if (argc > 1 && (streq(argv[1], "-h") || streq(argv[1], "--help")))
		usage();

	handle = open_dev("/dev/hidraw%d");
	if (handle == -1)
		trouble_shooting();

	init_dev(handle);

	configure(handle, argc, argv);

	close_dev(handle);
	exit(0);
}

/* EOF */
