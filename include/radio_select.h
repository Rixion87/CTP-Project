#pragma once
#if defined(RADIO_NRF905)
    #include <radioNRF905.h>
    using Radio = RadioNRF905;
#elif defined(RADIO_RFM95)
    #include <radioRFM95.h>
    using Radio = RadioRFM95;
#elif defined(RADIO_MKR1310)
    #include <radioMKR1310.h>
    using Radio = RadioMKR1310;
#endif