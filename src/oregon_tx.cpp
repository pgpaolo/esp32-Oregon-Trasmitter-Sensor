#include "oregon_tx.h"
#include "app_config.h"
#include <math.h>

OregonTx::OregonTx(uint8_t pin) : pin_(pin), halfUs_(OSV3_HALF_US_DEFAULT) {}

void OregonTx::begin() {
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
}

void OregonTx::setHalfBitUs(uint16_t value) {
    halfUs_ = constrain(value, 430, 550);
}

void OregonTx::setInvert(bool value) { invert_ = value; }

void OregonTx::sendLevel(bool level) {
    digitalWrite(pin_, invert_ ? !level : level);
    delayMicroseconds(halfUs_);
}

void OregonTx::sendBit(bool bit) {
    // OSV3 Manchester, beta polarity selectable from Web UI.
    // 0 = LOW/HIGH, 1 = HIGH/LOW. Invert swaps the complete RF polarity.
    if (bit) {
        sendLevel(HIGH);
        sendLevel(LOW);
    } else {
        sendLevel(LOW);
        sendLevel(HIGH);
    }
}

void OregonTx::sendNibble(uint8_t nibble) {
    // Oregon nibbles are sent least-significant bit first.
    for (uint8_t b = 0; b < 4; ++b) sendBit((nibble >> b) & 0x01U);
}

void OregonTx::setNibble(OregonFrame &frame, uint8_t index, uint8_t value) {
    const uint8_t byteIndex = index / 2U;
    value &= 0x0F;
    if ((index & 1U) == 0U) frame.bytes[byteIndex] = (frame.bytes[byteIndex] & 0x0F) | (value << 4U);
    else frame.bytes[byteIndex] = (frame.bytes[byteIndex] & 0xF0) | value;
}

uint8_t OregonTx::getNibble(const OregonFrame &frame, uint8_t index) {
    const uint8_t b = frame.bytes[index / 2U];
    return (index & 1U) ? (b & 0x0F) : (b >> 4U);
}

void OregonTx::initCommon(OregonFrame &frame, uint8_t length, uint16_t sensorCode,
                          uint8_t channel, uint8_t rollingId, uint8_t flags) {
    frame = OregonFrame{};
    frame.length = length;
    // Decoder representation used by companion gateway contains sync nibble A at n0.
    setNibble(frame, 0, 0xA);
    setNibble(frame, 1, (sensorCode >> 12) & 0xF);
    setNibble(frame, 2, (sensorCode >> 8) & 0xF);
    setNibble(frame, 3, (sensorCode >> 4) & 0xF);
    setNibble(frame, 4, sensorCode & 0xF);
    const uint8_t ch = (channel == 1) ? 1 : (channel == 2 ? 2 : 4);
    setNibble(frame, 5, ch);
    setNibble(frame, 6, (rollingId >> 4) & 0xF);
    setNibble(frame, 7, rollingId & 0xF);
    setNibble(frame, 8, flags & 0xF);
}

void OregonTx::finalizeChecksum(OregonFrame &frame, uint8_t checksumPosition) {
    uint8_t sum = 0;
    for (uint8_t i = 1; i < checksumPosition; ++i) sum = static_cast<uint8_t>(sum + getNibble(frame, i));
    setNibble(frame, checksumPosition, sum & 0x0F);
    setNibble(frame, checksumPosition + 1U, (sum >> 4U) & 0x0F);
    frame.checksumNibble = checksumPosition;
}

bool OregonTx::buildTHGR810(OregonFrame &frame, uint8_t channel, uint8_t rollingId,
                            float temperatureC, float humidityPct, bool batteryLow) {
    if (!isfinite(temperatureC) || !isfinite(humidityPct)) return false;
    temperatureC = constrain(temperatureC, -50.0f, 70.0f);
    humidityPct = constrain(humidityPct, 2.0f, 98.0f);
    initCommon(frame, 9, 0xF824, channel, rollingId, batteryLow ? 0x4 : 0x0);

    const int t10 = lroundf(fabsf(temperatureC) * 10.0f);
    setNibble(frame, 9, t10 % 10);             // tenths
    setNibble(frame, 10, (t10 / 10) % 10);    // ones
    setNibble(frame, 11, (t10 / 100) % 10);   // tens
    setNibble(frame, 12, temperatureC < 0 ? 8 : 0);

    const int rh = constrain(static_cast<int>(lroundf(humidityPct)), 0, 99);
    setNibble(frame, 13, rh % 10);
    setNibble(frame, 14, (rh / 10) % 10);
    setNibble(frame, 15, 0);                   // humidity status/reserved
    finalizeChecksum(frame, 16);
    return true;
}

bool OregonTx::buildUVN800(OregonFrame &frame, uint8_t channel, uint8_t rollingId, float uvIndex) {
    if (!isfinite(uvIndex)) return false;
    // UVN800 payload is an unsigned integer. We intentionally keep flags clear in beta.
    initCommon(frame, 8, 0xD874, channel, rollingId, 0);
    const uint8_t uv = constrain(static_cast<int>(lroundf(uvIndex)), 0, 25);
    frame.bytes[4] = uv; // companion gateway decodes raw byte 4 for D874/EC70
    setNibble(frame, 10, 0);
    setNibble(frame, 11, 0);
    setNibble(frame, 12, 0);
    setNibble(frame, 13, 0);
    finalizeChecksum(frame, 14);
    return true;
}

bool OregonTx::buildPCR800(OregonFrame &frame, uint8_t channel, uint8_t rollingId,
                           float rainRateMmH, float rainTotalMm) {
    if (!isfinite(rainRateMmH) || !isfinite(rainTotalMm)) return false;
    initCommon(frame, 11, 0x2914, channel, rollingId, 0);
    const uint32_t rateRaw = constrain(static_cast<long>(lroundf(max(0.0f, rainRateMmH) / 0.254f)), 0L, 99999L);
    // Companion gateway uses 0.254 mm per raw total count as its current PCR800 compatibility convention.
    const uint32_t totalRaw = constrain(static_cast<long>(lroundf(max(0.0f, rainTotalMm) / 0.254f)), 0L, 999999L);

    uint32_t v = rateRaw;
    for (uint8_t i = 0; i < 5; ++i) { setNibble(frame, 8 + i, v % 10); v /= 10; }
    v = totalRaw;
    for (uint8_t i = 0; i < 6; ++i) { setNibble(frame, 13 + i, v % 10); v /= 10; }
    finalizeChecksum(frame, 19);
    setNibble(frame, 21, 0);
    return true;
}

bool OregonTx::buildWGR800(OregonFrame &frame, uint8_t channel, uint8_t rollingId,
                           float averageKmh, float gustKmh, uint8_t directionIndex,
                           bool batteryLow) {
    if (!isfinite(averageKmh) || !isfinite(gustKmh)) return false;
    initCommon(frame, 10, 0x1984, channel, rollingId, batteryLow ? 0x4 : 0x0);
    setNibble(frame, 9, directionIndex & 0x0F);
    setNibble(frame, 10, 0);
    setNibble(frame, 11, 0);

    const int gust10 = constrain(static_cast<int>(lroundf(max(0.0f, gustKmh) / 3.6f * 10.0f)), 0, 999);
    setNibble(frame, 12, gust10 % 10);
    setNibble(frame, 13, (gust10 / 10) % 10);
    setNibble(frame, 14, (gust10 / 100) % 10);

    const int avg10 = constrain(static_cast<int>(lroundf(max(0.0f, averageKmh) / 3.6f * 10.0f)), 0, 999);
    setNibble(frame, 15, avg10 % 10);
    setNibble(frame, 16, (avg10 / 10) % 10);
    setNibble(frame, 17, (avg10 / 100) % 10);
    finalizeChecksum(frame, 18);
    return true;
}

void OregonTx::sendFrameOnce(const OregonFrame &frame) {
    noInterrupts();
    // 24 logical 1s preamble then OSV3 sync 0101.
    for (uint16_t i = 0; i < OSV3_PREAMBLE_BITS; ++i) sendBit(true);
    sendBit(false); sendBit(true); sendBit(false); sendBit(true);

    // n0 is the decoder's synthetic sync nibble A; transmit payload from n1.
    const uint8_t maxNibble = frame.length * 2U;
    for (uint8_t n = 1; n < maxNibble; ++n) sendNibble(getNibble(frame, n));
    digitalWrite(pin_, invert_ ? HIGH : LOW);
    interrupts();
}

void OregonTx::transmit(const OregonFrame &frame, uint8_t repeats, uint16_t repeatGapMs) {
    repeats = constrain(repeats, static_cast<uint8_t>(1), static_cast<uint8_t>(4));
    for (uint8_t i = 0; i < repeats; ++i) {
        sendFrameOnce(frame);
        if (i + 1U < repeats) delay(repeatGapMs);
    }
}

String OregonTx::hex(const OregonFrame &frame) const {
    String out;
    for (uint8_t i = 0; i < frame.length; ++i) {
        if (i) out += ' ';
        if (frame.bytes[i] < 0x10) out += '0';
        out += String(frame.bytes[i], HEX);
    }
    out.toUpperCase();
    return out;
}
