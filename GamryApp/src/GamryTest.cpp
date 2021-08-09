// Copyright(c) Gamry Instruments, Inc.
// Gamry Instruments, Inc.
// All rights reserved
// 
// Gamry Instruments, Inc.
// 734 Louis Drive
// Warminster, PA 18974

// This sample application makes use of the following GamryCOM objects
//    o GamryDeviceList
//    o GamryPstat
//    o GamryDtaqEis

#include <windows.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <conio.h>
#include <time.h>

//
// Import the Gamry Electrochemistry Toolkit
// 
// Note the Intellisense workaround.  Compile once to get Intellisense support.
// https://connect.microsoft.com/VisualStudio/feedback/details/704885/intellisense-does-not-correctly-handle-import-progid-in-c
//
#ifdef __INTELLISENSE__
#ifdef _DEBUG
#include "Debug\GamryCOM.tlh"
#else
#include "Release\GamryCOM.tlh"
#endif
#else
// Import the Gamry Electrochemistry Toolkit: GamryCOM using our TypeLib GUID
// This is created when GamryCOM is registered during installation of the Gamry Software Suite.
#import "libid:BD962F0D-A990-4823-9CF5-284D1CDD9C6D" no_namespace
#endif

//
// Helper Functions
// 
void Pause();
bool PauseExit();
long NumberPrompt();
void Delay(std::string message, long delay, long interval);
long SelectDevice(IGamryDeviceListPtr lpDeviceList, BSTR* Sections);
void CleanupAndExit(int status, IGamryDeviceListPtr lpDeviceList, IGamryPstatPtr lpPstat, IGamryDtaqPtr lpDtaq);
std::string EisStatusToString(long status);

//
// Main
//
int main(void)
{
	std::cout << "Hello World!\n";

	// Instrument Interactions
	IGamryDeviceList* lpDeviceList = NULL;
	IGamryPstat* lpPstat = NULL;

	BSTR Sections[16];
	long instrument_index;

	// Data Acquisition
	IGamryDtaqEis* lpDtaq = NULL;

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
			CleanupAndExit(EXIT_FAILURE, lpDeviceList, lpPstat, lpDtaq);
		}

		// Ask the Device List how many devices are available.  If there are none, exit.
		std::cout << "There are " << lpDeviceList->Count() << " Gamry device(s) connected to your system:" << std::endl;
		if (lpDeviceList->Count() < 1)
		{
			CleanupAndExit(EXIT_SUCCESS, lpDeviceList, lpPstat, lpDtaq);
		}

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

		lpPstat->Open();

		lpPstat->SetCell(CellOn);

		std::cout << "The cell was turned on. Press any key to turn the cell off and exit the programm." << std::endl;
		Pause();

		lpPstat->SetCell(CellOff);

		CleanupAndExit(EXIT_SUCCESS, lpDeviceList, lpPstat, lpDtaq);

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
		Pause();
	}

	CleanupAndExit(EXIT_SUCCESS, lpDeviceList, lpPstat, lpDtaq);

	return 0;
}

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

	exit(Status);
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
		std::cout << "0 automatically selected." << std::endl;
		return 0;
	}
}


// Turn our enums into human-readable strings
std::string EisStatusToString(long status)
{
	switch (status)
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

//
// Input Helper Functions
// 

// Classic press any key helper
void Pause()
{
	std::cout << "Press any key to continue..." << std::endl;
	_getch();
}

// Pause or Exit via ESC key
bool PauseExit()
{
	char ch;
	std::cout << "Press any key to continue, or <ESC> to exit." << std::endl;
	ch = _getch();
	if (ch == 27)
		return true;
	return false;
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


// Displays a nice running counter for delaying time
void Delay(std::string message, long delay, long interval)
{
	for (int t = 0; t <= delay; t += interval)
	{
		std::cout << '\r' << message << "... " << std::fixed << std::setprecision(2) << float(delay - t) / 1000.0F << " seconds remaining" << std::flush;
		Sleep(interval);
	}
	std::cout << '\r' << message << "...DONE!" << std::string(20, ' ') << std::endl << std::flush;

	return;
}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
