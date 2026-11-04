#include "KlipperParams.h"
#include "includes.h"

KLIPPER_PARAMS klipperParams = {
  .pa = 0.0f,
  .accel = 0,
  .scv = 0.0f,
  .z_offset = 0.0f,
  .loaded = false,
};

void klipperParamsUpdate(float pa, uint32_t accel, float scv, float z_offset)
{
  klipperParams.pa = pa;
  klipperParams.accel = accel;
  klipperParams.scv = scv;
  klipperParams.z_offset = z_offset;
  klipperParams.loaded = true;

  setParameter(P_LIN_ADV, 0, pa);
  setParameter(P_ACCELERATION, 0, (float) accel);
  setParameter(P_JERK, 0, scv);
  setParameter(P_PROBE_OFFSET, AXIS_INDEX_Z, z_offset);
}
