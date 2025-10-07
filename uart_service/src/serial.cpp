/*
 *  Copyright (C) 2012 Libelium Comunicaciones Distribuidas S.L.
 *  http://www.libelium.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Version 2.4 (For Raspberry Pi 2)
 *  Author: Sergio Martinez, Ruben Martin
 */

#include "serial.hpp"

struct bcm2835_peripheral gpio = {GPIO_BASE2};
struct bcm2835_peripheral bsc_rev1 = {IOBASE + 0X205000};
struct bcm2835_peripheral bsc_rev2 = {IOBASE + 0X804000};
struct bcm2835_peripheral bsc0;
volatile uint32_t *bcm2835_bsc01;

void *spi0 = MAP_FAILED;
static uint8_t *spi0Mem = NULL;

pthread_t idThread2;
pthread_t idThread3;
pthread_t idThread4;
pthread_t idThread5;
pthread_t idThread6;
pthread_t idThread7;
pthread_t idThread8;
pthread_t idThread9;
pthread_t idThread10;
pthread_t idThread11;
pthread_t idThread12;
pthread_t idThread13;

timeval start_program, end_point;

/*********************************
 *                               *
 * SerialPi Class implementation *
 * ----------------------------- *
 *********************************/

/******************
 * Public methods *
 ******************/

static int getBoardRev()
{

    FILE *cpu_info;
    char line[120];
    char *c, finalChar;
    static int rev = 0;

    if (REV != 0)
        return REV;

    if ((cpu_info = fopen("/proc/cpuinfo", "r")) == NULL)
    {
        fprintf(stderr, "Unable to open /proc/cpuinfo. Cannot determine board reivision.\n");
        exit(1);
    }

    while (fgets(line, 120, cpu_info) != NULL)
    {
        if (strncmp(line, "Revision", 8) == 0)
            break;
    }

    fclose(cpu_info);

    if (line == NULL)
    {
        fprintf(stderr, "Unable to determine board revision from /proc/cpuinfo.\n");
        exit(1);
    }

    for (c = line; *c; ++c)
        if (isdigit(*c))
            break;

    if (!isdigit(*c))
    {
        fprintf(stderr, "Unable to determine board revision from /proc/cpuinfo\n");
        fprintf(stderr, "  (Info not found in: %s\n", line);
        exit(1);
    }

    finalChar = c[strlen(c) - 2];

    if ((finalChar == '2') || (finalChar == '3'))
    {
        bsc0 = bsc_rev1;
        return 1;
    }
    else
    {
        bsc0 = bsc_rev2;
        return 2;
    }
}

// Constructor
SerialPi::SerialPi()
{
    REV = getBoardRev();
    serialPort = "/dev/ttyS0";
    //	serialPort = "/dev/ttyAMA0";
    timeOut = 1000;
}

// Sets the data rate in bits per second (baud) for serial data transmission
void SerialPi::begin(int serialSpeed)
{

    switch (serialSpeed)
    {
    case 50:
        speed = B50;
        break;
    case 75:
        speed = B75;
        break;
    case 110:
        speed = B110;
        break;
    case 134:
        speed = B134;
        break;
    case 150:
        speed = B150;
        break;
    case 200:
        speed = B200;
        break;
    case 300:
        speed = B300;
        break;
    case 600:
        speed = B600;
        break;
    case 1200:
        speed = B1200;
        break;
    case 1800:
        speed = B1800;
        break;
    case 2400:
        speed = B2400;
        break;
    case 9600:
        speed = B9600;
        break;
    case 19200:
        speed = B19200;
        break;
    case 38400:
        speed = B38400;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
    default:
        speed = B230400;
        break;
    }

    if ((sd = open(serialPort, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK)) == -1)
    {
        fprintf(stderr, "Unable to open the serial port %s - \n", serialPort);
        exit(-1);
    }

    fcntl(sd, F_SETFL, O_RDWR);

    tcgetattr(sd, &options);
    cfmakeraw(&options);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;

    tcsetattr(sd, TCSANOW, &options);

    ioctl(sd, TIOCMGET, &status);

    status |= TIOCM_DTR;
    status |= TIOCM_RTS;

    ioctl(sd, TIOCMSET, &status);

    usleep(10000);
}

void SerialPi::println(const char *message)
{
    const char *newline = "\r\n";
    char *msg = NULL;
    asprintf(&msg, "%s%s", message, newline);
    write(sd, msg, strlen(msg));
}

/* Writes binary data to the serial port. This data is sent as a byte
 * Returns: number of bytes written */
int SerialPi::send(unsigned char message)
{
    write(sd, &message, 1);
    return 1;
}

/* Get the numberof bytes (characters) available for reading from
 * the serial port.
 * Return: number of bytes avalable to read */
int SerialPi::available()
{
    int nbytes = 0;
    if (ioctl(sd, FIONREAD, &nbytes) < 0)
    {
        fprintf(stderr, "Failed to get byte count on serial.\n");
        raise(SIGINT);
    }
    return nbytes;
}

/* Reads 1 byte of incoming serial data
 * Returns: first byte of incoming serial data available */
char SerialPi::receive(int timeoutInMs)
{
	struct pollfd pfd;
    pfd.fd = sd;
    pfd.events = POLLIN;
	int ret = poll(&pfd, 1, timeoutInMs);
    if (ret == -1)
	{
        return 0;
	}
	else if (ret == 0)
	{
		return 26; // ^Z --> timeout
    }

	if (pfd.revents & POLLIN)
	{
		unsigned char c;
		ssize_t n = read(sd, &c, 1);
		if (n == 0) return 4; // EOT
		return c;
    }
	return 4; // EOT
}

/* returns the first valid (long) integer value from the current position.
 * initial characters that are not digits (or the minus sign) are skipped
 * function is terminated by the first character that is not a digit. */
long SerialPi::parseInt()
{
    bool isNegative = false;
    long value = 0;
    char c;

    // Skip characters until a number or - sign found
    do
    {
        c = peek();
        if (c == '-')
            break;
        if (c >= '0' && c <= '9')
            break;
        read(sd, &c, 1); // discard non-numeric
    } while (1);

    do
    {
        if (c == '-')
            isNegative = true;
        else if (c >= '0' && c <= '9') // is c a digit?
            value = value * 10 + c - '0';
        read(sd, &c, 1); // consume the character we got with peek
        c = peek();

    } while (c >= '0' && c <= '9');

    if (isNegative)
        value = -value;
    return value;
}

float SerialPi::parseFloat()
{
    boolean isNegative = false;
    boolean isFraction = false;
    long value = 0;
    char c;
    float fraction = 1.0;

    // Skip characters until a number or - sign found
    do
    {
        c = peek();
        if (c == '-')
            break;
        if (c >= '0' && c <= '9')
            break;
        read(sd, &c, 1); // discard non-numeric
    } while (1);

    do
    {
        if (c == '-')
            isNegative = true;
        else if (c == '.')
            isFraction = true;
        else if (c >= '0' && c <= '9')
        { // is c a digit?
            value = value * 10 + c - '0';
            if (isFraction)
                fraction *= 0.1;
        }
        read(sd, &c, 1); // consume the character we got with peek
        c = peek();
    } while ((c >= '0' && c <= '9') || (c == '.' && isFraction == false));

    if (isNegative)
        value = -value;
    if (isFraction)
        return value * fraction;
    else
        return value;
}

// Returns the next byte (character) of incoming serial data without removing it from the internal serial buffer.
char SerialPi::peek()
{
    // We obtain a pointer to FILE structure from the file descriptor sd
    FILE *f = fdopen(sd, "r+");
    // With a pointer to FILE we can do getc and ungetc
    c = getc(f);
    ungetc(c, f);
    return c;
}

// Remove any data remaining on the serial buffer
void SerialPi::flush()
{
    while (available())
    {
        read(sd, &c, 1);
    }
}

/* Sets the maximum milliseconds to wait for serial data when using SerialPi::readBytes()
 * The default value is set to 1000 */
void SerialPi::setTimeout(long millis)
{
    timeOut = millis;
}

// Disables serial communication
void SerialPi::end()
{
    close(sd);
}

/********** FUNCTIONS OUTSIDE CLASSES **********/

void SerialPi::delayMicroseconds(long micros)
{
    if (micros > 100)
    {
        struct timespec tim, tim2;
        tim.tv_sec = 0;
        tim.tv_nsec = micros * 1000;

        if (nanosleep(&tim, &tim2) < 0)
        {
            fprintf(stderr, "Nano sleep system call failed \n");
            exit(1);
        }
    }
    else
    {
        struct timeval tNow, tLong, tEnd;

        gettimeofday(&tNow, NULL);
        tLong.tv_sec = micros / 1000000;
        tLong.tv_usec = micros % 1000000;
        timeradd(&tNow, &tLong, &tEnd);

        while (timercmp(&tNow, &tEnd, <))
            gettimeofday(&tNow, NULL);
    }
}

// Configures the specified pin to behave either as an input or an output
void SerialPi::pinMode(int pin, Pinmode mode)
{
    if (mode == OUTPUT)
    {
        switch (pin)
        {
        case 4:
            GPFSEL0 &= ~(7 << 12);
            GPFSEL0 |= (1 << 12);
            break;
        case 6:
            GPFSEL0 &= ~(7 << 18);
            GPFSEL0 |= (1 << 18);
            break;
        case 8:
            GPFSEL0 &= ~(7 << 24);
            GPFSEL0 |= (1 << 24);
            break;
        case 9:
            GPFSEL0 &= ~(7 << 27);
            GPFSEL0 |= (1 << 27);
            break;
        case 10:
            GPFSEL1 &= ~(7 << 0);
            GPFSEL1 |= (1 << 0);
            break;
        case 11:
            GPFSEL1 &= ~(7 << 3);
            GPFSEL1 |= (1 << 3);
            break;
        case 14:
            GPFSEL1 &= ~(7 << 12);
            GPFSEL1 |= (1 << 12);
            break;
        case 17:
            GPFSEL1 &= ~(7 << 21);
            GPFSEL1 |= (1 << 21);
            break;
        case 18:
            GPFSEL1 &= ~(7 << 24);
            GPFSEL1 |= (1 << 24);
            break;
        case 21:
            GPFSEL2 &= ~(7 << 3);
            GPFSEL2 |= (1 << 3);
            break;
        case 27:
            GPFSEL2 &= ~(7 << 21);
            GPFSEL2 |= (1 << 21);
            break;
        case 22:
            GPFSEL2 &= ~(7 << 6);
            GPFSEL2 |= (1 << 6);
            break;
        case 23:
            GPFSEL2 &= ~(7 << 9);
            GPFSEL2 |= (1 << 9);
            break;
        case 24:
            GPFSEL2 &= ~(7 << 12);
            GPFSEL2 |= (1 << 12);
            break;
        case 25:
            GPFSEL2 &= ~(7 << 15);
            GPFSEL2 |= (1 << 15);
            break;
        }
    }
    else if (mode == INPUT)
    {
        switch (pin)
        {
        case 4:
            GPFSEL0 &= ~(7 << 12);
            break;
        case 6:
            GPFSEL0 &= ~(7 << 18);
            break;
        case 8:
            GPFSEL0 &= ~(7 << 24);
            break;
        case 9:
            GPFSEL0 &= ~(7 << 27);
            break;
        case 10:
            GPFSEL1 &= ~(7 << 0);
            break;
        case 11:
            GPFSEL1 &= ~(7 << 3);
            break;
        case 14:
            GPFSEL1 &= ~(7 << 12);
            break;
        case 17:
            GPFSEL1 &= ~(7 << 21);
            break;
        case 18:
            GPFSEL1 &= ~(7 << 24);
            break;
        case 21:
            GPFSEL2 &= ~(7 << 3);
            break;
        case 27:
            GPFSEL2 &= ~(7 << 3);
            break;
        case 22:
            GPFSEL2 &= ~(7 << 6);
            break;
        case 23:
            GPFSEL2 &= ~(7 << 9);
            break;
        case 24:
            GPFSEL2 &= ~(7 << 12);
            break;
        case 25:
            GPFSEL2 &= ~(7 << 15);
            break;
        }
    }
}

// Write a HIGH or a LOW value to a digital pin
void SerialPi::digitalWrite(int pin, int value)
{
    if (value == HIGH)
    {
        switch (pin)
        {
        case 4:
            GPSET0 = BIT_4;
            break;
        case 6:
            GPSET0 = BIT_6;
            break;
        case 8:
            GPSET0 = BIT_8;
            break;
        case 9:
            GPSET0 = BIT_9;
            break;
        case 10:
            GPSET0 = BIT_10;
            break;
        case 11:
            GPSET0 = BIT_11;
            break;
        case 14:
            GPSET0 = BIT_14;
            break;
        case 17:
            GPSET0 = BIT_17;
            break;
        case 18:
            GPSET0 = BIT_18;
            break;
        case 21:
            GPSET0 = BIT_21;
            break;
        case 27:
            GPSET0 = BIT_27;
            break;
        case 22:
            GPSET0 = BIT_22;
            break;
        case 23:
            GPSET0 = BIT_23;
            break;
        case 24:
            GPSET0 = BIT_24;
            break;
        case 25:
            GPSET0 = BIT_25;
            break;
        }
    }
    else if (value == LOW)
    {
        switch (pin)
        {
        case 4:
            GPCLR0 = BIT_4;
            break;
        case 6:
            GPCLR0 = BIT_6;
            break;
        case 8:
            GPCLR0 = BIT_8;
            break;
        case 9:
            GPCLR0 = BIT_9;
            break;
        case 10:
            GPCLR0 = BIT_10;
            break;
        case 11:
            GPCLR0 = BIT_11;
            break;
        case 14:
            GPCLR0 = BIT_14;
            break;
        case 17:
            GPCLR0 = BIT_17;
            break;
        case 18:
            GPCLR0 = BIT_18;
            break;
        case 21:
            GPCLR0 = BIT_21;
            break;
        case 27:
            GPCLR0 = BIT_27;
            break;
        case 22:
            GPCLR0 = BIT_22;
            break;
        case 23:
            GPCLR0 = BIT_23;
            break;
        case 24:
            GPCLR0 = BIT_24;
            break;
        case 25:
            GPCLR0 = BIT_25;
            break;
        }
    }

    delayMicroseconds(1);
    // Delay to allow any change in state to be reflected in the LEVn, register bit.
}
