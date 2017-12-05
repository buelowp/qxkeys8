/*
 * XKey8.cpp
 *
 *  Created on: Jan 5, 2017
 *      Author: buelowp
 */

#include "XKey8.h"

XKey8::XKey8(QObject* parent) : QObject(parent)
{
	m_buttons = new unsigned char[XK8_REPORT_LENGTH];
	for (int i = 0; i < XK8_MAX_BUTTONS; i++) {
		m_buttonTimes[i] = 0;
		m_buttonLedState[i] = LEDMode::OFF;
	}
}

XKey8::~XKey8()
{
    foreach (TEnumHIDInfo *panel, m_deviceMap) {
        CloseInterface(panel->Handle);
    }

    m_deviceMap.clear();
    m_devicePathMap.clear();
	//delete m_dev; responsiblity of piehid
	delete m_buttons;
}

bool XKey8::hasDevice(int handle)
{
    if (m_deviceMap.isEmpty())
        return false;

    QMap<int, TEnumHIDInfo*>::iterator i = m_deviceMap.find(handle);
    if (i == m_deviceMap.end())
        return false;
    
    return true;
}

// Slot:
void XKey8::queryForDevices()
{
	TEnumHIDInfo info[MAX_XKEY_DEVICES];
	long count;

	unsigned int res = EnumeratePIE(PI_VID, info, &count);

	if(res != 0) {
		std::cerr << "QXKeys: Error [" << res << "] Finding PI Engineering Devices." << std::endl;
		return;
	}
/* TODO: Need to figure out how to do this for multiple panels correctly
	// Test for change in current device connection:
	if(m_devicePath != "") {
		for(int i = 0; i < count; i++) {
			TEnumHIDInfo *d = &info[i];
			if(m_devicePath == (QString)(d->DevicePath)) { // uuid would be more appropriate
				setupDevice(d);
				return;
			}
		}
		qWarning() << "QXKeys: Device Disconnected from " << m_devicePath[handle];
		m_devicePath[handle];

		emit panelDisconnected(handle);
	}
*/
	// Setup Interface:
	for(int i = 0; i < count; i++) {
		TEnumHIDInfo *d = &info[i];
		if((d->PID == XK8_PID1 || d->PID == XK8_PID2) && d->UP == XK8_USAGE_PAGE) { // && d->UP == XK8_USAGE_PAGE && d->Usage == XK8_USAGE) {
            qDebug() << __PRETTY_FUNCTION__ << ": Found device with handle" << d->Handle;
            if (!hasDevice(d->Handle)) {
                setupDevice(d);
                memset(m_buttons, 0, XK8_REPORT_LENGTH);
                sendCommand(XK8_CMD_DESC, 0);
                sendCommand(XK8_CMD_TIMES, true);
                emit panelConnected(d->Handle);
            }
		}
	}
}

void XKey8::setupDevice(TEnumHIDInfo *d)
{
	char *p;
	int h;
	unsigned int e;

	if(d == NULL) {
        qWarning() << __PRETTY_FUNCTION__ << ": HID Info struct is NULL, this is not good";
		return;
	}

    QMap<int, buttonCallback>::const_iterator i = m_bcb.find(h);
    if (i == m_bcb.end()) {
        qWarning() << __PRETTY_FUNCTION__ << ": no data callback defined for handle" << h;
        return;
    }

    QMap<int, errorCallback>::const_iterator j = m_ecb.find(h);
    if (j == m_ecb.end()) {
        qWarning() << __PRETTY_FUNCTION__ << ": no error callback defined for handle" << h;
        return;
    }

    h = d->Handle;
	qDebug() << __PRETTY_FUNCTION__ << ": Device setup for PID" << d->PID << "with handle" << h;
    
    if ((e = SetupInterfaceEx(h)) != 0) {
		std::cerr << __PRETTY_FUNCTION__ << ": Failed [" << e << "] Setting up PI Engineering Device at " << p << std::endl;
		return;
	}

    if ((e = SetDataCallback(h, m_bcb[h]) != 0) {
		std::cerr << __PRETTY_FUNCTION__ << ": Critical Error [" << e << "] setting event callback for device at " << p << std::endl;
		return;
	}

    if ((e = SetErrorCallback(h, m_ecb[h])) != 0) {
		std::cerr << __PRETTY_FUNCTION__ << ": Critical Error [" << e << "] setting error callback for device at " << p << std::endl;
		return;
	}

	SuppressDuplicateReports(h, false); // true?

    int startButton = h * XK8_BUTTONS;
	m_deviceMap[h] = d;
	m_devicePathMap[h] = QString(d->DevicePath);
    for (int i = 0; i < XK8_BUTTONS; i++) {
        int globalButton = h * XK8_BUTTONS + i;
        m_buttonHandleMap[globalButton] = h;
        m_buttonTranslationMap[globalButton] = i;
    }
}

bool XKey8::handleDataEvent(unsigned char *pData, unsigned int deviceID, unsigned int error)
{
	emit dataEvent(deviceID, pData);

    qDebug() << __PRETTY_FUNCTION__ << ": got data for device ID" << deviceID;
	// Constant 214
	if(pData[0] == 0 && pData[2] != 214)
	{
		processButtons(deviceID, pData);
	}

	if(error != 0) {
		handleErrorEvent(deviceID, error);
	}

	return true;
}

bool XKey8::handleErrorEvent(unsigned int deviceID, unsigned int status)
{
	if(!hasDevice(deviceID)) {
		return false;
	}

    qDebug() << __PRETTY_FUNCTION__ << ": got and error for device ID" << deviceID;
	char err[128];
	GetErrorString(status, err, 128);

	std::cerr << "QXKeys: ErrorEvent deviceID: " << deviceID << std::endl;
	qWarning() << err;

	emit errorEvent(deviceID, status);

    /* TODO: Fix how this handles errors and add the call back in
	queryForDevices();
     */
	return true;
}

bool XKey8::isButtonDown(int num)
{
    int handle;
    
    if ((handle = getHandleForButton(num)) == -1)
        return false;
    
	if(!hasDevice(handle) || isNotButtonNumber(num)) {
		return false;
	}
	int col = 3 + num / 8; // button data stored in 3,4,5,6
	int row = num % 8;
	int bit = 1 << row;

	bool isdown = (m_buttons[col] & bit) > 0;

	return isdown;
}

void XKey8::setFlashFrequency(unsigned char freq)
{
    foreach (TEnumHIDInfo* panel, m_deviceMap) {
        sendCommand(panel->Handle, XK8_CMD_FLSH_FR, freq);
    }
}

void XKey8::setFlashFrequency(int handle, unsigned char freq)
{
    QMap<int, TEnumHIDInfo*>::iterator i = m_deviceMap.find(handle);
    if (i != m_deviceMap.end())
        sendCommand(handle, XK8_CMD_FLSH_FR, freq);
}

void XKey8::setBacklightIntensity(float blueBrightness)
{
    foreach (TEnumHIDInfo *panel, m_deviceMap) {
        // remove whole digits
        blueBrightness -= (int)blueBrightness;
        unsigned char bb = blueBrightness * 255;
        
        // Red backlight LEDs are bank 2
        sendCommand(panel->Handle, XK8_CMD_LED_INT, bb);
    }
}

void XKey8::setBacklightIntensity(int handle, float blueBrightness)
{
    QMap<int, TEnumHIDInfo*>::iterator i = m_deviceMap.find(handle);
    if (i != m_deviceMap.end()) {
        // remove whole digits
        blueBrightness -= (int)blueBrightness;
        unsigned char bb = blueBrightness * 255;
            
        // Red backlight LEDs are bank 2
        sendCommand(handle, XK8_CMD_LED_INT, bb);
    }
}
        
void XKey8::toggleButtonLEDState(int b)
{
	if (m_buttonLedState[b] == LEDMode::OFF) {
		setButtonBlueLEDState(b, LEDMode::ON);
		m_buttonLedState[b] = LEDMode::ON;
	}
	else {
		setButtonBlueLEDState(b, LEDMode::OFF);
		m_buttonLedState[b] = LEDMode::OFF;
	}
}

void XKey8::setButtonBlueLEDState(int ledNum, LEDMode mode)
{
    int handle;
    
    if ((handle = getHandleForButton(ledNum)) == -1)
        return;

    if(!hasDevice(handle) || isNotButtonNumber(ledNum)) {
		return;
	}

	sendCommand(handle, XK8_CMD_BTN_LED, ledNum, mode);
	m_buttonLedState[ledNum] = mode;
}

void XKey8::setPanelLED(PanelLED ledNum, LEDMode mode)
{
    int handle;
    
    if ((handle = getHandleForButton(ledNum)) == -1)
        return;
    
    if(!hasDevice(handle) || isNotButtonNumber(ledNum)) {
        return;
    }

	quint8 n = (quint8)ledNum;
	quint8 m = (quint8)mode;

	sendCommand(handle, XK8_CMD_PNL_LED, n, m);
}

bool XKey8::isNotButtonNumber(int num)
{
    QMap<int, int>::const_iterator i = m_buttonHandleMap.find(num);
    if (i != m_buttonHandleMap.end())
        return true;

    return false;
}

void XKey8::processButtons(int handle, unsigned char *pData)
{
	int columns = 4;
	int rows = 2;
	int d = 3; // start byte
	int buttonIndex = 0;
	int buttonId = 0;

	for(int i = 0; i < columns; i++) {
		for(int j = 0; j < rows; j++) {
			unsigned char bit = 1 << j;
			buttonIndex = i + (j * columns);
			if (buttonIndex > 5)
				buttonId = buttonIndex + 2;
			else
				buttonId = buttonIndex;

			bool wasdown = (m_buttons[i+d] & bit) > 0;
			bool  isdown = (pData[i+d] & bit) > 0;

			if( isdown && !wasdown ) {
				unsigned int time = dataToTime(pData);
				m_buttonTimes[buttonIndex] = time;

				emit buttonDown(buttonId, time);
			}
			if( !isdown && wasdown ) {
				unsigned int time = dataToTime(pData);
				int duration = time - m_buttonTimes[buttonIndex];
				m_buttonTimes[buttonIndex] = 0;

				qWarning() << __PRETTY_FUNCTION__ << ": button" << buttonId << ", time =" << time << ", duration =" << duration;

				emit buttonUp(handle, buttonId);
				emit buttonUp(buttonId, time, duration);
			}
		}

		// Update
		m_buttons[i+d] = pData[i+d];
	}
}

uint32_t XKey8::dataToTime(unsigned char *pData)
{
	return
	  (uint32_t)pData[7] << 24 |
      (uint32_t)pData[8] << 16 |
      (uint32_t)pData[9] << 8  |
      (uint32_t)pData[10];
}

unsigned char * XKey8::createDataBuffer(int handle)
{
    qDebug() << __PRETTY_FUNCTION__ << ": creating data buffer for handle" << handle;
	int length = GetWriteLength(handle);

	unsigned char *buffer = new unsigned char[length];
	memset(buffer, 0, length);

	return buffer;
}

uint32_t XKey8::sendCommand(int handle, unsigned char command, unsigned char data1,  unsigned char data2, unsigned char data3)
{
    unsigned char *buffer = createDataBuffer(handle);
    uint32_t result;

	if(!hasDevice(handle)) {
		return -1;
	}

	buffer[0]  = 0; // constant
	buffer[1]  = command;
	buffer[2]  = data1;
	buffer[3]  = data2;
	buffer[4]  = data3;

    qDebug() << __PRETTY_FUNCTION__ << ": writing data to handle" << handle;
    
	result = WriteData(handle, buffer);

	if (result != 0) {
		std::cerr << "QXKeys: Write Error [" << result << "]  - Unable to write to Device at " << m_dev->DevicePath << std::endl;
		queryForDevices();
	}

	delete[] buffer;
	return result;
}

QString XKey8::getProductString(int handle)
{
	if(!hasDevice(handle)) {
		return QString();
	}

	QString q = m_deviceMap[handle]->ProductString;
	return q;
}


// public:
QString XKey8::getManufacturerString(int handle)
{
	if(!hasDevice(handle)) {
		return QString();
	}

	QString q = m_deviceMap[handle]->ManufacturerString;
	return q;
}

void XKey8::turnButtonLedsOff()
{
    QList<int> buttons = m_deviceMap.keys();
    
    for (int i = 0; i < buttons.size(); i++) {
        setButtonBlueLEDState(i, OFF);
    }
}


