/*RadioHAL.h*/
/*Abstract interface the rest of the stack calls, allows other program*/

#pragma once
#include <stdint.h>
#include "packet.h"

class RadioHAL {
    public:
        virtual bool    begin()                                 = 0;
        virtual bool    send(const uint8_t* data, uint8_t len)  = 0;
        virtual int16_t receive(uint8_t* buf, uint8_t maxLen)   = 0;
        virtual int16_t getRSSI()                               = 0;
        virtual float   getSNR()                                = 0;
        virtual void    setTxPower(uint8_t dbm)                 = 0;
        virtual ~RadioHAL() {}
};