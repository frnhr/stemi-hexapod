/*

This file is part of STEMI hexapod library.

Copyright (C) 2017 STEMI d.o.o.
Authors: Luka Fucek, et al.
Contact: info@stemi.education
Web: https://www.stemi.education/

STEMI hexapod library is free software exclusively
for non-commercial activities.

In case of developing any comercial activities involving STEMI
hexapod library please contact stemi.education for providing custom
license at: info@stemi.education.

For non-commercial activities you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.

STEMI hexapod library is
distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with STEMI hexapod library.
If not, see http://www.gnu.org/licenses/.

For additional information please check http://www.stemi.education.

*/


#include "Robot.h"


float saturate(float value, float minimum, float maximum)
{
	return _min(maximum, _max(minimum, value));
}

Robot::Robot(SharedData *sharedDataNew) : body(sharedDataNew), hardware(sharedDataNew), grip(sharedDataNew, body)
{
	sharedData = sharedDataNew;
	sharedData->servoCtrl.power = 1;
	delay(10);
	go();
}

void Robot::wakeUp()
{
	//hardware.servoPower(1);
	body.setLinMode(LIN_MODE_PERMANENT);
	go(); //make a first run(), put the legs in the air
	

	//TODO hardware.setAllLEDs(100, RgbColor(0, 255, 255));

	unsigned long startTime = millis(); //start measuring time
	Serial.println("Waiting for 3 sec ...");
	delay(3000);


	body.setLinMode(LIN_MODE_PERMANENT);

	startTime = millis();
	while (millis() - startTime < 2000) //wait for 2 seconds until the robot becomes responsive (safety reasons)
	{
		go(); //run the algorithm, just to stand up
	}
	//hardware.setAllLEDs(30, RgbColor(0, 255, 255));
	Serial.println("STEMI has waken up!");
}

int Robot::go()
{
	body.run();
	body.packQArray();
	wait(); //wait until enough time has passed
	//mutex TODO
	//hardware.servoWrite(body.qAll);
	for (int i = 0; i < 18; i++)
		sharedData->servoCtrl.servoAngles[i] = body.qAll[i];
	measureTime(); //start measuring time

	return 0;
}

void Robot::measureTime()
{
	startTime = millis();
}

void Robot::wait()
{
	while (millis() - startTime < body.ts * 1000)
	{
		delay(1);
	}
	//Serial.println(millis() - startTime);
	if (millis() - startTime > body.ts * 1000)
	{
		//Serial.print("This cycle took more than expected: ");
		//Serial.println(millis() - startTime);
	}
}

void Robot::loopMove() {
	//Serial.println("pocetak");
	while (sharedData->moveCtrl.nMove > 0)
	{
		//Serial.println("before");
		go();
		//Serial.println("after");
	}
}
void Robot::loopHome(float timeWaiting) {
	long startTime2 = millis();

	while (!body.checkHomeMark() || (millis() - startTime2 < timeWaiting * 1000))
	{
		sharedData->moveCtrl.nMove = 0;
		go();
	}
	go();
}
void Robot::goHome(float time)
{
	loopHome(time);
}
void Robot::goForward(float distance)
{
	distance *= 1.08;
	body.setMoveParam(goSpeed, PI / 2, 0, abs(distance / goSpeed*sharedData->param.freq));
	loopMove();
}

void Robot::goBackwards(float distance)
{
	distance *= 1.08;
	body.setMoveParam(goSpeed, -PI / 2, 0, abs(distance / goSpeed*sharedData->param.freq));
	loopMove();
}

void Robot::goLeft(float distance)
{
	distance *= 1.08;
	body.setMoveParam(goSpeed, PI, 0, abs(distance / goSpeed*sharedData->param.freq));
	loopMove();
}

void Robot::goRight(float distance)
{
	distance *= 1.08;
	body.setMoveParam(goSpeed, 0, 0, abs(distance / goSpeed*sharedData->param.freq));
	loopMove();
}

void Robot::turnLeft(float distance)
{
	distance *= 1.15;
	body.setMoveParam(0, 0, -turnSpeed, abs((distance*PI / 180) / turnSpeed*sharedData->param.freq));
	loopMove();
}

void Robot::turnRight(float distance)
{
	distance *= 1.15;
	body.setMoveParam(0, 0, turnSpeed, abs((distance*PI / 180) / turnSpeed*sharedData->param.freq));
	loopMove();
}

void Robot::setRotation(float xTilt, float yTilt, float zTilt)
{
	sharedData->moveCtrl.tr[3] = saturate(zTilt, -0.15, 0.15);
	sharedData->moveCtrl.tr[4] = saturate(xTilt, -0.15, 0.15);
	sharedData->moveCtrl.tr[5] = saturate(yTilt, -0.15, 0.15);
}

void Robot::setTranslation(float xRotation, float yRotation, float zRotation)
{
	sharedData->moveCtrl.tr[0] = saturate(zRotation, 1, 7);
	sharedData->moveCtrl.tr[1] = saturate(xRotation, -2, 2);
	sharedData->moveCtrl.tr[2] = saturate(yRotation, -2, 2);
}

void Robot::danceHip(float angle, float translation, float time)
{
	sharedData->moveCtrl.tr[3] = angle;
	sharedData->moveCtrl.tr[1] = translation;
	goHome(time * 2 / 3);
	Serial.print(" time: ");
	Serial.println(time * 2 / 3);
	
}

void Robot::resetPose()
{
	setRotation(0, 0, 0);
	setTranslation(0, 0, sharedData->moveCtrl.tr[0]);
}

//check all touch inputs and change state and variables accordingly
void Robot::checkTouch()
{
	hardware.touch.checkTouch();
	if (hardware.touch.isTouchDetected())
	{
		int touchState = hardware.touch.getTouchPattern(true);

		//make changes based on input and current robotMode
		switch (robotMode)
		{
		case PRE_CALIBRATION_MODE:
			switch (touchState)
			{
			case 5:
				robotMode = CALIBRATION_MODE;
				//calibrationLegSelected = 0; 
				//calibrationServoLayerSelected = 0;
				break;
			default:
				robotMode = WALKING_MODE;
				break;
			}
			break;
		case CALIBRATION_MODE:
			switch (touchState)
			{
				//changing calibration value
			case 1:
				sharedData->servoCtrl.calibrationOffsetBytes[calibrationLegSelected * 3 + calibrationServoLayerSelected] =
					body.saturate((sharedData->servoCtrl.calibrationOffsetBytes[calibrationLegSelected * 3 + calibrationServoLayerSelected] - 10), -100, 100);
				for (int i = 0; i < 18; i++)
				{
					Serial.print(sharedData->servoCtrl.calibrationOffsetBytes[i]);
					Serial.print(" ");
				}
				Serial.println();
				Serial.println((sharedData->servoCtrl.calibrationOffsetBytes[calibrationLegSelected * 3 + calibrationServoLayerSelected] + 100.0) / 200 * 255);
				break;
			case 4:
				sharedData->servoCtrl.calibrationOffsetBytes[calibrationLegSelected * 3 + calibrationServoLayerSelected] =
					body.saturate((sharedData->servoCtrl.calibrationOffsetBytes[calibrationLegSelected * 3 + calibrationServoLayerSelected] + 10), -100, 100);
				for (int i = 0; i < 18; i++)
				{
					Serial.print(sharedData->servoCtrl.calibrationOffsetBytes[i]);
					Serial.print(" ");
				}
				Serial.println();
				Serial.println((sharedData->servoCtrl.calibrationOffsetBytes[calibrationLegSelected*3+ calibrationServoLayerSelected] + 100.0) / 200 * 255);
				break;
				//changing selected servo
			case 3:
				calibrationServoLayerSelected = (calibrationServoLayerSelected + 1) % 3;
				nudgeServos = 1;
				break;
			case 6:
				calibrationServoLayerSelected = (calibrationServoLayerSelected + 1) % 3;
				nudgeServos = 1;
				break;
			case 2:
				calibrationLegSelectedCounter = (calibrationLegSelectedCounter + 1) % 6;
				calibrationLegSelected = calibrationLegSelectedMap[calibrationLegSelectedCounter];
				Serial.println(calibrationLegSelected);
				break;
				//state change
			case 5:
				robotMode = WALKING_MODE;
				//pack and store calibrationd data
				//int8_t linDataByte[18];
				//for (int i = 0; i<18; i++)
					//linDataByte[i] = hardware.calibrationOffsets[]
				//hardware.storeCalibrationData(linDataByte);
				sharedData->servoCtrl.store = 1;//signal servo task to store current calib offsets
				body.setLinMode(0); //migrate to shareddata
				break;
			//reset values
			case 7:
				//TODO implement reset shareddata.servo.calibReset = 1;
				//TODO store it after immediately
				//for (int i = 0; i < 18; i++)
					//hardware.calibrationOffsetBytes[i] = 0;
				//hardware.storeCalibrationData(hardware.calibrationOffsetBytes);
				break;
			}
			break;
		case WALKING_MODE:
			switch (touchState)
			{
			case 1:
				sharedData->moveCtrl.tr[2] = body.saturate((sharedData->moveCtrl.tr[2] - 3), -3, 3);
				break;
			case 4:
				sharedData->moveCtrl.tr[2] = body.saturate((sharedData->moveCtrl.tr[2] + 3), -3, 3);
				break;
				//state change
			case 2:
				robotMode = (robotMode + 1) % NUMBER_OF_MODES;
				break;
			case 5:
				robotMode = PRE_CALIBRATION_MODE;
				break;
			}
			break;
		case OFFLINE_MODE:
			sharedData->moveCtrl.tr[2] = 0;
			sharedData->moveCtrl.gaitID = 3;
			body.setGaitUpDown(body.gait.selectSequence(sharedData->moveCtrl.gaitID));
			switch (touchState)
			{
			case 1:
				hipMode = -1;
				break;
			case 4:
				hipMode = 1;
				break;
				//state change
			case 2:
				robotMode = (robotMode + 1) % NUMBER_OF_MODES;
				break;
			case 5:
				robotMode = PRE_CALIBRATION_MODE;
				break;
			}
			break;
		case DANCING_MODE:
			switch (touchState)
			{
			case 1:
				hipMode = -1;
				break;
			case 4:
				hipMode = 1;
				break;
				//state change
			case 2:
				robotMode = (robotMode + 1) % NUMBER_OF_MODES;
				break;
			case 5:
				robotMode = PRE_CALIBRATION_MODE;
				break;
			}
			break;
		case RANDOM_MODE:
			switch (touchState)
			{
				//state change
			case 2:
				robotMode = (robotMode + 1) % NUMBER_OF_MODES;
				break;
			case 5:
				robotMode = PRE_CALIBRATION_MODE;
				break;
			}
			break;
		case GRIP_MODE:
			switch (touchState)
			{
				//make state specific initialisation parameters:
				//sharedData->moveCtrl.gaitID = 5;
				//body.setGaitUpDown(body.gait.selectSequence(body.sharedData->moveCtrl.gaitID));
				//body.nWalkingLegs = 4;
				//grip.setLegWorkspace();
				//change state
			case 2:
				robotMode = (robotMode + 1) % NUMBER_OF_MODES;
				//sharedData->moveCtrl.gaitID = 1;
				//body.setGaitUpDown(body.gait.selectSequence(body.sharedData->moveCtrl.gaitID));
				//body.nWalkingLegs = 6;
				//grip.resetWorkspace();
				break;
			case 5:
				robotMode = PRE_CALIBRATION_MODE;
				//sharedData->moveCtrl.gaitID = 1;
				//body.setGaitUpDown(body.gait.selectSequence(body.sharedData->moveCtrl.gaitID));
				//body.nWalkingLegs = 6;
				//grip.resetWorkspace();
				break;
			}
		}
		Serial.print("mode: ");
		Serial.println(robotMode);
	}
}

void Robot::modeGo()
{
	switch (robotMode)
	{
	case PRE_CALIBRATION_MODE:
		//TODO hardware.setAllLEDs(50, RgbColor(255, 0, 0));
		break;
	case CALIBRATION_MODE:
	{
		//TODO hardware.strip.SetBrightness(255);
		int selectedLED;
		//mapping from selected leg to LED number

		for (int i = 0; i < 6; i++)
		{
			if (i == calibrationLegSelectedMapLED[calibrationLegSelected])
			{
				switch (calibrationServoLayerSelected % 3)
				{
				case 0:
					//TODO hardware.strip.SetPixelColor(i, hardware.red);
					break;
				case 1:
					//TODO hardware.strip.SetPixelColor(i, hardware.blue);
					break;
				case 2:
					//TODO hardware.strip.SetPixelColor(i, hardware.yellow);
					break;
				}
			}
			//else
				//TODO hardware.strip.SetPixelColor(i, 0);
		}
		delay(1);
		//TODO hardware.strip.Show();

		//nudge servos
		body.setLinMode(1);
		//TODO all calibration functions move to servo task
		/*if (nudgeServos)
		{
			int delayTime = 7;
			Serial.println(calibrationServoLayerSelected);
			float nudgeAmmount = 0.08; //radians
			for (int j = 0; j < 20; j++)
			{
				for (int i = 0; i < 18; i++)
				{
					if (i % 3 == calibrationServoLayerSelected) body.qAll[i] += nudgeAmmount / delayTime;
				}
				hardware.servoWrite(body.qAll);
				delay(delayTime);
			}
			for (int j = 0; j < 40; j++)
			{
				for (int i = 0; i < 18; i++)
				{
					if (i % 3 == calibrationServoLayerSelected) body.qAll[i] -= nudgeAmmount / delayTime;
				}
				hardware.servoWrite(body.qAll);
				delay(delayTime);
			}
			for (int j = 0; j < 20; j++)
			{
				for (int i = 0; i < 18; i++)
				{
					if (i % 3 == calibrationServoLayerSelected) body.qAll[i] += nudgeAmmount / delayTime;
				}
				hardware.servoWrite(body.qAll);
				delay(delayTime);
			}
			nudgeServos = 0;
		}
		else*/
			go();

		break;
	}
	case WALKING_MODE:
		//walk
		//body.setCommand(); // set command from the package
		go(); //run the algorithm		
					//TODO hardware.setAllLEDsRainbow(100);
		//hardware.batteryStatus();
		break;
	case OFFLINE_MODE:
		//offline move
		//TODO hardware.setAllLEDs(100, RgbColor(0, 255, 0));

		if (hipMode == -1)
		{
			goBackwards(5);
			goHome();
			hipMode = 0;
		}
		else if (hipMode == 1)
		{
			goForward(5);
			goHome();
			hipMode = 0;
		}
		else
		{
			resetPose();
			goHome();
		}
		break;
	case DANCING_MODE:
		//dance
	{
		float pitch;
		float hipRotation = 0.15, hipTranslation = 0;

		//TODO hardware.setAllLEDs(100, RgbColor(0, 0, 255));
		if (hipMode == -1)
		{
			int randomNumber = random(4, 8);
			for (int i = 0; i < randomNumber; i++)
			{
				pitch = random(-5, 5);
				pitch /= (float)10;
				Serial.println(pitch);
				Serial.print(" ran: ");
				Serial.println(randomNumber);
				//second
				setRotation(pitch, 0, 0);
				danceHip(hipRotation, hipTranslation, 0.5);
				danceHip(-hipRotation, hipTranslation, 0.5);
				danceHip(hipRotation, hipTranslation, 0.5);
				danceHip(-hipRotation, hipTranslation, 0.5);
			}
			resetPose();
			hipMode = 0;
		}
		else if (hipMode == 1)
		{
			goForward(5);
			hipMode = 0;
		}
		else
		{
			goHome();
		}
		break;
	}
	case RANDOM_MODE:
		//TODO hardware.setAllLEDs(100, RgbColor(255, 255, 0));
		resetPose();
		goHome();
		break;
	case GRIP_MODE:
		/*body.setCommand();

		sharedData->moveCtrl.gaitID = 5;
		body.setGaitUpDown(body.gait.selectSequence(body.sharedData->moveCtrl.gaitID));
		body.nWalkingLegs = 4;

		grip.setPose(body.gaitCurFi);

		float gripPoint[3] = { 0, 10, 9 };
		grip.setGripParam(gripPoint, 12, 0);

		grip.calcPoints();
		body.moveOneLegGlobal(0, grip.point1);
		body.moveOneLegGlobal(3, grip.point0);

		
		
		go();
		*/
		//TODO hardware.setAllLEDs(100, RgbColor(255, 0, 255));
		break;
	}
}