#include "core/AudioFormat.hpp"

#include <gtest/gtest.h>

#include <cstdint>

TEST(AudioFormat, SampleFormatNamesCoverEveryEnumValue) {
    EXPECT_EQ(sampleFormatName(SampleFormat::Int16), "Int16");
    EXPECT_EQ(sampleFormatName(SampleFormat::Int24), "Int24");
    EXPECT_EQ(sampleFormatName(SampleFormat::Int32), "Int32");
    EXPECT_EQ(sampleFormatName(SampleFormat::Float32), "Float32");
    EXPECT_EQ(sampleFormatName(SampleFormat::Float64), "Float64");
}

TEST(AudioFormat, ConversionProvenanceNamesCoverEveryEnumValue) {
    EXPECT_EQ(provenanceName(ConversionProvenance::Native), "Native");
    EXPECT_EQ(provenanceName(ConversionProvenance::Quantized), "Quantized");
    EXPECT_EQ(provenanceName(ConversionProvenance::Resampled), "Resampled");
    EXPECT_EQ(provenanceName(ConversionProvenance::Converted), "Converted");
}

TEST(AudioFormat, FormatFieldsRoundTripThroughValueCopy) {
    const AudioFormat original{48000, 2, SampleFormat::Float32};
    const AudioFormat copy = original;

    EXPECT_EQ(copy.sampleRate, 48000U);
    EXPECT_EQ(copy.channels, 2U);
    EXPECT_EQ(copy.sampleFormat, SampleFormat::Float32);
    EXPECT_EQ(copy, original);
}

TEST(AudioFormat, BytesPerSampleAndFrameReflectBitDepthAndChannels) {
    EXPECT_EQ(bytesPerSample(SampleFormat::Int16), 2);
    EXPECT_EQ(bytesPerSample(SampleFormat::Int24), 3);
    EXPECT_EQ(bytesPerSample(SampleFormat::Int32), 4);
    EXPECT_EQ(bytesPerSample(SampleFormat::Float32), 4);
    EXPECT_EQ(bytesPerSample(SampleFormat::Float64), 8);

    const AudioFormat stereoFloat{44100, 2, SampleFormat::Float32};
    const AudioFormat fiveChannelInt24{96000, 5, SampleFormat::Int24};
    EXPECT_EQ(bytesPerFrame(stereoFloat), 8);
    EXPECT_EQ(bytesPerFrame(fiveChannelInt24), 15);
}

TEST(AudioFormat, NativeAndConvertedProvenanceRemainDistinguishable) {
    const AudioFormat nativeFormat{48000, 2, SampleFormat::Float32};
    const AudioFormat convertedFormat{44100, 2, SampleFormat::Int16};
    const FormatConversion native{nativeFormat, nativeFormat};
    const FormatConversion converted{nativeFormat, convertedFormat,
                                     ConversionProvenance::Converted};

    EXPECT_TRUE(native.source == native.target);
    EXPECT_EQ(native.provenance, ConversionProvenance::Native);
    EXPECT_NE(converted.source, converted.target);
    EXPECT_EQ(converted.provenance, ConversionProvenance::Converted);
}

TEST(AudioFormat, InvalidEnumsAndZeroChannelsHaveSafeResults) {
    const auto invalidSampleFormat = static_cast<SampleFormat>(255U);
    const auto invalidProvenance = static_cast<ConversionProvenance>(255U);
    const AudioFormat zeroChannelFormat{48000, 0, SampleFormat::Int16};

    EXPECT_EQ(bytesPerSample(invalidSampleFormat), 0);
    EXPECT_EQ(sampleFormatName(invalidSampleFormat), "");
    EXPECT_EQ(provenanceName(invalidProvenance), "");
    EXPECT_EQ(bytesPerFrame(zeroChannelFormat), 0);
}
