#pragma once

#include <Arduino.h>

struct OregonFrame {
    uint8_t bytes[16]{};
    uint8_t length{0};
    uint8_t checksumNibble{0};
};

class OregonTx {
public:
    explicit OregonTx(uint8_t pin);
    void begin();
    void setHalfBitUs(uint16_t value);
    void setInvert(bool value);

    bool buildTHGR810(OregonFrame &frame, uint8_t channel, uint8_t rollingId,
                      float temperatureC, float humidityPct, bool batteryLow);
    bool buildUVN800(OregonFrame &frame, uint8_t channel, uint8_t rollingId,
                     float uvIndex);
    bool buildPCR800(OregonFrame &frame, uint8_t channel, uint8_t rollingId,
                     float rainRateMmH, float rainTotalMm);
    bool buildWGR800(OregonFrame &frame, uint8_t channel, uint8_t rollingId,
                     float averageKmh, float gustKmh, uint8_t directionIndex,
                     bool batteryLow);

    void transmit(const OregonFrame &frame, uint8_t repeats = 2, uint16_t repeatGapMs = 120);
    String hex(const OregonFrame &frame) const;

private:
    uint8_t pin_;
    uint16_t halfUs_{488};
    bool invert_{false};

    void sendLevel(bool level);
    void sendBit(bool bit);
    void sendNibble(uint8_t nibble);
    void sendFrameOnce(const OregonFrame &frame);
    static void setNibble(OregonFrame &frame, uint8_t index, uint8_t value);
    static uint8_t getNibble(const OregonFrame &frame, uint8_t index);
    static void initCommon(OregonFrame &frame, uint8_t length, uint16_t sensorCode,
                           uint8_t channel, uint8_t rollingId, uint8_t flags);
    static void finalizeChecksum(OregonFrame &frame, uint8_t checksumPosition);
};
