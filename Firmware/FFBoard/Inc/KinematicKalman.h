#ifndef KINEMATICKALMAN_H_
#define KINEMATICKALMAN_H_

class KinematicKalman {
public:
    KinematicKalman();
    ~KinematicKalman() = default;

    void predict(float dt);
    void update(float measurement, float R);
    void setQ(float q);

    float getTheta() const;
    float getOmega() const;
    float getAlpha() const;

private:
    float x[3]; // State vector: [theta, omega, alpha] (position, velocity, acceleration)
    float P[3][3]; // Covariance matrix
    float q_var; // Process noise variance
};

#endif // KINEMATICKALMAN_H_