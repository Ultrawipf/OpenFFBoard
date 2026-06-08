#include "KinematicKalman.h"

KinematicKalman::KinematicKalman() {
    x[0] = 0.0f;
    x[1] = 0.0f;
    x[2] = 0.0f;

    P[0][0] = 1.0f; P[0][1] = 0.0f; P[0][2] = 0.0f;
    P[1][0] = 0.0f; P[1][1] = 1.0f; P[1][2] = 0.0f;
    P[2][0] = 0.0f; P[2][1] = 0.0f; P[2][2] = 1.0f;

    q_var = 1000000.0f;
}

void KinematicKalman::setQ(float q) {
    q_var = q;
}

float KinematicKalman::getTheta() const {
    return x[0];
}

float KinematicKalman::getOmega() const {
    return x[1];
}

float KinematicKalman::getAlpha() const {
    return x[2];
}

void KinematicKalman::predict(float dt) {
    if (dt <= 0.0f) {
        return;
    }

    // State prediction
    float theta = x[0] + x[1] * dt + 0.5f * x[2] * dt * dt;
    float omega = x[1] + x[2] * dt;
    x[0] = theta;
    x[1] = omega;
    // x[2] = x[2]

    // M = F * P
    float M00 = P[0][0] + dt * P[1][0] + 0.5f * dt * dt * P[2][0];
    float M01 = P[0][1] + dt * P[1][1] + 0.5f * dt * dt * P[2][1];
    float M02 = P[0][2] + dt * P[1][2] + 0.5f * dt * dt * P[2][2];

    float M10 = P[1][0] + dt * P[2][0];
    float M11 = P[1][1] + dt * P[2][1];
    float M12 = P[1][2] + dt * P[2][2];

    float M20 = P[2][0];
    float M21 = P[2][1];
    float M22 = P[2][2];

    // Q values
    float dt2 = dt * dt;
    float dt3 = dt2 * dt;
    float dt4 = dt3 * dt;

    float Q00 = q_var * dt4 * 0.25f;
    float Q01 = q_var * dt3 * 0.5f;
    float Q02 = q_var * dt2 * 0.5f;
    float Q11 = q_var * dt2;
    float Q12 = q_var * dt;
    float Q22 = q_var;

    // Covariance prediction: P = M * F^T + Q
    P[0][0] = M00 + M01 * dt + M02 * 0.5f * dt2 + Q00;
    P[0][1] = M01 + M02 * dt + Q01;
    P[0][2] = M02 + Q02;

    P[1][0] = P[0][1];
    P[1][1] = M11 + M12 * dt + Q11;
    P[1][2] = M12 + Q12;

    P[2][0] = P[0][2];
    P[2][1] = P[1][2];
    P[2][2] = M22 + Q22;
}

void KinematicKalman::update(float measurement, float R) {
    float S = P[0][0] + R;
    if (S <= 0.0f) {
        return;
    }

    float S_inv = 1.0f / S;

    // Kalman Gain K
    float K0 = P[0][0] * S_inv;
    float K1 = P[1][0] * S_inv;
    float K2 = P[2][0] * S_inv;

    // State update
    float y = measurement - x[0];
    x[0] = x[0] + K0 * y;
    x[1] = x[1] + K1 * y;
    x[2] = x[2] + K2 * y;

    // Covariance update: P = (I - K*H)*P
    float P00_old = P[0][0];
    float P01_old = P[0][1];
    float P02_old = P[0][2];
    float P11_old = P[1][1];
    float P12_old = P[1][2];
    float P22_old = P[2][2];

    P[0][0] = (1.0f - K0) * P00_old;
    P[0][1] = (1.0f - K0) * P01_old;
    P[0][2] = (1.0f - K0) * P02_old;

    P[1][0] = P[0][1];
    P[1][1] = -K1 * P01_old + P11_old;
    P[1][2] = -K1 * P02_old + P12_old;

    P[2][0] = P[0][2];
    P[2][1] = P[1][2];
    P[2][2] = -K2 * P02_old + P22_old;
}