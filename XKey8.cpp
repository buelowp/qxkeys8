/*
 * XKey8.cpp
 *
 *  Created on: Jan 5, 2017
 *      Author: buelowp
 */

#include "XKey8.h"

XKey8::XKey8(QObject* parent) : QObject(parent)
{
	m_dev        = NULL;
	m_devicePath = "";
	m_bcb = NULL;
	m_ecb = NULL;

	m_buttons = new unsigned char[XK8_REPORT_LENGTH];
	for (int i = 0; i < 12; i++) {
		m_buttonTimes.push_back(0);
		m_buttonLedState.push_back(LEDMode::OFF);
	}
}

XKey8::~XKey8()
{
	if(hasDevice()) {
		m_devicePath = "";
		CloseInterface(m_dev->Handle);
	}

	//delete m_dev; responsiblity of piehid
	delete m_buttons;
}

// Slot:
void XKey8::queryForDevice()
{
	TEnumHIDInfo info[MAX_XKEY_DEVICES];
	long count;

	unsigned int res = EnumeratePIE(PI_VID, info, &count);

	if(res != 0) {
		std::cerr << "QXKeys: Error [" << res << "] Finding PI Engineering Devices." << std::endl;
		return;
	}

	// Test for change in current device connection:
	if(m_devicePath != "") {
		for(int i = 0; i < count; i++) {
			TEnumHIDInfo *d = &info[i];
			if(m_devicePath == (QString)(d->DevicePath)) { // uuid would be more appropriate
				setupDevice(d);
				return;
			}
		}
		qWarning() << "QXKeys: Device Disconnected from " << m_devicePath;
		m_dev = NULL;
		m_devicePath = "";

		emit panelDisconnected();
	}

	// Setup Interface:
	for(int i = 0; i < count && !hasDevice(); i++) {
		TEnumHIDInfo *d = &info[i];
		if((d->PID == XK8_PID1 || d->PID == XK8_PID2) &&
			d->UP == XK8_USAGE_PAGE && d->Usage == XK8_USAGE) {
			setupDevice(d);
			memset(m_buttons, 0, XK8_REPORT_LENGTH);
			sendCommand(XK8_CMD_DESC, 0);
			sendCommand(XK8_CMD_TIMES, true);
			emit panelConnected();
		}
	}
}

void XKey8::setupDevice(TEnumHIDInfo *d)
{
	char *p = d->DevicePath;
	long h  = d->Handle;
	unsigned int e;

	if(d == NULL) {
		return;
	}

	e = SetupInterfaceEx(h);
	if(e != 0) {
		std::cerr << __PRETTY_FUNCTION__ << ": Failed [" << e << "] Setting up PI Engineering Device at " << p << std::endl;
		return;
	}

	e = SetDataCallback(h, m_bcb);
	if(e != 0) {
		std::cerr << __PRETTY_FUNCTION__ << ": Critical Error [" << e << "] setting event callback for device at " << p << std::endl;
		return;
	}

	e = SetErrorCallback(h, m_ecb);
	if(e != 0) {
		std::cerr << __PRETTY_FUNCTION__ << ": Critical Error [" << e << "] setting error callback for device at " << p << std::endl;
		return;
	}

	SuppressDuplicateReports(h, false); // true?

	m_dev = d;
	m_devicePath = d->DevicePath;
}

bool XKey8::handleDataEvent(unsigned char *pData, unsigned int deviceID, unsigned int error)
{
	emit dataEvent(pData);

	// Constant 214
	if(pData[0] == 0 && pData[2] != 214)
	{
		processButtons(pData);
	}

	if(error != 0) {
		handleErrorEvent(deviceID, error);
	}

	return true;
}

bool XKey8::handleErrorEvent(unsigned int deviceID, unsigned int status)
{
	if(!hasDevice()) {
		return false;
	}

	char err[128];
	GetErrorString(status, err, 128);

	std::cerr << "QXKeys: ErrorEvent deviceID: " << deviceID << std::endl;
	qWarning() << err;

	emit errorEvent(status);

	queryForDevice();
	return true;
}

bool XKey8::isButtonDown(int num)
{
	if(!hasDevice() || isNotButtonNumber(num)) {
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
	if(!hasDevice()) {
		return;
	}

	sendCommand(XK8_CMD_FLSH_FR, freq);
}

void XKey8::setBacklightIntensity(float blueBrightness)
{
	if(!hasDevice()) {
		return;
	}

	// remove whole digits
	blueBrightness -= (int)blueBrightness;
	unsigned char bb = blueBrightness * 255;

	// Red backlight LEDs are bank 2
	sendCommand(XK8_CMD_LED_INT, bb);
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

void XKey8::setButtonBlueLEDState(quint8 ledNum, LEDMode mode)
{
	if(!hasDevice() || isNotButtonNumber(ledNum)) {
		return;
	}

	sendCommand(XK8_CMD_BTN_LED, ledNum, mode);
	m_buttonLedState[ledNum] = mode;
}

void XKey8::setPanelLED(PanelLED ledNum, LEDMode mode)
{
	if(!hasDevice()) {
		return;
	}

	quint8 n = (quint8)ledNum;
	quint8 m = (quint8)mode;

	sendCommand(XK8_CMD_PNL_LED, n, m);
}

bool XKey8::isNotButtonNumber(int num)
{
	return (!((num >= 0 && num <  6) | (num >= 8 && num < 10))) ;
}

void XKey8::processButtons(unsigned char *pData)
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

				emit buttonUp(buttonId);
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

unsigned char * XKey8::createDataBuffer()
{
	int length = GetWriteLength(m_dev->Handle);

	unsigned char *buffer = new unsigned char[length];
	memset(buffer, 0, length);

	return buffer;
}

uint32_t XKey8::sendCommand(unsigned char command, unsigned char data1,  unsigned char data2, unsigned char data3)
{
	if(!hasDevice()) {
		return -1;
	}

	unsigned char *buffer = createDataBuffer();

	buffer[0]  = 0; // constant
	buffer[1]  = command;
	buffer[2]  = data1;
	buffer[3]  = data2;
	buffer[4]  = data3;

	uint32_t result;
	result = WriteData(m_dev->Handle, buffer);

	if (result != 0) {
		std::cerr << "QXKeys: Write Error [" << result << "]  - Unable to write to Device at " << m_dev->DevicePath << std::endl;
		queryForDevice();
	}

	delete[] buffer;
	return result;
}

QString XKey8::getProductString()
{
	if(!hasDevice()) {
		return QString();
	}

	QString q = m_dev->ProductString;
	return q;
}


// public:
QString XKey8::getManufacturerString()
{
	if(!hasDevice()) {
		return QString();
	}

	QString q = m_dev->ManufacturerString;
	return q;
}

void XKey8::turnButtonLedsOff()
{
	  for (int i = 0; i < 10; i++) {
		  setButtonBlueLEDState(i, OFF);
	  }
}