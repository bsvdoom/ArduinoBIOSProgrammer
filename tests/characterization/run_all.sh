#!/usr/bin/env bash
set -eu

baseline=false
if [ "${1:-}" = "--baseline" ]; then
  baseline=true
elif [ "$#" -ne 0 ]; then
  exit 2
fi

cd -- "$(dirname -- "$0")/../.."
test_dir=$(mktemp -d /tmp/xmodem-write-tests.XXXXXX)
trap 'rm -rf -- "$test_dir"' EXIT

legacy_status=0
python3 -B tests/characterization/run_tests.py || legacy_status=$?

g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Itests/characterization/stubs -Isrc \
  src/programmer.cpp src/xmodem.cpp \
  tests/characterization/write_characterization.cpp \
  -o "$test_dir/write_characterization"

export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
write_pass=0
write_known=0
write_fail=0
for test_name in $("$test_dir/write_characterization" --list); do
  printf 'START %s\n' "$test_name"
  test_status=0
  timeout -k 1s 2s "$test_dir/write_characterization" "$test_name" \
    > "$test_dir/result.log" 2>&1 || test_status=$?
  expected=''
  case "$test_name" in
    write_eot_before_first_block|write_eot_after_half_page|write_eot_after_full_page|write_eot_after_page_and_half)
      expected='wrong number of page writes' ;;
    write_exact_capacity) expected='exact capacity did not succeed' ;;
    write_last_data_ack) expected='last data block has no ACK' ;;
    write_eot_ack) expected='EOT has no ACK' ;;
    write_oversized) expected='extra data block was not inspected' ;;
    write_page_requires_two_blocks) expected='page writes or finalization occur in the wrong order' ;;
    write_missing_eot) expected='last data block not ACKed before waiting for EOT' ;;
    write_duplicate_final_block) expected='duplicate final block broke termination' ;;
  esac
  if [ "$test_status" -eq 0 ]; then
    printf 'PASS %s\n' "$test_name"
    write_pass=$((write_pass + 1))
  elif "$baseline" && [ "$test_status" -eq 1 ] && [ -n "$expected" ] &&
       grep -Fxq "FAIL $test_name: $expected" "$test_dir/result.log" &&
       ! grep -Eq 'Sanitizer|runtime error:' "$test_dir/result.log"; then
    printf 'KNOWN FAILURE %s: %s\n' "$test_name" "$expected"
    write_known=$((write_known + 1))
  else
    printf 'UNEXPECTED FAILURE %s exit=%s\n' "$test_name" "$test_status"
    cat "$test_dir/result.log"
    write_fail=$((write_fail + 1))
  fi
done
printf 'WRITE SUMMARY PASS=%s KNOWN_FAILURE=%s UNEXPECTED_FAILURE=%s\n' \
  "$write_pass" "$write_known" "$write_fail"

g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Itests/characterization/stubs -Isrc \
  src/programmer.cpp src/xmodem.cpp src/winbondflash.cpp \
  tests/characterization/flash_characterization.cpp \
  -o "$test_dir/flash_characterization"

flash_pass=0
flash_missing=0
flash_fail=0
for test_name in $("$test_dir/flash_characterization" --list); do
  printf 'START %s\n' "$test_name"
  test_status=0
  timeout -k 1s 2s "$test_dir/flash_characterization" "$test_name" \
    > "$test_dir/result.log" 2>&1 || test_status=$?
  if [ "$test_status" -eq 0 ]; then
    cat "$test_dir/result.log"
    flash_pass=$((flash_pass + 1))
  else
    printf 'UNEXPECTED FAILURE %s exit=%s\n' "$test_name" "$test_status"
    cat "$test_dir/result.log"
    flash_fail=$((flash_fail + 1))
  fi
done
printf 'FLASH SUMMARY PASS=%s MISSING_SAFEGUARD=%s UNEXPECTED_FAILURE=%s\n' \
  "$flash_pass" "$flash_missing" "$flash_fail"

if [ "$legacy_status" -ne 0 ] || [ "$write_fail" -ne 0 ] || \
   [ "$flash_fail" -ne 0 ]; then
  exit 1
fi
