#ifndef EXTERNALENCODERADAPTER_H_
#define EXTERNALENCODERADAPTER_H_

#include "MotorDriver.h"
#include "Encoder.h"

class ExternalEncoderAdapter {
public:
    ExternalEncoderAdapter(MotorDriver* motor, Encoder* encoder);
    ~ExternalEncoderAdapter();

    void update();

private:
    MotorDriver* motor;
    Encoder* encoder;
};

#endif // EXTERNALENCODERADAPTER_H_
