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

// device parameters
#define GMConnection			"GM_CONNECTION"
#define GMCell					"GM_CELL"
#define GMDeviceCount			"GM_DEVICE_COUNT"
#define GMZfreqValue			"GM_Z_FREQ_RBV"
#define GMCellState				"GM_CELL_STATE"
#define GMConnectionState		"GM_CONNECTION_STATE"
#define GMLabelString			"GM_LABEL"

// EIS measurement parameters
#define GMStartEis				"GM_START_EIS"
#define GMZfreq					"GM_Z_FREQ"
#define GMAmplitude				"GM_AMPLITUDE"
#define GMPrecision				"GM_PRECISION"
#define GMCyclesDelay			"GM_CYCLES_DELAY"

//#define DEFAULT_POLL_TIME		0.01
#define MAX_STRING_LEN			256

//
// Helper Functions
//
long NumberPrompt();
long SelectDevice(IGamryDeviceListPtr lpDeviceList, BSTR* Sections);
std::string EisStatusToString(long status);

/* Class definition for the Gamry Interface 1010e */
class Interface1010e : public asynPortDriver
{
public:
    Interface1010e(const char *portName, int pstatNum);

    /* These are the methods that we override from asynPortDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t nChars, size_t *nActual);
    //virtual void report(FILE *fp, int details);

    // These should be private but are called from C
    //virtual void pollerThread(void);

protected:
    // Device parameters
    int GMConnection_;
    int GMCell_;
    int GMDeviceCountValue_;
    int GMZfreqValue_;
    int GMCellState_;
    int GMConnectionState_;
    int GMLabelString_;

    // Digital I/O parameters
    //int digitalInput_;

    // EIS measurement parameters
    int GMStartEis_;
    int GMZfreq_;
    int GMAmplitude_;
    int GMPrecision_;
    int GMCyclesDealy_;

private:
    int pstatNum_;
    //double pollTime_;
    //int forceCallback_;

    // Instrument Interactions
    IGamryDeviceList* lpDeviceList;
    IGamryPstat* lpPstat;

    BSTR Sections[16];
    long instrument_index;

    // Data Acquisition
    IGamryDtaqEis* lpDtaq;
    gcDTAQEISSTATUS gm_status;

    int startEisMeas();
    int connectGamry();
    int disconnectGamry();

};

/*static void pollerThreadC(void * pPvt)
{
    Interface1010e *pInterface1010e = (Interface1010e *)pPvt;
    pInterface1010e->pollerThread();
}*/

/** Constructor for the Gamry Interface 1010e class
  */
Interface1010e::Interface1010e(const char *portName, int pstatNum)
    : asynPortDriver(portName, 1, 1,
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynDrvUserMask,  // Interfaces that we implement
                     asynInt32Mask | asynOctetMask,                                // Interfaces that do callbacks
                     ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=1, autoConnect=1 */
                     // this is how to start without ASYN_CANBLOCK
                     //ASYN_MULTIDEVICE, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=1, autoConnect=1 */
                     0, 0),  /* Default priority and stack size */
      pstatNum_(pstatNum)
      //pollTime_(DEFAULT_POLL_TIME),
      //forceCallback_(1)
{

    //pasynTrace->setTraceInfoMask(pasynUserSelf, ASYN_TRACEINFO_TIME | ASYN_TRACEINFO_THREAD);

    // Set up an exit handler, dont know the syntax yet...
    //epicsAtExit(disconnectGamry, (void*)this);

    // Device parameters
    createParam(GMConnection,			asynParamInt32,		&GMConnection_);
    createParam(GMCell,					asynParamInt32,		&GMCell_);
    createParam(GMDeviceCount,			asynParamInt32, 	&GMDeviceCountValue_);
    createParam(GMZfreqValue,			asynParamFloat64,	&GMZfreqValue_);
    createParam(GMCellState,			asynParamInt32, 	&GMCellState_);
    createParam(GMConnectionState,		asynParamInt32, 	&GMConnectionState_);
    createParam(GMLabelString,			asynParamOctet,		&GMLabelString_);

    // EIS measurement parameters
    createParam(GMStartEis,				asynParamInt32, 	&GMStartEis_);
    createParam(GMZfreq,				asynParamFloat64, 	&GMZfreq_);
    createParam(GMAmplitude,			asynParamFloat64, 	&GMAmplitude_);
    createParam(GMPrecision,			asynParamFloat64, 	&GMPrecision_);
    createParam(GMCyclesDelay,			asynParamInt32, 	&GMCyclesDealy_);

    // Set values of some parameters that need to be set because init record order is not predictable
    // or because the corresponding records are PINI=NO.
    setIntegerParam(GMStartEis_, 0);

    /* Start the thread to poll counters and digital inputs and do callbacks to
     * device support */
    /*epicsThreadCreate("Interface1010ePoller",
                      epicsThreadPriorityLow,
                      epicsThreadGetStackSize(epicsThreadStackMedium),
                      (EPICSTHREADFUNC)pollerThreadC,
                      this);*/

}

int Interface1010e::connectGamry()
{
    static const char *functionName = "connectGamry";

    // Initialize our COM instance
    // in the Console_Demo.cpp from Gamry it is "CoInitialize(NULL);", this crashes the IOC
    // we need to use the following in order to NOT block multithreading:
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

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
            disconnectGamry();
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
                  << "Connected " << std::endl
                  << std::endl;

        // take control of the selected device
        lpPstat->Open();

        // indicate it in the PV
        setIntegerParam(GMConnectionState_, 1);
        setStringParam(GMLabelString_, _bstr_t(lpPstat->Label()));

        //asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "There are %d Gamry device(s) connected to your system\n", lpDeviceList->Count());

        // Initialize the Dtaq object
        //lpDtaq->Init(lpPstat, frequency, amplitude, precision, 1);
        //gm_status = (gcDTAQEISSTATUS) lpDtaq->Result();
        //std::cout << "Gamry status is: " << EisStatusToString(gm_status) << std::endl;
        //std::cout << "Gamry cell is: " << lpPstat->Cell() << std::endl;
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
        disconnectGamry();
    }

    return 0;
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

int Interface1010e::disconnectGamry()
{
    static const char *functionName = "disconnectGamry";

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
    // indicate it in the PV
    setIntegerParam(GMConnectionState_, 0);
    //setStringParam(GMMessageString_, " ");
    std::cout << "Gamry device disconnected." << std::endl;

    return 0;
}

//
// EIS Experiment Setup (for now, only copy-paste from Gamrys example)
//
int Interface1010e::startEisMeas()
{
    double frequency, amplitude, precision;
    int paramNum=0, cyclesDelay;
    static const char *functionName = "startEisMeas";

    getDoubleParam (paramNum, GMZfreq_,    	&frequency);
    getDoubleParam (paramNum, GMAmplitude_,	&amplitude);
    getDoubleParam (paramNum, GMPrecision_,	&precision);
    getIntegerParam(paramNum, GMCyclesDealy_,	&cyclesDelay);

    // static cast<float> avoids c4244 warnings: conversion from 'double' to 'float', possible loss of data
    // maybe there is a better way to this ?
    lpDtaq->Init(lpPstat, static_cast<float>(frequency), static_cast<float>(amplitude),
                 static_cast<float>(precision), cyclesDelay);

    lpPstat->SetCell (CellOff);
    lpPstat->SetCtrlMode (PstatMode);
    lpPstat->SetIEStability (StabilityFast);
    lpPstat->SetCASpeed(1L);

    //lpPstat->SetCell(CellOn);
    //epicsThreadSleep(5.);
    //lpPstat->SetCell(CellOff);
    setIntegerParam(GMStartEis_, 1);

    return 0;
}

// Turn our enums into human-readable strings
std::string EisStatusToString(long gm_status)
{
    switch (gm_status)
    {
    case DtaqEISStatusInvalid:
        return "DtaqEISStatusInvalid";
    case DtaqEISStatusMeasOk:
        return "DtaqEISStatusMeasOk";
    case DtaqEISStatusCommErr:
        return "DtaqEISStatusCommErr";
    case DtaqEISStatusTimeout:
        return "DtaqEISStatusTimeout";
    case DtaqEISStatusCycleLim:
        return "DtaqEISStatusCycleLim";
    case DtaqEISStatusControl:
        return "DtaqEISStatusControl";
    case DtaqEISStatusOverrun:
        return "DtaqEISStatusOverrun";
    case DtaqEISStatusOverrange:
        return "DtaqEISStatusOverrange";
    case DtaqEISStatusOverrunQ:
        return "DtaqEISStatusOverrunQ";
    case DtaqEISStatusRetry:
        return "DtaqEISStatusRetry";
    case DtaqEISStatusDelay:
        return "DtaqEISStatusDelay";
    case DtaqEISStatusMeasuring:
        return "DtaqEISStatusMeasuring";
    default:
        return "Unknown";
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
    if (function == GMConnection_)
    {
        if (value)
        {
            status = connectGamry();
        }
        else
        {
            status = disconnectGamry();
        }
    }
    if (function == GMCell_)
    {
        if (value)
        {
            lpPstat->SetCell(CellOn);
            setIntegerParam(GMCellState_, 1);
        }
        else
        {
            lpPstat->SetCell(CellOff);
            setIntegerParam(GMCellState_, 0);
        }
    }
    if (function == GMStartEis_)
    {
        status = startEisMeas();
    }

    //std::cout << "PV has Value " << value << std::endl;
    callParamCallbacks(addr);
    if (status == 0)
    {
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s:%s, port %s, wrote %d to address %d\n",
                  driverName, functionName, this->portName, value, addr);
    }
    else
    {
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
    long longVal;
    //static const char *functionName = "readInt32";

    this->getAddress(pasynUser, &addr);
    //asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "There are %d Gamry device(s) connected to your system\n", lpDeviceList->Count());

    // Analog input function
    if (function == GMDeviceCountValue_)
    {
        longVal = lpDeviceList->Count();
        *value = longVal;
        setIntegerParam(addr, GMDeviceCountValue_, *value);
    }
    if (function == GMCellState_)
    {
        longVal = lpPstat->Cell();
        *value = longVal;
        setIntegerParam(addr, GMCellState_, *value);
    }

    // Other functions we call the base class method
    else
    {
        status = asynPortDriver::readInt32(pasynUser, value);
    }

    callParamCallbacks(addr);
    return (status==0) ? asynSuccess : asynError;
}

asynStatus Interface1010e::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
    int addr;
    int function = pasynUser->reason;
    int status=0;
    double doubleVal;
    //static const char *functionName = "readFloat64";

    this->getAddress(pasynUser, &addr);

    // Analog input function
    if (function == GMZfreqValue_)
    {
        doubleVal = lpDtaq->Zfreq();
        *value = doubleVal;
        setDoubleParam(addr, GMZfreqValue_, *value);
    }

    // Other functions we call the base class method
    else
    {
        status = asynPortDriver::readFloat64(pasynUser, value);
    }

    callParamCallbacks(addr);
    return (status==0) ? asynSuccess : asynError;
}

asynStatus Interface1010e::writeOctet(asynUser *pasynUser, const char *value,
                                      size_t nChars, size_t *nActual)
{
    int addr;
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    char labelString[MAX_STRING_LEN];
    const char *functionName = "writeOctet";

    this->getAddress(pasynUser, &addr);
    /* Set the parameter in the parameter library. */
    setStringParam(addr, function, (char *)value);

    setStringParam(GMLabelString_, labelString);


    /* Do callbacks so higher layers see any changes */
    status = (asynStatus)callParamCallbacks(addr, addr);

    if (status)
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                      "%s:%s: status=%d, function=%d, value=%s",
                      driverName, functionName, status, function, value);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s:%s: function=%d, value=%s\n",
                  driverName, functionName, function, value);
    *nActual = nChars;
    return status;

    // If this is NDFilePath call the base class because it may need to create the directory
    /*if (function == NDFilePath) {
        status = ADDriver::writeOctet(pasynUser, value, nChars, nActual);
    }

    if (currentlyAcquiring) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:%s: error, attempt to change setting while acquiring, function=%d, value=%s\n",
              driverName, functionName, function, value);
    }

    else if ((function == NDFilePath) ||
        (function == NDFileName) ||
        (function == NDFileTemplate)) {
        status = setFilePathAndName(false);
    }

    else if ((function == LFBackgroundPath_) ||
             (function == LFBackgroundFile_)) {
        status = setBackgroundFile();
    }

    else {
        /* If this parameter belongs to a base class call its method */
    //if (function < FIRST_LF_PARAM) status = ADDriver::writeOctet(pasynUser, value, nChars, nActual);
    //}
}

// disconnect from the device (unclean yet, i think...)
/*void Interface1010e::shutdown (void* arg) {
  //lpPstat->Close();
  std::cout << "Close the connection. Press any key." << std::endl;
  CoUninitialize();
  // wait for user input, don't close the command line yet
  std::cin.get(); // get one more char from the user
};*/

// TO-DO: poller thread
/*void Interface1010e::pollerThread()
{
  /* This function runs in a separate thread.  It waits for the poll
   * time */
/*static const char *functionName = "pollerThread";
epicsUInt32 newValue, changedBits, prevInput=0;
unsigned short biVal;;

while(1) {
  lock();

  // Read the digital inputs
biVal = lpPstat->Cell();
  newValue = biVal;
  changedBits = newValue ^ prevInput;
  if (forceCallback_ || (changedBits != 0)) {
    prevInput = newValue;
    forceCallback_ = 0;
  *value = longVal;
    setIntegerParam(addr, GMCellState_, *value);
    setUIntDigitalParam(digitalInput_, newValue, 0xFFFFFFFF);
  }
callParamCallbacks(0);
  unlock();
  epicsThreadSleep(pollTime_);
}
}

/** Configuration command, called directly or from iocsh */
extern "C" int Interface1010eConfig(const char *portName, int pstatNum)
{
    Interface1010e *pInterface1010e = new Interface1010e(portName, pstatNum);
    pInterface1010e = NULL;  /* This is just to avoid compiler warnings */
    return(asynSuccess);
}


static const iocshArg configArg0 = { "Port name", iocshArgString};
static const iocshArg configArg1 = { "potentiostat number", iocshArgInt};
static const iocshArg * const configArgs[] = {&configArg0,
                                              &configArg1
                                             };
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
