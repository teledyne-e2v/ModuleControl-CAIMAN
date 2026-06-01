#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include "i2c_caiman.hpp"

/* I2C default delay */
#define I2C_DEFAULT_DELAY 1

/* I2C internal address max length */
#define INT_ADDR_MAX_BYTES 8

/* I2C page max bytes */
#define PAGE_MAX_BYTES 4096

#define GET_I2C_DELAY(delay) ((delay) == 0 ? I2C_DEFAULT_DELAY : (delay))
#define GET_I2C_FLAGS(tenbit, flags) ((tenbit) ? ((flags) | I2C_M_TEN) : (flags))
#define GET_WRITE_SIZE(addr, remain, page_bytes) ((addr) + (remain) > (page_bytes) ? (page_bytes) - (addr) : remain)

static void i2c_delay(unsigned char delay);

/* Command Codes */
#define READ_MEM_CMD 0x0800
#define WRITE_MEM_CMD 0x0802

/* Acknowledge Codes */
#define READ_MEM_ACK 0x0801
#define WRITE_MEM_ACK 0x0803

/* Status Codes */
#define STATUS_SUCCESS 0x0000
#define STATUS_INVALID_CMD 0x0010
#define STATUS_INVALID_ACCESS 0x0021
#define STATUS_OUT_OF_RANGE 0x0022
#define STATUS_INCORRECT_SIZE 0x0024
#define STATUS_INCORRECT_ADDR 0x0025
#define STATUS_BUSY 0x0040

/*
**	@brief		:	Open i2c bus
**	#bus_name	:	i2c bus name such as: /dev/i2c-1
**	@return		:	failed return -1, success return i2c bus fd
*/
int i2c_open(const char *bus_name)
{
    int fd;

    /* Open i2c-bus devcice */
    if ((fd = open(bus_name, O_RDWR)) == -1)
    {

        return -1;
    }

    return fd;
}

void i2c_close(int bus)
{
    close(bus);
}

/*
**	@brief		:	Initialize I2CDevice with defualt value
**	#device	    :	I2CDevice struct
*/
void i2c_init_device(I2CDevice *device)
{
    /* 7 bit device address */
    device->tenbit = 0;

    /* 1ms delay */
    device->delay = 1;

    /* 8 bytes per page */
    device->page_bytes = 8;

    /* 1 byte internal(word) address */
    device->iaddr_bytes = 1;
}

/*
**	@brief		:	Get I2CDevice struct desc
**	#device	    :	I2CDevice struct
**  #buf        :   Description message buffer
**  #size       :   #buf size
**	@return		:	return i2c device desc
*/
char *i2c_get_device_desc(const I2CDevice *device, char *buf, size_t size)
{
    memset(buf, 0, size);
    snprintf(buf, size, "Device address: 0x%x, tenbit: %s, internal(word) address: %ld bytes, page max %d bytes, delay: %dms",
             device->addr, device->tenbit ? "True" : "False", device->iaddr_bytes, device->page_bytes, device->delay);

    return buf;
}

/*
**	@brief	:	read #len bytes data from #device #iaddr to #buf
**	#device	:	I2CDevice struct, must call i2c_device_init first
**	#iaddr	:	i2c_device internal address will read data from this address, no address set zero
**	#buf	:	i2c data will read to here
**	#len	:	how many data to read, lenght must less than or equal to buf size
**	@return : 	success return read data length, failed -1
*/
ssize_t i2c_caiman_read(const I2CDevice *device, uint32_t iaddr, void *buf, size_t len, void *status)
{
    ssize_t cnt;
    unsigned char addr[INT_ADDR_MAX_BYTES];
    unsigned char delay = GET_I2C_DELAY(device->delay);
    uint32_t regAddrHeader =((uint32_t)len << 16) | (uint32_t)READ_MEM_CMD; // Command code READ_MEM_CMD (0x0800) and length in high 16 bit
    uint64_t regAddrRead = ((uint64_t)iaddr << 32) | (uint64_t)regAddrHeader;

	unsigned char readbuf[len+4]; // creates read buffer with extra 4 bytes for status code
	ssize_t size = sizeof(readbuf);
	memset(readbuf, 0, size);

    unsigned char statusbuf[4]; // creates status buffer to store the first 4 bytes which is status code
	ssize_t size_status = sizeof(statusbuf);
	memset(statusbuf, 0, size_status);

    /* Set i2c slave address */
    if (i2c_select(device->bus, device->addr, device->tenbit) == -1)
    {

        return -1;
    }

    /* Convert i2c internal address */
    memset(addr, 0, sizeof(addr));
    // i2c_iaddr_convert(iaddr, device->iaddr_bytes, addr);
    i2c_iaddr_convert(regAddrRead, device->iaddr_bytes , addr);

    /* Write internal address to device  */
    if (write(device->bus, addr, device->iaddr_bytes) != device->iaddr_bytes)
    {

        perror("Write i2c internal address error");
        return -1;
    }

    /* Wait a while */
    i2c_delay(delay);

    /* Read count bytes data from int_addr specify address */
    if ((cnt = read(device->bus, readbuf, size)) == -1)
    {

        perror("Read i2c data error");
        return -1;
    }

    /* Print full buffer for debug */
    // printf("Read Buffer=");
	// for (int i = 0; i < size; i++) { printf("%02x ", readbuf[i]); }
    // printf("\n");
    
    /* Check status codes */
    memcpy(statusbuf, readbuf, size_status); // copy the first 4 bytes which is status code to status buffer
    uint16_t ack = (statusbuf[1] << 8) | statusbuf[0]; 
    uint16_t code = (statusbuf[3] << 8) | statusbuf[2];

    memcpy(status, statusbuf, size_status); //return status code to caller

    if(ack != READ_MEM_ACK)
    {
        fprintf(stderr, "I2C read error, aknowledge code: 0x%04X\n", ack);
        return -1;
    }

    if(code != STATUS_SUCCESS)
    {
        fprintf(stderr, "I2C read error, status code: 0x%04X\n", code);
        return -1;
    }
    
    /* Copy data to buf */
    memcpy(buf, readbuf + 4, len); // skip the first 4 bytes which is status code

    return cnt-4;
}

/*
**	@brief	:	write #buf data to i2c #device #iaddr address
**	#device	:	I2CDevice struct, must call i2c_device_init first
**	#iaddr	: 	i2c_device internal address, no address set zero
**	#buf	:	data will write to i2c device
**	#len	:	buf data length without '/0'
**	@return	: 	success return write data length, failed -1
*/
ssize_t i2c_caiman_write(const I2CDevice *device, uint32_t iaddr, const void *buf, size_t len)
{
    ssize_t remain = len;
    size_t cnt = 0;
    const unsigned char *buffer = (unsigned char *)buf;
    unsigned char delay = GET_I2C_DELAY(device->delay);
    // unsigned char tmp_buf[PAGE_MAX_BYTES + INT_ADDR_MAX_BYTES];

    unsigned char addr[INT_ADDR_MAX_BYTES];
    uint32_t regAddrHeader =((uint32_t)len << 16) | (uint32_t)WRITE_MEM_CMD; // Command code WRITE_MEM_CMD (0x0802) and length in high 16 bit
    uint64_t regAddrWrite = ((uint64_t)iaddr << 32) | (uint64_t)regAddrHeader;

	// unsigned char databuf[len]; // creates write buffer for
	// ssize_t size = sizeof(databuf);
	// memset(databuf, 0, size);

    /* Set i2c slave address */
    if (i2c_select(device->bus, device->addr, device->tenbit) == -1)
    {

        return -1;
    }

    /* Convert i2c internal address */
    // memset(addr, 0, sizeof(addr));
    // // i2c_iaddr_convert(iaddr, device->iaddr_bytes, addr);
    // i2c_iaddr_convert(regAddrWrite, device->iaddr_bytes , addr);

    size_t size = INT_ADDR_MAX_BYTES + len; // total size is address length and write buffer length
    unsigned char writebuf[size];
    memset(writebuf, 0, sizeof(writebuf));

    /* Convert i2c internal address */
    i2c_iaddr_convert(regAddrWrite, device->iaddr_bytes, writebuf);
    
    /* Copy data to tmp_buf */
    memcpy(writebuf + INT_ADDR_MAX_BYTES, buffer, size);

    /* Print full buffer for debug */
    printf("Write Buffer=");
	for (int i = 0; i < size; i++) { printf("%02x ", writebuf[i]); }
    printf("\n");

    /* Write to buf content to i2c device length is address length and write buffer length */
    if ((unsigned int)(write(device->bus, writebuf, size)) != size)
    {
        perror("I2C write error:");
        return -1;
    }

    return 0;
}

ssize_t i2c_caiman_status(const I2CDevice *device, void *status, size_t len, bool print)
{
    ssize_t cnt;

	unsigned char readbuf[len]; // creates read buffer with extra 4 bytes for status code
	ssize_t size = sizeof(readbuf);
	memset(readbuf, 0, size);

    unsigned char statusbuf[4]; // creates status buffer to store the first 4 bytes which is status code
	ssize_t size_status = sizeof(statusbuf);
	memset(statusbuf, 0, size_status);

    /* Set i2c slave address */
    if (i2c_select(device->bus, device->addr, device->tenbit) == -1)
    {

        return -1;
    }

    /* Read count bytes data from int_addr specify address */
    if ((cnt = read(device->bus, readbuf, size)) == -1)
    {
        perror("Read i2c data error");
        return -1;
    }

    /* Check status codes */
    memcpy(statusbuf, readbuf, size_status); // copy the first 4 bytes which is status code to status buffer
    uint16_t ack = (statusbuf[1] << 8) | statusbuf[0]; 
    uint16_t code = (statusbuf[3] << 8) | statusbuf[2];

    if(print)
    {
        fprintf(stderr, "I2C read aknowledge code: 0x%04X ", ack);
        switch (ack)
        {
            case READ_MEM_ACK:
                fprintf(stderr, "(READ_MEM_ACK) ");
                break;
            case WRITE_MEM_ACK:
                fprintf(stderr, "(WRITE_MEM_ACK) ");
                break;
            default:
                fprintf(stderr, "(Unknown ACK) ");
        }
        fprintf(stderr, "\n");


        fprintf(stderr, "I2C read status code: 0x%04X", code);
        switch (code)
        {
            case STATUS_SUCCESS:
                fprintf(stderr, "(STATUS_SUCCESS) ");
                break;
            case STATUS_INVALID_CMD:
                fprintf(stderr, "(STATUS_INVALID_CMD) ");
                break;
            case STATUS_INVALID_ACCESS:
                fprintf(stderr, "(STATUS_INVALID_ACCESS) ");
                break;
            case STATUS_OUT_OF_RANGE:
                fprintf(stderr, "(STATUS_OUT_OF_RANGE) ");
                break;
            case STATUS_INCORRECT_SIZE:
                fprintf(stderr, "(STATUS_INCORRECT_SIZE) ");
                break;
            case STATUS_INCORRECT_ADDR:
                fprintf(stderr, "(STATUS_INCORRECT_ADDR) ");
                break;
            case STATUS_BUSY:
                fprintf(stderr, "(STATUS_BUSY) ");
                break;
            default:
                fprintf(stderr, "(Unknown Status) ");
        }
        fprintf(stderr, "\n");
    }

    /* Copy data to buf */
    memcpy(status, readbuf, len);

    return cnt;
}

/*
**	@brief	:	i2c internal address convert
**	#iaddr	:	i2c device internal address
**	#len	:	i2c device internal address length
**	#addr	:	save convert address
*/
void i2c_iaddr_convert(uint64_t iaddr, unsigned int len, unsigned char *addr)
{
    union
    {
        uint64_t iaddr;
        unsigned char caddr[INT_ADDR_MAX_BYTES];
    } convert;

    /* I2C internal address order is big-endian, same with network order */
    // convert.iaddr = htonl(iaddr); //htonl only convert 32 bit, but iaddr may be 64 bit, so we need to convert it by ourselves
    for (unsigned int k = 0; k < INT_ADDR_MAX_BYTES; ++k)    
    {        
        // convert.caddr[INT_ADDR_MAX_BYTES - 1 - k] = (iaddr >> (8 * k)) & 0xFF;
        convert.caddr[k] = (iaddr >> (8 * k)) & 0xFF; // finally don't need to swap the bytes
    }

    /* Copy address to addr buffer */
    int i = len - 1;
    int j = INT_ADDR_MAX_BYTES - 1;

    while (i >= 0 && j >= 0)
    {

        addr[i--] = convert.caddr[j--];
    }
}

/*
**	@brief		:	Select i2c address @i2c bus
**	#bus		:	i2c bus fd
**	#dev_addr	:	i2c device address
**	#tenbit		:	i2c device address is tenbit
**	#return		:	success return 0, failed return -1
*/
int i2c_select(int bus, unsigned long dev_addr, unsigned long tenbit)
{
    /* Set i2c device address bit */
    if (ioctl(bus, I2C_TENBIT, tenbit))
    {

        perror("Set I2C_TENBIT failed");
        return -1;
    }

    /* Set i2c device as slave ans set it address */
    if (ioctl(bus, I2C_SLAVE, dev_addr))
    {

        perror("Set i2c device address failed");
        return -1;
    }

    return 0;
}

/*
**	@brief	:	i2c delay
**	#msec	:	milliscond to be delay
*/
static void i2c_delay(unsigned char msec)
{
    usleep(msec * 1e3);
}
