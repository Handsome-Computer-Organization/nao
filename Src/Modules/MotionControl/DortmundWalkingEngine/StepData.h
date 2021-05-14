#pragma once

#include "Point.h"
#include "WalkingInformations.h"
#include "Representations/MotionControl/WalkRequest.h"
/** Maximum number of possible foot positions in buffer */
#define PREVIEW_MAX	300  

class StepData
{
public:
  StepData()
  {
    direction=0;
    pitch=roll=0;
    footPos[0]=0;
    footPos[1]=0;
    onFloor[0]=true;
    onFloor[1]=true;
  }

  StepData operator = (const StepData &p)
  {
    footPos[0]=p.footPos[0];
    footPos[1]=p.footPos[1];
    onFloor[0]=p.onFloor[0];
    onFloor[1]=p.onFloor[1];
    direction=p.direction;
    pitch=p.pitch;
    roll=p.roll;
    return *this;
  }

  Point footPos[2];
  bool onFloor[2];
  float direction, pitch, roll;
};

class Footposition : public StepData
{
public:
  bool customStepRunning = false;
  bool inKick = false;
  unsigned int timestamp;
  WalkRequest::StepRequest customStep;

  DWE::WalkingPhase phase = DWE::unlimitedDoubleSupport;
  unsigned int singleSupportDurationInFrames;
  unsigned int doubleSupportDurationInFrames;
  unsigned int frameInPhase;
  float stepDurationInSec; // Additional info since dynamic step duration is possible
  int stepsSinceCustomStep = 0; // remember for reset of preview

  // kick hack, set specific angles in LimbCombinator
  int timeUntilKickHack = 0;
  int kickHackDuration = 0;
  Angle kickHackKneeAngle = 0_deg;

  Point speed;
  float lpxss = 0;
  float lastspdx = 0;
  Vector2f zmp, lastZMPRCS;

  void operator = (const StepData &p)
  {
    this->StepData::operator =(p);
  }

  Footposition& operator = (const Footposition& other)
  {
    this->StepData::operator =(other);
    customStepRunning = other.customStepRunning;
    inKick = other.inKick;
    timestamp = other.timestamp;
    customStep = other.customStep;
    phase = other.phase;
    singleSupportDurationInFrames = other.singleSupportDurationInFrames;
    doubleSupportDurationInFrames = other.doubleSupportDurationInFrames;
    frameInPhase = other.frameInPhase;
    stepDurationInSec = other.stepDurationInSec;
    stepsSinceCustomStep = other.stepsSinceCustomStep;
    timeUntilKickHack = other.timeUntilKickHack;
    kickHackDuration = other.kickHackDuration;
    kickHackKneeAngle = other.kickHackKneeAngle;
    speed = other.speed;
    lastspdx = other.lastspdx;
    lpxss = other.lpxss;
    zmp = other.zmp;
    lastZMPRCS = other.lastZMPRCS;
    return *this;
  }

  Footposition() {};
};


class ZMP : public Vector2f
{
public:
  ZMP()
  {
    x() = 0; y() = 0;
  }

  ZMP(const Point &p)
  {
    this->x() = p.x;
    this->y() = p.y;
  }

  const static int phaseToZMPFootMap[];

  unsigned int timestamp;

  float direction;

  ZMP(float x, float y)
  {
    this->x()=x;
    this->y()=y;
  }

  float operator[] (unsigned int i)
  {
    if (i == 0)
      return x();
    else 
      return y();
  }

  void operator =(Point &p)
  {
    this->x()=p.x;
    this->y()=p.y;
  }

  void operator =(float p)
  {
    x() = y() = 0;
  }

  ZMP operator +(const ZMP &zmp)
  {
    return ZMP(x() + zmp.x(), y()+ zmp.y());
  }

  ZMP operator +=(const ZMP &zmp)
  {
    x() += zmp.x();
    y() += zmp.y();
    return *this;
  }

  ZMP operator -=(const ZMP &zmp)
  {
    x() -= zmp.x();
    y() -= zmp.y();
    return *this;
  }

  ZMP operator *(const float f)
  {
    x() *= f;
    y() *= f;
    return *this;
  }

  operator Point()
  {
    return Point(x(), y(), 0);
  }


};

