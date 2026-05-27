#include <fstream>
#include "ModuleControl.hpp"

#define SENSORS_I2C_ADDR 0x27
#define PDA50_I2C_ADDR 0x18
#define TEMP_I2C_ADDR 0x4C
#define EEPROM_I2C_ADDR 0x50
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
		device.delay = 1;
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
	buffer[0] = 1;
	buffer[1] = 2;
	if ((i2c_ioctl_read(&device, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "READ ERROR: device=0x%x register address=0x%x\n", device.addr, regAddr);
		error=-3;
	}
	else
	{
		*value = buffer[0] * 256 + buffer[1];
	}
	//printf("error=%d\n",error);q
	return error;
}

int ModuleCtrl::readReg64b(uint32_t regAddr, int *value)
{
	//register 0x00000040
	//sudo i2ctransfer -f -y 9 w8@0x27 0x00 0x08 0x40 0x00 0x40 0x00 0x00 0x00
	//sudo i2ctransfer -f -y 9 r68@0x27
	unsigned char buffer[64];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;

	device.page_bytes = 256;
	// buffer[0] = 1;
	// buffer[1] = 2;
	// buffer[2] = 3;
	// buffer[3] = 4;
	// buffer[4] = 5;
	// buffer[5] = 6;
	// buffer[6] = 7;
	// buffer[7] = 8;
	// buffer[8] = 9;

	// uint64_t regAddrRead = (regAddr << 32) | 0x00400800;
	// printf("regAddrRead=0x%016lX\n",regAddrRead);
	
	error = i2c_read(&device, regAddr, buffer, size);
	// printf("Buffer=%02x %02x %02x %02x %02x %02x %02x %02x %02x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8]);
	printf("Buffer=");
	for (int i = 0; i < 68; i++) {
		printf("%02x ", buffer[i]);
		*value = (*value << 8) | buffer[i];
	}
	printf("\n");
	printf("Data: %s\n", buffer);
	printf("Value=0x%0136X\n", *value);
	// if ((i2c_ioctl_read(&device, regAddr, buffer, size)) != size)
	// {
	// 	fprintf(stderr, "READ ERROR: device=0x%x register address=0x%x\n", device.addr, regAddr);
	// 	error=-3;
	// }
	// else
	// {
	// 	*value = buffer[0] * 256 + buffer[1];
	// }
	//printf("error=%d\n",error);
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

	if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "WRITE ERROR: device=0x%x register address=0x%x\n", device.addr, regAddr);
		error=-3;
	}
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
	if ((i2c_ioctl_read(&device, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "Can't read 0x%x reg!\n", regAddr);
		error=-3;
	}

	line = (*buffer << 8) + *(buffer + 1);

	init.tintII = (int)b / line;
	init.tintclk = (int)(b - init.tintII * line);

	// write in reg_tint_ll
	regAddr = REG_TINT_LL;
	*buffer = ((init.tintII) & 0xff00) >> 8;
	*(buffer + 1) = (init.tintII & 0x00ff);

	if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
		error=-3;
	}

	// write in reg_tint_ck
	regAddr = REG_TINT_CK;
	*buffer = ((init.tintclk) & 0xff00) >> 8;
	*(buffer + 1) = (init.tintclk & 0x00ff);

	if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
		error=-3;
	}

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

	if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
		error=-3;
	}
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
	if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
		error=-3;
	}
	return error;
}


/************************************************
 *TLENS PDA50 functions
 ************************************************/
int enable_VdacPda(I2CDevice device, int bus)
{
	int regAddr;
	unsigned char buffer[1];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;

	// SET CFG_ENABLE = 1
	*buffer = 0x01;
	regAddr = 0x00;
	if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
		error=-3;
	}
	return error;
}

int disable_VdacPda(I2CDevice device, int bus)
{
	int regAddr;
	unsigned char buffer[1];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;

	// SET CFG_ENABLE = 0
	*buffer = 0x00;
	regAddr = 0x00;
	if ((i2c_ioctl_write(&device, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
		error=-3;
	}
	return error;
}

int ModuleCtrl::write_VdacPda(int dacValue)
{
	int regAddr, regValue;
	unsigned char buffer[4];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0,ret;
	int DAC_LSB=44; //LSB in mV
	int DAC_value;
	int minVal=0x0000;
	int maxVal=0xB3B0; //1045 in DAC value

	regValue=dacValue*DAC_LSB;

	// SET DAC VALUE
	// printf("PdaRegValue=%d\n", PdaRegValue);
	if (regValue > maxVal)
	{
		regValue = maxVal;
	}
	else if (regValue < minVal)
	{
		regValue = minVal;
	}

	devicepda.page_bytes = 8; //??


	regAddr = 0x00;
	ret=i2c_ioctl_write(&devicepda, regAddr, &regValue, size);

	if (ret != size)
	{
		fprintf(stderr, "Can't write in 0x%x reg!\n", regAddr);
		error=-3;
	}

	return error;
}

int ModuleCtrl::read_VdacPda(int *dacValue, double *voltageValue)
{

	int regAddr, b, regVal;
	unsigned char buffer[4];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0, ret;
	int DAC_LSB=44; //LSB in mV

	devicepda.page_bytes = 1;

	unsigned char MSB, LSB;
	// READ MSB
	regAddr = 0x00;

	ret=i2c_ioctl_read(&devicepda, regAddr, &regVal, size);

	if (ret != size)
	{
		fprintf(stderr, "Can't read 0x%x reg!\n", regAddr);
		error=-3;
	}
	else
	{
		printf("regVal=0x%x (%d)\n", regVal, regVal);

	}

	*dacValue = regVal/DAC_LSB;
	*voltageValue = *dacValue * DAC_LSB * (double)(0.001)+24;
	return error;
}

int ModuleCtrl::read_Temp(double *LocalTempValue, double *RemoteTempValue, int TempMode)
{
	int regAddr, b;
	unsigned char buffer[1];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;
	uint16_t value=0;
	uint8_t extended;
	unsigned char MSB, LSB;
	int mode;

	// READ TEMP MODE
	error=get_TempMode(&mode);
	extended=mode;

	// READ REMOTE_MSB
	regAddr = 0x01;
	if ((i2c_ioctl_read(&devicetemp, regAddr, &MSB, size)) != size)
	{
		fprintf(stderr, "READ ERROR: device=0x%x register address=0x%x\n", devicetemp.addr, regAddr);
		error=-3;
	}

	// READ REMOTE_LSB
	regAddr = 0x10;
	*buffer = LSB;
	if ((i2c_ioctl_read(&devicetemp, regAddr, &LSB, size)) != size)
	{
		fprintf(stderr, "READ ERROR: device=0x%x register address=0x%x\n", devicetemp.addr, regAddr);
		error=-3;
	}	
	
	if(error==0)
	{
		value = ((MSB & 0x7F) << 8) + LSB;
		//printf("REMOTE=0x%x (%d), MSB=0x%x, LSB=0x%x\n", value, value, MSB, LSB);
		*RemoteTempValue=(value >> 8) + (extended ? -64 : 0 ) + ((value & 0xF0) >> 4) * 0.0625;
		//fprintf(stderr, "RemoteTemp=%f°C\n",*RemoteTempValue);
	}


	// READ LOCAL_MSB
	regAddr = 0x00;
	if ((i2c_ioctl_read(&devicetemp, regAddr, &MSB, size)) != size)
	{
		fprintf(stderr, "READ ERROR: device=0x%x register address=0x%x\n", devicetemp.addr, regAddr);
		error=-3;
	}

	// READ LOCAL_LSB
	regAddr = 0x15;
	*buffer = LSB;
	if ((i2c_ioctl_read(&devicetemp, regAddr, &LSB, size)) != size)
	{
		fprintf(stderr, "READ ERROR: device=0x%x register address=0x%x\n", devicetemp.addr, regAddr);
		error=-3;
	}

	if(error==0)
	{
		value = ((MSB & 0x7F) << 8) + LSB;
		//printf("LOCAL=0x%x (%d), MSB=0x%x, LSB=0x%x\n", value, value, MSB, LSB);
		*LocalTempValue=(value >> 8) + (extended ? -64 : 0 ) + ((value & 0xF0) >> 4) * 0.0625;
		//fprintf(stderr, "RemoteTemp=%f°C\n",*RemoteTempValue);
	}


	return error;
}

int ModuleCtrl::get_TempMode(int *tempMode)
{
	int regAddr, b;
	unsigned char buffer[1];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;
	uint16_t value=0;

	unsigned char MSB, LSB;

	// READ CONFIG REG
	regAddr = 0x03;
	if ((i2c_ioctl_read(&devicetemp, regAddr, &MSB, size)) != size)
	{
		fprintf(stderr, "READ ERROR: device=0x%x register address=0x%x\n", devicetemp.addr, regAddr);
		error=-3;
	}
	else
	{
				
		//fprintf(stderr, "MSB=%x\n",MSB);
	}

	*tempMode=(MSB & 0x04) >> 2;

	return error;
}

int ModuleCtrl::set_TempMode(int tempMode)
{
	int regAddr, b;
	unsigned char buffer[1];
	ssize_t size = sizeof(buffer);
	memset(buffer, 0, size);
	int error=0;
	uint16_t value=0;

	unsigned char MSB, LSB;
	//*buffer = ((value)&0xff00) >> 8;
	*buffer= 0x04*tempMode;
	
	//devicepda.page_bytes = 8;
	// WRITE CONFIG REG
	regAddr = 0x09;	
	if ((i2c_ioctl_write(&devicetemp, regAddr, buffer, size)) != size)
	{
		fprintf(stderr, "WRITE ERROR: device=0x%x register address=0x%x\n", device.addr, regAddr);
		error=-3;
	}

	return error;
}

