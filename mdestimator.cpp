#include "SendSideBandwidthEstimation.h"
#include "mdestimator/mdestimator.h"

extern "C" struct Estimator
{
    SendSideBandwidthEstimation* instance;
};

extern "C" int mdestimator_create(Estimator** estimator)
{
    auto result = new Estimator();
    result->instance = new SendSideBandwidthEstimation;
    *estimator = result;
    return 0;
}

extern "C" void mdestimator_destroy(Estimator* estimator)
{
    if (!estimator)
    {
        return;
    }

    if (!estimator->instance)
    {
        return;
    }

    delete (estimator->instance);
    delete (estimator);
}

extern "C" void mdestimator_sent_packet(struct Estimator* estimator,
        uint32_t transportWideSeqNum,
        uint32_t size,
        uint64_t time,
        uint8_t mark,
        uint8_t rtx,
        uint8_t probing)
{
    if (!estimator || !estimator->instance)
    {
        return;
    }
    estimator->instance->SentPacket(transportWideSeqNum, size, time, mark, rtx, probing);
}

extern "C" void mdestimator_received_feedback(struct Estimator* estimator,
        uint8_t feedback_num,
        uint32_t* sequence_numbers,
        uint64_t* timestamps,
        uint64_t when)
{
    if (!estimator || !estimator->instance)
    {
        return;
    }

    estimator->instance->ReceivedFeedback(feedback_num, sequence_numbers, timestamps, when);
}

extern "C" void mdestimator_update_rtt(struct Estimator* estimator, uint64_t when, uint32_t rtt)
{
    if (!estimator || !estimator->instance)
    {
        return;
    }

    estimator->instance->UpdateRTT(when, rtt);
}

extern "C" uint32_t mdestimator_get_estimated_bitrate(struct Estimator* estimator)
{
    if (!estimator || !estimator->instance)
    {
        return 0;
    }

    return estimator->instance->GetEstimatedBitrate();
}

extern "C" uint32_t mdestimator_get_target_bitrate(struct Estimator* estimator)
{
    if (!estimator || !estimator->instance)
    {
        return 0;
    }

    return estimator->instance->GetTargetBitrate();
}

extern "C" uint32_t mdestimator_get_available_bitrate(struct Estimator* estimator)
{
    if (!estimator || !estimator->instance)
    {
        return 0;
    }

    return estimator->instance->GetAvailableBitrate();
}

extern "C" uint32_t mdestimator_get_min_rtt(struct Estimator* estimator)
{
    if (!estimator || !estimator->instance)
    {
        return 0;
    }

    return estimator->instance->GetMinRTT();
}

extern "C" void mdestimator_dump(struct Estimator* estimator, const char* file_name)
{
    if (!estimator || !estimator->instance)
    {
        return;
    }

    estimator->instance->Dump(file_name);
}

extern "C" void mdestimator_stop_dump(struct Estimator* estimator)
{
    if (!estimator || !estimator->instance)
    {
        return;
    }
    estimator->instance->StopDump();
}
