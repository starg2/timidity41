// SFZ Support Routines for TiMidity++
// Copyright (c) 2018 Starg <https://osdn.net/projects/timidity41>

extern "C"
{
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "timidity.h"
#include "common.h"
#include "controls.h"
#include "output.h"
#include "instrum.h"
#include "playmidi.h"
#include "tables.h"

#include "sfz.h"

// smplfile.c
Instrument *extract_sample_file(char *sample_file);
}

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <deque>
#include <exception>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace TimSFZ
{

using namespace std::string_literals;
using namespace std::string_view_literals;

struct TFFileCloser
{
    void operator()(timidity_file* pFile) const
    {
        if (pFile)
        {
            ::close_file(pFile);
        }
    }
};

struct InstrumentDeleter
{
    void operator()(Instrument* pInstrument) const
    {
        if (pInstrument)
        {
            ::free_instrument(pInstrument);
        }
    }
};

std::string ReadEntireFile(std::string url)
{
    std::unique_ptr<timidity_file, TFFileCloser> pFile(::open_file(url.data(), 1, OF_NORMAL));

    if (!pFile)
    {
        throw std::runtime_error("unable to open '"s + url + "'");
    }

    std::string buf;

    while (true)
    {
        int c = tf_getc(pFile.get());

        if (c == EOF)
        {
            break;
        }

        buf.push_back(static_cast<char>(c));
    }

    return buf;
}

std::string ConstructPath(std::string_view base, std::string_view relPath)
{
    std::size_t lastPathDelimiterOffset = base.find_last_of("/\\");
    return (lastPathDelimiterOffset == base.npos ? ""s : std::string(base, 0, lastPathDelimiterOffset))
        .append("/").append(relPath);
}

struct FileInfo
{
    std::string FilePath;
};

struct FileLocationInfo
{
    std::size_t FileID;
    std::size_t Line; // 1-based
};

class TextBuffer
{
public:
    TextBuffer() = default;

    TextBuffer(std::string str, FileLocationInfo loc)
        : m_Text(std::move(str)), m_Locations{PartLocationInfo{0, loc}}
    {
    }

    class View
    {
        friend class TextBuffer;

        View(const TextBuffer* pBuffer, std::size_t offset, std::size_t length)
            : m_pBuffer(pBuffer), m_Offset(offset), m_Length(length)
        {
        }

    public:
        View() : m_pBuffer(nullptr), m_Offset(0), m_Length(0)
        {
        }

        View(const View&) = default;
        View& operator=(const View&) = default;

        bool IsEmpty() const
        {
            return m_Length == 0;
        }

        std::size_t GetLength() const
        {
            return m_Length;
        }

        void SetLength(std::size_t len)
        {
            assert(len <= m_Length);
            m_Length = len;
        }

        char operator[](std::size_t i) const
        {
            return (*m_pBuffer)[m_Offset + i];
        }

        char Peek(std::size_t i = 0) const
        {
            return (*m_pBuffer)[m_Offset + i];
        }

        char PeekOr(std::size_t i = 0, char defaultValue = '\0') const
        {
            return i < m_Length ? (*m_pBuffer)[m_Offset + i] : defaultValue;
        }

        std::string ToString() const
        {
            return std::string(ToStringView());
        }

        std::string_view ToStringView() const
        {
            return std::string_view(m_pBuffer->m_Text.data() + m_Offset, m_Length);
        }

        void Advance(std::size_t count = 1)
        {
            assert(count <= m_Length);
            m_Offset += count;
            m_Length -= count;
        }

        FileLocationInfo GetLocationInfo(std::size_t i = 0) const
        {
            return m_pBuffer->GetLocationInfo(m_Offset + i);
        }

    private:
        const TextBuffer* m_pBuffer;
        std::size_t m_Offset;
        std::size_t m_Length;
    };

    View GetView() const
    {
        return View(this, 0, m_Text.size());
    }

    View GetView(std::size_t offset, std::size_t length) const
    {
        return View(this, offset, length);
    }

    char operator[](std::size_t offset) const
    {
        return m_Text[offset];
    }

private:
    struct PartLocationInfo
    {
        std::size_t Offset; // offset in m_Text
        FileLocationInfo FirstLocation;
    };

    auto FindMatchingLocationInfo(std::size_t offset) const
    {
        auto it = std::upper_bound(
            m_Locations.begin(),
            m_Locations.end(),
            offset,
            [] (auto&& a, auto&& b)
            {
                return a < b.Offset;
            }
        );

        assert(m_Locations.begin() < it);
        return std::prev(it);
    }

public:
    FileLocationInfo GetLocationInfo(std::size_t offset) const
    {
        auto it = FindMatchingLocationInfo(offset);
        auto loc = it->FirstLocation;
        loc.Line += std::count(m_Text.begin() + it->Offset, m_Text.begin() + offset, '\n');
        return loc;
    }

    void Append(char c)
    {
        m_Text.append(1, c);
    }

    void Append(std::string_view s)
    {
        m_Text.append(s);
    }

    void Append(std::string_view str, FileLocationInfo loc)
    {
        PartLocationInfo partLoc{m_Text.size(), loc};
        m_Text.append(str);
        m_Locations.push_back(partLoc);
    }

    void Append(const View& view)
    {
        assert(this != view.m_pBuffer);
        auto it = view.m_pBuffer->FindMatchingLocationInfo(view.m_Offset);
        auto partLoc = *it;
        std::ptrdiff_t offsetDiff = m_Text.size() - partLoc.Offset;
        partLoc.Offset = m_Text.size();
        partLoc.FirstLocation.Line += std::count(
            view.m_pBuffer->m_Text.begin() + it->Offset,
            view.m_pBuffer->m_Text.begin() + view.m_Offset,
            '\n'
        );

        m_Locations.push_back(std::move(partLoc));

        std::for_each(
            std::next(it),
            view.m_pBuffer->m_Locations.end(),
            [this, offsetDiff] (auto&& x)
            {
                PartLocationInfo partLoc = std::forward<decltype(x)>(x);
                partLoc.Offset += offsetDiff;
                this->m_Locations.push_back(std::move(partLoc));
            }
        );

        m_Text.append(view.m_pBuffer->m_Text, view.m_Offset, view.m_Length);
    }

private:
    std::string m_Text;
    std::vector<PartLocationInfo> m_Locations;  // must be sorted according to Offset
};

class ParserException : public std::runtime_error
{
public:
    ParserException(std::string_view fileName, std::size_t line, std::string_view msg)
        : runtime_error(FormatErrorMessage(fileName, line, msg))
    {
    }

private:
    std::string FormatErrorMessage(std::string_view fileName, std::size_t line, std::string_view msg)
    {
        std::ostringstream oss;
        oss << fileName << "(" << line << "): " << msg << "\n";
        return oss.str();
    }
};

class BasicParser
{
public:
    bool EndOfInput(TextBuffer::View& view)
    {
        return view.IsEmpty();
    }

    bool EndOfLine(TextBuffer::View& view)
    {
        return Char(view, '\n') || String(view, "\r\n");
    }

    template<typename T>
    bool CharIf(TextBuffer::View& view, T&& pred)
    {
        if (EndOfInput(view))
        {
            return false;
        }

        if (std::forward<T>(pred)(view.Peek()))
        {
            view.Advance();
            return true;
        }
        else
        {
            return false;
        }
    }

    bool AnyChar(TextBuffer::View& view, char& c)
    {
        return CharIf(view, [&c] (char x) { c = x; return true; });
    }

    bool Char(TextBuffer::View& view, char c)
    {
        return CharIf(view, [c] (char x) { return c == x; });
    }

    bool CharSet(TextBuffer::View& view, std::string_view cs)
    {
        return CharIf(view, [cs] (char x) { return cs.find(x) != cs.npos; });
    }

    bool CharRange(TextBuffer::View& view, std::pair<char, char> cr)
    {
        return CharIf(view, [cr] (char x) { return cr.first <= x && x <= cr.second; });
    }

    bool String(TextBuffer::View& view, std::string_view str)
    {
        auto curView = view;

        for (auto&& i : str)
        {
            if (!Char(curView, i))
            {
                return false;
            }
        }

        view = curView;
        return true;
    }

    bool WordStartChar(TextBuffer::View& view)
    {
        return CharIf(view, [] (char x) { return ('A' <= x && x <= 'Z') || ('a' <= x && x <= 'z') || x == '_'; });
    }

    bool WordContinueChar(TextBuffer::View& view)
    {
        return CharIf(
            view,
            [] (char x) { return ('A' <= x && x <= 'Z') || ('a' <= x && x <= 'z') || ('0' <= x && x <= '9') || x == '_'; }
        );
    }

    bool SpaceChar(TextBuffer::View& view)
    {
        return CharSet(view, " \t");
    }

    bool NonSpaceChar(TextBuffer::View& view)
    {
        return CharIf(view, [] (char x) { return x != ' ' && x != '\t' && x != '\r' && x != '\n'; });
    }

    bool AnyWord(TextBuffer::View& view, TextBuffer::View& word)
    {
        auto initView = view;

        if (!WordStartChar(view))
        {
            return false;
        }

        while (WordContinueChar(view))
        {
        }

        word = initView;
        word.SetLength(initView.GetLength() - view.GetLength());
        return true;
    }

    bool Word(TextBuffer::View& view, std::string_view word)
    {
        auto curView = view;

        if (String(curView, word) && !WordContinueChar(curView))
        {
            view = curView;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool AnyCharSequence(TextBuffer::View& view, TextBuffer::View& seq)
    {
        auto initView = view;

        if (String(view, "//") || String(view, "/*") || !NonSpaceChar(view))
        {
            return false;
        }

        auto curView = view;

        while (!String(curView, "//") && !String(curView, "/*") && NonSpaceChar(curView))
        {
            view = curView;
        }

        seq = initView;
        seq.SetLength(initView.GetLength() - view.GetLength());
        return true;
    }

    bool LineComment(TextBuffer::View& view)
    {
        if (String(view, "//"))
        {
            auto curView = view;

            while (!EndOfInput(curView) && !EndOfLine(curView))
            {
                char c;
                AnyChar(curView, c);
                view = curView;
            }

            return true;
        }
        else
        {
            return false;
        }
    }

    bool BlockComment(TextBuffer::View& view)
    {
        if (String(view, "/*"))
        {
            while (true)
            {
                if (String(view, "*/"))
                {
                    break;
                }
                else if (EndOfInput(view))
                {
                    // TODO: warn unterminated block comment
                    break;
                }
                else
                {
                    char c;
                    AnyChar(view, c);
                }
            }

            return true;
        }
        else
        {
            return false;
        }
    }

    bool DoSkips(TextBuffer::View& view)
    {
        if (!LineComment(view) && !BlockComment(view) && !SpaceChar(view))
        {
            return false;
        }

        while (LineComment(view) || BlockComment(view) || SpaceChar(view))
        {
        }

        return true;
    }

    bool DoSkipsNL(TextBuffer::View& view)
    {
        if (!LineComment(view) && !BlockComment(view) && !SpaceChar(view) && !EndOfLine(view))
        {
            return false;
        }

        while (LineComment(view) || BlockComment(view) || SpaceChar(view) || EndOfLine(view))
        {
        }

        return true;
    }

    bool Integer(TextBuffer::View& view, std::int32_t& n)
    {
        auto curView = view;
        Char(curView, '-');

        if (CharRange(curView, {'0', '9'}))
        {
            while (CharRange(curView, {'0', '9'}))
            {
            }

            auto intView = view;
            intView.SetLength(view.GetLength() - curView.GetLength());
            n = std::stoi(intView.ToString());
            view = curView;
            return true;
        }

        return false;
    }

    bool DoubleQuoteStringNoEscape(TextBuffer::View& view, TextBuffer::View& str)
    {
        auto curView = view;

        if (Char(curView, '"'))
        {
            auto startView = curView;
            auto endView = startView;

            while (true)
            {
                if (EndOfInput(curView) || EndOfLine(curView))
                {
                    // TODO: warn unterminated string literal
                    break;
                }
                else if (Char(curView, '"'))
                {
                    view = curView;
                    break;
                }
                else
                {
                    char c;
                    AnyChar(curView, c);
                    view = curView;
                    endView = curView;
                }
            }

            str = startView;
            str.SetLength(str.GetLength() - endView.GetLength());
            return true;
        }

        return false;
    }
};

class Preprocessor : private BasicParser
{
public:
    explicit Preprocessor(std::string url)
        : m_FileNames{url}, m_InBuffers{TextBuffer(ReadEntireFile(url), FileLocationInfo{0, 1})}
    {
        m_InputStack.push({m_InBuffers[0].GetView(), false});
    }

    void Preprocess()
    {
        while (true)
        {
            while (!m_InputStack.empty() && m_InputStack.top().View.IsEmpty())
            {
                m_InputStack.pop();
            }

            if (m_InputStack.empty())
            {
                break;
            }

            auto& curView = m_InputStack.top().View;
            auto initView = curView;

            // assume curview contains no preprocessor directives if m_InputStack.top().StartsAtMiddle
            DoSkipsNL(curView);

            if (!m_InputStack.top().StartsAtMiddle)
            {
                if (Word(curView, "#define"))
                {
                    DoSkips(curView);

                    if (!Char(curView, '$'))
                    {
                        throw ParserException(
                            m_FileNames[curView.GetLocationInfo().FileID],
                            curView.GetLocationInfo().Line,
                            "'#define': expected '$'"
                        );
                    }

                    TextBuffer::View nameView;
                    if (!AnyWord(curView, nameView))
                    {
                        throw ParserException(
                            m_FileNames[curView.GetLocationInfo().FileID],
                            curView.GetLocationInfo().Line,
                            "'#define': expected macro name"
                        );
                    }

                    DoSkips(curView);
                    auto macroDefView = curView;
                    auto macroDefEndView = macroDefView;

                    while (true)
                    {
                        DoSkips(curView);

                        TextBuffer::View seq;
                        if (AnyCharSequence(curView, seq))
                        {
                            macroDefEndView = curView;
                        }
                        else if (EndOfInput(curView) || EndOfLine(curView))
                        {
                            break;
                        }
                        else
                        {
                            assert(false);
                            break;
                        }
                    }

                    macroDefView.SetLength(macroDefView.GetLength() - macroDefEndView.GetLength());
                    if (m_DefinedMacros.insert_or_assign(nameView.ToString(), macroDefView).second)
                    {
                        // TODO: warn macro redefinition
                    }

                    continue;
                }
                else if (Word(curView, "#include"))
                {
                    m_IncludeCount++;

                    if (m_IncludeCount > 10000)
                    {
                        throw ParserException(
                            m_FileNames[curView.GetLocationInfo().FileID],
                            curView.GetLocationInfo().Line,
                            "too many #include directives (possible recursion?)"
                        );
                    }

                    DoSkips(curView);

                    TextBuffer::View pathView;
                    if (!DoubleQuoteStringNoEscape(curView, pathView))
                    {
                        throw ParserException(
                            m_FileNames[curView.GetLocationInfo().FileID],
                            curView.GetLocationInfo().Line,
                            "'#include': expected file name"
                        );
                    }

                    DoSkips(curView);

                    if (!EndOfInput(curView) && !EndOfLine(curView))
                    {
                        throw ParserException(
                            m_FileNames[curView.GetLocationInfo().FileID],
                            curView.GetLocationInfo().Line,
                            "'#include': unexpected characters after file name"
                        );
                    }

                    std::string path = ConstructPath(
                        m_FileNames[pathView.GetLocationInfo().FileID],
                        pathView.ToStringView()
                    );
                    m_FileNames.push_back(path);
                    auto& newBuf = m_InBuffers.emplace_back(
                        ReadEntireFile(path.data()),
                        FileLocationInfo{m_FileNames.size() - 1, 1}
                    );
                    m_InputStack.push({newBuf.GetView(), false});
                    continue;
                }
            }

            auto skipView = initView;
            skipView.SetLength(initView.GetLength() - curView.GetLength());
            m_OutBuffer.Append(skipView);

            if (Char(curView, '$'))
            {
                TextBuffer::View nameView;
                if (!AnyWord(curView, nameView))
                {
                    throw ParserException(
                        m_FileNames[curView.GetLocationInfo().FileID],
                        curView.GetLocationInfo().Line,
                        "expected macro name after '$'"
                    );
                }

                auto it = m_DefinedMacros.find(nameView.ToString());
                if (it == m_DefinedMacros.end())
                {
                    throw ParserException(
                        m_FileNames[curView.GetLocationInfo().FileID],
                        curView.GetLocationInfo().Line,
                        "macro '$"s.append(nameView.ToStringView()).append("' is not defined")
                    );
                }

                m_InputStack.push({it->second, true});
            }
            else if (TextBuffer::View word; AnyWord(curView, word))
            {
                m_OutBuffer.Append(word);
            }
            else if (char c; AnyChar(curView, c))
            {
                m_OutBuffer.Append(c);
            }
        }
    }

    std::string_view GetFileNameFromID(std::size_t id) const
    {
        return m_FileNames[id];
    }

    TextBuffer& GetOutBuffer()
    {
        return m_OutBuffer;
    }

    const TextBuffer& GetOutBuffer() const
    {
        return m_OutBuffer;
    }

private:
    struct InputStackItem
    {
        TextBuffer::View View;
        bool StartsAtMiddle;    // true for macro expansion results, false for main and #include'd files
    };

    std::vector<std::string> m_FileNames;
    std::deque<TextBuffer> m_InBuffers;
    std::stack<InputStackItem, std::vector<InputStackItem>> m_InputStack;
    TextBuffer m_OutBuffer;
    std::unordered_map<std::string, TextBuffer::View> m_DefinedMacros;
    std::int32_t m_IncludeCount = 0;
};

enum class OpCodeKind
{
    Unknown,
    AmpEG_Attack,
    AmpEG_Decay,
    AmpEG_Delay,
    AmpEG_Hold,
    AmpEG_Release,
    AmpEG_Sustain,
    AmpKeyCenter,
    AmpKeyTrack,
    AmpVelTrack,
    Cutoff,
    DefaultPath,
    End,
    HiKey,
    HiRand,
    HiVelocity,
    LoKey,
    LoopEnd,
    LoopMode,
    LoopStart,
    LoRand,
    LoVelocity,
    Offset,
    Key,
    Pan,
    PitchKeyCenter,
    Position,
    Resonance,
    RtDecay,
    Sample,
    SequenceLength,
    SequencePosition,
    SwDefault,
    SwDown,
    SwHiLast,
    SwHiKey,
    SwLast,
    SwLoLast,
    SwLoKey,
    SwPrevious,
    SwUp,
    Transpose,
    Trigger,
    Tune,
    Volume,
    Width,
    XfInHiKey,
    XfInHiVel,
    XfInLoKey,
    XfInLoVel,
    XfKeyCurve,
    XfOutHiKey,
    XfOutHiVel,
    XfOutLoKey,
    XfOutLoVel,
    XfVelCurve
};

enum class LoopModeKind
{
    NoLoop,
    OneShot,
    LoopContinuous,
    LoopSustain
};

enum class TriggerKind
{
    Attack,
    Legato,
    First,
    Release,
    ReleaseKey
};

enum class CrossfadeCurveKind
{
    Gain,
    Power
};

struct OpCodeAndValue
{
    FileLocationInfo Location;
    OpCodeKind OpCode;
    std::variant<std::int32_t, double, LoopModeKind, TriggerKind, CrossfadeCurveKind, std::string> Value;
};

enum class HeaderKind
{
    Control,
    Global,
    Master,
    Group,
    Region
};

struct Section
{
    template<typename T>
    std::optional<T> GetAs(OpCodeKind opCode) const
    {
        // search in reverse order
        auto it = std::find_if(OpCodes.rbegin(), OpCodes.rend(), [opCode] (auto&& x) { return x.OpCode == opCode; });

        if (it == OpCodes.rend())
        {
            return std::nullopt;
        }

        const T* pValue = std::get_if<T>(&it->Value);

        if (!pValue)
        {
            return std::nullopt;
        }

        return std::make_optional(*pValue);
    }

    FileLocationInfo GetLocationForOpCode(OpCodeKind opCode) const
    {
        // search in reverse order
        auto it = std::find_if(OpCodes.rbegin(), OpCodes.rend(), [opCode] (auto&& x) { return x.OpCode == opCode; });

        if (it == OpCodes.rend())
        {
            return HeaderLocation;
        }
        else
        {
            return it->Location;
        }
    }

    FileLocationInfo HeaderLocation;
    HeaderKind Header;
    std::vector<OpCodeAndValue> OpCodes;
};

class Parser : private BasicParser
{
public:
    explicit Parser(Preprocessor& pp) : m_Preprocessor(pp)
    {
    }

    Preprocessor& GetPreprocessor()
    {
        return m_Preprocessor;
    }

    const std::vector<Section>& GetSections() const
    {
        return m_Sections;
    }

    void Parse()
    {
        auto view = m_Preprocessor.GetOutBuffer().GetView();

        while (!view.IsEmpty())
        {
            DoSkipsNL(view);
            Section sec;
            sec.HeaderLocation = view.GetLocationInfo();

            if (!ParseHeader(view, sec.Header))
            {
                throw ParserException(
                    m_Preprocessor.GetFileNameFromID(view.GetLocationInfo().FileID),
                    view.GetLocationInfo().Line,
                    "expected section header"
                );
            }

            while (true)
            {
                DoSkipsNL(view);
                OpCodeAndValue opVal;
                opVal.Location = view.GetLocationInfo();

                if (ParseOpCode(view, opVal.OpCode))
                {
                    TextBuffer::View valView;
                    if (ParseValueString(view, valView))
                    {
                        switch (opVal.OpCode)
                        {
                        case OpCodeKind::AmpKeyCenter:
                        case OpCodeKind::HiKey:
                        case OpCodeKind::LoKey:
                        case OpCodeKind::PitchKeyCenter:
                        case OpCodeKind::Key:
                        case OpCodeKind::SwDefault:
                        case OpCodeKind::SwDown:
                        case OpCodeKind::SwHiKey:
                        case OpCodeKind::SwHiLast:
                        case OpCodeKind::SwLast:
                        case OpCodeKind::SwLoKey:
                        case OpCodeKind::SwLoLast:
                        case OpCodeKind::SwPrevious:
                        case OpCodeKind::SwUp:
                        case OpCodeKind::XfInHiKey:
                        case OpCodeKind::XfInLoKey:
                        case OpCodeKind::XfOutHiKey:
                        case OpCodeKind::XfOutLoKey:
                            if (std::int32_t n; ParseMIDINoteNumber(valView, n))
                            {
                                opVal.Value = n;
                            }
                            else
                            {
                                throw ParserException(
                                    m_Preprocessor.GetFileNameFromID(valView.GetLocationInfo().FileID),
                                    valView.GetLocationInfo().Line,
                                    "expected MIDI note number"
                                );
                            }
                            break;

                        case OpCodeKind::AmpEG_Attack:
                        case OpCodeKind::AmpEG_Decay:
                        case OpCodeKind::AmpEG_Delay:
                        case OpCodeKind::AmpEG_Hold:
                        case OpCodeKind::AmpEG_Release:
                        case OpCodeKind::AmpEG_Sustain:
                        case OpCodeKind::AmpKeyTrack:
                        case OpCodeKind::AmpVelTrack:
                        case OpCodeKind::Cutoff:
                        case OpCodeKind::End:
                        case OpCodeKind::HiRand:
                        case OpCodeKind::HiVelocity:
                        case OpCodeKind::LoopEnd:
                        case OpCodeKind::LoopStart:
                        case OpCodeKind::LoRand:
                        case OpCodeKind::LoVelocity:
                        case OpCodeKind::Offset:
                        case OpCodeKind::Pan:
                        case OpCodeKind::Position:
                        case OpCodeKind::Resonance:
                        case OpCodeKind::RtDecay:
                        case OpCodeKind::SequenceLength:
                        case OpCodeKind::SequencePosition:
                        case OpCodeKind::Transpose:
                        case OpCodeKind::Tune:
                        case OpCodeKind::Volume:
                        case OpCodeKind::Width:
                        case OpCodeKind::XfInHiVel:
                        case OpCodeKind::XfInLoVel:
                        case OpCodeKind::XfOutHiVel:
                        case OpCodeKind::XfOutLoVel:
                            try
                            {
                                opVal.Value = std::stod(valView.ToString());
                            }
                            catch (const std::invalid_argument&)
                            {
                                throw ParserException(
                                    m_Preprocessor.GetFileNameFromID(valView.GetLocationInfo().FileID),
                                    valView.GetLocationInfo().Line,
                                    "expected number"
                                );
                            }
                            catch (const std::out_of_range&)
                            {
                                throw ParserException(
                                    m_Preprocessor.GetFileNameFromID(valView.GetLocationInfo().FileID),
                                    valView.GetLocationInfo().Line,
                                    "overflow error in float literal"
                                );
                            }
                            break;

                        case OpCodeKind::LoopMode:
                            opVal.Value = GetLoopModeKind(valView);
                            break;

                        case OpCodeKind::Trigger:
                            opVal.Value = GetTriggerKind(valView);
                            break;

                        case OpCodeKind::XfKeyCurve:
                        case OpCodeKind::XfVelCurve:
                            opVal.Value = GetCrossfadeCurveKind(valView);
                            break;

                        default:
                            opVal.Value = valView.ToString();
                            break;
                        }

                        if (opVal.OpCode != OpCodeKind::Unknown)
                        {
                            sec.OpCodes.push_back(std::move(opVal));
                        }
                    }
                    else
                    {
                        assert(false);
                    }
                }
                else
                {
                    m_Sections.push_back(std::move(sec));
                    break;
                }
            }

        }
    }

private:
    bool ParseHeader(TextBuffer::View& view, HeaderKind& kind)
    {
        if (!Char(view, '<'))
        {
            return false;
        }

        TextBuffer::View word;
        if (!AnyWord(view, word))
        {
            throw ParserException(
                m_Preprocessor.GetFileNameFromID(view.GetLocationInfo().FileID),
                view.GetLocationInfo().Line,
                "expected header name"
            );
        }

        if (!Char(view, '>'))
        {
            throw ParserException(
                m_Preprocessor.GetFileNameFromID(view.GetLocationInfo().FileID),
                view.GetLocationInfo().Line,
                "expected '>'"
            );
        }

        static const std::unordered_map<std::string_view, HeaderKind> HeaderMap{
            {"control"sv, HeaderKind::Control},
            {"global"sv, HeaderKind::Global},
            {"group"sv, HeaderKind::Group},
            {"master"sv, HeaderKind::Master},
            {"region"sv, HeaderKind::Region}
        };

        auto it = HeaderMap.find(word.ToStringView());

        if (it == HeaderMap.end())
        {
            throw ParserException(
                m_Preprocessor.GetFileNameFromID(word.GetLocationInfo().FileID),
                word.GetLocationInfo().Line,
                "unknown header <"s.append(word.ToStringView()).append(">")
            );
        }

        kind = it->second;
        return true;
    }

    bool ParseOpCode(TextBuffer::View& view, OpCodeKind& op)
    {
        auto curView = view;

        TextBuffer::View word;
        if (!AnyWord(curView, word))
        {
            return false;
        }

        DoSkips(curView);

        if (!Char(curView, '='))
        {
            return false;
        }

        static const std::unordered_map<std::string_view, OpCodeKind> OpCodeMap{
            {"ampeg_attack"sv, OpCodeKind::AmpEG_Attack},
            {"ampeg_decay"sv, OpCodeKind::AmpEG_Decay},
            {"ampeg_delay"sv, OpCodeKind::AmpEG_Delay},
            {"ampeg_hold"sv, OpCodeKind::AmpEG_Hold},
            {"ampeg_release"sv, OpCodeKind::AmpEG_Release},
            {"ampeg_sustain"sv, OpCodeKind::AmpEG_Sustain},
            {"amp_keycenter"sv, OpCodeKind::AmpKeyCenter},
            {"amp_keytrack"sv, OpCodeKind::AmpKeyTrack},
            {"amp_veltrack"sv, OpCodeKind::AmpVelTrack},
            {"cutoff"sv, OpCodeKind::Cutoff},
            {"default_path"sv, OpCodeKind::DefaultPath},
            {"end"sv, OpCodeKind::End},
            {"hikey"sv, OpCodeKind::HiKey},
            {"hirand"sv, OpCodeKind::HiRand},
            {"hivel"sv, OpCodeKind::HiVelocity},
            {"key"sv, OpCodeKind::Key},
            {"lokey"sv, OpCodeKind::LoKey},
            {"loop_end"sv, OpCodeKind::LoopEnd},
            {"loop_mode"sv, OpCodeKind::LoopMode},
            {"loop_start"sv, OpCodeKind::LoopStart},
            {"lorand"sv, OpCodeKind::LoRand},
            {"lovel"sv, OpCodeKind::LoVelocity},
            {"offset"sv, OpCodeKind::Offset},
            {"pan"sv, OpCodeKind::Pan},
            {"pitch_keycenter"sv, OpCodeKind::PitchKeyCenter},
            {"position"sv, OpCodeKind::Position},
            {"resonance"sv, OpCodeKind::Resonance},
            {"rt_decay"sv, OpCodeKind::RtDecay},
            {"sample"sv, OpCodeKind::Sample},
            {"seq_length"sv, OpCodeKind::SequenceLength},
            {"seq_position"sv, OpCodeKind::SequencePosition},
            {"sw_default"sv, OpCodeKind::SwDefault},
            {"sw_down"sv, OpCodeKind::SwDown},
            {"sw_hikey"sv, OpCodeKind::SwHiKey},
            {"sw_hilast"sv, OpCodeKind::SwHiLast},
            {"sw_last"sv, OpCodeKind::SwLast},
            {"sw_lokey"sv, OpCodeKind::SwLoKey},
            {"sw_lolast"sv, OpCodeKind::SwLoLast},
            {"sw_previous"sv, OpCodeKind::SwPrevious},
            {"sw_up"sv, OpCodeKind::SwUp},
            {"transpose"sv, OpCodeKind::Transpose},
            {"trigger"sv, OpCodeKind::Trigger},
            {"tune"sv, OpCodeKind::Tune},
            {"volume"sv, OpCodeKind::Volume},
            {"width"sv, OpCodeKind::Width},
            {"xf_keycurve"sv, OpCodeKind::XfKeyCurve},
            {"xf_velcurve"sv, OpCodeKind::XfVelCurve},
            {"xfin_hikey"sv, OpCodeKind::XfInHiKey},
            {"xfin_hivel"sv, OpCodeKind::XfInHiVel},
            {"xfin_lokey"sv, OpCodeKind::XfInLoKey},
            {"xfin_lovel"sv, OpCodeKind::XfInLoVel},
            {"xfout_hikey"sv, OpCodeKind::XfOutHiKey},
            {"xfout_hivel"sv, OpCodeKind::XfOutHiVel},
            {"xfout_lokey"sv, OpCodeKind::XfOutLoKey},
            {"xfout_lovel"sv, OpCodeKind::XfOutLoVel}
        };

        auto it = OpCodeMap.find(word.ToStringView());

        if (it == OpCodeMap.end())
        {
            ctl->cmsg(
                CMSG_WARNING,
                VERB_VERBOSE,
                "%s(%u): ignoring unsupported opcode '%s'",
                std::string(m_Preprocessor.GetFileNameFromID(word.GetLocationInfo().FileID)).c_str(),
                static_cast<std::uint32_t>(word.GetLocationInfo().Line),
                word.ToString().c_str()
            );

            op = OpCodeKind::Unknown;
        }
        else
        {
            op = it->second;
        }

        view = curView;
        return true;
    }

    bool ParseValueString(TextBuffer::View& view, TextBuffer::View& value)
    {
        auto curView = view;

        while (SpaceChar(curView))
        {
        }

        auto startView = curView;
        auto endView = startView;

        while (true)
        {
            while (SpaceChar(curView))
            {
            }

            if (EndOfInput(curView) || EndOfLine(curView) || LineComment(curView) || BlockComment(curView))
            {
                break;
            }
            else if (Char(curView, '<'))
            {
                break;
            }
            else if (OpCodeKind op; ParseOpCode(curView, op))
            {
                break;
            }
            else if (TextBuffer::View seq; AnyCharSequence(curView, seq))
            {
                endView = curView;
            }
            else
            {
                assert(false);
            }
        }

        view = endView;
        value = startView;
        value.SetLength(startView.GetLength() - endView.GetLength());
        return true;
    }

    bool ParseMIDINoteNumber(TextBuffer::View& view, std::int32_t& n)
    {
        if (Integer(view, n))
        {
            return true;
        }

        auto pred = [&n] (char x)
        {
            switch (x)
            {
            case 'C':
            case 'c':
                n = 0;
                return true;

            case 'D':
            case 'd':
                n = 2;
                return true;

            case 'E':
            case 'e':
                n = 4;
                return true;

            case 'F':
            case 'f':
                n = 5;
                return true;

            case 'G':
            case 'g':
                n = 7;
                return true;

            case 'A':
            case 'a':
                n = 9;
                return true;

            case 'B':
            case 'b':
                n = 11;
                return true;

            default:
                return false;
            }
        };

        if (!CharIf(view, pred))
        {
            return false;
        }

        if (Char(view, '#'))
        {
            n++;
        }

        std::int32_t oct;
        if (!Integer(view, oct))
        {
            throw ParserException(
                m_Preprocessor.GetFileNameFromID(view.GetLocationInfo().FileID),
                view.GetLocationInfo().Line,
                "expected octave number"
            );
        }

        n += oct * 12;
        return true;
    }

    LoopModeKind GetLoopModeKind(TextBuffer::View view)
    {
        auto curView = view;
        if (TextBuffer::View word; AnyWord(curView, word))
        {
            static const std::unordered_map<std::string_view, LoopModeKind> LoopModeKindMap{
                {"no_loop"sv, LoopModeKind::NoLoop},
                {"one_shot"sv, LoopModeKind::OneShot},
                {"loop_continuous"sv, LoopModeKind::LoopContinuous},
                {"loop_sustain"sv, LoopModeKind::LoopSustain}
            };

            auto it = LoopModeKindMap.find(word.ToStringView());

            if (it != LoopModeKindMap.end())
            {
                return it->second;
            }
        }

        throw ParserException(
            m_Preprocessor.GetFileNameFromID(view.GetLocationInfo().FileID),
            view.GetLocationInfo().Line,
            "unknown loop_mode '"s.append(view.ToStringView()).append("'")
        );
    }

    TriggerKind GetTriggerKind(TextBuffer::View view)
    {
        auto curView = view;
        if (TextBuffer::View word; AnyWord(curView, word))
        {
            static const std::unordered_map<std::string_view, TriggerKind> TriggerKindMap{
                {"attack"sv, TriggerKind::Attack},
                {"first"sv, TriggerKind::First},
                {"legato"sv, TriggerKind::Legato},
                {"release"sv, TriggerKind::Release},
                {"release_key"sv, TriggerKind::ReleaseKey}
            };

            auto it = TriggerKindMap.find(word.ToStringView());

            if (it != TriggerKindMap.end())
            {
                return it->second;
            }
        }

        throw ParserException(
            m_Preprocessor.GetFileNameFromID(view.GetLocationInfo().FileID),
            view.GetLocationInfo().Line,
            "unknown trigger mode '"s.append(view.ToStringView()).append("'")
        );
    }

    CrossfadeCurveKind GetCrossfadeCurveKind(TextBuffer::View view)
    {
        auto curView = view;
        if (TextBuffer::View word; AnyWord(curView, word))
        {
            static const std::unordered_map<std::string_view, CrossfadeCurveKind> CrossfadeCurveKindMap{
                {"gain"sv, CrossfadeCurveKind::Gain},
                {"power"sv, CrossfadeCurveKind::Power}
            };

            auto it = CrossfadeCurveKindMap.find(word.ToStringView());

            if (it != CrossfadeCurveKindMap.end())
            {
                return it->second;
            }
        }

        throw ParserException(
            m_Preprocessor.GetFileNameFromID(view.GetLocationInfo().FileID),
            view.GetLocationInfo().Line,
            "unknown crossfade curve '"s.append(view.ToStringView()).append("'")
        );
    }

    Preprocessor& m_Preprocessor;
    std::vector<Section> m_Sections;
};

class InstrumentBuilder
{
public:
    InstrumentBuilder(Parser& parser, std::string_view filePath) : m_Parser(parser)
    {
        std::size_t pathDelimiterOffset = filePath.find_last_of("/\\");

        if (pathDelimiterOffset == filePath.npos)
        {
            m_Name = filePath;
        }
        else
        {
            m_FileDir = filePath.substr(0, pathDelimiterOffset + 1);
            m_Name = filePath.substr(pathDelimiterOffset + 1);
        }
    }

    std::unique_ptr<Instrument, InstrumentDeleter> BuildInstrument()
    {
        auto flatSections = FlattenSections(m_Parser.GetSections());
        std::unique_ptr<Instrument, InstrumentDeleter> pInstrument(reinterpret_cast<Instrument*>(safe_calloc(sizeof(Instrument), 1)));
        pInstrument->type = INST_SFZ;
        pInstrument->instname = safe_strdup(m_Name.c_str());

        std::vector<std::unique_ptr<Instrument, InstrumentDeleter>> sampleInstruments;
        sampleInstruments.reserve(flatSections.size());

        for (auto&& i : flatSections)
        {
            sampleInstruments.push_back(BuildSample(i));
        }

        pInstrument->samples = std::accumulate(
            sampleInstruments.begin(),
            sampleInstruments.end(),
            0,
            [] (auto&& a, auto&& b)
            {
                return a + b->samples;
            }
        );

        pInstrument->sample = reinterpret_cast<Sample*>(safe_calloc(sizeof(Sample), pInstrument->samples));
        Sample* pCurrentSample = pInstrument->sample;

        for (auto&& i : sampleInstruments)
        {
            pCurrentSample = std::copy_n(i->sample, i->samples, pCurrentSample);
            std::for_each(i->sample, i->sample + i->samples, [] (auto&& x) { x.data_alloced = false; });
        }

        return pInstrument;
    }

private:
    std::unique_ptr<Instrument, InstrumentDeleter> BuildSample(const Section& flatSection)
    {
        if (auto sampleName = flatSection.GetAs<std::string>(OpCodeKind::Sample))
        {
            std::string pathPrefix = m_FileDir + flatSection.GetAs<std::string>(OpCodeKind::DefaultPath).value_or("");

            if (!pathPrefix.empty() && (pathPrefix.back() != '/' && pathPrefix.back() != '\\'))
            {
                pathPrefix += '/';
            }

            auto pSampleInstrument = BuildSingleSampleInstrument(pathPrefix + *sampleName);

            for (int i = 0; i < pSampleInstrument->samples; i++)
            {
                Sample& s = pSampleInstrument->sample[i];

                s.high_key = 127;
                s.low_key = 0;
                s.root_key = 60;

                if (auto key = flatSection.GetAs<std::int32_t>(OpCodeKind::Key))
                {
                    int8 keyVal = static_cast<int8>(std::clamp(key.value(), 0, 127));
                    s.high_key = keyVal;
                    s.low_key = keyVal;
                    s.root_key = keyVal;
                }

                s.high_key = static_cast<int8>(std::clamp(flatSection.GetAs<std::int32_t>(OpCodeKind::HiKey).value_or(s.high_key), 0, 127));
                s.low_key = static_cast<int8>(std::clamp(flatSection.GetAs<std::int32_t>(OpCodeKind::LoKey).value_or(s.low_key), 0, 127));
                s.root_key = static_cast<int8>(std::clamp(flatSection.GetAs<std::int32_t>(OpCodeKind::PitchKeyCenter).value_or(s.root_key), 0, 127));

                s.root_freq = ::freq_table[s.root_key];

                s.high_vel = static_cast<uint8>(std::clamp(std::lround(flatSection.GetAs<double>(OpCodeKind::HiVelocity).value_or(127.0)), 0L, 127L));
                s.low_vel = static_cast<uint8>(std::clamp(std::lround(flatSection.GetAs<double>(OpCodeKind::LoVelocity).value_or(0.0)), 0L, 127L));

                if (auto end = flatSection.GetAs<double>(OpCodeKind::End))
                {
                    s.data_length = std::clamp(
                        std::llround(end.value() * (1 << FRACTION_BITS)),
                        static_cast<splen_t>(0),
                        s.data_length
                    );
                }

                s.loop_start = std::clamp(
                    static_cast<splen_t>(flatSection.GetAs<double>(OpCodeKind::LoopStart).value_or(0) * (1 << FRACTION_BITS)),
                    static_cast<splen_t>(0),
                    std::max(static_cast<splen_t>(0), s.data_length - (1 << FRACTION_BITS))
                );

                s.loop_end = std::clamp(
                    static_cast<splen_t>(flatSection.GetAs<double>(OpCodeKind::LoopEnd).value_or(s.data_length >> FRACTION_BITS) * (1 << FRACTION_BITS)),
                    static_cast<splen_t>(0),
                    s.data_length
                );

                if (auto offset = flatSection.GetAs<double>(OpCodeKind::Offset))
                {
                    s.offset = std::clamp(
                        std::llround(offset.value() * (1 << FRACTION_BITS)),
                        static_cast<splen_t>(0),
                        std::max(static_cast<splen_t>(0), s.data_length - (1 << FRACTION_BITS))
                    );
                }

                s.modes |= MODES_ENVELOPE;
                s.modes &= ~(MODES_LOOPING | MODES_PINGPONG | MODES_REVERSE | MODES_SUSTAIN | MODES_RELEASE);

                LoopModeKind defaultLoopModeKind = 
                    flatSection.GetAs<double>(OpCodeKind::LoopStart).has_value() || flatSection.GetAs<double>(OpCodeKind::LoopEnd).has_value()
                    ? LoopModeKind::LoopContinuous
                    : LoopModeKind::NoLoop;

                switch (flatSection.GetAs<LoopModeKind>(OpCodeKind::LoopMode).value_or(defaultLoopModeKind))
                {
                case LoopModeKind::NoLoop:
                    s.loop_start = s.data_length;
                    s.loop_end = s.data_length;
                    break;

                case LoopModeKind::OneShot:
                    s.modes |= MODES_NO_NOTEOFF;
                    s.loop_start = s.data_length;
                    s.loop_end = s.data_length;
                    break;

                case LoopModeKind::LoopContinuous:
                    s.modes |= MODES_LOOPING | MODES_SUSTAIN;
                    s.data_length = s.loop_end;
                    break;

                case LoopModeKind::LoopSustain:
                    s.modes |= MODES_LOOPING | MODES_SUSTAIN | MODES_RELEASE;
                    break;
                }

                s.volume = std::pow(10.0, flatSection.GetAs<double>(OpCodeKind::Volume).value_or(0.0) / 10.0);
                s.tune = std::pow(
                    2.0,
                    std::clamp(flatSection.GetAs<double>(OpCodeKind::Transpose).value_or(0.0), -127.0, 127.0) / 12.0
                        + std::clamp(flatSection.GetAs<double>(OpCodeKind::Tune).value_or(0.0), -100.0, 100.0) / 1200.0
                );


                double panValue = std::clamp(flatSection.GetAs<double>(OpCodeKind::Pan).value_or(0.0) / 200.0, -0.5, 0.5);

                if (pSampleInstrument->samples == 2)    // stereo
                {
                    s.volume *= std::clamp((i == 0 ? 0.5 - panValue : panValue + 0.5) * 2.0, 0.0, 2.0);

                    s.sample_pan = std::clamp(
                        s.sample_pan * flatSection.GetAs<double>(OpCodeKind::Width).value_or(100.0) / 100.0,
                        -0.5,
                        0.5
                    );

                    s.sample_pan = std::clamp(
                        s.sample_pan + flatSection.GetAs<double>(OpCodeKind::Position).value_or(0.0) / 100.0,
                        -0.5,
                        0.5
                    );
                }
                else
                {
                    s.sample_pan = panValue;

                    if (i == 0)
                    {
                        for (auto op : {OpCodeKind::Width, OpCodeKind::Position})
                        {
                            if (flatSection.GetAs<double>(op).has_value())
                            {
                                auto loc = flatSection.GetLocationForOpCode(op);
                                ctl->cmsg(
                                    CMSG_WARNING,
                                    VERB_VERBOSE,
                                    "%s(%u): 'width' and 'position' opcodes are only operational for stereo samples",
                                    std::string(m_Parser.GetPreprocessor().GetFileNameFromID(loc.FileID)).c_str(),
                                    static_cast<std::uint32_t>(loc.Line)
                                );
                            }
                        }
                    }
                }

                s.envelope_keyf_bpo = static_cast<int8>(std::clamp(flatSection.GetAs<std::int32_t>(OpCodeKind::AmpKeyCenter).value_or(60), -127, 127));
                s.envelope_velf_bpo = 0;
                s.modenv_velf_bpo = 0;

                s.envelope_delay = std::lround(std::clamp(flatSection.GetAs<double>(OpCodeKind::AmpEG_Delay).value_or(0.0), 0.0, 100.0) * s.sample_rate);

                switch (flatSection.GetAs<TriggerKind>(OpCodeKind::Trigger).value_or(TriggerKind::Attack))
                {
                case TriggerKind::Attack:
                    break;

                // TODO: support trigger=release
                case TriggerKind::Release:
                    if (i == 0)
                    {
                        auto loc = flatSection.GetLocationForOpCode(OpCodeKind::Trigger);
                        ctl->cmsg(
                            CMSG_WARNING,
                            VERB_VERBOSE,
                            "%s(%u): trigger=release is not implemented yet and will be treated as trigger=release_key",
                            std::string(m_Parser.GetPreprocessor().GetFileNameFromID(loc.FileID)).c_str(),
                            static_cast<std::uint32_t>(loc.Line)
                        );
                    }

                    s.modes |= MODES_TRIGGER_RELEASE;
                    break;

                case TriggerKind::ReleaseKey:
                    s.modes |= MODES_TRIGGER_RELEASE;
                    break;

                // TODO: support trigger=legato and trigger=first
                case TriggerKind::First:
                case TriggerKind::Legato:
                default:
                    if (i == 0)
                    {
                        auto loc = flatSection.GetLocationForOpCode(OpCodeKind::Trigger);
                        ctl->cmsg(
                            CMSG_WARNING,
                            VERB_VERBOSE,
                            "%s(%u): unsupported trigger mode",
                            std::string(m_Parser.GetPreprocessor().GetFileNameFromID(loc.FileID)).c_str(),
                            static_cast<std::uint32_t>(loc.Line)
                        );
                    }
                    break;
                }

                s.rt_decay = std::clamp(flatSection.GetAs<double>(OpCodeKind::RtDecay).value_or(0.0), 0.0, 200.0);

                s.envelope_offset[0] = ToOffset(65535);
                s.envelope_rate[0] = CalcRate(65535, std::clamp(flatSection.GetAs<double>(OpCodeKind::AmpEG_Attack).value_or(0.0), 0.0, 100.0));
                s.envelope_offset[1] = ToOffset(65534);
                s.envelope_rate[1] = CalcRate(1, std::clamp(flatSection.GetAs<double>(OpCodeKind::AmpEG_Hold).value_or(0.0), 0.0, 100.0));

                std::int32_t sustainLevel = std::lround(65533.0 * std::clamp(flatSection.GetAs<double>(OpCodeKind::AmpEG_Sustain).value_or(100.0), 0.0, 100.0) / 100.0);
                s.envelope_offset[2] = ToOffset(sustainLevel);
                s.envelope_rate[2] = CalcRate(65534 - sustainLevel, std::clamp(flatSection.GetAs<double>(OpCodeKind::AmpEG_Decay).value_or(0.0), 0.0, 100.0));

                double releaseTime = std::clamp(flatSection.GetAs<double>(OpCodeKind::AmpEG_Release).value_or(0.0), 0.0, 100.0);
                s.envelope_offset[3] = 0;
                s.envelope_rate[3] = CalcRate(65535, releaseTime);
                s.envelope_offset[4] = s.envelope_offset[3];
                s.envelope_rate[4] = s.envelope_rate[3];
                s.envelope_offset[5] = s.envelope_offset[3];
                s.envelope_rate[5] = s.envelope_rate[3];

                if (auto ampKeyTrack = flatSection.GetAs<double>(OpCodeKind::AmpKeyTrack))
                {
                    std::fill(
                        std::begin(s.envelope_keyf),
                        std::end(s.envelope_keyf),
                        static_cast<int16>(std::clamp(ampKeyTrack.value(), -96.0, 12.0) * 0.1 * std::log2(10.0))
                    );
                }

                if (auto ampVelTrack = flatSection.GetAs<double>(OpCodeKind::AmpVelTrack))
                {
                    // convert percent to rate
                    std::fill(
                        std::begin(s.envelope_velf),
                        std::end(s.envelope_velf),
                        static_cast<int16>(std::clamp(ampVelTrack.value() * 0.01, -1.0, 1.0) * 1200.0 / 127.0)
                    );
                }

                if (auto cutoff = flatSection.GetAs<double>(OpCodeKind::Cutoff))
                {
                    s.cutoff_freq = std::clamp(static_cast<int32>(std::round(cutoff.value())), 1, 20000);
                }

                if (auto resonance = flatSection.GetAs<double>(OpCodeKind::Resonance))
                {
                    int resoCB = static_cast<int>(std::round(std::clamp(resonance.value(), 0.0, 40.0) * 10.0));
                    s.resonance = static_cast<int16>(std::clamp(resoCB, 0, 960));
                }

                if (auto seqLen = flatSection.GetAs<double>(OpCodeKind::SequenceLength))
                {
                    s.seq_length = std::clamp(static_cast<int32>(std::round(seqLen.value())), 1, 100);

                    if (auto seqPos = flatSection.GetAs<double>(OpCodeKind::SequencePosition))
                    {
                        s.seq_position = std::clamp(static_cast<int32>(std::round(seqPos.value())), 1, 100);

                        if (s.seq_length < s.seq_position)
                        {
                            if (i == 0)
                            {
                                auto loc = flatSection.GetLocationForOpCode(OpCodeKind::SequencePosition);
                                ctl->cmsg(
                                    CMSG_WARNING,
                                    VERB_VERBOSE,
                                    "%s(%u): 'seq_position' is larger than 'seq_length'; this region will never be played",
                                    std::string(m_Parser.GetPreprocessor().GetFileNameFromID(loc.FileID)).c_str(),
                                    static_cast<std::uint32_t>(loc.Line)
                                );
                            }
                        }
                    }
                    else
                    {
                        if (i == 0)
                        {
                            auto loc = flatSection.GetLocationForOpCode(OpCodeKind::SequenceLength);
                            ctl->cmsg(
                                CMSG_WARNING,
                                VERB_VERBOSE,
                                "%s(%u): 'seq_length' was specified but 'seq_position' was not; this region will never be played",
                                std::string(m_Parser.GetPreprocessor().GetFileNameFromID(loc.FileID)).c_str(),
                                static_cast<std::uint32_t>(loc.Line)
                            );
                        }
                    }
                }
                else if (auto seqPos = flatSection.GetAs<double>(OpCodeKind::SequencePosition))
                {
                    if (i == 0)
                    {
                        auto loc = flatSection.GetLocationForOpCode(OpCodeKind::SequencePosition);
                        ctl->cmsg(
                            CMSG_WARNING,
                            VERB_VERBOSE,
                            "%s(%u): 'seq_position' was specified but 'seq_length' was not",
                            std::string(m_Parser.GetPreprocessor().GetFileNameFromID(loc.FileID)).c_str(),
                            static_cast<std::uint32_t>(loc.Line)
                        );
                    }
                }

                if (auto loRand = flatSection.GetAs<double>(OpCodeKind::LoRand))
                {
                    s.modes |= MODES_TRIGGER_RANDOM;
                    s.lorand = std::clamp(loRand.value(), 0.0, 1.0);
                    s.hirand = std::clamp(flatSection.GetAs<double>(OpCodeKind::HiRand).value_or(1.0), 0.0, 1.0);
                }
                else if (auto hiRand = flatSection.GetAs<double>(OpCodeKind::HiRand))
                {
                    s.modes |= MODES_TRIGGER_RANDOM;
                    s.lorand = 0.0;
                    s.hirand = std::clamp(hiRand.value(), 0.0, 1.0);
                }

                auto xfInHiKey = flatSection.GetAs<std::int32_t>(OpCodeKind::XfInHiKey);
                auto xfInLoKey = flatSection.GetAs<std::int32_t>(OpCodeKind::XfInLoKey);
                auto xfOutHiKey = flatSection.GetAs<std::int32_t>(OpCodeKind::XfOutHiKey);
                auto xfOutLoKey = flatSection.GetAs<std::int32_t>(OpCodeKind::XfOutLoKey);

                if (xfInHiKey.has_value() || xfInLoKey.has_value() || xfOutHiKey.has_value() || xfOutLoKey.has_value())
                {
                    switch (flatSection.GetAs<CrossfadeCurveKind>(OpCodeKind::XfKeyCurve).value_or(CrossfadeCurveKind::Power))
                    {
                    case CrossfadeCurveKind::Gain:
                        s.xfmode_key = CROSSFADE_GAIN;
                        break;

                    case CrossfadeCurveKind::Power:
                    default:
                        s.xfmode_key = CROSSFADE_POWER;
                        break;
                    }

                    s.xfin_hikey = static_cast<int8>(std::clamp(xfInHiKey.value_or(0), 0, 127));
                    s.xfin_lokey = static_cast<int8>(std::clamp(xfInLoKey.value_or(0), 0, 127));
                    s.xfout_hikey = static_cast<int8>(std::clamp(xfOutHiKey.value_or(127), 0, 127));
                    s.xfout_lokey = static_cast<int8>(std::clamp(xfOutLoKey.value_or(127), 0, 127));
                }
                else
                {
                    s.xfmode_key = CROSSFADE_NONE;
                }

                auto xfInHiVel = flatSection.GetAs<double>(OpCodeKind::XfInHiVel);
                auto xfInLoVel = flatSection.GetAs<double>(OpCodeKind::XfInLoVel);
                auto xfOutHiVel = flatSection.GetAs<double>(OpCodeKind::XfOutHiVel);
                auto xfOutLoVel = flatSection.GetAs<double>(OpCodeKind::XfOutLoVel);

                if (xfInHiVel.has_value() || xfInLoVel.has_value() || xfOutHiVel.has_value() || xfOutLoVel.has_value())
                {
                    switch (flatSection.GetAs<CrossfadeCurveKind>(OpCodeKind::XfVelCurve).value_or(CrossfadeCurveKind::Power))
                    {
                    case CrossfadeCurveKind::Gain:
                        s.xfmode_vel = CROSSFADE_GAIN;
                        break;

                    case CrossfadeCurveKind::Power:
                    default:
                        s.xfmode_vel = CROSSFADE_POWER;
                        break;
                    }

                    s.xfin_hivel = static_cast<int8>(std::clamp(std::lround(xfInHiVel.value_or(0.0)), 0L, 127L));
                    s.xfin_lovel = static_cast<int8>(std::clamp(std::lround(xfInLoVel.value_or(0.0)), 0L, 127L));
                    s.xfout_hivel = static_cast<int8>(std::clamp(std::lround(xfOutHiVel.value_or(127.0)), 0L, 127L));
                    s.xfout_lovel = static_cast<int8>(std::clamp(std::lround(xfOutLoVel.value_or(127.0)), 0L, 127L));
                }
                else
                {
                    s.xfmode_vel = CROSSFADE_NONE;
                }

                auto swDefault = flatSection.GetAs<std::int32_t>(OpCodeKind::SwDefault);
                auto swDown = flatSection.GetAs<std::int32_t>(OpCodeKind::SwDown);
                auto swHiKey = flatSection.GetAs<std::int32_t>(OpCodeKind::SwHiKey);
                auto swHiLast = flatSection.GetAs<std::int32_t>(OpCodeKind::SwHiLast);
                auto swLast = flatSection.GetAs<std::int32_t>(OpCodeKind::SwLast);
                auto swLoKey = flatSection.GetAs<std::int32_t>(OpCodeKind::SwLoKey);
                auto swLoLast = flatSection.GetAs<std::int32_t>(OpCodeKind::SwLoLast);
                auto swPrevious = flatSection.GetAs<std::int32_t>(OpCodeKind::SwPrevious);
                auto swUp = flatSection.GetAs<std::int32_t>(OpCodeKind::SwUp);

                if (swDown.has_value() || swHiLast.has_value() || swLast.has_value() || swLoLast.has_value() || swPrevious.has_value() || swUp.has_value())
                {
                    s.modes |= MODES_KEYSWITCH;
                    s.sw_lokey = static_cast<int8>(std::clamp(swLoKey.value_or(0), 0, 127));
                    s.sw_hikey = static_cast<int8>(std::clamp(swHiKey.value_or(127), 0, 127));

                    if (swDown.has_value())
                    {
                        s.sw_down = static_cast<int8>(std::clamp(*swDown, 0, 127));

                        if (!(s.sw_lokey <= s.sw_down && s.sw_down <= s.sw_hikey))
                        {
                            // sw_down is invalid; disable it
                            s.sw_down = -1;

                            if (i == 0)
                            {
                                auto loc = flatSection.GetLocationForOpCode(OpCodeKind::SwDown);

                                ctl->cmsg(
                                    CMSG_WARNING,
                                    VERB_VERBOSE,
                                    "%s(%u): 'sw_down' was specified but it is outside the range specified by 'sw_lokey' and 'sw_hikey'",
                                    std::string(m_Parser.GetPreprocessor().GetFileNameFromID(loc.FileID)).c_str(),
                                    static_cast<std::uint32_t>(loc.Line)
                                );
                            }
                        }
                    }
                    else
                    {
                        s.sw_down = -1;
                    }

                    if (swUp.has_value())
                    {
                        s.sw_up = static_cast<int8>(std::clamp(*swUp, 0, 127));

                        if (!(s.sw_lokey <= s.sw_up && s.sw_up <= s.sw_hikey))
                        {
                            // sw_up is invalid; disable it
                            s.sw_up = -1;

                            if (i == 0)
                            {
                                auto loc = flatSection.GetLocationForOpCode(OpCodeKind::SwUp);

                                ctl->cmsg(
                                    CMSG_WARNING,
                                    VERB_VERBOSE,
                                    "%s(%u): 'sw_up' was specified but it is outside the range specified by 'sw_lokey' and 'sw_hikey'",
                                    std::string(m_Parser.GetPreprocessor().GetFileNameFromID(loc.FileID)).c_str(),
                                    static_cast<std::uint32_t>(loc.Line)
                                );
                            }
                        }
                    }
                    else
                    {
                        s.sw_up = -1;
                    }

                    s.sw_default = (swDefault.has_value() ? static_cast<int8>(std::clamp(*swDefault, 0, 127)) : -1);

                    if (swLoLast.has_value() || swHiLast.has_value())
                    {
                        s.sw_lolast = static_cast<int8>(std::clamp(swLoLast.value_or(0), 0, 127));
                        s.sw_hilast = static_cast<int8>(std::clamp(swHiLast.value_or(127), 0, 127));
                    }
                    else
                    {
                        s.sw_lolast = (swLast.has_value() ? static_cast<int8>(std::clamp(*swLast, 0, 127)) : -1);
                        s.sw_hilast = s.sw_lolast;
                    }

                    s.sw_previous = (swPrevious.has_value() ? static_cast<int8>(std::clamp(*swPrevious, 0, 127)) : -1);
                }
            }

            return pSampleInstrument;
        }
        else
        {
            throw ParserException(
                m_Parser.GetPreprocessor().GetFileNameFromID(flatSection.HeaderLocation.FileID),
                flatSection.HeaderLocation.Line,
                "no sample specified for region"
            );
        }
    }

    std::unique_ptr<Instrument, InstrumentDeleter> BuildSingleSampleInstrument(std::string sampleUrl)
    {
        std::unique_ptr<Instrument, InstrumentDeleter> pInstrument(::extract_sample_file(sampleUrl.data()));

        if (!pInstrument)
        {
            throw std::runtime_error("unable to load sample '"s + sampleUrl + "'");
        }

        return pInstrument;
    }

    std::vector<Section> FlattenSections(const std::vector<Section>& sections)
    {
        std::vector<Section> flatSections;
        std::vector<OpCodeAndValue> controlOpCodes;
        std::vector<OpCodeAndValue> globalOpCodes;
        std::vector<OpCodeAndValue> masterOpCodes;
        std::vector<OpCodeAndValue> groupOpCodes;

        for (auto&& i : sections)
        {
            switch (i.Header)
            {
            case HeaderKind::Control:
                controlOpCodes = i.OpCodes;
                break;

            case HeaderKind::Global:
                globalOpCodes = i.OpCodes;
                masterOpCodes.clear();
                groupOpCodes.clear();
                break;

            case HeaderKind::Master:
                masterOpCodes = i.OpCodes;
                groupOpCodes.clear();
                break;

            case HeaderKind::Group:
                groupOpCodes = i.OpCodes;
                break;

            case HeaderKind::Region:
                auto& newSection = flatSections.emplace_back();
                newSection.Header = i.Header;
                newSection.HeaderLocation = i.HeaderLocation;
                auto& opCodes = newSection.OpCodes;
                opCodes.clear();
                opCodes.reserve(controlOpCodes.size() + globalOpCodes.size() + masterOpCodes.size() + groupOpCodes.size() + i.OpCodes.size());
                opCodes.insert(opCodes.end(), controlOpCodes.begin(), controlOpCodes.end());
                opCodes.insert(opCodes.end(), globalOpCodes.begin(), globalOpCodes.end());
                opCodes.insert(opCodes.end(), masterOpCodes.begin(), masterOpCodes.end());
                opCodes.insert(opCodes.end(), groupOpCodes.begin(), groupOpCodes.end());
                opCodes.insert(opCodes.end(), i.OpCodes.begin(), i.OpCodes.end());
                break;
            }
        }

        return flatSections;
    }

    std::int32_t CalcRate(std::int32_t diff, double sec) const
    {
        const std::int32_t envMax = 0x3FFFFFFF;
        const std::int32_t envMin = 1;

        if (std::abs(sec) < 1.0e-6)
        {
            return envMax + 1;
        }

        diff = std::max(diff, 1) << 14;

        double rate = static_cast<double>(diff) / ::play_mode->rate * ::control_ratio / sec;

        if (::fast_decay)
        {
            rate *= 2.0;
        }

        return std::clamp(static_cast<std::int32_t>(std::lround(rate)), envMin, envMax);
    }

    std::int32_t ToOffset(std::int32_t n) const
    {
        return n << 14;
    }

    Parser& m_Parser;
    std::string m_FileDir;
    std::string m_Name;
};

struct InstrumentCacheEntry
{
    InstrumentCacheEntry(std::unique_ptr<Instrument, InstrumentDeleter> pInstrument) : pInstrument(std::move(pInstrument))
    {
    }

    std::unique_ptr<Instrument, InstrumentDeleter> pInstrument;
    std::vector<Instrument*> RefInstruments;
};

class InstrumentCache
{
public:
    Instrument* LoadSFZ(std::string filePath)
    {
        auto it = m_Instruments.find(filePath);

        if (it == m_Instruments.end())
        {
            try
            {
                TimSFZ::Preprocessor pp(filePath);
                pp.Preprocess();
                TimSFZ::Parser parser(pp);
                parser.Parse();
                TimSFZ::InstrumentBuilder builder(parser, filePath);
                it = m_Instruments.emplace(filePath, builder.BuildInstrument()).first;
            }
            catch (const std::exception& e)
            {
                char str[] = "%s";
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, str, e.what());
                return nullptr;
            }
        }

        std::unique_ptr<Instrument, InstrumentDeleter> pInstRef(reinterpret_cast<Instrument*>(safe_calloc(sizeof(Instrument), 1)));
        it->second.RefInstruments.push_back(pInstRef.get());
        pInstRef->type = it->second.pInstrument->type;
        pInstRef->instname = safe_strdup(it->second.pInstrument->instname);
        pInstRef->samples = it->second.pInstrument->samples;
        pInstRef->sample = reinterpret_cast<Sample*>(safe_calloc(sizeof(Sample), it->second.pInstrument->samples));
        std::copy_n(it->second.pInstrument->sample, it->second.pInstrument->samples, pInstRef->sample);
        std::for_each(pInstRef->sample, pInstRef->sample + pInstRef->samples, [] (auto&& x) { x.data_alloced = false; });

        return pInstRef.release();
    }

    void FreeInstrument(Instrument* pInstrument)
    {
        safe_free(pInstrument->instname);
        pInstrument->instname = nullptr;

        auto it = std::find_if(
            m_Instruments.begin(),
            m_Instruments.end(),
            [pInstrument] (auto&& x)
            {
                auto it = std::find(x.second.RefInstruments.begin(), x.second.RefInstruments.end(), pInstrument);
                return it != x.second.RefInstruments.end();
            }
        );

        if (it != m_Instruments.end())
        {
            it->second.RefInstruments.erase(std::find(it->second.RefInstruments.begin(), it->second.RefInstruments.end(), pInstrument));

            if (it->second.RefInstruments.empty())
            {
                m_Instruments.erase(it);
            }
        }
    }

    void FreeAll()
    {
        m_Instruments.clear();
    }

private:
    std::unordered_map<std::string, InstrumentCacheEntry> m_Instruments;
};

InstrumentCache GlobalInstrumentCache;

} // namespace TimSFZ

extern "C"
{

// THis is no-op for now, but may be used in the future.
void init_sfz(void)
{
}

void free_sfz(void)
{
    TimSFZ::GlobalInstrumentCache.FreeAll();
}

Instrument *extract_sfz_file(char *sample_file)
{
    return TimSFZ::GlobalInstrumentCache.LoadSFZ(sample_file);
}

void free_sfz_file(Instrument *ip)
{
    TimSFZ::GlobalInstrumentCache.FreeInstrument(ip);
}

} // extern "C"
