


#ifndef RR_UTIL_H
#define RR_UTIL_H

#include <cstdint> // Add missing include for uint64_t
#include <concepts> // Add missing include for std::integral

uint64_t GetCuurentTimeMs();
uint64_t GetCuurentTimeUs();

template <std::integral T>
T ByteSwap(T value) {
    
}

#endif // RR_UTIL_H
