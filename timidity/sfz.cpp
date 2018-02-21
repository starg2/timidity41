// SFZ Support Routines for TiMidity++
// Copyright (c) 2018 Starg <https://osdn.net/projects/timidity41>

extern "C"
{
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "timidity.h"
#include "common.h"

#include "sfz.h"
}

#include <cassert>
#include <cstdint>
#include <cstdio>

#include <algorithm>
#include <iterator>
#include <memory>
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

using TFFilePtr = std::unique_ptr<timidity_file, TFFileCloser>;

std::string ReadEntireFile(char* pURL)
{
    TFFilePtr pFile(::open_file(pURL, 1, OF_NORMAL));

    if (!pFile)
    {
        throw std::runtime_error("unable to open '"s + pURL + "'");
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
            assert(m_Length > 0);
            m_Offset++;
            m_Length--;
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
            while (!EndOfInput(view) && !EndOfLine(view))
            {
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
};

class Preprocessor : private BasicParser
{
public:
    explicit Preprocessor(char* pURL)
        : m_FileNames{pURL}, m_InBuffers{TextBuffer(ReadEntireFile(pURL), FileLocationInfo{0, 1})}
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

            if (m_InputStack.top().StartsAtMiddle)
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
                    if (!AnyCharSequence(curView, pathView))
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
            else
            {
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
    LoKey,
    LoopEnd,
    LoopMode,
    LoopStart,
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
    FileLocationInfo HeaderLocation;
    HeaderKind Header;
    std::vector<OpCodeAndValue> OpCodes;
    std::vector<Section> Children;
};

class Parser : private BasicParser
{
public:
    explicit Parser(Preprocessor& pp) : m_Preprocessor(pp)
    {
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

                        case OpCodeKind::LoopEnd:
                        case OpCodeKind::LoopStart:
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
                    switch (sec.Header)
                    {
                    case HeaderKind::Control:
                    case HeaderKind::Global:
                    case HeaderKind::Group:
                        m_Sections.push_back(std::move(sec));
                        break;

                    case HeaderKind::Region:
                        if (!m_Sections.empty() && m_Sections.back().Header == HeaderKind::Group)
                        {
                            m_Sections.back().Children.push_back(std::move(sec));
                        }
                        else
                        {
                            m_Sections.push_back(std::move(sec));
                        }
                        break;

                    default:
                        break;
                    }
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
            {"lokey"sv, OpCodeKind::LoKey},
            {"loop_end"sv, OpCodeKind::LoopEnd},
            {"loop_mode"sv, OpCodeKind::LoopMode},
            {"loop_start"sv, OpCodeKind::LoopStart},
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

} // namespace TimSFZ
