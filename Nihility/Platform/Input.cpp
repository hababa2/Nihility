#include "Input.hpp"

#include "Platform.hpp"
#include "Core/Time.hpp"
#include "Core/Settings.hpp"

#ifdef NH_PLATFORM_WINDOWS

#include "WindowsInclude.hpp"
#include "Core/Logger.hpp"

#include <Xinput.h>
#include <hidsdi.h>
#include <hidpi.h>
#pragma comment(lib ,"xinput.lib")
#pragma comment(lib ,"hid.lib")

bool Input::useController;
U32 Input::activeController;

Input::InputState Input::osStates;
Input::InputState Input::currentStates;
Input::InputState Input::previousStates;
CircularQueue<ButtonEvent> Input::eventQueue;
Vector<Controller> Input::controllers(4, {});
F64 Input::currentTimestamp;
F64 Input::holdThreshold;
F64 Input::doublePressThreshold;

Hashmap<U64, Vector<ButtonCode>> Input::actionBindings;
Hashmap<U64, Vector<Input::ButtonAxisBinding>> Input::buttonAxisBindings;
Hashmap<U64, Vector<Input::JoystickAxisBinding>> Input::joystickAxisBindings;

F32 Input::mouseSensitivity;
I16 Input::mouseWheelDelta;
I16 Input::mouseHWheelDelta;
F32 Input::mousePosX;
F32 Input::mousePosY;
F32 Input::deltaMousePosX;
F32 Input::deltaMousePosY;
F32 Input::deltaRawMousePosX;
F32 Input::deltaRawMousePosY;

bool Input::inputConsumed;

HWND Input::hwnd;

bool Input::Initialize()
{
	Logger::Trace("Initializing Input System...");

	eventQueue.Create(256);

	POINT p;
	GetCursorPos(&p);

	mousePosX = (F32)(p.x - Settings::WindowPositionX());
	mousePosY = (F32)(p.y - Settings::WindowPositionY());

	I32 sensitivity;
	SystemParametersInfoA(SPI_GETMOUSESPEED, 0, &sensitivity, 0);

	mouseSensitivity = (F32)sensitivity;

	return true;
}

void Input::RegisterDevices()
{
	RAWINPUTDEVICE deviceList[6];
	deviceList[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	deviceList[0].usUsage = HID_USAGE_GENERIC_POINTER;
	deviceList[0].dwFlags = RIDEV_DEVNOTIFY;
	deviceList[0].hwndTarget = nullptr;

	deviceList[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
	deviceList[1].usUsage = HID_USAGE_GENERIC_MOUSE;
	deviceList[1].dwFlags = RIDEV_DEVNOTIFY | RIDEV_INPUTSINK; //RIDEV_NOLEGACY
	deviceList[1].hwndTarget = Platform::hWnd;

	deviceList[2].usUsagePage = HID_USAGE_PAGE_GENERIC;
	deviceList[2].usUsage = HID_USAGE_GENERIC_KEYPAD;
	deviceList[2].dwFlags = RIDEV_DEVNOTIFY;
	deviceList[2].hwndTarget = nullptr;

	deviceList[3].usUsagePage = HID_USAGE_PAGE_GENERIC;
	deviceList[3].usUsage = HID_USAGE_GENERIC_KEYBOARD;
	deviceList[3].dwFlags = RIDEV_DEVNOTIFY | RIDEV_NOLEGACY | RIDEV_APPKEYS;
	deviceList[3].hwndTarget = nullptr;

	deviceList[4].usUsagePage = HID_USAGE_PAGE_GENERIC;
	deviceList[4].usUsage = HID_USAGE_GENERIC_GAMEPAD;
	deviceList[4].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
	deviceList[4].hwndTarget = Platform::hWnd;

	deviceList[5].usUsagePage = HID_USAGE_PAGE_GENERIC;
	deviceList[5].usUsage = HID_USAGE_GENERIC_MULTI_AXIS_CONTROLLER;
	deviceList[5].dwFlags = RIDEV_DEVNOTIFY;
	deviceList[5].hwndTarget = nullptr;

	RegisterRawInputDevices(deviceList, CountOf32(deviceList), sizeof(RAWINPUTDEVICE));
}

void Input::Shutdown()
{
	Logger::Trace("Shutting Down Input System...");

	SetControllerRumbleStrength(0.0f, 0.0f);
	SetControllerLedColor(0.0f, 0.0f, 0.0f);
	SetControllerTriggerEffect(TriggerEffectType::None, Trigger::Both);

	eventQueue.Destroy();

	//TODO: Update controllers to reset effects
}

void Input::Update()
{
	memcpy(&previousStates, &currentStates, sizeof(InputState));
	memcpy(&currentStates, &osStates, sizeof(InputState));

	POINT p;
	GetCursorPos(&p);

	F32 newX = (F32)(p.x - (I32)Settings::WindowPositionX());
	F32 newY = (F32)(p.y - (I32)Settings::WindowPositionY());

	deltaMousePosX = newX - mousePosX;
	deltaMousePosY = newY - mousePosY;

	mousePosX = newX;
	mousePosY = newY;

	mouseWheelDelta = 0;
	mouseHWheelDelta = 0;
	inputConsumed = false;

	UpdateXboxControllers();
}

void Input::UpdateRawInput(I64 lParam)
{
	currentTimestamp = Time::AbsoluteTime();
	U32 size = 0;
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));

	alignas(8) U8 buffer[1024];
	if (size > sizeof(buffer)) { return; }

	RawInput* input = (RawInput*)buffer;

	if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, input, &size, sizeof(RAWINPUTHEADER)) > 0)
	{
		switch (input->header.dwType)
		{
		case RIM_TYPEMOUSE: {
			RawMouse& mouse = input->data.mouse;

			if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
			{
				RECT rect;
				if (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP)
				{
					rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
					rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
					rect.right = GetSystemMetrics(SM_CXVIRTUALSCREEN);
					rect.bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);
				}
				else
				{
					rect.left = 0;
					rect.top = 0;
					rect.right = Platform::screenWidth;
					rect.bottom = Platform::screenHeight;
				}

				I32 absoluteX = MulDiv(mouse.lLastX, rect.right, U16_MAX) + rect.left;
				I32 absoluteY = MulDiv(mouse.lLastY, rect.bottom, U16_MAX) + rect.top;

				//TODO:
			}
			else if (mouse.lLastX != 0 || mouse.lLastY != 0)
			{
				I32 relativeX = mouse.lLastX;
				I32 relativeY = mouse.lLastY;

				deltaRawMousePosX = relativeX * mouseSensitivity;
				deltaRawMousePosY = relativeY * mouseSensitivity;
			}

			POINT p;
			GetCursorPos(&p);

			HWND handle = WindowFromPoint(p);

			if (handle == Platform::hWnd)
			{
				if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) { UpdateButtonState(ButtonCode::LeftMouse, true); }
				if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) { UpdateButtonState(ButtonCode::RightMouse, true); }
				if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) { UpdateButtonState(ButtonCode::MiddleMouse, true); }
				if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) { UpdateButtonState(ButtonCode::XMouseOne, true); }
				if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) { UpdateButtonState(ButtonCode::XMouseTwo, true); }

				if (mouse.usButtonFlags & RI_MOUSE_WHEEL) { mouseWheelDelta = (I16)((F32)(I16)mouse.usButtonData / WHEEL_DELTA); }
				if (mouse.usButtonFlags & RI_MOUSE_HWHEEL) { mouseHWheelDelta = (I16)((F32)(I16)mouse.usButtonData / WHEEL_DELTA); }
			}

			if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) { UpdateButtonState(ButtonCode::LeftMouse, false); }
			if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) { UpdateButtonState(ButtonCode::RightMouse, false); }
			if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) { UpdateButtonState(ButtonCode::MiddleMouse, false); }
			if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP) { UpdateButtonState(ButtonCode::XMouseOne, false); }
			if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP) { UpdateButtonState(ButtonCode::XMouseTwo, false); }
		} break;
		case RIM_TYPEKEYBOARD: {
			RawKeyboard& keyboard = input->data.keyboard;
			UpdateButtonState((ButtonCode)keyboard.VKey, keyboard.Message == WM_KEYDOWN || keyboard.Message == WM_SYSKEYDOWN);
		} break;
		case RIM_TYPEHID: {
			RID_DEVICE_INFO deviceInfo{};
			UINT deviceInfoSize = sizeof(deviceInfo);
			bool gotInfo = GetRawInputDeviceInfo(input->header.hDevice, RIDI_DEVICEINFO, &deviceInfo, &deviceInfoSize) > 0;

			GetRawInputDeviceInfo(input->header.hDevice, RIDI_PREPARSEDDATA, 0, &size);

			alignas(8) U8 preparsedBuffer[1024];
			if (size > sizeof(preparsedBuffer)) { return; }

			PHIDP_PREPARSED_DATA data = (PHIDP_PREPARSED_DATA)preparsedBuffer;

			bool gotPreparsedData = GetRawInputDeviceInfo(input->header.hDevice, RIDI_PREPARSEDDATA, data, &size) > 0;

			if (gotInfo && gotPreparsedData)
			{
				U32 i = 0;
				for (Controller& controller : controllers)
				{
					if (input->header.hDevice == controller.deviceHandle)
					{
						if (IsDualshock4(deviceInfo.hid))
						{
							UpdateDualshock4(i, input->data.hid.bRawData, input->data.hid.dwSizeHid);
						}
						else if (IsDualsense(deviceInfo.hid))
						{
							UpdateDualsense(i, input->data.hid.bRawData, input->data.hid.dwSizeHid);
						}
						else
						{
							ParseGenericController(i, input->data.hid.bRawData, input->data.hid.dwSizeHid, data);
						}
					}

					++i;
				}
			}
		} break;
		}
	}
}

void Input::UpdateConnectionStatus(void* deviceHandle, U64 status)
{
	U32 playerIndex = 0;
	for (Controller& controller : controllers)
	{
		XINPUT_STATE state;
		controller.connected = (XInputGetState(playerIndex, &state) == ERROR_SUCCESS);
		++playerIndex;
	}

	if (status == GIDC_ARRIVAL) { ConnectHIDJoystick(deviceHandle); }
	else if (status == GIDC_REMOVAL) { DisconnectHIDJoystick(deviceHandle); }
}

void Input::ConnectHIDJoystick(void* deviceHandle)
{
	WCHAR deviceName[1024] = {};
	UINT deviceNameLength = sizeof(deviceName) / sizeof(*deviceName);
	GetRawInputDeviceInfoW(deviceHandle, RIDI_DEVICENAME, deviceName, &deviceNameLength);
	if (!IsXboxController(deviceName))
	{
		U32 joystickIndex = 4;
		while (joystickIndex < controllers.size() && wcscmp(deviceName, controllers[joystickIndex].deviceName) != 0) { ++joystickIndex; }

		if (joystickIndex == controllers.size()) { controllers.emplace_back(); }

		Controller& controller = controllers[joystickIndex];
		controller.deviceHandle = deviceHandle;
		wcscpy_s(controller.deviceName, deviceName);
		controller.outputFile = CreateFileW(deviceName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
		HidD_GetProductString(controller.outputFile, controller.productName, Controller::maxNameLength);
		HidD_GetManufacturerString(controller.outputFile, controller.manufacturerName, Controller::maxNameLength);
		controllers[joystickIndex].connected = true;
	}
}

void Input::DisconnectHIDJoystick(void* deviceHandle)
{
	for (U32 i = 4; i < controllers.size(); ++i)
	{
		if (deviceHandle == controllers[i].deviceHandle && !IsXboxController(controllers[i].deviceName))
		{
			Controller& controller = controllers[i];
			controller.connected = false;
			if (controller.outputFile != INVALID_HANDLE_VALUE)
			{
				DWORD bytesTransferred;
				GetOverlappedResult(controller.outputFile, (LPOVERLAPPED)&controller.overlapped, &bytesTransferred, true);
				CloseHandle(controller.outputFile);
			}
		}
	}
}

void Input::UpdateDualshock4(U32 index, U8 rawData[], UL32 byteCount)
{
	static const UL32 usbInputByteCount = 64;
	static const UL32 bluetoothInputByteCount = 547;

	U32 offset = 0;
	if (byteCount == bluetoothInputByteCount) { offset = 2; }
	else if (byteCount != usbInputByteCount) { return; }

	F32 leftStickX = *(rawData + offset + 1) / 255.0f * 2 - 1;
	F32 leftStickY = *(rawData + offset + 2) / 255.0f * 2 - 1;
	F32 rightStickX = *(rawData + offset + 3) / 255.0f * 2 - 1;
	F32 rightStickY = *(rawData + offset + 4) / 255.0f * 2 - 1;
	F32 leftTrigger = *(rawData + offset + 8) / 255.0f;
	F32 rightTrigger = *(rawData + offset + 9) / 255.0f;
	U8 dpad = 0b1111 & *(rawData + offset + 5);

	UpdateButtonState(ButtonCode::GamepadX, 1 & (*(rawData + offset + 5) >> 4)); //Square
	UpdateButtonState(ButtonCode::GamepadA, 1 & (*(rawData + offset + 5) >> 5)); //X
	UpdateButtonState(ButtonCode::GamepadB, 1 & (*(rawData + offset + 5) >> 6)); //Circle
	UpdateButtonState(ButtonCode::GamepadY, 1 & (*(rawData + offset + 5) >> 7)); //Triangle
	UpdateButtonState(ButtonCode::GamepadLeftShoulder, 1 & (*(rawData + offset + 6) >> 0)); //L1
	UpdateButtonState(ButtonCode::GamepadRightShoulder, 1 & (*(rawData + offset + 6) >> 1)); //R1
	UpdateButtonState(ButtonCode::GamepadView, 1 & (*(rawData + offset + 6) >> 4)); //Share
	UpdateButtonState(ButtonCode::GamepadMenu, 1 & (*(rawData + offset + 6) >> 5)); //Options
	UpdateButtonState(ButtonCode::GamepadLeftThumbstickButton, 1 & (*(rawData + offset + 6) >> 6)); //L3
	UpdateButtonState(ButtonCode::GamepadRightThumbstickButton, 1 & (*(rawData + offset + 6) >> 7)); //R3
	//1 & (*(rawData + offset + 7) >> 0) PS button
	//1 & (*(rawData + offset + 7) >> 1) Touch pad button

	UpdateButtonState(ButtonCode::GamepadDPadUp, dpad == 0 || dpad == 1 || dpad == 7); //Dpad Up
	UpdateButtonState(ButtonCode::GamepadDPadDown, dpad == 3 || dpad == 4 || dpad == 5); //Dpad Down
	UpdateButtonState(ButtonCode::GamepadDPadLeft, dpad == 5 || dpad == 6 || dpad == 7); //Dpad Left
	UpdateButtonState(ButtonCode::GamepadDPadRight, dpad == 1 || dpad == 2 || dpad == 3); //Dpad Right

	UpdateButtonState(ButtonCode::GamepadLeftThumbstickUp, -leftStickY > 0.9f); //Left Thumbstick Up
	UpdateButtonState(ButtonCode::GamepadLeftThumbstickDown, leftStickY > 0.9f); //Left Thumbstick Down
	UpdateButtonState(ButtonCode::GamepadLeftThumbstickLeft, -leftStickX > 0.9f); //Left Thumbstick Left
	UpdateButtonState(ButtonCode::GamepadLeftThumbstickRight, leftStickX > 0.9f); //Left Thumbstick Right

	UpdateButtonState(ButtonCode::GamepadRightThumbstickUp, -rightStickY > 0.9f); //Right Thumbstick Up
	UpdateButtonState(ButtonCode::GamepadRightThumbstickDown, rightStickY > 0.9f); //Right Thumbstick Down
	UpdateButtonState(ButtonCode::GamepadRightThumbstickLeft, -rightStickX > 0.9f); //Right Thumbstick Left
	UpdateButtonState(ButtonCode::GamepadRightThumbstickRight, rightStickX > 0.9f); //Right Thumbstick Right

	UpdateButtonState(ButtonCode::GamepadLeftTrigger, leftTrigger > 0.9f); //L2
	UpdateButtonState(ButtonCode::GamepadRightTrigger, rightTrigger > 0.9f); //R2

	UpdateAxisState(AxisCode::LeftJoystickX, leftStickX);
	UpdateAxisState(AxisCode::LeftJoystickY, -leftStickY);
	UpdateAxisState(AxisCode::RightJoystickX, rightStickX);
	UpdateAxisState(AxisCode::RightJoystickY, -rightStickY);
	UpdateAxisState(AxisCode::LeftTrigger, leftTrigger);
	UpdateAxisState(AxisCode::RightTrigger, rightTrigger);

	Controller& controller = controllers[index];

	I32 headerSize = 0;
	I32 outputByteCount = 0;
	if (byteCount == usbInputByteCount)
	{
		outputByteCount = 32;
		controller.outputBuffer[0] = 0x05;
		controller.outputBuffer[1] = 0xFF;
	}
	if (byteCount == bluetoothInputByteCount)
	{
		outputByteCount = 78;
		controller.outputBuffer[0] = 0x11;
		controller.outputBuffer[1] = 0XC0;
		controller.outputBuffer[3] = 0x07;
		headerSize = 1;
	}
	controller.outputBuffer[5 + offset + headerSize] = (U8)(controller.leftRumble * 0xFF);
	controller.outputBuffer[4 + offset + headerSize] = (U8)(controller.rightRumble * 0xFF);
	controller.outputBuffer[6 + offset + headerSize] = (U8)(controller.ledRed * 0xFF);
	controller.outputBuffer[7 + offset + headerSize] = (U8)(controller.ledGreen * 0xFF);
	controller.outputBuffer[8 + offset + headerSize] = (U8)(controller.ledBlue * 0xFF);

	if (byteCount == bluetoothInputByteCount)
	{
		U32 crc = MakeReflectedCRC32(controller.outputBuffer, 75);
		memcpy(controller.outputBuffer + 75, &crc, sizeof(crc));
	}

	UL32 bytesTransferred;
	if (GetOverlappedResult(controller.outputFile, (LPOVERLAPPED)&controller.overlapped, &bytesTransferred, false))
	{
		if (controller.outputFile != INVALID_HANDLE_VALUE)
		{
			WriteFile(controller.outputFile, (void*)(controller.outputBuffer + headerSize), outputByteCount, 0, (LPOVERLAPPED)&controller.overlapped);
		}
	}
}

void Input::UpdateDualsense(U32 index, U8 rawData[], UL32 byteCount)
{
	bool bluetooth = rawData[0] == 0x31;
	if (rawData[0] != 0x01 && rawData[0] != 0x31) { return; }

	U32 offset = (bluetooth ? 2 : 0);

	F32 leftStickX = rawData[1 + offset] / 255.0f * 2 - 1;
	F32 leftStickY = rawData[2 + offset] / 255.0f * 2 - 1;
	F32 rightStickX = rawData[3 + offset] / 255.0f * 2 - 1;
	F32 rightStickY = rawData[4 + offset] / 255.0f * 2 - 1;
	F32 leftTrigger = rawData[5 + offset] / 255.0f;
	F32 rightTrigger = rawData[6 + offset] / 255.0f;
	I32 dpad = rawData[8 + offset] & 0x0F;

	UpdateButtonState(ButtonCode::GamepadX, rawData[8 + offset] & 0x10); //Square
	UpdateButtonState(ButtonCode::GamepadA, rawData[8 + offset] & 0x20); //X
	UpdateButtonState(ButtonCode::GamepadB, rawData[8 + offset] & 0x40); //Circle
	UpdateButtonState(ButtonCode::GamepadY, rawData[8 + offset] & 0x80); //Triangle
	UpdateButtonState(ButtonCode::GamepadLeftShoulder, rawData[9 + offset] & 0x01); //L1
	UpdateButtonState(ButtonCode::GamepadRightShoulder, rawData[9 + offset] & 0x02); //R1
	UpdateButtonState(ButtonCode::GamepadView, rawData[9 + offset] & 0x10); //Share
	UpdateButtonState(ButtonCode::GamepadMenu, rawData[9 + offset] & 0x20); //Options
	UpdateButtonState(ButtonCode::GamepadLeftThumbstickButton, rawData[9 + offset] & 0x40); //L3
	UpdateButtonState(ButtonCode::GamepadRightThumbstickButton, rawData[9 + offset] & 0x80); //R3
	//rawData[10 + offset] & 0x01 PS button
	//rawData[10 + offset] & 0x02 Touch pad button
	//rawData[10 + offset] & 0x04 Microphone button

	UpdateButtonState(ButtonCode::GamepadDPadUp, dpad == 7 || dpad == 0 || dpad == 1); //Dpad Up
	UpdateButtonState(ButtonCode::GamepadDPadDown, dpad == 3 || dpad == 4 || dpad == 5); //Dpad Down
	UpdateButtonState(ButtonCode::GamepadDPadLeft, dpad == 5 || dpad == 6 || dpad == 7); //Dpad Left
	UpdateButtonState(ButtonCode::GamepadDPadRight, dpad == 1 || dpad == 2 || dpad == 3); //Dpad Right

	UpdateButtonState(ButtonCode::GamepadLeftThumbstickUp, -(rawData[2 + offset] / 255.0f * 2 - 1) > 0.9f); //Left Thumbstick Up
	UpdateButtonState(ButtonCode::GamepadLeftThumbstickDown, (rawData[2 + offset] / 255.0f * 2 - 1) > 0.9f); //Left Thumbstick Down
	UpdateButtonState(ButtonCode::GamepadLeftThumbstickLeft, -(leftStickX / 255.0f * 2 - 1) > 0.9f); //Left Thumbstick Left
	UpdateButtonState(ButtonCode::GamepadLeftThumbstickRight, (leftStickX / 255.0f * 2 - 1) > 0.9f); //Left Thumbstick Right

	UpdateButtonState(ButtonCode::GamepadRightThumbstickUp, -(rightStickY / 255.0f * 2 - 1) > 0.9f); //Right Thumbstick Up
	UpdateButtonState(ButtonCode::GamepadRightThumbstickDown, (rightStickY / 255.0f * 2 - 1) > 0.9f); //Right Thumbstick Down
	UpdateButtonState(ButtonCode::GamepadRightThumbstickLeft, -(rightStickX / 255.0f * 2 - 1) > 0.9f); //Right Thumbstick Left
	UpdateButtonState(ButtonCode::GamepadRightThumbstickRight, (rightStickX / 255.0f * 2 - 1) > 0.9f); //Right Thumbstick Right

	UpdateButtonState(ButtonCode::GamepadLeftTrigger, leftTrigger > 0.9f); //L2
	UpdateButtonState(ButtonCode::GamepadRightTrigger, rightTrigger > 0.9f); //R2

	UpdateAxisState(AxisCode::LeftJoystickX, leftStickX);
	UpdateAxisState(AxisCode::LeftJoystickY, -leftStickY);
	UpdateAxisState(AxisCode::RightJoystickX, rightStickX);
	UpdateAxisState(AxisCode::RightJoystickY, -rightStickY);
	UpdateAxisState(AxisCode::LeftTrigger, leftTrigger);
	UpdateAxisState(AxisCode::RightTrigger, rightTrigger);

	Controller& controller = controllers[index];
	memset(controller.outputBuffer, 0, sizeof(controller.outputBuffer));

	I32 headerSize = 0;
	I32 outputByteCount = 0;
	if (bluetooth)
	{
		offset = 2;
		headerSize = 1;
		outputByteCount = 78;
		controller.outputBuffer[0] = 0xA2;
		controller.outputBuffer[1] = 0x31;
		controller.outputBuffer[2] = 0x02;
	}
	else
	{
		offset = 1;
		headerSize = 0;
		outputByteCount = 48;
		controller.outputBuffer[0] = 0x02;
	}

	// Enable rumble, disable audio haptics, enable right and left trigger effect
	controller.outputBuffer[0 + offset + headerSize] |= 0x0F;
	// Enable LED color
	controller.outputBuffer[1 + offset + headerSize] |= 0x04;
	// Enable rumble low pass filter
	controller.outputBuffer[38 + offset + headerSize] |= 0x04;

	controller.outputBuffer[3 + offset + headerSize] = (BYTE)(controller.leftRumble * 0xFF);
	controller.outputBuffer[2 + offset + headerSize] = (BYTE)(controller.rightRumble * 0xFF);

	SetDualsenseTriggerEffect(controller.outputBuffer + 10 + offset + headerSize, controller.rightTriggerEffect);
	SetDualsenseTriggerEffect(controller.outputBuffer + 21 + offset + headerSize, controller.leftTriggerEffect);

	controller.outputBuffer[44 + offset + headerSize] = (BYTE)(controller.ledRed * 0xFF);
	controller.outputBuffer[45 + offset + headerSize] = (BYTE)(controller.ledGreen * 0xFF);
	controller.outputBuffer[46 + offset + headerSize] = (BYTE)(controller.ledBlue * 0xFF);

	if (bluetooth)
	{
		U32 crc = MakeReflectedCRC32(controller.outputBuffer, 74);
		memcpy(controller.outputBuffer + 74, &crc, sizeof(crc));
	}

	UL32 bytesTransferred;
	if (GetOverlappedResult(controller.outputFile, (LPOVERLAPPED)&controller.overlapped, &bytesTransferred, false))
	{
		if (controller.outputFile != INVALID_HANDLE_VALUE)
		{
			WriteFile(controller.outputFile, (void*)(controller.outputBuffer + headerSize), outputByteCount, 0, (LPOVERLAPPED)&controller.overlapped);
		}
	}
}

void Input::UpdateXboxControllers()
{
	for (U32 playerIndex = 0; playerIndex < 4; ++playerIndex)
	{
		Controller& controller = controllers[playerIndex];
		XINPUT_STATE xinput;
		if (controller.connected && XInputGetState(playerIndex, &xinput) == ERROR_SUCCESS)
		{
			F32 leftStickX = xinput.Gamepad.sThumbLX / 32767.0f;
			F32 leftStickY = xinput.Gamepad.sThumbLY / 32767.0f;
			F32 rightStickX = xinput.Gamepad.sThumbRX / 32767.0f;
			F32 rightStickY = xinput.Gamepad.sThumbRY / 32767.0f;
			F32 leftTrigger = xinput.Gamepad.bLeftTrigger / 255.0f;
			F32 rightTrigger = xinput.Gamepad.bRightTrigger / 255.0f;

			UpdateButtonState(ButtonCode::GamepadX, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_X);
			UpdateButtonState(ButtonCode::GamepadA, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_A);
			UpdateButtonState(ButtonCode::GamepadB, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_B);
			UpdateButtonState(ButtonCode::GamepadY, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
			UpdateButtonState(ButtonCode::GamepadLeftShoulder, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
			UpdateButtonState(ButtonCode::GamepadRightShoulder, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
			UpdateButtonState(ButtonCode::GamepadView, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
			UpdateButtonState(ButtonCode::GamepadMenu, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_START);
			UpdateButtonState(ButtonCode::GamepadLeftThumbstickButton, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
			UpdateButtonState(ButtonCode::GamepadRightThumbstickButton, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);

			UpdateButtonState(ButtonCode::GamepadDPadUp, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
			UpdateButtonState(ButtonCode::GamepadDPadDown, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
			UpdateButtonState(ButtonCode::GamepadDPadLeft, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
			UpdateButtonState(ButtonCode::GamepadDPadRight, xinput.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

			UpdateButtonState(ButtonCode::GamepadLeftThumbstickUp, leftStickY > 0.9f);
			UpdateButtonState(ButtonCode::GamepadLeftThumbstickDown, -leftStickY > 0.9f);
			UpdateButtonState(ButtonCode::GamepadLeftThumbstickLeft, -leftStickX > 0.9f);
			UpdateButtonState(ButtonCode::GamepadLeftThumbstickRight, leftStickX > 0.9f);

			UpdateButtonState(ButtonCode::GamepadRightThumbstickUp, rightStickY > 0.9f);
			UpdateButtonState(ButtonCode::GamepadRightThumbstickDown, -rightStickY > 0.9f);
			UpdateButtonState(ButtonCode::GamepadRightThumbstickLeft, -rightStickX > 0.9f);
			UpdateButtonState(ButtonCode::GamepadRightThumbstickRight, rightStickX > 0.9f);

			UpdateButtonState(ButtonCode::GamepadLeftTrigger, leftTrigger > 0.9f);
			UpdateButtonState(ButtonCode::GamepadRightTrigger, rightTrigger > 0.9f);

			UpdateAxisState(AxisCode::LeftJoystickX, leftStickX);
			UpdateAxisState(AxisCode::LeftJoystickY, leftStickY);
			UpdateAxisState(AxisCode::RightJoystickX, rightStickX);
			UpdateAxisState(AxisCode::RightJoystickY, rightStickY);
			UpdateAxisState(AxisCode::LeftTrigger, leftTrigger);
			UpdateAxisState(AxisCode::RightTrigger, rightTrigger);

			XINPUT_VIBRATION vibration;
			vibration.wLeftMotorSpeed = (WORD)(controller.leftRumble * 0xFFFF);
			vibration.wRightMotorSpeed = (WORD)(controller.rightRumble * 0xFFFF);
			XInputSetState(playerIndex, &vibration);
		}
		else
		{
			controller.connected = false;
		}
	}
}

void Input::ParseGenericController(U32 index, U8 rawData[], UL32 dataSize, PHIDP_PREPARSED_DATA preparsedData)
{
	//HIDP_CAPS caps;
	//HidP_GetCaps(preparsedData, &caps);
	//
	//HIDP_VALUE_CAPS* valueCaps;
	//Memory::Allocate(&valueCaps, caps.NumberInputValueCaps);
	//HidP_GetValueCaps(HidP_Input, valueCaps, &caps.NumberInputValueCaps, preparsedData);
	//for (U32 i = 0; i < caps.NumberInputValueCaps; ++i)
	//{
	//	UL32 value;
	//	NTSTATUS status = HidP_GetUsageValue(HidP_Input, valueCaps[i].UsagePage, 0, valueCaps[i].Range.UsageMin, &value, preparsedData, (PCHAR)rawData, dataSize);
	//	F32 maxValue = (F32)(1 << (valueCaps[i].BitSize)) - 1;
	//	F32 normalizedValue = (value / maxValue) * 2 - 1;
	//	U32 usage = valueCaps[i].Range.UsageMin;
	//	if (usage >= 0x30 && usage <= 0x37)
	//	{
	//		I32 axisIndex = usage - 0x30;
	//		out->currentInputs[*GenericInputs::Axis0Positive + 2 * axisIndex] = normalizedValue;
	//		out->currentInputs[*GenericInputs::Axis0Negative + 2 * axisIndex] = -normalizedValue;
	//	}
	//	if (usage == 0x39)
	//	{
	//		L32 hat = value - valueCaps[i].LogicalMin;
	//		out->currentInputs[*GenericInputs::HatUp] = (hat == 0 || hat == 1 || hat == 7) ? 1.0f : 0.1f;
	//		out->currentInputs[*GenericInputs::HatRight] = (hat == 1 || hat == 2 || hat == 3) ? 1.0f : 0.1f;
	//		out->currentInputs[*GenericInputs::HatDown] = (hat == 3 || hat == 4 || hat == 5) ? 1.0f : 0.1f;
	//		out->currentInputs[*GenericInputs::HatLeft] = (hat == 5 || hat == 6 || hat == 7) ? 1.0f : 0.1f;
	//	}
	//}
	//Memory::Free(&valueCaps);
	//
	//HIDP_BUTTON_CAPS* buttonCaps;
	//Memory::Allocate(&buttonCaps, caps.NumberInputButtonCaps);
	//HidP_GetButtonCaps(HidP_Input, buttonCaps, &caps.NumberInputButtonCaps, preparsedData);
	//for (U32 i = 0; i < caps.NumberInputButtonCaps; ++i)
	//{
	//	U32 buttonCount = buttonCaps->Range.UsageMax - buttonCaps->Range.UsageMin + 1;
	//	USAGE* usages;
	//	Memory::Allocate(&usages, buttonCount);
	//	HidP_GetUsages(HidP_Input, buttonCaps[i].UsagePage, 0, usages, (PULONG)&buttonCount, preparsedData, (PCHAR)rawData, dataSize);
	//	for (U32 usagesIndex = 0; usagesIndex < buttonCount; ++usagesIndex)
	//	{
	//		U32 buttonIndex = usages[usagesIndex] - 1;
	//		if (buttonIndex < 32) { out->currentInputs[*GenericInputs::Button0 + buttonIndex] = 1.0f; }
	//	}
	//	Memory::Free(&usages);
	//}
	//
	//Memory::Free(&buttonCaps);
}

void Input::SetDualsenseTriggerEffect(U8* dst, TriggerEffect effect)
{
	if (effect.type == TriggerEffectType::Weapon)
	{
		U16 startAndEndZones = (1 << (U8)glm::round(effect.weapon.startPosition * 8)) | (1 << (U8)glm::round(effect.weapon.endPosition * 8));
		dst[0] = 0x25;
		*((U16*)(&dst[1])) = startAndEndZones;
		dst[3] = (U8)glm::round(effect.weapon.strength * 7);
	}
	if (effect.type == TriggerEffectType::Resistance)
	{
		U32 forceZones = 0;
		U16 activeZones = 0;
		for (U32 i = 0; i < 10; ++i)
		{
			if (effect.vibration.zoneStrengths[i] > 0)
			{
				activeZones |= 1 << i;
				forceZones |= ((U8)glm::round(effect.vibration.zoneStrengths[i] * 7)) << (3 * i);
			}
		}
		dst[0] = 0x21;
		*((U16*)(&dst[1])) = activeZones;
		*((U32*)(&dst[3])) = forceZones;
	}
	if (effect.type == TriggerEffectType::Vibration)
	{
		U32 forceZones = 0;
		U16 activeZones = 0;
		for (U32 i = 0; i < 10; ++i)
		{
			if (effect.vibration.zoneStrengths[i] > 0)
			{
				activeZones |= 1 << i;
				forceZones |= ((U8)glm::round(effect.vibration.zoneStrengths[i] * 7)) << (3 * i);
			}
		}
		dst[0] = 0x26;
		*((U16*)(&dst[1])) = activeZones;
		*((U32*)(&dst[3])) = forceZones;
		dst[9] = (U8)(effect.vibration.frequency * 255);
	}
}

void Input::UpdateButtonState(ButtonCode code, bool value)
{
	ButtonState& state = osStates.buttonStates[*code];

	state.doubleClicked = false;

	if (value)
	{
		if (state.pressed)
		{
			if (!state.held && currentTimestamp - state.lastPressed > holdThreshold)
			{
				state.held = true;
				eventQueue.Push(ButtonEvent{ code, InputType::Hold, currentTimestamp });
			}
		}
		else
		{
			state.pressed = true;

			eventQueue.Push(ButtonEvent{ code, InputType::Press, currentTimestamp });

			if (currentTimestamp - state.lastPressed < doublePressThreshold)
			{
				state.doubleClicked = true;
				eventQueue.Push(ButtonEvent{ code, InputType::DoublePress, currentTimestamp });
			}

			state.lastPressed = currentTimestamp;
		}
	}
	else if (state.pressed)
	{
		state.pressed = false;
		state.held = false;

		eventQueue.Push(ButtonEvent{ code, InputType::Release, currentTimestamp });
	}
}

void Input::UpdateAxisState(AxisCode code, F32 value)
{
	osStates.axisStates[*code] = value;
}

void Input::SetControllerRumbleStrength(F32 left, F32 right)
{
	for (Controller& controller : controllers)
	{
		controller.leftRumble = left;
		controller.rightRumble = right;
	}
}

void Input::SetControllerLedColor(F32 red, F32 green, F32 blue)
{
	red = glm::clamp(red, 0.0f, 1.0f);
	green = glm::clamp(green, 0.0f, 1.0f);
	blue = glm::clamp(blue, 0.0f, 1.0f);

	for (Controller& controller : controllers)
	{
		controller.ledRed = red;
		controller.ledGreen = green;
		controller.ledBlue = blue;
	}
}

void Input::SetControllerLedColor(LedColor color, F32 strength)
{

	strength = glm::clamp(strength, 0.0f, 1.0f);

	F32 red = (color == LedColor::Red || color == LedColor::Magenta || color == LedColor::Yellow || color == LedColor::White) * strength;
	F32 green = (color == LedColor::Green || color == LedColor::Yellow || color == LedColor::Cyan || color == LedColor::White) * strength;
	F32 blue = (color == LedColor::Red || color == LedColor::Magenta || color == LedColor::Cyan || color == LedColor::White) * strength;

	for (Controller& controller : controllers)
	{
		controller.ledRed = red;
		controller.ledGreen = green;
		controller.ledBlue = blue;
	}
}

void Input::SetControllerTriggerEffect(TriggerEffectType type, Trigger trigger)
{
	for (Controller& controller : controllers)
	{
		switch (type)
		{
		case TriggerEffectType::None: {
			if (trigger == Trigger::Left || trigger == Trigger::Both) { controller.leftTriggerEffect.type = TriggerEffectType::None; }
			if (trigger == Trigger::Right || trigger == Trigger::Both) { controller.rightTriggerEffect.type = TriggerEffectType::None; }
		} break;
		case TriggerEffectType::Resistance: {
			if (trigger == Trigger::Left || trigger == Trigger::Both)
			{
				controller.leftTriggerEffect = {
				.type = TriggerEffectType::Resistance,
				.resistance = {
					.zoneStrengths = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 1.0f}
				}
				};
			}

			if (trigger == Trigger::Right || trigger == Trigger::Both)
			{
				controller.rightTriggerEffect = {
					.type = TriggerEffectType::Resistance,
					.resistance = {
						.zoneStrengths = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 1.0f}
					}
				};
			}
		} break;
		case TriggerEffectType::Vibration: {
			if (trigger == Trigger::Left || trigger == Trigger::Both)
				controller.leftTriggerEffect = {
					.type = TriggerEffectType::Vibration,
					.vibration = {
						.zoneStrengths = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 1.0f},
						.frequency = 0.3f
					}
			};

			if (trigger == Trigger::Right || trigger == Trigger::Both)
			{
				controller.rightTriggerEffect = {
					.type = TriggerEffectType::Vibration,
					.vibration = {
						.zoneStrengths = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 1.0f},
						.frequency = 0.3f
					}
				};
			}
		} break;
		case TriggerEffectType::Weapon: {
			if (trigger == Trigger::Left || trigger == Trigger::Both)
			{
				controller.leftTriggerEffect.type = TriggerEffectType::Weapon;
				controller.leftTriggerEffect.weapon.startPosition = 0.2f;
				controller.leftTriggerEffect.weapon.endPosition = 0.4f;
				controller.leftTriggerEffect.weapon.strength = 0.8f;
			}

			if (trigger == Trigger::Right || trigger == Trigger::Both)
			{
				controller.rightTriggerEffect.type = TriggerEffectType::Weapon;
				controller.rightTriggerEffect.weapon.startPosition = 0.2f;
				controller.rightTriggerEffect.weapon.endPosition = 0.4f;
				controller.rightTriggerEffect.weapon.strength = 0.8f;
			}
		} break;
		}
	}
}

bool Input::IsXboxController(CW* deviceName)
{
	return wcsstr(deviceName, L"IG_");
}

bool Input::IsDualshock4(RID_DEVICE_INFO_HID info)
{
	static const UL32 sonyVendorID = 0x054C;
	static const UL32 ds4Gen1ProductID = 0x05C4;
	static const UL32 ds4Gen2ProductID = 0x09CC;

	return info.dwVendorId == sonyVendorID && (info.dwProductId == ds4Gen1ProductID || info.dwProductId == ds4Gen2ProductID);
}

bool Input::IsDualsense(RID_DEVICE_INFO_HID info)
{
	static const UL32 sonyVendorID = 0x054C;
	static const UL32 dualsenseProductID = 0x0CE6;
	static const UL32 dualsenseEdgeProductID = 0x0DF2;

	return info.dwVendorId == sonyVendorID && (info.dwProductId == dualsenseProductID || info.dwProductId == dualsenseEdgeProductID);
}

U32 Input::MakeReflectedCRC32(U8* data, U32 byteCount)
{
	static const U32 crcTable[] = {
		0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,
		0x7EB17CBD,0xE7B82D07,0x90BF1D91,0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,0x646BA8C0,
		0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,
		0xA50AB56B,0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,
		0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,0x76DC4190,
		0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,
		0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,0x8BBEB8EA,
		0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
		0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,
		0x206F85B3,0xB966D409,0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,
		0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,
		0x7D079EB1,0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
		0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,0xD80D2BDA,
		0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,
		0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,0x9B64C2B0,0xEC63F226,0x756AA39C,
		0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
		0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,
		0xF862AE69,0x616BFFD3,0x166CCF45,0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,
		0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,
		0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
	};

	U32 crc = ~0;
	while (byteCount--)
	{
		crc = (crc >> 8) ^ crcTable[(U8)crc ^ *data];
		++data;
	}
	return ~crc;
}

#endif

void Input::BindAction(const StringView& actionName, ButtonCode code)
{
	U64 hash = Hash(actionName.data(), actionName.size());
	actionBindings[hash].push_back(code);
}

void Input::BindAxis(const StringView& axisName, ButtonCode code, F32 scale)
{
	U64 hash = Hash(axisName.data(), axisName.size());
	buttonAxisBindings[hash].push_back({ code, scale });
}

void Input::BindAxis(const StringView& axisName, AxisCode code, F32 scale)
{
	U64 hash = Hash(axisName.data(), axisName.size());
	joystickAxisBindings[hash].push_back({ code, scale });
}

bool Input::GetAction(const StringView& actionName, InputType type)
{
	U64 hash = Hash(actionName.data(), actionName.size());
	auto it = actionBindings.find(hash);
	if (it != actionBindings.end())
	{
		for (ButtonCode code : it->second)
		{
			switch (type)
			{
			case InputType::Press: { return OnButtonDown(code); } break;
			case InputType::Unpress: { return OnButtonUp(code); } break;
			case InputType::DoublePress: { return OnButtonDoubleClick(code); } break;
			case InputType::Hold: { return ButtonHeld(code); } break;
			case InputType::Release: { return OnButtonRelease(code); } break;
			}
		}
	}

	return false;
}

F32 Input::GetAxis(const StringView& axisName)
{
	U64 hash = Hash(axisName.data(), axisName.size());
	F32 value = 0.0f;

	auto btnIt = buttonAxisBindings.find(hash);
	if (btnIt != buttonAxisBindings.end())
	{
		for (const auto& binding : btnIt->second)
		{
			if (ButtonDown(binding.code))
			{
				value += binding.scale;
			}
		}
	}

	auto joyIt = joystickAxisBindings.find(hash);
	if (joyIt != joystickAxisBindings.end())
	{
		for (const auto& binding : joyIt->second)
		{
			F32 axisValue = GetAxis(binding.code);

			if (glm::abs(axisValue) > 0.1f)
			{
				value += axisValue * binding.scale;
			}
		}
	}

	return glm::clamp(value, -1.0f, 1.0f);
}

bool Input::ButtonUp(ButtonCode code) { return !inputConsumed && !currentStates.buttonStates[*code].pressed; }

bool Input::ButtonDown(ButtonCode code) { return !inputConsumed && currentStates.buttonStates[*code].pressed; }

bool Input::ButtonHeld(ButtonCode code) { return !inputConsumed && currentStates.buttonStates[*code].held; }

bool Input::ButtonDragging(ButtonCode code) { return !inputConsumed && currentStates.buttonStates[*code].pressed && (deltaMousePosX || deltaMousePosY); }

bool Input::OnButtonUp(ButtonCode code) { return !inputConsumed && !currentStates.buttonStates[*code].pressed && previousStates.buttonStates[*code].pressed; }

bool Input::OnButtonDown(ButtonCode code) { return !inputConsumed && currentStates.buttonStates[*code].pressed && !previousStates.buttonStates[*code].pressed; }

bool Input::OnButtonChange(ButtonCode code) { return !inputConsumed && currentStates.buttonStates[*code].pressed != previousStates.buttonStates[*code].pressed; }

bool Input::OnButtonDoubleClick(ButtonCode code) { return !inputConsumed && currentStates.buttonStates[*code].doubleClicked; }

bool Input::OnButtonHold(ButtonCode code) { return !inputConsumed && currentStates.buttonStates[*code].held && !previousStates.buttonStates[*code].held; }

bool Input::OnButtonRelease(ButtonCode code) { return !inputConsumed && !currentStates.buttonStates[*code].held && previousStates.buttonStates[*code].held; }

glm::vec2 Input::MousePosition() { return { mousePosX, mousePosY }; }

glm::vec2 Input::PreviousMousePos() { return { mousePosX - deltaMousePosX, mousePosY - deltaMousePosY }; }

glm::vec2 Input::MouseDelta() { return { deltaMousePosX, deltaMousePosY }; }

void Input::ConsumeInput() { inputConsumed = true; }

I16 Input::MouseWheelDelta() { return mouseWheelDelta; }

I16 Input::MouseHWheelDelta() { return mouseHWheelDelta; }

F32 Input::GetAxis(AxisCode code)
{
	return currentStates.axisStates[*code];
}

const CircularQueue<ButtonEvent>& Input::GetInputEvents()
{
	return eventQueue;
}