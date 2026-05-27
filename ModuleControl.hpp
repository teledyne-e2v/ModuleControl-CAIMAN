#pragma once
#include "i2c.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief 
 * 
 * @param device 
 * @param bus 
 * @return int 
 */
int enable_VdacPda(I2CDevice device, int bus);

/**
 * @brief 
 * 
 * @param device 
 * @param bus 
 * @return int 
 */
int disable_VdacPda(I2CDevice device, int bus);

/**
 * @brief This class is used to communicate with the camera module
 * 
 */
class ModuleCtrl
{
private:
    /**
     * @brief 
     * Bus number
     * 
     */
    int bus;

    /**
     * @brief 
     * PDA IC2 device (control lens)
     */
    I2CDevice devicepda;

    /**
     * @brief 
     * IC2 Device (control module)
     */
    I2CDevice device;

    /**
     * @brief 
     * IC2 Device (temperature sensor)
     */
    I2CDevice devicetemp;

public:
    /**
     * @brief 
     * Initialise IC2 connection
     */
    int ModuleControlInit(const char bus_name[32]);

    ~ModuleCtrl();

    /**
     * @brief 
     * Close IC2 connection
     * 
     */
    void ModuleControlClose();

    /**
     * @brief Read the state of the sensor
     * 
     * @return int The sensor state value
     */
    int read_sensor_state(int *state);

    /**
     * @brief Set the exposition time
     * 
     * @param b 
     * @return int 
     */
    int setTint(float b);

    /**
     * @brief Set the Analog Gain
     * 
     * @param b 
     * @return int 
     */
    int setAnalogGain(float again);

    /**
     * @brief Set the Digital Gain
     * 
     * @param b1 
     * @return int 
     */
    int setDigitalGain(float dgain);

    /**
     * @brief 
     * Set the PDA
     * 
     * @param PdaRegValue 
     * @return int 
     */
    int write_VdacPda(int dacValue);

    /**
     * @brief write specified value in the specified register
     * 
     * @param registre 
     * @param value 
     * @return int 
     */
    int writeReg(int regAddr, int value);

    /**
     * @brief 
     * Read specified register
     * 
     * @param registre 
     * @param value 
     * @return int 
     */
    int readReg(int regAddr, int *value);

    int readReg64b(uint32_t regAddr, int *value);

    int read_VdacPda( int *dacValue, double *voltageValue);

    int read_Temp(double *LocalTempValue, double *RemoteTempValue, int TempMode);
    int get_TempMode(int *tempMode);
    int set_TempMode(int tempMode);
};

struct solution
{
    int tintII;
    int tintclk;
};
