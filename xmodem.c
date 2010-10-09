
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

int main(int argc, const char ** argv)
{
	int cpt = 1;
	int retry = 0;
	int oldcpt = 0;
	int pkt = 0;
	const char * output = argv[2];
	const char * input = argv[1];
	unsigned char send_buf[HEAD+PKT_LEN+CSUM];
	unsigned char recv_buf[16];

	struct timespec delta;
	struct termios options;

	signal(SIGINT, xmodem_interrupt);

	if(argc < 3) {
		printf("usage: %s <input file> <serial port>\n", argv[0]);
		exit(0);
	}

	printf("########################################\n");
	printf("send file %s on %s\n", argv[1], argv[2]);
	printf("########################################\n");
	serial = open(output, O_RDWR | O_NOCTTY | O_NDELAY);

	if(serial < 0) {
		printf("error opening '%s'\n",output);
		exit(-1);
	}

	fcntl(serial, F_SETFL, 0);

	tcgetattr(serial, &options);
	cfsetispeed(&options, B115200);
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	tcsetattr(serial, TCSANOW, &options);

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
				csum.checksum = xmodem_calculate_crc(csum.checksum, send_buf[i+3]);
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

