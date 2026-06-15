#include "KinematicKalman.h"

// Constructor: Initializes the filter state to zero and error covariance to identity
KinematicKalman::KinematicKalman() {
    // Set initial state vector [theta, omega, alpha] to zero
    x[0] = 0.0;
    x[1] = 0.0;
    x[2] = 0.0;

    // Set initial covariance matrix P to identity (uncorrelated states with unit variance)
    P[0][0] = 1.0; P[0][1] = 0.0; P[0][2] = 0.0;
    P[1][0] = 0.0; P[1][1] = 1.0; P[1][2] = 0.0;
    P[2][0] = 0.0; P[2][1] = 0.0; P[2][2] = 1.0;

    // Set default process noise variance representing acceleration model uncertainty
    q_var = 10000.0;
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

    double dt_d = dt;

    // Predict new position and velocity based on kinematic integration
    double theta = x[0] + x[1] * dt_d + 0.5 * x[2] * dt_d * dt_d;
    double omega = x[1] + x[2] * dt_d;
    
    sequence.fetch_add(1, std::memory_order_release);
    x[0] = theta;
    x[1] = omega;
    sequence.fetch_add(1, std::memory_order_release);
    // x[2] = x[2] (constant acceleration model)

    // Compute helper matrix M = F * P representing the intermediate covariance multiplication
    double M00 = P[0][0] + dt_d * P[1][0] + 0.5 * dt_d * dt_d * P[2][0];
    double M01 = P[0][1] + dt_d * P[1][1] + 0.5 * dt_d * dt_d * P[2][1];
    double M02 = P[0][2] + dt_d * P[1][2] + 0.5 * dt_d * dt_d * P[2][2];

    double M10 = P[1][0] + dt_d * P[2][0];
    double M11 = P[1][1] + dt_d * P[2][1];
    double M12 = P[1][2] + dt_d * P[2][2];

    double M20 = P[2][0];
    double M21 = P[2][1];
    double M22 = P[2][2];

    // Compute powers of dt to scale the discrete process noise matrix Q
    double dt2 = dt_d * dt_d;
    double dt3 = dt2 * dt_d;
    double dt4 = dt3 * dt_d;

    // Populate discrete process noise covariance matrix Q (integrated random walk on acceleration)
    double Q00 = q_var * dt4 * 0.25;
    double Q01 = q_var * dt3 * 0.5;
    double Q02 = q_var * dt2 * 0.5;
    double Q11 = q_var * dt2;
    double Q12 = q_var * dt_d;
    double Q22 = q_var;

    // Compute predicted covariance matrix P = M * F^T + Q analytically to save CPU cycles
    P[0][0] = M00 + M01 * dt_d + M02 * 0.5 * dt2 + Q00;
    P[0][1] = M01 + M02 * dt_d + Q01;
    P[0][2] = M02 + Q02;

    P[1][0] = P[0][1]; // Symmetric matrix optimization
    P[1][1] = M11 + M12 * dt_d + Q11;
    P[1][2] = M12 + Q12;

    P[2][0] = P[0][2]; // Symmetric matrix optimization
    P[2][1] = P[1][2]; // Symmetric matrix optimization
    P[2][2] = M22 + Q22;
}

// Corrects the estimated state and covariance using the new position measurement and noise R
void KinematicKalman::update(float measurement, float R) {
    // Cast to double for internal computation precision
    double meas_d = measurement;
    double R_d = R;

    // Calculate innovation covariance S (H * P * H^T + R)
    double S = P[0][0] + R_d;
    if (S <= 0.0) {
        return; // Guard against negative covariance or division by zero
    }

    // Invert S to solve for the Kalman gain
    double S_inv = 1.0 / S;

    // Calculate optimal Kalman gains K = P * H^T * S_inv (observing only position)
    double K0 = P[0][0] * S_inv;
    double K1 = P[1][0] * S_inv;
    double K2 = P[2][0] * S_inv;

    // Correct the state vector using the measurement error (y = measurement - estimate)
    double y = meas_d - x[0];
    double new_x0 = x[0] + K0 * y;
    double new_x1 = x[1] + K1 * y;
    double new_x2 = x[2] + K2 * y;

    sequence.fetch_add(1, std::memory_order_release);
    x[0] = new_x0;
    x[1] = new_x1;
    x[2] = new_x2;
    sequence.fetch_add(1, std::memory_order_release);

    // Update covariance matrix using the Strict Joseph Form: P = (I - K*H) * P * (I - K*H)^T + K * R * K^T
    // Cache current covariance values
    double P00 = P[0][0];
    double P01 = P[0][1];
    double P02 = P[0][2];
    double P11 = P[1][1];
    double P12 = P[1][2];
    double P22 = P[2][2];

    double j00 = 1.0 - K0;
    
    // Intermediate M = (I - K*H) * P
    double m00 = j00 * P00;
    double m01 = j00 * P01;
    double m02 = j00 * P02;
    double m10 = P01 - K1 * P00;
    double m11 = P11 - K1 * P01;
    double m12 = P12 - K1 * P02;
    double m20 = P02 - K2 * P00;
    double m21 = P12 - K2 * P01;
    double m22 = P22 - K2 * P02;

    // Final P = M * (I - K*H)^T + K * R * K^T
    P[0][0] = m00 * j00 + K0 * K0 * R_d;
    P[0][1] = m01 - m00 * K1 + K0 * K1 * R_d;
    P[0][2] = m02 - m00 * K2 + K0 * K2 * R_d;

    P[1][1] = m11 - m10 * K1 + K1 * K1 * R_d;
    P[1][2] = m12 - m10 * K2 + K1 * K2 * R_d;

    P[2][2] = m22 - m20 * K2 + K2 * K2 * R_d;

    // Symmetric matrix assignment
    P[1][0] = P[0][1];
    P[2][0] = P[0][2];
    P[2][1] = P[1][2];
}