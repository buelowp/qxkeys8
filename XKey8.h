/*
 * XKey8.h
 *
 *  Created on: Jan 5, 2017
 *      Author: buelowp
 */

#ifndef XKEY8_H_
#define XKEY8_H_

#define XK8_PID1 0x046a
#define XK8_PID2 0x046d

#define XK8_REPORT_LENGTH 33
#define XK8_MESSAGE_LENGTH 36

#define XK8_BUTTONS 8

#define XK8_CMD_BTN_LED 181
#define XK8_CMD_PNL_LED 179
#define XK8_CMD_DESC    214
#define XK8_CMD_STATUS  177
#define XK8_CMD_TIMES   210
#define XK8_CMD_LED_INT 187
#define XK8_CMD_FLSH_FR 180
#define XK8_CMD_SET_PID 204

//#define XK8_USAGE_PAGE 0xFFFF
//#define XK8_USAGE      0xFFFF
/* This is more correct based on later updates to the PI Engineering github */
#define XK8_USAGE_PAGE 0x000c
#define XK8_USAGE      0x0001

#include <iostream>
#include <QtCore/QtCore>
#include <PieHid32.h>

typedef enum {
	OFF   = 0,
	ON    = 1,
	BLINK = 2
} LEDMode;

typedef enum {
	GRN_LED = 6,
	RED_LED = 7
} PanelLED;

typedef unsigned int(*buttonCallback)(unsigned char*, unsigned int, unsigned int);
typedef unsigned int(*errorCallback)(unsigned int, unsigned int);

class XKey8 : public QObject {
	Q_OBJECT
public:
	XKey8(QObject *parent = 0);
    XKey8(int, QObject *parent = 0);
	virtual ~XKey8();

	bool         hasDevice()        { return (m_dev != NULL); }; // && m_dev->Handle != 0

    unsigned int getPID()           { return (hasDevice() ? m_dev->PID : 0); };
    unsigned int getUsage()         { return (hasDevice() ? m_dev->Usage : 0); };
    unsigned int getUP()            { return (hasDevice() ? m_dev->UP : 0); };
    long         getReadSize()      { return (hasDevice() ? m_dev->readSize : 0); };
    long         getWriteSize()     { return (hasDevice() ? m_dev->writeSize : 0);  };
    unsigned int getHandle()        { return (hasDevice() ? m_dev->Handle : 0); };
    unsigned int getVersion()       { return (hasDevice() ? m_dev->Version : 0); };
    QString      getDevicePath()    { return (hasDevice() ? m_devicePath : ""); };

    QString getProductString();
    QString getManufacturerString();
    void registerCallback(buttonCallback b) { m_bcb = b; }
    void registerErrorCallback(errorCallback e) { m_ecb = e; }

    bool isButtonDown(int num);
	void turnButtonLedsOff();
	void toggleButtonLEDState(int);

	bool handleErrorEvent(unsigned int deviceID, unsigned int status);
	bool handleDataEvent(unsigned char  *pData, unsigned int deviceID, unsigned int error);

    static int queryForDevices(vector<int>*);

public slots:
	void queryForDevice();

	void setBacklightIntensity(float blueBrightness);
	void setFlashFrequency(unsigned char freq);

	void setButtonBlueLEDState(quint8 buttonNumber, LEDMode mode);

	void setPanelLED(PanelLED ledNum, LEDMode mode);

signals:
	void panelDisconnected();
	void panelConnected();
    void panelConnected(int);

	void errorEvent(unsigned int  status);
	void dataEvent(unsigned char *pData);

	void buttonDown(int, unsigned int);
	void buttonUp(int);
    void buttonUp(int, int);
	void buttonUp(int, unsigned int, int);

protected:
	TEnumHIDInfo* getDevice() { return m_dev; }

private:
	void setupDevice(TEnumHIDInfo *dev);
	void processButtons(unsigned char *pData);
	bool isNotButtonNumber(int num);
	uint32_t dataToTime(unsigned char *pData);

	uint32_t sendCommand(unsigned char command, unsigned char data1 = 0, unsigned char data2 = 0, unsigned char data3 = 0);

	unsigned char* createDataBuffer();

	TEnumHIDInfo *m_dev;
	unsigned char *m_buttons;
	QVector<int> m_buttonTimes;
	QString m_devicePath;
	buttonCallback m_bcb;
	errorCallback m_ecb;
	QVector<LEDMode> m_buttonLedState;
    uint32_t m_handle;
};

#endif /* XKEY8_H_ */
