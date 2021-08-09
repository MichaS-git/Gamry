#include <windows.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <conio.h>
#include <time.h>

#include <iocsh.h>
#include <asynPortDriver.h>

#include <epicsExport.h>

//#include <GamryCom.tlh>

#import "libid:BD962F0D-A990-4823-9CF5-284D1CDD9C6D" no_namespace

static const char *driverName = "Interface1010e";

// output parameters
#define analogOutValueString      "ANALOG_OUT_VALUE"

// Instrument Interactions
IGamryDeviceList* lpDeviceList = NULL;

// Create instances of our interfaces
CoCreateInstance(__uuidof(GamryDeviceList), NULL, CLSCTX_ALL, __uuidof(IGamryDeviceList), reinterpret_cast<void**>(&lpDeviceList));

/* Class definition for the Gamry Interface 1010e */
class Interface1010e : public asynPortDriver {
public:
  Interface1010e(const char *portName, int pstatNum);

  /* These are the methods that we override from asynPortDriver */
  virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
  virtual asynStatus getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high);

protected:
  // Analog output parameters
  int analogOutValue_;

private:
  int pstatNum_;
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
  // Analog output parameters
  createParam(analogOutValueString,            asynParamInt32, &analogOutValue_);
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
    status = lpDeviceList->Count();
    //status = cbAOut(pstatNum_, addr, BIP10VOLTS, value);
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

/** Configuration command, called directly or from iocsh */
extern "C" int Iterface1010eConfig(const char *portName, int pstatNum)
{
  Iterface1010e *Ipterface1010e = new Iterface1010e(portName, pstatNum);
  pIterface1010e = NULL;  /* This is just to avoid compiler warnings */
  return(asynSuccess);
}


static const iocshArg configArg0 = { "Port name",      iocshArgString};
static const iocshArg configArg1 = { "potentiostat number",      iocshArgInt};
static const iocshArg * const configArgs[] = {&configArg0,
                                              &configArg1};
static const iocshFuncDef configFuncDef = {"Iterface1010eConfig", 2, configArgs};
static void configCallFunc(const iocshArgBuf *args)
{
  Iterface1010eConfig(args[0].sval, args[1].ival);
}

void drvIterface1010eRegister(void)
{
  iocshRegister(&configFuncDef,configCallFunc);
}

extern "C" {
epicsExportRegistrar(drvIterface1010eRegister);
}
