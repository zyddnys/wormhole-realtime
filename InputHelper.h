#pragma once

#include "common.h"

struct InputHelper
{
private:
	LPDIRECTINPUTDEVICE8	m_diKeyboardDevice;
	char                    m_diKeyStateBuffer[256];
private:
	LPDIRECTINPUTDEVICE8	m_diMouseDevice;
	DIMOUSESTATE            m_diMouseState;
private:
	LPDIRECTINPUT8			m_diDirectInput;
private:
	BOOL Device_Read(IDirectInputDevice8 *pDIDevice, void *pBuffer, long lSize)
	{
		HRESULT hr;
		while (true)
		{
			pDIDevice->Poll();
			pDIDevice->Acquire();
			if (SUCCEEDED(hr = pDIDevice->GetDeviceState(lSize, pBuffer))) break;
			if (hr != DIERR_INPUTLOST && hr != DIERR_NOTACQUIRED) return FALSE;
			if (FAILED(pDIDevice->Acquire())) return FALSE;
		}
		return TRUE;
	}
public:
	InputHelper(InputHelper const &) = delete;
	InputHelper &operator=(InputHelper const &) = delete;
	InputHelper(InputHelper &&a) noexcept
	{
		m_diKeyboardDevice = a.m_diKeyboardDevice;
		CopyMemory(m_diKeyStateBuffer, a.m_diKeyStateBuffer, sizeof(m_diKeyStateBuffer));
		m_diMouseDevice = a.m_diMouseDevice;
		m_diMouseState = a.m_diMouseState;
		m_diDirectInput = a.m_diDirectInput;
		ZeroMemory(&a.m_diKeyboardDevice, sizeof(m_diKeyboardDevice));
		ZeroMemory(a.m_diKeyStateBuffer, sizeof(m_diKeyStateBuffer));
		ZeroMemory(&a.m_diMouseDevice, sizeof(m_diMouseDevice));
		ZeroMemory(&a.m_diMouseState, sizeof(m_diMouseState));
		ZeroMemory(&a.m_diDirectInput, sizeof(m_diDirectInput));
	}
	InputHelper &operator=(InputHelper &&a) noexcept
	{
		if (std::addressof(a) != this)
		{
			m_diKeyboardDevice = a.m_diKeyboardDevice;
			CopyMemory(m_diKeyStateBuffer, a.m_diKeyStateBuffer, sizeof(m_diKeyStateBuffer));
			m_diMouseDevice = a.m_diMouseDevice;
			m_diMouseState = a.m_diMouseState;
			m_diDirectInput = a.m_diDirectInput;
			ZeroMemory(&a.m_diKeyboardDevice, sizeof(m_diKeyboardDevice));
			ZeroMemory(a.m_diKeyStateBuffer, sizeof(m_diKeyStateBuffer));
			ZeroMemory(&a.m_diMouseDevice, sizeof(m_diMouseDevice));
			ZeroMemory(&a.m_diMouseState, sizeof(m_diMouseState));
			ZeroMemory(&a.m_diDirectInput, sizeof(m_diDirectInput));
		}
		return *this;
	}
public:
	InputHelper()
	{
		ZeroMemory(&m_diKeyboardDevice, sizeof(m_diKeyboardDevice));
		ZeroMemory(m_diKeyStateBuffer, sizeof(m_diKeyStateBuffer));
		ZeroMemory(&m_diMouseDevice, sizeof(m_diMouseDevice));
		ZeroMemory(&m_diMouseState, sizeof(m_diMouseState));
		ZeroMemory(&m_diDirectInput, sizeof(m_diDirectInput));
	}
	InputHelper(HINSTANCE hInstance, HWND hWnd)
	{
		ZeroMemory(&m_diMouseState, sizeof(m_diMouseState));
		ZeroMemory(m_diKeyStateBuffer, sizeof(m_diKeyStateBuffer));

		THROW(DirectInput8Create(hInstance, 0x0800, IID_IDirectInput8, (void **)&m_diDirectInput, NULL));

		// keyboard
		THROW(m_diDirectInput->CreateDevice(GUID_SysKeyboard, &m_diKeyboardDevice, NULL));
		THROW(m_diKeyboardDevice->SetDataFormat(&c_dfDIKeyboard));
		THROW(m_diKeyboardDevice->SetCooperativeLevel(hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE));

		// mouse
		THROW(m_diDirectInput->CreateDevice(GUID_SysMouse, &m_diMouseDevice, NULL));
		THROW(m_diMouseDevice->SetDataFormat(&c_dfDIMouse));
		THROW(m_diMouseDevice->SetCooperativeLevel(hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE));
	}
	void Update()
	{
		// read mouse state
		ZeroMemory(&m_diMouseState, sizeof(m_diMouseState));
		Device_Read(m_diMouseDevice, (LPVOID)&m_diMouseState, sizeof(m_diMouseState));

		// read keyboard state
		ZeroMemory(m_diKeyStateBuffer, sizeof(m_diKeyStateBuffer));
		Device_Read(m_diKeyboardDevice, (LPVOID)m_diKeyStateBuffer, sizeof(m_diKeyStateBuffer));
	}
	char *GetKeyboardMap()
	{
		return m_diKeyStateBuffer;
	}
	LPDIMOUSESTATE GetMouseState()
	{
		return &m_diMouseState;
	}

	bool IsKeyDown(char mask)
	{
		return m_diKeyStateBuffer[mask] & 0x80;
	}

	bool IsLeftMouseDown()
	{
		return m_diMouseState.rgbButtons[0] & 0x80;
	}
	bool IsRightMouseDown()
	{
		return m_diMouseState.rgbButtons[1] & 0x80;
	}
};
