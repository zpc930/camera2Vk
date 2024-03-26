#include "FpsCollector.h"
#include "Common.h"

#define NANOSECONDS_TO_MILLISECONDS 0.000001f
#define MILLISECONDS_TO_NANOSECONDS 1000000.0f
#define MILLISECONDS_TO_SECONDS 0.001f

static uint64_t fpsPeriodNs = 1000 * MILLISECONDS_TO_NANOSECONDS;
void FpsCollector::collect() {
        mCurrentFrameIndex++;
        mTotalFrame++;
        if (mLastFpsCalTime == 0) {
                mLastFpsCalTime = getTimeNano();
        }
        if (getTimeNano() - mLastFpsCalTime >= fpsPeriodNs) {
                LOG_D("%s : fps is %d in %.3f ms, mTotalFrame = %ld", mTagName, mCurrentFrameIndex,
                        (getTimeNano() - mLastFpsCalTime)  * NANOSECONDS_TO_MILLISECONDS * 1.0f, mTotalFrame);
                mCurrentFrameIndex = 0;
                mLastFpsCalTime = getTimeNano();
        }
        if (mLastCommitTime > 0 && (getTimeNano() - mLastCommitTime) > mCommitLatencyThreshold) {
            LOG_W("%s : %f ms passed since last collect time", mTagName, (getTimeNano() - mLastCommitTime) * NANOSECONDS_TO_MILLISECONDS * 1.0f);
        }
        mLastCommitTime = getTimeNano();
}
