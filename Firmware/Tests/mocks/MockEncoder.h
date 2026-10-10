/*
 * MockEncoder.h
 *
 * Encoder implementing the Encoder class interface without hardware.
 * The test sets the raw position in counts. The scaling functions of the
 * Encoder base class (getPos_f, getPosAbs_f) are the real firmware implementation.
 */
#ifndef HOSTTEST_MOCKENCODER_H_
#define HOSTTEST_MOCKENCODER_H_

#include "Encoder.h"
#include "ClassChooser.h"
#include <vector>

class MockEncoder : public Encoder {
public:
	static constexpr uint16_t SELECTION_ID = 60; //!< Class chooser id. Not used by any firmware encoder

	MockEncoder(uint32_t cpr = 10000, EncoderType type = EncoderType::incremental) : type(type) { this->cpr = cpr; }

	static inline ClassIdentifier info = {.name = "Mock encoder", .id = CLSID_CUSTOM, .visibility = ClassVisibility::visible};
	const ClassIdentifier getInfo() override { return info; }

	// --- Encoder interface ---
	EncoderType getEncoderType() override { return type; }

	int32_t getPos() override {
		readCount++;
		return (reversed ? -position : position) - offset;
	}

	/** Absolute position ignores offsets set by setPos like absolute encoders do */
	int32_t getPosAbs() override { return type == EncoderType::absolute ? (reversed ? -position : position) : getPos(); }

	void setPos(int32_t pos) override {
		setPosHistory.push_back(pos);
		offset = (reversed ? -position : position) - pos;
	}

	// --- Test helpers ---
	void setCpr(uint32_t cpr) { this->cpr = cpr; }

	/** Sets the raw shaft position in counts */
	void setRawPosition(int32_t counts) { position = counts; }

	/** Sets the raw shaft position in rotations */
	void setRotations(float rotations) { position = (int32_t)(rotations * (float)cpr); }

	/** Sets the raw shaft position in degrees */
	void setDegrees(float degrees) { setRotations(degrees / 360.0f); }

	/** Class registry entry to make the mock selectable by a ClassChooser<Encoder> */
	static class_entry<Encoder> classEntry() { return add_class<MockEncoder, Encoder>(SELECTION_ID); }

	// --- State ---
	EncoderType type;
	int32_t position = 0; //!< Raw position in counts
	int32_t offset = 0;	  //!< Offset caused by setPos
	bool reversed = false;
	uint32_t readCount = 0;
	std::vector<int32_t> setPosHistory;
};

#endif
