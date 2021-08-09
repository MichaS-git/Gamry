TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += Gamry

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install Gamry.dbd into <top>/dbd
DBD += Gamry.dbd

# specify all source files to be compiled and added to the library
#Gamry_SRCS += GamryTest.cpp
Gamry_SRCS += Interface1010eDriver.cpp

Gamry_LIBS += asyn

Gamry_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

