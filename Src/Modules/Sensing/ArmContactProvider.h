#ifndef __ArmContactProvider_
#define __ArmContactProvider_

#include "Tools/Module/Module.h"
#include "Representations/Sensing/ArmContact.h"
#include "Representations/Sensing/FallDownState.h"
#include "Representations/BehaviorControl/BallSymbols.h"
#include "Representations/BehaviorControl/RoleSymbols.h"
#include "Representations/Configuration/FieldDimensions.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameInfo.h"
#include "Representations/Infrastructure/SensorData/InertialSensorData.h"
#include "Representations/Infrastructure/SensorData/JointSensorData.h"
#include "Representations/Infrastructure/JointRequest.h"
#include "Representations/MotionControl/MotionSelection.h"
#include "Representations/MotionControl/ArmMovement.h"
#include "Representations/Modeling/BallModel.h"
#include "Representations/Modeling/RobotMap.h"
#include "Representations/Modeling/RobotPose.h"
#include "Tools/RingBufferWithSum.h"

MODULE(ArmContactProvider,
{ ,
  USES(ArmMovement),
  USES(JointRequest),
  REQUIRES(BallModelAfterPreview),
  REQUIRES(BallSymbols),
  REQUIRES(FallDownState),
  REQUIRES(FieldDimensions),
  REQUIRES(FrameInfo),
  REQUIRES(GameInfo),
  REQUIRES(InertialSensorData),
  REQUIRES(JointSensorData),
  REQUIRES(MotionSelection),
  REQUIRES(RobotMap),
  REQUIRES(RobotPose),
  REQUIRES(RoleSymbols),
  PROVIDES(ArmContact),
  LOADS_PARAMETERS(
  {,
    (bool) enableAvoidArmContact,
    (bool) useRobotMap,
    (bool) useArmPitchDiff,
    (float) maxRobotDist,
    (Angle) maxSum,
    (Angle) minAngleDiff,
    (bool) bothArmsBack,
  }),
});

class ArmContactProvider : public ArmContactProviderBase
{
public:
  
	ArmContactProvider(void);
	~ArmContactProvider(void);

private:
	void update(ArmContact& armContact);

  void resetBuffers();

  ArmContact localArmContact;
  RingBuffer<JointRequest,4> requestSensorDelayBuffer;
  RingBufferWithSum<Angle,186> bufferLeft;   // 2 seconds in Motion frames
  RingBufferWithSum<Angle,186> bufferRight;  // 2 seconds in Motion frames
  Angle lastPitchLeft, lastPitchRight;
  bool falling;

  JointRequest lastRequest;
  unsigned timeWhenWalkStarted;
  MotionRequest::Motion lastMotionType;
};

#endif // __ArmContactProvider_
