#ifndef EXTERNALENCODERADAPTER_H_
#define EXTERNALENCODERADAPTER_H_

#include "MotorDriver.h"
#include "Encoder.h"

class ExternalEncoderAdapter {
public:
    ExternalEncoderAdapter(MotorDriver* motor, Encoder* encoder);
    ~ExternalEncoderAdapter();

    void update();
    Encoder* getEncoder() const { return encoder; }

private:
    MotorDriver* motor;
    Encoder* encoder;
};

#endif // EXTERNALENCODERADAPTER_H_
