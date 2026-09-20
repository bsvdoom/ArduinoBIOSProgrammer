#include "winbondflash.h"
#include "programmer.h"
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

class CharacterizationStream final : public Stream {
 public:
  void reset(std::vector<std::uint8_t> values = {}) {
    input = std::deque<std::uint8_t>(values.begin(), values.end());
    output.clear();
  }

  int available() override { return input.empty() ? 0 : 1; }
  int read() override {
    if (input.empty()) return -1;
    const auto value = input.front();
    input.pop_front();
    return value;
  }
  std::size_t write(std::uint8_t value) override {
    output.push_back(value);
    return 1;
  }

  std::deque<std::uint8_t> input;
  std::vector<std::uint8_t> output;
};

CharacterizationStream test_serial;
Stream &Serial = test_serial;
SPIClass SPI;

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint32_t kCapacity = 16UL * 1024UL * 1024UL;
constexpr std::uint8_t kChipSelect = SS;

void require(bool condition, const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}

struct Transaction {
  Bytes sent;
  Bytes received;
};

struct FakeSpiBus {
  std::array<std::uint8_t, 3> jedec{{0xef, 0x40, 0x18}};
  std::array<std::uint8_t, 8> unique_id{{1, 2, 3, 4, 5, 6, 7, 8}};
  std::deque<std::uint8_t> status1;
  std::uint8_t final_status1 = 0;
  std::uint8_t status2 = 0;
  bool selected = false;
  bool spi_started = false;
  bool spi_ended = false;
  std::uint8_t bit_order = 0xff;
  std::uint8_t clock_divider = 0xff;
  std::uint8_t data_mode = 0xff;
  std::vector<std::pair<std::uint8_t, int>> pin_modes;
  std::vector<int> chip_select_calls;
  std::vector<std::string> events;
  std::vector<unsigned int> microsecond_delays;
  std::vector<Transaction> transactions;
  Transaction current;
  std::size_t transaction_count = 0;
  std::size_t log_limit = 1024;
  std::uint32_t now_ms = 0;
  std::uint32_t millis_step = 1;
  std::size_t millis_calls = 0;

  void reset() { *this = FakeSpiBus{}; }

  void clear_trace() {
    chip_select_calls.clear();
    events.clear();
    microsecond_delays.clear();
    transactions.clear();
    current = {};
    transaction_count = 0;
    require(!selected, "cannot clear trace while /CS is LOW");
  }

  std::uint8_t memory_byte(std::uint32_t address) const {
    return static_cast<std::uint8_t>((address * 37U + 11U) & 0xffU);
  }

  void select() {
    require(!selected, "nested /CS LOW");
    selected = true;
    current = {};
    chip_select_calls.push_back(LOW);
  }

  void deselect() {
    chip_select_calls.push_back(HIGH);
    if (!selected) return;
    selected = false;
    ++transaction_count;
    if (transactions.size() < log_limit) transactions.push_back(current);
    current = {};
  }

  std::uint8_t response_for(std::uint8_t value) {
    require(selected, "SPI transfer while /CS is HIGH");
    const std::size_t index = current.sent.size();
    const std::uint8_t command = index == 0 ? value : current.sent.front();
    if (command == 0x9f && index >= 1 && index <= 3) return jedec[index - 1];
    if (command == 0x05 && index == 1) {
      if (status1.empty()) return final_status1;
      const auto result = status1.front();
      status1.pop_front();
      return result;
    }
    if (command == 0x35 && index == 1) return status2;
    if (command == 0x4b && index >= 5 && index <= 12)
      return unique_id[index - 5];
    if (command == 0x03 && index >= 4) {
      const std::uint32_t address =
          (static_cast<std::uint32_t>(current.sent[1]) << 16) |
          (static_cast<std::uint32_t>(current.sent[2]) << 8) |
          current.sent[3];
      return memory_byte((address + static_cast<std::uint32_t>(index - 4)) &
                         0x00ffffffUL);
    }
    return 0xff;
  }

  std::uint8_t transfer(std::uint8_t value) {
    const auto result = response_for(value);
    if (current.sent.size() < 300) {
      current.sent.push_back(value);
      current.received.push_back(result);
    }
    return result;
  }
};

FakeSpiBus bus;
std::uint8_t expected_chip_select = kChipSelect;

void require_transactions(const std::vector<Bytes> &expected) {
  require(!bus.selected, "/CS remained LOW");
  require(bus.transactions.size() == expected.size(),
          "unexpected SPI transaction count");
  require(bus.transaction_count == expected.size(),
          "not every transaction was logged");
  for (std::size_t i = 0; i < expected.size(); ++i)
    require(bus.transactions[i].sent == expected[i],
            "transaction " + std::to_string(i) + " has unexpected bytes");
  require(std::count(bus.chip_select_calls.begin(), bus.chip_select_calls.end(), LOW) ==
              static_cast<long>(expected.size()),
          "a transaction lacks a /CS LOW edge");
  require(std::count(bus.chip_select_calls.begin(), bus.chip_select_calls.end(), HIGH) >=
              static_cast<long>(expected.size()),
          "a transaction lacks a /CS HIGH boundary");
}

void require_balanced_chip_select() {
  require(!bus.selected, "/CS remained LOW after the test");
  require(std::count(bus.chip_select_calls.begin(), bus.chip_select_calls.end(), LOW) ==
              static_cast<long>(bus.transaction_count),
          "a logged transaction lacks exactly one /CS LOW edge");
  require(std::count(bus.chip_select_calls.begin(), bus.chip_select_calls.end(), HIGH) >=
              static_cast<long>(bus.transaction_count),
          "a logged transaction lacks a /CS HIGH boundary");
}

winbondFlashSPI initialized_flash() {
  bus.reset();
  winbondFlashSPI flash;
  require(flash.begin(winbondFlashClass::autoDetect, SPI, kChipSelect),
          "EF 40 18 initialization failed");
  bus.clear_trace();
  return flash;
}

void require_cs_initialized_before_transfer(std::uint8_t pin) {
  const auto output = std::find(
      bus.events.begin(), bus.events.end(),
      "pinMode:" + std::to_string(pin) + ":" + std::to_string(OUTPUT));
  const auto high = std::find(
      bus.events.begin(), bus.events.end(),
      "digitalWrite:" + std::to_string(pin) + ":" + std::to_string(HIGH));
  const auto transfer = std::find(bus.events.begin(), bus.events.end(), "transfer");
  require(output != bus.events.end() && high != bus.events.end() &&
              transfer != bus.events.end() && output < high && high < transfer,
          "CS OUTPUT/HIGH did not precede the first SPI transfer");
}

Bytes read_command(std::uint32_t address, std::size_t count) {
  Bytes command{0x03, static_cast<std::uint8_t>(address >> 16),
                static_cast<std::uint8_t>(address >> 8),
                static_cast<std::uint8_t>(address)};
  command.resize(4 + count, 0x00);
  return command;
}

Bytes page_program(std::uint32_t address, const std::array<std::uint8_t, 256> &data) {
  Bytes command{0x02, static_cast<std::uint8_t>(address >> 16),
                static_cast<std::uint8_t>(address >> 8), 0x00};
  command.insert(command.end(), data.begin(), data.end());
  return command;
}

Bytes erase_command(std::uint8_t command, std::uint32_t address) {
  return Bytes{command, static_cast<std::uint8_t>(address >> 16),
               static_cast<std::uint8_t>(address >> 8),
               static_cast<std::uint8_t>(address)};
}

std::array<std::uint8_t, 256> page_pattern() {
  std::array<std::uint8_t, 256> data{};
  for (std::size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<std::uint8_t>((i * 29U + 7U) & 0xffU);
  return data;
}

Bytes xmodem_packet(std::uint8_t sequence, std::uint8_t seed) {
  Bytes packet{XMODEM_SOH, sequence,
               static_cast<std::uint8_t>(0xffU - sequence)};
  std::uint16_t crc = 0;
  for (std::size_t i = 0; i < XMODEM_BLOCK_SIZE; ++i) {
    const auto value = static_cast<std::uint8_t>(seed + i);
    packet.push_back(value);
    crc ^= static_cast<std::uint16_t>(value) << 8;
    for (int bit = 0; bit < 8; ++bit)
      crc = static_cast<std::uint16_t>(
          (crc & 0x8000U) ? (crc << 1) ^ 0x1021U : crc << 1);
  }
  packet.push_back(static_cast<std::uint8_t>(crc >> 8));
  packet.push_back(static_cast<std::uint8_t>(crc));
  return packet;
}

Bytes one_page_xmodem_input() {
  auto input = xmodem_packet(1, 0x10);
  const auto second = xmodem_packet(2, 0x90);
  input.insert(input.end(), second.begin(), second.end());
  return input;
}

bool sent_command(std::uint8_t command) {
  return std::any_of(bus.transactions.begin(), bus.transactions.end(),
                     [command](const Transaction &transaction) {
                       return !transaction.sent.empty() &&
                              transaction.sent.front() == command;
                     });
}

std::string run_programmer_command(char command, Bytes serial_input,
                                   const std::function<void()> &configure) {
  bus.reset();
  expected_chip_select = kChipSelect;
  test_serial.reset();
  const auto close_file = [](FILE *file) { fclose(file); };
  std::unique_ptr<FILE, decltype(close_file)> console(tmpfile(), close_file);
  require(console != nullptr, "tmpfile failed");
  Programmer programmer(console.get());
  programmer.initialize();
  require(programmer.size == kCapacity, "Programmer did not detect 16 MiB");

  bus.clear_trace();
  test_serial.reset(std::move(serial_input));
  configure();
  require(fseek(console.get(), 0, SEEK_END) == 0, "cannot seek console");
  require(fputc(command, console.get()) == command, "cannot enqueue command");
  require(fseek(console.get(), -1, SEEK_END) == 0,
          "cannot seek queued command");
  programmer.task();

  require(fflush(console.get()) == 0, "cannot flush console");
  rewind(console.get());
  std::string text;
  for (int value; (value = fgetc(console.get())) != EOF;) {
    require(text.size() < 4096, "console output exceeded test bound");
    text += static_cast<char>(value);
  }
  return text;
}

void test_jedec_command_and_ef4018() {
  bus.reset();
  expected_chip_select = kChipSelect;
  winbondFlashSPI flash;
  require(flash.begin(winbondFlashClass::autoDetect, SPI, kChipSelect),
          "EF 40 18 was not accepted");
  require_transactions({Bytes{0xab}, Bytes{0x9f, 0x00, 0x00, 0x00}});
  require(bus.chip_select_calls == std::vector<int>({HIGH, LOW, HIGH, LOW, HIGH}),
          "initial /CS state or JEDEC boundaries changed");
  require(bus.pin_modes ==
              std::vector<std::pair<std::uint8_t, int>>{{MISO, INPUT_PULLUP},
                                                        {kChipSelect, OUTPUT}},
          "default pin initialization changed");
  require_cs_initialized_before_transfer(kChipSelect);
  require(bus.spi_started && bus.bit_order == MSBFIRST &&
              bus.clock_divider == SPI_CLOCK_DIV2 && bus.data_mode == SPI_MODE0,
          "SPI initialization changed");
  require(bus.microsecond_delays == std::vector<unsigned int>{5},
          "release delay changed");
}

void test_capacity_ef4018() {
  auto flash = initialized_flash();
  require(flash.bytes() == 16777216L, "wrong byte capacity");
  require(flash.pages() == 65536UL, "wrong page count");
  require(flash.sectors() == 4096U, "wrong sector count");
  require(flash.blocks() == 256U, "wrong block count");
  require_transactions({});
}

void test_unknown_jedec_rejected() {
  bus.reset();
  expected_chip_select = kChipSelect;
  bus.jedec = {{0xef, 0x40, 0x19}};
  winbondFlashSPI flash;
  require(!flash.begin(winbondFlashClass::autoDetect, SPI, kChipSelect),
          "unknown EF JEDEC ID was accepted");
  require_transactions({Bytes{0xab}, Bytes{0x9f, 0x00, 0x00, 0x00}});
}

void test_custom_cs_pin_initialized() {
  bus.reset();
  expected_chip_select = 7;
  winbondFlashSPI flash;
  require(flash.begin(winbondFlashClass::autoDetect, SPI, expected_chip_select),
          "initialization with custom CS failed");
  require(bus.pin_modes ==
              std::vector<std::pair<std::uint8_t, int>>{{MISO, INPUT_PULLUP},
                                                        {expected_chip_select, OUTPUT}},
          "custom CS pin was not configured as OUTPUT");
  require_cs_initialized_before_transfer(expected_chip_select);
  require_transactions({Bytes{0xab}, Bytes{0x9f, 0x00, 0x00, 0x00}});
}

void test_identifier_helpers() {
  auto flash = initialized_flash();
  require(flash.readManufacturer() == 0xef, "manufacturer byte changed");
  require(flash.readPartID() == 0x4018, "part ID byte order changed");
  require_transactions({Bytes{0x9f, 0x00, 0x00, 0x00},
                        Bytes{0x9f, 0x00, 0x00, 0x00}});
}

void expect_read(std::uint32_t address, std::size_t count) {
  auto flash = initialized_flash();
  std::array<std::uint8_t, 8> output{};
  require(count <= output.size(), "test buffer too small");
  require(flash.read(address, output.data(), static_cast<std::uint16_t>(count)),
          "valid read was rejected");
  for (std::size_t i = 0; i < count; ++i)
    require(output[i] == bus.memory_byte((address + i) & 0x00ffffffUL),
            "read data order changed");
  require_transactions({Bytes{0x05, 0xff}, read_command(address, count)});
}

void test_read_address_zero() { expect_read(0x000000, 4); }
void test_read_middle_address() { expect_read(0x123456, 4); }
void test_read_last_valid_address() { expect_read(0xffffff, 1); }
void test_read_ends_at_capacity() { expect_read(kCapacity - 4, 4); }

void test_read_busy_returns_failure() {
  auto flash = initialized_flash();
  bus.final_status1 = 0x01;
  std::uint8_t output = 0xa5;
  require(!flash.read(0, &output, 1), "busy read was not rejected");
  require(output == 0xa5, "busy read modified its destination");
  require_transactions({Bytes{0x05, 0xff}});
}

void test_out_of_range_read_rejected() {
  auto flash = initialized_flash();
  std::uint8_t output = 0;
  require(!flash.read(kCapacity, &output, 1),
          "read starting at capacity was accepted");
  require_transactions({});
}

void test_read_crosses_capacity_rejected() {
  auto flash = initialized_flash();
  std::array<std::uint8_t, 2> output{};
  require(!flash.read(kCapacity - 1, output.data(), 2),
          "cross-boundary read was accepted");
  require_transactions({});
}

void test_read_start_above_capacity_rejected() {
  auto flash = initialized_flash();
  std::uint8_t output = 0xa5;
  require(!flash.read(kCapacity + 1, &output, 1),
          "read above capacity was accepted");
  require(output == 0xa5, "rejected read modified its destination");
  require_transactions({});
}

void test_read_overflow_input_rejected() {
  auto flash = initialized_flash();
  std::uint8_t output = 0xa5;
  require(!flash.read(UINT32_MAX - 10U, &output, UINT16_MAX),
          "overflowing address/length pair was accepted");
  require(output == 0xa5, "overflowing read modified its destination");
  require_transactions({});
}

void test_zero_length_read_is_no_op() {
  auto flash = initialized_flash();
  std::uint8_t output = 0xa5;
  require(flash.read(kCapacity, &output, 0),
          "zero-length read at capacity was rejected");
  require(!flash.read(kCapacity + 1, &output, 0),
          "zero-length read above capacity was accepted");
  require(output == 0xa5, "zero-length read modified its destination");
  require_transactions({});
}

void test_page_program_middle() {
  auto flash = initialized_flash();
  auto data = page_pattern();
  bus.status1 = {0x02, 0x00};
  require(flash.writePage(0x123400, data.data()), "Page Program failed");
  require_transactions({Bytes{0x06}, Bytes{0x05, 0xff},
                        page_program(0x123400, data), Bytes{0x05, 0xff}});
  require(bus.transactions[2].sent.size() == 260,
          "page program did not send exactly 256 data bytes");
}

void test_first_page_program() {
  auto flash = initialized_flash();
  auto data = page_pattern();
  bus.status1 = {0x02, 0x00};
  require(flash.writePage(0, data.data()), "first Page Program failed");
  require_transactions({Bytes{0x06}, Bytes{0x05, 0xff},
                        page_program(0, data), Bytes{0x05, 0xff}});
}

void test_last_page_program() {
  auto flash = initialized_flash();
  auto data = page_pattern();
  bus.status1 = {0x02, 0x00};
  require(flash.writePage(kCapacity - FLASH_PAGE_SIZE, data.data()),
          "last Page Program failed");
  require_transactions({Bytes{0x06}, Bytes{0x05, 0xff},
                        page_program(0xffff00, data), Bytes{0x05, 0xff}});
}

void test_page_program_without_wel() {
  auto flash = initialized_flash();
  auto data = page_pattern();
  bus.final_status1 = 0x00;
  require(!flash.writePage(0x001200, data.data()),
          "Page Program accepted WEL=0");
  require_transactions({Bytes{0x06}, Bytes{0x05, 0xff}});
}

void test_unaligned_page_address_rejected() {
  auto flash = initialized_flash();
  auto data = page_pattern();
  require(!flash.writePage(0x1234ab, data.data()),
          "unaligned Page Program was accepted");
  require_transactions({});
}

void test_out_of_range_page_program_rejected() {
  auto flash = initialized_flash();
  auto data = page_pattern();
  require(!flash.writePage(kCapacity, data.data()),
          "Page Program at capacity was accepted");
  require(!flash.writePage(UINT32_MAX, data.data()),
          "oversized Page Program address was accepted");
  require_transactions({});
}

void test_chip_erase_sequence() {
  auto flash = initialized_flash();
  bus.status1 = {0x02, 0x00};
  require(flash.eraseAll(), "Chip Erase failed");
  require_transactions({Bytes{0x06}, Bytes{0x05, 0xff}, Bytes{0xc7},
                        Bytes{0x05, 0xff}});
}

void test_chip_erase_without_wel() {
  auto flash = initialized_flash();
  bus.final_status1 = 0x00;
  require(!flash.eraseAll(), "Chip Erase accepted WEL=0");
  require_transactions({Bytes{0x06}, Bytes{0x05, 0xff}});
}

void test_busy_clears() {
  auto flash = initialized_flash();
  bus.status1 = {0x01, 0x03, 0x00};
  require(flash.waitUntilReady(10), "BUSY did not clear after scripted polls");
  require_transactions({Bytes{0x05, 0xff}, Bytes{0x05, 0xff},
                        Bytes{0x05, 0xff}});
}

void test_busy_immediately_clear() {
  auto flash = initialized_flash();
  bus.final_status1 = 0x00;
  require(flash.waitUntilReady(1), "ready device timed out");
  require_transactions({Bytes{0x05, 0xff}});
}

void test_busy_clears_just_before_timeout() {
  auto flash = initialized_flash();
  bus.status1 = {0x01, 0x01, 0x01, 0x01, 0x00};
  require(flash.waitUntilReady(5), "BUSY clearing before deadline timed out");
  require(bus.millis_calls == 5, "unexpected simulated time sampling");
  require_transactions({Bytes{0x05, 0xff}, Bytes{0x05, 0xff},
                        Bytes{0x05, 0xff}, Bytes{0x05, 0xff},
                        Bytes{0x05, 0xff}});
}

void test_busy_never_clears() {
  auto flash = initialized_flash();
  bus.final_status1 = 0x01;
  require(!flash.waitUntilReady(5), "permanent BUSY was reported successful");
  require(bus.millis_calls == 6, "timeout did not use the expected deadline");
  require_transactions({Bytes{0x05, 0xff}, Bytes{0x05, 0xff},
                        Bytes{0x05, 0xff}, Bytes{0x05, 0xff},
                        Bytes{0x05, 0xff}});
}

void test_busy_timeout_across_millis_rollover() {
  auto flash = initialized_flash();
  bus.now_ms = UINT32_MAX - 2U;
  bus.final_status1 = 0x01;
  require(!flash.waitUntilReady(3), "millis rollover defeated BUSY timeout");
  require(bus.millis_calls == 4, "rollover timeout used the wrong elapsed time");
  require_transactions({Bytes{0x05, 0xff}, Bytes{0x05, 0xff},
                        Bytes{0x05, 0xff}});
}

void test_status_register_and_cs() {
  auto flash = initialized_flash();
  bus.status1 = {0xa2};
  bus.status2 = 0x5a;
  require(flash.readSR() == 0x5aa2, "status register byte order changed");
  require_transactions({Bytes{0x05, 0xff}, Bytes{0x35, 0xff}});
  require(bus.chip_select_calls == std::vector<int>({LOW, HIGH, HIGH, LOW, HIGH}),
          "readSR /CS call sequence changed");
}

void test_write_disable() {
  auto flash = initialized_flash();
  require(flash.setWriteEnable(false), "Write Disable returned failure");
  require_transactions({Bytes{0x04}});
}

void test_addressed_erase_commands() {
  auto flash = initialized_flash();
  bus.status1 = {0x02, 0x00, 0x02, 0x00, 0x02, 0x00};
  require(flash.eraseSector(0x123000), "Sector Erase failed");
  require(flash.erase32kBlock(0x128000), "32 KiB Block Erase failed");
  require(flash.erase64kBlock(0x120000), "64 KiB Block Erase failed");
  require_transactions({Bytes{0x06}, Bytes{0x05, 0xff},
                        Bytes{0x20, 0x12, 0x30, 0x00}, Bytes{0x05, 0xff},
                        Bytes{0x06}, Bytes{0x05, 0xff},
                        Bytes{0x52, 0x12, 0x80, 0x00}, Bytes{0x05, 0xff},
                        Bytes{0x06}, Bytes{0x05, 0xff},
                        Bytes{0xd8, 0x12, 0x00, 0x00}, Bytes{0x05, 0xff}});
}

void test_erase_first_and_last_units() {
  auto flash = initialized_flash();
  bus.status1 = {0x02, 0x00, 0x02, 0x00, 0x02, 0x00,
                 0x02, 0x00, 0x02, 0x00, 0x02, 0x00};
  require(flash.eraseSector(0), "first sector was rejected");
  require(flash.eraseSector(kCapacity - FLASH_SECTOR_SIZE),
          "last sector was rejected");
  require(flash.erase32kBlock(0), "first 32 KiB block was rejected");
  require(flash.erase32kBlock(kCapacity - FLASH_BLOCK32_SIZE),
          "last 32 KiB block was rejected");
  require(flash.erase64kBlock(0), "first 64 KiB block was rejected");
  require(flash.erase64kBlock(kCapacity - FLASH_BLOCK64_SIZE),
          "last 64 KiB block was rejected");
  require_transactions({
      Bytes{0x06}, Bytes{0x05, 0xff}, erase_command(0x20, 0), Bytes{0x05, 0xff},
      Bytes{0x06}, Bytes{0x05, 0xff},
      erase_command(0x20, kCapacity - FLASH_SECTOR_SIZE), Bytes{0x05, 0xff},
      Bytes{0x06}, Bytes{0x05, 0xff}, erase_command(0x52, 0), Bytes{0x05, 0xff},
      Bytes{0x06}, Bytes{0x05, 0xff},
      erase_command(0x52, kCapacity - FLASH_BLOCK32_SIZE), Bytes{0x05, 0xff},
      Bytes{0x06}, Bytes{0x05, 0xff}, erase_command(0xd8, 0), Bytes{0x05, 0xff},
      Bytes{0x06}, Bytes{0x05, 0xff},
      erase_command(0xd8, kCapacity - FLASH_BLOCK64_SIZE), Bytes{0x05, 0xff}});
}

void test_erase_invalid_addresses_rejected() {
  auto flash = initialized_flash();
  require(!flash.eraseSector(1), "unaligned Sector Erase was accepted");
  require(!flash.eraseSector(kCapacity), "out-of-range Sector Erase was accepted");
  require(!flash.eraseSector(UINT32_MAX), "oversized Sector Erase was accepted");
  require(!flash.erase32kBlock(1), "unaligned 32 KiB erase was accepted");
  require(!flash.erase32kBlock(kCapacity),
          "out-of-range 32 KiB erase was accepted");
  require(!flash.erase32kBlock(UINT32_MAX),
          "oversized 32 KiB erase was accepted");
  require(!flash.erase64kBlock(1), "unaligned 64 KiB erase was accepted");
  require(!flash.erase64kBlock(kCapacity),
          "out-of-range 64 KiB erase was accepted");
  require(!flash.erase64kBlock(UINT32_MAX),
          "oversized 64 KiB erase was accepted");
  require_transactions({});
}

void test_wel_failure_blocks_all_erase_commands() {
  auto flash = initialized_flash();
  bus.final_status1 = 0x00;
  require(!flash.eraseSector(0), "Sector Erase accepted WEL=0");
  require(!flash.erase32kBlock(0), "32 KiB Block Erase accepted WEL=0");
  require(!flash.erase64kBlock(0), "64 KiB Block Erase accepted WEL=0");
  require(!flash.eraseAll(), "Chip Erase accepted WEL=0");
  require_transactions({Bytes{0x06}, Bytes{0x05, 0xff},
                        Bytes{0x06}, Bytes{0x05, 0xff},
                        Bytes{0x06}, Bytes{0x05, 0xff},
                        Bytes{0x06}, Bytes{0x05, 0xff}});
}

void test_page_program_busy_timeout_is_failure() {
  auto flash = initialized_flash();
  auto data = page_pattern();
  bus.status1 = {0x02};
  bus.final_status1 = 0x01;
  require(!flash.writePage(0, data.data()),
          "Page Program BUSY timeout was reported successful");
  require(bus.transactions.size() >= 4, "Page Program did not poll BUSY");
  require(bus.transactions[2].sent == page_program(0, data),
          "Page Program command was not issued after WEL=1");
}

void test_chip_erase_busy_timeout_is_failure() {
  auto flash = initialized_flash();
  bus.status1 = {0x02};
  bus.final_status1 = 0x01;
  bus.millis_step = 50000;
  require(!flash.eraseAll(), "Chip Erase BUSY timeout was reported successful");
  require(bus.transactions.size() >= 4 && bus.transactions[2].sent == Bytes{0xc7},
          "Chip Erase command was not issued after WEL=1");
}

void expect_operation_timeout(
    std::uint32_t timeout_ms, std::uint8_t command,
    const std::function<bool(winbondFlashSPI &)> &operation) {
  auto flash = initialized_flash();
  bus.status1 = {0x02};
  bus.final_status1 = 0x01;
  bus.millis_step = timeout_ms;
  require(!operation(flash), "operation BUSY timeout was reported successful");
  require(bus.millis_calls == 2, "operation used the wrong timeout constant");
  require(sent_command(command), "modifying command was not issued with WEL=1");
}

void test_operation_specific_timeout_values() {
  require(FLASH_PAGE_PROGRAM_TIMEOUT_MS == 5UL, "Page Program timeout changed");
  require(FLASH_SECTOR_ERASE_TIMEOUT_MS == 500UL, "Sector Erase timeout changed");
  require(FLASH_BLOCK32_ERASE_TIMEOUT_MS == 2000UL,
          "32 KiB Block Erase timeout changed");
  require(FLASH_BLOCK64_ERASE_TIMEOUT_MS == 2500UL,
          "64 KiB Block Erase timeout changed");
  require(FLASH_CHIP_ERASE_TIMEOUT_MS == 250000UL, "Chip Erase timeout changed");

  auto data = page_pattern();
  expect_operation_timeout(FLASH_PAGE_PROGRAM_TIMEOUT_MS, 0x02,
                           [&data](winbondFlashSPI &flash) {
                             return flash.writePage(0, data.data());
                           });
  expect_operation_timeout(FLASH_SECTOR_ERASE_TIMEOUT_MS, 0x20,
                           [](winbondFlashSPI &flash) {
                             return flash.eraseSector(0);
                           });
  expect_operation_timeout(FLASH_BLOCK32_ERASE_TIMEOUT_MS, 0x52,
                           [](winbondFlashSPI &flash) {
                             return flash.erase32kBlock(0);
                           });
  expect_operation_timeout(FLASH_BLOCK64_ERASE_TIMEOUT_MS, 0xd8,
                           [](winbondFlashSPI &flash) {
                             return flash.erase64kBlock(0);
                           });
  expect_operation_timeout(FLASH_CHIP_ERASE_TIMEOUT_MS, 0xc7,
                           [](winbondFlashSPI &flash) {
                             return flash.eraseAll();
                           });
}

void test_programmer_erase_wel_failure() {
  const auto text = run_programmer_command('e', {}, [] {
    bus.final_status1 = 0x00;
  });
  require(text.find("Erase failed") != std::string::npos,
          "Programmer omitted the erase error");
  require(text.find("Done") == std::string::npos,
          "Programmer reported erase success after WEL failure");
  require(!sent_command(0xc7), "Programmer issued Chip Erase with WEL=0");
}

void test_programmer_erase_busy_timeout() {
  const auto text = run_programmer_command('e', {}, [] {
    bus.status1 = {0x02};
    bus.final_status1 = 0x01;
    bus.millis_step = 50000;
  });
  require(sent_command(0xc7), "Programmer did not issue Chip Erase with WEL=1");
  require(text.find("Erase failed") != std::string::npos,
          "Programmer omitted the erase timeout error");
  require(text.find("Done") == std::string::npos,
          "Programmer reported erase success after BUSY timeout");
}

void test_programmer_write_wel_failure() {
  const auto text = run_programmer_command('w', one_page_xmodem_input(), [] {
    bus.final_status1 = 0x00;
  });
  require(text.find("Flash write failed") != std::string::npos,
          "Programmer omitted the Page Program error");
  require(text.find("Write done") == std::string::npos,
          "Programmer reported write success after WEL failure");
  require(!sent_command(0x02), "Programmer issued Page Program with WEL=0");
}

void test_programmer_write_busy_timeout() {
  const auto text = run_programmer_command('w', one_page_xmodem_input(), [] {
    bus.status1 = {0x02};
    bus.final_status1 = 0x01;
  });
  require(sent_command(0x02), "Programmer did not issue Page Program with WEL=1");
  require(text.find("Flash write failed") != std::string::npos,
          "Programmer omitted the Page Program timeout error");
  require(text.find("Write done") == std::string::npos,
          "Programmer reported write success after BUSY timeout");
}

void test_programmer_read_failure() {
  const auto text = run_programmer_command('r', Bytes{XSTART}, [] {
    bus.final_status1 = 0x01;
  });
  require(text.find("Flash read failed") != std::string::npos,
          "Programmer omitted the flash read error");
  require(!sent_command(0x03), "Programmer issued Read Data while BUSY");
  require(test_serial.output.empty(),
          "Programmer sent stale data after the flash read error");
}

void test_suspend_resume() {
  auto flash = initialized_flash();
  flash.eraseSuspend();
  flash.eraseResume();
  require_transactions({Bytes{0x75}, Bytes{0x7a}});
}

void test_unique_id() {
  auto flash = initialized_flash();
  require(flash.readUniqueID() == UINT64_C(0x0102030405060708),
          "unique ID byte order changed");
  require_transactions({Bytes{0x4b, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00}});
}

void test_power_down() {
  auto flash = initialized_flash();
  flash.end();
  require_transactions({Bytes{0xb9}});
  require(bus.spi_ended, "SPI.end was not called");
  require(bus.microsecond_delays == std::vector<unsigned int>{5},
          "power-down delay changed");
}

using Test = std::function<void()>;

const std::vector<std::pair<std::string, Test>> &tests() {
  static const std::vector<std::pair<std::string, Test>> all{
      {"jedec_command_and_ef4018", test_jedec_command_and_ef4018},
      {"capacity_ef4018", test_capacity_ef4018},
      {"unknown_jedec_rejected", test_unknown_jedec_rejected},
      {"custom_cs_pin_initialized", test_custom_cs_pin_initialized},
      {"identifier_helpers", test_identifier_helpers},
      {"read_address_zero", test_read_address_zero},
      {"read_middle_address", test_read_middle_address},
      {"read_last_valid_address", test_read_last_valid_address},
      {"read_ends_at_capacity", test_read_ends_at_capacity},
      {"read_busy_returns_failure", test_read_busy_returns_failure},
      {"out_of_range_read_rejected", test_out_of_range_read_rejected},
      {"read_crosses_capacity_rejected", test_read_crosses_capacity_rejected},
      {"read_start_above_capacity_rejected",
       test_read_start_above_capacity_rejected},
      {"read_overflow_input_rejected", test_read_overflow_input_rejected},
      {"zero_length_read_is_no_op", test_zero_length_read_is_no_op},
      {"first_page_program", test_first_page_program},
      {"page_program_middle", test_page_program_middle},
      {"last_page_program", test_last_page_program},
      {"page_program_without_wel", test_page_program_without_wel},
      {"unaligned_page_address_rejected", test_unaligned_page_address_rejected},
      {"out_of_range_page_program_rejected",
       test_out_of_range_page_program_rejected},
      {"chip_erase_sequence", test_chip_erase_sequence},
      {"chip_erase_without_wel", test_chip_erase_without_wel},
      {"busy_immediately_clear", test_busy_immediately_clear},
      {"busy_clears", test_busy_clears},
      {"busy_clears_just_before_timeout", test_busy_clears_just_before_timeout},
      {"busy_never_clears", test_busy_never_clears},
      {"busy_timeout_across_millis_rollover",
       test_busy_timeout_across_millis_rollover},
      {"status_register_and_cs", test_status_register_and_cs},
      {"write_disable", test_write_disable},
      {"addressed_erase_commands", test_addressed_erase_commands},
      {"erase_first_and_last_units", test_erase_first_and_last_units},
      {"erase_invalid_addresses_rejected",
       test_erase_invalid_addresses_rejected},
      {"wel_failure_blocks_all_erase_commands",
       test_wel_failure_blocks_all_erase_commands},
      {"page_program_busy_timeout_is_failure",
       test_page_program_busy_timeout_is_failure},
      {"chip_erase_busy_timeout_is_failure",
       test_chip_erase_busy_timeout_is_failure},
      {"operation_specific_timeout_values",
       test_operation_specific_timeout_values},
      {"programmer_erase_wel_failure", test_programmer_erase_wel_failure},
      {"programmer_erase_busy_timeout", test_programmer_erase_busy_timeout},
      {"programmer_write_wel_failure", test_programmer_write_wel_failure},
      {"programmer_write_busy_timeout", test_programmer_write_busy_timeout},
      {"programmer_read_failure", test_programmer_read_failure},
      {"suspend_resume", test_suspend_resume},
      {"unique_id", test_unique_id},
      {"power_down", test_power_down},
  };
  return all;
}

}  // namespace

// The production base class declares this virtual member but only the concrete
// SPI subclass defines an implementation. Supply an unreachable test-side key
// function so the host linker can emit the abstract base vtable; all exercised
// address transfers still use winbondFlashSPI's production inline override.
void winbondFlashClass::transfer_addr(std::uint32_t) {
  throw std::runtime_error("unexpected abstract transfer_addr call");
}

void pinMode(unsigned char pin, int mode) {
  bus.pin_modes.push_back({pin, mode});
  bus.events.push_back("pinMode:" + std::to_string(pin) + ":" +
                       std::to_string(mode));
}
void digitalWrite(unsigned char pin, int value) {
  require(pin == expected_chip_select, "unexpected chip-select pin");
  bus.events.push_back("digitalWrite:" + std::to_string(pin) + ":" +
                       std::to_string(value));
  if (value == LOW)
    bus.select();
  else if (value == HIGH)
    bus.deselect();
  else
    throw std::runtime_error("invalid digitalWrite value");
}
void delay(unsigned long) {}
void delayMicroseconds(unsigned int microseconds) {
  bus.microsecond_delays.push_back(microseconds);
}
unsigned long millis() {
  ++bus.millis_calls;
  const std::uint32_t result = bus.now_ms;
  bus.now_ms += bus.millis_step;
  return result;
}
int vfprintf_P(FILE *file, const char *format, va_list args) {
  static_assert(sizeof(std::uint32_t) == sizeof(unsigned int));
  std::string host_format(format);
  for (std::size_t pos = 0;
       (pos = host_format.find("%lu", pos)) != std::string::npos;)
    host_format.replace(pos, 3, "%u");
  require(fseek(file, 0, SEEK_CUR) == 0, "cannot switch console to output");
  return vfprintf(file, host_format.c_str(), args);
}

void SPIClass::begin() {
  bus.spi_started = true;
  bus.events.push_back("spi.begin");
}
void SPIClass::end() { bus.spi_ended = true; }
void SPIClass::setBitOrder(std::uint8_t order) { bus.bit_order = order; }
void SPIClass::setClockDivider(std::uint8_t divider) { bus.clock_divider = divider; }
void SPIClass::setDataMode(std::uint8_t mode) { bus.data_mode = mode; }
std::uint8_t SPIClass::transfer(std::uint8_t value) {
  bus.events.push_back("transfer");
  return bus.transfer(value);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: flash_characterization TEST_NAME\n";
    return 2;
  }
  const std::string requested = argv[1];
  if (requested == "--list") {
    for (const auto &test : tests()) std::cout << test.first << '\n';
    return 0;
  }
  const auto found = std::find_if(
      tests().begin(), tests().end(),
      [&requested](const auto &entry) { return entry.first == requested; });
  if (found == tests().end()) {
    std::cerr << "unknown test: " << requested << '\n';
    return 2;
  }
  try {
    found->second();
    require_balanced_chip_select();
    std::cout << "PASS " << requested << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL " << requested << ": " << error.what() << '\n';
    return 1;
  }
}
