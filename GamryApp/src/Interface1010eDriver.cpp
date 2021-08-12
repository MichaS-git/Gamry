#include <windows.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <conio.h>
#include <time.h>

#include <epicsExit.h>
#include <iocsh.h>
#include <asynPortDriver.h>

#include <epicsExport.h>

//
// Import the Gamry Electrochemistry Toolkit
//
// Note the Intellisense workaround.  Compile once to get the GamryCom.tlh file
#include <GamryCom.tlh>
//
// Another way to import the Gamry Electrochemistry Toolkit: GamryCOM using our TypeLib GUID
// This is created when GamryCOM is registered during installation of the Gamry Software Suite.
//#import "libid:BD962F0D-A990-4823-9CF5-284D1CDD9C6D" no_namespace

static const char *driverName = "Interface1010e";

// output parameters
#define analogOutValueString      "ANALOG_OUT_VALUE"

// Analog input parameters
#define analogInValueString       "ANALOG_IN_VALUE"
#define analogInRangeString       "ANALOG_IN_RANGE"

//
// Helper Functions
//
long NumberPrompt();
long SelectDevice(IGamryDeviceListPtr lpDeviceList, BSTR* Sections);
void CleanupAndExit(int status, IGamryDeviceListPtr lpDeviceList, IGamryPstatPtr lpPstat, IGamryDtaqPtr lpDtaq);

/* Class definition for the Gamry Interface 1010e */
class Interface1010e : public asynPortDriver {
public:
  Interface1010e(const char *portName, int pstatNum);

  // exit the IOC with "exit()" disconnects the device properly
  static void shutdown(void *arg);
  // this shuld be the better way but epicsAtExit cant call CleanupAndExit with arguments...
  //void CleanupAndExit(int Status, IGamryDeviceListPtr lpDeviceList, IGamryPstatPtr lpPstat, IGamryDtaqPtr lpDtaq);

  /* These are the methods that we override from asynPortDriver */
  virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
  virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
  virtual asynStatus getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high);
  //virtual void report(FILE *fp, int details);

protected:
  // Analog output parameters
  int analogOutValue_;

  // Analog input parameters
  int analogInValue_;
  int analogInRange_;

private:
  int pstatNum_;

  // Instrument Interactions
  IGamryDeviceList* lpDeviceList;
  IGamryPstat* lpPstat;

  BSTR Sections[16];
  long instrument_index;

  // Data Acquisition
  IGamryDtaqEis* lpDtaq;

};

/** Constructor for the Gamry Interface 1010e class
  */
Interface1010e::Interface1010e(const char *portName, int pstatNum)
  : asynPortDriver(portName, 1, 1,
      asynInt32Mask | asynDrvUserMask,  // Interfaces that we implement
      0,                                // Interfaces that do callbacks
      ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=1, autoConnect=1 */
      0, 0),  /* Default priority and stack size */
    pstatNum_(pstatNum)
{

  // Set up an exit handler
  epicsAtExit(shutdown, (void*)this);

  // Analog output parameters
  createParam(analogOutValueString,            asynParamInt32, &analogOutValue_);

  // Analog input parameters
  createParam(analogInValueString,             asynParamInt32, &analogInValue_);
  createParam(analogInRangeString,             asynParamInt32, &analogInRange_);

  // Initialize our COM instance
  CoInitialize(NULL);

  try
  {
	std::cout << "Loading Gamry devices..." << std::endl;

	// Create instances of our interfaces
	CoCreateInstance(__uuidof(GamryDeviceList), NULL, CLSCTX_ALL, __uuidof(IGamryDeviceList), reinterpret_cast<void**>(&lpDeviceList));
	CoCreateInstance(__uuidof(GamryPstat), NULL, CLSCTX_ALL, __uuidof(IGamryPstat), reinterpret_cast<void**>(&lpPstat));
	CoCreateInstance(__uuidof(GamryDtaqEis), NULL, CLSCTX_ALL, __uuidof(IGamryDtaqEis), reinterpret_cast<void**>(&lpDtaq));

	if (!lpDeviceList || !lpPstat || !lpDtaq)
	{
		std::cout << "Could not find any devices, please check the connection!" << std::endl;
		CleanupAndExit(EXIT_FAILURE, lpDeviceList, lpPstat, lpDtaq);
	}

	// Ask the Device List how many devices are available.
	std::cout << "There are " << lpDeviceList->Count() << " Gamry device(s) connected to your system:" << std::endl;

	// Initialize our pstat object
	instrument_index = SelectDevice(lpDeviceList, Sections);
	lpPstat->Init(Sections[instrument_index]);

	// Display some information about the device
	std::cout << std::endl
		<< "Selected device: " << std::endl
		<< "Section:  " << _bstr_t(Sections[instrument_index]) << std::endl
		<< "Label:    " << _bstr_t(lpPstat->Label()) << std::endl
		<< "Serial:   " << _bstr_t(lpPstat->SerialNo()) << std::endl
		<< std::endl;

	// take control of the selected device
	lpPstat->Open();

  }
  catch (_com_error& e)
  {

	std::cout << std::showbase
		<< std::internal
		<< std::setfill('0')
		<< std::hex
		<< std::setw(8);

	if (e.Error() == 0xE000002E)
	{
		std::cout << "Selected device is not authorized for AC (" << e.Error() << ")" << std::endl;
	}
	else
	{
		std::cout << "There was an error (" << e.Error() << "): " << e.ErrorMessage() << std::endl;
	}
	CleanupAndExit(EXIT_FAILURE, lpDeviceList, lpPstat, lpDtaq);
  }

  //int value;
  //this->readInt32(pasynUserSelf, &value);

}

asynStatus Interface1010e::getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high)
{
  int function = pasynUser->reason;

  // Both the analog outputs and analog inputs are 16-bit devices
  if ((function == analogOutValue_) ||
      (function == analogInValue_)) {
    *low = 0;
    *high = 65535;
    return(asynSuccess);
  } else {
    return(asynError);
  }
}

asynStatus Interface1010e::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  int addr;
  int function = pasynUser->reason;
  int status=0;
  static const char *functionName = "writeInt32";

  this->getAddress(pasynUser, &addr);
  setIntegerParam(addr, function, value);

  // Analog output functions
  if (function == analogOutValue_) {
    //lpDeviceList->Count();
	std::cout << "There are " << lpDeviceList->Count() << " Gamry device(s) connected to your system:" << std::endl;
  }

  callParamCallbacks(addr);
  if (status == 0) {
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
             "%s:%s, port %s, wrote %d to address %d\n",
             driverName, functionName, this->portName, value, addr);
  } else {
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "%s:%s, port %s, ERROR writing %d to address %d, status=%d\n",
             driverName, functionName, this->portName, value, addr, status);
  }
  return (status==0) ? asynSuccess : asynError;
}

asynStatus Interface1010e::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
  int addr;
  int function = pasynUser->reason;
  int status=0;
  unsigned short shortVal;
  int range;
  static const char *functionName = "readInt32";

  this->getAddress(pasynUser, &addr);
  asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "There are %d Gamry device(s) connected to your system\n", lpDeviceList->Count());

  // Analog input function
  if (function == analogInValue_) {
    getIntegerParam(addr, analogInRange_, &range);
    //status = cbAIn(boardNum_, addr, range, &shortVal);
    //shortVal = lpDeviceList->Count();
    //setIntegerParam(addr, analogInValue_, lpDeviceList->Count());
	std::cout << "There are " << lpDeviceList->Count() << " Gamry device(s) connected to your system." << std::endl;
  }

  // Other functions we call the base class method
  else {
     status = asynPortDriver::readInt32(pasynUser, value);
  }

  callParamCallbacks(addr);
  return (status==0) ? asynSuccess : asynError;
}

// disconnect from the device
void Interface1010e::shutdown (void* arg) {
  //lpPstat->Open();
  //std::cout << "Closed the connection, wait a bit..." << std::endl;
  //epicsThreadSleep(5.0);
  CoUninitialize();
};

// Stop the expriment and release any COM pointers we have lying around
void CleanupAndExit(int Status, IGamryDeviceListPtr lpDeviceList, IGamryPstatPtr lpPstat, IGamryDtaqPtr lpDtaq)
{
	if (lpDtaq)
	{
		try
		{
			lpDtaq->Stop();
		}
		catch (_com_error& e)
		{
			std::cout << "Dtaq exception during cleanup (" << e.Error() << "): " << e.ErrorMessage() << std::endl;
		}

		lpDtaq->Release();
	}

	if (lpPstat)
	{
		try
		{
			lpPstat->SetCell(CellOff);
			lpPstat->Close(VARIANT_TRUE);
		}
		catch (_com_error& e)
		{
			std::cout << "Pstat exception during cleanup (" << e.Error() << "): " << e.ErrorMessage() << std::endl;
		}

		lpPstat->Release();
	}

	if (lpDeviceList)
		lpDeviceList->Release();

	CoUninitialize();

	//exit(Status);
}

// Menu to select an instrument if multiple connected.
// Automatically selects a single instrument if only one connected.
long SelectDevice(IGamryDeviceListPtr lpDeviceList, BSTR* Sections)
{
	SAFEARRAY* psa;

	// Ask the device list for the section names
	psa = lpDeviceList->EnumSections();

	for (long i = 0; i < lpDeviceList->Count(); ++i)
	{
		SafeArrayGetElement(psa, &i, &Sections[i]);
		std::cout << i << ". " << _bstr_t(Sections[i]) << std::endl;
	}

	SafeArrayDestroy(psa);

	if (lpDeviceList->Count() > 1)
	{
		return NumberPrompt();
	}
	else
	{
		std::cout << " Device 0 automatically selected." << std::endl;
		return 0;
	}
}

// Prompt for a number, do basic input checking
long NumberPrompt()
{
	char ch = 'A';

	while (!isdigit(ch))
	{
		std::cout << "Select: ";
		ch = _getch();
	}

	long num = ch - '0';

	std::cout << num << std::endl;

	return num;
}

/** Configuration command, called directly or from iocsh */
extern "C" int Interface1010eConfig(const char *portName, int pstatNum)
{
  Interface1010e *pInterface1010e = new Interface1010e(portName, pstatNum);
  pInterface1010e = NULL;  /* This is just to avoid compiler warnings */
  return(asynSuccess);
}


static const iocshArg configArg0 = { "Port name",      iocshArgString};
static const iocshArg configArg1 = { "potentiostat number",      iocshArgInt};
static const iocshArg * const configArgs[] = {&configArg0,
                                              &configArg1};
static const iocshFuncDef configFuncDef = {"Interface1010eConfig", 2, configArgs};
static void configCallFunc(const iocshArgBuf *args)
{
  Interface1010eConfig(args[0].sval, args[1].ival);
}

void drvInterface1010eRegister(void)
{
  iocshRegister(&configFuncDef,configCallFunc);
}

extern "C" {
epicsExportRegistrar(drvInterface1010eRegister);
}
