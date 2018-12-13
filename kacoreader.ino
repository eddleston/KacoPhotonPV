// Process to take serial data from Kaco inverter and upload
// to pvoutput.org
// 
// With thanks to Ian Hutt, this c++ version adapted from the python version:
// https://github.com/cvpeck/KacoPV/blob/master/kaco2pvo/kaco2pvo.py
//
// To be run on Particle Photon, connected to inverter via serial adapter, e.g.:
// 
// https://amzn.to/2QMKJFo
//
// To test, un-comment the LOCAL_TESTING=“Test”
//
// Data gets delivered to PVOutput.org by setting up a webhook in Particle Cloud
// and using pass through parameters (  e.g. v1: {{{v1}}}  )
//
// This software in any form is covered by the following Open Source BSD license
//
// Copyright 2018, Paul Eddleston
// Copyright 2013-2014, Ian Hutt
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
// OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITE
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
// OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "Particle.h"
#include <list>

// Constants
const unsigned long SEND_INTERVAL_MS = 10000;
const size_t READ_BUF_SIZE = 128;

// Global variables
String serialData;
int counter = 0;
system_tick_t lastSend = 0;
char readBuf[READ_BUF_SIZE];
size_t readBufOffset = 0;

// PV generation values (W) less than this will be set to 0
float  PVS_MIN_PVP = 7.5;
// PV generation values greater than this will be set to this
float PVS_MAX_PVP = 6000.0;

String LOCAL_TESTING = ""; // Production Mode
//String LOCAL_TESTING = "Test"; // Test Mode

// How often in minutes to update PVoutput
int PVO_STATUS_INTERVAL = 5;
// Time in hours of updates from Kaco unit (normally 10 seconds)
float SAMPLE_TIME = 10.0/3600.0;

int PV_DAILY_UPLOAD_TIME_HOUR = 23;
int PV_DAILY_UPLOAD_TIME_MIN = 45;

system_tick_t TIME_NOW = Time.now();
system_tick_t PV_DAILY_UPLOAD_TIME = Time.now() - ((Time.hour(TIME_NOW) * 3600) + (Time.minute(TIME_NOW) * 60));

system_tick_t lastStatus = Time.now();
system_tick_t lastOutput = Time.now();
float dailyEnergy = 0.0;

SYSTEM_THREAD(ENABLED);

class PowerReading
{
    time_t _timeOfReading = Time.now();
    int _dailyRunTime = 0;
    int _operatingState = 0;
    float _generatorVoltage = 0.0;
    float _generatorCurrent = 0.0;
    float _generatorPower = 0.0;
    float _lineVoltage = 0.0;
    float _lineCurrentFeedIn = 0.0;
    float _powerFeedIn = 0.0;
    float _unitTemperature = 0.0;

    public:
    
    PowerReading() { }
	
    PowerReading (time_t timeOfReading, int dailyRunTime, int operatingState,
                  float generatorVoltage, float generatorCurrent, float generatorPower, 
                  float lineVoltage, float lineCurrentFeedIn, float powerFeedIn, float unitTemperature)
    {
        _timeOfReading = timeOfReading;
        _dailyRunTime = dailyRunTime;
        _operatingState = operatingState;
        _generatorVoltage = generatorVoltage;
        _generatorCurrent = generatorCurrent;
        _generatorPower = generatorPower;
        _lineVoltage = lineVoltage;
        _lineCurrentFeedIn = lineCurrentFeedIn;
        _powerFeedIn = powerFeedIn;
        _unitTemperature = unitTemperature;
    }
	
    time_t timeOfReading()
    {
        return _timeOfReading;
    }
 
    float generatedPower()
    {
        return _generatorPower;
    }

    float temperature()
    {
        return _unitTemperature;
    }

    float generatedVoltage()
    {
        return _generatorVoltage;
    }

    float generatedCurrent()
    {
        return _generatorCurrent;
    }
};

class PowerReadingList
{
    std::list<PowerReading> _powerReadingList;
    float _avgGeneratedPower = 0.0;
    float _avgGeneratedVoltage = 0.0;
    float _avgGeneratedCurrent = 0.0;
    int _totalReadings = 0;
	
    public:
	
    PowerReadingList() { }
	
    void addReading(PowerReading powerReading)
    {
        _powerReadingList.push_back(powerReading);
        _totalReadings++;
		
        // new average = old average + ((new value - old average) / new number of readings)
        _avgGeneratedPower = _avgGeneratedPower + ((powerReading.generatedPower() - _avgGeneratedPower) / _totalReadings);
        _avgGeneratedVoltage = _avgGeneratedVoltage + ((powerReading.generatedVoltage() - _avgGeneratedVoltage) / _totalReadings);
        _avgGeneratedCurrent = _avgGeneratedCurrent + ((powerReading.generatedCurrent() - _avgGeneratedCurrent) / _totalReadings);
    }
	
    void clear()
    {
        _powerReadingList.clear();
        _avgGeneratedPower = 0.0;
        _avgGeneratedVoltage = 0.0;
        _avgGeneratedCurrent = 0.0;
        _totalReadings = 0;
    }
	
    float averageGeneratedPower() 
    {
        return _avgGeneratedPower;
    }

    float averageGeneratedVoltage() 
    {
        return _avgGeneratedVoltage;
    }

    float averageGeneratedCurrent() 
    {
        return _avgGeneratedCurrent;
    }
};

PowerReadingList powerReadingList;

void setup()
{
    Serial1.begin(9600, SERIAL_DATA_BITS_8 | SERIAL_STOP_BITS_1 | SERIAL_PARITY_NO | SERIAL_FLOW_CONTROL_NONE);
}

void postPVstatus(time_t timeOfReading, float energyGen, float powerGen, 
                  int energyUse, int powerUse, int temp, float volts)
{
    String params = String::format("{\"d\": \"%s\", \"t\": \"%s\", \"v1\": %.8f, "
                                    "\"v2\": %.8f, \"v3\": %d, \"v4\": %d, "
                                    "\"v5\": %d, \"v6\": %.2f, \"c1\": 0, "
                                    "\"n\": 0}",
                                    Time.format(timeOfReading, "%Y%m%d").c_str(),
                                    Time.format(timeOfReading, "%H:%M").c_str(), 
                                    energyGen, 
                                    powerGen, 
                                    energyUse, // Energy used for later use if required 
                                    powerUse, // Power used for later use if required
                                    temp, 
                                    volts);

    Particle.publish(LOCAL_TESTING + "PVStatus", params);
}

void addReading(PowerReading newPowerReading)
{
    time_t timeNow = Time.now();
    dailyEnergy += (newPowerReading.generatedPower() * SAMPLE_TIME);
    powerReadingList.addReading(newPowerReading);
	
    if ((Time.minute(timeNow) % PVO_STATUS_INTERVAL == 0) && (Time.minute(lastStatus) != Time.minute(timeNow)))
    {
        postPVstatus(timeNow, 
	             dailyEnergy, 
                     powerReadingList.averageGeneratedPower(), 
                     0, 
                     0, 
                     newPowerReading.temperature(), 
                     powerReadingList.averageGeneratedVoltage());
     
        lastStatus = timeNow;
		
        powerReadingList.clear();
    }

    if ((timeNow > PV_DAILY_UPLOAD_TIME) && (Time.day(timeNow) != Time.day(lastOutput)))
    {
        lastOutput = timeNow;
        dailyEnergy = 0;        
    }
}

void processReading() {
    // Split string, which is fixed columns.
    char *placeholder = strtok(readBuf, " ");
    char *running_time = strtok(NULL, " ");
    int status = atoi(strtok(NULL, " "));
    float generator_voltage = atof(strtok(NULL, " "));
    float generator_current = atof(strtok(NULL, " "));
    int generator_power = atoi(strtok(NULL, " "));
    float line_voltage = atof(strtok(NULL, " "));
    float line_current = atof(strtok(NULL, " "));
    int power_fed_into_grid = atoi(strtok(NULL, " "));
    
    char *temp = strtok(NULL, " \r\n");
    int temperature = atoi(temp);

    String log_datetime = Time.format("%Y-%m-%d %H:%M:%S");
    
    char sData[128];
    sprintf(sData, "%s,%d,%.1f,%.2f,%d,%.1f,%.2f,%d,%d", log_datetime.c_str(), status, generator_voltage, generator_current, generator_power, line_voltage, line_current, power_fed_into_grid, temperature);

    serialData = String(sData);

    if (serialData != "")
        Particle.publish(LOCAL_TESTING + "Solar_Update", serialData);

    PowerReading powerReading(
        Time.now(),
        Time.now(), 
        status,
        generator_voltage,
        generator_current,
        generator_power, 
        line_voltage, 
        line_current, 
        power_fed_into_grid,
        temperature
    );
    
    addReading(powerReading);
}

void loop() {
    if (LOCAL_TESTING == "Test")
    {
        test();
    }
    else
    {
        if (millis() - lastSend >= SEND_INTERVAL_MS) {
            lastSend = millis();

            // Read data from serial
            while(Serial1.available()) {
                if (readBufOffset < READ_BUF_SIZE) {
                    char c = Serial1.read();
                    if (c != '\r' && c != '\n') {
                        // Add character to buffer
                        readBuf[readBufOffset++] = c;
                    }
                    else {
                        // End of line character found, process line
                        readBuf[readBufOffset++] = 0;
                        processReading();
                        readBufOffset = 0;
                    }
                }
                else {
                    Particle.publish("readBuf overflow, emptying buffer");
                    readBufOffset = 0;
                }
            }
        }
    }
}

void test() {
    String readings[80] = {
        "00.00.0000 00.00 3 170.0 180.0 80 150.0 200.0 50",
        "00.00.0000 00.00 3 180.0 190.0 90 160.0 210.0 60",
        "00.00.0000 00.00 3 190.0 200.0 100 170.0 220.0 70",
        "00.00.0000 00.00 3 200.0 210.0 110 180.0 230.0 80",
        "00.00.0000 00.00 3 210.0 220.0 120 190.0 240.0 90"
    };
	
    for (int i = 0; i < 80; i++)
    {		
        strcpy(readBuf, readings[i % 5].c_str());
        processReading();
        delay(10000);
    }
}
