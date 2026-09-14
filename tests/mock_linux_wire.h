#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct MockLinuxWireState
{
    int openCalls = 0;
    int closeCalls = 0;
    std::string lastDevicePath;
    int setSlaveCalls = 0;
    uint8_t lastSetSlaveAddr = 0;
    int writeCalls = 0;
    std::vector<uint8_t> lastWriteBuffer;
    bool lastWriteWasIoctl = false;
    uint8_t lastWriteSlaveAddr = 0;
    int logErrors = 1;
    int setErrorLoggingCalls = 0;
    int setTimeoutCalls = 0;
    uint32_t lastTimeoutUs = 0;
    int readCalls = 0;
    std::vector<uint8_t> lastReadBuffer;
    int ioctlReadCalls = 0;
    int probeCalls = 0;
    uint8_t lastProbeAddr = 0;
    uint16_t lastIoctlAddr = 0;
    std::vector<uint8_t> lastIoctlInternal;
};

void mockLinuxWireReset();
void mockLinuxWireSetReadData(const std::vector<uint8_t> &data);
void mockLinuxWireSetIoctlReadData(const std::vector<uint8_t> &data);
void mockLinuxWireForceReadError(int err);
void mockLinuxWireClearReadError();
void mockLinuxWireForceSetSlaveError(int err);
void mockLinuxWireClearSetSlaveError();
void mockLinuxWireForceWriteError(int err);
void mockLinuxWireClearWriteError();
/* lw_probe result: 0 ACK (default), 1 driver-owned, -1 with errno. */
void mockLinuxWireSetProbeResult(int result, int err);
const MockLinuxWireState &mockLinuxWireState();
