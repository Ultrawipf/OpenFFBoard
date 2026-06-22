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
    q_continuous = 10000.0f;
}

// Sets the process noise variance parameter
void KinematicKalman::setQ(float q) {
    q_continuous = q;
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

    float dt2 = dt * dt;

    // Compute helper matrix M = F * P representing the intermediate covariance multiplication
    float M00 = P[0][0] + dt * P[1][0] + 0.5f * dt2 * P[2][0];
    float M01 = P[0][1] + dt * P[1][1] + 0.5f * dt2 * P[2][1];
    float M02 = P[0][2] + dt * P[1][2] + 0.5f * dt2 * P[2][2];

    float M10 = P[1][0] + dt * P[2][0];
    float M11 = P[1][1] + dt * P[2][1];
    float M12 = P[1][2] + dt * P[2][2];

    float M20 = P[2][0];
    float M21 = P[2][1];
    float M22 = P[2][2];

    // Compute powers of dt to scale the discrete process noise matrix Q
    float dt3 = dt2 * dt;
    float dt4 = dt3 * dt;
    float dt5 = dt4 * dt;

    // Populate discrete process noise covariance matrix Q (CWNA model)
    float Q00 = q_continuous * dt5 / 20.0f;
    float Q01 = q_continuous * dt4 / 8.0f;
    float Q02 = q_continuous * dt3 / 6.0f;
    float Q11 = q_continuous * dt3 / 3.0f;
    float Q12 = q_continuous * dt2 / 2.0f;
    float Q22 = q_continuous * dt;

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

// Corrects the estimated state and covariance using the new position measurement
void KinematicKalman::updatePosition(float z_theta, float R_position) {
    // Calculate innovation covariance S (H * P * H^T + R)
    float S = P[0][0] + R_position;
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
    float y = z_theta - x[0];
    float new_x0 = x[0] + K0 * y;
    float new_x1 = x[1] + K1 * y;
    float new_x2 = x[2] + K2 * y;

    sequence.fetch_add(1, std::memory_order_release);
    x[0] = new_x0;
    x[1] = new_x1;
    x[2] = new_x2;
    sequence.fetch_add(1, std::memory_order_release);

    // Update covariance matrix using the Strict Joseph Form: P = (I - K*H) * P * (I - K*H)^T + K * R * K^T
    // Cache current covariance values
    float P00 = P[0][0], P01 = P[0][1], P02 = P[0][2];
    float P11 = P[1][1], P12 = P[1][2], P22 = P[2][2];

    float j00 = 1.0f - K0;
    
    // Intermediate M = (I - K*H) * P
    float m00 = j00 * P00;
    float m01 = j00 * P01;
    float m02 = j00 * P02;
    float m10 = P01 - K1 * P00;
    float m11 = P11 - K1 * P01;
    float m12 = P12 - K1 * P02;
    float m20 = P02 - K2 * P00;
    float m21 = P12 - K2 * P01;
    float m22 = P22 - K2 * P02;

    // Final P = M * (I - K*H)^T + K * R * K^T
    P[0][0] = m00 * j00 + K0 * K0 * R_position;
    P[0][1] = m01 - m00 * K1 + K0 * K1 * R_position;
    P[0][2] = m02 - m00 * K2 + K0 * K2 * R_position;

    P[1][1] = m11 - m10 * K1 + K1 * K1 * R_position;
    P[1][2] = m12 - m10 * K2 + K1 * K2 * R_position;

    P[2][2] = m22 - m20 * K2 + K2 * K2 * R_position;

    // Symmetric matrix assignment
    P[1][0] = P[0][1];
    P[2][0] = P[0][2];
    P[2][1] = P[1][2];
}

// Corrects the estimated state and covariance using velocity measurement with OOSM and ANC
void KinematicKalman::updateVelocity(float z_omega, float dt_pulse) {
    // Clamp dt_pulse to prevent division by zero and Floating-Point Overflow on dt_pulse^4
    if (dt_pulse < 1e-4f) {
        dt_pulse = 1e-4f;
    }
    
    float dt2 = dt_pulse * dt_pulse;
    float dt4 = dt2 * dt2;
    float R_omega = q_continuous / dt4; // Loi adaptative ANC

    // OOSM measurement matrix for velocity (H = [0, 1, -0.5*dt_pulse])
    float H2 = -0.5f * dt_pulse;

    // hP = H * P
    float hP0 = P[1][0] + H2 * P[2][0];
    float hP1 = P[1][1] + H2 * P[2][1];
    float hP2 = P[1][2] + H2 * P[2][2];

    // S = H * P * H^T + R_omega
    float S = hP1 + H2 * hP2 + R_omega;
    if (S <= 0.0f) {
        return;
    }

    float S_inv = 1.0f / S;

    // K = P * H^T * S_inv
    float K0 = (P[0][1] + H2 * P[0][2]) * S_inv;
    float K1 = (P[1][1] + H2 * P[1][2]) * S_inv;
    float K2 = (P[2][1] + H2 * P[2][2]) * S_inv;

    // Correct the state vector (y = z_omega - H * x)
    float y = z_omega - (x[1] + H2 * x[2]);

    float new_x0 = x[0] + K0 * y;
    float new_x1 = x[1] + K1 * y;
    float new_x2 = x[2] + K2 * y;

    sequence.fetch_add(1, std::memory_order_release);
    x[0] = new_x0;
    x[1] = new_x1;
    x[2] = new_x2;
    sequence.fetch_add(1, std::memory_order_release);

    // Update covariance matrix using Strict Joseph Form
    // Intermediate M = (I - K*H) * P
    float P00 = P[0][0], P01 = P[0][1], P02 = P[0][2];
    float P10 = P[1][0], P11 = P[1][1], P12 = P[1][2];
    float P20 = P[2][0], P21 = P[2][1], P22 = P[2][2];

    float m00 = P00 - K0 * P10 - K0 * H2 * P20;
    float m01 = P01 - K0 * P11 - K0 * H2 * P21;
    float m02 = P02 - K0 * P12 - K0 * H2 * P22;

    float m10 = (1.0f - K1) * P10 - K1 * H2 * P20;
    float m11 = (1.0f - K1) * P11 - K1 * H2 * P21;
    float m12 = (1.0f - K1) * P12 - K1 * H2 * P22;

    float m20 = -K2 * P10 + (1.0f - K2 * H2) * P20;
    float m21 = -K2 * P11 + (1.0f - K2 * H2) * P21;
    float m22 = -K2 * P12 + (1.0f - K2 * H2) * P22;

    // IKH^T components (I - K*H)^T
    float IKH_00 = 1.0f, IKH_01 = -K0, IKH_02 = -K0 * H2;
    float IKH_10 = 0.0f, IKH_11 = 1.0f - K1, IKH_12 = -K1 * H2;
    float IKH_20 = 0.0f, IKH_21 = -K2, IKH_22 = 1.0f - K2 * H2;

    // Final P = M * (I - K*H)^T + K * R_omega * K^T
    P[0][0] = m00 * IKH_00 + m01 * IKH_01 + m02 * IKH_02 + K0 * K0 * R_omega;
    P[0][1] = m00 * IKH_10 + m01 * IKH_11 + m02 * IKH_12 + K0 * K1 * R_omega;
    P[0][2] = m00 * IKH_20 + m01 * IKH_21 + m02 * IKH_22 + K0 * K2 * R_omega;

    P[1][1] = m10 * IKH_10 + m11 * IKH_11 + m12 * IKH_12 + K1 * K1 * R_omega;
    P[1][2] = m10 * IKH_20 + m11 * IKH_21 + m12 * IKH_22 + K1 * K2 * R_omega;

    P[2][2] = m20 * IKH_20 + m21 * IKH_21 + m22 * IKH_22 + K2 * K2 * R_omega;

    // Symmetric matrix assignment
    P[1][0] = P[0][1];
    P[2][0] = P[0][2];
    P[2][1] = P[1][2];
}