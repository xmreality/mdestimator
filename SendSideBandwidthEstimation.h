#pragma once
#include <deque>
#include <utility>
#include <vector>
#include <map>
#include "acumulator.h"
#include "MovingCounter.h"
#include "CircularBuffer.h"
#include "WrapExtender.h"

#define FD_INVALID (int)-1

class SendSideBandwidthEstimation
{

public:
    enum ChangeState
    {
        Initial,
        Increase,
        OverShoot,
        Congestion,
        Recovery
    };
public:
    SendSideBandwidthEstimation();
    ~SendSideBandwidthEstimation();
    void SentPacket(const uint32_t transportWideSeqNum, const uint32_t size, const uint64_t time, const uint8_t mark, const uint8_t rtx, const uint8_t probing);
    void ReceivedFeedback(const uint8_t feedbackNum, const uint32_t* sequenceNumbers, const uint64_t* timestamps, const uint64_t when = 0);
    void UpdateRTT(const uint64_t when, const uint32_t newRtt);
    uint32_t GetEstimatedBitrate() const;
    uint32_t GetTargetBitrate() const;
    uint32_t GetAvailableBitrate() const;
    uint32_t GetMinRTT() const;

    int Dump(const char* filename);
    int StopDump();

private:
    void SetState(const ChangeState newState);
    void EstimateBandwidthRate(const uint64_t when);

    struct Stats
    {
        uint64_t time;
        uint32_t size;
        uint8_t mark;
        uint8_t rtx;
        uint8_t probing;
    };

    CircularBuffer<Stats, uint16_t, 32768> transportWideSentPacketsStats;
    uint64_t bandwidthEstimation = 0;
    uint64_t targetBitrate = 0;
    uint64_t availableRate = 0;
    uint64_t firstSent = 0;
    uint64_t firstRecv = 0;
    uint64_t prevSent = 0;
    uint64_t prevRecv = 0;
    uint32_t rtt = 0;
    uint64_t lastChange = 0;
    int fd = FD_INVALID;

    int64_t accumulatedDelta = 0;

    ChangeState state = ChangeState::Initial;
    uint32_t consecutiveChanges = 0;

    WrapExtender<uint8_t, uint64_t> feedbackNumExtender;
    uint64_t lastFeedbackNum = 0;

    MovingMinCounter<uint64_t> rttMin;
    Acumulator<uint32_t, uint64_t> deltaAcumulator;
    Acumulator<uint32_t, uint64_t> totalSentAcumulator;
    Acumulator<uint32_t, uint64_t> mediaSentAcumulator;
    Acumulator<uint32_t, uint64_t> rtxSentAcumulator;
    Acumulator<uint32_t, uint64_t> probingSentAcumulator;
    Acumulator<uint32_t, uint64_t> totalRecvAcumulator;
    Acumulator<uint32_t, uint64_t> mediaRecvAcumulator;
    Acumulator<uint32_t, uint64_t> rtxRecvAcumulator;
    Acumulator<uint32_t, uint64_t> probingRecvAcumulator;
};
