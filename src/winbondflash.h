/*
Winbond spi flash memory chip operating library for Arduino
by WarMonkey (luoshumymail@gmail.com)
for more information, please visit bbs.kechuang.org
latest version available on http://code.google.com/p/winbondflash
*/

#ifndef _WINBONDFLASH_H__
#define _WINBONDFLASH_H__

#include <inttypes.h>
#include <SPI.h>

#define FLASH_PAGE_SIZE 256ul
#define FLASH_SECTOR_SIZE 4096ul
#define FLASH_BLOCK32_SIZE 32768ul
#define FLASH_BLOCK64_SIZE 65536ul

// W25Q128FV reference maximums plus documented safety margin.
static const uint32_t FLASH_PAGE_PROGRAM_TIMEOUT_MS = 5UL;
static const uint32_t FLASH_SECTOR_ERASE_TIMEOUT_MS = 500UL;
static const uint32_t FLASH_BLOCK32_ERASE_TIMEOUT_MS = 2000UL;
static const uint32_t FLASH_BLOCK64_ERASE_TIMEOUT_MS = 2500UL;
static const uint32_t FLASH_CHIP_ERASE_TIMEOUT_MS = 250000UL;

//W25Q64 = 256_bytes_per_page * 16_pages_per_sector * 16_sectors_per_block * 128_blocks_per_chip
//= 256b*16*16*128 = 8Mbyte = 64MBits

#define _W25Q80  winbondFlashClass::W25Q80
#define _W25Q16  winbondFlashClass::W25Q16
#define _W25Q32  winbondFlashClass::W25Q32
#define _W25Q64  winbondFlashClass::W25Q64
#define _W25Q128 winbondFlashClass::W25Q128

class winbondFlashClass {
public:  
	enum partNumberType {
		custom = -1,
		autoDetect = 0,
		W25Q80 = 1,
		W25Q16 = 2,
		W25Q32 = 4,
		W25Q64 = 8,
		W25Q128 = 16
	};

	bool begin(partNumberType _partno = autoDetect);
	void end();

	long bytes();
	uint32_t pages();
	uint16_t sectors();
	uint16_t blocks();

	bool read(uint32_t addr,uint8_t *buf,uint16_t n=256);

	bool setWriteEnable(bool cmd = true);
	inline bool WE(bool cmd = true) {return setWriteEnable(cmd);}

	bool writePage(uint32_t addr_start,uint8_t *buf);//addr is 8bit-aligned, 0x00ffff00
	//write a page, sizeof(buf) is 256 bytes
	bool eraseSector(uint32_t addr);//addr is 12bit-aligned, 0x00fff000
	//erase a sector ( 4096bytes )
	bool erase32kBlock(uint32_t addr);//addr is 15bit-aligned, 0x00ff8000
	//erase a 32k block ( 32768b )
	bool erase64kBlock(uint32_t addr);//addr is 16bit-aligned, 0x00ff0000
	//erase a 64k block ( 65536b )
	bool eraseAll();

	void eraseSuspend();
	void eraseResume();

	bool busy();
	bool waitUntilReady(uint32_t timeout_ms);

	uint8_t  readManufacturer();
	uint16_t readPartID();
	uint64_t readUniqueID();
	uint16_t readSR();

private:
	partNumberType partno;
	bool checkPartNo(partNumberType _partno);
	bool validRange(uint32_t addr,uint32_t length);
	bool validAlignedRange(uint32_t addr,uint32_t length);

protected:
	virtual void select() = 0;
	virtual uint8_t transfer(uint8_t x) = 0;
	virtual void transfer_addr(uint32_t addr);
	virtual void deselect() = 0;
};

class winbondFlashSPI: public winbondFlashClass {
private:
	uint8_t nss;
	SPIClass spi;

	inline void select() {
		digitalWrite(nss,LOW);
	}

	inline void deselect() {
		digitalWrite(nss,HIGH);
	}

	inline uint8_t transfer(uint8_t x) {
		return spi.transfer(x);
	}

	void transfer_addr(uint32_t addr) {
		spi.transfer(addr >> 16);
		spi.transfer(addr >> 8);
		spi.transfer(addr);
	}

public:
	bool begin(partNumberType _partno = autoDetect, SPIClass &_spi = SPI, uint8_t _nss = SS);
	void end();
};

#endif
