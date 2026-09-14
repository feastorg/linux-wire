#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Wire.h"
#include "mock_linux_wire.h"

static void testPlainReadUsesRead()
{
    mockLinuxWireReset();
    mockLinuxWireSetReadData({0x11, 0x22});

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    uint8_t count = tw.requestFrom(static_cast<uint8_t>(0x20), static_cast<uint8_t>(2));
    assert(count == 2);
    assert(tw.available() == 2);
    assert(tw.read() == 0x11);
    assert(tw.read() == 0x22);

    const auto &state = mockLinuxWireState();
    assert(state.readCalls == 1);
    assert(state.ioctlReadCalls == 0);

    tw.end();
}

static void testRepeatedStartUsesIoctl()
{
    mockLinuxWireReset();
    mockLinuxWireSetIoctlReadData({0xAB});

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    tw.beginTransmission(0x50);
    assert(tw.write(0x10) == 1);
    assert(tw.endTransmission(false) == 0);

    uint8_t count = tw.requestFrom(static_cast<uint8_t>(0x50), static_cast<uint8_t>(1));
    assert(count == 1);
    assert(tw.read() == 0xAB);

    const auto &state = mockLinuxWireState();
    assert(state.writeCalls == 0); // sendStop=false should defer write
    assert(state.ioctlReadCalls == 1);
    assert(state.lastIoctlAddr == 0x50);
    assert(state.lastIoctlInternal.size() == 1);
    assert(state.lastIoctlInternal[0] == 0x10);

    tw.end();
}

static void testInternalAddressClamp()
{
    mockLinuxWireReset();
    mockLinuxWireSetIoctlReadData({0x01, 0x02});

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    uint8_t count = tw.requestFrom(static_cast<uint8_t>(0x40),
                                   static_cast<uint8_t>(2),
                                   0x12345678,
                                   static_cast<uint8_t>(6),
                                   static_cast<uint8_t>(1));
    assert(count == 2);
    assert(tw.read() == 0x01);
    assert(tw.read() == 0x02);

    const auto &state = mockLinuxWireState();
    assert(state.ioctlReadCalls == 1);
    assert(state.lastIoctlInternal.size() == 4);
    assert(state.lastIoctlInternal[0] == 0x12);
    assert(state.lastIoctlInternal[1] == 0x34);
    assert(state.lastIoctlInternal[2] == 0x56);
    assert(state.lastIoctlInternal[3] == 0x78);

    tw.end();
}

static void testTimeoutFlagOnReadFailure()
{
    mockLinuxWireReset();

    TwoWire tw;
    tw.setErrorLogging(false);
    tw.setWireTimeout(1000, true);
    tw.begin("/dev/i2c-mock");

    assert(mockLinuxWireState().logErrors == 0);
    assert(mockLinuxWireState().lastTimeoutUs == 1000);

    mockLinuxWireForceReadError(ETIMEDOUT);

    uint8_t count = tw.requestFrom(static_cast<uint8_t>(0x30), static_cast<uint8_t>(1));
    assert(count == 0);
    assert(tw.getWireTimeoutFlag());

    const auto &state = mockLinuxWireState();
    assert(state.openCalls == 2); // initial begin + reopen after timeout
    assert(state.logErrors == 0);
    assert(state.lastTimeoutUs == 1000);

    mockLinuxWireClearReadError();
    tw.end();
}

static void testDeferredWriteFlushes()
{
    mockLinuxWireReset();

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    tw.beginTransmission(static_cast<uint8_t>(0x22));
    tw.write(static_cast<uint8_t>(0x55));
    assert(tw.endTransmission(false) == 0);

    // No requestFrom; starting a new transmission should flush pending data.
    tw.beginTransmission(static_cast<uint8_t>(0x33));

    const auto &state = mockLinuxWireState();
    assert(state.writeCalls == 1);
    assert(state.lastSetSlaveAddr == 0x22);
    assert(state.lastWriteBuffer.size() == 1);
    assert(state.lastWriteBuffer[0] == 0x55);

    tw.endTransmission();
    tw.end();
}

static void testInternalAddressRequestFlushesPendingWrite()
{
    mockLinuxWireReset();
    mockLinuxWireSetIoctlReadData({0x77});

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    tw.beginTransmission(static_cast<uint8_t>(0x10));
    tw.write(static_cast<uint8_t>(0xAA));
    assert(tw.endTransmission(false) == 0);

    uint8_t count = tw.requestFrom(static_cast<uint8_t>(0x20),
                                   static_cast<uint8_t>(1),
                                   static_cast<uint32_t>(0x05),
                                   static_cast<uint8_t>(1),
                                   static_cast<uint8_t>(1));
    assert(count == 1);
    assert(tw.read() == 0x77);

    const auto &state = mockLinuxWireState();
    assert(state.writeCalls == 1);
    assert(!state.lastWriteWasIoctl);
    assert(state.lastWriteSlaveAddr == 0x10);
    assert(state.ioctlReadCalls == 1);
    assert(state.lastIoctlAddr == 0x20);
    assert(state.lastIoctlInternal.size() == 1);
    assert(state.lastIoctlInternal[0] == 0x05);

    tw.end();
}

static void testDeferredWriteFlushFailureBlocksRequestFrom()
{
    mockLinuxWireReset();

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    tw.beginTransmission(static_cast<uint8_t>(0x10));
    tw.write(static_cast<uint8_t>(0xAA));
    assert(tw.endTransmission(false) == 0);

    mockLinuxWireForceSetSlaveError(ENXIO);
    uint8_t count = tw.requestFrom(static_cast<uint8_t>(0x20), static_cast<uint8_t>(1));
    assert(count == 0);
    assert(tw.available() == 0);

    const auto &state = mockLinuxWireState();
    assert(state.setSlaveCalls == 1);
    assert(state.readCalls == 0);
    assert(state.writeCalls == 0);

    mockLinuxWireClearSetSlaveError();
    tw.beginTransmission(static_cast<uint8_t>(0x20));
    assert(tw.write(static_cast<uint8_t>(0x01)) == 1);
    assert(tw.endTransmission() == 0);

    tw.end();
}

static void testDeferredWriteFlushFailureBlocksNewTransmission()
{
    mockLinuxWireReset();

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    tw.beginTransmission(static_cast<uint8_t>(0x22));
    tw.write(static_cast<uint8_t>(0x55));
    assert(tw.endTransmission(false) == 0);

    mockLinuxWireForceWriteError(EIO);
    tw.beginTransmission(static_cast<uint8_t>(0x33));
    assert(tw.write(static_cast<uint8_t>(0x66)) == 0);
    assert(tw.endTransmission() == 4);

    const auto &state = mockLinuxWireState();
    assert(state.writeCalls == 1);
    assert(state.lastWriteSlaveAddr == 0x22);

    mockLinuxWireClearWriteError();
    tw.beginTransmission(static_cast<uint8_t>(0x33));
    assert(tw.write(static_cast<uint8_t>(0x66)) == 1);
    assert(tw.endTransmission() == 0);

    tw.end();
}

static void testEmptyTransmissionProbesInsteadOfWriting()
{
    /* An empty endTransmission() is the Arduino idiom for "is anyone at this
       address?". lw_write cannot send zero bytes, so it must use lw_probe. */
    mockLinuxWireReset();

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    tw.beginTransmission(static_cast<uint8_t>(0x40));
    assert(tw.endTransmission() == 0); /* ACK */
    {
        const auto &state = mockLinuxWireState();
        assert(state.probeCalls == 1);
        assert(state.lastProbeAddr == 0x40);
        assert(state.writeCalls == 0);
    }

    mockLinuxWireSetProbeResult(1, 0); /* owned by a kernel driver: present */
    tw.beginTransmission(static_cast<uint8_t>(0x68));
    assert(tw.endTransmission() == 0);

    mockLinuxWireSetProbeResult(-1, ENXIO); /* NACK on address */
    tw.beginTransmission(static_cast<uint8_t>(0x41));
    assert(tw.endTransmission() == 2);

    mockLinuxWireSetProbeResult(-1, EOPNOTSUPP); /* adapter cannot probe */
    tw.beginTransmission(static_cast<uint8_t>(0x42));
    assert(tw.endTransmission() == 4);

    /* A transmission with data still goes through lw_write, not lw_probe. */
    mockLinuxWireSetProbeResult(0, 0);
    tw.beginTransmission(static_cast<uint8_t>(0x43));
    tw.write(static_cast<uint8_t>(0x01));
    assert(tw.endTransmission() == 0);
    {
        const auto &state = mockLinuxWireState();
        assert(state.probeCalls == 4);
        assert(state.writeCalls == 1);
    }

    tw.end();
}

static void testTxBufferOverflow()
{
    mockLinuxWireReset();

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    tw.beginTransmission(static_cast<uint8_t>(0x40));
    for (int i = 0; i < LINUX_WIRE_BUFFER_LENGTH; ++i)
    {
        assert(tw.write(static_cast<uint8_t>(i)) == 1);
    }
    // One more byte should be rejected.
    assert(tw.write(0xFF) == 0);
    assert(tw.endTransmission() == 0);

    tw.beginTransmission(static_cast<uint8_t>(0x40));
    for (int i = 0; i < LINUX_WIRE_BUFFER_LENGTH + 1; ++i)
    {
        size_t wrote = tw.write(static_cast<uint8_t>(i));
        if (i < LINUX_WIRE_BUFFER_LENGTH)
        {
            assert(wrote == 1);
        }
        else
        {
            assert(wrote == 0);
        }
    }
    assert(tw.endTransmission() == 0);

    tw.end();
}

static void testFlushOnDifferentAddress()
{
    mockLinuxWireReset();

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    tw.beginTransmission(static_cast<uint8_t>(0x10));
    tw.write(static_cast<uint8_t>(0xAA));
    assert(tw.endTransmission(false) == 0);

    // Requesting from a different address should flush the pending write.
    tw.requestFrom(static_cast<uint8_t>(0x20), static_cast<uint8_t>(1));

    const auto &state = mockLinuxWireState();
    assert(state.writeCalls == 1);
    assert(!state.lastWriteWasIoctl);
    assert(state.lastWriteSlaveAddr == 0x10);
    assert(state.lastWriteBuffer.size() == 1);
    assert(state.lastWriteBuffer[0] == 0xAA);

    tw.end();
}

static void testZeroInternalAddressFallback()
{
    mockLinuxWireReset();
    mockLinuxWireSetReadData({0x99});

    TwoWire tw;
    tw.begin("/dev/i2c-mock");

    uint8_t count = tw.requestFrom(static_cast<uint8_t>(0x33),
                                   static_cast<uint8_t>(1),
                                   0,
                                   static_cast<uint8_t>(0),
                                   static_cast<uint8_t>(1));
    assert(count == 1);
    assert(tw.read() == 0x99);

    const auto &state = mockLinuxWireState();
    assert(state.readCalls == 1);
    assert(state.ioctlReadCalls == 0);

    tw.end();
}

static void testErrorLoggingToggle()
{
    mockLinuxWireReset();

    TwoWire tw;
    tw.begin("/dev/i2c-mock");
    assert(mockLinuxWireState().logErrors == 1);

    tw.setErrorLogging(false);
    assert(mockLinuxWireState().logErrors == 0);

    tw.setErrorLogging(true);
    assert(mockLinuxWireState().logErrors == 1);

    tw.end();
}

int main()
{
    testPlainReadUsesRead();
    testRepeatedStartUsesIoctl();
    testInternalAddressClamp();
    testInternalAddressRequestFlushesPendingWrite();
    testTimeoutFlagOnReadFailure();
    testDeferredWriteFlushes();
    testDeferredWriteFlushFailureBlocksRequestFrom();
    testDeferredWriteFlushFailureBlocksNewTransmission();
    testEmptyTransmissionProbesInsteadOfWriting();
    testTxBufferOverflow();
    testFlushOnDifferentAddress();
    testZeroInternalAddressFallback();
    testErrorLoggingToggle();

    std::puts("linux_wire tests passed");
    return 0;
}
