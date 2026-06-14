/*
 * SCS.h
 * Feetech Serial Servo Communication Layer Protocol
 * Date: 2019.12.18
 * Author:
 */

#ifndef _SCS_H
#define _SCS_H

#include "INST.h"

class SCS{
public:
	SCS();
	SCS(u8 End);
	SCS(u8 End, u8 Level);
	int genWrite(u8 ID, u8 MemAddr, u8 *nDat, u8 nLen);//Standard write instruction
	int regWrite(u8 ID, u8 MemAddr, u8 *nDat, u8 nLen);//Asynchronous write instruction
	int RegWriteAction(u8 ID = 0xfe);//Asynchronous write execute instruction
	void syncWrite(u8 ID[], u8 IDN, u8 MemAddr, u8 *nDat, u8 nLen);//Synchronous write instruction
	int writeByte(u8 ID, u8 MemAddr, u8 bDat);//Write 1 byte
	int writeWord(u8 ID, u8 MemAddr, u16 wDat);//Write 2 bytes
	int Read(u8 ID, u8 MemAddr, u8 *nData, u8 nLen);//Read instruction
	int readByte(u8 ID, u8 MemAddr);//Read 1 byte
	int readWord(u8 ID, u8 MemAddr);//Read 2 bytes
	int Ping(u8 ID);//Ping instruction
	int syncReadPacketTx(u8 ID[], u8 IDN, u8 MemAddr, u8 nLen);//Synchronous read packet transmit
	int syncReadPacketRx(u8 ID, u8 *nDat);//Synchronous read response receive, returns byte count on success, 0 on failure
	int syncReadRxPacketToByte();//Decode one byte
	int syncReadRxPacketToWrod(u8 negBit=0);//Decode two bytes, negBit is the direction bit, negBit=0 means no direction
public:
	u8 Level;//Servo return level
	u8 End;//Processor endianness
	u8 Error;//Servo status
	u8 syncReadRxPacketIndex;
	u8 syncReadRxPacketLen;
	u8 *syncReadRxPacket;
protected:
	virtual int writeSCS(unsigned char *nDat, int nLen) = 0;
	virtual int readSCS(unsigned char *nDat, int nLen) = 0;
	virtual int writeSCS(unsigned char bDat) = 0;
	virtual void rFlushSCS() = 0;
	virtual void wFlushSCS() = 0;
protected:
	void writeBuf(u8 ID, u8 MemAddr, u8 *nDat, u8 nLen, u8 Fun);
	void Host2SCS(u8 *DataL, u8* DataH, u16 Data);//Split one 16-bit value into two 8-bit values
	u16	SCS2Host(u8 DataL, u8 DataH);//Combine two 8-bit values into one 16-bit value
	int	Ack(u8 ID);//Return acknowledgment
	int checkHead();//Frame header detection
};
#endif
