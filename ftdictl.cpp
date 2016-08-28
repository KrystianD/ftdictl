#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <tclap/CmdLine.h>

#include <string>
#include <vector>

using namespace std;

#include "ftdi/ftdi.h"
#include "ftdi/ftdi_i.h"
#include <libusb-1.0/libusb.h>

#define IOMODE 8

struct {
	string name;
	int val;
} functions[] = {
	{ "IOMODE", 8 },
	{ "KEEP_AWAKE", 21 },
	{ "DRIVE_0", 6 },
	{ "DRIVE_1", 7 },
	{ "TRISTATE", 0 },
};


struct ftdi_context *ftdi;
uint8_t vals = 0;

TCLAP::MultiArg<string>* setArg;
TCLAP::MultiArg<int>* inputArg;
TCLAP::MultiArg<int>* highArg;
TCLAP::MultiArg<int>* lowArg;
TCLAP::MultiArg<int>* toggleArg;
TCLAP::ValueArg<int>* getArg;
TCLAP::SwitchArg* dumpSwitch;
TCLAP::SwitchArg* detachSwitch;

std::vector<std::string> explode(const std::string& str, const std::string& delim, size_t maxCount = 0, size_t start = 0);

bool getPin(int pin)
{
	uint8_t vals;
	ftdi_read_pins(ftdi, &vals);
	return vals & (1 << pin);
}
void setPin(int pin, bool value)
{
	vals &= ~(1 << pin);
	vals |= (1 << (pin + 4)) | (value << pin);
	ftdi_set_bitmode(ftdi, vals, BITMODE_CBUS);
}
void setPinInput(int pin)
{
	vals &= ~((1 << (pin + 4)) | (1 << pin));
	ftdi_set_bitmode(ftdi, vals, BITMODE_CBUS);
}

string findFunctionNameByVal(int val)
{
	for (int i = 0; i < 5; i++) {
		if (functions[i].val == val)
			return functions[i].name;
	}
	char buf[10];
	sprintf(buf, "%d", val);
	return buf;
}

void printCBUSState(int num)
{
	int func = ftdi->eeprom->cbus_function[num];
	printf("CBUS%d: %s", num, findFunctionNameByVal(func).c_str());
	printf(" ");
	if (func == IOMODE) {
		bool state = getPin(num);
		if (state)
			printf("(HIGH)");
		else
			printf("(LOW)");
	}
	printf("\n");
}

void process()
{
	bool changed = false;
	const vector<string>& vals = setArg->getValue();
	for (size_t i = 0; i < vals.size(); i++) {
		vector<string> parts = explode(vals[i], ":");

		int num = atoi(parts[0].c_str());
		string& func = parts[1];

		if (num < 0 || num > 3)
			continue;

		for (int i = 0; i < 5; i++) {
			if (functions[i].name == func) {
				if (ftdi->eeprom->cbus_function[num] != functions[i].val) {
					ftdi->eeprom->cbus_function[num] = functions[i].val;
					changed = true;
					break;
				}
			}
		}
	}

	if (changed) {
		ftdi_eeprom_build(ftdi);
		ftdi_write_eeprom(ftdi);
		printf("settings has been changed, FTDI power cycle is required\n");
		return;
	}

	if (detachSwitch->isSet())
		return;

	const vector<int>& outputVals = highArg->getValue();
	for (size_t i = 0; i < outputVals.size(); i++) {
		int num = outputVals[i];
		if (num < 0 || num > 3)
			continue;

		printf("set HIGH %d\r\n", num);
		setPin(num, 1);
	}

	const vector<int>& lowVals = lowArg->getValue();
	for (size_t i = 0; i < lowVals.size(); i++) {
		int num = lowVals[i];
		if (num < 0 || num > 3)
			continue;

		printf("set LOW %d\r\n", num);
		setPin(num, 0);
	}

	const vector<int>& toggleVals = toggleArg->getValue();
	for (size_t i = 0; i < toggleVals.size(); i++) {
		int num = toggleVals[i];
		if (num < 0 || num > 3)
			continue;

		printf("toggle %d\r\n", num);
		setPin(num, !getPin(num));
	}

	const vector<int>& inputVals = inputArg->getValue();
	for (size_t i = 0; i < inputVals.size(); i++) {
		int num = inputVals[i];
		if (num < 0 || num > 3)
			continue;

		printf("set input %d\r\n", num);
		setPinInput(num);
	}

	if (dumpSwitch->isSet()) {
		unsigned int chipid;
		ftdi_read_chipid(ftdi, &chipid);
		printf("ftdi_read_chipid: %d 0x%08x\n", ftdi->type, chipid);
		printCBUSState(0);
		printCBUSState(1);
		printCBUSState(2);
		printCBUSState(3);
	}
}

int main(int argc, char** argv)
{
	TCLAP::CmdLine cmd("app");

	setArg = new TCLAP::MultiArg<string>("", "set", "set CBUS function", false, "NUM:FUNCTION", cmd);
	inputArg = new TCLAP::MultiArg<int>("", "input", "set CBUS GPIO as input", false, "NUM", cmd);
	highArg = new TCLAP::MultiArg<int>("", "high", "set CBUS GPIO as high", false, "NUM", cmd);
	lowArg = new TCLAP::MultiArg<int>("", "low", "set CBUS GPIO as low", false, "NUM", cmd);
	toggleArg = new TCLAP::MultiArg<int>("", "toggle", "toggle CBUS GPIO", false, "NUM", cmd);
	getArg = new TCLAP::ValueArg<int>("", "get", "get CBUS GPIO state", false, -1, "NUM", cmd);

	dumpSwitch = new TCLAP::SwitchArg("d", "dump", "dump FTDI data", cmd);
	detachSwitch = new TCLAP::SwitchArg("u", "detach", "detach FTDI data", cmd);

	try {
		cmd.parse(argc, argv);

	} catch (TCLAP::ArgException& e) {
		std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
		return 1;
	}

	int ret;
	if ((ftdi = ftdi_new()) == 0) {
		fprintf(stderr, "ftdi_new failed\n");
		return EXIT_FAILURE;
	}
	if ((ret = ftdi_usb_open(ftdi, 0x0403, 0x6015)) < 0) {
		fprintf(stderr, "unable to open ftdi device: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
		ftdi_free(ftdi);
		return EXIT_FAILURE;
	}

	libusb_set_auto_detach_kernel_driver(ftdi->usb_dev, 1);

	ftdi_read_eeprom(ftdi);
	ftdi_eeprom_decode(ftdi, 0);

	process();

	if ((ret = ftdi_usb_close(ftdi)) < 0) {
		fprintf(stderr, "unable to close ftdi device: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
		ftdi_free(ftdi);
		return EXIT_FAILURE;
	}
	ftdi_free(ftdi);
}

std::vector<std::string> explode(const std::string& str, const std::string& delim, size_t maxCount, size_t start)
{
	std::vector<std::string> parts;
	size_t idx = start, delimIdx;

	delimIdx = str.find(delim, idx);
	if (delimIdx == std::string::npos) {
		parts.push_back(str);
		return parts;
	}
	do {
		if (parts.size() == maxCount - 1) {
			std::string part = str.substr(idx);
			parts.push_back(part);
			idx = str.size();
			break;
		}
		std::string part = str.substr(idx, delimIdx - idx);
		parts.push_back(part);
		idx = delimIdx + delim.size();
		delimIdx = str.find(delim, idx);
	} while (delimIdx != std::string::npos && idx < str.size());

	if (idx < str.size()) {
		std::string part = str.substr(idx);
		parts.push_back(part);
	}

	return parts;
}
