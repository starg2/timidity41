// TiMidity++ Win32 GUI New Console
// Copyright (c) 2018 Starg <https://osdn.net/projects/timidity41>


extern "C"
{

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "timidity.h"
#include "common.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"

#include "w32g.h"
#include "w32g_res.h"

#include "w32g_new_console.h"
}

#include <windows.h>

#include <cstddef>
#include <cstdarg>
#include <cstdio>

#include <algorithm>
#include <array>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include <tchar.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define NCM_CLEAR  (WM_USER + 100)
#define NCM_WRITE  (WM_USER + 101)  // WPARAM: COLORREF, LPARAM: LPCTSTR
#define NCM_WRITELINE  (WM_USER + 102)  // WPARAM: COLORREF, LPARAM: LPCTSTR

namespace TimW32gNewConsole
{

LPCTSTR pClassName = _T("TimW32gNewConsole");

// Campbell color theme
// https://github.com/Microsoft/console/blob/master/tools/ColorTool/schemes/campbell.ini
const COLORREF BackgroundColor = RGB(12, 12, 12);
const COLORREF NormalColor = RGB(204, 204, 204);
const COLORREF ErrorColor = RGB(231, 72, 86);
const COLORREF WarningColor = RGB(249, 241, 165);
const COLORREF InfoColor = RGB(58, 150, 221);

using TString = std::basic_string<TCHAR>;
using TStringView = std::basic_string_view<TCHAR>;

struct StyledLineFragment
{
	std::size_t Offset;	// offset in std::string
	std::size_t Length;
	COLORREF Color;
};

struct StyledLine
{
	std::size_t Offset;	// offset in std::vector<StyledLineFragment>
	std::size_t Length;
};

class StyledTextBuffer
{
public:
    StyledTextBuffer()
    {

    }

	void Clear()
	{
		m_Fragments.clear();
		m_Lines.clear();
		m_String.clear();
        m_MaxColumnLength = 0;
	}

	void Append(COLORREF color, LPCTSTR pText)
	{
		Append(color, TStringView(pText));
	}

	void Append(COLORREF color, TStringView text)
	{
		std::size_t offset = 0;

		while (offset < text.size())
		{
            // split input into lines
			std::size_t nlOffset = text.find_first_of(_T("\r\n"), offset);

			if (nlOffset == text.npos)
			{
				AppendNoNewline(color, text.substr(offset));
				break;
			}
			else
			{
				if (offset < nlOffset)
				{
					AppendNoNewline(color, text.substr(offset, offset - nlOffset));
				}

				AppendNewline();

				if (text[nlOffset] == _T('\r') && nlOffset + 1 < text.size() && text[nlOffset + 1] == _T('\n'))
				{
					offset = nlOffset + 2;
				}
				else
				{
					offset = nlOffset + 1;
				}
			}
		}
	}

    void AppendNewline()
    {
        m_Lines.push_back({m_Fragments.size(), 0});
    }

    std::size_t GetLineCount() const
	{
		return m_Lines.size();
	}

    std::size_t GetMaxColumnLength() const
    {
        return m_MaxColumnLength;
    }

	TStringView GetString() const
	{
		return m_String;
	}

	const std::vector<StyledLine>& GetLines() const
	{
		return m_Lines;
	}

	const std::vector<StyledLineFragment>& GetFragments() const
	{
		return m_Fragments;
	}

private:
	void AppendNoNewline(COLORREF color, TStringView text)
	{
		std::size_t stringOffset = m_String.size();
		m_String.append(text);

		std::size_t fragmentOffset = m_Fragments.size();
		m_Fragments.push_back({stringOffset, text.size(), color});

		if (m_Lines.empty())
		{
			m_Lines.push_back({fragmentOffset, 1});
		}
		else
		{
			m_Lines.back().Length++;
		}

        // update m_MaxColumnLength
        m_MaxColumnLength = std::transform_reduce(
            m_Fragments.begin() + m_Lines.back().Offset,
            m_Fragments.begin() + m_Lines.back().Offset + m_Lines.back().Length,
            m_MaxColumnLength,
            [] (auto a, auto b)
            {
                return std::max(a, b);
            },
            [] (auto&& a)
            {
                return a.Length;
            }
        );
	}

	TString m_String;
	std::vector<StyledLine> m_Lines;
	std::vector<StyledLineFragment> m_Fragments;
    std::size_t m_MaxColumnLength = 0;    // max number of characters in line
};

StyledTextBuffer GlobalNewConsoleBuffer;

class NewConsoleWindow
{
public:
    explicit NewConsoleWindow(StyledTextBuffer& buffer) : m_Buffer(buffer)
    {
    }

	NewConsoleWindow(const NewConsoleWindow&) = delete;
	NewConsoleWindow& operator=(const NewConsoleWindow&) = delete;
	~NewConsoleWindow() = default;

	void Clear()
	{
		::SendMessage(m_hWnd, NCM_CLEAR, 0, 0);
	}

	void Write(LPCTSTR pText)
	{
        ::SendMessage(m_hWnd, NCM_WRITE, NormalColor, reinterpret_cast<LPARAM>(pText));
	}

	void WriteV(LPCTSTR pFormat, va_list args)
	{
		std::array<TCHAR, BUFSIZ> buf;
		std::vsnprintf(buf.data(), buf.size(), pFormat, args);
        ::SendMessage(m_hWnd, NCM_WRITE, NormalColor, reinterpret_cast<LPARAM>(buf.data()));
    }

    void WriteLine(COLORREF color, LPCTSTR pText)
    {
        ::SendMessage(m_hWnd, NCM_WRITELINE, color, reinterpret_cast<LPARAM>(pText));
    }

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		auto pConsoleWindow = reinterpret_cast<NewConsoleWindow*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));

		switch (msg)
		{
		case WM_CREATE:
			pConsoleWindow = new NewConsoleWindow(GlobalNewConsoleBuffer);
			::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pConsoleWindow));
			pConsoleWindow->m_hWnd = hWnd;
            pConsoleWindow->OnCreate();
			return 0;

		case WM_DESTROY:
			if (pConsoleWindow)
			{
                pConsoleWindow->OnDestroy();
				pConsoleWindow->m_hWnd = nullptr;
				::SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
				delete pConsoleWindow;
			}
			return 0;

		default:
			if (pConsoleWindow)
			{
                switch (msg)
                {
				case WM_SIZE:
					pConsoleWindow->OnSize();
					return 0;

				case WM_PAINT:
					pConsoleWindow->OnPaint();
					return 0;

                case WM_VSCROLL:
                    pConsoleWindow->OnVScroll(wParam, lParam);
                    return 0;

                case WM_HSCROLL:
                    pConsoleWindow->OnHScroll(wParam, lParam);
                    return 0;

                case WM_MOUSEWHEEL:
                    pConsoleWindow->OnMouseWheel(wParam, lParam);
                    return 0;

                case WM_MOUSEHWHEEL:
                    pConsoleWindow->OnMouseHWheel(wParam, lParam);
                    return 0;

                case WM_KEYDOWN:
                    pConsoleWindow->OnKeyDown(wParam, lParam);
                    return 0;

                case NCM_CLEAR:
                    pConsoleWindow->OnNewConsoleClear();
                    return 0;

                case NCM_WRITE:
                    pConsoleWindow->OnNewConsoleWrite(wParam, lParam);
                    return 0;

                case NCM_WRITELINE:
                    pConsoleWindow->OnNewConsoleWriteLine(wParam, lParam);
                    return 0;

                default:
                    break;
                }
			}
			break;
		}

		return ::DefWindowProc(hWnd, msg, wParam, lParam);
	}

private:
    void OnCreate()
    {
		InitializeGDIResource();
        UpdateScrollBars();
    }

    void OnDestroy()
    {
		UninitializeGDIResource();
    }

	void OnSize()
	{
		// Recreate everything
		InitializeGDIResource();
        UpdateScrollBars();
	}

	void OnPaint()
	{
		PAINTSTRUCT ps;
		HDC hDC = ::BeginPaint(m_hWnd, &ps);

		RECT rc;
		::GetClientRect(m_hWnd, &rc);
		::FillRect(m_hBackDC, &rc, m_hBgBrush);

        int lineCount = std::min(static_cast<int>(m_Buffer.GetLineCount() - m_CurrentTopLineNumber), GetVisibleLinesInWindow());

        for (int i = 0; i < lineCount; i++)
        {
            auto lineInfo = m_Buffer.GetLines()[m_CurrentTopLineNumber + i];
            auto first = m_Buffer.GetFragments().begin() + lineInfo.Offset;
            auto last = first + lineInfo.Length;

            int x = -m_CurrentLeftColumnNumber * m_FontWidth;
            int y = i * m_FontHeight;

            std::for_each(
                first,
                last,
                [str = m_Buffer.GetString(), hWnd = m_hWnd, hDC = m_hBackDC, &x, y, fontWidth = m_FontWidth] (const StyledLineFragment& lf)
                {
                    ::SetTextColor(hDC, lf.Color);
                    ::TextOut(hDC, x, y, str.data() + lf.Offset, lf.Length);
                    x += lf.Length * fontWidth;
                }
            );
        }

        ::BitBlt(hDC, 0, 0, rc.right - rc.left, rc.bottom - rc.top, m_hBackDC, 0, 0, SRCCOPY);
		::EndPaint(m_hWnd, &ps);
	}

    void OnVScroll(WPARAM wParam, LPARAM)
    {
        switch (LOWORD(wParam))
        {
        case SB_TOP:
            m_CurrentTopLineNumber = 0;
            break;

        case SB_BOTTOM:
            m_CurrentTopLineNumber = GetMaxTopLineNumber();
            break;

        case SB_LINEUP:
            m_CurrentTopLineNumber = std::max(0, m_CurrentTopLineNumber - 1);
            break;

        case SB_LINEDOWN:
            m_CurrentTopLineNumber = std::min(m_CurrentTopLineNumber + 1, GetMaxTopLineNumber());
            break;

        case SB_PAGEUP:
            m_CurrentTopLineNumber = std::max(0, m_CurrentTopLineNumber - GetVisibleLinesInWindow());
            break;

        case SB_PAGEDOWN:
            m_CurrentTopLineNumber = std::min(m_CurrentTopLineNumber + GetVisibleLinesInWindow(), GetMaxTopLineNumber());
            break;

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            {
                SCROLLINFO si = {};
                si.cbSize = sizeof(SCROLLINFO);
                si.fMask = SIF_TRACKPOS;
                ::GetScrollInfo(m_hWnd, SB_VERT, &si);
                m_CurrentTopLineNumber = si.nTrackPos;
            }
            break;

        default:
            break;
        }

        UpdateScrollBars();
    }

    void OnHScroll(WPARAM wParam, LPARAM)
    {
        switch (LOWORD(wParam))
        {
        case SB_LEFT:
            m_CurrentLeftColumnNumber = 0;
            break;

        case SB_RIGHT:
            m_CurrentLeftColumnNumber = GetMaxLeftColumnNumber();
            break;

        case SB_LINELEFT:
            m_CurrentLeftColumnNumber = std::max(0, m_CurrentLeftColumnNumber - 1);
            break;

        case SB_LINERIGHT:
            m_CurrentLeftColumnNumber = std::min(m_CurrentLeftColumnNumber + 1, GetMaxLeftColumnNumber());
            break;

        case SB_PAGELEFT:
            m_CurrentLeftColumnNumber = std::max(0, m_CurrentLeftColumnNumber - GetVisileColumnsInWindow());
            break;

        case SB_PAGERIGHT:
            m_CurrentLeftColumnNumber = std::min(m_CurrentLeftColumnNumber + GetVisileColumnsInWindow(), GetMaxLeftColumnNumber());
            break;

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            {
                SCROLLINFO si = {};
                si.cbSize = sizeof(SCROLLINFO);
                si.fMask = SIF_TRACKPOS;
                ::GetScrollInfo(m_hWnd, SB_HORZ, &si);
                m_CurrentLeftColumnNumber = si.nTrackPos;
            }
            break;

        default:
            break;
        }

        UpdateScrollBars();
    }

    void OnMouseWheel(WPARAM wParam, LPARAM)
    {
        m_CurrentTopLineNumber = std::clamp(
            m_CurrentTopLineNumber - GET_WHEEL_DELTA_WPARAM(wParam) * 3 / WHEEL_DELTA,
            0,
            GetMaxTopLineNumber()
        );

        UpdateScrollBars();
    }

    void OnMouseHWheel(WPARAM wParam, LPARAM)
    {
        m_CurrentLeftColumnNumber = std::clamp(
            m_CurrentLeftColumnNumber + GET_WHEEL_DELTA_WPARAM(wParam) * 4 / WHEEL_DELTA,
            0,
            GetMaxLeftColumnNumber()
        );

        UpdateScrollBars();
    }

    void OnKeyDown(WPARAM wParam, LPARAM)
    {
        switch (wParam)
        {
        case VK_UP:
            m_CurrentTopLineNumber = std::max(0, m_CurrentTopLineNumber - 1);
            break;

        case VK_DOWN:
            m_CurrentTopLineNumber = std::min(m_CurrentTopLineNumber + 1, GetMaxTopLineNumber());
            break;

        case VK_LEFT:
            m_CurrentLeftColumnNumber = std::max(0, m_CurrentLeftColumnNumber - 1);
            break;

        case VK_RIGHT:
            m_CurrentLeftColumnNumber = std::min(m_CurrentLeftColumnNumber + 1, GetMaxLeftColumnNumber());
            break;

        default:
            break;
        }

        UpdateScrollBars();
    }

    void OnNewConsoleClear()
    {
        m_Buffer.Clear();
        UpdateScrollBars();
    }

    void OnNewConsoleWrite(WPARAM wParam, LPARAM lParam)
    {
        bool shouldAutoScroll = ShouldAutoScroll();
        m_Buffer.Append(static_cast<COLORREF>(wParam), reinterpret_cast<LPCTSTR>(lParam));

        if (shouldAutoScroll)
        {
            DoAutoScroll();
        }

        UpdateScrollBars();
    }

    void OnNewConsoleWriteLine(WPARAM wParam, LPARAM lParam)
    {
        bool shouldAutoScroll = ShouldAutoScroll();
        m_Buffer.Append(static_cast<COLORREF>(wParam), reinterpret_cast<LPCTSTR>(lParam));
        m_Buffer.AppendNewline();

        if (shouldAutoScroll)
        {
            DoAutoScroll();
        }

        UpdateScrollBars();
    }

    void InitializeGDIResource()
	{
		UninitializeGDIResource();

		HDC hDC = ::GetDC(m_hWnd);
		m_hBackDC = ::CreateCompatibleDC(hDC);
		m_SavedDCState = ::SaveDC(m_hBackDC);

		RECT rc;
		::GetClientRect(m_hWnd, &rc);
		m_hBackBitmap = ::CreateCompatibleBitmap(hDC, rc.right - rc.left, rc.bottom - rc.top);
		::SelectObject(m_hBackDC, m_hBackBitmap);

		::ReleaseDC(m_hWnd, hDC);

#ifdef JAPANESE
		m_hFont = ::CreateFont(
			-14,
			0,
			0,
			0,
			FW_REGULAR,
			false,
			false,
			false,
			SHIFTJIS_CHARSET,
			OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY,
			FIXED_PITCH | FF_DONTCARE,
			_T("ÇlÇr ÉSÉVÉbÉN")
		);
#else
		m_hFont = ::CreateFont(
			-14,
			0,
			0,
			0,
			FW_REGULAR,
			false,
			false,
			false,
			DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY,
			FIXED_PITCH | FF_DONTCARE,
			_T("Consolas")
		);
#endif
		::SelectObject(m_hBackDC, m_hFont);
        ::SetBkColor(m_hBackDC, BackgroundColor);

        TEXTMETRIC tm;
        ::GetTextMetrics(m_hBackDC, &tm);
        m_FontHeight = tm.tmHeight;
        m_FontWidth = tm.tmAveCharWidth;

		m_hBgBrush = ::CreateSolidBrush(BackgroundColor);
	}

	void UninitializeGDIResource()
	{
		if (m_hBgBrush)
		{
			::DeleteObject(m_hBgBrush);
			m_hBgBrush = nullptr;
		}

		if (m_hBackDC)
		{
			::RestoreDC(m_hBackDC, m_SavedDCState);
			m_SavedDCState = 0;
			::DeleteObject(m_hFont);
			m_hFont = nullptr;
			::DeleteObject(m_hBackBitmap);
			m_hBackBitmap = nullptr;
			::DeleteDC(m_hBackDC);
			m_hBackDC = nullptr;
		}
	}

    int GetMaxTopLineNumber() const
    {
        return std::max(0, static_cast<int>(m_Buffer.GetLineCount() - GetVisibleLinesInWindow()));
    }

    int GetMaxLeftColumnNumber() const
    {
        return m_Buffer.GetMaxColumnLength() - GetVisileColumnsInWindow();
    }

    int GetVisibleLinesInWindow() const
    {
        RECT rc;
        ::GetClientRect(m_hWnd, &rc);
        return (rc.bottom - rc.top) / m_FontHeight;
    }

    int GetVisileColumnsInWindow() const
    {
        RECT rc;
        ::GetClientRect(m_hWnd, &rc);
        return (rc.right - rc.left) / m_FontWidth;
    }

    bool ShouldAutoScroll() const
    {
        return GetMaxTopLineNumber() <= m_CurrentTopLineNumber;
    }

    void DoAutoScroll()
    {
        m_CurrentTopLineNumber = GetMaxTopLineNumber();
    }

    void UpdateScrollBars()
    {
        {
            SCROLLINFO si = {};
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_ALL;
            si.nMin = 0;
            si.nMax = m_Buffer.GetLineCount() - 1;
            si.nPage = static_cast<UINT>(GetVisibleLinesInWindow());
            si.nPos = m_CurrentTopLineNumber;

            ::SetScrollInfo(m_hWnd, SB_VERT, &si, true);
        }

        {
            SCROLLINFO si = {};
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_ALL;
            si.nMin = 0;
            si.nMax = m_Buffer.GetMaxColumnLength() - 1;
            si.nPage = static_cast<UINT>(GetVisileColumnsInWindow());
            si.nPos = m_CurrentLeftColumnNumber;

            ::SetScrollInfo(m_hWnd, SB_HORZ, &si, true);
        }

        InvalidateRect(m_hWnd, nullptr, true);
    }

	StyledTextBuffer& m_Buffer;
	HWND m_hWnd = nullptr;

    // scroll info
    int m_CurrentTopLineNumber = 0;    // line # of the top line of the window
    int m_CurrentLeftColumnNumber = 0;

	HDC m_hBackDC = nullptr;
	int m_SavedDCState = 0;
	HBITMAP m_hBackBitmap = nullptr;
	HFONT m_hFont = nullptr;
    int m_FontHeight = 0;
    int m_FontWidth = 0;   // monospace font only
	HBRUSH m_hBgBrush = nullptr;
};

} // namespace TimW32gNewConsole

extern "C" void ClearNewConsoleBuffer(void)
{
    TimW32gNewConsole::GlobalNewConsoleBuffer.Clear();
}

extern "C" void NewConsoleBufferWriteCMsg(int type, int verbosity_level, LPCTSTR str)
{
    COLORREF color = TimW32gNewConsole::NormalColor;

    if (type == CMSG_FATAL || type == CMSG_ERROR)
	{
        color = TimW32gNewConsole::ErrorColor;
	}
	else if (type == CMSG_WARNING)
	{
        color = TimW32gNewConsole::WarningColor;
	}
	else if (type == CMSG_INFO && verbosity_level <= VERB_NORMAL)
	{
        color = TimW32gNewConsole::InfoColor;
	}

    if (::IsWindow(hConsoleWnd))
    {
        auto pConsoleWindow = reinterpret_cast<TimW32gNewConsole::NewConsoleWindow*>(
            ::GetWindowLongPtr(::GetDlgItem(hConsoleWnd, IDC_EDIT), GWLP_USERDATA)
        );

        if (pConsoleWindow)
        {
            pConsoleWindow->WriteLine(color, str);
            return;
        }
    }

    TimW32gNewConsole::GlobalNewConsoleBuffer.Append(color, str);
    TimW32gNewConsole::GlobalNewConsoleBuffer.AppendNewline();
}

extern "C" void InitializeNewConsole(void)
{
    WNDCLASSEX wc = {};
	wc.cbSize = sizeof(wc);

	if (!::GetClassInfoEx(::GetModuleHandle(nullptr), TimW32gNewConsole::pClassName, &wc))
	{
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = &TimW32gNewConsole::NewConsoleWindow::WindowProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = ::GetModuleHandle(nullptr);
		wc.hIcon = nullptr;
		wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = nullptr;
		wc.lpszMenuName = nullptr;
		wc.lpszClassName = TimW32gNewConsole::pClassName;
		wc.hIconSm = nullptr;

		::RegisterClassEx(&wc);
	}
}

extern "C" void NewConsoleClear(HWND hwnd)
{
	auto pConsoleWindow = reinterpret_cast<TimW32gNewConsole::NewConsoleWindow*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (pConsoleWindow)
	{
		pConsoleWindow->Clear();
	}
}

extern "C" void NewConsoleWrite(HWND hwnd, LPCTSTR str)
{
	auto pConsoleWindow = reinterpret_cast<TimW32gNewConsole::NewConsoleWindow*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (pConsoleWindow)
	{
		pConsoleWindow->Write(str);
	}
}

extern "C" void NewConsoleWriteV(HWND hwnd, LPCTSTR format, va_list args)
{
	auto pConsoleWindow = reinterpret_cast<TimW32gNewConsole::NewConsoleWindow*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (pConsoleWindow)
	{
		pConsoleWindow->WriteV(format, args);
	}
}
