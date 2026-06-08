#ifndef KINEMATICKALMAN_H_
#define KINEMATICKALMAN_H_

/**
 * @brief A 3-state Kinematic Kalman Filter for real-time motor shaft tracking.
 * Establishes states for Position (theta), Velocity (omega), and Acceleration (alpha).
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
};

#endif // KINEMATICKALMAN_H_