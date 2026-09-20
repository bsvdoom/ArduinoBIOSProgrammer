#include "programmer.h"
#include "xmodem.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void require(bool condition, const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}

// This peer releases each frame only after C or the previous frame's ACK.
class WritePeer final : public Stream {
 public:
  std::vector<Bytes> frames;
  std::vector<std::string> events;
  Bytes output;
  std::size_t next = 0;
  std::size_t position = 0;
  std::size_t complete_packets = 0;
  std::size_t data_acks = 0;
  std::size_t eot_acks = 0;
  bool started = false;

  int available() override {
    return started && next < frames.size() && position < frames[next].size();
  }

  int read() override {
    if (!available()) return -1;
    const auto value = frames[next][position++];
    if (position == frames[next].size()) {
      if (frames[next].size() == 133) {
        ++complete_packets;
        events.push_back("data " + std::to_string(complete_packets));
      } else if (frames[next] == Bytes{XMODEM_EOT}) {
        events.push_back("EOT");
      }
    }
    return value;
  }

  std::size_t write(std::uint8_t value) override {
    if (std::getenv("WRITE_TRACE")) std::cerr << "TRACE Serial.write " << unsigned(value) << " frame=" << next << " offset=" << position << '\n';
    output.push_back(value);
    if (value == XSTART) started = true;
    if (value == XMODEM_CAN) events.push_back("CAN");
    if (value == XMODEM_ACK && next < frames.size()) {
      require(next < frames.size() && position == frames[next].size(),
              "ACK before the complete incoming frame");
      if (frames[next] == Bytes{XMODEM_EOT}) {
        ++eot_acks;
        events.push_back("ACK EOT");
      } else {
        ++data_acks;
        events.push_back("ACK data " + std::to_string(data_acks));
      }
      ++next;
      position = 0;
    }
    return 1;
  }
};

struct PageWrite {
  std::uint32_t address;
  Bytes payload;
  std::size_t received_packets;
};

WritePeer peer;
std::uint32_t capacity = 512;
Bytes flash_memory;
std::vector<PageWrite> writes;

Bytes pattern(std::size_t size) {
  Bytes data(size);
  for (std::size_t i = 0; i < size; ++i)
    data[i] = static_cast<std::uint8_t>((i * 37 + (i >> 7) * 11) & 255);
  return data;
}

Bytes packet(std::uint8_t sequence, const std::uint8_t *payload) {
  Bytes frame{XMODEM_SOH, sequence, static_cast<std::uint8_t>(255 - sequence)};
  std::uint16_t crc = 0;
  for (std::size_t i = 0; i < XMODEM_BLOCK_SIZE; ++i) {
    frame.push_back(payload[i]);
    crc ^= static_cast<std::uint16_t>(payload[i]) << 8;
    for (int bit = 0; bit < 8; ++bit)
      crc = static_cast<std::uint16_t>((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
  }
  frame.push_back(static_cast<std::uint8_t>(crc >> 8));
  frame.push_back(static_cast<std::uint8_t>(crc));
  return frame;
}

std::vector<Bytes> transfer(std::size_t bytes, bool eot = true) {
  const auto input = pattern(bytes);
  std::vector<Bytes> frames;
  for (std::size_t offset = 0; offset < bytes; offset += XMODEM_BLOCK_SIZE)
    frames.push_back(packet(static_cast<std::uint8_t>(offset / XMODEM_BLOCK_SIZE + 1),
                            input.data() + offset));
  if (eot) frames.push_back({XMODEM_EOT});
  return frames;
}

std::string run_write(std::vector<Bytes> frames, std::uint32_t size = 512) {
  peer = WritePeer{};
  peer.frames = std::move(frames);
  capacity = size;
  flash_memory.assign(capacity, 0xa5);
  writes.clear();
  const auto close_file = [](FILE *file) { fclose(file); };
  std::unique_ptr<FILE, decltype(close_file)> console(tmpfile(), close_file);
  require(console != nullptr, "tmpfile failed");
  Programmer programmer(console.get());
  if (std::getenv("WRITE_TRACE")) std::cerr << "TRACE initialize\n";
  programmer.initialize();
  if (std::getenv("WRITE_TRACE")) std::cerr << "TRACE initialized\n";
  require(programmer.size == capacity, "recognized fake capacity was not used");
  require(fseek(console.get(), 0, SEEK_END) == 0, "cannot seek console");
  require(fputc('w', console.get()) == 'w', "cannot enqueue write command");
  require(fseek(console.get(), -1, SEEK_END) == 0, "cannot seek write command");
  if (std::getenv("WRITE_TRACE")) std::cerr << "TRACE task\n";
  programmer.task();
  if (std::getenv("WRITE_TRACE")) std::cerr << "TRACE task returned\n";
  if (std::getenv("WRITE_TRACE")) std::cerr << "TRACE flush\n";
  fflush(console.get());
  if (std::getenv("WRITE_TRACE")) std::cerr << "TRACE rewind\n";
  rewind(console.get());
  if (std::getenv("WRITE_TRACE")) std::cerr << "TRACE read log\n";
  std::string text;
  for (int c; (c = fgetc(console.get())) != EOF;) {
    require(text.size() < 4096, "console log exceeds test bound");
    text += static_cast<char>(c);
  }
  return text;
}

void expect_rejected(const std::string &text) {
  require(text.find("Write done") == std::string::npos, "invalid transfer reported success");
  require(text.find("Invalid file size") != std::string::npos ||
              text.find("Out of memory") != std::string::npos ||
              text.find("Timeout") != std::string::npos ||
              text.find("Cancelled") != std::string::npos,
          "invalid transfer lacks an explicit error");
}

void expect_pages(std::size_t byte_count) {
  require(writes.size() == byte_count / FLASH_PAGE_SIZE, "wrong number of page writes");
  auto expected = pattern(byte_count);
  expected.resize(capacity, 0xa5);
  require(flash_memory == expected, "flash bytes changed order or unwritten region was modified");
  for (std::size_t i = 0; i < writes.size(); ++i) {
    require(writes[i].address == i * FLASH_PAGE_SIZE, "wrong page address");
    require(writes[i].received_packets >= 2 * (i + 1),
            "page programmed before both complete XMODEM packets");
  }
}

void expect_early_eot(std::size_t bytes) {
  const auto text = run_write(transfer(bytes));
  expect_pages(bytes / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE);
  expect_rejected(text);
  require(peer.eot_acks == 0, "early EOT was acknowledged as successful termination");
  require(!peer.output.empty() && peer.output.back() == XMODEM_CAN,
          "early EOT did not cancel the sender");
}

void test_empty_silence() {
  const auto text = run_write({});
  expect_pages(0);
  expect_rejected(text);
}

void test_exact_capacity() {
  for (const auto size : {256U, 512U, 768U}) {
    const auto text = run_write(transfer(size), size);
    expect_pages(size);
    require(text.find("Write done") != std::string::npos, "exact capacity did not succeed");
    require(peer.next == peer.frames.size(), "transfer finished before all ACKs");
  }
}

void test_last_data_ack() {
  run_write(transfer(512));
  require(peer.data_acks == 4, "last data block has no ACK");
  const auto last = std::find(peer.events.begin(), peer.events.end(), "ACK data 4");
  const auto eot = std::find(peer.events.begin(), peer.events.end(), "EOT");
  require(last < eot && eot != peer.events.end(), "EOT was not read after the last data ACK");
}

void test_eot_ack() {
  run_write(transfer(512));
  require(peer.eot_acks == 1, "EOT has no ACK");
  require(peer.output == Bytes({XSTART, XMODEM_ACK, XMODEM_ACK, XMODEM_ACK,
                                XMODEM_ACK, XMODEM_ACK}),
          "unexpected successful transfer control sequence");
}

void test_oversized() {
  const auto text = run_write(transfer(640));
  expect_pages(512);
  expect_rejected(text);
  require(peer.complete_packets == 5, "extra data block was not inspected");
  require(peer.data_acks == 4 && peer.eot_acks == 0, "extra data was acknowledged");
  require(peer.output.back() == XMODEM_CAN, "oversized transfer did not cancel the sender");
}

void test_page_requires_two_blocks() {
  auto frames = transfer(128, false);
  frames.push_back({XMODEM_CAN});
  const auto text = run_write(frames);
  expect_pages(0);
  expect_rejected(text);
  run_write(transfer(512));
  expect_pages(512);
  const std::vector<std::string> expected{
      "data 1", "ACK data 1", "data 2", "write 0", "ACK data 2",
      "data 3", "ACK data 3", "data 4", "write 256", "ACK data 4",
      "EOT", "ACK EOT"};
  require(peer.events == expected, "page writes or finalization occur in the wrong order");
}

void test_missing_second_block() {
  const auto text = run_write(transfer(128, false));
  expect_pages(0);
  expect_rejected(text);
}

void test_missing_eot() {
  const auto text = run_write(transfer(512, false));
  expect_pages(512);
  expect_rejected(text);
  require(peer.data_acks >= 4, "last data block not ACKed before waiting for EOT");
  require(text.find("Timeout") != std::string::npos, "missing EOT did not time out");
  require(peer.eot_acks == 0, "ACK for nonexistent EOT");
}

void test_invalid_second_crc() {
  auto frames = transfer(256, false);
  frames.back().back() ^= 1;
  const auto text = run_write(frames);
  expect_pages(0);
  expect_rejected(text);
  require(peer.data_acks == 1, "bad CRC block was acknowledged");
}

void test_duplicate_final_block() {
  auto frames = transfer(512, false);
  frames.push_back(frames.back());
  frames.push_back({XMODEM_EOT});
  const auto text = run_write(frames);
  expect_pages(512);
  require(text.find("Write done") != std::string::npos, "duplicate final block broke termination");
  require(peer.data_acks == 5 && peer.eot_acks == 1, "duplicate/EOT ACK missing");
}

}  // namespace

// Link-time fake flash: real declarations and real Programmer/XModem code.
// The flash driver is intentionally replaced, not copied or modified.
Stream &Serial = peer;
SPIClass SPI;

void digitalWrite(unsigned char, int) { throw std::runtime_error("unexpected GPIO access"); }
void delay(unsigned long) {}
std::uint8_t SPIClass::transfer(std::uint8_t) {
  throw std::runtime_error("unexpected SPI access");
}

int vfprintf_P(FILE *file, const char *format, va_list args) {
  // AVR uint32_t is unsigned long; this host uses unsigned int. Adapt only
  // the console shim so the production size diagnostic has no varargs UB.
  static_assert(sizeof(std::uint32_t) == sizeof(unsigned int));
  std::string host_format(format);
  for (std::size_t pos = 0; (pos = host_format.find("%lu", pos)) != std::string::npos;)
    host_format.replace(pos, 3, "%u");
  // A real UART is duplex. A host update FILE needs an intervening seek
  // when switching from command input to diagnostic output.
  require(fseek(file, 0, SEEK_CUR) == 0, "cannot switch console to output");
  return vfprintf(file, host_format.c_str(), args);
}

bool winbondFlashSPI::begin(partNumberType, SPIClass &, std::uint8_t) { return true; }
long winbondFlashClass::bytes() { return capacity; }
bool winbondFlashClass::busy() { return false; }
void winbondFlashClass::transfer_addr(std::uint32_t) {
  throw std::runtime_error("unexpected direct flash addressing");
}
bool winbondFlashClass::setWriteEnable(bool enabled) {
  require(enabled, "unexpected write-disable sequence");
  return true;
}
bool winbondFlashClass::writePage(std::uint32_t address, std::uint8_t *data) {
  require(address % FLASH_PAGE_SIZE == 0 && address <= capacity &&
              FLASH_PAGE_SIZE <= capacity - address, "page write exceeds flash capacity");
  writes.push_back({address, Bytes(data, data + FLASH_PAGE_SIZE), peer.complete_packets});
  std::copy(data, data + FLASH_PAGE_SIZE, flash_memory.begin() + address);
  peer.events.push_back("write " + std::to_string(address));
  return true;
}
bool winbondFlashClass::eraseAll() { throw std::runtime_error("automatic erase attempted"); }
bool winbondFlashClass::read(std::uint32_t, std::uint8_t *, std::uint16_t) {
  throw std::runtime_error("automatic readback attempted");
}

int main(int argc, char **argv) {
  std::cout << std::unitbuf;
  const std::vector<std::pair<std::string, std::function<void()>>> tests{
      {"write_empty_silence", test_empty_silence},
      {"write_eot_before_first_block", [] { expect_early_eot(0); }},
      {"write_eot_after_half_page", [] { expect_early_eot(128); }},
      {"write_eot_after_full_page", [] { expect_early_eot(256); }},
      {"write_eot_after_page_and_half", [] { expect_early_eot(384); }},
      {"write_exact_capacity", test_exact_capacity},
      {"write_last_data_ack", test_last_data_ack},
      {"write_eot_ack", test_eot_ack},
      {"write_oversized", test_oversized},
      {"write_page_requires_two_blocks", test_page_requires_two_blocks},
      {"write_missing_second_block", test_missing_second_block},
      {"write_missing_eot", test_missing_eot},
      {"write_invalid_second_crc", test_invalid_second_crc},
      {"write_duplicate_final_block", test_duplicate_final_block},
  };
  if (argc != 2) return 2;
  if (std::string(argv[1]) == "--list") {
    for (const auto &test : tests) std::cout << test.first << '\n';
    return 0;
  }
  for (const auto &test : tests) {
    if (test.first != argv[1]) continue;
    std::cout << "START " << test.first << '\n';
    try {
      test.second();
      std::cout << "PASS " << test.first << '\n';
      return 0;
    } catch (const std::exception &error) {
      std::cout << "FAIL " << test.first << ": " << error.what() << '\n';
      return 1;
    }
  }
  return 2;
}
