#ifndef MDESTIMATOR_H
#define MDESTIMATOR_H

#ifdef __cplusplus

#include <cstdint>

#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

struct Estimator;

__attribute__((visibility("default"))) int mdestimator_create(struct Estimator** estimator);
__attribute__((visibility("default"))) void mdestimator_destroy(struct Estimator* estimator);

__attribute__((visibility("default"))) void mdestimator_sent_packet(struct Estimator* estimator,
        uint32_t transportWideSeqNum,
        uint32_t size,
        uint64_t time,
        uint8_t mark,
        uint8_t rtx,
        uint8_t probing);

__attribute__((visibility("default"))) void mdestimator_received_feedback(struct Estimator* estimator,
        uint8_t feedback_num,
        uint32_t* sequence_numbers,
        uint64_t* timestamps,
        uint64_t when);

__attribute__((visibility("default"))) void mdestimator_update_rtt(struct Estimator* estimator, uint64_t when, uint32_t rtt);
__attribute__((visibility("default"))) uint32_t mdestimator_get_estimated_bitrate(struct Estimator* estimator);
__attribute__((visibility("default"))) uint32_t mdestimator_get_target_bitrate(struct Estimator* estimator);
__attribute__((visibility("default"))) uint32_t mdestimator_get_available_bitrate(struct Estimator* estimator);
__attribute__((visibility("default"))) uint32_t mdestimator_get_min_rtt(struct Estimator* estimator);
__attribute__((visibility("default"))) void mdestimator_dump(struct Estimator* estimator, const char* file_name);
__attribute__((visibility("default"))) void mdestimator_stop_dump(struct Estimator* estimator);

#ifdef __cplusplus
}
#endif//__cplusplus

#endif//MDESTIMATOR_H
