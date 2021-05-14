
#include "SelfLocator2017.h"

#include "Tools/Math/GaussianDistribution2D.h"

#define LOGGING
#include "Tools/Debugging/CSVLogger.h"

/*---------------------------- class SelfLocator2017 PUBLIC methods ------------------------------*/

namespace
{
  /** Helper functions **/
  Pose2f getSymmetricPoseOnField(const Pose2f& pose)
  {
    return pose.dotMirror();
  }


  bool isPoseOnSameFieldSide(const Pose2f& p1, const Pose2f& p2)
  {
    return  (((p1.translation.x() <= 0) && (p2.translation.x() <= 0))
      ||
      ((p1.translation.x() >= 0) && (p2.translation.x() >= 0))
      );
  }

  bool isPoseOnOwnFieldSide(const Pose2f & p)
  {
    static const Pose2f own(-1000, 0);
    return isPoseOnSameFieldSide(p, own);
  }

  bool arePosesCloseToEachOther(const Pose2f & p1, const Pose2f & p2, const SelfLocator2017Parameters & parameters)
  {
    return ((p1.translation - p2.translation).norm() < parameters.sensorReset.maxDistanceForLocalResetting
      && std::abs(Angle::normalize(p1.rotation - p2.rotation)) < parameters.sensorReset.maxAngleForLocalResetting);
  }

  bool getLeftAndRightGoalPostFromGoalPercept(const CLIPGoalPercept & theGoalPercept, Vector2f & leftPost, Vector2f & rightPost, float& validity)
  {
    validity = 0.f;
    if (theGoalPercept.goalPosts[0].goalPostSide == CLIPGoalPercept::GoalPost::leftPost && theGoalPercept.goalPosts[1].goalPostSide == CLIPGoalPercept::GoalPost::rightPost)
    {
      leftPost = theGoalPercept.goalPosts[0].locationOnField.cast<float>();
      rightPost = theGoalPercept.goalPosts[1].locationOnField.cast<float>();
    }
    else if (theGoalPercept.goalPosts[1].goalPostSide == CLIPGoalPercept::GoalPost::leftPost && theGoalPercept.goalPosts[0].goalPostSide == CLIPGoalPercept::GoalPost::rightPost)
    {
      leftPost = theGoalPercept.goalPosts[1].locationOnField.cast<float>();
      rightPost = theGoalPercept.goalPosts[0].locationOnField.cast<float>();
    }
    else
      return false;

    validity = (theGoalPercept.goalPosts[0].validity + theGoalPercept.goalPosts[1].validity) / 2;
    return true;
  }

  static std::string TAG = "SelfLocator";
}


SelfLocator2017::SelfLocator2017() :
  initialized(false),
  lastExecuteTimeStamp(0),
  foundGoodPosition(false),
  timeStampLastGoodPosition(0),
  localizationState(positionTracking),
  localizationStateAfterGettingUp(localizationState),
  penalizedTimeStamp(0),
  unpenalizedTimeStamp(0),
  lastPenalty(PENALTY_NONE),
  lastNonPlayingTimeStamp(0),
  lastPositionLostTimeStamp(0),
  timeStampFirstReadyState(0),
  distanceTraveledFromLastFallDown(0.f),
  symmetryPosUpdate(0),
  symmertryNegUpdate(0),
  lastBestSymmetryConfidence(0),
  lastNonSetTimestamp(0),
  gotPickedUp(false),
  lastBestHypothesisUniqueId(0)
{}

SelfLocator2017::~SelfLocator2017()
{
  PoseHypothesis2017::cleanup();
}

void SelfLocator2017::update(RobotPose & robotPose)
{
  executeCommonCode();
  robotPose = m_robotPose;
}

void SelfLocator2017::update(SideConfidence & confidence)
{
  executeCommonCode();
  confidence = m_sideConfidence;
}


void SelfLocator2017::update(RobotPoseHypothesis & robotPoseHypothesis)
{
  executeCommonCode();
  robotPoseHypothesis = m_robotPoseHypothesis;
}

void SelfLocator2017::update(RobotPoseHypotheses & robotPoseHypotheses)
{
  executeCommonCode();
  robotPoseHypotheses = m_robotPoseHypotheses;
}

void SelfLocator2017::update(RobotPoseHypothesesCompressed & robotPoseHypotheses)
{
  executeCommonCode();
  robotPoseHypotheses = RobotPoseHypothesesCompressed(m_robotPoseHypotheses);
}


void SelfLocator2017::predictHypotheses()
{
  Pose2f odometryDelta = theOdometryData - lastOdometryData;
  lastOdometryData = theOdometryData;

  distanceTraveledFromLastFallDown += odometryDelta.translation.norm();

  for (auto& hypothesis : poseHypotheses)
  {
    hypothesis->predict(odometryDelta, parameters);
  }
}


void SelfLocator2017::updateHypothesesPositionConfidence()
{
  bool updateSpherical, updateInfiniteLines, updateWeighted;
  for (auto& hypothesis : poseHypotheses)
  {
    updateSpherical = hypothesis->updatePositionConfidenceWithLocalFeaturePerceptionsSpherical(theLineMatchingResult, theCLIPCenterCirclePercept, theCLIPGoalPercept, thePenaltyCrossPercept, theFieldDimensions, theCameraMatrix, theCameraMatrixUpper, parameters);
    updateInfiniteLines = hypothesis->updatePositionConfidenceWithLocalFeaturePerceptionsInfiniteLines(theLineMatchingResult, theCLIPCenterCirclePercept, theFieldDimensions, parameters);
    updateWeighted = hypothesis->updatePositionConfidenceWithLocalFeaturePerceptionsWeighted(theCLIPFieldLinesPercept, theCLIPCenterCirclePercept, theCLIPGoalPercept, thePenaltyCrossPercept, theFieldDimensions, parameters);

    if (!(updateSpherical || updateInfiniteLines || updateWeighted))
    {
      // TODO: Negative update as nothing was seen?
    }
  }
}

void SelfLocator2017::updateHypothesesState()
{
  for (auto& hypothesis : poseHypotheses)
  {
    hypothesis->updateStateWithLocalFeaturePerceptionsSpherical(parameters);
    hypothesis->updateStateWithLocalFeaturePerceptionsInfiniteLines(parameters);
    hypothesis->updateStateRotationWithLocalFieldLines(theCLIPFieldLinesPercept, parameters);
  }
}

void SelfLocator2017::updateHypothesesSymmetryConfidence()
{
  for (auto& hypothesis : poseHypotheses)
  {
    // Update symmetry only when playing or ready
    if ((theGameInfo.state == STATE_PLAYING || theGameInfo.state == STATE_READY) && parameters.symmetryUpdate.updateWithRemoteModels)
    {
      hypothesis->updateSymmetryByComparingRemoteToLocalModels(theBallModel, theRemoteBallModel, theLocalRobotMap, theRemoteRobotMap, theTeammateData, theFrameInfo, parameters);
    }
    else
    {
      hypothesis->setSymmetryConfidence(1.f);
    }

    // no symmetrie loss for goalie. ever.
    if (theBehaviorData.role == BehaviorData::keeper)
    {
      hypothesis->setSymmetryConfidence(1.f);
    }

    // Check whether symmetry has been lost for this hypothesis
    if (hasSymmetryBeenLost(*hypothesis))
    {
      // Mirror hypothesis if symmetric poses are allowed. If not mirror it.
      if (parameters.localizationStateUpdate.symmetryHandling == SelfLocator2017Parameters::LocalizationStateUpdate::noSymmetricPoses)
      {
        hypothesis->mirror();
        hypothesis->setSymmetryConfidence(1 - hypothesis->getSymmetryConfidence());
      }
    }
  }

  // Check confident state for symmetry with two players only.
  std::underlying_type<SideConfidence::ConfidenceState>::type cs = m_sideConfidence.confidenceState;
  const PoseHypothesis2017* bestHyp = getBestHypothesis();
  float bestSymmetryConfidence = bestHyp ? bestHyp->getSymmetryConfidence() : 0.f;

  if (bestSymmetryConfidence > lastBestSymmetryConfidence)
  {
    if (bestSymmetryConfidence == 1.f)
    {
      cs = SideConfidence::CONFIDENT;
    }
    // The lower the more confident -> So increase if best hypo switched
    symmetryPosUpdate = ++symmetryPosUpdate % symmetryUpdatesBeforeAdjustingState;
    if (symmetryPosUpdate == 0)
      cs--;
  }
  else if (bestSymmetryConfidence < lastBestSymmetryConfidence)
  {
    symmertryNegUpdate = ++symmertryNegUpdate % symmetryUpdatesBeforeAdjustingState;
    if (symmertryNegUpdate == 0)
      cs++;
  }

  cs = std::min(std::underlying_type<SideConfidence::ConfidenceState>::type(SideConfidence::numOfConfidenceStates - 1),
    std::max(std::underlying_type<SideConfidence::ConfidenceState>::type(SideConfidence::CONFIDENT), cs));

  m_sideConfidence.confidenceState = static_cast<SideConfidence::ConfidenceState>(cs);

  lastBestSymmetryConfidence = bestSymmetryConfidence;
}


void SelfLocator2017::executeCommonCode()
{
  if (!initialized)
  {
    addHypothesesOnInitialKickoffPositions();
    addHypothesesOnManualPositioningPositions();
    // Initialize distance with in a way it will cause spawns on 1st fall down
    distanceTraveledFromLastFallDown = parameters.spawning.minDistanceBetweenFallDowns;
    initialized = true;
  }

  if (lastExecuteTimeStamp == theFrameInfo.time)
  {
    return;
  }
  lastExecuteTimeStamp = theFrameInfo.time;


  initDebugging();

  // Save best hypothesis data
  const PoseHypothesis2017* bestHypothesis = getBestHypothesis();
  Pose2f pose;
  float pc = 0.f, sc = 0.f;
  if (bestHypothesis)
  {
    bestHypothesis->getRobotPose(pose);
    pc = bestHypothesis->getPositionConfidence();
    sc = bestHypothesis->getSymmetryConfidence();
  }

  // Checking whether robot has been picked up (human error detection)
  checkBeingPickedUp();

  // Predict new position
  predictHypotheses();

  // Fill matrices for update
  for (auto& hyp : poseHypotheses)
    hyp->fillCorrectionMatrices(theLineMatchingResult, theCLIPCenterCirclePercept, theCLIPGoalPercept, thePenaltyCrossPercept, theFieldDimensions,
      theCameraMatrix, theCameraMatrixUpper, parameters);

  // Update state of hypotheses
  updateHypothesesState();

  // Evaluate status of localization (OK, Symmetrie lost, Lost)
  evaluateLocalizationState();

  // Add new hypotheses
  addNewHypotheses();

  // Update confidence of hypotheses
  updateHypothesesPositionConfidence();

  // Update symmetry confidence of each hypothesis
  updateHypothesesSymmetryConfidence();

  // Remove unnecessary hypotheses
  pruneHypotheses();

  doGlobalDebugging();

  // reset hypotheses if no more are left
  if (poseHypotheses.empty())
  {
    OUTPUT_ERROR("SelfLocator2017 pruned all hypotheses because of their state -> resetting!");

    // readd the best last with a small validity bonus, if it was inside carpet
    if (pc > 0.f)
    {
      Vector2f lastPosition = pose.translation;
      if (theFieldDimensions.clipToCarpet(lastPosition) < 100)
        poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
          pose,
          std::max(pc * .8f, parameters.spawning.positionConfidenceWhenPositionedManually + 0.1f),
          sc,
          theFrameInfo.time,
          parameters));
    }

    // re-add best one from before as well as some poses around the field
    // TODO: if it was an state update error and the position was good, maybe we only want new hypotheses from line matches?
    addHypothesesOnManualPositioningPositions();
    addHypothesesOnPenaltyPositions(0.2f);
    addNewHypothesesFromLineMatches();

  }

  // Fill local storage for representations
  generateOutputData();

}

void SelfLocator2017::checkBeingPickedUp()
{
  // Fill buffer
  if (theGameInfo.state == STATE_SET)
  {
    // In set state -> wait for robot to stand still
    if (!gotPickedUp && theFrameInfo.getTimeSince(lastNonSetTimestamp) > 1000)
    {
      accDataBuffer.push_front(theInertialData.acc);
    }
  }
  // Check that the robot is just entering the field
  else if (theFrameInfo.getTimeSince(unpenalizedTimeStamp) < 5000)
  {
    accDataBuffer.push_front(theInertialData.acc);
  }
  // No reason to detect being picked up -> Clear data
  else
  {
    accDataBuffer.clear();
  }

  // Evaluate buffer
  if (accDataBuffer.full())
  {
    float minZ = std::numeric_limits<float>::max(), maxZ = std::numeric_limits<float>::min();
    for (const auto& data : accDataBuffer)
    {
      if (data.z() < minZ)
        minZ = data.z();

      if (data.z() > maxZ)
        maxZ = data.z();
    }

    gotPickedUp = std::abs(maxZ - minZ) > parameters.spawning.accZforPickedUpDifference;
  }
  else
  {
    gotPickedUp = false;
  }
}

void SelfLocator2017::normalizeWeights()
{
  double sumOfPositionConfidences = 0;
  for (auto& hypothesis : poseHypotheses)
  {
    sumOfPositionConfidences += hypothesis->getPositionConfidence();
  }
  double factor = 1 / sumOfPositionConfidences;
  for (auto& hypothesis : poseHypotheses)
  {
    hypothesis->normalizePositionConfidence(factor);
  }
}


void SelfLocator2017::pruneHypothesesInOwnHalf()
{
  PoseHypotheses::iterator it = poseHypotheses.begin();
  while (it != poseHypotheses.end() && poseHypotheses.size() > 1)
  {
    Pose2f strikerPose;
    (*it)->getRobotPose(strikerPose);
    if (strikerPose.translation.x() < 0) // striker will never reach the own half because of manually placement.
    {
      it = poseHypotheses.erase(it);
    }
    else
    {
      it++;
    }
  }
  // end
}

void SelfLocator2017::pruneHypothesesInOpponentHalf()
{
  PoseHypotheses::iterator it = poseHypotheses.begin();
  while (it != poseHypotheses.end() && poseHypotheses.size() > 1)
  {
    Pose2f keeperPose;
    (*it)->getRobotPose(keeperPose);
    if (keeperPose.translation.x() > 0)
    {
      it = poseHypotheses.erase(it);
    }
    else
    {
      it++;
    }
  }
}

void SelfLocator2017::pruneHypothesesOutsideField()
{
  for (PoseHypotheses::iterator it = poseHypotheses.begin();
    it != poseHypotheses.end();)
  {
    const PoseHypotheses::value_type& hypothesis = (*it);
    if (hypothesis->isInsideFieldPlusX(theFieldDimensions, 100))
    {
      //advance
      it++;
    }
    else
    {
      //delete and auto advance
      it = poseHypotheses.erase(it);
    }
  }
}

void SelfLocator2017::pruneHypothesesOutsideCarpet()
{
  for (PoseHypotheses::iterator it = poseHypotheses.begin();
    it != poseHypotheses.end();)
  {
    const PoseHypotheses::value_type& hypothesis = (*it);
    if (hypothesis->isInsideCarpet(theFieldDimensions))
    {
      //advance
      it++;
    }
    else
    {
      if (parameters.debugging.displayWarnings)
        OUTPUT_WARNING("SelfLocator2017 detected hypothesis position that is outside of carpet -> deleting hypothesis");

      DEBUG_LOG(TAG, "Detected hypothesis position that is outside of carpet -> deleting hypothesis");

      //delete and auto advance
      it = poseHypotheses.erase(it);
    }
  }
}

void SelfLocator2017::pruneHypothesesWithInvalidValues()
{
  for (PoseHypotheses::iterator it = poseHypotheses.begin();
    it != poseHypotheses.end();)
  {
    const PoseHypotheses::value_type& hypothesis = (*it);
    if (hypothesis->containsInvalidValues())
    {
      OUTPUT_ERROR("SelfLocator2017 detected impossible state (NaN) -> deleting hypothesis");

      DEBUG_LOG(TAG, "Detected impossible state (NaN) -> deleting hypothesis");

      //delete and auto advance
      it = poseHypotheses.erase(it);
    }
    else
    {
      //advance
      it++;
    }
  }
}

void SelfLocator2017::pruneHypotheses()
{
  if (poseHypotheses.empty())
  {
    OUTPUT_WARNING("SelfLocator2017:no hypotheses left before pruning!");
    DEBUG_LOG(TAG, "No hypotheses left before pruning!");

    return;
  }

  pruneHypothesesOutsideCarpet();
  pruneHypothesesWithInvalidValues();

  // special case: no position on opp side
  if (theGameInfo.competitionType != COMPETITION_TYPE_MIXEDTEAM 
    && (theBehaviorData.role == BehaviorData::keeper // Let's simply assume that the keeper will never be in the opponent half!  :)
      || (theGameInfo.state == STATE_SET && Global::getSettings().gameMode != Settings::penaltyShootout))) // Cannot be in opponent half in set
    pruneHypothesesInOpponentHalf();

  // in penalty shootout the striker will never reach the own half because of manually placement
  if (Global::getSettings().gameMode == Settings::penaltyShootout && theBehaviorData.role != BehaviorData::keeper)
    pruneHypothesesInOwnHalf();

  // Prune hypothesis outside of field in set state. We can be sure to be inside of field after set!
  if (theGameInfo.state == STATE_SET && Global::getSettings().gameMode != Settings::penaltyShootout)
    pruneHypothesesOutsideField();


  // Delete positions that seem to be symmetric
  for (PoseHypotheses::size_type i = 0; i < poseHypotheses.size(); i++)
  {
    Pose2f mirrorOfHypothesis;
    poseHypotheses[i]->getRobotPose(mirrorOfHypothesis);
    mirrorOfHypothesis = getSymmetricPoseOnField(mirrorOfHypothesis);
    for (PoseHypotheses::size_type j = i + 1; j < poseHypotheses.size(); j++)
    {
      Pose2f hypoPose;
      poseHypotheses[j]->getRobotPose(hypoPose);
      // Identify mirror hypothesis
      if (arePosesCloseToEachOther(mirrorOfHypothesis, hypoPose, parameters))
      {
        switch (parameters.localizationStateUpdate.symmetryHandling)
        {
        case SelfLocator2017Parameters::LocalizationStateUpdate::noSymmetricPoses:
          if (poseHypotheses[i]->getSymmetryConfidence() < poseHypotheses[j]->getSymmetryConfidence())
          {
            poseHypotheses[i]->scalePositionConfidence(0);
          }
          else
          {
            poseHypotheses[j]->scalePositionConfidence(0);
          }
          break;

        case SelfLocator2017Parameters::LocalizationStateUpdate::useThresholds:
          // Check if one of them is above threshold to prune other
          if (hasSymmetryBeenFoundAgainAfterLoss(*poseHypotheses[i])
            || hasSymmetryBeenFoundAgainAfterLoss(*poseHypotheses[j]))
          {
            if (poseHypotheses[i]->getSymmetryConfidence() < poseHypotheses[j]->getSymmetryConfidence())
            {
              poseHypotheses[i]->scalePositionConfidence(0);
            }
            else
            {
              poseHypotheses[j]->scalePositionConfidence(0);
            }
          }
          break;

        default:
          // should never happen!
          break;
        }
      }
    }
  }


  // "merge" very close hypotheses, i.e. simply delete the less confident one
  GaussianDistribution3D gd1, gd2;
  for (PoseHypotheses::size_type k = 0; k < poseHypotheses.size(); k++)
  {
    poseHypotheses[k]->extractGaussianDistribution3DFromStateEstimation(gd1);
    for (PoseHypotheses::size_type j = k + 1; j < poseHypotheses.size(); j++)
    {
      poseHypotheses[j]->extractGaussianDistribution3DFromStateEstimation(gd2);
      double likelihood = gd1.normalizedProbabilityAt(gd2.mean) * gd2.normalizedProbabilityAt(gd1.mean);
      if (likelihood > parameters.pruning.likelihoodTresholdForMerging)
      {
        if (poseHypotheses[k]->getPositionConfidence() > poseHypotheses[j]->getPositionConfidence())
        {
          poseHypotheses[j]->scalePositionConfidence(0);
        }
        else
        {
          poseHypotheses[k]->scalePositionConfidence(0);
        }
      }
    }
  }

  // remove hypotheses with too little position confidence
  float maxConfidence = 0.0;
  for (auto& hypothesis : poseHypotheses)
  {
    maxConfidence = std::max(maxConfidence, hypothesis->getPositionConfidence());
  }

  const double pruningThreshold = std::max(maxConfidence / 4, 0.1f);

  PoseHypotheses::iterator it = poseHypotheses.begin();
  while (it != poseHypotheses.end() && poseHypotheses.size() > 1)
  {
    if ((*it)->getPositionConfidence() < pruningThreshold) // no need to make this small threshold a magic number
    {
      it = poseHypotheses.erase(it);
    }
    else
    {
      it++;
    }
  }

  // remove the worst ones to keep the number low
  const PoseHypotheses::size_type maxHypotheses = static_cast<PoseHypotheses::size_type>(parameters.pruning.maxNumberOfHypotheses);
  while (poseHypotheses.size() > maxHypotheses)
  {
    PoseHypotheses::iterator worst = poseHypotheses.begin();
    for (it = poseHypotheses.begin(); it != poseHypotheses.end(); it++)
    {
      if ((*it)->getPositionConfidence() < (*worst)->getPositionConfidence())
      {
        worst = it;
      }
    }
    poseHypotheses.erase(worst);
  }
}


bool SelfLocator2017::hasSymmetryBeenLost(const PoseHypothesis2017 & hypotheses)
{
  bool symmetryConfidenceLost = hypotheses.getSymmetryConfidence() < parameters.localizationStateUpdate.symmetryLostWhenBestConfidenceBelowThisThreshold;
  return symmetryConfidenceLost;
}

bool SelfLocator2017::hasSymmetryBeenFoundAgainAfterLoss(const PoseHypothesis2017 & hypotheses)
{
  bool symmetryConfidenceFoundAgain = hypotheses.getSymmetryConfidence() > parameters.localizationStateUpdate.symmetryFoundAgainWhenBestConfidenceAboveThisThreshold;
  return symmetryConfidenceFoundAgain;
}

bool SelfLocator2017::hasPositionTrackingFailed()
{
  if (theFrameInfo.getTimeSince(lastNonPlayingTimeStamp) < 5000)
    return false;

  const PoseHypothesis2017* bestHyp = getBestHypothesis();

  if (!bestHyp)
    return true;

  bool positionConfidenceLost = bestHyp->getPositionConfidence() < parameters.localizationStateUpdate.positionLostWhenBestConfidenceBelowThisThreshold;
  return positionConfidenceLost;
  //  bool symmetryConfidenceLost = bestHyp.getSymmetryConfidence() < parameters.localizationStateUpdate.symmetryLostWhenBestConfidenceBelowThisThreshold;
  //  bool symmetryLost = positionConfidenceLost || symmetryConfidenceLost;
  //  return symmetryLost;
}

bool SelfLocator2017::hasPositionBeenFoundAfterLoss()
{
  const PoseHypothesis2017* bestHyp = getBestHypothesis();

  if (!bestHyp)
    return false;

  bool positionIsConfident = bestHyp->getPositionConfidence() > parameters.localizationStateUpdate.positionFoundAgainWhenBestConfidenceAboveThisThreshold;
  //  bool symmetryIsConfident =  bestHyp.getSymmetryConfidence() > parameters.localizationStateUpdate.symmetryFoundAgainWhenBestConfidenceAboveThisThreshold;
  //  bool symmetryFoundAgain = positionIsConfident && symmetryIsConfident;
  return positionIsConfident;
}

void SelfLocator2017::evaluateLocalizationState()
{
  // are we playing regularly yet?
  if (theRobotInfo.penalty != PENALTY_NONE || (theGameInfo.state != STATE_PLAYING && theGameInfo.state != STATE_READY))
  {
    lastNonPlayingTimeStamp = theFrameInfo.time;
  }


  // falling down is a transition which will be taken no matter which state we had before.
  if (theFallDownState.state != FallDownState::upright)
  {
    if (localizationState != fallenDown)
    {
      handleFallDown();

      localizationStateAfterGettingUp = localizationState;
    }
    localizationState = fallenDown;
  }
  // penalized is a transition which will be taken no matter which state we had before.
  if (theRobotInfo.penalty != PENALTY_NONE)
  {
    if (localizationState != penalized)
    {
      penalizedTimeStamp = theFrameInfo.time;
    }
    lastPenalty = theRobotInfo.penalty;
    localizationState = penalized;
  }
  if (theGameInfo.state == STATE_SET)
  {
    handleSetState();
  }
  else
  {
    lastNonSetTimestamp = theFrameInfo.time;
  }
  if (theGameInfo.state == STATE_INITIAL)
  {
    handleInitialState();
  }


  switch (localizationState)
  {
  case positionTracking:
    if (hasPositionTrackingFailed())
    {
      lastPositionLostTimeStamp = theFrameInfo.time;
      localizationState = positionLost;
    }
    break;
  case positionLost:
    if (hasPositionBeenFoundAfterLoss())
    {
      localizationState = positionTracking;
    }
    break;
  case fallenDown:
    if (theFallDownState.state == FallDownState::upright && theCameraMatrix.isValid)
    {
      handleGettingUpAfterFallDown();
      localizationState = localizationStateAfterGettingUp;
    }
    break;
  case penalized:
    if (theRobotInfo.penalty == PENALTY_NONE)
    {
      handleUnPenalized();
      unpenalizedTimeStamp = theFrameInfo.time;
      localizationState = positionTracking;
    }
    break;
  default:
    // should never happen!
    char text[100];
    std::sprintf(text, "SelfLocator2017 in unknown localization state %d (%s)", localizationState, getName(localizationState));
    OUTPUT_ERROR(&text[0]);
    break;
  }
}


void SelfLocator2017::handleFallDown()
{
  if (parameters.localizationStateUpdate.symmetryLostWhenFallDownInCenterCircle)
  {
    // Handle fall down in center circle
    const PoseHypothesis2017* bestHyp = getBestHypothesis();
    Pose2f p;
    if (bestHyp)
      bestHyp->getRobotPose(p);

    if (p.translation.norm() < parameters.localizationStateUpdate.unknownSymmetryRadiusAroundCenter)
    {
      for (auto& hypothesis : poseHypotheses)
      {
        hypothesis->setSymmetryConfidence(0.f); //only case where symmetry is set to 0
      }
    }
  }
}

void SelfLocator2017::handleGettingUpAfterFallDown()
{
  // Check if moved far enough that it makes sense to spawn new hypothesis
  if (distanceTraveledFromLastFallDown < parameters.spawning.minDistanceBetweenFallDowns)
  {
    return;
  }
  distanceTraveledFromLastFallDown = 0.f;

  // if we fell on the side, we might end up with +/- 90° orientation errors,
  // so add those hypotheses...
  std::size_t size = poseHypotheses.size();
  for (std::size_t i = 0; i < size; i++)
  {
    PoseHypothesis2017& hypothesis = *poseHypotheses[i];
    hypothesis.scalePositionConfidence(parameters.sensorUpdate.confidenceScaleFactorAfterFallDown); // = 0.75

    for (unsigned char j = 1; j <= parameters.spawning.noAdditionalHypothesisAfterFallDown / 2; ++j)
    {
      poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(hypothesis, theFrameInfo.time)); // copy hypotheses
      poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(hypothesis, theFrameInfo.time)); // copy hypotheses
      PoseHypotheses::value_type& p1 = *(poseHypotheses.end() - 2); // Get new hypothesis
      PoseHypotheses::value_type & p2 = *(poseHypotheses.end() - 1); // Get new hypothesis

      const float rot = float(j) * parameters.spawning.angleOfAdditionalHypothesisAfterFallDown / parameters.spawning.noAdditionalHypothesisAfterFallDown;
      const float scale = 1 - std::abs(Angle::normalize(rot)) / pi;

      const float confidenceScaleMin = 1 - parameters.sensorUpdate.confidenceScaleFactorAfterFallDown;
      const float confidenceScaleMax = parameters.sensorUpdate.confidenceScaleFactorAfterFallDown;
      float confidenceScale = 0;
      if (confidenceScaleMin < confidenceScaleMax)
      {
        confidenceScale = confidenceScaleMin + (confidenceScaleMax - confidenceScaleMin) * scale;
      }
      else
      {
        confidenceScale = confidenceScaleMax + (confidenceScaleMin - confidenceScaleMax) * scale;
      }
      p1->rotateHypothesis(rot);
      p1->scalePositionConfidence(confidenceScale);
      p2->rotateHypothesis(-rot);
      p2->scalePositionConfidence(confidenceScale);
    }
  }
}

void SelfLocator2017::handleUnPenalized()
{
  if (lastPenalty == PENALTY_SPL_ILLEGAL_MOTION_IN_SET)
  {
    // If penalty was illegal motion in set
    // -> robots stayed where they are (none taken out of game)
    // -> do nothing, just keep as is
    return;
  }

  if (theFrameInfo.getTimeSince(penalizedTimeStamp) > 15000)
  {
    // penalized for more than 15 seconds, so it was most likely no mistake
    // and we can delete all other hypotheses
    poseHypotheses.clear();
  }
  else
  {
    // penalized for less than 15 seconds, so it might have been a mistake
    // and we just decrease the confidence of the other hypotheses
    for (auto& hypothesis : poseHypotheses)
    {
      hypothesis->scalePositionConfidence(0.5f);
    }
  }
  if (lastPenalty == PENALTY_MANUAL)
  {
    addHypothesesOnPenaltyPositions(0.3f);
  }
  else
  {
    addHypothesesOnPenaltyPositions(0.7f);
  }
  addHypothesesOnManualPositioningPositions();
}

void SelfLocator2017::handleSetState()
{
  // delete all hypotheses on the wrong side of the field unless in penalty shootout
  if (Global::getSettings().gameMode != Settings::penaltyShootout)
  {
    auto i = poseHypotheses.begin();
    while (i != poseHypotheses.end() && poseHypotheses.size() > 1)
    {
      Pose2f pose;
      (*i)->getRobotPose(pose);
      // should be "pose.translation.x > 0", but we allow a little buffer of 20cm
      bool poseIsInOpponentHalf = pose.translation.x() > 200; // no need to make this small threshold a magic number
      if (poseIsInOpponentHalf)
      {
        i = poseHypotheses.erase(i);
      }
      else
      {
        //also reset symmetry confidence of existing hypotheses
        (*i)->setSymmetryConfidence(parameters.spawning.symmetryConfidenceWhenPositionedManually);
        i++;
      }
    }
  }
}

void SelfLocator2017::handleInitialState()
{
  timeStampFirstReadyState = theFrameInfo.time;
}


void SelfLocator2017::addHypothesesOnManualPositioningPositions()
{
  const float sc = parameters.spawning.symmetryConfidenceWhenPositionedManually;

  bool ownKickoff = theGameInfo.kickingTeam == theOwnTeamInfo.teamNumber;
  // special position for goalie
  const bool isGoalKeeper = (theBehaviorData.role == BehaviorData::keeper);
  if (isGoalKeeper)
  {
    // penalty shootout
    if (Global::getSettings().gameMode == Settings::penaltyShootout)
    {
      poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
        Pose2f(0, positionsByRules.penaltyShootoutGoaliePosition),
        parameters.spawning.positionConfidenceWhenPositionedManuallyForGoalKeeper,
        sc,
        theFrameInfo.time,
        parameters));
    }
    else
    {
      poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
        Pose2f(0, positionsByRules.goaliePosition),
        parameters.spawning.positionConfidenceWhenPositionedManuallyForGoalKeeper,
        sc,
        theFrameInfo.time,
        parameters));
    }
  }
  // one of the field positions for field players
  else
  {
    // penalty shootout
    if (Global::getSettings().gameMode == Settings::penaltyShootout)
    {
      addPenaltyStrikerStartingHypothesis();
    }
    else
    {
      for (auto& position : (ownKickoff ? positionsByRules.fieldPlayerPositionsOwnKickoff : positionsByRules.fieldPlayerPositionsOppKickoff))
      {
        poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
          Pose2f(0, position),
          parameters.spawning.positionConfidenceWhenPositionedManually,
          sc,
          theFrameInfo.time,
          parameters));
      }
    }
  }
}

void SelfLocator2017::addHypothesesOnInitialKickoffPositions()
{
  if (Global::getSettings().gameMode != Settings::penaltyShootout)
  {
    addHypothesesOnPenaltyPositions(parameters.spawning.positionConfidenceWhenPositionedManually);
  }
  else
  {
    addPenaltyStrikerStartingHypothesis();
  }
}

void SelfLocator2017::addPenaltyStrikerStartingHypothesis()
{
  // in 2018 rules: starting position on circle around penalty mark
  Vector2f penaltyMarkPos = Vector2f(
    theFieldDimensions.xPosOpponentPenaltyMark,
    theFieldDimensions.yPosCenterGoal);

  Vector2f backTransl = Vector2f(positionsByRules.penaltyShootStartingRadius, 0);
  Vector2f rot;

  const float sc = parameters.spawning.symmetryConfidenceWhenPositionedManually;

  for (int angle : positionsByRules.penaltyShootAngles) {
    Vector2f rot = Vector2f(backTransl).rotate(Angle::fromDegrees(angle));
    poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
      Pose2f(rot.angle(), penaltyMarkPos - rot),
      parameters.spawning.positionConfidenceWhenPositionedManually,
      sc,
      theFrameInfo.time,
      parameters));
  }
}

void SelfLocator2017::addHypothesesOnPenaltyPositions(float newPositionConfidence)
{
  if (Global::getSettings().gameMode != Settings::penaltyShootout)
  {
    float sc = parameters.spawning.symmetryConfidenceWhenPositionedManually;

    for (const auto& offset : positionsByRules.xOffsetPenaltyPositions)
    {
      poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
        Pose2f(pi_2, offset, theFieldDimensions.yPosRightSideline),
        newPositionConfidence,
        sc,
        theFrameInfo.time,
        parameters));
      poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
        Pose2f(-pi_2, offset, theFieldDimensions.yPosLeftSideline),
        newPositionConfidence,
        sc,
        theFrameInfo.time,
        parameters));
    }
  }
}

bool SelfLocator2017::addNewHypothesesFromLineMatches()
{
  const bool isLost = (localizationState == positionLost);

  const PoseHypothesis2017 * bestHypo = getBestHypothesis();
  Pose2f hypoPose;
  if (bestHypo)
    bestHypo->getRobotPose(hypoPose);

  std::vector<HypothesisBase> additionalHypotheses;
  size_t size = theLineMatchingResult.poseHypothesis.size();
  if (size > 0)
  {
    for (auto& ph : theLineMatchingResult.poseHypothesis)
    {
      // Only add hypotheses on own side
      // AddPoseToHypothesisVector will handle symmetry
      // Also check if pose is reasonable unique or is close to actual best pose
      if (ph.pose.translation.x() < 0 && (isLost || size < 7 || !bestHypo || arePosesCloseToEachOther(ph.pose, hypoPose, parameters)))
        addPoseToHypothesisVector((Pose2f)ph.pose, additionalHypotheses, parameters.spawning.lineBasedPositionConfidenceWhenPositionTracking);
    }
  }

  // Add hypotheses to the system
  for (auto& hyp : additionalHypotheses)
  {
    poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
      hyp.pose,
      hyp.positionConfidence,
      hyp.symmetryConfidence,
      theFrameInfo.time,
      parameters));
  }
  return !additionalHypotheses.empty();
}

bool SelfLocator2017::addNewHypothesesFromPenaltyCrossLine()
{
  // Add new hypotheses by PenaltyCross
  // it gives an absolute Pose

  Pose2f pose;
  const PoseHypothesis2017* bestHypo = getBestHypothesis();
  float poseConfidence = 0.f;
  float bestHypoConfidence = std::max(bestHypo ? bestHypo->getPositionConfidence() : 0.f, 0.5f);

  std::vector<HypothesisBase> additionalHypotheses;
  if (PoseGenerator::getPoseFromPenaltyCrossAndLine(theFieldDimensions, theCLIPFieldLinesPercept, thePenaltyCrossPercept, pose, poseConfidence))
  {
    float confidence = bestHypoConfidence * poseConfidence * parameters.spawning.penaltyCrossBaseConfidence;
    addPoseToHypothesisVector(pose, additionalHypotheses, confidence);
  }

  // Add hypotheses to the system
  for (auto& hyp : additionalHypotheses)
  {
    poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
      hyp.pose,
      hyp.positionConfidence,
      hyp.symmetryConfidence,
      theFrameInfo.time,
      parameters));
  }
  return !additionalHypotheses.empty();
}

bool SelfLocator2017::addNewHypothesesFromCenterCirleAndLine()
{
  // Add new hypotheses by CenterCircle and it's CenterLine
  // it gives an absolute Pose
  
  Pose2f pose;
  const PoseHypothesis2017 * bestHypo = getBestHypothesis();
  float poseConfidence = 0.f;
  float bestHypoConfidence = std::max(bestHypo ? bestHypo->getPositionConfidence() : 0.f, 0.5f);

  std::vector<HypothesisBase> additionalHypotheses;
  if (PoseGenerator::getPoseFromCenterCircleAndCenterLine(theCLIPFieldLinesPercept, theCLIPCenterCirclePercept, pose, poseConfidence) > 0)
  {
    float confidence = bestHypoConfidence * poseConfidence * parameters.spawning.centerCircleBaseConfidence;
    addPoseToHypothesisVector(pose, additionalHypotheses, confidence);
  }

  // Add hypotheses to the system
  for (auto& hyp : additionalHypotheses)
  {
    poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
      hyp.pose,
      hyp.positionConfidence,
      hyp.symmetryConfidence,
      theFrameInfo.time,
      parameters));
  }
  return !additionalHypotheses.empty();
}

bool SelfLocator2017::addNewHypothesesFromGoal()
{
  const bool isGoalKeeper = (theBehaviorData.role == BehaviorData::keeper);
  if (isGoalKeeper) return false; // Do not use for goalie

  std::vector<HypothesisBase> additionalHypotheses;
  if (theCLIPGoalPercept.numberOfGoalPosts >= 2)
  {
    float poseConfidence = 0.f;
    Vector2f leftPost = Vector2f();
    Vector2f rightPost = Vector2f();
    Pose2f pose;
    const PoseHypothesis2017* best = getBestHypothesis();
    float bestHypoConfidence = std::max(best ? best->getPositionConfidence() : 0.f, 0.5f);

    if (getLeftAndRightGoalPostFromGoalPercept(theCLIPGoalPercept, leftPost, rightPost, poseConfidence)
      && PoseGenerator::getPoseFromGoalObservation(theFieldDimensions, leftPost, rightPost, pose))
    {
      float confidence = bestHypoConfidence * poseConfidence * parameters.spawning.goalBaseConfidence;
      addPoseToHypothesisVector(pose, additionalHypotheses, confidence);
    }
  }

  // Add hypotheses to the system
  for (auto& hyp : additionalHypotheses)
  {
    poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
      hyp.pose,
      hyp.positionConfidence,
      hyp.symmetryConfidence,
      theFrameInfo.time,
      parameters));
  }
  return !additionalHypotheses.empty();
}

bool SelfLocator2017::addNewHypothesesFromLandmark()
{
  return addNewHypothesesFromPenaltyCrossLine()
    || addNewHypothesesFromCenterCirleAndLine()
    || addNewHypothesesFromGoal();
}

bool SelfLocator2017::addNewHypothesesIfSymmetryLost()
{
  if (parameters.localizationStateUpdate.symmetryHandling == SelfLocator2017Parameters::LocalizationStateUpdate::useThresholds)
  {
    std::size_t size = poseHypotheses.size();
    for (std::size_t i = 0; i < size; i++)
    {
      PoseHypotheses::value_type::element_type& hypothesis = *poseHypotheses[i];

      // Check if symmetry has been lost for hypothesis and that there is no symmetric pose yet
      if (hasSymmetryBeenLost(hypothesis) && !hypothesis.mirroredHypothesis())
      {
        Pose2f symmetricPose;
        hypothesis.getRobotPose(symmetricPose);
        symmetricPose = getSymmetricPoseOnField(symmetricPose);

        poseHypotheses.push_back(std::make_unique<PoseHypothesis2017>(
          symmetricPose,
          hypothesis.getPositionConfidence(),
          (hypothesis.getSymmetryConfidence() + parameters.localizationStateUpdate.symmetryFoundAgainWhenBestConfidenceAboveThisThreshold) / 2,
          theFrameInfo.time,
          parameters));

        // Link mirrored hypothesis
        poseHypotheses.back()->mirroredHypothesis() = &hypothesis;
        hypothesis.mirroredHypothesis() = poseHypotheses.back().get();
      }
    }

    return poseHypotheses.size() > size;
  }
  else
  {
    return false;
  }
}

bool SelfLocator2017::addNewHypotheses()
{
  bool added = false;

  // Add hypothesis in initial all the time
  if (theGameInfo.state == STATE_INITIAL)
  {
    addHypothesesOnInitialKickoffPositions();
    added = true;
  }

  switch (localizationState)
  {
  case positionLost:
    // Add hypothesis for set state
    if (theGameInfo.state == STATE_SET)
    {
      addHypothesesOnManualPositioningPositions();
      added = true;
    }
    else if (parameters.spawning.landmarkBasedHypothesesSpawn & SelfLocator2017Parameters::Spawning::spawnIfPositionLost)
    {
      added |= addNewHypothesesFromLineMatches();
      added |= addNewHypothesesFromLandmark();
    }
    break;

  case positionTracking:
  {
    const PoseHypothesis2017* bestHyp = getBestHypothesis();
    // Add hypothesis for set state
    if (theGameInfo.state == STATE_SET && (gotPickedUp || Global::getSettings().gameMode == Settings::penaltyShootout))
    {
      addHypothesesOnManualPositioningPositions();
      added = true;
    }
    else if (parameters.spawning.landmarkBasedHypothesesSpawn & SelfLocator2017Parameters::Spawning::spawnIfPositionTracking
      && (!bestHyp || bestHyp->getPositionConfidence() < parameters.spawning.spawnWhilePositionTrackingWhenBestConfidenceBelowThisThreshold))
    {
      added |= addNewHypothesesFromLineMatches();
      added |= addNewHypothesesFromLandmark();
    }
    else if (gotPickedUp)
    {
      // Got picked up but we're not in set -> Unpenalized situation
      addHypothesesOnPenaltyPositions(.7f);
    }
    break;
  }

  default:
    void(0); //nothing todo;
  }

  added |= addNewHypothesesIfSymmetryLost();

  return added;
}


float SelfLocator2017::getRobotPoseValidity(const PoseHypothesis2017 & poseHypothesis)
{
  float validity = 0;

  switch (localizationState)
  {
  case positionTracking:
    validity = poseHypothesis.getPositionConfidence();
    break;
  case positionLost:
    validity = 0;
    break;
  case fallenDown:
    validity = poseHypothesis.getPositionConfidence();
    break;
  default:
    // should never happen!
    break;
  }
  return validity;
}


inline void SelfLocator2017::initDebugging()
{
  //DECLARE_DEBUG_DRAWING("module:SelfLocator2017:hypotheses", "drawingOnField");
  DECLARE_DEBUG_DRAWING("module:SelfLocator2017:correspondences", "drawingOnField");
  DECLARE_DEBUG_DRAWING("module:SelfLocator2017:LocalizationState", "drawingOnField");
  DECLARE_DEBUG_DRAWING("module:SelfLocator2017:poseFromCenterCircle", "drawingOnField");
  DECLARE_DEBUG_DRAWING("module:SelfLocator2017:poseFromPenaltyCross", "drawingOnField");
  DECLARE_DEBUG_DRAWING("module:SelfLocator2017:GaussianTools:covarianceEllipse", "drawingOnField");
}

void SelfLocator2017::doGlobalDebugging()
{
  ; // IMPORTANT!!! Do not remove!

  if (poseHypotheses.empty())
  {
    DRAWTEXT("module:SelfLocator2017:LocalizationState", -500, -2200, 50, ColorRGBA(255, 255, 255), "LocalizationState: no more hypotheses! Should never happen!");
    return;
  }

  const PoseHypothesis2017& bestHyp = *getBestHypothesis();
  if (!bestHyp.isInsideCarpet(theFieldDimensions))
    return;

  /* NOT NEEDED; Can be observed by representation
  COMPLEX_DRAWING("module:SelfLocator2017:hypotheses")
  {
    for (auto &hypothesis : poseHypotheses)
    {
      float factor = std::max(std::min(theFrameInfo.getTimeSince(hypothesis->getCreationTime()) / parameters.debugging.durationHighlightAddedHypothesis, 1.f), 0.f);
      hypothesis->draw(ColorRGBA(255, static_cast<unsigned char>(factor * 255), static_cast<unsigned char>(factor * 255), static_cast<unsigned char>(255.0*hypothesis->getPositionConfidence())));
    }

    //recolor the best
    bestHyp.draw(ColorRGBA(255, 0, 0, (unsigned char)(255.0*bestHyp.getPositionConfidence())));
  }
  */

  switch (localizationState)
  {
  case positionTracking:
    DRAWTEXT("module:SelfLocator2017:LocalizationState", -1000, -3500, 100, ColorRGBA(255, 255, 255), "LocalizationState: positionTracking");
    break;
  case positionLost:
    DRAWTEXT("module:SelfLocator2017:LocalizationState", -1000, -3500, 100, ColorRGBA(255, 255, 255), "LocalizationState: positionLost");
    break;
  case fallenDown:
    DRAWTEXT("module:SelfLocator2017:LocalizationState", -1000, -3500, 100, ColorRGBA(255, 255, 255), "LocalizationState: fallenDown");
    break;
  case penalized:
    DRAWTEXT("module:SelfLocator2017:LocalizationState", -1000, -3500, 100, ColorRGBA(255, 255, 255), "LocalizationState: penalized");
    break;
  default:
    DRAWTEXT("module:SelfLocator2017:LocalizationState", -1000, -3500, 100, ColorRGBA(255, 255, 255), "LocalizationState: default... should never happen!");
    // should never happen!
    break;
  }
}

void SelfLocator2017::generateOutputData()
{
  // Get best hypothesis
  const PoseHypothesis2017* bestRobotPoseHypothesis = getBestHypothesis();

  // Fill robot pose
  if (bestRobotPoseHypothesis)
  {
    bestRobotPoseHypothesis->getRobotPose(m_robotPose);
    m_robotPose.validity = getRobotPoseValidity(*bestRobotPoseHypothesis);
    m_robotPose.symmetry = bestRobotPoseHypothesis->getSymmetryConfidence();
    if (m_robotPose.validity > parameters.spawning.spawnWhilePositionTrackingWhenBestConfidenceBelowThisThreshold)
    {
      foundGoodPosition = true;
      timeStampLastGoodPosition = theFrameInfo.time;
      lastGoodPosition = m_robotPose;
      distanceTraveledFromLastGoodPosition = Pose2f();
    }
    else if (foundGoodPosition)
    {
      distanceTraveledFromLastGoodPosition += (theOdometryData - lastOdometryData);
    }
  }
  else
  {
    m_robotPose.rotation = 0.f;
    m_robotPose.translation = Vector2f::Zero();
    m_robotPose.validity = 0.f;
    m_robotPose.symmetry = 0.f;
  }

  // Fill side confidence
  m_sideConfidence.sideConfidence = bestRobotPoseHypothesis ? bestRobotPoseHypothesis->getSymmetryConfidence() : 0.f;

  // Helper
  GaussianDistribution3D gd;

  // Fill best hypothesis
  if (bestRobotPoseHypothesis)
  {
    bestRobotPoseHypothesis->extractGaussianDistribution3DFromStateEstimation(gd);
    m_robotPoseHypothesis.validity = getRobotPoseValidity(*bestRobotPoseHypothesis);
    m_robotPoseHypothesis.covariance = gd.covariance;
    m_robotPoseHypothesis.robotPoseReceivedMeasurementUpdate = bestRobotPoseHypothesis->performedSensorUpdate();
    bestRobotPoseHypothesis->getRobotPose((Pose2f&)m_robotPoseHypothesis, m_robotPoseHypothesis.covariance);
  }
  else
  {
    m_robotPoseHypothesis.validity = 0.f;
    m_robotPoseHypothesis.covariance = Matrix3d::Zero();
    m_robotPoseHypothesis.robotPoseReceivedMeasurementUpdate = false;
  }

  // Fill all hypotheses
  m_robotPoseHypotheses.hypotheses.clear();
  for (const auto& hypothesis : poseHypotheses)
  {
    RobotPoseHypothesis ph;
    hypothesis->extractGaussianDistribution3DFromStateEstimation(gd);
    ph.validity = getRobotPoseValidity(*hypothesis);
    ph.symmetry = hypothesis->getSymmetryConfidence();
    ph.covariance = gd.covariance;
    ph.robotPoseReceivedMeasurementUpdate = hypothesis->performedSensorUpdate();
    hypothesis->getRobotPose((Pose2f&)ph, ph.covariance);
    m_robotPoseHypotheses.hypotheses.push_back(ph);
  }
}


const PoseHypothesis2017* SelfLocator2017::getBestHypothesis()
{
  PoseHypotheses::value_type::pointer current = nullptr;
  PoseHypotheses::value_type::pointer best = nullptr;
  float bestConfidence = 0;

  // Search for overall best and current hypothesis
  for (const auto& hypothesis : poseHypotheses)
  {
    float confidence = hypothesis->getPositionConfidence();
    if (!best || confidence > bestConfidence)
    {
      best = hypothesis.get();
      bestConfidence = confidence;
    }
    if (hypothesis->getUniqueId() == lastBestHypothesisUniqueId)
    {
      current = hypothesis.get();
    }
  }

  // Check if current is not alive anymore OR
  if (!current ||
    // if overall best is not currently used hypothesis
    (
      best != current &&
      (
        // Check if best confidence is much better than currently used hypothesis OR
        (bestConfidence - current->getPositionConfidence()) > parameters.processUpdate.positionConfidenceHysteresisForKeepingBestHypothesis ||
        // we're just entering the field -> Allow to switch more often in this case as a lot of symmetric (within own half) hypotheses might be present
        theFrameInfo.getTimeSince(lastNonPlayingTimeStamp) < 5000
      )
    )
  )
  {
    if (best) // Check if best is set (maybe no hypotheses?, This should not happen)
    {
      // Be less certain about side confidence since best hypothesis got switched
      // This is required if exactly two robots see the ball (no goalie)
      // The one that switched the best hypothesis more often lately has less impact on symmetry update
      if (lastBestHypothesisUniqueId && (theFrameInfo.getTimeSince(lastNonPlayingTimeStamp) > 15000))
      {
        std::underlying_type<SideConfidence::ConfidenceState>::type cs = m_sideConfidence.confidenceState;

        cs = std::min(std::underlying_type<SideConfidence::ConfidenceState>::type(SideConfidence::numOfConfidenceStates - 1),
          std::max(std::underlying_type<SideConfidence::ConfidenceState>::type(SideConfidence::CONFIDENT), ++cs)); // The lower the more confident -> So increase if best hypo switched

        m_sideConfidence.confidenceState = static_cast<SideConfidence::ConfidenceState>(cs);
      }

      // Check if there's a symmetric pose with a higher symmetry confidence
      if (best->mirroredHypothesis() && best->mirroredHypothesis()->getSymmetryConfidence() > best->getSymmetryConfidence())
        best = best->mirroredHypothesis();

      if (parameters.debugging.displayWarnings)
        OUTPUT_TEXT("Switching best hypothesis because of confidence.");

      DEBUG_LOG(TAG, "Switching best hypothesis because of confidence.");

      // Set new best hypothesis
      lastBestHypothesisUniqueId = best->getUniqueId();
    }
    return best;
  }
  else
  {
    // Check if there's a symmetric pose with a higher symmetry confidence
    if (current->mirroredHypothesis() && current->mirroredHypothesis()->getSymmetryConfidence() > current->getSymmetryConfidence())
    {
      if (parameters.debugging.displayWarnings)
        OUTPUT_TEXT("Switching best hypothesis because of symmetry.");

      DEBUG_LOG(TAG, "Switching best hypothesis because of symmetry.");

      current = current->mirroredHypothesis();
      lastBestHypothesisUniqueId = current->getUniqueId();
    }
    return current;
  }
}

void SelfLocator2017::addPoseToHypothesisVector(const Pose2f & pose, std::vector<HypothesisBase> & additionalHypotheses, const float& poseConfidence)
{
  const PoseHypothesis2017* bestHypo = getBestHypothesis();

  const Pose2f symmetricPose = getSymmetricPoseOnField(pose), * nearestPose = 0;

  float symmetryConfidence = 0;

  // Determine minimum position confidence for hypothesis to check for closest position
  const float minConfidence = std::max(
    parameters.localizationStateUpdate.positionLostWhenBestConfidenceBelowThisThreshold,
    bestHypo ? bestHypo->getPositionConfidence() - parameters.spawning.confidenceIntervalForCheckOfHypothesis : 0);

  double bestDistance = -1;
  for (auto& hypo : poseHypotheses)
  {
    // Check hypothesis qualifies for check
    if (hypo->getPositionConfidence() >= minConfidence)
    {
      Pose2f hypoPose;
      hypo->getRobotPose(hypoPose);
      const float distanceToPose = (pose - hypoPose).translation.norm();
      const float distanceToSymPose = (symmetricPose - hypoPose).translation.norm();

      // Check closest pose for this hypothesis
      const Pose2f * current_closest;
      float distance;
      if (distanceToPose <= distanceToSymPose)
      {
        distance = distanceToPose;
        current_closest = &pose;
      }
      else
      {
        distance = distanceToSymPose;
        current_closest = &symmetricPose;
      }

      // Do not spawn if pose is on other side of the field but we are still entering the field (initial or penalized)
      const int limit = static_cast<int>(parameters.spawning.limitSpawningToOwnSideTimeout);
      if (((theFrameInfo.getTimeSince(timeStampFirstReadyState) < limit) || (theFrameInfo.getTimeSince(unpenalizedTimeStamp) < limit)) &&
        !isPoseOnOwnFieldSide(*current_closest))
        continue;

      // Check if pose is closer than best distance
      if (bestDistance < 0 || (distance < bestDistance))
      {
        bestDistance = distance;
        nearestPose = current_closest;
        symmetryConfidence = hypo->getSymmetryConfidence();
      }
    }
  }

  // Check if odometry from is better than others
  if (parameters.spawning.useOdometryForSpawning && foundGoodPosition)
  {
    Pose2f odometryPose = (lastGoodPosition + distanceTraveledFromLastGoodPosition);
    const float distanceToPose = (pose - odometryPose).translation.norm();
    const float distanceToSymPose = (symmetricPose - odometryPose).translation.norm();

    // Check closest pose for this hypothesis
    const Pose2f * current_closest = (distanceToPose <= distanceToSymPose) ? &pose : &symmetricPose;

    if (bestDistance < 0 || (odometryPose - *current_closest).translation.norm() < bestDistance)
    {
      nearestPose = current_closest;
      symmetryConfidence = parameters.localizationStateUpdate.symmetryFoundAgainWhenBestConfidenceAboveThisThreshold * (bestHypo ? bestHypo->getSymmetryConfidence() : 0.f);
    }
  }

  // Check if we found a decent pose to spawn
  if (nearestPose)
  {
    // Only spawn if confidence higher than lost threshold
    if (poseConfidence >= parameters.localizationStateUpdate.positionLostWhenBestConfidenceBelowThisThreshold)
      additionalHypotheses.push_back(HypothesisBase(*nearestPose, poseConfidence, symmetryConfidence));
  }
  else if (localizationState == positionLost)
  {
    // Add the normal pose if loca has been lost
    additionalHypotheses.push_back(HypothesisBase(pose,
      parameters.localizationStateUpdate.positionLostWhenBestConfidenceBelowThisThreshold,
      parameters.localizationStateUpdate.symmetryLostWhenBestConfidenceBelowThisThreshold));
  }
}


MAKE_MODULE(SelfLocator2017, modeling)
