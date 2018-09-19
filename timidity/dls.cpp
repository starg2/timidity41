// DLS Support Routines for TiMidity++
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

#include "tables.h"

#include "dls.h"
}

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace TimDLS
{

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

struct TimDeleter
{
    void operator()(void* p) const
    {
        safe_free(p);
    }
};

class DLSParserException : public std::exception
{
public:
    DLSParserException(std::string fileName, std::string_view msg) : m_Message(fileName.append(": error: ").append(msg))
    {
    }

    virtual const char* what() const noexcept override
    {
        return m_Message.c_str();
    }

private:
    std::string m_Message;
};

struct DLSWaveSampleLoop
{
    enum class LoopType : std::uint32_t
    {
        Forward
    };

    LoopType Type;
    std::uint32_t LoopStart;
    std::uint32_t LoopLength;
};

struct DLSWaveSampleInfo
{
    std::uint8_t UnityNote;
    std::int16_t FineTune;
    std::int32_t Attenuation;
    bool NoTruncation;
    bool NoCompression;
    std::vector<DLSWaveSampleLoop> SampleLoops;
};

struct DLSConnectionBlock
{
    enum class SourceKind : std::uint16_t
    {
        None,
        LFO,
        KeyOnVelocity,
        KeyNumber,
        EG1,
        EG2,
        PitchWheel,

        CC1 = 0x81,
        CC7 = 0x87,
        CC10 = 0x8A,
        CC11 = 0x8B
    };

    SourceKind Source;
    SourceKind Control;

    enum class DestinationKind : std::uint16_t
    {
        None,
        Attenuation,
        Pitch = 3,
        Pan,

        LFOFrequency = 0x104,
        LFOStartDelay,

        EG1AttackTime = 0x206,
        EG1DecayTime,
        EG1ReleaseTime = 0x209,
        EG1SustainLevel,

        EG2AttackTime = 0x30A,
        EG2DecayTime,
        EG2ReleaseTime = 0x30D,
        EG2SustainLevel
    };

    DestinationKind Destination;

    enum class TransformKind : std::uint16_t
    {
        None,
        Concave
    };

    TransformKind Transform;

    std::int32_t Scale;
};

struct DLSArticulator
{
    std::vector<DLSConnectionBlock> ConnectionBlocks;
};

struct DLSWaveLink
{
    bool PhaseMaster;
    std::uint16_t PhaseGroup;
    bool Left;
    bool Right;
    std::uint32_t TableIndex;
};

struct DLSRegion
{
    std::uint16_t LoKey;
    std::uint16_t HiKey;
    std::uint16_t LoVelocity;
    std::uint16_t HiVelocity;
    bool SelfNonExclusive;
    std::uint16_t KeyGroup;

    std::optional<DLSWaveSampleInfo> SampleInfo;
    DLSWaveLink WaveLink;
    std::vector<DLSArticulator> Articulators;
};

struct DLSInstrument
{
    std::string Name;
    std::uint8_t ProgramNumber;
    std::uint16_t Bank;
    std::vector<DLSRegion> Regions;
    std::vector<DLSArticulator> Articulators;
};

struct DLSWaveInfo
{
    std::uint16_t FormatTag;
    std::uint16_t Channels;
    std::uint32_t SamplesPerSec;
    std::uint32_t AvgBytesPerSec;
    std::uint16_t BlockAlign;
    std::uint16_t BitsPerSample;

    std::optional<DLSWaveSampleInfo> SampleInfo;

    std::unique_ptr<char, TimDeleter> pData;
    std::uint32_t DataLength;
};

struct DLSCollection
{
    std::vector<DLSInstrument> Instruments;
    std::vector<std::uint32_t> PoolTable;
    std::uint32_t WavePoolOffset; // offset of wave pool from the beginning of the DLS file
};

std::int32_t CalcRate(std::int32_t diff, double sec)
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

std::int32_t ToOffset(std::int32_t n)
{
    return n << 14;
}

double TimeCentToSecond(std::int32_t tc)
{
    return tc == static_cast<std::int32_t>(0x80000000) ? 0.0 : std::pow(2.0, tc / (1200.0 * 65536));
}

class DLSParser
{
public:
    explicit DLSParser(std::string_view fileName) : m_FileName(fileName)
    {
    }

    void Parse()
    {
        std::unique_ptr<timidity_file, TFFileCloser> pFile(::open_file(m_FileName.data(), 1, OF_NORMAL));

        if (!pFile)
        {
            throw DLSParserException(m_FileName, "unable to open file");
        }

        ParseRIFF(pFile.get());

#if 0
        for (const DLSInstrument& i : m_DLS.Instruments)
        {
            if (i.Bank == 128)
            {
                for (const DLSRegion& r : i.Regions)
                {
                    ctl->cmsg(
                        CMSG_INFO,
                        VERB_NORMAL,
                        "%d %%dls \"gm.dls\" 128 %d %d # %s",
                        r.SampleInfo->UnityNote,
                        i.ProgramNumber,
                        r.SampleInfo->UnityNote,
                        i.Name.c_str()
                    );
                }
            }
            else
            {
                ctl->cmsg(
                    CMSG_INFO,
                    VERB_NORMAL,
                    "%d %%dls \"gm.dls\" %d %d # %s",
                    i.ProgramNumber,
                    i.Bank,
                    i.ProgramNumber,
                    i.Name.c_str()
                );
            }
        }
#endif
    }

    std::unique_ptr<Instrument, InstrumentDeleter> BuildInstrument(std::uint8_t bank, std::int8_t programNumber, std::int8_t note)
    {
        // find DLSInstrument
        auto itInst = std::find_if(
            m_DLS.Instruments.begin(),
            m_DLS.Instruments.end(),
            [bank, programNumber] (const DLSInstrument& inst)
            {
                return inst.Bank == bank && inst.ProgramNumber == programNumber;
            }
        );

        if (itInst == m_DLS.Instruments.end())
        {
            ctl->cmsg(
                CMSG_ERROR,
                VERB_NORMAL,
                "%s: no instrument found with [bank = %d, pc = %d]",
                m_FileName.c_str(),
                bank,
                programNumber
            );

            return nullptr;
        }

        std::unique_ptr<Instrument, InstrumentDeleter> pInstrument(reinterpret_cast<Instrument*>(safe_calloc(sizeof(Instrument), 1)));
        pInstrument->type = INST_DLS;
        pInstrument->instname = safe_strdup(itInst->Name.c_str());

        ctl->cmsg(
            CMSG_INFO,
            VERB_NOISY,
            "%s: loading instrument '%s' [bank = %d, pc = %d]",
            m_FileName.c_str(),
            pInstrument->instname,
            bank,
            programNumber
        );

        auto noteMatcher = [note] (const DLSRegion& r)
        {
            return note < 0 || (r.LoKey <= static_cast<std::uint16_t>(note) && static_cast<std::uint16_t>(note) <= r.HiKey);
        };

        std::size_t regionCount = std::count_if(itInst->Regions.begin(), itInst->Regions.end(), noteMatcher);
        pInstrument->samples = static_cast<int>(regionCount);
        pInstrument->sample = reinterpret_cast<Sample*>(safe_calloc(sizeof(Sample), regionCount));

        std::size_t filledSamples = 0;
        for (const DLSRegion& r : itInst->Regions)
        {
            if (noteMatcher(r))
            {
                Sample* pSample = &pInstrument->sample[filledSamples];
                auto waveInfo = ParseWAVEList(m_DLS.WavePoolOffset + m_DLS.PoolTable.at(r.WaveLink.TableIndex));

                pSample->data = reinterpret_cast<sample_t*>(waveInfo.pData.release());
                pSample->data_alloced = 1;
                pSample->data_length = static_cast<splen_t>(waveInfo.DataLength + 1) << FRACTION_BITS;
                pSample->data_type = SAMPLE_TYPE_INT16;
                pSample->sample_rate = static_cast<std::int32_t>(waveInfo.SamplesPerSec);

                {
                    const DLSWaveSampleInfo* pSampleInfo = nullptr;

                    if (r.SampleInfo.has_value())
                    {
                        pSampleInfo = &*r.SampleInfo;
                    }
                    else if (waveInfo.SampleInfo.has_value())
                    {
                        pSampleInfo = &*waveInfo.SampleInfo;
                    }

                    DLSWaveSampleLoop loop{DLSWaveSampleLoop::LoopType::Forward, 0, waveInfo.DataLength};

                    if (pSampleInfo)
                    {
                        if (!pSampleInfo->SampleLoops.empty())
                        {
                            pSample->modes |= MODES_LOOPING | MODES_SUSTAIN;
                            loop = pSampleInfo->SampleLoops[0];
                        }

                        pSample->root_key = std::clamp<std::int8_t>(pSampleInfo->UnityNote, 0, 127);
                        pSample->tune = std::pow(2.0, pSampleInfo->FineTune / 1200.0);
                        pSample->volume = (pSampleInfo->Attenuation == 0x80000000 ? 0.0 : std::pow(10.0, pSampleInfo->Attenuation / 200.0 / 65536.0));
                    }
                    else
                    {
                        pSample->root_key = 60;
                        pSample->tune = 1.0;
                        pSample->volume = 1.0;
                    }

                    pSample->loop_start = std::clamp<splen_t>(loop.LoopStart, 0, waveInfo.DataLength - 1) << FRACTION_BITS;
                    pSample->loop_end = std::clamp(
                        pSample->loop_start + (static_cast<splen_t>(loop.LoopLength) << FRACTION_BITS),
                        pSample->loop_start + (1 << FRACTION_BITS),
                        pSample->data_length
                    );

                    pSample->root_freq = ::freq_table[pSample->root_key];
                }

                pSample->low_key = static_cast<std::int8_t>(r.LoKey);
                pSample->high_key = static_cast<std::int8_t>(r.HiKey);
                pSample->low_vel = static_cast<std::uint8_t>(r.LoVelocity);
                pSample->high_vel = static_cast<std::uint8_t>(r.HiVelocity);

                pSample->def_pan = 64;
                pSample->cfg_amp = 1.0;
                pSample->modes |= (waveInfo.BitsPerSample == 16 ? MODES_16BIT : 0);

                pSample->cutoff_freq = 20000;
                pSample->cutoff_low_limit = -1;
                pSample->envelope_velf_bpo = 64;
                pSample->modenv_velf_bpo = 64;
                pSample->key_to_fc_bpo = 60;
                pSample->scale_freq = 60;
                pSample->scale_factor = 1024;

                pSample->sample_type = (r.WaveLink.Left ? SF_SAMPLETYPE_LEFT : (r.WaveLink.Right ? SF_SAMPLETYPE_RIGHT : SF_SAMPLETYPE_MONO));
                pSample->sf_sample_link = -1;

                pSample->lpf_type = -1;
                pSample->hpf[0] = -1;
                pSample->hpf[1] = 10;

                pSample->tremolo_freq = 5000;
                pSample->vibrato_freq = 5000;

                double attackTime = 0.0;
                double holdTime = 0.0;
                double decayTime = 0.0;
                std::int32_t sustainLevel = 65533;
                double releaseTime = 0.0;

                // DLS Level 1 specifies that melodic instruments use global articulators and that drum instruments use region articulators.
                if (bank != 128 && !r.Articulators.empty())
                {
                    ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "%s: warning: region articulators ignored for melodic instrument", m_FileName.c_str());
                }

                if (bank == 128 && !itInst->Articulators.empty())
                {
                    ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "%s: warning: global articulators ignored for drum instrument", m_FileName.c_str());
                }

                for (const DLSArticulator& a : (bank == 128 ? r.Articulators : itInst->Articulators))
                {
                    for (const DLSConnectionBlock& b : a.ConnectionBlocks)
                    {
                        switch (b.Destination)
                        {
                        case DLSConnectionBlock::DestinationKind::EG1AttackTime:
                            if (b.Source == DLSConnectionBlock::SourceKind::None && b.Control == DLSConnectionBlock::SourceKind::None && b.Transform == DLSConnectionBlock::TransformKind::None)
                            {
                                attackTime = TimeCentToSecond(b.Scale);
                                continue;
                            }
                            else if (b.Source == DLSConnectionBlock::SourceKind::KeyOnVelocity && b.Control == DLSConnectionBlock::SourceKind::None && b.Transform == DLSConnectionBlock::TransformKind::None)
                            {
                                pSample->envelope_velf[0] = static_cast<int16>(b.Scale / 65536);
                                continue;
                            }
                            break;

                        case DLSConnectionBlock::DestinationKind::EG1DecayTime:
                            if (b.Source == DLSConnectionBlock::SourceKind::None && b.Control == DLSConnectionBlock::SourceKind::None && b.Transform == DLSConnectionBlock::TransformKind::None)
                            {
                                decayTime = TimeCentToSecond(b.Scale);
                                continue;
                            }
                            else if (b.Source == DLSConnectionBlock::SourceKind::KeyNumber && b.Control == DLSConnectionBlock::SourceKind::None && b.Transform == DLSConnectionBlock::TransformKind::None)
                            {
                                // this is probably incorrect
                                pSample->envelope_keyf[1] = static_cast<int16>(b.Scale / 65536);
                                continue;
                            }
                            break;

                        case DLSConnectionBlock::DestinationKind::EG1SustainLevel:
                            if (b.Source == DLSConnectionBlock::SourceKind::None && b.Control == DLSConnectionBlock::SourceKind::None && b.Transform == DLSConnectionBlock::TransformKind::None)
                            {
                                sustainLevel = std::lround(65533.0 * std::clamp(b.Scale, 0, 1000) / 1000.0);
                                continue;
                            }
                            break;

                        case DLSConnectionBlock::DestinationKind::EG1ReleaseTime:
                            if (b.Source == DLSConnectionBlock::SourceKind::None && b.Control == DLSConnectionBlock::SourceKind::None && b.Transform == DLSConnectionBlock::TransformKind::None)
                            {
                                releaseTime = TimeCentToSecond(b.Scale);
                                continue;
                            }
                            break;

                        case DLSConnectionBlock::DestinationKind::LFOFrequency:
                            if (b.Source == DLSConnectionBlock::SourceKind::None && b.Control == DLSConnectionBlock::SourceKind::None && b.Transform == DLSConnectionBlock::TransformKind::None)
                            {
                                // mHz ?
                                int16 freq = static_cast<int16>(std::lround(std::pow(2.0, (b.Scale / 65536.0 - 6900.0) / 1200.0) * 440.0 * 1000.0));
                                pSample->tremolo_freq = freq;
                                pSample->vibrato_freq = freq;
                                continue;
                            }
                            break;

                        case DLSConnectionBlock::DestinationKind::LFOStartDelay:
                            if (b.Source == DLSConnectionBlock::SourceKind::None && b.Control == DLSConnectionBlock::SourceKind::None && b.Transform == DLSConnectionBlock::TransformKind::None)
                            {
                                int16 delay = static_cast<int16>(std::lround(TimeCentToSecond(b.Scale) * ::play_mode->rate * 1000.0));
                                pSample->tremolo_delay = delay;
                                pSample->vibrato_delay = delay;
                                continue;
                            }
                            break;

                        case DLSConnectionBlock::DestinationKind::Attenuation:
                            if (b.Source == DLSConnectionBlock::SourceKind::LFO && b.Control == DLSConnectionBlock::SourceKind::None && b.Transform == DLSConnectionBlock::TransformKind::None)
                            {
                                double gain = b.Scale / 655360.0;   // dB
                                double rate = std::pow(10.0, gain * 0.1);
                                pSample->tremolo_to_amp = static_cast<int16>(std::lround((rate - 1.0) * 10000.0));
                                continue;
                            }
                            break;

                        case DLSConnectionBlock::DestinationKind::Pitch:
                            if (b.Source == DLSConnectionBlock::SourceKind::LFO && b.Control == DLSConnectionBlock::SourceKind::None && b.Transform == DLSConnectionBlock::TransformKind::None)
                            {
                                pSample->vibrato_to_pitch = static_cast<int16>(b.Scale / 65536);
                                continue;
                            }
                            break;

                        default:
                            break;
                        }

                        ctl->cmsg(
                            CMSG_WARNING,
                            VERB_NOISY,
                            "%s: unsupported connection block [source = %d, control = %d, scale = %d, destination = %d, transform = %d]",
                            m_FileName.c_str(),
                            b.Source,
                            b.Control,
                            b.Scale,
                            b.Destination,
                            b.Transform
                        );
                    }
                }

                pSample->envelope_offset[0] = ToOffset(65535);
                pSample->envelope_rate[0] = CalcRate(65535, attackTime);
                pSample->envelope_offset[1] = ToOffset(65534);
                pSample->envelope_rate[1] = CalcRate(1, holdTime);

                pSample->envelope_offset[2] = ToOffset(sustainLevel);
                pSample->envelope_rate[2] = CalcRate(65534 - sustainLevel, std::clamp(decayTime, 0.0, 100.0));

                pSample->envelope_offset[3] = 0;
                pSample->envelope_rate[3] = CalcRate(sustainLevel, releaseTime);
                pSample->envelope_offset[4] = pSample->envelope_offset[3];
                pSample->envelope_rate[4] = pSample->envelope_rate[3];
                pSample->envelope_offset[5] = pSample->envelope_offset[3];
                pSample->envelope_rate[5] = pSample->envelope_rate[3];

                filledSamples++;

                if (filledSamples >= regionCount)
                {
                    break;
                }
            }
        }

        return pInstrument;
    }

private:
    enum class ChunkLocation
    {
        DLS,
        INS,
        RGN,
        WAVE
    };

    void PrintUnknownChunk(std::uint32_t cc)
    {
        ctl->cmsg(
            CMSG_WARNING,
            VERB_DEBUG,
            "%s: warning: skipping unknown chunk '%c%c%c%c'",
            m_FileName.c_str(),
            cc & 0xFF,
            (cc >> 8) & 0xFF,
            (cc >> 16) & 0xFF,
            cc >> 24
        );
    }

    // array size is 5 because string literals include a terminating NULL character
    static constexpr std::uint32_t MakeFourCC(const char (&cc)[5])
    {
        return (cc[3] << 24) | (cc[2] << 16) | (cc[1] << 8) | cc[0];
    }

    std::uint32_t ReadFourCC(timidity_file* pFile)
    {
        char cc[5] = {};

        if (::tf_read(cc, 1, 4, pFile) != 4)
        {
            throw DLSParserException(m_FileName, "unexpected end of file");
        }

        return MakeFourCC(cc);
    }

    std::uint16_t ReadUInt16(timidity_file* pFile)
    {
        std::uint16_t n;

        if (::tf_read(&n, sizeof(n), 1, pFile) != 1)
        {
            throw DLSParserException(m_FileName, "unexpected end of file");
        }

#ifdef LITTLE_ENDIAN
        return n;
#else
        return (n << 8) | (n >> 8);
#endif
    }

    std::uint32_t ReadUInt32(timidity_file* pFile)
    {
        std::uint32_t n;

        if (::tf_read(&n, sizeof(n), 1, pFile) != 1)
        {
            throw DLSParserException(m_FileName, "unexpected end of file");
        }

#ifdef LITTLE_ENDIAN
        return n;
#else
        return (n << 24) | ((n & 0xFF00) << 8) | ((n & 0xFF0000) >> 8) | (n >> 24);
#endif
    }

    std::string ReadString(timidity_file* pFile, std::uint32_t length)
    {
        std::string str(length, '\0');

        if (::tf_read(str.data(), 1, length, pFile) != length)
        {
            throw DLSParserException(m_FileName, "unexpected end of file");
        }

        return str;
    }

    std::uint32_t Align2(std::uint32_t n)
    {
        return n & 1 ? n + 1 : n;
    }

    void DoSkip(timidity_file* pFile, std::uint32_t n)
    {
        if (n)
        {
            if (::tf_seek(pFile, n, SEEK_CUR) == -1)
            {
                throw DLSParserException(m_FileName, "unexpected end of file");
            }
        }
    }

    std::uint32_t SkipChunk(timidity_file* pFile)
    {
        std::uint32_t totalChunkSize = Align2(ReadUInt32(pFile));
        DoSkip(pFile, totalChunkSize);
        return totalChunkSize + 4;
    }

    std::uint32_t ParseINFOItem(timidity_file* pFile, ChunkLocation loc)
    {
        std::uint32_t cc = ReadFourCC(pFile);
        std::uint32_t chunkSize = ReadUInt32(pFile);

        switch (cc)
        {
        case MakeFourCC("INAM"):
            if (loc == ChunkLocation::INS)
            {
                assert(!m_DLS.Instruments.empty());
                m_DLS.Instruments.back().Name = ReadString(pFile, chunkSize);
                ctl->cmsg(CMSG_INFO, VERB_DEBUG, "%s: INAM: %s", m_FileName.c_str(), m_DLS.Instruments.back().Name.c_str());
                break;
            }
            [[fallthrough]];

        default:
            ctl->cmsg(
                CMSG_INFO,
                VERB_DEBUG,
                "%s: %c%c%c%c: %s",
                m_FileName.c_str(),
                cc & 0xFF,
                (cc >> 8) & 0xFF,
                (cc >> 16) & 0xFF,
                cc >> 24,
                ReadString(pFile, chunkSize).c_str()
            );
            break;
        }

        if (chunkSize & 1)
        {
            DoSkip(pFile, 1);
        }

        return Align2(chunkSize) + 8;
    }

    std::uint32_t ParseVERS(timidity_file* pFile)
    {
        std::uint32_t chunkSize = ReadUInt32(pFile);

        if (chunkSize == 8)
        {
            std::uint32_t versionMS = ReadUInt32(pFile);
            std::uint32_t versionLS = ReadUInt32(pFile);

            ctl->cmsg(
                CMSG_INFO,
                VERB_DEBUG,
                "%s: DLS version: %d.%d.%d.%d",
                m_FileName.c_str(),
                versionMS >> 16,
                versionMS & 0xFFFF,
                versionLS >> 16,
                versionLS & 0xFFFF
            );
        }
        else
        {
            ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'vers' chunk has invalid size", m_FileName.c_str());
            DoSkip(pFile, Align2(chunkSize));
        }

        return Align2(chunkSize) + 4;
    }

    std::uint32_t ParseDLID(timidity_file* pFile)
    {
        return SkipChunk(pFile);
    }

    std::uint32_t ParseCOLH(timidity_file* pFile)
    {
        std::uint32_t chunkSize = ReadUInt32(pFile);

        if (chunkSize == 4)
        {
            std::uint32_t count = ReadUInt32(pFile);

            ctl->cmsg(
                CMSG_INFO,
                VERB_DEBUG,
                "%s: instrument count: %d",
                m_FileName.c_str(),
                count
            );
        }
        else
        {
            ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'colh' chunk has invalid size", m_FileName.c_str());
            DoSkip(pFile, Align2(chunkSize));
        }

        return Align2(chunkSize) + 4;
    }

    std::uint32_t ParseINSH(timidity_file* pFile)
    {
        std::uint32_t chunkSize = ReadUInt32(pFile);

        if (chunkSize == 12)
        {
            /* std::uint32_t regionCount = */ ReadUInt32(pFile);

            assert(!m_DLS.Instruments.empty());
            auto& instrument = m_DLS.Instruments.back();
            std::uint32_t bank = ReadUInt32(pFile);
            instrument.Bank = static_cast<std::uint16_t>(bank & 0x80000000 ? 128 : bank >> 8);  // FIXME!!

            std::uint32_t programNumber = ReadUInt32(pFile);
            instrument.ProgramNumber = static_cast<std::uint8_t>(programNumber);
        }
        else
        {
            ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'insh' chunk has invalid size", m_FileName.c_str());
            DoSkip(pFile, Align2(chunkSize));
        }

        return Align2(chunkSize) + 4;
    }

    std::uint32_t ParseLARTItem(timidity_file* pFile, ChunkLocation loc)
    {
        std::uint32_t cc = ReadFourCC(pFile);
        std::uint32_t readSize = 4;

        switch (cc)
        {
        case MakeFourCC("art1"):
            {
                std::uint32_t chunkSize = ReadUInt32(pFile);

                if (chunkSize >= 8)
                {
                    std::uint32_t structSize = ReadUInt32(pFile);

                    if (structSize == 8)
                    {
                        std::uint32_t connectionBlockCount = ReadUInt32(pFile);

                        if (chunkSize == structSize + connectionBlockCount * 12)
                        {
                            DLSArticulator* pArt;

                            switch (loc)
                            {
                            case ChunkLocation::INS:
                                assert(!m_DLS.Instruments.empty());
                                pArt = &m_DLS.Instruments.back().Articulators.emplace_back();
                                break;

                            case ChunkLocation::RGN:
                                assert(!m_DLS.Instruments.empty());
                                assert(!m_DLS.Instruments.back().Regions.empty());

                                pArt = &m_DLS.Instruments.back().Regions.back().Articulators.emplace_back();
                                break;

                            default:
                                assert(false);
                                throw DLSParserException(m_FileName, "invalid argument passed while parsing 'art1'");
                                break;
                            }

                            pArt->ConnectionBlocks.resize(connectionBlockCount);

                            for (auto& c : pArt->ConnectionBlocks)
                            {
                                c.Source = DLSConnectionBlock::SourceKind{ReadUInt16(pFile)};
                                c.Control = DLSConnectionBlock::SourceKind{ReadUInt16(pFile)};
                                c.Destination = DLSConnectionBlock::DestinationKind{ReadUInt16(pFile)};
                                c.Transform = DLSConnectionBlock::TransformKind{ReadUInt16(pFile)};
                                c.Scale = static_cast<std::int32_t>(ReadUInt32(pFile));
                            }
                        }
                        else
                        {
                            ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'art1' chunk has invalid size", m_FileName.c_str());
                            DoSkip(pFile, Align2(chunkSize) - 8);
                        }
                    }
                    else
                    {
                        ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'art1' chunk has invalid size", m_FileName.c_str());
                        DoSkip(pFile, Align2(chunkSize) - 4);
                    }
                }
                else
                {
                    ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'art1' chunk has invalid size", m_FileName.c_str());
                    DoSkip(pFile, Align2(chunkSize));
                }

                readSize += Align2(chunkSize) + 4;
            }
            break;

        default:
            PrintUnknownChunk(cc);
            readSize += SkipChunk(pFile);
            break;
        }

        return readSize;
    }

    std::uint32_t ParseRGNH(timidity_file* pFile)
    {
        std::uint32_t chunkSize = ReadUInt32(pFile);

        if (chunkSize == 12)
        {
            assert(!m_DLS.Instruments.empty());
            assert(!m_DLS.Instruments.back().Regions.empty());
            auto& region = m_DLS.Instruments.back().Regions.back();
            region.LoKey = ReadUInt16(pFile);
            region.HiKey = ReadUInt16(pFile);
            region.LoVelocity = ReadUInt16(pFile);
            region.HiVelocity = ReadUInt16(pFile);
            region.SelfNonExclusive = !!(ReadUInt16(pFile) & 1);
            region.KeyGroup = ReadUInt16(pFile);
        }
        else
        {
            ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'rgnh' chunk has invalid size", m_FileName.c_str());
            DoSkip(pFile, Align2(chunkSize));
        }

        return Align2(chunkSize) + 4;
    }

    std::uint32_t ParseWSMP(timidity_file* pFile, ChunkLocation loc, DLSWaveSampleInfo* pSampleInfo = nullptr)
    {
        std::uint32_t chunkSize = ReadUInt32(pFile);

        if (chunkSize >= 20)
        {
            std::uint32_t structSize = ReadUInt32(pFile);

            if (structSize == 20)
            {
                switch (loc)
                {
                case ChunkLocation::RGN:
                    assert(!m_DLS.Instruments.empty());
                    assert(!m_DLS.Instruments.back().Regions.empty());

                    pSampleInfo = &m_DLS.Instruments.back().Regions.back().SampleInfo.emplace();
                    break;

                case ChunkLocation::WAVE:
                    assert(pSampleInfo);
                    break;

                default:
                    assert(false);
                    throw DLSParserException(m_FileName, "invalid argument passed while parsing 'wsmp'");
                }

                pSampleInfo->UnityNote = static_cast<std::uint8_t>(ReadUInt16(pFile));
                pSampleInfo->FineTune = static_cast<std::int16_t>(ReadUInt16(pFile));
                pSampleInfo->Attenuation = static_cast<std::int32_t>(ReadUInt32(pFile));

                std::uint32_t options = ReadUInt32(pFile);
                pSampleInfo->NoTruncation = !!(options & 1);
                pSampleInfo->NoCompression = !!(options & 2);

                std::uint32_t loopCount = ReadUInt32(pFile);

                if (loopCount == 1)
                {
                    std::uint32_t loopInfoSize = ReadUInt32(pFile);

                    if (loopInfoSize == 16)
                    {
                        auto& loop = pSampleInfo->SampleLoops.emplace_back();

                        ReadUInt32(pFile);
                        loop.Type = DLSWaveSampleLoop::LoopType::Forward;

                        loop.LoopStart = ReadUInt32(pFile);
                        loop.LoopLength = ReadUInt32(pFile);
                    }
                    else
                    {
                        ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: invalid loop info size", m_FileName.c_str());
                        DoSkip(pFile, Align2(chunkSize) - 24);
                    }
                }
                else if (loopCount == 0)
                {
                    // skip
                }
                else
                {
                    ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: invalid loop count", m_FileName.c_str());
                    DoSkip(pFile, Align2(chunkSize) - 20);
                }
            }
            else
            {
                ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'wsmp' chunk has invalid size", m_FileName.c_str());
                DoSkip(pFile, Align2(chunkSize) - 4);
            }
        }
        else
        {
            ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'wsmp' chunk has invalid size", m_FileName.c_str());
            DoSkip(pFile, Align2(chunkSize));
        }

        return Align2(chunkSize) + 4;
    }

    std::uint32_t ParseWLNK(timidity_file* pFile)
    {
        std::uint32_t chunkSize = ReadUInt32(pFile);

        if (chunkSize == 12)
        {
            assert(!m_DLS.Instruments.empty());
            assert(!m_DLS.Instruments.back().Regions.empty());

            auto& waveLink = m_DLS.Instruments.back().Regions.back().WaveLink;
            waveLink.PhaseMaster = !!(ReadUInt16(pFile) & 1);
            waveLink.PhaseGroup = ReadUInt16(pFile);

            std::uint32_t channel = ReadUInt32(pFile);
            waveLink.Left = !!(channel & 1);
            waveLink.Right = !!(channel & 2);

            waveLink.TableIndex = ReadUInt32(pFile);
        }
        else
        {
            ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'wlnk' chunk has invalid size", m_FileName.c_str());
            DoSkip(pFile, Align2(chunkSize));
        }

        return Align2(chunkSize) + 4;
    }

    std::uint32_t ParseLRGNItem(timidity_file* pFile)
    {
        std::uint32_t cc = ReadFourCC(pFile);
        std::uint32_t readSize = 4;

        switch (cc)
        {
        case MakeFourCC("LIST"):
            {
                std::uint32_t listSize = ReadUInt32(pFile);
                readSize += 4;
                std::uint32_t listType = ReadFourCC(pFile);
                std::uint32_t currentListSize = listSize - 4;

                switch (listType)
                {
                case MakeFourCC("rgn "):
                    assert(!m_DLS.Instruments.empty());
                    m_DLS.Instruments.back().Regions.emplace_back();

                    while (currentListSize >= 2)
                    {
                        std::uint32_t cc = ReadFourCC(pFile);
                        currentListSize -= 4;

                        switch (cc)
                        {
                        case MakeFourCC("LIST"):
                            {
                                std::uint32_t listSize2 = ReadUInt32(pFile);
                                currentListSize -= 4;
                                std::uint32_t listType2 = ReadFourCC(pFile);
                                std::uint32_t currentListSize2 = listSize2 - 4;

                                switch (listType2)
                                {
                                case MakeFourCC("lart"):
                                    while (currentListSize2 >= 2)
                                    {
                                        currentListSize2 -= ParseLARTItem(pFile, ChunkLocation::RGN);
                                    }
                                    break;

                                default:
                                    PrintUnknownChunk(listType2);
                                    break;
                                }

                                DoSkip(pFile, Align2(currentListSize2));
                                currentListSize -= Align2(listSize2);
                            }
                            break;

                        case MakeFourCC("rgnh"):
                            currentListSize -= ParseRGNH(pFile);
                            break;

                        case MakeFourCC("wsmp"):
                            currentListSize -= ParseWSMP(pFile, ChunkLocation::RGN);
                            break;

                        case MakeFourCC("wlnk"):
                            currentListSize -= ParseWLNK(pFile);
                            break;

                        default:
                            PrintUnknownChunk(cc);
                            currentListSize -= SkipChunk(pFile);
                            break;
                        }
                    }
                    break;

                default:
                    PrintUnknownChunk(listType);
                    break;
                }

                DoSkip(pFile, Align2(currentListSize));
                readSize += Align2(listSize);
            }
            break;

        default:
            PrintUnknownChunk(cc);
            readSize += SkipChunk(pFile);
            break;
        }

        return readSize;
    }

    std::uint32_t ParseLINSItem(timidity_file* pFile)
    {
        std::uint32_t cc = ReadFourCC(pFile);
        std::uint32_t readSize = 4;

        switch (cc)
        {
        case MakeFourCC("LIST"):
            {
                std::uint32_t listSize = ReadUInt32(pFile);
                readSize += 4;
                std::uint32_t listType = ReadFourCC(pFile);
                std::uint32_t currentListSize = listSize - 4;

                switch (listType)
                {
                case MakeFourCC("ins "):
                    m_DLS.Instruments.emplace_back();

                    while (currentListSize >= 2)
                    {
                        std::uint32_t cc = ReadFourCC(pFile);
                        currentListSize -= 4;

                        switch (cc)
                        {
                        case MakeFourCC("LIST"):
                            {
                                std::uint32_t listSize2 = ReadUInt32(pFile);
                                currentListSize -= 4;
                                std::uint32_t listType2 = ReadFourCC(pFile);
                                std::uint32_t currentListSize2 = listSize2 - 4;

                                switch (listType2)
                                {
                                case MakeFourCC("INFO"):
                                    while (currentListSize2 >= 2)
                                    {
                                        currentListSize2 -= ParseINFOItem(pFile, ChunkLocation::INS);
                                    }
                                    break;

                                case MakeFourCC("lrgn"):
                                    while (currentListSize2 >= 2)
                                    {
                                        currentListSize2 -= ParseLRGNItem(pFile);
                                    }
                                    break;

                                case MakeFourCC("lart"):
                                    while (currentListSize2 >= 2)
                                    {
                                        currentListSize2 -= ParseLARTItem(pFile, ChunkLocation::INS);
                                    }
                                    break;

                                default:
                                    PrintUnknownChunk(listType2);
                                    break;
                                }

                                DoSkip(pFile, Align2(currentListSize2));
                                currentListSize -= Align2(listSize2);
                            }
                            break;

                        case MakeFourCC("dlid"):
                            currentListSize -= ParseDLID(pFile);
                            break;

                        case MakeFourCC("insh"):
                            currentListSize -= ParseINSH(pFile);
                            break;

                        default:
                            PrintUnknownChunk(cc);
                            currentListSize -= SkipChunk(pFile);
                            break;
                        }
                    }
                    break;

                default:
                    PrintUnknownChunk(listType);
                    break;
                }

                DoSkip(pFile, Align2(currentListSize));
                readSize += Align2(listSize);
            }
            break;

        default:
            PrintUnknownChunk(cc);
            readSize += SkipChunk(pFile);
            break;
        }

        return readSize;
    }

    std::uint32_t ParsePTBL(timidity_file* pFile)
    {
        std::uint32_t chunkSize = ReadUInt32(pFile);

        if (chunkSize >= 8)
        {
            std::uint32_t structSize = ReadUInt32(pFile);

            if (structSize == 8)
            {
                std::uint32_t cueCount = ReadUInt32(pFile);

                if (chunkSize == 8 + cueCount * 4)
                {
                    m_DLS.PoolTable.resize(cueCount);

#ifdef LITTLE_ENDIAN
                    if (::tf_read(m_DLS.PoolTable.data(), 4, cueCount, pFile) != cueCount)
                    {
                        throw DLSParserException(m_FileName, "unexpected end of file");
                    }
#else
                    for (auto& c : m_DLS.PoolTable)
                    {
                        c = ReadUInt32(pFile);
                    }
#endif
                }
                else
                {
                    ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'wlnk' chunk has invalid size", m_FileName.c_str());
                    DoSkip(pFile, Align2(chunkSize) - 8);
                }
            }
            else
            {
                ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'wlnk' chunk has invalid size", m_FileName.c_str());
                DoSkip(pFile, Align2(chunkSize) - 4);
            }
        }
        else
        {
            ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'ptbl' chunk has invalid size", m_FileName.c_str());
            DoSkip(pFile, Align2(chunkSize));
        }

        return Align2(chunkSize) + 4;
    }

    void ParseRIFF(timidity_file* pFile)
    {
        if (ReadFourCC(pFile) != MakeFourCC("RIFF"))
        {
            throw DLSParserException(m_FileName, "not an RIFF file");
        }

        std::size_t riffSize = ReadUInt32(pFile);

        if (riffSize >= 4 && ReadFourCC(pFile) != MakeFourCC("DLS "))
        {
            throw DLSParserException(m_FileName, "not a DLS file");
        }

        riffSize -= 4;

        while (riffSize >= 2)
        {
            std::uint32_t cc = ReadFourCC(pFile);
            riffSize -= 4;

            switch (cc)
            {
            case MakeFourCC("LIST"):
                {
                    std::uint32_t listSize = ReadUInt32(pFile);
                    riffSize -= 4;

                    std::uint32_t listType = ReadFourCC(pFile);
                    std::uint32_t currentListSize = listSize - 4;

                    switch (listType)
                    {
                    case MakeFourCC("INFO"):
                        while (currentListSize >= 2)
                        {
                            currentListSize -= ParseINFOItem(pFile, ChunkLocation::DLS);
                        }
                        break;

                    case MakeFourCC("lins"):
                        while (currentListSize >= 2)
                        {
                            currentListSize -= ParseLINSItem(pFile);
                        }
                        break;

                    case MakeFourCC("wvpl"):
                        m_DLS.WavePoolOffset = static_cast<std::uint32_t>(::tf_tell(pFile));
                        break;

                    default:
                        PrintUnknownChunk(listType);
                        break;
                    }

                    DoSkip(pFile, Align2(currentListSize));
                    riffSize -= Align2(listSize);
                }
                break;

            case MakeFourCC("vers"):
                riffSize -= ParseVERS(pFile);
                break;

            case MakeFourCC("dlid"):
                riffSize -= ParseDLID(pFile);
                break;

            case MakeFourCC("colh"):
                riffSize -= ParseCOLH(pFile);
                break;

            case MakeFourCC("ptbl"):
                riffSize -= ParsePTBL(pFile);
                break;

            default:
                PrintUnknownChunk(cc);
                riffSize -= SkipChunk(pFile);
                break;
            }
        }

        if (riffSize)
        {
            // skip padding byte
            tf_getc(pFile);
            riffSize--;
        }

        if (tf_getc(pFile) != EOF)
        {
            ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: data exist after the RIFF chunk", m_FileName.c_str());
        }
    }

    DLSWaveInfo ParseWAVEList(std::uint32_t offset)
    {
        std::unique_ptr<timidity_file, TFFileCloser> pFile(::open_file(m_FileName.data(), 1, OF_NORMAL));

        if (::tf_seek(pFile.get(), offset, SEEK_SET) == -1)
        {
            throw DLSParserException(m_FileName, "cannot seek file");
        }

        if (ReadFourCC(pFile.get()) != MakeFourCC("LIST"))
        {
            throw DLSParserException(m_FileName, "expected 'LIST'");
        }

        std::uint32_t listSize = ReadUInt32(pFile.get());

        if (ReadFourCC(pFile.get()) != MakeFourCC("wave"))
        {
            throw DLSParserException(m_FileName, "expected 'wave'");
        }

        listSize -= 4;
        DLSWaveInfo waveInfo = {};

        while (listSize >= 2)
        {
            std::uint32_t cc = ReadFourCC(pFile.get());
            listSize -= 4;

            switch (cc)
            {
            case MakeFourCC("LIST"):
                {
                    std::uint32_t listSize2 = ReadUInt32(pFile.get());
                    listSize -= 4;
                    std::uint32_t listType2 = ReadFourCC(pFile.get());
                    std::uint32_t currentListSize2 = listSize2 - 4;

                    switch (listType2)
                    {
                    case MakeFourCC("INFO"):
                        while (currentListSize2 >= 2)
                        {
                            currentListSize2 -= ParseINFOItem(pFile.get(), ChunkLocation::WAVE);
                        }
                        break;

                    default:
                        PrintUnknownChunk(listType2);
                        break;
                    }

                    DoSkip(pFile.get(), Align2(currentListSize2));
                    listSize -= Align2(listSize2);
                }
                break;

            case MakeFourCC("dlib"):
                listSize -= ParseDLID(pFile.get());
                break;

            case MakeFourCC("fmt "):
                {
                    std::uint32_t chunkSize = ReadUInt32(pFile.get());
                    listSize -= 4;

                    if (chunkSize >= 16)
                    {
                        waveInfo.FormatTag = ReadUInt16(pFile.get());
                        waveInfo.Channels = ReadUInt16(pFile.get());

                        if (waveInfo.Channels != 1)
                        {
                            throw DLSParserException(m_FileName, "multichannel samples are not supported");
                        }

                        waveInfo.SamplesPerSec = ReadUInt32(pFile.get());
                        waveInfo.AvgBytesPerSec = ReadUInt32(pFile.get());
                        waveInfo.BlockAlign = ReadUInt16(pFile.get());
                        waveInfo.BitsPerSample = ReadUInt16(pFile.get());

                        if (waveInfo.BitsPerSample != 8 && waveInfo.BitsPerSample != 16)
                        {
                            throw DLSParserException(m_FileName, "unsupported bit rate");
                        }

                        DoSkip(pFile.get(), Align2(chunkSize) - 16);
                    }
                    else
                    {
                        ctl->cmsg(CMSG_WARNING, VERB_DEBUG, "%s: warning: 'fmt ' chunk has invalid size", m_FileName.c_str());
                        DoSkip(pFile.get(), Align2(chunkSize));
                    }

                    listSize -= Align2(chunkSize);
                }
                break;

            case MakeFourCC("data"):
                {
                    std::uint32_t chunkSize = ReadUInt32(pFile.get());
                    listSize -= 4;

                    std::size_t bufferSize = chunkSize + 128 * 2;
                    waveInfo.pData.reset(reinterpret_cast<char*>(safe_large_malloc(bufferSize)));

#ifdef LITTLE_ENDIAN
                    if (::tf_read(waveInfo.pData.get(), 1, chunkSize, pFile.get()) != chunkSize)
                    {
                        throw DLSParserException(m_FileName, "unexpected end of file");
                    }
#else
                    for (std::size_t i = 0; i < chunkSize / 2; i++)
                    {
                        reinterpret_cast<std::uint16_t*>(waveInfo.pData.get())[i] = ReadUInt16(pFile.get());
                    }
#endif
                    std::memset(waveInfo.pData.get() + chunkSize, 0, bufferSize - chunkSize);
                    waveInfo.DataLength = chunkSize;

                    if (chunkSize & 1)
                    {
                        DoSkip(pFile.get(), 1);
                    }

                    listSize -= Align2(chunkSize);
                }
                break;

            case MakeFourCC("wsmp"):
                listSize -= ParseWSMP(pFile.get(), ChunkLocation::WAVE, &waveInfo.SampleInfo.emplace());
                break;

            default:
                PrintUnknownChunk(cc);
                listSize -= SkipChunk(pFile.get());
                break;
            }
        }

        waveInfo.DataLength /= (waveInfo.BitsPerSample == 0 ? 1 : waveInfo.BitsPerSample / 8);
        return waveInfo;
    }

    std::string m_FileName;
    DLSCollection m_DLS = {};
};

struct InstrumentCacheEntry
{
    InstrumentCacheEntry(
        std::string_view filePath,
        std::uint8_t bank,
        std::uint8_t programNumber,
        std::uint8_t note,
        std::unique_ptr<Instrument, InstrumentDeleter> pInstrument
    )
        : FilePath(filePath), Bank(bank), ProgramNumber(programNumber), Note(note), pInstrument(std::move(pInstrument))
    {
    }

    std::string FilePath;
    std::uint8_t Bank;
    std::int8_t ProgramNumber;
    std::int8_t Note;
    std::unique_ptr<Instrument, InstrumentDeleter> pInstrument;
    std::vector<Instrument*> RefInstruments;
};

class InstrumentCache
{
public:
    Instrument* LoadDLS(std::string filePath, std::uint8_t bank, std::int8_t programNumber, std::int8_t note)
    {
        try
        {
            auto itDLS = m_DLSParsers.find(filePath);

            if (itDLS == m_DLSParsers.end())
            {
                DLSParser parser(filePath);
                parser.Parse();
                itDLS = m_DLSParsers.emplace(filePath, std::move(parser)).first;
            }

            auto itInst = std::find_if(
                m_Instruments.begin(),
                m_Instruments.end(),
                [&filePath, bank, programNumber, note] (auto&& x)
                {
                    return x.FilePath == filePath && x.Bank == bank && x.ProgramNumber == programNumber
                        && (note < 0 || x.Note == note);
                }
            );

            if (itInst == m_Instruments.end())
            {
                auto pInstrument = itDLS->second.BuildInstrument(bank, programNumber, note);

                m_Instruments.emplace_back(filePath, bank, programNumber, note, std::move(pInstrument));
                itInst = std::prev(m_Instruments.end());
            }

            std::unique_ptr<Instrument, InstrumentDeleter> pInstRef(reinterpret_cast<Instrument*>(safe_calloc(sizeof(Instrument), 1)));
            itInst->RefInstruments.push_back(pInstRef.get());
            pInstRef->type = itInst->pInstrument->type;
            pInstRef->instname = safe_strdup(itInst->pInstrument->instname);
            pInstRef->samples = itInst->pInstrument->samples;
            pInstRef->sample = reinterpret_cast<Sample*>(safe_calloc(sizeof(Sample), itInst->pInstrument->samples));
            std::copy_n(itInst->pInstrument->sample, itInst->pInstrument->samples, pInstRef->sample);
            std::for_each(pInstRef->sample, pInstRef->sample + pInstRef->samples, [] (auto&& x) { x.data_alloced = false; });

            return pInstRef.release();
        }
        catch (const std::exception& e)
        {
            char str[] = "%s";
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, str, e.what());
            return nullptr;
        }
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
        m_DLSParsers.clear();
    }

private:
    std::unordered_map<std::string, DLSParser> m_DLSParsers;
    std::vector<InstrumentCacheEntry> m_Instruments;
};

InstrumentCache GlobalInstrumentCache;

} // namespace TimDLS

extern "C" void init_dls(void)
{
}

extern "C" void free_dls(void)
{
    TimDLS::GlobalInstrumentCache.FreeAll();
}

extern "C" Instrument *extract_dls_file(char *sample_file, uint8 font_bank, int8 font_preset, int8 font_keynote)
{
    return TimDLS::GlobalInstrumentCache.LoadDLS(sample_file, font_bank, font_preset, font_keynote);
}

extern "C" void free_dls_file(Instrument *ip)
{
    TimDLS::GlobalInstrumentCache.FreeInstrument(ip);
}
