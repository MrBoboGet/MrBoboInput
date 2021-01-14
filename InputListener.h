#pragma once
#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <time.h>
#include <thread>
#include <atomic>
#include <fstream>
#include <conio.h>
#include <unordered_map>
//LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
KBDLLHOOKSTRUCT KBDStruct;
HHOOK HookHandle;
clock_t DeltaTime;
struct StoredMessage
{
	KBDLLHOOKSTRUCT MessageData;
	WPARAM MessageType;
	LPARAM lParam;
	clock_t DeltaTime;
};
std::vector<StoredMessage> SavedMessages = {};
std::vector<StoredMessage> MessagesToSend = {};
bool RecordingInput = false;
std::atomic<bool> ShouldSerializeData{ false };
std::atomic<bool> ShouldSendData{ false };
std::atomic<bool> SendDataAsInput{ false };
std::unordered_map<DWORD, int> PressedBeforeRelease;
void SerializeData()
{
	MessagesToSend = SavedMessages;
	std::ofstream SavedData("SavedRecording",std::ios::binary|std::ios::out);
	if (!SavedData)
	{
		std::cout << "Error creating file" << std::endl;
	}
	for (size_t i = 0; i < SavedMessages.size(); i++)
	{
		std::string MessageData((char*)&SavedMessages[i], sizeof(StoredMessage));
		SavedData << MessageData;
	}
	SavedData.close();
	SavedMessages.clear();
	ShouldSerializeData = false;
	std::cout << "Saved Data" << std::endl;
}
void DeSerializeData()
{
	if (MessagesToSend.size() == 0)
	{
		std::fstream SaveFile("SavedRecording",std::ios::in|std::ios::binary|std::ios::out);
		StoredMessage NyMessage;
		SaveFile.read((char*)&NyMessage, sizeof(StoredMessage));
		if (!SaveFile)
		{
			std::cout << GetLastError() << std::endl;
		}
		while (SaveFile)
		{
			MessagesToSend.push_back(NyMessage);
			SaveFile.read((char*)&NyMessage, sizeof(StoredMessage));
		}
		SaveFile.close();
	}
}
LRESULT HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
	//std::cout << "Hej" << std::endl;
	if (nCode >= 0)
	{
		// the action is valid: HC_ACTION.
		if (wParam == WM_KEYDOWN)
		{
			// lParam is the pointer to the struct containing the data needed, so cast and assign it to kdbStruct.
			KBDStruct = *((KBDLLHOOKSTRUCT*)lParam);
			if (RecordingInput && !ShouldSerializeData && (KBDStruct.vkCode != VK_F1 && KBDStruct.vkCode != VK_F2&&KBDStruct.vkCode != VK_F3))
			{
				PressedBeforeRelease[KBDStruct.vkCode] = 1;
				StoredMessage NewMessage;
				NewMessage.MessageData = KBDStruct;
				NewMessage.DeltaTime = clock()-DeltaTime;
				NewMessage.lParam = lParam;
				NewMessage.MessageType = wParam;
				SavedMessages.push_back(NewMessage);
				DeltaTime = clock();
			}
			if (KBDStruct.vkCode == VK_F1 && !ShouldSerializeData)
			{
				if (!RecordingInput)
				{
					std::cout << "Now recording input" << std::endl;
				}
				RecordingInput = true;
				DeltaTime = clock();
			}
			if (KBDStruct.vkCode == VK_F2)
			{
				if (RecordingInput)
				{
					ShouldSerializeData = true;
					std::cout << "Stopped recording" << std::endl;
				}
				RecordingInput = false;
			}
			if(KBDStruct.vkCode == VK_F3)
			{
				//checkar huruvida vi ska skicka data eller inte
				if (!ShouldSendData)
				{
					std::cout << "started sending data as keypresses" << std::endl;
				}
				else
				{
					std::cout << "stopped sending data" << std::endl;
				}
				SendDataAsInput = false;
				ShouldSendData = !ShouldSendData;
			}
			if (KBDStruct.vkCode == VK_F4)
			{
				//checkar huruvida vi ska skicka data eller inte
				SendDataAsInput = true;
				if (!ShouldSendData)
				{
					std::cout << "started sending data as input" << std::endl;
				}
				else
				{
					std::cout << "stopped sending data" << std::endl;
				}
				ShouldSendData = !ShouldSendData;
			}
			// a key (non-system) is pressed.
		}
		else if (wParam == WM_KEYUP)
		{
			KBDStruct = *((KBDLLHOOKSTRUCT*)lParam);
			if (RecordingInput && !ShouldSerializeData && (KBDStruct.vkCode != VK_F1 && KBDStruct.vkCode != VK_F2 && KBDStruct.vkCode != VK_F3))
			{
				StoredMessage NewMessage;
				PressedBeforeRelease[KBDStruct.vkCode] = 0;
				NewMessage.MessageData = KBDStruct;
				NewMessage.DeltaTime = clock()-DeltaTime;
				NewMessage.lParam = lParam;
				NewMessage.MessageType = wParam;
				SavedMessages.push_back(NewMessage);
				DeltaTime = clock();
			}
		}
	}
	// call the next hook in the hook chain. This is nessecary or your hook chain will break and the hook stops
	return CallNextHookEx(HookHandle, nCode, wParam, lParam);
}
std::atomic<bool> ShouldStop{ false };
//std::string WindowToRecieveInput;
void CreateHook()
{
	HookHandle = SetWindowsHookExA(WH_KEYBOARD_LL, HookCallback, NULL, 0);
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{

	}
}
std::string WindowToRecieveInput = "";
HWND WindowHandle;
std::atomic<bool> FoundWindow{ false };
BOOL RecieveWindowCallback(HWND NewWindowHandle,LPARAM lParam)
{
	char WindowTitleBuffer[500];
	GetWindowTextA(NewWindowHandle, (LPSTR)WindowTitleBuffer, 500);
	//std::cout << std::string(WindowTitleBuffer) << std::endl;
	//std::cout << "Window we want to recieve " << WindowToRecieveInput << std::endl;
	if (std::string(WindowTitleBuffer) == WindowToRecieveInput)
	{
		FoundWindow = true;
		WindowHandle = NewWindowHandle;
		return(FALSE);
	}
	else
	{
		FoundWindow = false;
		return(TRUE);
	}
}
class InputListener
{
private:
	std::thread HookThread;
	std::string ChangeWindowCommand = "window=";
	int MessagesToSendIndex = 0;
	bool WasSendingData = false;
	bool PreviousShouldSendData = false;
	clock_t Timer;
public:
	InputListener()
	{
		std::cout << "Type window to recieve input" << std::endl;
		std::getline(std::cin, WindowToRecieveInput);
		DeltaTime = clock();
		HookThread = std::thread(CreateHook);
	}
	LPARAM GetLParam(KBDLLHOOKSTRUCT StructToConvert,WPARAM MessageType)
	{
		unsigned int IntToReturn = 0;
		IntToReturn |= 1;
		IntToReturn += (StructToConvert.scanCode<<15);
		IntToReturn |= ((StructToConvert.flags & 1) << 29);
		if (MessageType == WM_KEYDOWN)
		{
			IntToReturn |= (PressedBeforeRelease[StructToConvert.vkCode] << 30);
		}
		else if (MessageType == WM_KEYUP)
		{
			IntToReturn |= (3 << 30);
		}
		return((LPARAM)IntToReturn);
	}
	bool GetRecievingWindow()
	{
		EnumWindows(RecieveWindowCallback, NULL);
		if (FoundWindow)
		{
			FoundWindow = false;
			return(true);
		}
		else
		{
			return(false);
		}
	}
	void Listen()
	{
		//settar en hook, printar datan den får
		while (true)
		{
			if(ShouldSendData)
			{
				if (PreviousShouldSendData != ShouldSendData)
				{
					std::cout << "Searchinf for window" << std::endl;
					bool WindowOk = GetRecievingWindow();
					if (!WindowOk)
					{
						std::cout << "Failed to find window" << std::endl;
						ShouldSendData = false;
						//continue;
					}
					else
					{
						std::cout << "Found Window" << std::endl;
					}
					Timer = clock();
				}
				if (ShouldSendData)
				{
					if (MessagesToSend.size() == 0)
					{
						DeSerializeData();
						Timer = clock();
					}
					if (MessagesToSendIndex >= MessagesToSend.size())
					{
						MessagesToSendIndex = 0;
						std::cout << "\x07" << std::endl;
					}
					if (MessagesToSend[MessagesToSendIndex].DeltaTime <= clock()-Timer)
					{
						//vi skickar meddelandet hur man nu gör det
						//TODO checka om detta faktiskt fungerar
						//std::cout << "Skickar Data" << std::endl;
						if (!SendDataAsInput)
						{
							StoredMessage NewMessage = MessagesToSend[MessagesToSendIndex];
							LPARAM NewLParam = GetLParam(NewMessage.MessageData, NewMessage.MessageType);
							if (!PostMessage(WindowHandle, NewMessage.MessageType, NewMessage.MessageData.vkCode, NewLParam))//(LPARAM)&MessagesToSend[MessagesToSendIndex].MessageData))
							{
								std::cout << "Error sending message " << GetLastError() << std::endl;
							}
							std::cout << char(MessagesToSend[MessagesToSendIndex].MessageData.vkCode) << std::endl;
						}
						else
						{
							StoredMessage NewMessage = MessagesToSend[MessagesToSendIndex];
							INPUT InputArray[1];
							ZeroMemory(InputArray, sizeof(InputArray));
							InputArray[0].type = INPUT_KEYBOARD;
							InputArray[0].ki.wVk = NewMessage.MessageData.vkCode;
							std::cout << char(NewMessage.MessageData.vkCode) << std::endl;
							if (NewMessage.MessageType == WM_KEYUP)
							{
								InputArray[0].ki.dwFlags = KEYEVENTF_KEYUP;
							}
							UINT uSent = SendInput(ARRAYSIZE(InputArray), InputArray, sizeof(INPUT));
							if (uSent != ARRAYSIZE(InputArray))
							{
								std::cout<<"SendInput failed: "<< GetLastError()<<std::endl;
							}
						}
						MessagesToSendIndex += 1;
						Timer = clock();
					}
				}
				PreviousShouldSendData = true;
			}
			else
			{
				//utifall vi vill att koden ska kunna pausas mitt i
				/*
				if (PreviousShouldSendData == true)
				{
					//vi kollar huruvida den sista var av typen, ner, och om det var det, skickar motsvarande av up
					int LastMessageIndex = MessagesToSendIndex - 1;
					if (LastMessageIndex < 0)
					{
						LastMessageIndex = MessagesToSend.size() - 1;
					}
					if (MessagesToSend[LastMessageIndex].MessageType == WM_KEYDOWN)
					{
						//vi måste skicka motsvarande 
					}
				}
				*/
				PreviousShouldSendData = false;
			}
			if (ShouldSerializeData)
			{
				SerializeData();
			}
			//std::string Input;
			//std::getline(std::cin, Input);
			//if (Input.substr(0,ChangeWindowCommand.size()) == ChangeWindowCommand)
			//{
				//WindowToRecieveInput = ChangeWindowCommand.substr(ChangeWindowCommand.size());
				//std::cout << "Successfully changed window to " + WindowToRecieveInput << std::endl;
			//}
		}
	}
	~InputListener()
	{
		ShouldStop = true;
		UnhookWindowsHookEx(HookHandle);
		
	}
};