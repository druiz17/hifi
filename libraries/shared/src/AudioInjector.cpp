//
//  AudioInjector.cpp
//  hifi
//
//  Created by Stephen Birarda on 4/23/13.
//
//

#include <sys/time.h>
#include <fstream>

#include "SharedUtil.h"
#include "PacketHeaders.h"

#include "AudioInjector.h"

const int BUFFER_LENGTH_BYTES = 512;
const int BUFFER_LENGTH_SAMPLES = BUFFER_LENGTH_BYTES / sizeof(int16_t);
const float SAMPLE_RATE = 22050.0;
const float BUFFER_SEND_INTERVAL_USECS = (BUFFER_LENGTH_SAMPLES / SAMPLE_RATE) * 1000000;

AudioInjector::AudioInjector(const char* filename) :
    _bearing(0),
    _attenuationModifier(255)
{
    _position[0] = 0.0f;
    _position[1] = 0.0f;
    _position[2] = 0.0f;
    
    std::fstream sourceFile;
    
    sourceFile.open(filename, std::ios::in | std::ios::binary);
    sourceFile.seekg(0, std::ios::end);
    
    _numTotalBytesAudio = sourceFile.tellg();
    sourceFile.seekg(0, std::ios::beg);
    long sizeOfShortArray = _numTotalBytesAudio / 2;
    _audioSampleArray = new int16_t[sizeOfShortArray];
    
    sourceFile.read((char *)_audioSampleArray, _numTotalBytesAudio);
}

AudioInjector::~AudioInjector() {
    delete[] _audioSampleArray;
}

void AudioInjector::setPosition(float* position) {
    _position[0] = position[0];
    _position[1] = position[1];
    _position[2] = position[2];
}

void AudioInjector::injectAudio(UDPSocket *injectorSocket, sockaddr *destinationSocket) {
    timeval startTime;
    
    // one byte for header, 3 positional floats, 1 bearing float, 1 attenuation modifier byte
    int leadingBytes = 1 + (sizeof(float) * 4) + 1;
    unsigned char dataPacket[BUFFER_LENGTH_BYTES + leadingBytes];
    
    dataPacket[0] = PACKET_HEADER_INJECT_AUDIO;
    unsigned char *currentPacketPtr = dataPacket + 1;
    
    for (int i = 0; i < 3; i++) {
        memcpy(currentPacketPtr, &_position[i], sizeof(float));
        currentPacketPtr += sizeof(float);
    }
    
    memcpy(currentPacketPtr, &_bearing, sizeof(float));
    currentPacketPtr += sizeof(float);
        
    *currentPacketPtr = _attenuationModifier;
    currentPacketPtr++;
    
    for (int i = 0; i < _numTotalBytesAudio; i += BUFFER_LENGTH_SAMPLES) {
        gettimeofday(&startTime, NULL);
        
        memcpy(currentPacketPtr, _audioSampleArray + i, BUFFER_LENGTH_BYTES);
        injectorSocket->send(destinationSocket, dataPacket, sizeof(dataPacket));
        
        double usecToSleep = usecTimestamp(&startTime) + BUFFER_SEND_INTERVAL_USECS - usecTimestampNow();
        usleep(usecToSleep);
    }
}
