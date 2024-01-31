/*****************************************************************************
* @copyright Copyright (c) 2023, ThunderSoft Technologies, Inc.
* All Rights Reserved Confidential and Proprietary - ThunderSoft Technologies, Inc.
******************************************************************************/
#ifndef TXR_RENDER_FPS_COLLECTOR_H
#define TXR_RENDER_FPS_COLLECTOR_H

#include <ctime>
#include "stdio.h"

class FpsCollector{
private:
    uint64_t mLastFpsCalTime;
    int32_t mCurrentFrameIndex;
    uint64_t mCommitLatencyThreshold;
    uint64_t mLastCommitTime;
    char* mTagName;
    uint64_t mTotalFrame;
public:
    FpsCollector(char* tag, uint64_t commitLatencyThresholdNs) {
        mTagName = tag;
        mCommitLatencyThreshold = commitLatencyThresholdNs;
        mLastFpsCalTime = 0;
        mCurrentFrameIndex = 0;
        mLastCommitTime = 0;
        mTotalFrame = 0;
    }
    uint64_t getTotalFrame() const { return mTotalFrame; };
    void collect();
};


#endif //TXR_RENDER_FPS_COLLECTOR_H
