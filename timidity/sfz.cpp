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
#include "tables.h"

#include "sfz.h"

// smplfile.c
Instrument *extract_sample_file(char *sample_file);
}

#include <cassert>
#include <cstdint>
#include <cstdio>

#include <algorithm>
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
    std::uint32_t Line; // 1-based
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
    ParserException(std::string_view fileName, std::uint32_t line, std::string_view msg)
        : runtime_error(FormatErrorMessage(fileName, line, msg))
    {
    }

private:
    std::string FormatErrorMessage(std::string_view fileName, std::uint32_t line, std::string_view msg)
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
        return CharIf(view, [] (char x) { return 'A' <= x && x <= 'Z' || 'a' <= x && x <= 'z' || x == '_'; });
    }

    bool WordContinueChar(TextBuffer::View& view)
    {
        return CharIf(
            view,
            [] (char x) { return 'A' <= x && x <= 'Z' || 'a' <= x && x <= 'z' || '0' <= x && x <= '9' || x == '_'; }
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

        if (!NonSpaceChar(view))
        {
            return false;
        }

        while (NonSpaceChar(view))
        {
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
            DoSkips(curView);

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
    std::vector<TextBuffer> m_InBuffers;
    std::stack<InputStackItem, std::vector<InputStackItem>> m_InputStack;
    TextBuffer m_OutBuffer;
    std::unordered_map<std::string, TextBuffer::View> m_DefinedMacros;
};

enum class OpCodeKind
{
    HiKey,
    HiVelocity,
    LoKey,
    LoopEnd,
    LoopMode,
    LoopStart,
    LoVelocity,
    PitchKeyCenter,
    Sample
};

enum class LoopModeKind
{
    NoLoop,
    OneShot,
    LoopContinuous,
    LoopSustain
};

struct OpCodeAndValue
{
    FileLocationInfo Location;
    OpCodeKind OpCode;
    std::variant<std::int32_t, double, LoopModeKind, std::string> Value;
};

enum class HeaderKind
{
    Control,
    Global,
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
                        case OpCodeKind::HiKey:
                        case OpCodeKind::LoKey:
                        case OpCodeKind::PitchKeyCenter:
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

                        case OpCodeKind::HiVelocity:
                        case OpCodeKind::LoopEnd:
                        case OpCodeKind::LoopStart:
                        case OpCodeKind::LoVelocity:
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

                        default:
                            opVal.Value = valView.ToString();
                            break;
                        }

                        sec.OpCodes.push_back(std::move(opVal));
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
            {"hikey"sv, OpCodeKind::HiKey},
            {"hivel"sv, OpCodeKind::HiVelocity},
            {"lokey"sv, OpCodeKind::LoKey},
            {"loop_end"sv, OpCodeKind::LoopEnd},
            {"loop_mode"sv, OpCodeKind::LoopMode},
            {"loop_start"sv, OpCodeKind::LoopStart},
            {"lovel"sv, OpCodeKind::LoVelocity},
            {"pitch_keycenter"sv, OpCodeKind::PitchKeyCenter},
            {"sample"sv, OpCodeKind::Sample}
        };

        auto it = OpCodeMap.find(word.ToStringView());

        if (it == OpCodeMap.end())
        {
            throw ParserException(
                m_Preprocessor.GetFileNameFromID(word.GetLocationInfo().FileID),
                word.GetLocationInfo().Line,
                "unknown opcode '"s.append(word.ToStringView()).append("'")
            );
        }

        op = it->second;
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

    Preprocessor& m_Preprocessor;
    std::vector<Section> m_Sections;
};

class InstrumentBuilder
{
public:
    InstrumentBuilder(Parser& parser, std::string_view name) : m_Parser(parser), m_Name(name)
    {
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
            auto pSampleInstrument = BuildSingleSampleInstrument(*sampleName);

            for (auto&& i : flatSection.OpCodes)
            {
                for (std::size_t j = 0; j < pSampleInstrument->samples; j++)
                {
                    auto pSample = &pSampleInstrument->sample[j];

                    switch (i.OpCode)
                    {
                    case OpCodeKind::HiKey:
                        pSample->high_key = static_cast<int8>(std::get<std::int32_t>(i.Value));
                        break;

                    case OpCodeKind::HiVelocity:
                        pSample->high_vel = static_cast<uint8>(std::get<double>(i.Value));
                        break;

                    case OpCodeKind::LoKey:
                        pSample->low_key = static_cast<int8>(std::get<std::int32_t>(i.Value));
                        break;

                    case OpCodeKind::LoopEnd:
                        pSample->loop_end = static_cast<splen_t>(std::get<double>(i.Value)) << FRACTION_BITS;
                        break;

                    case OpCodeKind::LoopMode:
                        pSample->modes &= ~(MODES_LOOPING | MODES_PINGPONG | MODES_REVERSE | MODES_SUSTAIN);

                        switch (std::get<LoopModeKind>(i.Value))
                        {
                            case LoopModeKind::NoLoop:
                                break;

                            case LoopModeKind::OneShot:
                                // ???
                                break;

                            case LoopModeKind::LoopContinuous:
                                pSample->modes |= MODES_LOOPING | MODES_SUSTAIN;
                                break;

                            case LoopModeKind::LoopSustain:
                                pSample->modes |= MODES_LOOPING | MODES_SUSTAIN | MODES_RELEASE;
                                break;
                        }
                        break;

                    case OpCodeKind::LoopStart:
                        pSample->loop_start = static_cast<splen_t>(std::get<double>(i.Value)) << FRACTION_BITS;
                        break;

                    case OpCodeKind::LoVelocity:
                        pSample->low_vel = static_cast<uint8>(std::get<double>(i.Value));
                        break;

                    case OpCodeKind::PitchKeyCenter:
                        pSample->root_key = static_cast<int8>(std::get<std::int32_t>(i.Value));
                        pSample->root_freq = ::freq_table[pSample->root_key];
                        break;

                    case OpCodeKind::Sample:
                        break;
                    }
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
        std::vector<OpCodeAndValue> groupOpCodes;

        for (auto&& i : sections)
        {
            switch (i.Header)
            {
            case HeaderKind::Control:
                controlOpCodes.insert(controlOpCodes.end(), i.OpCodes.begin(), i.OpCodes.end());
                break;

            case HeaderKind::Global:
                globalOpCodes.insert(globalOpCodes.end(), i.OpCodes.begin(), i.OpCodes.end());
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
                opCodes.reserve(controlOpCodes.size() + globalOpCodes.size() + groupOpCodes.size() + i.OpCodes.size());
                opCodes.insert(opCodes.end(), controlOpCodes.begin(), controlOpCodes.end());
                opCodes.insert(opCodes.end(), globalOpCodes.begin(), globalOpCodes.end());
                opCodes.insert(opCodes.end(), groupOpCodes.begin(), groupOpCodes.end());
                opCodes.insert(opCodes.end(), i.OpCodes.begin(), i.OpCodes.end());
                break;
            }
        }

        return flatSections;
    }

    Parser& m_Parser;
    std::string m_Name;
};

struct InstrumentCacheEntry
{
    InstrumentCacheEntry(std::string_view filePath, std::unique_ptr<Instrument, InstrumentDeleter> pInstrument)
        : FilePath(filePath), pInstrument(std::move(pInstrument))
    {
    }

    std::string FilePath;
    std::unique_ptr<Instrument, InstrumentDeleter> pInstrument;
    std::vector<Instrument*> RefInstruments;
};

class InstrumentCache
{
public:
    Instrument* LoadSFZ(std::string filePath)
    {
        auto it = std::find_if(
            m_Instruments.begin(),
            m_Instruments.end(),
            [&filePath] (auto&& x)
            {
                return x.FilePath == filePath;
            }
        );

        if (it == m_Instruments.end())
        {
            try
            {
                TimSFZ::Preprocessor pp(filePath);
                pp.Preprocess();
                TimSFZ::Parser parser(pp);
                parser.Parse();
                TimSFZ::InstrumentBuilder builder(parser, filePath);
                m_Instruments.emplace_back(filePath, builder.BuildInstrument());
            }
            catch (const std::exception& e)
            {
                char str[] = "%s";
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, str, e.what());
                return nullptr;
            }

            it = std::prev(m_Instruments.end());
        }

        std::unique_ptr<Instrument, InstrumentDeleter> pInstRef(reinterpret_cast<Instrument*>(safe_calloc(sizeof(Instrument), 1)));
        it->RefInstruments.push_back(pInstRef.get());
        pInstRef->type = it->pInstrument->type;
        pInstRef->instname = safe_strdup(it->pInstrument->instname);
        pInstRef->samples = it->pInstrument->samples;
        pInstRef->sample = reinterpret_cast<Sample*>(safe_calloc(sizeof(Sample), it->pInstrument->samples));
        std::copy_n(it->pInstrument->sample, it->pInstrument->samples, pInstRef->sample);
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
                auto it = std::find(x.RefInstruments.begin(), x.RefInstruments.end(), pInstrument);
                return it != x.RefInstruments.end();
            }
        );

        if (it != m_Instruments.end())
        {
            it->RefInstruments.erase(std::find(it->RefInstruments.begin(), it->RefInstruments.end(), pInstrument));

            if (it->RefInstruments.empty())
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
    std::vector<InstrumentCacheEntry> m_Instruments;
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