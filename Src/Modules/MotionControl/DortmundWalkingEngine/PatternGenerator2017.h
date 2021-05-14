/*
Copyright 2011, Oliver Urbann
All rights reserved.

This file is part of MoToFlex.

MoToFlex is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MoToFlex is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MoToFlex.  If not, see <http://www.gnu.org/licenses/>.

Contact e-mail: oliver.urbann@tu-dortmund.de
*/

/** 
* @file Modules/MotionControl/DortmundWalkingEngine/PatternGenerator2017.h
* Generator for foot steps
* @author <a href="mailto:oliver.urbann@tu-dortmund.de> Oliver Urbann</a>
*/

#pragma once
#include "StepData.h"

#include "Point.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <list>
#include "WalkingInformations.h"
#include "Representations/MotionControl/SpeedInfo.h"
#include "Representations/MotionControl/WalkingEngineParams.h"
#include "Representations/Modeling/Path.h"
#include "Representations/MotionControl/WalkingInfo.h"
#include "Representations/Sensing/RobotModel.h"
#include "Representations/MotionControl/FootSteps.h"
#include "Representations/Configuration/RobotDimensions.h"
#include "Representations/Sensing/FallDownState.h"
#include "Representations/Infrastructure/SensorData/InertialSensorData.h"
#include "Representations/Infrastructure/RobotInfo.h"
#include "Representations/MotionControl/Footpositions.h"
#include "Representations/MotionControl/FLIPMParams.h"
#include "Representations/MotionControl/MotionSelection.h"
#include "Representations/MotionControl/ReferenceModificator.h"
#include "Representations/MotionControl/RefZMP.h"
#include "Representations/MotionControl/FallDownAngleReduction.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/BehaviorControl/RoleSymbols.h"
#include "Tools/RingBufferWithSum.h"
#include "Representations/Modeling/BallModel.h"
#include "Tools/Module/Module.h"
#include "Representations/Infrastructure/RobotHealth.h"
#include "Representations/MotionControl/WalkRequest.h"
#include "Tools/Settings.h"
#include "Representations/Modeling/IMUModel.h"
#include "Modules/MotionControl/DortmundWalkingEngine/CSConverter2019.h"

MODULE(PatternGenerator2017,
{,
  REQUIRES(WalkingEngineParams),
  REQUIRES(SpeedRequest),
  REQUIRES(Path),
  REQUIRES(RobotModel),
  REQUIRES(RobotDimensions),
  REQUIRES(FallDownState),
  REQUIRES(FLIPMParameter),
  REQUIRES(MotionSelection),
  REQUIRES(MotionRequest),
  REQUIRES(InertialSensorData),
  REQUIRES(IMUModel),
  REQUIRES(RobotInfo),
  REQUIRES(ZMPModel),
  USES(WalkingInfo),
  USES(ReferenceModificator),
  REQUIRES(FrameInfo),
  REQUIRES(BallModel),
  REQUIRES(BallModelAfterPreview),
  REQUIRES(RobotHealth),
  PROVIDES(FootSteps),
  PROVIDES(Footpositions),
  PROVIDES(RefZMP2018),
  PROVIDES(SpeedInfo),
  PROVIDES(FallDownAngleReduction),
  REQUIRES(RobotPoseAfterPreview),
  REQUIRES(RoleSymbols),

  LOADS_PARAMETERS(
  {,
    (Vector2f)(Vector2f(0.07f, 0.10f)) prependStepsTransMax,
    (Angle)(30_deg) prependStepsRotMax,
    (int)(3) maxPrependedSteps,
    (float)(0.f) stepRotationWeight,
    (float)(6.5f) maxCustomKickScore,
    (int)(3) stepsBetweenCustomSteps,
    ((WalkRequest) StepRequestVector) activeKicks,
    ((WalkRequest) StepRequestVector) activeGoalieKicks,
    (bool)(true) useResetPreview,
    (bool)(true) skipStepInReset,
    (bool)(false) useHalfFramesInRefZMP,
    (bool)(false) waitForZMP,
    (int) (90) slowDownTemperature,
    (Angle)(2_deg) maxGyroSpeedForTransition,
    (float)(0.239f) initialStandbyHeight,
    (bool)(false) useRealSpeedInCustomSteps,
    (float)(0.5f) penaltyShootoutSlowDownFactor,
    (bool)(false) useIMUModel,
    (float)(1.f) fallDownFactor,
    (float)(2.f) fallDownExponentialBasis,
    (bool)(false) useSafetySteps,
    (int)(1000) angleSumForSafetyStepFront,
    (int)(300) angleSumForSafetyStepBack,
    (float)(2.f) angleSumDecay,
    (float)(0.1f) sensorErrorMinForStepCorrection,
    (float)(0.35f) absoluteMinStepDuration,
    (float)(1.f) absoluteMaxStepDuration,
  }),
});

/**
* @class PatternGenerator2017
* This module generates the target foot positions on ground (not for the swing leg).
*/
class PatternGenerator2017 : public PatternGenerator2017Base
{
public:

  // state machine in general:
  // standby (other motion called) -> goingToReady -> ready (walk 0,0,0) -> walking -> stopping
  // ready -> goingToStandby -> standby
  /**
  * This function changes the newState (the desired walk state) and the
  * currentState, if possible.
  * State change will happen during updateWalkPhase() otherwise, 
  * since walk states change usually at end of a walk phase.
  * @return True if state change was successful.
  */
  bool updateWalkState();

  /**
  * This function changes the walk phase depending on the walk state.
  */
  void updateWalkPhase();

  /**
  * Returns the next feet positions. Must be called every frame if getCurrentState()!=standby!
  * @returns Position for both feet in double support phase, for support leg otherwise.
  */
  StepData getNextStep();

  /**
  * Updates the location of the CoM in robot coordinate system.
  * @param CoM The position of the center of mass in robot coordinate system.
  */
  void updateCoM(Point CoM);


  /** Constructor.
  */
  PatternGenerator2017();

  /** Returns the length of a current single support phase */
  unsigned int singleSupportDuration();
  /** Returns the length of a current double support phase */
  unsigned int doubleSupportDuration();

  /** Is the controller running? */
  bool isWalking();

  /** Destructor */
  ~PatternGenerator2017(void);

  /** Resets the generator */
  void reset();

protected:
  
  /** The current state of the pattern generator. */
  DWE::State currentState = DWE::standby;

  /** The request state */
  DWE::State newState = DWE::standby;

  /** The last time the walk stopped. */
  unsigned timeStampLastZeroRequest = 0;

  int stepsSinceLastCustomStep = 0;

  /** The current executed speed (after applyAcceleration()) */
  DWE::MovementInformation currentMovement;

  /** The requested speed */
  DWE::MovementInformation newMovement;

  /** Current walking phase */
  DWE::WalkingPhase currentWalkingPhase = DWE::unlimitedDoubleSupport;
  DWE::WalkingPhase walkingPhaseBeforePreview = DWE::unlimitedDoubleSupport;
  
  FootSteps localSteps; /**< Static foot steps on the ground. Just one step except at the beginning when preview is filled. */
  /** for creating the current foot position going to the CSConverter */
  Footpositions localFootPositions; /**< Both foot positions, including movement */
  FootSteps plannedFootSteps; /**< All static foot steps, kept for calculating localFootPositions. */
  Footposition resetFootPosition;
  Footposition nextPlannedFootPosition;
  std::vector<Point> currentFootStepTrajectory; /**< trajectory of the current foot step (w/o preview). recalculated once a new foot step phase completes. */
  /** For ZMP generation */
  RefZMP2018 localRefZMP2018;
  Point plotZMP = Point();
  float lastspdx = 0; // x speed in previous frame
  float lpxss = 0;										/**< Last position of ZMP along the x axis */
  ZMP zmp, lastZMPRCS;

  
  /** Default rotation of feet. Set to 0. */
  float baseRot = 0.f;

  /** Current step duration */
  float curStepDuration;

  Point distanceLeft;
  bool running = false; /* true, if the walk is active and speed is not zero, i.e. robot is not just standing. */

  /** Skip the current frame to reach the preview delta */
  bool skip;

  /** Last speed, to make sure limits are respected. */
  Pose2f lastSpeed = Pose2f();
    
  /** Current time (frames since running) */
  int currentTimeStamp;

  CSConverter2019::Parameters csConverterParams;

  /** 
  * Time counter in current state, from <length of phase> to 1.
  * If zero, state/phase change is happening (e.g. double support to single support).
  **/
  unsigned int stateCounter;

  /**
  * Calculates the length of a support phase.
  * @param ratio The current double support ratio. To calculate the
  *				legnth of a single support phase use 1-ds.
  * @param stepDur The current step duration
  * @return the duration of the phase in frames.
  */
  inline unsigned int getPhaseLength(float ratio, float stepDur);
  /** Initialize a new walk phase */
  void initWalkingPhase();
  /** The last added step. */
  Footposition lastStep;

  /** Current position of CoM. */
  Point CoM;
  Point lastCoM;

  /**
  * Current position of robot.
  * It is NOT the real position/direction. It is only used to place the feet, and only changed
  * during a phase change.
  */
  Pose2f robotPose2f; // the position next to the newest step, i.e. the theRobotPoseAfterPreview.
  Pose2f robotPose2fAfterCurrentStep;
  Pose2f speedBeforeStep; // speed before this step, used for acc if preview is reset
  float direction; // the direction of the upper body (changes continously during rotation in single support phases).

  bool resetPreviewPossible = false;
  int stepsInPreview = 0;
  int desiredPreviewLength; // desired length of the step buffer
  int currentPreviewLength; // the length of the step buffer
  Point appliedReactiveStep[2];
  int angleSumBodyTiltFront = 0; // if this exceeds \param angleSumForSafetyStepFront, do safety custom step
  int angleSumBodyTiltBack = 0; // if this exceeds \param angleSumForSafetyStepBack, do safety custom step
  unsigned timeWhenLastSafetyStepTriggered = 0;

  /** deltaDirection is added to the direction in every frame, one entry for each support phase (left/right single/double). */
  float deltaDirection[5];

  /** new footpositions forwarded to following modules. */
  Pose2f leftFootPose2f, rightFootPose2f;
  /** last foot positions of the respective foot. */
  Pose2f lastLeftFootPose2f, lastRightFootPose2f;

  /** Offset added from theReferenceModificator */
  Point sideStepSum[2];

  /** If the ready state was reached at least once, to save the actual walkingheight. */
  bool wasReady;

  /** Contains the walkHeight of the robot in the ready state. */
  float walkHeight;

  /** Add a new step to buffer. */
  void addStep();

  void addRefZMP(const Footposition &footPosition);
  struct RefZMPState
  {
    float lpxss;										/**< Last position of ZMP along the x axis */
    ZMP zmp, lastZMPRCS;
  };
  std::vector<RefZMPState> refZMPStates;

  /** Calculate some values for the next step (currently step duration). */
  float calcDynamicStepDuration(float speedX, float speedY, float speedR);

  /** Set the foot position for the next step, update the robotPose2f and set the direction & deltaDirection. */
  void setStepLength();

  /** Applies the speed requested in newMovement. */
  void applyAcceleration();

  void updateFallDownReduction();

  /** Calculate optimal preview depending on N and step phase length */
  int getOptimalPreviewLength();

  /** Decrease or increase preview on demand, calls addStep() as much as needed. */
  void updatePreview();

  bool isSpeed0() { return 
    (theSpeedRequest.translation.x() == 0) &&
    (theSpeedRequest.translation.y() == 0) &&
    (theSpeedRequest.rotation == 0); };

  /* SpeedInfo related stuff */
  RingBufferWithSum<Pose2f, PREVIEW_LENGTH> speedBuffer;
  bool decelByAcc;
  Point ballWalkingCS;
    
  /**
  * Coordinate system of footPos is always a standing robot with
  * root in the current swing foot.
  * Foot positions for double support phases are ignored.
  * Foot positions for support foot are ignored.
  * Foot trajectory interpolates positions via b-splines and adds result to swing foot position.
  * TODO: make config files smaller by removing useless information!
  */
  struct Step : public Streamable
  {
    Pose2f footPos[2];   // pose2f offset for left/right foot
    bool onFloor[2];     // foot is on floor
    std::vector<Vector3f> swingFootTraj;// control points for trajectory of foot in air, not used atm
    int duration;        // number of frames for this step
    std::vector<Point> spline;    // calculated on loading
    bool kick;
    bool mirrored = false;
    virtual void serialize(In* in, Out* out);
    void mirror();
  };
  
  struct StepsFile : Streamable
  {
    typedef std::vector<Step> StepsT;
    typedef StepsT::iterator StepsIterator;
    
    Vector2f ballOffset;
    Angle kickAngle;
    float kickDistance[2];
    float translationThresholdXFront = 0.015f;
    float translationThresholdXBack = 0.015f;
    float translationThresholdY = 0.015f;
    Angle rotationThreshold = 4_deg;

    int timeUntilKickHack = 120; // time (ms) until kickhack is triggered
    int kickHackDuration = 100; // duration (ms) of the kickhack
    Angle kickHackKneeAngle = -10_deg;

    StepsT steps;
    virtual void serialize(In* in, Out* out);
    void mirror();
    bool isApplicable(float distance);
  };
  
  typedef std::vector<StepsFile> StepFiles;
  typedef StepFiles::iterator StepFilesIterator;
  
  StepFiles stepFiles;

  void loadStepFiles();
  void saveStepFiles();
  void loadStepFile(std::string file, int idx);
  void saveStepFile(std::string file, int idx, bool robot = false);
  Vector2f prependCustomStep(std::list<Step>& steps, Vector2f distance, bool onFloorLeft);
  bool prependCustomSteps(std::list<Step>& steps, Pose2f pose, bool startFootLeft, bool endFootLeft, int maxSteps, const StepsFile &stepsFile);
  Pose2f calcKickPose(const StepsFile& steps, const Vector2f& kickTarget);
  bool transitionToCustomSteps();
  bool isStablePosition();
  WalkRequest::StepRequest selectCustomStepForKick(const Vector2f& kickTarget);
  float getScore(const std::list<PatternGenerator2017::Step>& steps);
  void transitionFromCustomSteps();
  void transitionToWalk(DWE::MovementInformation &moveInf);
  void setCustomWalkingPhase();
  WalkRequest::StepRequest currentCustomStep = WalkRequest::StepRequest::none;
  WalkRequest::StepRequest previousCustomStep = WalkRequest::StepRequest::none;

  /** Current step file executed. -1 if no one executed*/
  int currentStepFileIndex = -1;
  
  Vector2f fallDownAngleReductionFactor;

  /** The next and current custom step */
  StepsFile::StepsIterator executedStep;
  StepsFile::StepsIterator currentExecutedStep;

  /** Current executed steps, may be modified according to ball position / mirror */
  StepsFile currentSteps;

  /** Begin and end of original custom steps without prepended or appended steps */
  std::vector<Step>::iterator currentStepsBegin, currentStepsEnd;

  bool customStepKickInPreview = false;
  
  unsigned lastTimeExecuted = 0;
  void updateFootSteps(); /**< set foot steps */
  void updateFootPositions(); /**< interpolate foot steps into foot positions using speed and trajectory of steps */
  void createFootStepTrajectory(); /**< create trajectory for step once every foot pos has been added to plannedFootSteps */
  void updateRefZMP2018(); /**< create reference zmp using foot steps, removes foot steps */
  void update(FootSteps & footSteps); 
  void update(Footpositions & footPositions); 
  void update(RefZMP2018 & refZMP2018); 
  void update(SpeedInfo & speedInfo);
  void update(FallDownAngleReduction& fallDownAngleReduction);
};
