/**
 * \file JoystickDeviceParameters.h
 * \author Heiner Walter <heiner.walter@tu-dortmund.de>
 *
 * This file contains parameters for a specific joystick device.
 * Which device config file has to be loaded is previously loaded from general 
 * joystick config file.
 */
#pragma once

#include "Tools/Streams/Streamable.h"
#include "Tools/Streams/InStreams.h"
#include "Representations/MotionControl/SpecialActionRequest.h"
#include "Representations/MotionControl/WalkRequest.h"

// TODO: put implementation in cpp file?!
// TODO: special file for SpecialActionButtonPair, os is okay to have the class here?

class JoystickDeviceParameters : public Streamable
{
public:

  /**
   * \class SpecialActionButtonPair
   * 
   * This class is a pair of a special action and a button id. This class is 
   * used to define special actions which are triggered by joystick button events.
   */
  class SpecialActionButtonPair : public Streamable
  {
  public:
    /** Default constructor. */
    SpecialActionButtonPair() :
      specialAction(SpecialActionRequest()),
      button(0),
      duration(0) {}

    //TODO: don't pass by value ?!
    /** Constructor sets member variables to given values. */
    SpecialActionButtonPair(SpecialActionRequest specialAction_, int button_, unsigned duration_) :
      specialAction(specialAction_),
      button(button_),
      duration(duration_) {}

    /** Destructor. */
    ~SpecialActionButtonPair() {}

    /// Define a special actions which is triggered by the given \c button id.
    SpecialActionRequest specialAction;

    /// Define a button id which triggers the special actions.
    int button;

    /// Define minimum duration of the special action.
    unsigned duration;

    virtual void serialize(In* in, Out* out)
    {
      STREAM_REGISTER_BEGIN;

      STREAM(specialAction);
      STREAM(button);
      STREAM(duration);

      STREAM_REGISTER_FINISH;
    }
  };

  /**
   * \class StepRequestButtonPair
   *
   * Class for storing information about the CustomStep execution via the left control keys
   */
  class StepRequestButtonPair : public Streamable
  {
  public:
    /** Default constructor. */
    StepRequestButtonPair() :
      stepRequest(WalkRequest::StepRequest::none),
      stateValueX(0),
      stateValueY(0),
      duration(0),
      mirror(false){}

    //TODO: don't pass by value ?!
    /** Constructor sets member variables to given values. */
    StepRequestButtonPair(WalkRequest::StepRequest stepRequest_, int stateValueX_, int stateValueY_, unsigned duration_, bool mirror_) :
      stepRequest(stepRequest_),
      stateValueX(stateValueX_),
      stateValueY(stateValueY_),
      duration(duration_),
      mirror(mirror_) {}

    /** Destructor. */
    ~StepRequestButtonPair() {}

    /// Define a CustomStep
    WalkRequest::StepRequest stepRequest;

    /// TODO
    int stateValueX;

    /// TODO
    int stateValueY;

    /// TODO
    unsigned duration;

    /// TODO
    bool mirror;

    virtual void serialize(In* in, Out* out)
    {
      STREAM_REGISTER_BEGIN;

      STREAM(stepRequest, WalkRequest);
      STREAM(stateValueX);
      STREAM(stateValueY);
      STREAM(duration);
      STREAM(mirror);

      STREAM_REGISTER_FINISH;
    }
  };

  void loadConfigFile(const std::string& fileName)
  {
    // Attempt to load config file with given name.
    InMapFile stream(fileName);
    if (!stream.exists())
    {
      // Fall back to default device file if file with fileName does not exist.
      stream = InMapFile("joystick_Default.cfg");
    }
    // Fill *this with contents of config file.
    ASSERT(stream.exists());
    stream >> *this;
  }

  /// Checks whether the rotation axis is split into left and right axes.
  bool walkRotAxisSplit() const
  {
    return walkRotLeftAxis >= 0 && (unsigned int)walkRotLeftAxis <= axesCount &&
           walkRotRightAxis >= 0 && (unsigned int)walkRotRightAxis <= axesCount;
  }

  /// Absolute axis values below this threshold does not cause a motion.
  float minAxisValue;

  /// The number of avaiable buttons on the joystick.
  unsigned int buttonsCount;
  /// The number of avaiable axes on the joystick.
  unsigned int axesCount;

  /// Define axis which controls the walking speed in x-direction.
  /// Negative values deactivate x-direction walking control.
  int walkXAxis;

  /// Define a second axis which controls the walking speed in x-direction.
  /// The values of \c walkXAxis and \c additionalWalkXAxis are added, 
  /// but cut off at max speed.
  int additionalWalkXAxis;

  /// Define axis which controls the walking speed in y-direction.
  /// Negative values deactivate y-direction walking control.
  int walkYAxis;

  /// Define axis which controls the rotational walking speed.
  /// Negative values deactivate rotational walking control.
  int walkRotAxis;

  /// Define two axes which controls together the rotational walking speed.
  /// If walkRotAxis is valid, this axis is ignored.
  int walkRotLeftAxis;

  /// Define two axes which controls together the rotational walking speed.
  /// If walkRotAxis is valid, this axis is ignored.
  int walkRotRightAxis;

  /// Define axis which controls the head pan.
  /// Negative values deactivate head pan control.
  int headPanAxis;
  /// Define axis which controls the head tilt.
  /// Negative values deactivate head tilt control.
  int headTiltAxis;

  /// Define button which triggers the robot to stand up (same as chest button).
  int startButton;

  /// Define button which triggers the robot to stand up (same as chest button).
  int backButton;

  /// Define button which triggers a kick with the left foot.
  int leftKickButton;
  /// Define button which triggers a kick with the right foot.
  int rightKickButton;

  /// Define a list of special actions and button ids. The button in each of the 
  /// \c SpecialActionButtonPair elements triggers the related special action.
  std::vector<SpecialActionButtonPair> specialActions;

  /// Define the axis which controls the execution of CustomSteps
  int customStepXAxis;

  int customStepYAxis;

  std::vector<StepRequestButtonPair> activeKicks;

  virtual void serialize(In* in, Out* out)
  {
    STREAM_REGISTER_BEGIN;

    STREAM(minAxisValue);

    STREAM(buttonsCount);
    STREAM(axesCount);

    STREAM(walkXAxis);
    STREAM(additionalWalkXAxis);
    STREAM(walkYAxis);
    STREAM(walkRotAxis);
    STREAM(walkRotLeftAxis);
    STREAM(walkRotRightAxis);

    STREAM(headPanAxis);
    STREAM(headTiltAxis);

    STREAM(startButton);
    STREAM(backButton);

    STREAM(leftKickButton);
    STREAM(rightKickButton);

    STREAM(specialActions);

    STREAM(customStepXAxis);
    STREAM(customStepYAxis);

    STREAM(activeKicks);

    STREAM_REGISTER_FINISH;
  }
};