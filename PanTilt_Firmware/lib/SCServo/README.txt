Communication layer: SCS
----------------------------
Hardware interface layer: SCSerial
----------------------------
Application layer: SMS_STS and SCSCL correspond to Feetech's three servo series



SMS_STS sms_sts;  // define SMSBL/SMSCL/STSCL series servo
SCSCL sc;         // define SCSCL series servo



INST.h          --- Instruction definition header file
SCS.h/SCS.cpp   --- Communication layer program
SCSerial.h/SCSerial.cpp --- Hardware interface program
SMS_STS.h/SMS_STS.cpp   --- SMSBL/SMSCL/STSCL application layer program
SCSCL.h/SCSCL.cpp       --- SCSCL application layer program
(Memory table definitions are in the application layer header files SMS_STS.h and SCSCL.h;
 memory table definitions differ between servo series)


Class hierarchy: SCS <--- SCSerial <--- SMS_STS / SCSCL

Tested on Arduino with Atmega2560
