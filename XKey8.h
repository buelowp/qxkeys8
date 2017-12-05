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

#define XK8_BUTTONS     10  // Technically, it's 8, but ordering skips 6 and 7, so the last button is 9 for 10 buttons
#define XK8_MAX_PANELS  4
#define XK8_MAX_BUTTONS (XK8_BUTTONS * XK8_MAX_PANELS)

#define MAX_XKEY_DEVICES        4

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
/*
struct TEnumHIDInfo {
    int Handle;
    int PID;
    int UP;
    char *DevicePath;
    int Usage;
    int readSize;
    int writeSize;
    int Version;
};
#define PI_VID                    0x5F3

typedef unsigned int (*PHIDDataEvent)(unsigned char *pData, unsigned int deviceID, unsigned int error);
typedef unsigned int (*PHIDErrorEvent)( unsigned int deviceID,unsigned int status);
unsigned int EnumeratePIE(long VID, TEnumHIDInfo *info, long *count);
unsigned int SetupInterfaceEx(long hnd);
unsigned int SetDataCallback(long hnd, PHIDDataEvent pDataEvent);
void GetErrorString(int err, char* out_str, int size);
void CloseInterface(long hnd);
unsigned int GetWriteLength(long hnd);
unsigned int WriteData(long hnd, unsigned char *data);
*/
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
	virtual ~XKey8();

	bool         hasDevice(int handle);

    unsigned int getPID(int h)           { return (hasDevice(h) ? m_deviceMap[h]->PID : 0); };
    unsigned int getUsage(int h)         { return (hasDevice(h) ? m_deviceMap[h]->Usage : 0); };
    unsigned int getUP(int h)            { return (hasDevice(h) ? m_deviceMap[h]->UP : 0); };
    long         getReadSize(int h)      { return (hasDevice(h) ? m_deviceMap[h]->readSize : 0); };
    long         getWriteSize(int h)     { return (hasDevice(h) ? m_deviceMap[h]->writeSize : 0);  };
    unsigned int getHandle(int h)        { return (hasDevice(h) ? m_deviceMap[h]->Handle : 0); };
    unsigned int getVersion(int h)       { return (hasDevice(h) ? m_deviceMap[h]->Version : 0); };
    QString      getDevicePath(int h)    { return (hasDevice(h) ? m_devicePathMap[h] : QString("")); };
    int          getHandleForButton(int b)
    {
        QMap<int, int>::iterator i = m_buttonHandleMap.find(b);
        if (i == m_buttonHandleMap.end())
            return -1;
        
        return i.value();
    }

    QString getProductString(int);
    QString getManufacturerString(int);
    void registerCallback(buttonCallback b) { m_bcb = b; }
    void registerErrorCallback(errorCallback e) { m_ecb = e; }

    bool isButtonDown(int num);
	void turnButtonLedsOff(int);
    void turnButtonLedsOff();
	void toggleButtonLEDState(int);

	bool handleErrorEvent(unsigned int deviceID, unsigned int status);
	bool handleDataEvent(unsigned char *pData, unsigned int deviceID, unsigned int error);

public slots:
	void queryForDevices();

	void setBacklightIntensity(float blueBrightness);
    void setBacklightIntensity(int, float);
	void setFlashFrequency(unsigned char freq);
    void setFlashFrequency(int, unsigned char);

	void setButtonBlueLEDState(int buttonNumber, LEDMode mode);

	void setPanelLED(PanelLED ledNum, LEDMode mode);

signals:
	void panelDisconnected();
	void panelConnected();
    void panelConnected(int);

	void errorEvent(int, unsigned int  status);
	void dataEvent(int, unsigned char *pData);

	void buttonDown(int, unsigned int);
	void buttonUp(int);
    void buttonUp(int, int);
	void buttonUp(int, unsigned int, int);

protected:
	TEnumHIDInfo* getDevice(int h)
    {
        QMap<int, TEnumHIDInfo*>::iterator i = m_deviceMap.find(h);
        if (i == m_deviceMap.end())
            return NULL;
        
        return i.value();
    }

private:
	bool setupDevice(TEnumHIDInfo *dev);
	void processButtons(int, unsigned char *pData);
	bool isNotButtonNumber(int num);
	uint32_t dataToTime(unsigned char *pData);
    int translatePanelButtonToGlobalButton(int, int);

	uint32_t sendCommand(int, unsigned char, unsigned char data1 = 0, unsigned char data2 = 0, unsigned char data3 = 0);

	unsigned char* createDataBuffer(int);

	QMap<int, TEnumHIDInfo*> m_deviceMap;        // Handle is the key
    QMap<int, int> m_buttonHandleMap;       // Button number is the key, handle is the value
    QMap<int, int> m_buttonTranslationMap;  // Global button is the key, panel button is the value
	unsigned char *m_buttons;
	QMap<int, int> m_buttonTimes;           // Button number is the key
	QMap<int, QString> m_devicePathMap;       // Handle is the key
    buttonCallback m_bcb;        // Handle is the key
	errorCallback m_ecb;         // Handle is the key
	QMap<int, LEDMode> m_buttonLedState;    // Button number is the key
};

#endif /* XKEY8_H_ */
