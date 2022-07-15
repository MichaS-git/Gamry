/*Interface1010eDriver.cpp
 *
 *This is a driver for the Interface 1010E potentiostat from Gamry.
 *It is using the Gamry Electrochemistry Toolkit that needs to be installed before using
 *this driver. The Gamry Explain Scripts (Experimental Control Language)
 *were used as templates for the measurement functions.
 *
 *
 *
 *
 */

#include <windows.h>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <conio.h>
#include <time.h>
#include <math.h>

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

using namespace std;

static const char *driverName = "Interface1010e";

// device parameters
#define GMConnection			"GM_CONNECTION"
#define GMCell					"GM_CELL"
#define GMDeviceCount			"GM_DEVICE_COUNT"
#define GMLabelString			"GM_LABEL"
#define GMSerialString			"GM_SERIAL"
#define GMModelString			"GM_MODEL"
#define GMCalACString			"GM_CAL_AC"
#define GMCalDCString			"GM_CAL_DC"
#define GMExpType				"GM_EXP_TYPE"
#define GMStartExp				"GM_START_EXP"
#define GMState				    "GM_STATE"

// EIS measurement parameters
#define EISFreqInit				"EIS_FREQ_INIT"
#define EISFreqFinal			"EIS_FREQ_FINAL"
#define EISPointDensity			"EIS_POINT_DENSITY"
#define EISACAmpl			    "EIS_AC_AMPL"
#define EISDCAmpl			    "EIS_DC_AMPL"
#define EISArea			        "EIS_AREA"
#define EISZGuess			    "EIS_Z_GUESS"
#define EISSpeed			    "EIS_SPEED"
// EIS measurement output
#define EISZFreq			    "EIS_Z_FREQ"
#define EISZReal			    "EIS_Z_REAL"
#define EISZImag			    "EIS_Z_IMAG"
#define EISZSig			        "EIS_Z_SIG"
#define EISZMod			        "EIS_Z_MOD"
#define EISZPhz			        "EIS_Z_PHZ"
#define EISIdc			        "EIS_I_DC"
#define EISVdc			        "EIS_V_DC"
#define EISIERange			    "EIS_IE_RANGE"

// DC measurement parameters
#define GMVoltage				"GM_VOLTAGE"
#define GMTotalTime				"GM_TOTAL_TIME"
#define GMSampleRate			"GM_SAMPLE_RATE"

//#define DEFAULT_POLL_TIME		0.01
#define MAX_STRING_LEN			256

//
// Helper Functions
//
long NumberPrompt();
void Delay(std::string message, long delay, long interval);
long SelectDevice(IGamryDeviceListPtr lpDeviceList, BSTR* Sections);
std::string EisStatusToString(long status);
std::string ReadzStatusToString(long status);

/* Class definition for the Gamry Interface 1010e */
class Interface1010e : public asynPortDriver
{
public:
    Interface1010e(const char *portName, int pstatNum);

    /* These are the methods that we override from asynPortDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    //virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
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
    int GMLabelString_;
    int GMSerialString_;
    int GMModelString_;
    int GMCalACString_;
    int GMCalDCString_;
    int GMExpType_;
    int GMStartExp_;
    int GMState_;

    // Digital I/O parameters
    //int digitalInput_;

    // EIS measurement parameters
    int EISFreqInit_;
    int EISFreqFinal_;
    int EISPointDensity_;
    int EISACAmpl_;
    int EISDCAmpl_;
    int EISArea_;
    int EISZGuess_;
    int EISSpeed_;
    // EIS measurement output
    int EISZFreq_;
    int EISZReal_;
    int EISZImag_;
    int EISZSig_;
    int EISZMod_;
    int EISZPhz_;
    int EISIdc_;
    int EISVdc_;
    int EISIERange_;

    // DC measurement parameters
    int GMVoltage_;
    int GMTotalTime_;
    int GMSampleRate_;

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
    IGamryReadZ* lpReadZ;
    //_IGamryReadZEvents* lpReadZEvents;
    IGamryDtaqEis* lpDtaqEis;
    IGamryDtaqIvt* lpDtaqIvt;
    gcDTAQEISSTATUS gm_status;
    IGamrySignalConst* lpSignal;
    IGamrySignal* lpSignalBase;

    int connectGamry();
    int cleanupAndExit();
    int startExperiment();
    int printHardwareSettings();
    int eisMeasurement();
    int DCMeasurement();

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
                     asynInt32Mask | asynFloat64Mask | asynOctetMask,                    // Interfaces that do callbacks
                     ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=1, autoConnect=1 */
                     // this is how to start without ASYN_CANBLOCK
                     //ASYN_MULTIDEVICE, 1, /* ASYN_MULTIDEVICE=1, autoConnect=1 */
                     0, 0),  /* Default priority and stack size */
      pstatNum_(pstatNum)
      //pollTime_(DEFAULT_POLL_TIME),
      //forceCallback_(1)
{

    //pasynTrace->setTraceInfoMask(pasynUserSelf, ASYN_TRACEINFO_TIME | ASYN_TRACEINFO_THREAD);

    // Set up an exit handler, dont know the syntax yet...
    //epicsAtExit(cleanupAndExit, (void*)this);

    // Device parameters
    createParam(GMConnection,			asynParamInt32,		&GMConnection_);
    createParam(GMCell,					asynParamInt32,		&GMCell_);
    createParam(GMDeviceCount,			asynParamInt32, 	&GMDeviceCountValue_);
    createParam(GMLabelString,			asynParamOctet,		&GMLabelString_);
    createParam(GMSerialString,			asynParamOctet,		&GMSerialString_);
    createParam(GMModelString,			asynParamOctet,		&GMModelString_);
    createParam(GMCalACString,			asynParamOctet,		&GMCalACString_);
    createParam(GMCalDCString,			asynParamOctet,		&GMCalDCString_);
    createParam(GMExpType,				asynParamInt32,		&GMExpType_);
    createParam(GMStartExp,				asynParamInt32, 	&GMStartExp_);
    createParam(GMState,				asynParamInt32, 	&GMState_);

    // EIS measurement parameters
    createParam(EISFreqInit,			asynParamFloat64, 	&EISFreqInit_);
    createParam(EISFreqFinal,			asynParamFloat64, 	&EISFreqFinal_);
    createParam(EISPointDensity,		asynParamFloat64, 	&EISPointDensity_);
    createParam(EISACAmpl,			    asynParamFloat64, 	&EISACAmpl_);
    createParam(EISDCAmpl,			    asynParamFloat64, 	&EISDCAmpl_);
    createParam(EISArea,			    asynParamFloat64, 	&EISArea_);
    createParam(EISZGuess,			    asynParamFloat64, 	&EISZGuess_);
    createParam(EISSpeed,			    asynParamInt32, 	&EISSpeed_);
    // EIS measurement output
    createParam(EISZFreq,			    asynParamFloat64,	&EISZFreq_);
    createParam(EISZReal,			    asynParamFloat64,	&EISZReal_);
    createParam(EISZImag,			    asynParamFloat64,	&EISZImag_);
    createParam(EISZSig,			    asynParamFloat64,	&EISZSig_);
    createParam(EISZMod,			    asynParamFloat64,	&EISZMod_);
    createParam(EISZPhz,			    asynParamFloat64,	&EISZPhz_);
    createParam(EISIdc,			        asynParamFloat64,	&EISIdc_);
    createParam(EISVdc,			        asynParamFloat64,	&EISVdc_);
    createParam(EISIERange,			    asynParamInt32,	    &EISIERange_);

    // DC measurement parameters
    createParam(GMVoltage,				asynParamFloat64, 	&GMVoltage_);
    createParam(GMTotalTime,			asynParamFloat64, 	&GMTotalTime_);
    createParam(GMSampleRate,			asynParamFloat64, 	&GMSampleRate_);

    // Set values of some parameters that need to be set because init record order is not predictable
    // or because the corresponding records are PINI=NO.
    setIntegerParam(GMStartExp_, 0);

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

        if (!lpDeviceList || !lpPstat)
        {
            std::cout << "Could not find any devices, please check the connection!" << std::endl;
            cleanupAndExit();
        }

        // Ask the Device List how many devices are available.
        std::cout << "There are " << lpDeviceList->Count() << " Gamry device(s) connected to your system:" << std::endl;

        // Initialize our pstat object
        instrument_index = SelectDevice(lpDeviceList, Sections);
        lpPstat->Init(Sections[instrument_index]);

        // Display some information about the device
        std::cout << std::endl
                  << "Selected device: "    << std::endl
                  << "Section:  "           << _bstr_t(Sections[instrument_index]) << std::endl
                  << "Label:    "           << _bstr_t(lpPstat->Label()) << std::endl
                  << "Serial:   "           << _bstr_t(lpPstat->SerialNo()) << std::endl
                  << "Model:   "            << _bstr_t(lpPstat->ModelNo()) << std::endl
                  << "Calibration DC:   "   << _bstr_t(lpPstat->CalDate(0)) << std::endl
                  << "Calibration AC:   "   << _bstr_t(lpPstat->CalDate(1)) << std::endl
                  << "Connected "           << std::endl
                  << std::endl;

        // take control of the selected device
        lpPstat->Open();

        // set the values to the PVs
        setStringParam(GMLabelString_, _bstr_t(lpPstat->Label()));
        setStringParam(GMSerialString_, _bstr_t(lpPstat->SerialNo()));
        setStringParam(GMModelString_, _bstr_t(lpPstat->ModelNo()));
        setStringParam(GMCalACString_, _bstr_t(lpPstat->CalDate(1)));
        setStringParam(GMCalDCString_, _bstr_t(lpPstat->CalDate(0)));

        //asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "There are %d Gamry device(s) connected to your system\n", lpDeviceList->Count());
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
        cleanupAndExit();
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

int Interface1010e::cleanupAndExit()
{
    static const char *functionName = "cleanupAndExit";

    if (lpDtaqEis)
    {
        try
        {
            lpDtaqEis->Stop();
        }
        catch (_com_error& e)
        {
            std::cout << "Dtaq exception during cleanup (" << e.Error() << "): " << e.ErrorMessage() << std::endl;
        }

        lpDtaqEis->Release();
    }

    if (lpDtaqIvt)
    {
        try
        {
            lpDtaqIvt->Stop();
        }
        catch (_com_error& e)
        {
            std::cout << "Dtaq exception during cleanup (" << e.Error() << "): " << e.ErrorMessage() << std::endl;
        }

        lpDtaqIvt->Release();
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

    std::cout << "Gamry device disconnected." << std::endl;

    return 0;
}

int Interface1010e::startExperiment()
{
    int expType;
    static const char *functionName = "startExperiment";

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s:%s: entry\n", driverName, functionName);
    getIntegerParam(GMExpType_, &expType);
    setIntegerParam(GMState_, 1);
    callParamCallbacks();

    //std::cout << "expType has Value " << expType << std::endl;

    if (expType == 0)
    {
        DCMeasurement();
    }
    if (expType == 1)
    {
        eisMeasurement();
    }

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s:%s: exit\n", driverName, functionName);
    return asynSuccess;
}

int Interface1010e::printHardwareSettings()
{
    // taken from ifunction PrintHardwareSettings () - in pc6.exp
    std::cout << std::endl
              << "Model:  "                 << lpPstat->ModelNo() << std::endl
              << "Section:    "             << lpPstat->Section() << std::endl
              << "Serial Number:   "        << lpPstat->SerialNo() << std::endl
              << "Control Mode:   "         << lpPstat->CtrlMode() << std::endl
              << "Electrometer:   "         << lpPstat->Electrometer() << std::endl
              << "I/E Stability:   "        << lpPstat->IEStability() << std::endl
              << "Control Amp Speed: "      << lpPstat->CASpeed() << std::endl
              << "Current Convention: "     << lpPstat->IConvention() << std::endl
              << "Ich Range: "              << lpPstat->IchRange() << std::endl
              << "Ich Auto Range: "         << lpPstat->IchRangeMode() << std::endl
              << "Ich Offset Enable: "      << lpPstat->IchOffsetEnable() << std::endl
              << "Ich Offset (V): "         << lpPstat->IchOffset() << std::endl
              << "Ich Filter: "             << lpPstat->IchFilter() << std::endl
              << "Vch Range: "              << lpPstat->VchRange() << std::endl
              << "Vch Auto Range: "         << lpPstat->VchRangeMode() << std::endl
              << "Vch Offset Enable: "      << lpPstat->VchOffsetEnable() << std::endl
              << "Vch Offset (V): "         << lpPstat->VchOffset() << std::endl
              << "Vch Filter: "             << lpPstat->VchFilter() << std::endl
              << "I/E Range Lower Limit: "  << lpPstat->IERangeLowerLimit() << std::endl
              << "I/E AutoRange: "          << lpPstat->IERangeMode() << std::endl
              << "I/E Range: "              << lpPstat->IERange() << std::endl
              << "Ach Select: "             << lpPstat->AchSelect() << std::endl
              << "Ach Range: "              << lpPstat->AchRange() << std::endl
              << "Ach Offset Enable: "      << lpPstat->AchOffsetEnable() << std::endl
              << "Ach Offset (V): "         << lpPstat->AchOffset() << std::endl
              << "Ach Filter: "             << lpPstat->AchFilter() << std::endl
              << "Sense Cable ID: "         << lpPstat->CableId(CableIdSelectMain) << std::endl
              << "Power Cable ID: "         << lpPstat->CableId(CableIdSelectPwr) << std::endl
              << "DC Calibration Date: "    << lpPstat->CalDate(0) << std::endl
              << "AC Calibration Date: "    << lpPstat->CalDate(1) << std::endl
              << "SenseSpeedMode: "         << lpPstat->SenseSpeedMode() << std::endl
              << std::endl;

    return 0;
}

//
// EIS Experiment Setup (procedure taken from .../Gamry Instruments/Framework/Scripts/PotentiostaticEIS.exp)
//
int Interface1010e::eisMeasurement()
{
    double freqInit, freqFinal, pointDensity, acAmpl, dcAmpl, area, zGuess, IERange;
    float fLimUpper, fLimLower, freq, logIncrement, zReal;
    int point = 0, speed, maxPoints, tries;
    const int pollEis = 100;
    gcREADZSTATUS status;
    ofstream datafile;

    static const char *functionName = "eisMeasurement";

    getDoubleParam (EISFreqInit_,       &freqInit);
    getDoubleParam (EISFreqFinal_,      &freqFinal);
    getDoubleParam (EISPointDensity_,   &pointDensity);
    getDoubleParam (EISACAmpl_,         &acAmpl);
    getDoubleParam (EISDCAmpl_,         &dcAmpl);
    getDoubleParam (EISArea_,           &area);
    getDoubleParam (EISZGuess_,         &zGuess);

    getIntegerParam(EISSpeed_, &speed);

    // Frequency limits of the device, probably it's better to write the Limits to the DRVH/DRVL fields of the PV but how??
    fLimUpper = lpPstat->FreqLimitUpper ();
    fLimLower = lpPstat->FreqLimitLower ();

    if (freqInit > fLimUpper)
    {
        std::cout << "Warning, initial frequency exceeds upper limit -  " << fLimUpper << std::endl;
        freqInit = fLimUpper;
    }
    if (freqFinal > fLimUpper)
    {
        std::cout << "Warning, final frequency exceeds upper limit -  " << fLimUpper << std::endl;
        freqFinal = fLimUpper;
    }
    if (freqInit < fLimLower)
    {
        std::cout << "Warning, initial frequency exceeds lower limit -  " << fLimLower << std::endl;
        freqInit = fLimLower;
    }
    if (freqFinal < fLimLower)
    {
        std::cout << "Warning, final frequency exceeds lower limit -  " << fLimLower << std::endl;
        freqFinal = fLimLower;
    }

    // calculates the number of points for a EIS curve (function CheckEISPoints - in CommonFunctions.exp)
    maxPoints = (int)round(1 + abs(log10 (freqFinal) - log10 (freqInit)) * pointDensity);

    // create and initialize a GamryReadZ instance for data-aquisition
    CoCreateInstance(__uuidof(GamryReadZ), NULL, CLSCTX_ALL, __uuidof(IGamryReadZ), reinterpret_cast<void**>(&lpReadZ));
    lpReadZ->Init(lpPstat);

    if (speed == 0)
    {
        lpReadZ->SetSpeed(ReadZSpeedFast);
    }
    if (speed == 1)
    {
        lpReadZ->SetSpeed(ReadZSpeedNorm);
    }
    if (speed == 2)
    {
        lpReadZ->SetSpeed(ReadZSpeedLow);
    }

    // events from GamryReadZ instance come from here (don't know how to use OnDataAvailable/OnDataDone)
    //CoCreateInstance(__uuidof(GamryReadZ), NULL, CLSCTX_ALL, __uuidof(_IGamryReadZEvents), reinterpret_cast<void**>(&lpReadZEvents));

    // InitializePstat
    lpPstat->SetCell (CellOff);
    lpPstat->SetThermoSelect (ThermoDegreesC);
    lpPstat->SetElectrometer (ElectrometerNormal);
    lpPstat->SetAchSelect (GND);
    lpPstat->SetCtrlMode (PstatMode);
    lpPstat->SetIEStability (StabilityFast);
    lpPstat->SetCASpeed(100000.); // CASpeedMedFast
    lpPstat->SetSenseSpeedMode (1);
    lpPstat->SetIConvention (Anodic);
    lpPstat->SetGround (Float);
    lpPstat->SetIchRange(3.0);
    lpPstat->SetIchRangeMode (0);
    lpPstat->SetIchFilter (2.5);
    lpPstat->SetVchRange(3.0);
    lpPstat->SetVchRangeMode (0);

    lpPstat->SetIchOffsetEnable (1); //2.94
    lpPstat->SetVchOffsetEnable (1); //2.94

    lpPstat->SetVchFilter (2.5);
    lpPstat->SetAchRange (3.0);
    lpPstat->SetIERangeLowerLimit(0);
    lpPstat->SetIERange (0.03);
    lpPstat->SetIERangeMode (0);
    lpPstat->SetAnalogOut (0.0);
    lpPstat->SetVoltage ((float)dcAmpl);
    lpPstat->SetPosFeedEnable (0);
    lpPstat->SetIruptMode (IruptOff, EuNone, 0., 0., 1.);

    IERange = lpPstat->TestIERange ((float)(dcAmpl / zGuess));
    lpPstat->SetIERange (IERange);

    //printHardwareSettings();

    // Turn the cell on before acquiring data
    std::cout << "Turning cell on..." << std::endl;
    lpPstat->SetCell(CellOn);

    // Setup initial guesses for VGS algorithm
    lpReadZ->SetGain (1.0);
    lpReadZ->SetINoise (0.0);
    lpReadZ->SetVNoise (0.0);
    lpReadZ->SetIENoise (0.0);
    lpReadZ->SetZmod ((float)zGuess);

    //std::cout << "MeasureI has Value " << lpPstat->MeasureI() << std::endl;
    lpReadZ->SetIdc (lpPstat->MeasureI());
    lpReadZ->SetCycleLim(2, 10);

    logIncrement = (float)(1.0 / pointDensity);
    if (freqInit > freqFinal)
    {
        logIncrement = -logIncrement;
    }

    // Run the Experiment
    // Do initial 1st point check
    lpReadZ->Measure ((float)freqInit, (float)(acAmpl * 0.001));

    // open a Datafile to write to
    datafile.open ("EISPOT.DTA");
    // Column header
    datafile    << "Pt" << '\t'
                << "Freq" << '\t'
                << "Zreal" << '\t'
                << "Zimag" << '\t'
                << "Zsig" << '\t'
                << "Zmod" << '\t'
                << "Zphz" << '\t'
                << "Idc" << '\t'
                << "Vdc" << '\t'
                << "IERange" << '\t'
                << '\n'
                << "#" << '\t'
                << "Hz" << '\t'
                << "Ohm" << '\t'
                << "Ohm" << '\t'
                << "V" << '\t'
                << "Ohm" << '\t'
                << "deg" << '\t'
                << "A" << '\t'
                << "V" << '\t'
                << "#" << '\t'
                << '\n';

    //pointCount = pointsToCook;

    while (point < maxPoints)
    {
        // calculate the current frequency
        freq = (float)pow(10.0, log10 (freqInit) + point * logIncrement);

        //limit this frequency to Pstat's allowed range
        if (freq < fLimLower)
        {
            freq = fLimLower;
        }
        if (freq > fLimUpper)
        {
            freq = fLimUpper;
        }

        status = (gcREADZSTATUS) lpReadZ->Measure (freq, (float)(acAmpl * 0.001));
        //std::cout << "before delay: " << _bstr_t(lpReadZ->StatusMessage()) <<std::endl;

        // Comment from example Gamry_DC_Console_Demo.cpp:
        // This simple example uses polling, so delay a few seconds, giving the experiment time to take measurement
        // In a more robust application, use DataAvailable events.
        Sleep(pollEis);

        // There are no examples from Gamry how to use DataAvailable events. We read Zreal and if it is zero,
        // we assume that the Data is not ready. So we wait for "pollEis"-ms and read again, max. 10 tries.
        zReal = lpReadZ->Zreal();
        if (zReal == 0)
        {
            tries = 0;
            while (tries < 10)
            {
                Sleep(pollEis);
                zReal = lpReadZ->Zreal();
                //std::cout << "zReal: " << zReal << "at try: " << tries <<std::endl;
                if (zReal != 0)
                    break;
                tries += 1;
            }
        }
        //std::cout << "after delay: " << _bstr_t(lpReadZ->StatusMessage()) <<std::endl;

        setDoubleParam(EISZFreq_,       lpReadZ->Zfreq());
        setDoubleParam(EISZReal_,       lpReadZ->Zreal());
        setDoubleParam(EISZImag_,       lpReadZ->Zimag());
        setDoubleParam(EISZSig_,        lpReadZ->Zsig());
        setDoubleParam(EISZMod_,        lpReadZ->Zmod());
        setDoubleParam(EISZPhz_,        lpReadZ->Zphz());
        setDoubleParam(EISIdc_,         lpReadZ->Idc());
        setDoubleParam(EISVdc_,         lpReadZ->Vdc());
        setIntegerParam(EISIERange_,    lpReadZ->IERange());

        /* Do callbacks so higher layers see any changes */
        callParamCallbacks();
        datafile    << point << '\t'
                    << lpReadZ->Zfreq() << '\t'
                    << lpReadZ->Zreal() << '\t'
                    << lpReadZ->Zimag() << '\t'
                    << lpReadZ->Zsig() << '\t'
                    << lpReadZ->Zmod() << '\t'
                    << lpReadZ->Zphz() << '\t'
                    << lpReadZ->Idc() << '\t'
                    << lpReadZ->Vdc() << '\t'
                    << lpReadZ->IERange() << '\t'
                    << '\n';

        point = point + 1;
    }

    datafile.close();
    lpPstat->SetCell(CellOff);
    std::cout << "EIS experiment done!" << std::endl;
    setIntegerParam(GMState_, 0);
    //cleanupAndExit();

    return 0;
}

//
// DC Experiment Setup (almost copy-paste from Gamrys example)
//
int Interface1010e::DCMeasurement()
{
    double voltage, totalTime, sampleRate;
    static const char *functionName = "DCMeasurement";

    // Signal Parameters from PV (user input)
    getDoubleParam (GMVoltage_,     &voltage);
    getDoubleParam (GMTotalTime_,   &totalTime);
    getDoubleParam (GMSampleRate_,  &sampleRate);

    // Pstat Settings
    long IERange = 9L;
    float IchRange = 3.0F;
    float VchRange = 3.0F;

    float IchFilter = 5.0F;
    float VchFilter = 5.0F;

    // Output Data
    VARIANT Voltage_data;
    VARIANT Current_data;

    long index_v[2];
    long index_i[2];
    long PointCount = 0;
    long CurrentPoint = 0;
    const long PointsToCook = 256;

    const int delay_interval = 100;
    SAFEARRAY* psa;

    try
    {
        CoCreateInstance(__uuidof(GamryDtaqIvt), NULL, CLSCTX_ALL, __uuidof(IGamryDtaqIvt), reinterpret_cast<void**>(&lpDtaqIvt));
        CoCreateInstance(__uuidof(GamrySignalConst), NULL, CLSCTX_ALL, __uuidof(IGamrySignalConst), reinterpret_cast<void**>(&lpSignal));

        lpDtaqIvt->Init(lpPstat);
        // static cast<float> avoids c4244 warnings: conversion from 'double' to 'float', possible loss of data
        // maybe there is a better way to this ?
        lpSignal->Init(lpPstat, static_cast<float>(voltage), static_cast<float>(totalTime), static_cast<float>(sampleRate), PstatMode);

        // Potentiostat settings
        lpPstat->SetCell (CellOff);
        lpPstat->SetCtrlMode (PstatMode);
        lpPstat->SetIEStability (StabilityNorm);
        lpPstat->SetCASpeed(100.0);

        lpPstat->SetIConvention (Cathodic);

        lpPstat->SetIchRange(IchRange);
        lpPstat->SetIchRangeMode (FALSE);
        lpPstat->SetIchFilter (IchFilter);

        lpPstat->SetVchRange(VchRange);
        lpPstat->SetVchRangeMode (FALSE);
        lpPstat->SetVchFilter (VchFilter);

        lpPstat->SetIchOffsetEnable (FALSE);
        lpPstat->SetVchOffsetEnable (FALSE);

        lpPstat->SetIERange (IERange);
        lpPstat->SetIERangeMode (FALSE);
        lpPstat->SetIERangeLowerLimit(1);

        lpPstat->SetAnalogOut (0.0);
        lpPstat->SetVoltage (0.0);

        // Need to query the interface and put it into a base signal (Do not cast)
        // Then set and initialize the signal via the Pstat.
        lpSignal->QueryInterface(&lpSignalBase);
        lpPstat->SetSignal(lpSignalBase);
        lpPstat->InitSignal();

        // Turn the cell on before acquiring data
        std::cout << "Turning cell on..." << std::endl;
        lpPstat->SetCell(CellOn);

        // Run the Experiment
        std::cout << "Running..." << std::endl;
        lpDtaqIvt->Run(VARIANT_TRUE);

        // This simple example uses polling, so delay a few seconds, giving the experiment time to take measurement
        // In a more robust application, use DataAvailable events.
        Delay("Measuring", (long) (totalTime * 1000), delay_interval);

        // Display the raw data on the screen.
        CurrentPoint = 0;
        PointCount = PointsToCook;
        lpDtaqIvt->Cook(&PointCount, &psa);

        // Always "Cook" until routine returns no data.
        while (PointCount > 0)
        {
            // Loop through and display each point
            for (int i = 0; i < PointCount; i++)
            {
                CurrentPoint++;

                // Indexes are specific locations based on Dtaq.  See documentation for details.
                // SafeArray indexing is reversed from normal conventions.

                index_v[0] = 1;
                index_v[1] = i;

                index_i[0] = 3;
                index_i[1] = i;

                SafeArrayGetElement(psa, index_v, &Voltage_data);
                SafeArrayGetElement(psa, index_i, &Current_data);

                std::cout << '[' << std::setw(4) << CurrentPoint << "]: ";

                std::cout << std::scientific
                          << std::setprecision(8)
                          << std::right
                          << std::setw(20) << Current_data.fltVal << " A "
                          << std::setw(20) << Voltage_data.fltVal << " V "
                          << std::endl;

                std::cout << std::resetiosflags(std::ios::right);
            }

            SafeArrayDestroy(psa);

            // Cook and loop
            PointCount = PointsToCook;
            lpDtaqIvt->Cook(&PointCount, &psa);
        }
        SafeArrayDestroy(psa);

        std::cout << std::endl;
        std::cout << "Experiment complete." << std::endl;
    }
    catch(_com_error &e)
    {

        std::cout << std::showbase
                  << std::internal
                  << std::setfill('0')
                  << std::hex
                  << std::setw(8);

        if (e.Error() == 0xE000002E)
        {
            std::cout << "Selected device is not authorized for DC (" << e.Error() << ")" << std::endl;
        }
        else
        {
            std::cout << "There was an error (" << e.Error() << "): " << e.ErrorMessage() << std::endl;
        }
    }

    setIntegerParam(GMState_, 0);
    //cleanupAndExit();

    return 0;
}

// Displays a nice running counter for delaying time
void Delay(std::string message, long delay, long interval)
{
    for (int t = 0; t <= delay; t += interval)
    {
        std::cout << '\r' << message << "... " << std::fixed << std::setprecision(2) << float(delay - t) / 1000.0F << " seconds remaining" << std::string(5, ' ') << std::flush;
        Sleep(interval);
    }
    std::cout << '\r' << message << "...DONE!" << std::string(20, ' ') << std::endl << std::flush;

    return;
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

// Turn our enums into human-readable strings
std::string ReadzStatusToString(long status)
{
    switch (status)
    {
    case ReadZStatusOk:
        return "ReadZStatusOk";
    case ReadZStatusRetry:
        return "ReadZStatusRetry";
    case ReadZStatusError:
        return "ReadZStatusError";
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
            status = cleanupAndExit();
        }
    }
    if (function == GMCell_)
    {
        if (value)
        {
            lpPstat->SetCell(CellOn);
        }
        else
        {
            lpPstat->SetCell(CellOff);
        }
    }
    if (function == GMStartExp_)
    {
        status = startExperiment();
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

    // Other functions we call the base class method
    else
    {
        status = asynPortDriver::readInt32(pasynUser, value);
    }

    callParamCallbacks(addr);
    return (status==0) ? asynSuccess : asynError;
}

/*asynStatus Interface1010e::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
    int addr;
    int function = pasynUser->reason;
    int status = 0;
    double doubleVal = 0;
    //static const char *functionName = "readFloat64";

    this->getAddress(pasynUser, &addr);

    // Analog input function
    if (function == EISZFreq_)
    {
        //setDoubleParam(addr, EISZFreq_, *value);
    }

    // Other functions we call the base class method
    else
    {
        status = asynPortDriver::readFloat64(pasynUser, value);
    }

    callParamCallbacks(addr);
    return (status==0) ? asynSuccess : asynError;
}*/

asynStatus Interface1010e::writeOctet(asynUser *pasynUser, const char *value,
                                      size_t nChars, size_t *nActual)
{
    int addr;
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    //char labelString[MAX_STRING_LEN];
    const char *functionName = "writeOctet";

    this->getAddress(pasynUser, &addr);
    /* Set the parameter in the parameter library. */
    setStringParam(addr, function, (char *)value);

    //setStringParam(GMLabelString_, labelString);


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
