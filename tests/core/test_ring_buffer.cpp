#include "audio/AudioRingBuffer.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>

using abracapture::audio::AudioRingBuffer;

TEST(AudioRingBuffer, SingleThreadedWriteReadPreservesOrder) {
    AudioRingBuffer buffer(8);
    const float input[] = {1.0F, 2.0F, 3.0F, 4.0F};
    float output[4] = {};

    EXPECT_EQ(buffer.write(input, 4), 0U);
    EXPECT_EQ(buffer.available(), 4U);
    EXPECT_EQ(buffer.read(output, 4), 4U);
    EXPECT_TRUE(output[0] == 1.0F);
    EXPECT_TRUE(output[1] == 2.0F);
    EXPECT_TRUE(output[2] == 3.0F);
    EXPECT_TRUE(output[3] == 4.0F);
    EXPECT_EQ(buffer.available(), 0U);
}

TEST(AudioRingBuffer, CapacityBoundaryAcceptsExactlyCapacityFrames) {
    AudioRingBuffer buffer(4);
    const float input[] = {10.0F, 20.0F, 30.0F, 40.0F};

    EXPECT_EQ(buffer.capacity(), 4U);
    EXPECT_EQ(buffer.write(input, 4), 0U);
    EXPECT_EQ(buffer.available(), 4U);
    EXPECT_EQ(buffer.overflowCount(), 0U);
}

TEST(AudioRingBuffer, FullBufferReportsOverflowAndKeepsQueuedFrames) {
    AudioRingBuffer buffer(4);
    const float first[] = {1.0F, 2.0F, 3.0F};
    const float second[] = {4.0F, 5.0F};
    float output[4] = {};

    EXPECT_EQ(buffer.write(first, 3), 0U);
    EXPECT_EQ(buffer.write(second, 2), 1U);
    EXPECT_EQ(buffer.overflowCount(), 1U);
    EXPECT_EQ(buffer.read(output, 4), 4U);
    EXPECT_TRUE(output[0] == 1.0F);
    EXPECT_TRUE(output[1] == 2.0F);
    EXPECT_TRUE(output[2] == 3.0F);
    EXPECT_TRUE(output[3] == 4.0F);
}

TEST(AudioRingBuffer, PartialReadReportsMissingFrames) {
    AudioRingBuffer buffer(8);
    const float input[] = {7.0F, 8.0F};
    float output[4] = {};

    EXPECT_EQ(buffer.write(input, 2), 0U);
    EXPECT_EQ(buffer.read(output, 4), 2U);
    EXPECT_TRUE(output[0] == 7.0F);
    EXPECT_TRUE(output[1] == 8.0F);
    EXPECT_EQ(buffer.underrunCount(), 2U);
    EXPECT_EQ(buffer.available(), 0U);
}

TEST(AudioRingBuffer, EmptyReadReturnsNoFramesAndCountsUnderrun) {
    AudioRingBuffer buffer(4);
    float output[3] = {9.0F, 9.0F, 9.0F};

    EXPECT_EQ(buffer.read(output, 3), 0U);
    EXPECT_EQ(buffer.underrunCount(), 3U);
    EXPECT_EQ(buffer.available(), 0U);
    EXPECT_TRUE(output[0] == 9.0F);
    EXPECT_TRUE(output[1] == 9.0F);
    EXPECT_TRUE(output[2] == 9.0F);
}

TEST(AudioRingBuffer, ConstructionRequiresNonzeroPowerOfTwoCapacity) {
    EXPECT_THROW(AudioRingBuffer(0), std::invalid_argument);
    EXPECT_THROW(AudioRingBuffer(3), std::invalid_argument);
    EXPECT_EQ(AudioRingBuffer(1).capacity(), 1U);
    EXPECT_EQ(AudioRingBuffer(16).capacity(), 16U);
}

TEST(AudioRingBuffer, ResetClearsQueuedFramesAndCounters) {
    AudioRingBuffer buffer(4);
    const float input[] = {1.0F, 2.0F, 3.0F, 4.0F, 5.0F};
    float output[6] = {};

    EXPECT_EQ(buffer.write(input, 5), 1U);
    EXPECT_EQ(buffer.read(output, 6), 4U);
    EXPECT_EQ(buffer.overflowCount(), 1U);
    EXPECT_EQ(buffer.underrunCount(), 2U);

    buffer.reset();
    EXPECT_EQ(buffer.available(), 0U);
    EXPECT_EQ(buffer.overflowCount(), 0U);
    EXPECT_EQ(buffer.underrunCount(), 0U);
}
