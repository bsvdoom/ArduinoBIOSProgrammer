#include "xmodem.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kPayloadSize = XMODEM_BLOCK_SIZE;
constexpr std::size_t kPacketSize = 3 + kPayloadSize + 2;

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class FakeStream final : public Stream {
 public:
  void push_input(std::uint8_t value) { input_.push_back(value); }

  void push_input(const std::vector<std::uint8_t>& values) {
    input_.insert(input_.end(), values.begin(), values.end());
  }

  void respond_after(std::size_t written_bytes, std::uint8_t value) {
    responses_.push_back({written_bytes, value});
    std::sort(responses_.begin(), responses_.end());
  }

  const std::vector<std::uint8_t>& output() const { return output_; }

  int available() override { return input_.empty() ? 0 : 1; }

  int read() override {
    if (input_.empty()) {
      return -1;
    }
    const std::uint8_t value = input_.front();
    input_.pop_front();
    return value;
  }

  std::size_t write(std::uint8_t value) override {
    output_.push_back(value);
    while (next_response_ < responses_.size() &&
           responses_[next_response_].first == output_.size()) {
      input_.push_back(responses_[next_response_].second);
      ++next_response_;
    }
    return 1;
  }

 private:
  std::deque<std::uint8_t> input_;
  std::vector<std::uint8_t> output_;
  std::vector<std::pair<std::size_t, std::uint8_t>> responses_;
  std::size_t next_response_ = 0;
};

std::uint16_t xmodem_crc(const std::uint8_t* data, std::size_t size) {
  std::uint16_t crc = 0;
  for (std::size_t index = 0; index < size; ++index) {
    crc ^= static_cast<std::uint16_t>(data[index]) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000U) != 0
                ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021U)
                : static_cast<std::uint16_t>(crc << 1);
    }
  }
  return crc;
}

std::vector<std::uint8_t> receive_packet(
    std::uint8_t sequence, const std::array<std::uint8_t, kPayloadSize>& payload) {
  std::vector<std::uint8_t> packet;
  packet.reserve(kPacketSize);
  packet.push_back(XMODEM_SOH);
  packet.push_back(sequence);
  packet.push_back(static_cast<std::uint8_t>(0xffU - sequence));
  packet.insert(packet.end(), payload.begin(), payload.end());
  const std::uint16_t crc = xmodem_crc(payload.data(), payload.size());
  packet.push_back(static_cast<std::uint8_t>(crc >> 8));
  packet.push_back(static_cast<std::uint8_t>(crc));
  return packet;
}

std::vector<std::uint8_t> decoded_payload(
    const std::vector<std::uint8_t>& wire, std::size_t packet_count) {
  require(wire.size() == packet_count * kPacketSize,
          "unexpected XMODEM wire length");
  std::vector<std::uint8_t> result;
  result.reserve(packet_count * kPayloadSize);
  for (std::size_t packet = 0; packet < packet_count; ++packet) {
    const std::size_t offset = packet * kPacketSize;
    require(wire[offset] == XMODEM_SOH, "packet does not start with SOH");
    require(static_cast<std::uint8_t>(wire[offset + 1] + wire[offset + 2]) ==
                0xffU,
            "invalid sequence complement");
    result.insert(result.end(), wire.begin() + offset + 3,
                  wire.begin() + offset + 3 + kPayloadSize);
  }
  return result;
}

std::vector<std::uint8_t> send_payload(
    const std::vector<std::uint8_t>& data) {
  require(!data.empty() && data.size() % kPayloadSize == 0,
          "test payload must contain whole XMODEM blocks");
  FakeStream stream;
  const std::size_t blocks = data.size() / kPayloadSize;
  for (std::size_t block = 1; block <= blocks; ++block) {
    stream.respond_after(block * kPacketSize, XMODEM_ACK);
  }
  XModem modem(stream);
  for (std::size_t block = 0; block < blocks; ++block) {
    auto* input = const_cast<std::uint8_t*>(data.data() + block * kPayloadSize);
    require(modem.block_send(input) == XModem::NEXT,
            "block_send did not accept ACK");
  }
  return decoded_payload(stream.output(), blocks);
}

void test_send_short_byte_order() {
  std::vector<std::uint8_t> input(kPayloadSize, 0xa5U);
  input[0] = 0x12U;
  input[1] = 0xafU;
  input[2] = 0x34U;
  input[3] = 0x56U;
  require(send_payload(input) == input,
          "12 AF 34 56 changed on the XMODEM wire");
}

void test_send_00_ff() {
  std::vector<std::uint8_t> input(256);
  for (std::size_t index = 0; index < input.size(); ++index) {
    input[index] = static_cast<std::uint8_t>(index);
  }
  require(send_payload(input) == input,
          "00 01 ... FF changed on the XMODEM wire");
}

void test_send_long_pattern() {
  std::vector<std::uint8_t> input(3 * kPayloadSize);
  for (std::size_t index = 0; index < input.size(); ++index) {
    input[index] = static_cast<std::uint8_t>((index * 37U + 11U) & 0xffU);
  }
  require(send_payload(input) == input,
          "multi-block deterministic pattern changed");
}

void test_exact_128_boundary() {
  std::vector<std::uint8_t> input(kPayloadSize);
  for (std::size_t index = 0; index < input.size(); ++index) {
    input[index] = static_cast<std::uint8_t>(255U - index);
  }
  require(send_payload(input) == input, "128-byte boundary changed data");
}

void test_multiple_blocks() {
  std::vector<std::uint8_t> input(4 * kPayloadSize);
  for (std::size_t index = 0; index < input.size(); ++index) {
    input[index] = static_cast<std::uint8_t>((index ^ (index >> 3)) & 0xffU);
  }
  require(send_payload(input) == input,
          "successive blocks were not preserved");
}

void test_block_number_rollover() {
  FakeStream stream;
  for (std::size_t packet = 1; packet <= 256; ++packet) {
    stream.respond_after(packet * kPacketSize, XMODEM_ACK);
  }
  XModem modem(stream);
  std::array<std::uint8_t, kPayloadSize> payload{};
  for (std::size_t packet = 0; packet < 256; ++packet) {
    payload[0] = static_cast<std::uint8_t>(packet);
    require(modem.block_send(payload.data()) == XModem::NEXT,
            "ACK failed during rollover test");
  }
  const auto& wire = stream.output();
  require(wire[253 * kPacketSize + 1] == 254U,
          "block 254 has the wrong sequence");
  require(wire[254 * kPacketSize + 1] == 255U,
          "block 255 has the wrong sequence");
  require(wire[255 * kPacketSize + 1] == 0U,
          "block sequence did not roll from 255 to 0");
}

void test_start_and_ack_handling() {
  FakeStream start_stream;
  start_stream.push_input(XSTART);
  XModem starter(start_stream);
  require(starter.start_send() == XModem::START,
          "start_send did not accept CRC-mode request");

  FakeStream stream;
  stream.respond_after(kPacketSize, XMODEM_ACK);
  XModem modem(stream);
  std::array<std::uint8_t, kPayloadSize> payload{};
  require(modem.block_send(payload.data()) == XModem::NEXT,
          "block_send did not accept ACK");
}

void test_eot_handling() {
  FakeStream send_stream;
  send_stream.respond_after(1, XMODEM_ACK);
  send_stream.respond_after(2, XMODEM_ACK);
  XModem sender(send_stream);
  require(sender.finish_send() == XModem::END,
          "finish_send did not finish after EOT/ACK");
  require(send_stream.output() ==
              std::vector<std::uint8_t>({XMODEM_EOT, XMODEM_ETB}),
          "finish_send emitted an unexpected terminator sequence");

  FakeStream receive_stream;
  receive_stream.push_input(XMODEM_EOT);
  XModem receiver(receive_stream);
  std::array<std::uint8_t, kPayloadSize> payload{};
  require(receiver.block_receive(payload.data()) == XModem::END,
          "block_receive did not accept EOT");
  require(receive_stream.output() ==
              std::vector<std::uint8_t>({XSTART, XMODEM_ACK}),
          "block_receive did not ACK EOT");
}

void test_can_handling() {
  FakeStream send_stream;
  send_stream.respond_after(4, XMODEM_CAN);
  XModem sender(send_stream);
  std::array<std::uint8_t, kPayloadSize> payload{};
  require(sender.block_send(payload.data()) == XModem::CANCEL,
          "block_send did not honor CAN");

  FakeStream receive_stream;
  receive_stream.push_input(XMODEM_CAN);
  XModem receiver(receive_stream);
  require(receiver.block_receive(payload.data()) == XModem::CANCEL,
          "block_receive did not honor CAN");
}

void test_timeout() {
  FakeStream stream;
  XModem modem(stream);
  modem.retries = 1;
  require(modem.start_send() == XModem::TIMEOUT,
          "start_send did not report timeout");
}

void test_duplicate_receive_block() {
  std::array<std::uint8_t, kPayloadSize> first{};
  std::array<std::uint8_t, kPayloadSize> second{};
  for (std::size_t index = 0; index < kPayloadSize; ++index) {
    first[index] = static_cast<std::uint8_t>(index);
    second[index] = static_cast<std::uint8_t>(255U - index);
  }

  FakeStream stream;
  stream.push_input(receive_packet(1, first));
  XModem modem(stream);
  std::array<std::uint8_t, kPayloadSize> destination{};
  require(modem.block_receive(destination.data()) == XModem::NEXT,
          "first receive block failed");
  require(destination == first, "first receive payload changed");

  stream.push_input(receive_packet(1, first));
  stream.push_input(receive_packet(2, second));
  destination.fill(0x5aU);
  require(modem.block_receive(destination.data()) == XModem::NEXT,
          "next block after duplicate failed");
  require(destination == second,
          "duplicate block was written or next payload changed");
  require(stream.output() ==
              std::vector<std::uint8_t>({XSTART, XMODEM_ACK, XMODEM_ACK}),
          "duplicate block did not produce the expected ACK sequence");
}

void test_nak_retry_identical_and_in_bounds() {
  FakeStream stream;
  stream.respond_after(kPacketSize, XMODEM_NACK);
  stream.respond_after(2 * kPacketSize, XMODEM_ACK);
  XModem modem(stream);
  auto payload = std::make_unique<std::uint8_t[]>(kPayloadSize);
  for (std::size_t index = 0; index < kPayloadSize; ++index) {
    payload[index] = static_cast<std::uint8_t>((index * 29U + 7U) & 0xffU);
  }

  require(modem.block_send(payload.get()) == XModem::NEXT,
          "retried block did not complete after NAK then ACK");
  const auto& wire = stream.output();
  require(wire.size() == 2 * kPacketSize,
          "retry did not emit exactly two packets");
  require(std::equal(wire.begin(), wire.begin() + kPacketSize,
                     wire.begin() + kPacketSize),
          "packet repeated after NAK is not byte-for-byte identical");
}

using Test = std::function<void()>;

const std::vector<std::pair<std::string, Test>>& tests() {
  static const std::vector<std::pair<std::string, Test>> all = {
      {"send_short_byte_order", test_send_short_byte_order},
      {"send_00_ff", test_send_00_ff},
      {"send_long_pattern", test_send_long_pattern},
      {"exact_128_boundary", test_exact_128_boundary},
      {"multiple_blocks", test_multiple_blocks},
      {"block_number_rollover", test_block_number_rollover},
      {"start_and_ack_handling", test_start_and_ack_handling},
      {"eot_handling", test_eot_handling},
      {"can_handling", test_can_handling},
      {"timeout", test_timeout},
      {"duplicate_receive_block", test_duplicate_receive_block},
      {"nak_retry_identical_and_in_bounds",
       test_nak_retry_identical_and_in_bounds},
  };
  return all;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: xmodem_characterization TEST_NAME\n";
    return 2;
  }
  const std::string requested = argv[1];
  const auto found = std::find_if(
      tests().begin(), tests().end(),
      [&requested](const auto& entry) { return entry.first == requested; });
  if (found == tests().end()) {
    std::cerr << "unknown test: " << requested << '\n';
    return 2;
  }
  try {
    found->second();
    std::cout << "PASS " << requested << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << requested << ": " << error.what() << '\n';
    return 1;
  }
}
