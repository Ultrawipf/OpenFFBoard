/*
 * Motor driver and encoder class interfaces (MotorDriver.cpp, Encoder.cpp, ClassChooser.h)
 *
 * Tests the behaviour of the base classes that every driver and encoder inherits and
 * verifies that MockMotorDriver and MockEncoder can be used wherever the firmware
 * expects a MotorDriver or Encoder (direct, via base class pointer and via ClassChooser).
 */
#include "doctest.h"
#include "TestHelpers.h"
#include "FirmwareFixture.h"
#include "MockEncoder.h"
#include "MockMotorDriver.h"

#include <memory>

using doctest::Approx;

TEST_SUITE("Encoder") {

TEST_CASE("base encoder is a dummy without position") {
	Encoder encoder;
	CHECK(encoder.getEncoderType() == EncoderType::NONE);
	CHECK(encoder.getCpr() == 0);
	CHECK(encoder.getPos() == 0);
	CHECK(encoder.getPos_f() == 0.0f); // No division by zero without cpr
	CHECK(encoder.getPosAbs_f() == 0.0f);
	CHECK(encoder.getInfo().id == CLSID_ENCODER_NONE);
	CHECK(encoder.getClassType() == ClassType::Encoder);
}

TEST_CASE("mock encoder reports the position set by the test") {
	MockEncoder mock(10000);
	Encoder &encoder = mock; // Used through the firmware interface

	CHECK(encoder.getEncoderType() == EncoderType::incremental);
	CHECK(encoder.getCpr() == 10000);
	CHECK(encoder.getPos() == 0);

	mock.setRawPosition(2500);
	CHECK(encoder.getPos() == 2500);
	CHECK(encoder.getPos_f() == Approx(0.25)); // Rotations. Scaling is done by the Encoder base class

	mock.setDegrees(-180);
	CHECK(encoder.getPos() == -5000);
	CHECK(encoder.getPos_f() == Approx(-0.5));

	mock.setRotations(3);
	CHECK(encoder.getPos() == 30000);
	CHECK(mock.readCount > 0);
}

TEST_CASE("mock encoder position can be redefined by the firmware") {
	MockEncoder mock(10000);
	Encoder &encoder = mock;
	mock.setRawPosition(1234);

	encoder.setPos(0); // Zero at the current position
	CHECK(encoder.getPos() == 0);
	mock.setRawPosition(1334);
	CHECK(encoder.getPos() == 100);

	encoder.setPos(5000);
	CHECK(encoder.getPos() == 5000);
	CHECK(mock.setPosHistory == std::vector<int32_t>{0, 5000});
}

TEST_CASE("mock absolute encoder keeps the absolute position when zeroed") {
	MockEncoder mock(4096, EncoderType::absolute);
	Encoder &encoder = mock;
	mock.setRawPosition(1024);
	encoder.setPos(0);

	CHECK(encoder.getEncoderType() == EncoderType::absolute);
	CHECK(encoder.getPos() == 0);
	CHECK(encoder.getPosAbs() == 1024);
	CHECK(encoder.getPosAbs_f() == Approx(0.25));
}

TEST_CASE("mock incremental encoder has no separate absolute position") {
	MockEncoder mock(4096);
	mock.setRawPosition(1024);
	mock.setPos(0);
	CHECK(mock.getPosAbs() == 0);
}

TEST_CASE("mock encoder direction can be reversed") {
	MockEncoder mock(1000);
	mock.setRawPosition(250);
	mock.reversed = true;
	CHECK(mock.getPos() == -250);
}

TEST_CASE("mock encoder can be created by a class chooser") {
	const std::vector<class_entry<Encoder>> encoders = {add_class<Encoder, Encoder>(0), MockEncoder::classEntry()};
	ClassChooser<Encoder> chooser(encoders);

	CHECK(chooser.isValidClassId(MockEncoder::SELECTION_ID));
	CHECK_FALSE(chooser.isValidClassId(MockEncoder::SELECTION_ID + 1));

	std::unique_ptr<Encoder> created(chooser.Create(MockEncoder::SELECTION_ID));
	REQUIRE(created != nullptr);
	CHECK(dynamic_cast<MockEncoder *>(created.get()) != nullptr);
	CHECK(created->getSelectionID() == MockEncoder::SELECTION_ID);
	CHECK(std::string(created->getInfo().name) == "Mock encoder");

	std::unique_ptr<Encoder> none(chooser.Create(0));
	REQUIRE(none != nullptr);
	CHECK(none->getEncoderType() == EncoderType::NONE);

	CHECK(chooser.Create(99) == nullptr);
}

} // TEST_SUITE

TEST_SUITE("MotorDriver") {

TEST_CASE("base motor driver is a dummy that is always ready") {
	MotorDriver driver;
	CHECK(driver.motorReady());
	CHECK_FALSE(driver.hasIntegratedEncoder());
	CHECK(driver.getInfo().id == CLSID_MOT_NONE);
	CHECK(driver.getClassType() == ClassType::Motordriver);
	CHECK(driver.getDrvSlewRate() == MAX_SLEW_RATE);
	CHECK_FALSE(driver.startSlewRateCalibration());
	CHECK_FALSE(driver.isSlewRateCalibrationInProgress());

	// Must always return an encoder. Without assignment it is a dummy
	REQUIRE(driver.getEncoder() != nullptr);
	CHECK(driver.getEncoder()->getEncoderType() == EncoderType::NONE);

	driver.turn(1000); // No effect, must not crash
	driver.stopMotor();
	driver.startMotor();
	driver.emergencyStop();
}

TEST_CASE("mock motor driver records the requested torque") {
	MockMotorDriver mock;
	MotorDriver &driver = mock; // Used through the firmware interface

	driver.turn(1000);
	driver.turn(-32768);
	driver.turn(32767);
	CHECK(mock.lastTorque == 32767);
	CHECK(mock.torqueHistory == std::vector<int16_t>{1000, -32768, 32767});
}

TEST_CASE("mock motor driver records start and stop") {
	MockMotorDriver mock;
	MotorDriver &driver = mock;

	driver.startMotor();
	CHECK(mock.running);
	driver.turn(500);

	driver.stopMotor();
	CHECK_FALSE(mock.running);
	CHECK(mock.lastTorque == 0); // Base class stop sets the torque to 0
	CHECK(mock.startCalls == 1);
	CHECK(mock.stopCalls == 1);
}

TEST_CASE("emergency stop stops the motor and reset restarts it") {
	MockMotorDriver mock;
	MotorDriver &driver = mock;
	driver.startMotor();
	driver.turn(500);

	driver.emergencyStop(false);
	CHECK(mock.emergency);
	CHECK_FALSE(mock.running);
	CHECK(mock.lastTorque == 0);

	driver.emergencyStop(true);
	CHECK_FALSE(mock.emergency);
	CHECK(mock.running);
	CHECK(mock.emergencyStopCalls == 2);
}

TEST_CASE("mock motor driver state is controlled by the test") {
	MockMotorDriver mock;
	MotorDriver &driver = mock;

	CHECK(driver.motorReady());
	mock.ready = false;
	CHECK_FALSE(driver.motorReady());

	driver.setPowerLimit(1234);
	CHECK(mock.powerLimit == 1234);

	driver.setupDriver();
	CHECK(mock.setupCalls == 1);
}

TEST_CASE("mock motor driver slew rate calibration") {
	MockMotorDriver mock;
	MotorDriver &driver = mock;

	CHECK_FALSE(driver.startSlewRateCalibration()); // Not supported by default
	CHECK(mock.slewCalibrationRequests == 1);

	mock.supportsSlewCalibration = true;
	CHECK(driver.startSlewRateCalibration());
	CHECK(driver.isSlewRateCalibrationInProgress());

	mock.slewRate = 1500;
	mock.slewCalibrationInProgress = false;
	CHECK_FALSE(driver.isSlewRateCalibrationInProgress());
	CHECK(driver.getDrvSlewRate() == 1500);
}

TEST_CASE("external encoder can be assigned to a driver") {
	MockMotorDriver mock;
	MotorDriver &driver = mock;
	CHECK_FALSE(driver.hasIntegratedEncoder());

	std::shared_ptr<Encoder> encoder = std::make_shared<MockEncoder>(2000);
	driver.setEncoder(encoder);
	CHECK(driver.getEncoder() == encoder.get());
	CHECK(driver.getEncoder()->getCpr() == 2000);
}

TEST_CASE("mock motor driver can provide an integrated encoder") {
	MockMotorDriver mock;
	MotorDriver &driver = mock;
	std::shared_ptr<MockEncoder> encoder = mock.useIntegratedEncoder();

	CHECK(driver.hasIntegratedEncoder());
	CHECK(driver.getEncoder() == encoder.get());
	encoder->setRawPosition(77);
	CHECK(driver.getEncoder()->getPos() == 77);
}

TEST_CASE("mock motor driver can be created by a class chooser") {
	const std::vector<class_entry<MotorDriver>> drivers = {add_class<MotorDriver, MotorDriver>(0), MockMotorDriver::classEntry()};
	ClassChooser<MotorDriver> chooser(drivers);
	int instancesBefore = MockMotorDriver::instances();

	{
		std::unique_ptr<MotorDriver> created(chooser.Create(MockMotorDriver::SELECTION_ID));
		REQUIRE(created != nullptr);
		CHECK(dynamic_cast<MockMotorDriver *>(created.get()) != nullptr);
		CHECK(created->getSelectionID() == MockMotorDriver::SELECTION_ID);
		CHECK(MockMotorDriver::instances() == instancesBefore + 1);
	}
	CHECK(MockMotorDriver::instances() == instancesBefore);

	// Drivers can refuse to be created if their resources are in use
	MockMotorDriver::creatable() = false;
	CHECK_FALSE(chooser.isCreatable(MockMotorDriver::SELECTION_ID));
	CHECK(chooser.Create(MockMotorDriver::SELECTION_ID) == nullptr);
	MockMotorDriver::creatable() = true;
	CHECK(chooser.isCreatable(MockMotorDriver::SELECTION_ID));
}

TEST_CASE_FIXTURE(FirmwareFixture, "class chooser lists the selectable classes for the command system") {
	const std::vector<class_entry<MotorDriver>> drivers = {add_class<MotorDriver, MotorDriver>(0), MockMotorDriver::classEntry()};
	ClassChooser<MotorDriver> chooser(drivers);

	std::vector<CommandReply> replies;
	chooser.replyAvailableClasses(replies);
	REQUIRE(replies.size() == 2);
	CHECK(replies[0].reply == "0:1:None");
	CHECK(replies[1].reply == "60:1:Mock driver");
	CHECK(replies[1].adr == MockMotorDriver::SELECTION_ID);
	CHECK(replies[1].val == CLSID_CUSTOM);
}

} // TEST_SUITE
