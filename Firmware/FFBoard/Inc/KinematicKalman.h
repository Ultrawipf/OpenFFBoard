#ifndef KINEMATICKALMAN_H_
#define KINEMATICKALMAN_H_

#include <atomic>

/**
 * @brief A 3-state Kinematic Kalman Filter for real-time motor shaft tracking.
 * Establishes states for Position (theta), Velocity (omega), and Acceleration (alpha).
 * 
 * @details
 * System characteristics:
 * - Sample-rate independent: The filter uses the exact elapsed time (dt measured via micros()) passed to predict(dt) instead of a fixed frequency. This ensures perfect kinematic calculation, whether the encoder is polled at 10kHz or slowed down to 1kHz by its Scaler.
 * - Resolution-aware: R (Measurement Noise) is not just calculated via (step^2)/12, but makes the filter "Resolution-aware" in real-time. A low CPR will give a high R (more aggressive smoothing), while a high CPR will reduce R (maximum trust in the sensor and minimum latency).
 * 
 * Constant choices:
 * - q_var (Process Noise): Set very high (e.g., 1000000.0f) to tolerate enormous Jerk typical in Force Feedback applications.
 * - Integration constants (0.5 and 0.25): Found in the source code, these are not adjustable parameters but mathematical constants for discrete integration (Discrete White Noise Acceleration model: 1/2*a*t^2 for position, and 1/4*a^2*t^4 for covariance).
 */
class KinematicKalman {
public:
    // Initializes the state vector to zero, covariance matrix to identity, and process noise variance.
    KinematicKalman();
    ~KinematicKalman() = default;

    // Performs the state and covariance prediction step over the time step dt.
    void predict(float dt);

    // Updates the state and covariance estimates using the new position measurement and noise variance R.
    void update(float measurement, float R);

    // Sets the acceleration process noise variance (q).
    void setQ(float q);

    // Getter for the estimated angular position (theta) in radians.
    float getTheta() const;

    // Getter for the estimated angular velocity (omega) in radians/second.
    float getOmega() const;

    // Getter for the estimated angular acceleration (alpha) in radians/second^2.
    float getAlpha() const;

private:
    float x[3]; // State vector: [theta (position), omega (velocity), alpha (acceleration)]
    float P[3][3]; // Estimation error covariance matrix
    float q_var; // Acceleration process noise variance (model uncertainty)
    mutable std::atomic<uint32_t> sequence{0}; // SeqLock counter
};

#endif // KINEMATICKALMAN_H_