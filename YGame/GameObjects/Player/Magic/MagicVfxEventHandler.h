#pragma once

#include "IMagicEventHandler.h"
#include "Vector3.h"

#include <string>

class BaseCollider;
class Player;

class MagicVfxEventHandler : public IMagicEventHandler {
public:
  void Execute(const MagicTimelineEvent &event,
               const MagicEventContext &context) override;

private:
  Vector3 ResolveTargetPoint(Player &player, const MagicTimelineEvent &event,
                             const Vector3 &origin) const;
  float ResolveRange(const MagicTimelineEvent &event, float chargeTime) const;
  float ResolveScale(const MagicTimelineEvent &event, float chargeTime) const;
  float ResolveTimeScale(const MagicTimelineEvent &event) const;
  std::string ResolveAssetName(const MagicTimelineEvent &event,
                               const std::string &fallback) const;
};
