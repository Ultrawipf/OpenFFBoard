#include "KinematicKalman.h"

// Constructor: Initializes the filter state to zero and error covariance to identity
KinematicKalman::KinematicKalman() {
    // Set initial state vector [theta, omega, alpha] to zero
    x[0] = 0.0f;
    x[1] = 0.0f;
    x[2] = 0.0f;

    // Set initial covariance matrix P to identity (uncorrelated states with unit variance)
    P[0][0] = 1.0f; P[0][1] = 0.0f; P[0][2] = 0.0f;
    P[1][0] = 0.0f; P[1][1] = 1.0f; P[1][2] = 0.0f;
    P[2][0] = 0.0f; P[2][1] = 0.0f; P[2][2] = 1.0f;

    // Set default process noise variance representing acceleration model uncertainty
    q_var = 1000000.0f;
}

// Sets the process noise variance parameter
void KinematicKalman::setQ(float q) {
    q_var = q;
}

// Retrieves estimated position (theta)
float KinematicKalman::getTheta() const {
    uint32_t seq;
    float val;
    do {
        seq = sequence.load(std::memory_order_acquire);
        val = x[0];
    } while ((seq & 1) || sequence.load(std::memory_order_acquire) != seq);
    return val;
}

// Retrieves estimated velocity (omega)
float KinematicKalman::getOmega() const {
    uint32_t seq;
    float val;
    do {
        seq = sequence.load(std::memory_order_acquire);
        val = x[1];
    } while ((seq & 1) || sequence.load(std::memory_order_acquire) != seq);
    return val;
}

// Retrieves estimated acceleration (alpha)
float KinematicKalman::getAlpha() const {
    uint32_t seq;
    float val;
    do {
        seq = sequence.load(std::memory_order_acquire);
        val = x[2];
    } while ((seq & 1) || sequence.load(std::memory_order_acquire) != seq);
    return val;
}

// Predicts state and covariance forward by time step dt using kinematic equations
void KinematicKalman::predict(float dt) {
    // Ignore invalid time steps
    if (dt <= 0.0f) {
        return;
    }

    // Predict new position and velocity based on kinematic integration
    float theta = x[0] + x[1] * dt + 0.5f * x[2] * dt * dt;
    float omega = x[1] + x[2] * dt;
    
    sequence.fetch_add(1, std::memory_order_release);
    x[0] = theta;
    x[1] = omega;
    sequence.fetch_add(1, std::memory_order_release);
    // x[2] = x[2] (constant acceleration model)

    // Compute helper matrix M = F * P representing the intermediate covariance multiplication
    float M00 = P[0][0] + dt * P[1][0] + 0.5f * dt * dt * P[2][0];
    float M01 = P[0][1] + dt * P[1][1] + 0.5f * dt * dt * P[2][1];
    float M02 = P[0][2] + dt * P[1][2] + 0.5f * dt * dt * P[2][2];

    float M10 = P[1][0] + dt * P[2][0];
    float M11 = P[1][1] + dt * P[2][1];
    float M12 = P[1][2] + dt * P[2][2];

    float M20 = P[2][0];
    float M21 = P[2][1];
    float M22 = P[2][2];

    // Compute powers of dt to scale the discrete process noise matrix Q
    float dt2 = dt * dt;
    float dt3 = dt2 * dt;
    float dt4 = dt3 * dt;

    // Populate discrete process noise covariance matrix Q (integrated random walk on acceleration)
    float Q00 = q_var * dt4 * 0.25f;
    float Q01 = q_var * dt3 * 0.5f;
    float Q02 = q_var * dt2 * 0.5f;
    float Q11 = q_var * dt2;
    float Q12 = q_var * dt;
    float Q22 = q_var;

    // Compute predicted covariance matrix P = M * F^T + Q analytically to save CPU cycles
    P[0][0] = M00 + M01 * dt + M02 * 0.5f * dt2 + Q00;
    P[0][1] = M01 + M02 * dt + Q01;
    P[0][2] = M02 + Q02;

    P[1][0] = P[0][1]; // Symmetric matrix optimization
    P[1][1] = M11 + M12 * dt + Q11;
    P[1][2] = M12 + Q12;

    P[2][0] = P[0][2]; // Symmetric matrix optimization
    P[2][1] = P[1][2]; // Symmetric matrix optimization
    P[2][2] = M22 + Q22;
}

// Corrects the estimated state and covariance using the new position measurement and noise R
void KinematicKalman::update(float measurement, float R) {
    // Calculate innovation covariance S (H * P * H^T + R)
    float S = P[0][0] + R;
    if (S <= 0.0f) {
        return; // Guard against negative covariance or division by zero
    }

    // Invert S to solve for the Kalman gain
    float S_inv = 1.0f / S;

    // Calculate optimal Kalman gains K = P * H^T * S_inv (observing only position)
    float K0 = P[0][0] * S_inv;
    float K1 = P[1][0] * S_inv;
    float K2 = P[2][0] * S_inv;

    // Correct the state vector using the measurement error (y = measurement - estimate)
    float y = measurement - x[0];
    float new_x0 = x[0] + K0 * y;
    float new_x1 = x[1] + K1 * y;
    float new_x2 = x[2] + K2 * y;

    sequence.fetch_add(1, std::memory_order_release);
    x[0] = new_x0;
    x[1] = new_x1;
    x[2] = new_x2;
    sequence.fetch_add(1, std::memory_order_release);

    // Cache current covariance values before updating the matrix
    float P00_old = P[0][0];
    float P01_old = P[0][1];
    float P02_old = P[0][2];
    float P11_old = P[1][1];
    float P12_old = P[1][2];
    float P22_old = P[2][2];

    // Update covariance matrix using the Joseph form projection P = (I - K*H) * P
    P[0][0] = (1.0f - K0) * P00_old;
    P[0][1] = (1.0f - K0) * P01_old;
    P[0][2] = (1.0f - K0) * P02_old;

    P[1][0] = P[0][1]; // Symmetric matrix optimization
    P[1][1] = -K1 * P01_old + P11_old;
    P[1][2] = -K1 * P02_old + P12_old;

    P[2][0] = P[0][2]; // Symmetric matrix optimization
    P[2][1] = P[1][2]; // Symmetric matrix optimization
    P[2][2] = -K2 * P02_old + P22_old;
}