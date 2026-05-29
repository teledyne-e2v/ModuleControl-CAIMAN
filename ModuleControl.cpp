#include <fstream>
#include "ModuleControl.hpp"

#define DEBUG_MODE 1

#define SENSORS_I2C_ADDR 0x27
#define BOOTSTRAP_MANUFACTURER_NAME 		0x00000000
#define BOOTSTRAP_MODEL_NAME 				0x00000040
#define BOOTSTRAP_DEVICE_VERSION 			0x00000080
#define BOOTSTRAP_MANUFACTURER_INFO 		0x000000C0
#define BOOTSTRAP_SERIAL_NUMBER 			0x00000100
#define BOOTSTRAP_USER_DEFINE_NAME 			0x00000140
#define BOOTSTRAP_DEVICE_FIRMWARE_VERSION 	0x00000180

/* Status Codes */
#define STATUS_NULL 				0x00000000
#define STATUS_READ_SUCCESS 		0x00000801
#define STATUS_READ_INVALID_CMD 	0x00100801
#define STATUS_READ_INVALID_ACCESS 	0x00210801
#define STATUS_READ_OUT_OF_RANGE 	0x00220801
#define STATUS_READ_INCORRECT_SIZE 	0x00240801
#define STATUS_READ_INCORRECT_ADDR 	0x00250801
#define STATUS_READ_BUSY 			0x00400801
#define STATUS_WRITE_SUCCESS 		0x00000803
#define STATUS_WRITE_INVALID_CMD 	0x00100803
#define STATUS_WRITE_INVALID_ACCESS 0x00210803
#define STATUS_WRITE_OUT_OF_RANGE 	0x00220803
#define STATUS_WRITE_INCORRECT_SIZE 0x00240803
#define STATUS_WRITE_INCORRECT_ADDR 0x00250803
#define STATUS_WRITE_BUSY 			0x00400803

#define REG_FB_STATE 0x0068
#define REG_LINE_LENGTH 0x0006
#define REG_TINT_LL 0x0020
#define REG_TINT_CK 0x0021
#define REG_ANA_GAIN 0x0022
#define REG_DIG_GAIN 0x0022


/************************************************
 *Main fuction
 ************************************************/

ModuleCtrl::~ModuleCtrl()
{
#ifndef DEBUG_MODE
	ModuleControlClose(); // close IC2
#endif
}

/************************************************
 *Init I2C bus
 ************************************************/
int ModuleCtrl::ModuleControlInit(const char bus_name[32])
{
	int error = 0;

	/* Open i2c bus */
	if ((bus = i2c_open(bus_name)) == -1)
	{
		fprintf(stderr, "Open i2c bus:%s error!\n", bus_name);
		error = -3;
	}
	else
	{
		printf("Bus %s open\n", bus_name);

		/* Init sensor i2c device */
		memset(&device, 0, sizeof(device));
		i2c_init_device(&device);
		device.bus = bus;
		/*device address*/
		device.addr = SENSORS_I2C_ADDR;
		/*Unknown value*/
		device.page_bytes = 256;
		/*Address length in bytes*/
		device.iaddr_bytes = 8;
		device.delay = 5;
	}
	
	return error;
}

/************************************************
 *Close I2C bus
 ************************************************/
void ModuleCtrl::ModuleControlClose()
{
	/* Close de bus*/
	i2c_close(bus);

	printf("I2C Bus closed\n");
}

/************************************************
 *Read register
 ************************************************/
int ModuleCtrl::readReg(uint32_t regAddr, uint32_t *value)
{
	unsigned char buffer[2];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;
	device.page_bytes = 256;

	unsigned char status[4];
	ssize_t size_status = sizeof(status);
	memset(status, 0, size_status);
	uint16_t status_value = 0;

	if ((i2c_caiman_read(&device, regAddr, buffer, size, status)) != size)
	{
		fprintf(stderr, "READ ERROR: device=0x%x register address=0x%x\n", device.addr, regAddr); //TODO: print status code to debug it: issue!
		status_value = (status[3] << 24) | (status[2] << 16) | (status[1] << 8) | status[0];
		print_i2c_status(status_value);

		error=-3;
	}
	else
	{
		// *value = (buffer[0] << 8) | buffer[1]; don't need to swap bytes! memcpy may do the job
		#ifdef DEBUG_MODE
		status_value = (status[3] << 24) | (status[2] << 16) | (status[1] << 8) | status[0];
		print_i2c_status(status_value);
		#endif

		*value = (buffer[1] << 8) | buffer[0];
	}

	return error;
}

int ModuleCtrl::readReg64b(uint32_t regAddr, char *str)
{
	unsigned char buffer[64];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;
	device.page_bytes = 256;

	unsigned char status[4];
	ssize_t size_status = sizeof(status);
	memset(status, 0, size_status);
	uint16_t status_value = 0;
	
	error = i2c_caiman_read(&device, regAddr, buffer, size, status);
	status_value = (status[3] << 24) | (status[2] << 16) | (status[1] << 8) | status[0];

	#ifdef DEBUG_MODE
	printf("buffer_fb=%s\n", buffer);
	print_i2c_status(status_value);
	#endif

	strcpy(str, (char*)buffer);

	return error;
}

int ModuleCtrl::printBootstrapData()
{
	int error=0;
	unsigned char buffer[64];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	device.page_bytes = 256;

	unsigned char status[4];
	ssize_t size_status = sizeof(status);
	memset(status, 0, size_status);
	uint16_t status_value = 0;

	error = i2c_caiman_read(&device, BOOTSTRAP_MANUFACTURER_NAME, buffer, size, status);
	printf("MANUFACTURER_NAME=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_MODEL_NAME, buffer, size, status);
	printf("MODEL_NAME=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_DEVICE_VERSION, buffer, size, status);
	printf("DEVICE_VERSION=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_MANUFACTURER_INFO, buffer, size, status);
	printf("MANUFACTURER_INFO=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_SERIAL_NUMBER, buffer, size, status);
	printf("SERIAL_NUMBER=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_USER_DEFINE_NAME, buffer, size, status);
	printf("USER_DEFINE_NAME=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_DEVICE_FIRMWARE_VERSION, buffer, size, status);
	printf("DEVICE_FIRMWARE_VERSION=%s\n", buffer);

	return error;
}

/************************************************
 *Write register
 ************************************************/
int ModuleCtrl::writeReg(uint32_t regAddr, uint32_t value)
{
	unsigned char buffer[4];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;

	// *buffer = ((value)&0xff000000) >> 24;
	// *(buffer + 1) = ((value)&0x00ff0000) >> 16;
	// *(buffer + 2) = ((value)&0x0000ff00) >> 8;
	// *(buffer + 3) = ((value)&0x000000ff);

	*buffer = ((value)&0x000000ff);
	*(buffer + 1) = ((value)&0x0000ff00) >> 8;
	*(buffer + 2) = ((value)&0x00ff0000) >> 16;
	*(buffer + 3) = ((value)&0xff000000) >> 24;

	device.page_bytes = 256;


	i2c_caiman_write(&device, regAddr, buffer, size);

	// if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	// {
	// 	fprintf(stderr, "WRITE ERROR: device=0x%x register address=0x%x\n", device.addr, regAddr);
	// 	error=-3;
	// }
	//printf("error=%d\n",error);
	return error;
}

int ModuleCtrl::read_i2c_status(uint32_t *status)
{
	unsigned char buffer[4];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;
	device.page_bytes = 256;

	error = i2c_caiman_status(&device, buffer, size, false);

	#ifdef DEBUG_MODE
	printf("I2C Acknowledge Buffer= ");
	for (int i = 0; i < size; i++) { printf("%02x ", buffer[i]); }
    printf("\n");
	#endif

	*status = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];

	return error;
}

int ModuleCtrl::print_i2c_status(uint32_t status)
{
	switch (status)
	{
		case STATUS_NULL:
			printf("I2C Status: 0x%08x NULL\n", status);
			break;
		case STATUS_READ_SUCCESS:
			printf("I2C Status: 0x%08x READ_SUCCESS\n", status);
			break;
		case STATUS_READ_INVALID_CMD:
			printf("I2C Status: 0x%08x READ_INVALID_CMD\n", status);
			break;
		case STATUS_READ_INVALID_ACCESS:
			printf("I2C Status: 0x%08x READ_INVALID_ACCESS\n", status);
			break;
		case STATUS_READ_OUT_OF_RANGE:
			printf("I2C Status: 0x%08x READ_OUT_OF_RANGE\n", status);
			break;
		case STATUS_READ_INCORRECT_SIZE:
			printf("I2C Status: 0x%08x READ_INCORRECT_SIZE\n", status);
			break;
		case STATUS_READ_INCORRECT_ADDR:
			printf("I2C Status: 0x%08x READ_INCORRECT_ADDR\n", status);
			break;
		case STATUS_READ_BUSY:
			printf("I2C Status: 0x%08x READ_BUSY\n", status);
			break;
		case STATUS_WRITE_SUCCESS:
			printf("I2C Status: 0x%08x WRITE_SUCCESS\n", status);
			break;
		case STATUS_WRITE_INVALID_CMD:
			printf("I2C Status: 0x%08x WRITE_INVALID_CMD\n", status);
			break;
		case STATUS_WRITE_INVALID_ACCESS:
			printf("I2C Status: 0x%08x WRITE_INVALID_ACCESS\n", status);
			break;
		case STATUS_WRITE_OUT_OF_RANGE:
			printf("I2C Status: 0x%08x WRITE_OUT_OF_RANGE\n", status);
			break;
		case STATUS_WRITE_INCORRECT_SIZE:
			printf("I2C Status: 0x%08x WRITE_INCORRECT_SIZE\n", status);
			break;
		case STATUS_WRITE_INCORRECT_ADDR:
			printf("I2C Status: 0x%08x WRITE_INCORRECT_ADDR\n", status);
			break;
		case STATUS_WRITE_BUSY:
			printf("I2C Status: 0x%08x WRITE_BUSY\n", status);
			break;
		default:
			printf("I2C Acknowledge: 0x%08x UNKNOWN\n", status);
			break;
	}

	return 0;
}

int ModuleCtrl::read_sensor_state(int *state)
{
	int ulAddress;
	int ulValue=0;
	int error=0;

	ulAddress = REG_FB_STATE;

	// error=this->readReg(ulAddress, &ulValue);
	*state=ulValue;

	return error;
}

int ModuleCtrl::setTint(float b)
{
	int regAddr, line;
	unsigned char buffer[2];
	ssize_t size = sizeof(buffer);
	struct solution init;
	int error=0;
	memset(buffer, 0, size);
	device.page_bytes = 256;

	b = b * 1000000; // convert to nanoseconds
	b = b / 20;

	// read the line length
	regAddr = REG_LINE_LENGTH;
	// if ((i2c_ioctl_read(&device, regAddr, buffer, size)) != size)
	// {
	// 	fprintf(stderr, "Can't read 0x%x reg!\n", regAddr);
	// 	error=-3;
	// }

	line = (*buffer << 8) + *(buffer + 1);

	init.tintII = (int)b / line;
	init.tintclk = (int)(b - init.tintII * line);

	// write in reg_tint_ll
	regAddr = REG_TINT_LL;
	*buffer = ((init.tintII) & 0xff00) >> 8;
	*(buffer + 1) = (init.tintII & 0x00ff);

	// if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	// {
	// 	fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
	// 	error=-3;
	// }

	// write in reg_tint_ck
	regAddr = REG_TINT_CK;
	*buffer = ((init.tintclk) & 0xff00) >> 8;
	*(buffer + 1) = (init.tintclk & 0x00ff);

	// if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	// {
	// 	fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
	// 	error=-3;
	// }

	return error;
}

/************************************************
 *Set the analog gain
 ************************************************/

int ModuleCtrl::setAnalogGain(float again)
{
	int regAddr, b;
	unsigned char buffer[2];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;
	regAddr = REG_ANA_GAIN;
	device.page_bytes = 256;

	b = (int)(again * 100);

	switch (b)
	{
	case 100:
		b = 0;
		break;
	case 120:
		b = 1;
		break;
	case 145:
		b = 2;
		break;
	case 150:
		b = 2;
		break;
	case 171:
		b = 3;
		break;
	case 200:
		b = 4;
		break;
	case 240:
		b = 5;
		break;
	case 300:
		b = 6;
		break;
	case 343:
		b = 7;
		break;
	case 400:
		b = 8;
		break;
	case 480:
		b = 9;
		break;
	case 500:
		b = 9;
		break;
	case 600:
		b = 10;
		break;
	case 686:
		b = 11;
		break;
	case 700:
		b = 11;
		break;
	case 800:
		b = 12;
		break;
	case 900:
		b = 13;
		break;
	case 960:
		b = 13;
		break;
	case 1000:
		b = 13;
		break;
	case 1200:
		b = 14;
		break;
	case 1600:
		b = 15;
		break;
	default:
		printf("Forbidden value, couldn't set analog gain\n");
		return 0;
	}

	*(buffer + 1) = (*(buffer + 1) & 0xf0) + b;

	// if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	// {
	// 	fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
	// 	error=-3;
	// }
	return 0;
}


/************************************************
 *Set the digital gain
 ************************************************/
int ModuleCtrl::setDigitalGain(float dgain)
{
	int regAddr, b;
	unsigned char buffer[2];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;
	regAddr = REG_DIG_GAIN;
	device.page_bytes = 256;

	//from x0.004 to x16 (decimal ex: x1=256, x1.5=384...)
	b = (int)(dgain * 256);

	if (b > 4095)
		b = 4095;
	if (b < 1)
		b = 1;

	//write (b) in register
	*buffer = ((b)&0xff00) >> 8;
	*(buffer + 1) = ((b)&0x00ff);
	// if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	// {
	// 	fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
	// 	error=-3;
	// }
	return error;
}



