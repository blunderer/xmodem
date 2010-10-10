/*
 * Copyright 2010 Tristan Lelong
 * 
 * This file is part of xmodem.
 * 
 * xmodem is free software: you can redistribute it and/or modify it under 
 * the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * xmodem is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with xmodem. If not, see http://www.gnu.org/licenses/.
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>


#define HEAD		3
#define CSUM		2
#define PKT_LEN 	128
#define CRCXMODEM	0x0000

int serial = 0;
int xmodem_running = 1;

void xmodem_interrupt(int sign)
{
	xmodem_running = 0;
	close(serial);

	exit(0);
}

int xmodem_calculate_crc(unsigned int crc, unsigned char data)
{
	crc  = (unsigned char)(crc >> 8) | (crc << 8);
	crc ^= data;
	crc ^= (unsigned char)(crc & 0xff) >> 4;
	crc ^= (crc << 8) << 4;
	crc ^= ((crc & 0xff) << 4) << 1;
	return crc;
}

void xmodem_usage(const char * argv0)
{
	printf("usage: %s [-m <mode> -s <speed>] -p <serial port> -i <input file>\n", argv0);
	printf("options are:\n");
	printf("\t-s <baud>: 1200 1800 2400 4800 9600 19200 38400 57600 230400 115200. default is 115200\n");
	printf("\t-m <mode>: [5-8] [N|E|O] [1-2] ex: 7E2. default is 8N1 \n");
	printf("\t-p <serial port>\n");
	printf("\t-i <file>\n");

	exit(0);
}

void xmodem_configure_serial(int port, int speed, char * mode)
{
	struct termios options;

	tcgetattr(serial, &options);
	options.c_cflag |= (CLOCAL | CREAD);

	switch(speed) {
		case 1200:
			cfsetispeed(&options, B1200);
		break;
		case 1800:
			cfsetispeed(&options, B1800);
		break;
		case 2400:
			cfsetispeed(&options, B2400);
		break;
		case 4800:
			cfsetispeed(&options, B4800);
		break;
		case 9600:
			cfsetispeed(&options, B9600);
		break;
		case 19200:
			cfsetispeed(&options, B19200);
		break;
		case 38400:
			cfsetispeed(&options, B38400);
		break;
		case 57600:
			cfsetispeed(&options, B57600);
		break;
		case 230400:
			cfsetispeed(&options, B230400);
		break;
		case 115200:
		default:
			cfsetispeed(&options, B115200);
		break;
	}

	options.c_cflag &= ~CSIZE;
	switch(mode[0]) {
		case '5':
			options.c_cflag |= CS5;
		break;
		case '6':
			options.c_cflag |= CS6;
		break;
		case '7':
			options.c_cflag |= CS7;
		break;
		case '8':
		default:
			options.c_cflag |= CS8;
		break;
	}

	switch(mode[1]) {
		case 'E':
			options.c_cflag |= PARENB;
			options.c_cflag &= ~PARODD;
		break;
		case 'O':
			options.c_cflag |= PARENB;
			options.c_cflag |= PARODD;
		break;
		case 'N':
			options.c_cflag &= ~PARENB;
		default:
		break;
	}

	switch(mode[2]) {
		case '1':
			options.c_cflag &= ~CSTOPB;
		break;
		case '2':
		default:
			options.c_cflag |= CSTOPB;
		break;
	}

	tcsetattr(serial, TCSANOW, &options);
}

int main(int argc, char ** argv)
{
	int opt = 0;
	int cpt = 1;
	int retry = 0;
	int oldcpt = 0;
	int pkt = 0;
	int speed = 115200;

	char * mode = "8N1";
	const char * output = NULL;
	const char * input = NULL;
	unsigned char send_buf[HEAD+PKT_LEN+CSUM];
	unsigned char recv_buf[16];

	struct timespec delta;

	signal(SIGINT, xmodem_interrupt);

	if(argc < 3) {
		xmodem_usage(argv[0]);
	}

	while((opt = getopt(argc, argv, "i:s:m:p:")) != -1) {
		switch (opt) {
			case 'i':
				input = optarg;
				break;
			case 'p':
				output = optarg;
				break;
			case 's':
				speed = atoi(optarg);
				break;
			case 'm':
				mode = optarg;
				break;
			default:
				printf("unknown option '%c'\n", opt);
				xmodem_usage(argv[0]);
				break;
		}
	}

	if(!input) {
		printf("missing input file %d\n", argc);
		xmodem_usage(argv[0]);
	}
	if(!output) {
		printf("missing serial port\n");
		xmodem_usage(argv[0]);
	}

	printf("########################################\n");
	printf("send file %s on %s\n", input, output);
	printf("########################################\n");
	serial = open(output, O_RDWR | O_NOCTTY | O_NDELAY);

	if(serial < 0) {
		printf("error opening '%s'\n",output);
		exit(-1);
	}

	fcntl(serial, F_SETFL, 0);
	xmodem_configure_serial(serial, speed, mode);

	FILE * fin = fopen(input, "r");
	if(fin == NULL) {
		printf("error opening '%s'\n",input);
		close(serial);
		exit(-1);
	}

	fseek(fin, 0, SEEK_END);
	int nb = ftell(fin);
	fseek(fin, 0, SEEK_SET);
	pkt = nb/PKT_LEN+((nb%PKT_LEN)?1:0); 
	printf("-- %d pkt to be sent\n", pkt);

	printf("-- wait for synchro\n"); 
	while(xmodem_running) {
		delta.tv_sec = 0;
		delta.tv_nsec = 1000;
		nanosleep(&delta, NULL);
		int size = read(serial, recv_buf, 1);
		if(size == 1) {
			if(recv_buf[0] == 'C') {
				break;
			}
		}
	}
	printf("-- got 0x%02x: start transfert\n", recv_buf[0]);

	while(!feof(fin)) {
		int i;
		union  {
			unsigned int checksum;
			unsigned char val[4];
		} csum;
		int size_in = PKT_LEN;
		int size_out = 0;

		delta.tv_sec = 0;
		delta.tv_nsec = 1000;

		nanosleep(&delta, NULL);

		if(oldcpt < cpt) {
			retry = 0;
			memset(send_buf, 0x1A, sizeof(send_buf));

			send_buf[0] = 0x01;
			send_buf[1] = cpt;
			send_buf[2] = 255 - cpt;

			size_in = fread(send_buf+3, 1, PKT_LEN, fin);

			csum.checksum = CRCXMODEM;
			for(i = 0; i < PKT_LEN; i++) {
				csum.checksum = xmodem_calculate_crc(csum.checksum, send_buf[i+HEAD]);
			}

			send_buf[PKT_LEN+HEAD] = csum.val[1];
			send_buf[PKT_LEN+HEAD+1] = csum.val[0];

			oldcpt++;
		}

		retry++;

		size_out = write(serial, send_buf, sizeof(send_buf));

		if(size_out != sizeof(send_buf)) {
			printf("ERROR: sent %d/%d bytes\n", size_out, sizeof(send_buf));
			exit(-1);
		} else {
			int total = 0;
			while(xmodem_running) {
				int size = read(serial, recv_buf, 1);
				total += size;
				if(total == 1) {
					if(recv_buf[0] == 0x15) {
						printf("ERROR[%02d] NACK\n", retry);
						break;
					} else if(recv_buf[0] == 0x06) {
						printf("OK[%02d/%02d]\r", cpt, pkt);
						fflush(stdout);
						cpt++;
						break;
					} else {
						printf("WARN: received 0x%02x ", recv_buf[0]);
					}
				} else {
					if(total % 20 == 1) {
						printf("WARN: received ");
					}
					printf("0x%02x ", recv_buf[0]);
					if(total % 20 == 0) {
						printf("\n");
					}
				}
			}
		}
	}
	printf("\n");

	printf("-- end of file\n"); 
	send_buf[0] = 0x04;
	write(serial, send_buf, 1);

	printf("-- EOT sent\n"); 

	int size_ans = read(serial, recv_buf, 1);
	if(size_ans == 1) {
		if(recv_buf[0] == 0x15) {
			printf("ERROR NACK\n");
		} else if(recv_buf[0] == 0x06) {
			printf("OK <EOT> ACKed\n"); 
		} else {
			printf("WARN: unexpected received 0x%02x ", recv_buf[0]);
		}
	}
	
	fclose(fin);
	
	printf("-- done\n"); 

	printf("########################################\n");
	printf("Transfert complete. Press ^C to exit\n");
	printf("########################################\n");

	while(xmodem_running) {
		int size = read(serial, send_buf, sizeof(send_buf));
		if(size > 0) {
			write(1, send_buf, size);
		}
	}

	close(serial);

	return 0;
}

