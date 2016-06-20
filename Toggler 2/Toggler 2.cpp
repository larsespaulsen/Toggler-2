// Toggler 2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Definitions.h"
#include <iostream>       // std::cout
#include <thread>   
#include <chrono>
#include <string>
#include <atomic>
#include <mutex>
using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

mutex CONTROL;
__int8 Mode;
bool CANCEL;
DWORD ErrorCode;
DWORD ProfileAcceleration;
DWORD ProfileDeceleration;
DWORD ProfileVelocity;
HANDLE KeyHandle;
WORD NodeId;
long StartPosition;
int SampleTime_s;
int TogglePeriod_ms;
long Acceleration;
long Velocity;

void Disconnect()
{
	VCS_CloseAllDevices(&ErrorCode);
	cout << "Devices closed.\n";
}

bool Connect()
{
	HANDLE hNewKeyHandle = VCS_OpenDeviceDlg(&ErrorCode);

	HANDLE hNewKeyHandletwo = VCS_OpenDeviceDlg(&ErrorCode);

	//Close Previous Device
	if (KeyHandle)
	{
		if (KeyHandle) VCS_CloseDevice(KeyHandle, &ErrorCode);
		KeyHandle = 0;
		cout << "Previous device closed.\n";
	}

	if (hNewKeyHandle)
	{
		KeyHandle = hNewKeyHandle;

		//Clear Error History
		if (VCS_ClearFault(KeyHandle, NodeId, &ErrorCode))
		{
			//Read Operation Mode
			if (VCS_GetOperationMode(KeyHandle, NodeId, &Mode, &ErrorCode))
			{
				//Read Position Profile Objects
				if (VCS_GetPositionProfile(KeyHandle, NodeId, &ProfileVelocity, &ProfileAcceleration, &ProfileDeceleration, &ErrorCode))
				{
					//Write Profile Position Mode
					if (VCS_SetOperationMode(KeyHandle, NodeId, OMD_PROFILE_POSITION_MODE, &ErrorCode))
					{
						//Write Profile Position Objects
						if (VCS_SetPositionProfile(KeyHandle, NodeId, 100, 1000, 1000, &ErrorCode))
						{
							//Read Actual Position
							if (VCS_GetPositionIs(KeyHandle, NodeId, &StartPosition, &ErrorCode))
							{
								return TRUE;
							}
						}
					}
				}
			}
		}

	}
	else
	{
		cout << "Can't open device!\n" << ErrorCode;
	}

	return FALSE;
}

int SetPositionMode()
{
	BOOL oFault = FALSE;

	if (!VCS_GetFaultState(KeyHandle, NodeId, &oFault, &ErrorCode))
	{
		cout << "Failed to enable device!\n" << ErrorCode;
		return 0;
	}

	if (!VCS_ClearFault(KeyHandle, NodeId, &ErrorCode))
	{
		cout << "Failed to enable device!\n" << ErrorCode;
		return 0;
	}

	if (!VCS_SetEnableState(KeyHandle, NodeId, &ErrorCode))
	{
		cout << "Failed to enable device!\n" << ErrorCode;
		return 0;
	}

	if (VCS_GetOperationMode(KeyHandle, NodeId, &Mode, &ErrorCode))
	{
		//Usefull print if SetPositionMode fails.
		//cout << "KeyHandle: " << m_KeyHandle << "\nNodeId: " << m_usNodeId << "\nMode: " << m_bMode << "\nErrorCode: " << m_ulErrorCode << "\n";
	}
	else
	{
		cout << "Failed to get operation mode!\n" << ErrorCode;
		return 0;
	}
	return 1;
}

void SetVelocityAndAceleration()
{
	DWORD Velocity, Acceleration;
	bool complete = false;
	while (!complete)
	{
		//clear console
		cout << "\n"; cin.clear(); fflush(stdin);

		cout << "Old Values:\n" << "Velocity:\t" << ProfileVelocity << "\nAcceleration:\t" << ProfileAcceleration << "\n\n";

		cout << "New Values:\n" << "Velocity:\t"; cin >> Velocity;

		cout << "Acceleration\t"; cin >> Acceleration;

		cout << "\n";

		if (!VCS_SetPositionProfile(KeyHandle, NodeId, Velocity, Acceleration, Acceleration/*Deacceleration*/, &ErrorCode))
		{
			cout << "Failed to update speed, error: " << ErrorCode << "\n";
		}
		else{
			complete = true;
		}
	}
}

void Canceller()
{
	int blocker;
	//Cancel on input
	cout << "Press any key + enter to cancel:\n"; 
	cin.clear(); fflush(stdin); cin >> blocker;
	CONTROL.lock(); CANCEL = 1; CONTROL.unlock();
}

void Toggle(long from, long to)
{

	if (VCS_GetPositionIs(KeyHandle, NodeId, &StartPosition, &ErrorCode))
	{
		
		system_clock::time_point StopTime = system_clock::now() + seconds(SampleTime_s);
		system_clock::time_point starttime = system_clock::now();
		bool sender = 1;
		//Take Control.
		CONTROL.lock();
		while ((system_clock::now() <= StopTime) && !CANCEL)
		{
			//Give main control to enable cancellation.
			CONTROL.unlock();
			if (system_clock::now() < starttime + milliseconds(TogglePeriod_ms / 2))
			{
				if (sender == 1)
				{
					if (!VCS_MoveToPosition(KeyHandle, NodeId, to, TRUE, TRUE, &ErrorCode))
					{
						cout << "error: " << ErrorCode;
					}
					else
					{
						//cout << "Go to " << to << "!\n";
					}
					sender = 0;
				}
			}
			else if (system_clock::now() >= starttime + milliseconds(TogglePeriod_ms / 2) && system_clock::now() < starttime + milliseconds(TogglePeriod_ms))
			{
				if (sender == 0)
				{
					if (!VCS_MoveToPosition(KeyHandle, NodeId, from, TRUE, TRUE, &ErrorCode))
					{
						cout << "error: " << ErrorCode;
					}
					else
					{
						//cout << "Go to " << from << "!\n";
					}
					sender = 1;
				}
			}
			else
			{
				starttime = system_clock::now();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

			CONTROL.lock();
		}

		////If canceled away from Startpos stop here.
		//if (VCS_GetPositionIs(KeyHandle, NodeId, &StartPosition, &ErrorCode))
		//{
		//	if (!VCS_MoveToPosition(KeyHandle, NodeId, StartPosition, TRUE, TRUE, &ErrorCode))
		//	{
		//		cout << "Fatal error when trying to stop " << ErrorCode;
		//	}
		//}

		CONTROL.unlock();
	}
	else
	{
		cout << "Toggling failed to begin " << ErrorCode << "\n";
	}

	//cout << "\n\n\nToggling finished. Press any key + enter to continue.";
}

bool InterfaceYesOrNo(string prompt)
{
	string YesOrNo;

	while (true)
	{
		cout << prompt << "\ty/n\t";
		cin.clear(); fflush(stdin); cin >> YesOrNo;
		if (YesOrNo[0] == 'y')
		{
			return true;
		}
		if (YesOrNo[0] == 'n')
		{
			return false;
		}
	}
}



struct Pattern
{
	long from;
	long to;
};


int	main()
{
	//Startup sequence.

	SampleTime_s = 8;
	Mode = 0;
	KeyHandle = 0;
	StartPosition = 0;
	SampleTime_s = 8;
	TogglePeriod_ms = 2000;
	NodeId = 1;
	Velocity = 0;
	Acceleration = 0;

	long to, from;
	int NumberOfPatterns = 1;
	int Runs = 0;

	if (Connect())
	{
		cout << "Device Connected.\n";
	}
	if (SetPositionMode())
	{
		cout << "Device in Position mode.\n";
	}


	if (ErrorCode == 0)
	{
		cout << "Device Opened with " << ErrorCode << " errors.\n\n";
	}
	else
	{
		cout << "Device error: " << ErrorCode << "\n\n";
		return 0;
	}

	cout << "Profile velocity is:\t\t" << ProfileVelocity << "\n";
	cout << "Profile Acceleration is:\t" << ProfileAcceleration << "\n";
	cout << "Position is :\t\t\t" << StartPosition << "\n\n";


	
	
	//Change Velocity and Acceleration prompt.
	if (InterfaceYesOrNo("Change Velocity and Acceleration?")) 
	{ 
		SetVelocityAndAceleration(); 
	}
	else
	{
		cout << "\n";
	}


	cout		<< "Pattern sample time in seconds:\t\t";
	cin.clear(); fflush(stdin); cin >> SampleTime_s;

	cout << "Pattern toggle period in milliseconds:\t";
	cin.clear(); fflush(stdin); cin >> TogglePeriod_ms;

	cout << "Number of patterns to run?\t\t";
	cin.clear(); fflush(stdin); cin >> NumberOfPatterns;

	//delete[] pattern;
	Pattern* pattern = new Pattern[NumberOfPatterns];
	for (int i = 0; i < NumberOfPatterns; i++)
	{
		if (i == 0)
		{
			cout << "\nPattern\t" << i + 1 << "\t from:\t0\n";
			pattern[i].from = 0;
		}
		else
		{
			cout << "\nPattern\t" << i + 1 << "\t from:\t";
			cin.clear(); fflush(stdin); cin >> from;
			pattern[i].from = from;
		}
		cout << "Pattern\t" << i + 1 << "\t to:\t";
		cin.clear(); fflush(stdin); cin >> to;
		pattern[i].to = to;
	}

	//End of startup sequence.
	while (true)
	{
		if (VCS_GetPositionIs(KeyHandle, NodeId, &StartPosition, &ErrorCode))
		{
			cout << "\nPosition is :\t\t\t" << StartPosition << "\n\n";
		}
		else
		{
			cout << "Failed to update position";
		}
		
		if(StartPosition != 0)
		{
			if (InterfaceYesOrNo("Go to position 0?"))
			{
				if (!VCS_MoveToPosition(KeyHandle, NodeId, 0, TRUE, TRUE, &ErrorCode))
				{
					cout << "Failed to go to position 0. " << ErrorCode;
				}
			}
		}
			//Start prompt.
			if (InterfaceYesOrNo("\n\nStart toggling?"))
			{
				CONTROL.lock(); CANCEL = 0; CONTROL.unlock();
				thread cancel_thread(Canceller);
				Runs = 0;

				while (Runs < NumberOfPatterns)
				{
					thread toggle_thread(Toggle, pattern[Runs].from, pattern[Runs].to);
					toggle_thread.join();
					Runs++;
				}
				if (CANCEL)
				{
					cout << "\nPattern canceled!\n";
				}
				else
				{
					cout << "\n\n\nPattern finished. Press any key + enter to continue:\n";
				}		
				cancel_thread.join();
			}else
			{
				if(InterfaceYesOrNo("Exit and close device?"))
				{ 
					delete[] pattern;
					Disconnect(); 
					return 0; 
				}
			}

			cin.clear(); fflush(stdin);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			if (InterfaceYesOrNo("\nReconfigure run?"))
			{
				//Change Velocity and Acceleration prompt.
				if (InterfaceYesOrNo("Change Velocity and Acceleration?"))
				{
					SetVelocityAndAceleration();
				}
				else
				{
					cout << "\n";
				}


				cout << "\nPattern sample time in seconds:\t\t";
				cin.clear(); fflush(stdin); cin >> SampleTime_s;

				cout << "\nPattern toggle period in milliseconds:\t";
				cin.clear(); fflush(stdin); cin >> TogglePeriod_ms;

				cout << "\nNumber of patterns to run?\t\t";
				cin.clear(); fflush(stdin); cin >> NumberOfPatterns;

				//delete[] pattern;
				Pattern* pattern = new Pattern[NumberOfPatterns];
				for (int i = 0; i < NumberOfPatterns; i++)
				{
					cout << "\nPattern\t" << i + 1 << "\t from:\t";
					cin.clear(); fflush(stdin); cin >> from;
					cout << "Pattern\t" << i + 1 << "\t to:\t";
					cin.clear(); fflush(stdin); cin >> to;
					pattern[i].from = from;
					pattern[i].to = to;
				}

			}

	}

	std::cout << "Main sleeping 10 seconds before ending.\n";
	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	return 0;
}
