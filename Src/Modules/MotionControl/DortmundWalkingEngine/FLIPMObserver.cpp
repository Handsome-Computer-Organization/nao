#include "FLIPMObserver.h"
//#include "CSTransform.h"

//#define LOGGING
#include "Tools/Debugging/CSVLogger.h"

using namespace DWE;

void FLIPMObserver::update(ObservedFLIPMError& observedFLIPMError)
{
  //////////////////////////////////////////////////////
  DECLARE_PLOT("module:FLIPMObserver:Delayed_CoM1.x");	/////////////////
  DECLARE_PLOT("module:FLIPMObserver:Delayed_CoM1.y");	//// Targets ////
  DECLARE_PLOT("module:FLIPMObserver:Delayed_CoM2.x");	/////////////////
  DECLARE_PLOT("module:FLIPMObserver:Delayed_CoM2.y");
  DECLARE_PLOT("module:FLIPMObserver:Delayed_ZMP.x");
  DECLARE_PLOT("module:FLIPMObserver:Delayed_ZMP.y");
  DECLARE_PLOT("module:FLIPMObserver:Delayed_Acc.x");
  DECLARE_PLOT("module:FLIPMObserver:Delayed_Acc.y");
  //////////////////////////////////////////////////////
  DECLARE_PLOT("module:FLIPMObserver:realCoM1.x");		/////////////////////
  DECLARE_PLOT("module:FLIPMObserver:realCoM1.y");		//// Real Values ////
  DECLARE_PLOT("module:FLIPMObserver:realZMP.x");			/////////////////////
  DECLARE_PLOT("module:FLIPMObserver:realZMP.y");
  DECLARE_PLOT("module:FLIPMObserver:realAcc.x");
  DECLARE_PLOT("module:FLIPMObserver:realAcc.y");
  DECLARE_PLOT("module:FLIPMObserver:realCoM2.x");
  DECLARE_PLOT("module:FLIPMObserver:realCoM2.y");
  //////////////////////////////////////////////////////
  DECLARE_PLOT("module:FLIPMObserver:ACC1diff.x");		   /////////////////////
  DECLARE_PLOT("module:FLIPMObserver:ACC1diff.y");		   //// Differences ////
  DECLARE_PLOT("module:FLIPMObserver:CoM1diff.x");		   /////////////////////
  DECLARE_PLOT("module:FLIPMObserver:CoM1diff.y");
  DECLARE_PLOT("module:FLIPMObserver:ZMP1diff.x");
  DECLARE_PLOT("module:FLIPMObserver:ZMP1diff.y");
  DECLARE_PLOT("module:FLIPMObserver:CoM2diff.x");
  DECLARE_PLOT("module:FLIPMObserver:CoM2diff.y");
  //////////////////////////////////////////////////////
  DECLARE_PLOT("module:FLIPMObserver:CoM1diffRatio.x");	  ///////////////
  DECLARE_PLOT("module:FLIPMObserver:CoM1diffRatio.y");	  //// Ratio ////
  DECLARE_PLOT("module:FLIPMObserver:ACC1diffRatio.x");	  ///////////////
  DECLARE_PLOT("module:FLIPMObserver:ACC1diffRatio.y");
  DECLARE_PLOT("module:FLIPMObserver:CoM2diffRatio.x");
  DECLARE_PLOT("module:FLIPMObserver:CoM2diffRatio.y");
  //////////////////////////////////////////////////////

  static int delayCounter = 0;

  /* retrieve robotPosition */
  Pose2f robotPosition(theWalkingInfo.robotPosition.rotation, Vector2f(theWalkingInfo.robotPosition.translation.x(), theWalkingInfo.robotPosition.translation.y()));

  /////////////////
  //// Targets ////
  /////////////////

  /* retrieve target values (WCS) */
  Vector2f targetAccWCS(theTargetCoM.state_x[2], theTargetCoM.state_y[2]);
  Vector2f targetCoM1WCS(theTargetCoM.state_x[0], theTargetCoM.state_y[0]);
  Vector2f targetCoM2WCS(theTargetCoM.state_x[3], theTargetCoM.state_y[3]);

  /* rotate target values from WCS in RCS */
  if (theFLIPMObserverGains.useRCS) {
    targetAccWCS.rotate(-robotPosition.rotation);
    targetCoM1WCS.rotate(-robotPosition.rotation);
    targetCoM2WCS.rotate(-robotPosition.rotation);
  }

  /* add the target values on the delay buffer */
  accDelayBuffer.push_front(targetAccWCS);
  coM1DelayBuffer.push_front(targetCoM1WCS);
  coM2DelayBuffer.push_front(targetCoM2WCS);

  //////////////
  //// CoM1 ////
  //////////////
  Vector2f realCoM1;
  /* CoM1 Provider (WCS) */
  if (theFLIPMObserverGains.CoM1Provider == FLIPMObserverGains::MRE_CoM) {
    realCoM1.x() = theFLIPMObservedState.actualCoM_MRE.x;
    realCoM1.y() = theFLIPMObservedState.actualCoM_MRE.y;
  }
  else if (theFLIPMObserverGains.CoM1Provider == FLIPMObserverGains::targetCoM) {
    realCoM1.x() = coM1DelayBuffer[theWalkingEngineParams.sensorControl.halSensorDelay].x();
    realCoM1.y() = coM1DelayBuffer[theWalkingEngineParams.sensorControl.halSensorDelay].y();
  }
  else if (theFLIPMObserverGains.CoM1Provider == FLIPMObserverGains::IMU_CoM) {
    realCoM1.x() = theFLIPMObservedState.actualCoM_IMU.x;
    realCoM1.y() = theFLIPMObservedState.actualCoM_IMU.y;
  }

  /* CoM1 Provider (WCS->RCS) */
  realCoM1.rotate(-robotPosition.rotation);

  /* add offsets to CoM1 which are in RCS */
  realCoM1.x() += theFLIPMObserverGains.CoM1OffsetX;
  realCoM1.y() += theFLIPMObserverGains.CoM1OffsetY;

  /* CoM1 Provider (RCS->WCS)*/
  if (!theFLIPMObserverGains.useRCS) {
    realCoM1.rotate(robotPosition.rotation);
  }

  realCoM1DelayBuffer.push_front(realCoM1);

  //////////////
  //// CoM2 ////
  //////////////
  Vector2f realCoM2;
  /* CoM2 Provider (WCS) */
  if (theFLIPMObserverGains.CoM2Provider == FLIPMObserverGains::MRE_CoM) {
    realCoM2.x() = theFLIPMObservedState.actualCoM_MRE.x;
    realCoM2.y() = theFLIPMObservedState.actualCoM_MRE.y;
  }
  else if (theFLIPMObserverGains.CoM2Provider == FLIPMObserverGains::targetCoM) {
    realCoM2.x() = coM2DelayBuffer[theWalkingEngineParams.sensorControl.halSensorDelay].x();
    realCoM2.y() = coM2DelayBuffer[theWalkingEngineParams.sensorControl.halSensorDelay].y();
  }
  else if (theFLIPMObserverGains.CoM2Provider == FLIPMObserverGains::IMU_CoM) {
    realCoM2.x() = theFLIPMObservedState.actualCoM_IMU.x;
    realCoM2.y() = theFLIPMObservedState.actualCoM_IMU.y;
  }

  /* CoM2 Provider (WCS->RCS) */
  realCoM2.rotate(-robotPosition.rotation);

  /* add offsets to CoM2 which are in RCS */
  realCoM2.x() += theFLIPMObserverGains.CoM2OffsetX;
  realCoM2.y() += theFLIPMObserverGains.CoM2OffsetY;

  /* CoM2 Provider (RCS->WCS)*/
  if (!theFLIPMObserverGains.useRCS) {
    realCoM2.rotate(robotPosition.rotation);
  }

  realCoM2DelayBuffer.push_front(realCoM2);

  //////////////
  //// ACC1 ////
  //////////////
  Vector2f realAcc;
  /* ACC Provider (WCS)*/
  if (theFLIPMObserverGains.ACC1Provider == FLIPMObserverGains::ZMP_CoM1_Acc) {
    realAcc.x() = (-1 * theZMPModel.ZMP_WCS.x() + realCoM1.x()) * (theFLIPMParameter.paramsX.g / theFLIPMParameter.paramsX.z_h);
    realAcc.y() = (-1 * theZMPModel.ZMP_WCS.y() + realCoM1.y()) * (theFLIPMParameter.paramsY.g / theFLIPMParameter.paramsY.z_h);
  }
  else if (theFLIPMObserverGains.ACC1Provider == FLIPMObserverGains::ZMP_Acc) {
    float filter = theFLIPMObserverGains.accFilter;
    filteredAccX = filteredAccX* (1 - filter) + (-1 * theZMPModel.ZMP_WCS.x() + realCoM1DelayBuffer[theFLIPMObserverGains.CoM1Delay].x()) * (theFLIPMParameter.paramsX.g / theFLIPMParameter.paramsX.z_h) * filter;
    filteredAccY = filteredAccY* (1 - filter) + (-1 * theZMPModel.ZMP_WCS.y() + realCoM1DelayBuffer[theFLIPMObserverGains.CoM1Delay].y()) * (theFLIPMParameter.paramsY.g / theFLIPMParameter.paramsY.z_h) * filter;
    realAcc.x() = filteredAccX;
    realAcc.y() = filteredAccY;
  }
  else if (theFLIPMObserverGains.ACC1Provider == FLIPMObserverGains::ZMP_Target_Acc) {
    float filter = theFLIPMObserverGains.accFilter;
    filteredAccX = filteredAccX* (1 - filter) + (-1 * theZMPModel.ZMP_WCS.x() + targetCoM1WCS.x()) * (theFLIPMParameter.paramsX.g / theFLIPMParameter.paramsX.z_h) * filter;
    filteredAccY = filteredAccY* (1 - filter) + (-1 * theZMPModel.ZMP_WCS.y() + targetCoM1WCS.y()) * (theFLIPMParameter.paramsY.g / theFLIPMParameter.paramsY.z_h) * filter;
    realAcc.x() = filteredAccX;
    realAcc.y() = filteredAccY;
  }
  else if (theFLIPMObserverGains.ACC1Provider == FLIPMObserverGains::targetAcc) {
    realAcc.x() = accDelayBuffer[theWalkingEngineParams.sensorControl.sensorDelay].x();
    realAcc.y() = accDelayBuffer[theWalkingEngineParams.sensorControl.sensorDelay].y();
  }
  else if (theFLIPMObserverGains.ACC1Provider == FLIPMObserverGains::IMU_Acc) {
    float filter = theFLIPMObserverGains.accFilter;
    filteredAccX = filteredAccX* (1 - filter) + (theFLIPMObservedState.actualAcc.x()) * filter;
    filteredAccY = filteredAccY* (1 - filter) + (theFLIPMObservedState.actualAcc.y()) * filter;
    realAcc.x() = filteredAccX;
    realAcc.y() = filteredAccY;
  }

  /* ACC Provider (WCS->RCS) */
  if (theFLIPMObserverGains.useRCS) {
    realAcc.rotate(-robotPosition.rotation);
  }

  /////////////////////
  //// Differences ////
  /////////////////////

  /* calculate the CoM differences and the Acceleration difference */
  Vector2f ACC_1_diff, CoM_1_diff, CoM_2_diff;

  if (theTargetCoM.isRunning
    && static_cast<unsigned int>(accDelayBuffer.size()) > theWalkingEngineParams.sensorControl.sensorDelay
    && static_cast<unsigned int>(coM1DelayBuffer.size()) > theWalkingEngineParams.sensorControl.halSensorDelay
    && static_cast<unsigned int>(realCoM2DelayBuffer.size()) > theFLIPMObserverGains.CoM2Delay
    && static_cast<unsigned int>(realCoM1DelayBuffer.size()) > theFLIPMObserverGains.CoM1Delay)
  {
    /* ZMP calculation
    Vector2f ZMPDelay, realZMP;
    ZMPDelay.x() = coM1DelayBuffer[theWalkingEngineParams.halSensorDelay].x() - (accDelayBuffer[theWalkingEngineParams.sensorDelay].x()*(paramsFLIPMObsv.z_h / paramsFLIPMObsv.g));
    ZMPDelay.y() = coM1DelayBuffer[theWalkingEngineParams.halSensorDelay].y() - (accDelayBuffer[theWalkingEngineParams.sensorDelay].y()*(paramsFLIPMObsv.z_h / paramsFLIPMObsv.g));
    PLOT("module:FLIPMObserver:Delayed_ZMP.x", ZMPDelay.x());
    PLOT("module:FLIPMObserver:Delayed_ZMP.y", ZMPDelay.y());

    realZMP.x() = theZMPModel.ZMP_WCS.x();
    realZMP.y() = theZMPModel.ZMP_WCS.y();
    PLOT("module:FLIPMObserver:realZMP.x", realZMP.x());
    PLOT("module:FLIPMObserver:realZMP.y", realZMP.y());
    ZMP_1_diff = realZMP - ZMPDelay;
    */

    ACC_1_diff = realAcc - accDelayBuffer[theWalkingEngineParams.sensorControl.sensorDelay];
    CoM_1_diff = realCoM1DelayBuffer[theFLIPMObserverGains.CoM1Delay] - coM1DelayBuffer[theWalkingEngineParams.sensorControl.halSensorDelay];
    CoM_2_diff = realCoM1DelayBuffer[theFLIPMObserverGains.CoM1Delay] - realCoM2DelayBuffer[theFLIPMObserverGains.CoM2Delay] + Vector2f(theFLIPMObserverGains.CoM2DiffXOffset, theFLIPMObserverGains.CoM2DiffYOffset);

    ACC_1_diff.x() = ACC_1_diff.x() > 0.f ? std::min(ACC_1_diff.x(), theFLIPMObserverGains.maxDiffACCClip) : std::max(ACC_1_diff.x(), -theFLIPMObserverGains.maxDiffACCClip);
    ACC_1_diff.y() = ACC_1_diff.y() > 0.f ? std::min(ACC_1_diff.y(), theFLIPMObserverGains.maxDiffACCClip) : std::max(ACC_1_diff.y(), -theFLIPMObserverGains.maxDiffACCClip);

    CoM_1_diff.x() = CoM_1_diff.x() > 0.f ? std::min(CoM_1_diff.x(), theFLIPMObserverGains.maxDiffCoMClip) : std::max(CoM_1_diff.x(), -theFLIPMObserverGains.maxDiffCoMClip);
    CoM_1_diff.y() = CoM_1_diff.y() > 0.f ? std::min(CoM_1_diff.y(), theFLIPMObserverGains.maxDiffCoMClip) : std::max(CoM_1_diff.y(), -theFLIPMObserverGains.maxDiffCoMClip);

    CoM_2_diff.x() = CoM_2_diff.x() > 0.f ? std::min(CoM_2_diff.x(), theFLIPMObserverGains.maxDiffCoMClip) : std::max(CoM_2_diff.x(), -theFLIPMObserverGains.maxDiffCoMClip);
    CoM_2_diff.y() = CoM_2_diff.y() > 0.f ? std::min(CoM_2_diff.y(), theFLIPMObserverGains.maxDiffCoMClip) : std::max(CoM_2_diff.y(), -theFLIPMObserverGains.maxDiffCoMClip);
  }
  else {
    ACC_1_diff.setZero();
    CoM_1_diff.setZero();
    CoM_2_diff.setZero();
    filteredAccX = 0.f;
    filteredAccY = 0.f;
  }

  ///////////////
  // Targets ////
  ///////////////
  PLOT("module:FLIPMObserver:Delayed_CoM1.x", coM1DelayBuffer[theWalkingEngineParams.sensorControl.halSensorDelay].x());
  PLOT("module:FLIPMObserver:Delayed_CoM1.y", coM1DelayBuffer[theWalkingEngineParams.sensorControl.halSensorDelay].y());
  PLOT("module:FLIPMObserver:Delayed_CoM2.x", coM2DelayBuffer[theWalkingEngineParams.sensorControl.halSensorDelay].x());
  PLOT("module:FLIPMObserver:Delayed_CoM2.y", coM2DelayBuffer[theWalkingEngineParams.sensorControl.halSensorDelay].y());
  PLOT("module:FLIPMObserver:Delayed_Acc.x", accDelayBuffer[theWalkingEngineParams.sensorControl.sensorDelay].x());
  PLOT("module:FLIPMObserver:Delayed_Acc.y", accDelayBuffer[theWalkingEngineParams.sensorControl.sensorDelay].y());
  ///////////////////
  // Real Values ////
  ///////////////////
  PLOT("module:FLIPMObserver:realCoM1.x", realCoM1DelayBuffer[theFLIPMObserverGains.CoM1Delay].x());
  PLOT("module:FLIPMObserver:realCoM1.y", realCoM1DelayBuffer[theFLIPMObserverGains.CoM1Delay].y());
  PLOT("module:FLIPMObserver:realAcc.x", realAcc.x());
  PLOT("module:FLIPMObserver:realAcc.y", realAcc.y());
  PLOT("module:FLIPMObserver:realCoM2.x", realCoM2DelayBuffer[theFLIPMObserverGains.CoM2Delay].x());
  PLOT("module:FLIPMObserver:realCoM2.y", realCoM2DelayBuffer[theFLIPMObserverGains.CoM2Delay].y());
  ///////////////////
  // Differences ////
  ///////////////////
  PLOT("module:FLIPMObserver:ACC1diff.x", ACC_1_diff.x());
  PLOT("module:FLIPMObserver:ACC1diff.y", ACC_1_diff.y());
  PLOT("module:FLIPMObserver:CoM1diff.x", CoM_1_diff.x());
  PLOT("module:FLIPMObserver:CoM1diff.y", CoM_1_diff.y());
  PLOT("module:FLIPMObserver:CoM2diff.x", CoM_2_diff.x());
  PLOT("module:FLIPMObserver:CoM2diff.y", CoM_2_diff.y());

  /* calculate if walking or not!*/
#if 1
  static bool sensorOn = false;

  if (theFootSteps.walkState == State::walking)
    sensorOn = true;
  else
    sensorOn = false;

#else
  static bool sensorOn = true;
#endif

  /* ZMP Smoothing to reduce jerk*/
  if (sensorOn && localSensorScale < 1) {
    if (counter > 25) {
      localSensorScale += 1.0f / theWalkingEngineParams.walkTransition.zmpSmoothPhase;
    }
    else {
      counter++;
    }
  }
  if (!sensorOn) {
    delayCounter++;
    counter = 0;
  }
  else {
    delayCounter = 0;
  }
  if (delayCounter > PREVIEW_LENGTH && localSensorScale > 0) {
    localSensorScale -= 1.0f / theWalkingEngineParams.walkTransition.zmpSmoothPhase;
  }

  if (theFLIPMObserverGains.useRCS) {
    CoM_1_diff.x() *= (theFLIPMObserverGains.sensorControlRatioObserverX[0] * localSensorScale);
    CoM_1_diff.y() *= (theFLIPMObserverGains.sensorControlRatioObserverY[0] * localSensorScale);
    ACC_1_diff.x() *= (theFLIPMObserverGains.sensorControlRatioObserverX[1] * localSensorScale);
    ACC_1_diff.y() *= (theFLIPMObserverGains.sensorControlRatioObserverY[1] * localSensorScale);
    CoM_2_diff.x() *= (theFLIPMObserverGains.sensorControlRatioObserverX[2] * localSensorScale);
    CoM_2_diff.y() *= (theFLIPMObserverGains.sensorControlRatioObserverY[2] * localSensorScale);
  }
  else {
    /* Transform WCS into RCS and back, to apply some scaling*/
    CoM_1_diff.rotate(-robotPosition.rotation);
    CoM_1_diff.x() *= (theFLIPMObserverGains.sensorControlRatioObserverX[0] * localSensorScale) *theFLIPMObserverGains.sensorXFactor;
    CoM_1_diff.y() *= (theFLIPMObserverGains.sensorControlRatioObserverY[0] * localSensorScale) *theFLIPMObserverGains.sensorYFactor;
    CoM_1_diff.rotate(robotPosition.rotation);

    ACC_1_diff.rotate(-robotPosition.rotation);
    ACC_1_diff.x() *= (theFLIPMObserverGains.sensorControlRatioObserverX[1] * localSensorScale) *theFLIPMObserverGains.sensorXFactor;
    ACC_1_diff.y() *= (theFLIPMObserverGains.sensorControlRatioObserverY[1] * localSensorScale) *theFLIPMObserverGains.sensorYFactor;
    ACC_1_diff.rotate(robotPosition.rotation);

    CoM_2_diff.rotate(-robotPosition.rotation);
    CoM_2_diff.x() *= (theFLIPMObserverGains.sensorControlRatioObserverX[2] * localSensorScale) *theFLIPMObserverGains.sensorXFactor;
    CoM_2_diff.y() *= (theFLIPMObserverGains.sensorControlRatioObserverY[2] * localSensorScale) *theFLIPMObserverGains.sensorYFactor;
    CoM_2_diff.rotate(robotPosition.rotation);
  }

  if (theFLIPMObserverGains.activateSensorX == true) {
    observedFLIPMError.ObservedError.x() = theFLIPMObserverParameter.observerParamsX.L * Vector3d(CoM_1_diff.x(), ACC_1_diff.x(), CoM_2_diff.x());
  }
  else {
    observedFLIPMError.ObservedError.x() = theFLIPMObserverParameter.observerParamsX.L * Vector3d(0, 0, 0);
  }

  if (theFLIPMObserverGains.activateSensorY == true) {
    observedFLIPMError.ObservedError.y() = theFLIPMObserverParameter.observerParamsY.L * Vector3d(CoM_1_diff.y(), ACC_1_diff.y(), CoM_2_diff.y());
  }
  else {
    observedFLIPMError.ObservedError.y() = theFLIPMObserverParameter.observerParamsY.L * Vector3d(0, 0, 0);
  }

  PLOT("module:FLIPMObserver:ACC1diffRatio.x", ACC_1_diff.x());
  PLOT("module:FLIPMObserver:ACC1diffRatio.y", ACC_1_diff.y());
  PLOT("module:FLIPMObserver:CoM1diffRatio.x", CoM_1_diff.x());
  PLOT("module:FLIPMObserver:CoM1diffRatio.y", CoM_1_diff.y());
  PLOT("module:FLIPMObserver:CoM2diffRatio.x", CoM_2_diff.x());
  PLOT("module:FLIPMObserver:CoM2diffRatio.y", CoM_2_diff.y());
  
  LOG("FLIPMObserver", "ACC1diffRatio.x", ACC_1_diff.x());
  LOG("FLIPMObserver", "ACC1diffRatio.y", ACC_1_diff.y());
  LOG("FLIPMObserver", "CoM1diffRatio.x", CoM_1_diff.x());
  LOG("FLIPMObserver", "CoM1diffRatio.y", CoM_1_diff.y());
  LOG("FLIPMObserver", "CoM2diffRatio.x", CoM_2_diff.x());
  LOG("FLIPMObserver", "CoM2diffRatio.y", CoM_2_diff.y());
  LOG("FLIPMObserver", "angleX", theInertialSensorData.angle.x());
  LOG("FLIPMObserver", "angleY", theInertialSensorData.angle.x());
  if (theTargetCoM.isRunning)
  {
    LOG("FLIPMObserver", "real Acc x", realAcc.x());
    LOG("FLIPMObserver", "real Acc y", realAcc.y());
  }

}


MAKE_MODULE(FLIPMObserver, dortmundWalkingEngine)