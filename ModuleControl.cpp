#include <fstream>
#include "ModuleControl.hpp"

#define SENSORS_I2C_ADDR 0x27
#define BOOTSTRAP_MANUFACTURER_NAME 		0x00000000
#define BOOTSTRAP_MODEL_NAME 				0x00000040
#define BOOTSTRAP_DEVICE_VERSION 			0x00000080
#define BOOTSTRAP_MANUFACTURER_INFO 		0x000000C0
#define BOOTSTRAP_SERIAL_NUMBER 			0x00000100
#define BOOTSTRAP_USER_DEFINE_NAME 			0x00000140
#define BOOTSTRAP_DEVICE_FIRMWARE_VERSION 	0x00000180

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
int ModuleCtrl::readReg(int regAddr, int *value)
{
	unsigned char buffer[2];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;
	device.page_bytes = 256;

	if ((i2c_caiman_read(&device, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "READ ERROR: device=0x%x register address=0x%x\n", device.addr, regAddr);
		error=-3;
	}
	else
	{
		// *value = (buffer[0] << 8) | buffer[1]; don't need to swap bytes! memcpy may do the job
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
	
	error = i2c_caiman_read(&device, regAddr, buffer, size);
	// printf("buffer_fb=%s\n", buffer);
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

	error = i2c_caiman_read(&device, BOOTSTRAP_MANUFACTURER_NAME, buffer, size);
	printf("MANUFACTURER_NAME=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_MODEL_NAME, buffer, size);
	printf("MODEL_NAME=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_DEVICE_VERSION, buffer, size);
	printf("DEVICE_VERSION=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_MANUFACTURER_INFO, buffer, size);
	printf("MANUFACTURER_INFO=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_SERIAL_NUMBER, buffer, size);
	printf("SERIAL_NUMBER=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_USER_DEFINE_NAME, buffer, size);
	printf("USER_DEFINE_NAME=%s\n", buffer);
	error = i2c_caiman_read(&device, BOOTSTRAP_DEVICE_FIRMWARE_VERSION, buffer, size);
	printf("DEVICE_FIRMWARE_VERSION=%s\n", buffer);

	return error;
}

/************************************************
 *Write register
 ************************************************/
int ModuleCtrl::writeReg(int regAddr, int value)
{
	unsigned char buffer[2];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;

	*buffer = ((value)&0xff00) >> 8;
	*(buffer + 1) = ((value)&0x00ff);

	device.page_bytes = 256;

	// if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	// {
	// 	fprintf(stderr, "WRITE ERROR: device=0x%x register address=0x%x\n", device.addr, regAddr);
	// 	error=-3;
	// }
	//printf("error=%d\n",error);
	return error;
}
int ModuleCtrl::read_sensor_state(int *state)
{
	int ulAddress;
	int ulValue=0;
	int error=0;

	ulAddress = REG_FB_STATE;

	error=this->readReg(ulAddress, &ulValue);
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



