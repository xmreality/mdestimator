#include "SendSideBandwidthEstimation.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

constexpr uint64_t kInitialDuration = 1500E3;// 1500ms
constexpr uint64_t kReportInterval = 250E3;  // 250ms
constexpr uint64_t kMonitorDuration = 250E3; // 250ms
constexpr uint64_t kLongTermDuration = 60E6; // 60s
constexpr uint64_t kMinRate = 128E3;         // 128kbps
constexpr uint64_t kMaxRate = 100E6;         // 100mbps
constexpr uint64_t kMinRateChangeBps = 16000;
constexpr double kSamplingStep = 0.05;
constexpr double kInitialRampUp = 1.30;

SendSideBandwidthEstimation::SendSideBandwidthEstimation() :
        bandwidthEstimation(0),
        targetBitrate(0),
        availableRate(0),
        firstSent(0),
        firstRecv(0),
        prevSent(0),
        prevRecv(0),
        rtt(0),
        lastChange(0),
        fd(FD_INVALID),
        accumulatedDelta(0),
        state(ChangeState::Initial),
        consecutiveChanges(0),
        lastFeedbackNum(0),
        rttMin(kLongTermDuration),
        deltaAcumulator(kMonitorDuration, 1E6),
        totalSentAcumulator(kMonitorDuration, 1E6),
        mediaSentAcumulator(kMonitorDuration, 1E6),
        rtxSentAcumulator(kMonitorDuration, 1E6),
        probingSentAcumulator(kMonitorDuration, 1E6),
        totalRecvAcumulator(kMonitorDuration, 1E6),
        mediaRecvAcumulator(kMonitorDuration, 1E6),
        rtxRecvAcumulator(kMonitorDuration, 1E6),
        probingRecvAcumulator(kMonitorDuration, 1E6)
{
}

SendSideBandwidthEstimation::~SendSideBandwidthEstimation()
{
    if (fd != FD_INVALID)
    {
        close(fd);
    }
}

void SendSideBandwidthEstimation::SentPacket(const uint32_t transportWideSeqNum,
        const uint32_t size,
        const uint64_t time,
        const uint8_t mark,
        const uint8_t rtx,
        const uint8_t probing)
{
    if (!firstSent)
    {
        firstSent = time;
    }

    totalSentAcumulator.Update(time, size);

    if (probing == 1)
    {
        probingSentAcumulator.Update(time, size);
        rtxSentAcumulator.Update(time);
        mediaSentAcumulator.Update(time);
    }
    else if (rtx == 1)
    {
        probingSentAcumulator.Update(time);
        rtxSentAcumulator.Update(time, size);
        mediaSentAcumulator.Update(time);
    }
    else
    {
        probingSentAcumulator.Update(time);
        rtxSentAcumulator.Update(time);
        mediaSentAcumulator.Update(time, size);
    }

    transportWideSentPacketsStats.Set(transportWideSeqNum, SendSideBandwidthEstimation::Stats{time, size, mark, rtx, probing});
}

void SendSideBandwidthEstimation::ReceivedFeedback(const uint8_t feedbackNum, const uint32_t* sequenceNumbers, const uint64_t* timestamps, const uint64_t when)
{
    feedbackNumExtender.Extend(feedbackNum);
    uint64_t extFeedbackNum = feedbackNumExtender.GetExtSeqNum();
    if (lastFeedbackNum && extFeedbackNum > lastFeedbackNum + 1)
    {
        totalRecvAcumulator.Reset(0);
    }

    lastFeedbackNum = extFeedbackNum;

    if (feedbackNum == 0)
    {
        return;
    }

    const auto last = transportWideSentPacketsStats.Get(sequenceNumbers[0]);
    if (last.has_value())
    {
        const auto sentTime = last->time;
        if (when > sentTime)
        {
            //Calculate rtt proxy, it is guaranteed to be bigger than rtt
            const auto rttProxy = (when - sentTime) / 1000ULL;
            rttMin.Add(rttProxy, when);
        }
    }

    for (size_t i = 0; i < feedbackNum; ++i)
    {
        const auto transportSeqNum = sequenceNumbers[i];
        const auto receivedTime = timestamps[i];
        const auto stat = transportWideSentPacketsStats.Get(transportSeqNum);

        if (stat.has_value())
        {
            const auto sentTime = stat->time;

            if (!firstRecv)
            {
                firstRecv = receivedTime;
            }

            const auto fb = when - firstSent;
            const auto sent = sentTime - firstSent;
            const auto recv = receivedTime ? receivedTime - firstRecv : 0;
            const auto deltaSent = sent - prevSent;
            const auto deltaRecv = receivedTime ? recv - prevRecv : 0;
            const auto delta = static_cast<int64_t>(receivedTime ? deltaRecv - deltaSent : 0);

            accumulatedDelta += delta;
            if (accumulatedDelta < 0)
            {
                accumulatedDelta = 0;
            }

            if (fd != FD_INVALID)
            {
                char msg[1024];
                const auto len = snprintf(msg,
                        1024,
                        "%.8lu|%u|%hhu|%u|%lu|%lu|%lu|%lu|%ld|%u|%u|%u|%u|%d|%d|%d\n",
                        fb,
                        transportSeqNum,
                        feedbackNum,
                        stat->size,
                        sent,
                        recv,
                        deltaSent,
                        deltaRecv,
                        delta,
                        GetEstimatedBitrate(),
                        GetTargetBitrate(),
                        GetAvailableBitrate(),
                        rtt,
                        stat->mark,
                        stat->rtx,
                        stat->probing);
                [[maybe_unused]] const auto writeResult = write(fd, msg, len);
            }

            if (receivedTime != 0)
            {
                totalRecvAcumulator.Update(receivedTime, stat->size);
                if (stat->probing)
                {
                    probingRecvAcumulator.Update(receivedTime, stat->size);
                    rtxRecvAcumulator.Update(receivedTime);
                    mediaRecvAcumulator.Update(receivedTime);
                }
                else if (stat->rtx)
                {
                    probingRecvAcumulator.Update(receivedTime);
                    rtxRecvAcumulator.Update(receivedTime, stat->size);
                    mediaRecvAcumulator.Update(receivedTime);
                }
                else
                {
                    probingRecvAcumulator.Update(receivedTime);
                    rtxRecvAcumulator.Update(receivedTime);
                    mediaRecvAcumulator.Update(receivedTime, stat->size);
                }

                prevSent = sent;
                prevRecv = recv;
            }
        }
    }

    EstimateBandwidthRate(when);
}

void SendSideBandwidthEstimation::UpdateRTT(const uint64_t when, const uint32_t newRtt)
{
    rtt = newRtt;
    rttMin.Add(when, newRtt);
}

uint32_t SendSideBandwidthEstimation::GetEstimatedBitrate() const
{
    return bandwidthEstimation;
}

uint32_t SendSideBandwidthEstimation::GetAvailableBitrate() const
{
    return availableRate;
}

uint32_t SendSideBandwidthEstimation::GetMinRTT() const
{
    const auto min = rttMin.GetMin();
    if (!min)
    {
        return rtt;
    }
    return *min;
}

uint32_t SendSideBandwidthEstimation::GetTargetBitrate() const
{
    return std::min(std::max(targetBitrate, kMinRate), kMaxRate);
}

void SendSideBandwidthEstimation::SetState(const ChangeState newState)
{
    if (state == newState)
    {
        consecutiveChanges++;
    }
    else
    {
        consecutiveChanges = 0;
    }
    state = newState;
}

void SendSideBandwidthEstimation::EstimateBandwidthRate(const uint64_t when)
{
    const auto minRtt = GetMinRTT();
    assert(accumulatedDelta >= 0);
    const auto rttEstimated = minRtt + static_cast<uint32_t>(accumulatedDelta) / 1000;

    const auto totalRecvBitrate = static_cast<uint64_t>(std::round(totalRecvAcumulator.GetInstantAvg() * 8.0));
    const auto totalSentBitrate = static_cast<uint64_t>(std::round(totalSentAcumulator.GetInstantAvg() * 8.0));
    const auto rtxSentBitrate = static_cast<uint64_t>(std::round(rtxSentAcumulator.GetInstantAvg() * 8.0));

    const auto instantSentBitrate = static_cast<uint64_t>(std::round(mediaSentAcumulator.GetInstantAvg() * 8.0));

    if (state == ChangeState::Initial)
    {
        if (!lastChange)
        {
            lastChange = when;
        }
        else if (lastChange + kInitialDuration < when)
        {
            SetState(ChangeState::Increase);
        }

        if (!totalRecvAcumulator.IsInWindow())
        {
            return;
        }
        bandwidthEstimation = totalRecvBitrate;
    }
    else if (minRtt > 0 && static_cast<double>(rttEstimated) > (20.0 + static_cast<double>(minRtt) * 1.3))
    {
        SetState(ChangeState::Congestion);
        if (!totalRecvAcumulator.IsInWindow())
        {
            return;
        }
        bandwidthEstimation = totalRecvBitrate;
    }
    else if (instantSentBitrate > targetBitrate)
    {
        SetState(ChangeState::OverShoot);
        bandwidthEstimation = std::max<uint64_t>(bandwidthEstimation, totalRecvBitrate);
    }
    else if (state == ChangeState::Congestion || state == ChangeState::OverShoot)
    {
        SetState(ChangeState::Recovery);
    }
    else if (state == ChangeState::Recovery)
    {
        if (totalRecvAcumulator.IsInWindow())
        {
            //Decrease bwe to converge to the received rate
            bandwidthEstimation = bandwidthEstimation * 0.99 + totalRecvBitrate * 0.01;
        }
        const auto confidenceAmplifier = 1.0 + std::log(static_cast<double>(consecutiveChanges) + 1.0);
        const auto rateChange = std::max<uint64_t>(
                static_cast<uint64_t>(std::round(static_cast<double>(totalSentBitrate) * confidenceAmplifier * kSamplingStep)),
                kMinRateChangeBps);

        targetBitrate = std::min(bandwidthEstimation, targetBitrate + rateChange);

        if (targetBitrate == bandwidthEstimation)
        {
            SetState(ChangeState::Increase);
        }
    }
    else
    {
        const auto confidenceAmplifier = 1.0 + std::log(static_cast<double>(consecutiveChanges) + 1.0);
        const auto rateChange = std::max<uint64_t>(
                static_cast<uint64_t>(std::round(static_cast<double>(totalSentBitrate) * confidenceAmplifier * kSamplingStep)),
                kMinRateChangeBps);

        bandwidthEstimation = std::max(bandwidthEstimation, totalSentBitrate - rtxSentBitrate + std::min<int64_t>(rateChange, totalSentBitrate));
    }

    bandwidthEstimation = std::min(std::max(bandwidthEstimation, kMinRate), kMaxRate);

    if (state == ChangeState::Initial)
    {
        targetBitrate = bandwidthEstimation * kInitialRampUp;
    }
    else if (state != ChangeState::Recovery)
    {
        targetBitrate = bandwidthEstimation;
    }

    const auto rtxSent = rtxSentAcumulator.GetAcumulated();
    const auto mediaSent = mediaSentAcumulator.GetAcumulated();
    const auto totalSent = rtxSent + mediaSent;

    const auto overhead = totalSent ? static_cast<double>(mediaSent) / static_cast<double>(totalSent) : 1.0;
    availableRate = static_cast<uint64_t>(std::round(static_cast<double>(targetBitrate) * overhead));
    availableRate = std::min(std::max(availableRate, kMinRate), kMaxRate);

    if (state != ChangeState::Initial && ((state == ChangeState::Congestion && consecutiveChanges == 0) || (lastChange + kReportInterval < when)))
    {
        lastChange = when;
    }
}

int SendSideBandwidthEstimation::Dump(const char* filename)
{
    if (fd != FD_INVALID)
    {
        return 0;
    }

    fprintf(stderr, "-SendSideBandwidthEstimation::Dump() [\"%s\"]\n", filename);
    if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0)
    {
        return false;
    }

    return 1;
}

int SendSideBandwidthEstimation::StopDump()
{
    if (fd == FD_INVALID)
    {
        return 0;
    }

    fprintf(stderr, "-SendSideBandwidthEstimation::StopDump()\n");
    close(fd);
    fd = FD_INVALID;
    return 1;
}
