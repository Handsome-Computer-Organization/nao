#include "WalkParamsProvider.h"
#include "Tools/Module/ModuleManager.h"

void WalkParamsProvider::update(WalkingEngineParams& walkingEngineParams)
{
  if (!initializedWP)
  {
    load(walkingEngineParams);
  }
}

void WalkParamsProvider::update(LegJointSensorControlParameters& legJointSensorControlParameters)
{
  if (!initializedLJSCP)
  {
    load(legJointSensorControlParameters);
  }
}

void WalkParamsProvider::update(FLIPMObserverGains& flipmObserverGains) {
  if (!initializedFOP)
  {
    load(flipmObserverGains);
  }
}

void WalkParamsProvider::load(WalkingEngineParams& walkingEngineParams)
{
  ModuleManager::Configuration config;
  InMapFile stream("modules.cfg");
  if (stream.exists())
  {
    stream >> config;
  }
  else
  {
    ASSERT(false);
  }

  bool flipm = false;
  for (ModuleManager::Configuration::RepresentationProvider por : config.representationProviders) {
    if (por.representation == "TargetCoM" && por.provider == "FLIPMController") {
      flipm = true;
      break;
    }
  }

  if (flipm) {
    InMapFile file("walkingParamsFLIPM.cfg");
    if (file.exists())
      file >> walkingEngineParams;
    else
    {
      ASSERT(false);
    }
  }
  else {
    InMapFile file("walkingParams.cfg");
    if (file.exists())
      file >> walkingEngineParams;
    else
    {
      ASSERT(false);
    }
  }
  initializedWP = true;
}

void WalkParamsProvider::load(FLIPMObserverGains& flipmObserverGains) {
  InMapFile file("flipmObserverGains.cfg");
  if (file.exists())
    file >> flipmObserverGains;
  else
  {
    ASSERT(false);
  }
  initializedFOP = true;
}

void WalkParamsProvider::load(LegJointSensorControlParameters& legJointSensorControlParameters)
{
  InMapFile file("legJointSensorControlParameters.cfg");
  if (file.exists())
    file >> legJointSensorControlParameters;
  else
  {
    ASSERT(false);
  }
  initializedLJSCP = true;
}

MAKE_MODULE(WalkParamsProvider, dortmundWalkingEngine)