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
#include <windowsx.h>

#include <cstddef>
#include <cstdarg>
#include <cstdio>

#include <algorithm>
#include <array>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <tchar.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

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

bool CopyTextToClipboard(TStringView text)
{
    bool ret = false;

    if (::OpenClipboard(nullptr))
    {
        HGLOBAL hGlobal = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, (text.size() + 1) * sizeof(TCHAR));

        if (hGlobal)
        {
            auto p = reinterpret_cast<LPTSTR>(::GlobalLock(hGlobal));
            text.copy(p, text.size());
            p[text.size()] = _T('\0');
            ::GlobalUnlock(hGlobal);

            ::EmptyClipboard();

#ifdef UNICODE
            UINT format = CF_UNICODETEXT;
#else
            UINT format = CF_TEXT;
#endif

            if (::SetClipboardData(format, hGlobal))
            {
                ret = true;
            }
            else
            {
                ::GlobalFree(hGlobal);
            }
        }

        ::CloseClipboard();
    }

    return ret;
}

template<typename T>
class UniqueLock
{
public:
    UniqueLock() : m_pLock(nullptr)
    {
    }

    explicit UniqueLock(T& lock) : m_pLock(&lock)
    {
        m_pLock->DoLockUnique();
    }

    UniqueLock(const UniqueLock&) = delete;
    UniqueLock& operator=(const UniqueLock&) = delete;

    UniqueLock(UniqueLock&& rhs) noexcept : m_pLock()
    {
        swap(rhs);
    }

    UniqueLock& operator=(UniqueLock&& rhs) noexcept
    {
        UniqueLock(std::move(rhs)).swap(*this);
        return *this;
    }

    ~UniqueLock()
    {
        Unlock();
    }

    void swap(UniqueLock& rhs) noexcept
    {
        using std::swap;
        swap(m_pLock, rhs.m_pLock);
    }

    void Unlock()
    {
        if (m_pLock)
        {
            m_pLock->DoUnlockUnique();
            m_pLock = nullptr;
        }
    }

private:
    T* m_pLock;
};

template<typename T>
class SharedLock
{
public:
    SharedLock() : m_pLock(nullptr)
    {
    }

    explicit SharedLock(T& lock) : m_pLock(&lock)
    {
        m_pLock->DoLockShared();
    }

    SharedLock(const SharedLock&) = delete;
    SharedLock& operator=(const SharedLock&) = delete;

    SharedLock(SharedLock&& rhs) noexcept : m_pLock()
    {
        swap(rhs);
    }

    SharedLock& operator=(SharedLock&& rhs) noexcept
    {
        SharedLock(std::move(rhs)).swap(*this);
        return *this;
    }

    ~SharedLock()
    {
        Unlock();
    }

    void swap(SharedLock& rhs) noexcept
    {
        using std::swap;
        swap(m_pLock, rhs.m_pLock);
    }

    void Unlock()
    {
        if (m_pLock)
        {
            m_pLock->DoUnlockShared();
            m_pLock = nullptr;
        }
    }

private:
    T* m_pLock;
};

class SRWLock
{
    friend class UniqueLock<SRWLock>;
    friend class SharedLock<SRWLock>;

public:
    SRWLock()
    {
        ::InitializeSRWLock(&m_Lock);
    }

    SRWLock(const SRWLock&) = delete;
    SRWLock& operator=(const SRWLock&) = delete;
    SRWLock(SRWLock&&) = delete;
    SRWLock& operator=(SRWLock&&) = delete;

    ~SRWLock() = default;

    UniqueLock<SRWLock> LockUnique()
    {
        return UniqueLock<SRWLock>(*this);
    }

    SharedLock<SRWLock> LockShared()
    {
        return SharedLock<SRWLock>(*this);
    }

    SRWLOCK* Get()
    {
        return &m_Lock;
    }

private:
    void DoLockUnique()
    {
        ::AcquireSRWLockExclusive(&m_Lock);
    }

    void DoUnlockUnique()
    {
        ::ReleaseSRWLockExclusive(&m_Lock);
    }

    void DoLockShared()
    {
        ::AcquireSRWLockShared(&m_Lock);
    }

    void DoUnlockShared()
    {
        ::ReleaseSRWLockShared(&m_Lock);
    }

    SRWLOCK m_Lock;
};

struct TextLocationInfo
{
    std::size_t Line;
    std::size_t Column;
};

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

    std::size_t GetColumnLength(std::size_t line) const
    {
        return std::transform_reduce(
            m_Fragments.begin() + m_Lines[line].Offset,
            m_Fragments.begin() + m_Lines[line].Offset + m_Lines[line].Length,
            0,
            [] (auto a, auto b)
            {
                return a + b;
            },
            [] (auto&& a)
            {
                return a.Length;
            }
        );
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

    TStringView GetLineString(std::size_t line) const
    {
        const auto& lineInfo = m_Lines[line];

        if (lineInfo.Length == 0)
        {
            return {};
        }

        std::size_t first = m_Fragments[lineInfo.Offset].Offset;
        std::size_t last = lineInfo.Offset + lineInfo.Length == m_Fragments.size()
            ? m_String.size()
            : m_Fragments[lineInfo.Offset + lineInfo.Length].Offset;

        return TStringView(m_String.data() + first, last - first);
    }

    TString CopySubstring(TextLocationInfo start, TextLocationInfo end) const
    {
        TString ret(GetLineString(start.Line).substr(start.Column, start.Line < end.Line ? TStringView::npos : end.Column + 1 - start.Column));

        for (std::size_t line = start.Line + 1; line <= end.Line; line++)
        {
            ret.append(_T("\r\n"));
            ret.append(GetLineString(line).substr(0, line < end.Line ? TStringView::npos : end.Column + 1));
        }

        return ret;
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
        m_MaxColumnLength = std::max(GetColumnLength(GetLineCount() - 1), m_MaxColumnLength);
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
        auto lock = m_Lock.LockUnique();
        m_Buffer.Clear();
	}

	void Write(LPCTSTR pText)
	{
        Write(NormalColor, pText, false);
	}

	void WriteV(LPCTSTR pFormat, va_list args)
	{
		std::array<TCHAR, BUFSIZ> buf;
		std::vsnprintf(buf.data(), buf.size(), pFormat, args);
        Write(NormalColor, buf.data(), false);
    }

    void Write(COLORREF color, LPCTSTR pText, bool newline)
    {
        auto lock = m_Lock.LockUnique();
        bool shouldAutoScroll = ShouldAutoScroll();

        m_Buffer.Append(color, pText);

        if (newline)
        {
            m_Buffer.AppendNewline();
        }

        if (shouldAutoScroll)
        {
            DoAutoScroll();
        }
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

                case WM_LBUTTONDOWN:
                    pConsoleWindow->OnLButtonDown(wParam, lParam);
                    return 0;

                case WM_LBUTTONUP:
                    pConsoleWindow->OnLButtonUp(wParam, lParam);
                    return 0;

                case WM_MOUSEMOVE:
                    pConsoleWindow->OnMouseMove(wParam, lParam);
                    return 0;

                case WM_KEYDOWN:
                    pConsoleWindow->OnKeyDown(wParam, lParam);
                    return 0;

                case WM_TIMER:
                    pConsoleWindow->OnTimer(wParam, lParam);
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
        ::SetTimer(m_hWnd, 1, 200, nullptr);

        ::ShowScrollBar(m_hWnd, SB_BOTH, true);
		InitializeGDIResource();
        InvalidateRect(m_hWnd, nullptr, true);
    }

    void OnDestroy()
    {
		UninitializeGDIResource();
    }

	void OnSize()
	{
		// Recreate everything
		InitializeGDIResource();
        InvalidateRect(m_hWnd, nullptr, true);
    }

	void OnPaint()
	{
		PAINTSTRUCT ps;
		HDC hDC = ::BeginPaint(m_hWnd, &ps);

		RECT rc;
		::GetClientRect(m_hWnd, &rc);
		::FillRect(m_hBackDC, &rc, m_hBgBrush);

        {
            auto lock = m_Lock.LockShared();
            UpdateScrollBarsNoLock();

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

            if (m_SelStart.has_value() && m_SelEnd.has_value())
            {
                auto [x, y] = PositionFromTextLocation(*m_SelStart);
                auto [xe, ye] = PositionFromTextLocation(*m_SelEnd);

                if (m_SelStart->Line == m_SelEnd->Line)
                {
                    ::BitBlt(m_hBackDC, x, y, xe - x + m_FontWidth, m_FontHeight, nullptr, 0, 0, DSTINVERT);
                }
                else
                {
                    ::BitBlt(m_hBackDC, x, y, (rc.right - rc.left) - x, m_FontHeight, nullptr, 0, 0, DSTINVERT);
                    ::BitBlt(m_hBackDC, 0, y + m_FontHeight, rc.right - rc.left, ye - (y + m_FontHeight), nullptr, 0, 0, DSTINVERT);
                    ::BitBlt(m_hBackDC, 0, ye, xe + m_FontWidth, m_FontHeight, nullptr, 0, 0, DSTINVERT);
                }
            }
        }

        ::BitBlt(hDC, 0, 0, rc.right - rc.left, rc.bottom - rc.top, m_hBackDC, 0, 0, SRCCOPY);
		::EndPaint(m_hWnd, &ps);
	}

    void OnVScroll(WPARAM wParam, LPARAM)
    {
        auto lock = m_Lock.LockUnique();

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

        InvalidateRect(m_hWnd, nullptr, true);
    }

    void OnHScroll(WPARAM wParam, LPARAM)
    {
        auto lock = m_Lock.LockUnique();

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

        InvalidateRect(m_hWnd, nullptr, true);
    }

    void OnMouseWheel(WPARAM wParam, LPARAM)
    {
        auto lock = m_Lock.LockUnique();

        m_CurrentTopLineNumber = std::clamp(
            m_CurrentTopLineNumber - GET_WHEEL_DELTA_WPARAM(wParam) * 3 / WHEEL_DELTA,
            0,
            GetMaxTopLineNumber()
        );

        InvalidateRect(m_hWnd, nullptr, true);
    }

    void OnMouseHWheel(WPARAM wParam, LPARAM)
    {
        auto lock = m_Lock.LockUnique();

        m_CurrentLeftColumnNumber = std::clamp(
            m_CurrentLeftColumnNumber + GET_WHEEL_DELTA_WPARAM(wParam) * 4 / WHEEL_DELTA,
            0,
            GetMaxLeftColumnNumber()
        );

        InvalidateRect(m_hWnd, nullptr, true);
    }

    void OnLButtonDown(WPARAM, LPARAM lParam)
    {
        auto lock = m_Lock.LockShared();
        m_SelStart = TextLocationFromPosition(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), true);
        m_SelEnd = m_SelStart;

        if (m_SelStart.has_value())
        {
            SetCapture(m_hWnd);
        }

        InvalidateRect(m_hWnd, nullptr, true);
    }

    void OnLButtonUp(WPARAM, LPARAM lParam)
    {
        if (m_SelStart.has_value())
        {
            ::ReleaseCapture();

            auto lock = m_Lock.LockShared();
            m_SelEnd = TextLocationFromPosition(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);

            if (m_SelStart.has_value() && m_SelEnd.has_value())
            {
                if (std::make_pair(m_SelStart->Line, m_SelStart->Column) > std::make_pair(m_SelEnd->Line, m_SelEnd->Column))
                {
                    m_SelStart.swap(m_SelEnd);
                }

                CopyTextToClipboard(m_Buffer.CopySubstring(*m_SelStart, *m_SelEnd));
            }
            else
            {
                m_SelStart.reset();
                m_SelEnd.reset();
            }

            InvalidateRect(m_hWnd, nullptr, true);
        }
    }

    void OnMouseMove(WPARAM wParam, LPARAM lParam)
    {
        if (m_SelStart.has_value() && (wParam & MK_LBUTTON))
        {
            auto lock = m_Lock.LockShared();
            m_SelEnd = TextLocationFromPosition(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);
            InvalidateRect(m_hWnd, nullptr, true);
        }
    }

    void OnKeyDown(WPARAM wParam, LPARAM)
    {
        auto lock = m_Lock.LockUnique();

        switch (wParam)
        {
        case 'A':
            if (::GetKeyState(VK_CONTROL) < 0)
            {
                m_SelStart = {0, 0};
                std::size_t lastLine = std::max(static_cast<std::size_t>(0), m_Buffer.GetLineCount() - 1);
                m_SelEnd = {lastLine, std::max(static_cast<std::size_t>(0), m_Buffer.GetColumnLength(lastLine) - 1)};
                CopyTextToClipboard(m_Buffer.CopySubstring(*m_SelStart, *m_SelEnd));
            }
            break;

        case VK_UP:
        case 'K':
        case 'P':
            m_CurrentTopLineNumber = std::max(0, m_CurrentTopLineNumber - 1);
            break;

        case VK_DOWN:
        case 'J':
        case 'N':
            m_CurrentTopLineNumber = std::min(m_CurrentTopLineNumber + 1, GetMaxTopLineNumber());
            break;

        case VK_LEFT:
        case 'H':
        case 'B':
            m_CurrentLeftColumnNumber = std::max(0, m_CurrentLeftColumnNumber - 1);
            break;

        case VK_RIGHT:
        case 'L':
        case 'F':
            m_CurrentLeftColumnNumber = std::min(m_CurrentLeftColumnNumber + 1, GetMaxLeftColumnNumber());
            break;

        case VK_HOME:
            m_CurrentTopLineNumber = 0;
            break;

        case VK_END:
            m_CurrentTopLineNumber = GetMaxTopLineNumber();
            break;

        default:
            break;
        }

        InvalidateRect(m_hWnd, nullptr, true);
    }

    void OnTimer(WPARAM, LPARAM)
    {
        InvalidateRect(m_hWnd, nullptr, true);
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
        return !m_SelStart.has_value() && !m_SelEnd.has_value() && GetMaxTopLineNumber() <= m_CurrentTopLineNumber;
    }

    void DoAutoScroll()
    {
        m_CurrentTopLineNumber = GetMaxTopLineNumber();
    }

    void UpdateScrollBarsNoLock()
    {
        SCROLLINFO siv = {};
        SCROLLINFO sih = {};

        siv.cbSize = sizeof(SCROLLINFO);
        siv.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
        siv.nMin = 0;
        siv.nMax = m_Buffer.GetLineCount() - 1;
        siv.nPage = static_cast<UINT>(GetVisibleLinesInWindow());
        siv.nPos = m_CurrentTopLineNumber;

        sih.cbSize = sizeof(SCROLLINFO);
        sih.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
        sih.nMin = 0;
        sih.nMax = m_Buffer.GetMaxColumnLength() - 1;
        sih.nPage = static_cast<UINT>(GetVisileColumnsInWindow());
        sih.nPos = m_CurrentLeftColumnNumber;

        ::SetScrollInfo(m_hWnd, SB_VERT, &siv, true);
        ::SetScrollInfo(m_hWnd, SB_HORZ, &sih, true);
    }

    std::optional<TextLocationInfo> TextLocationFromPosition(int x, int y, bool exact) const
    {
        int line = m_CurrentTopLineNumber + y / m_FontHeight;

        if (!exact)
        {
            line = std::clamp(line, 0, std::max(static_cast<int>(m_Buffer.GetLineCount() - 1), 0));
        }

        if (0 <= line && line < m_Buffer.GetLineCount())
        {
            int col = m_CurrentLeftColumnNumber + x / m_FontWidth;

            if (!exact)
            {
                col = std::clamp(col, 0, std::max(static_cast<int>(m_Buffer.GetColumnLength(line) - 1), 0));
            }

            if (0 <= col && col < m_Buffer.GetColumnLength(line))
            {
                return TextLocationInfo{static_cast<std::size_t>(line), static_cast<std::size_t>(col)};
            }
        }

        return std::nullopt;
    }

    std::pair<int, int> PositionFromTextLocation(TextLocationInfo loc) const
    {
        return {
            static_cast<int>((loc.Column - m_CurrentLeftColumnNumber) * m_FontWidth),
            static_cast<int>((loc.Line - m_CurrentTopLineNumber) * m_FontHeight)
        };
    }

	StyledTextBuffer& m_Buffer;
    std::optional<TextLocationInfo> m_SelStart;
    std::optional<TextLocationInfo> m_SelEnd;   // inclusive
    SRWLock m_Lock;

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
    if (::IsWindow(hConsoleWnd))
    {
        auto pConsoleWindow = reinterpret_cast<TimW32gNewConsole::NewConsoleWindow*>(
            ::GetWindowLongPtr(::GetDlgItem(hConsoleWnd, IDC_EDIT), GWLP_USERDATA)
        );

        if (pConsoleWindow)
        {
            pConsoleWindow->Clear();
        }
    }

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
            pConsoleWindow->Write(color, str, true);
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
