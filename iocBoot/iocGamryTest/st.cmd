#!../../bin/windows-x64-static/GamryTest

#- You may have to change GamryTest to something else
#- everywhere it appears in this file

< envPaths

## Register all support components
dbLoadDatabase "../../dbd/GamryTest.dbd"
GamryTest_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadTemplate("Gamry.substitutions")

## Configure port driver
# Interface1010eConfig(portName,        # The name to give to this asyn port driver
#                      pstatNum)        # The number of this potentiostat
Interface1010eConfig("Gamry_1", 1)

#asynSetTraceMask Gamry_1 -1 255
asynSetTraceInfoMask((Gamry_1, 0, TIME|THREAD)

iocInit

## Start any sequence programs
#seq sncxxx,"user=Administrator"
