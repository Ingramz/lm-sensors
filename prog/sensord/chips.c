/*
 * sensord
 *
 * A daemon that periodically logs sensor information to syslog.
 *
 * Copyright (c) 1999-2002 Merlin Hughes <merlin@merlin.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sensord.h"

/* TODO: Temp in C/F */

/** formatters **/

static char buff[4096];

static const char *
fmtExtra
(int alarm, int beep) {
  if (alarm)
    sprintf (buff + strlen (buff), " [ALARM]");
  if (beep)
    sprintf (buff + strlen (buff), " (beep)");
  return buff;
}

static const char *
fmtTemps_1
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.1f C (limit = %.1f C, hysteresis = %.1f C)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtTemps_minmax_1
(const double values[], int alarm, int beep) {
 sprintf (buff, "%.1f C (min = %.1f C, max = %.1f C)", values[0], values[1], values[2]);
 return fmtExtra (alarm, beep);
}

static const char *
fmtTemp_only
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.1f C", values[0]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtVolt_2
(const double values[], int alarm, int beep) {
  sprintf (buff, "%+.2f V", values[0]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtVolt_3
(const double values[], int alarm, int beep) {
  sprintf (buff, "%+.3f V", values[0]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtVolts_2
(const double values[], int alarm, int beep) {
  sprintf (buff, "%+.2f V (min = %+.2f V, max = %+.2f V)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtFans_0
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.0f RPM (min = %.0f RPM, div = %.0f)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtFans_nodiv_0
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.0f RPM (min = %.0f RPM)", values[0], values[1]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtFan_only
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.0f RPM", values[0]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtSoundAlarm
(const double values[], int alarm, int beep) {
  sprintf (buff, "Sound alarm %s", (values[0] < 0.5) ? "disabled" : "enabled");
  return fmtExtra (alarm, beep);
}

static const char *
rrdF0
(const double values[]) {
  sprintf (buff, "%.0f", values[0]);
  return buff;
}

static const char *
rrdF1
(const double values[]) {
  sprintf (buff, "%.1f", values[0]);
  return buff;
}

static const char *
rrdF2
(const double values[]) {
  sprintf (buff, "%.2f", values[0]);
  return buff;
}

static const char *
rrdF3
(const double values[]) {
  sprintf (buff, "%.3f", values[0]);
  return buff;
}

static void getAvailableFeatures (const sensors_chip_name *name,
                                  const sensors_feature *feature,
                                  short *has_features,
                                  int *feature_nrs, int size,
                                  int first_val)
{
  const sensors_subfeature *iter;
  int i = 0;

  while ((iter = sensors_get_all_subfeatures(name, feature, &i))) {
    int index0;

    index0 = iter->type - first_val;
    if (index0 < 0 || index0 >= size)
      /* New feature in libsensors? Ignore. */
      continue;

    has_features[index0] = 1;
    feature_nrs[index0] = iter->number;
  }
}

#define IN_FEATURE(x)      has_features[x - SENSORS_SUBFEATURE_IN_INPUT]
#define IN_FEATURE_NR(x)   feature_nrs[x - SENSORS_SUBFEATURE_IN_INPUT]
static void fillChipVoltage (FeatureDescriptor *voltage,
                             const sensors_chip_name *name,
                             const sensors_feature *feature)
{
  const int size = SENSORS_SUBFEATURE_IN_BEEP - SENSORS_SUBFEATURE_IN_INPUT + 1;
  short has_features[SENSORS_SUBFEATURE_IN_BEEP - SENSORS_SUBFEATURE_IN_INPUT + 1] = { 0, };
  int feature_nrs[SENSORS_SUBFEATURE_IN_BEEP - SENSORS_SUBFEATURE_IN_INPUT + 1];
  int pos = 0;

  voltage->rrd = rrdF2;
  voltage->type = DataType_voltage;

  getAvailableFeatures (name, feature, has_features,
                        feature_nrs, size, SENSORS_SUBFEATURE_IN_INPUT);

  voltage->dataNumbers[pos++] = IN_FEATURE_NR(SENSORS_SUBFEATURE_IN_INPUT);

  if (IN_FEATURE(SENSORS_SUBFEATURE_IN_MIN) &&
      IN_FEATURE(SENSORS_SUBFEATURE_IN_MAX)) {
    voltage->format = fmtVolts_2;
    voltage->dataNumbers[pos++] = IN_FEATURE_NR(SENSORS_SUBFEATURE_IN_MIN);
    voltage->dataNumbers[pos++] = IN_FEATURE_NR(SENSORS_SUBFEATURE_IN_MAX);
  } else {
    voltage->format = fmtVolt_2;
  }
  
  /* terminate the list */
  voltage->dataNumbers[pos] = -1;

  /* alarm if applicable */
  if (IN_FEATURE(SENSORS_SUBFEATURE_IN_ALARM)) {
    voltage->alarmNumber = IN_FEATURE_NR(SENSORS_SUBFEATURE_IN_ALARM);
  } else if (IN_FEATURE(SENSORS_SUBFEATURE_IN_MIN_ALARM)) {
    voltage->alarmNumber = IN_FEATURE_NR(SENSORS_SUBFEATURE_IN_MIN_ALARM);
  } else if (IN_FEATURE(SENSORS_SUBFEATURE_IN_MAX_ALARM)) {
    voltage->alarmNumber = IN_FEATURE_NR(SENSORS_SUBFEATURE_IN_MAX_ALARM);
  } else {
    voltage->alarmNumber = -1;
  }
  /* beep if applicable */
  if (IN_FEATURE(SENSORS_SUBFEATURE_IN_BEEP)) {
    voltage->beepNumber = IN_FEATURE_NR(SENSORS_SUBFEATURE_IN_ALARM);
  } else {
    voltage->beepNumber = -1;
  }
}

#define TEMP_FEATURE(x)      has_features[x - SENSORS_SUBFEATURE_TEMP_INPUT]
#define TEMP_FEATURE_NR(x)   feature_nrs[x - SENSORS_SUBFEATURE_TEMP_INPUT]
static void fillChipTemperature (FeatureDescriptor *temperature,
                                 const sensors_chip_name *name,
                                 const sensors_feature *feature)
{
  const int size = SENSORS_SUBFEATURE_TEMP_BEEP - SENSORS_SUBFEATURE_TEMP_INPUT + 1;
  short has_features[SENSORS_SUBFEATURE_TEMP_BEEP - SENSORS_SUBFEATURE_TEMP_INPUT + 1] = { 0, };
  int feature_nrs[SENSORS_SUBFEATURE_TEMP_BEEP - SENSORS_SUBFEATURE_TEMP_INPUT + 1];
  int pos = 0;

  temperature->rrd = rrdF1;
  temperature->type = DataType_temperature;

  getAvailableFeatures (name, feature, has_features,
                        feature_nrs, size, SENSORS_SUBFEATURE_TEMP_INPUT);

  temperature->dataNumbers[pos++] = TEMP_FEATURE_NR(SENSORS_SUBFEATURE_TEMP_INPUT);

  if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_MIN) &&
      TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_MAX)) {
    temperature->format = fmtTemps_minmax_1;
    temperature->dataNumbers[pos++] = TEMP_FEATURE_NR(SENSORS_SUBFEATURE_TEMP_MIN);
    temperature->dataNumbers[pos++] = TEMP_FEATURE_NR(SENSORS_SUBFEATURE_TEMP_MAX);
  } else if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_MAX) &&
             TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_MAX_HYST)) {
    temperature->format = fmtTemps_1;
    temperature->dataNumbers[pos++] = TEMP_FEATURE_NR(SENSORS_SUBFEATURE_TEMP_MAX);
    temperature->dataNumbers[pos++] = TEMP_FEATURE_NR(SENSORS_SUBFEATURE_TEMP_MAX_HYST);
  } else {
    temperature->format = fmtTemp_only;
  }
  
  /* terminate the list */
  temperature->dataNumbers[pos] = -1;

  /* alarm if applicable */
  if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_ALARM)) {
    temperature->alarmNumber = TEMP_FEATURE_NR(SENSORS_SUBFEATURE_TEMP_ALARM);
  } else if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_MAX_ALARM)) {
    temperature->alarmNumber = TEMP_FEATURE_NR(SENSORS_SUBFEATURE_TEMP_MAX_ALARM);
  } else {
    temperature->alarmNumber = -1;
  }
  /* beep if applicable */
  if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_BEEP)) {
    temperature->beepNumber = TEMP_FEATURE_NR(SENSORS_SUBFEATURE_TEMP_BEEP);
  } else {
    temperature->beepNumber = -1;
  }
}

#define FAN_FEATURE(x)      has_features[x - SENSORS_SUBFEATURE_FAN_INPUT]
#define FAN_FEATURE_NR(x)   feature_nrs[x - SENSORS_SUBFEATURE_FAN_INPUT]
static void fillChipFan (FeatureDescriptor *fan,
                         const sensors_chip_name *name,
                         const sensors_feature *feature)
{
  const int size = SENSORS_SUBFEATURE_FAN_BEEP - SENSORS_SUBFEATURE_FAN_INPUT + 1;
  short has_features[SENSORS_SUBFEATURE_FAN_BEEP - SENSORS_SUBFEATURE_FAN_INPUT + 1] = { 0, };
  int feature_nrs[SENSORS_SUBFEATURE_FAN_BEEP - SENSORS_SUBFEATURE_FAN_INPUT + 1];
  int pos = 0;

  fan->rrd = rrdF0;
  fan->type = DataType_rpm;

  getAvailableFeatures (name, feature, has_features,
                        feature_nrs, size, SENSORS_SUBFEATURE_FAN_INPUT);

  fan->dataNumbers[pos++] = FAN_FEATURE_NR(SENSORS_SUBFEATURE_FAN_INPUT);

  if (FAN_FEATURE(SENSORS_SUBFEATURE_FAN_MIN)) {
    fan->dataNumbers[pos++] = FAN_FEATURE_NR(SENSORS_SUBFEATURE_FAN_MIN);
    if (FAN_FEATURE(SENSORS_SUBFEATURE_FAN_DIV)) {
      fan->format = fmtFans_0;
      fan->dataNumbers[pos++] = FAN_FEATURE_NR(SENSORS_SUBFEATURE_FAN_DIV);
    } else {
      fan->format = fmtFans_nodiv_0;
    }
  } else {
      fan->format = fmtFan_only;
  }
  
  /* terminate the list */
  fan->dataNumbers[pos] = -1;

  /* alarm if applicable */
  if (FAN_FEATURE(SENSORS_SUBFEATURE_FAN_ALARM)) {
    fan->alarmNumber = FAN_FEATURE_NR(SENSORS_SUBFEATURE_FAN_ALARM);
  } else {
    fan->alarmNumber = -1;
  }
  /* beep if applicable */
  if (FAN_FEATURE(SENSORS_SUBFEATURE_FAN_BEEP)) {
    fan->beepNumber = FAN_FEATURE_NR(SENSORS_SUBFEATURE_FAN_BEEP);
  } else {
    fan->beepNumber = -1;
  }
}

static void fillChipVid (FeatureDescriptor *vid,
                         const sensors_chip_name *name,
                         const sensors_feature *feature)
{
  int i = 0;
  const sensors_subfeature *sub;

  sub = sensors_get_all_subfeatures(name, feature, &i);
  if (!sub)
    return;

  vid->format = fmtVolt_3;
  vid->rrd = rrdF3;
  vid->type = DataType_voltage;
  vid->alarmNumber = -1;
  vid->beepNumber = -1;
  vid->dataNumbers[0] = sub->number;
  vid->dataNumbers[1] = -1;
}

static void fillChipBeepEnable (FeatureDescriptor *beepen,
                                const sensors_chip_name *name,
                                const sensors_feature *feature)
{
  int i = 0;
  const sensors_subfeature *sub;

  sub = sensors_get_all_subfeatures(name, feature, &i);
  if (!sub)
    return;

  beepen->format = fmtSoundAlarm;
  beepen->rrd = rrdF0;
  beepen->type = DataType_other;
  beepen->alarmNumber = -1;
  beepen->beepNumber = -1;
  beepen->dataNumbers[0] = sub->number;
  beepen->dataNumbers[1] = -1;
}

static
FeatureDescriptor * generateChipFeatures (const sensors_chip_name *chip)
{
	int nr, count = 1;
	const sensors_feature *sensor;
	FeatureDescriptor *features;

	/* How many main features do we have? */
	nr = 0;
	while ((sensor = sensors_get_features(chip, &nr)))
		count++;

	/* Allocate the memory we need */
	features = calloc(count, sizeof(FeatureDescriptor));
	if (!features)
		return NULL;

	/* Fill in the data structures */
	count = 0;
	nr = 0;
	while ((sensor = sensors_get_features(chip, &nr))) {
		switch (sensor->type) {
		case SENSORS_FEATURE_TEMP:
			fillChipTemperature(&features[count], chip, sensor);
			break;
		case SENSORS_FEATURE_IN:
			fillChipVoltage(&features[count], chip, sensor);
			break;
		case SENSORS_FEATURE_FAN:
			fillChipFan(&features[count], chip, sensor);
			break;
		case SENSORS_FEATURE_VID:
			fillChipVid(&features[count], chip, sensor);
			break;
		case SENSORS_FEATURE_BEEP_ENABLE:
			fillChipBeepEnable(&features[count], chip, sensor);
			break;
		default:
			continue;
		}

		features[count].feature = sensor;
		count++;
	}

	return features;
}

ChipDescriptor * knownChips;

int initKnownChips (void)
{
  int nr, count = 1;
  const sensors_chip_name *name;

  /* How many chips do we have? */
  nr = 0;
  while ((name = sensors_get_detected_chips(NULL, &nr)))
    count++;

  /* Allocate the memory we need */
  knownChips = calloc(count, sizeof(ChipDescriptor));
  if (!knownChips)
    return 1;

  /* Fill in the data structures */
  count = 0;
  nr = 0;
  while ((name = sensors_get_detected_chips(NULL, &nr))) {
    knownChips[count].name = name;
    if ((knownChips[count].features = generateChipFeatures(name)))
      count++;
  }

  return 0;
}

void freeKnownChips (void)
{
  int index0;

  for (index0 = 0; knownChips[index0].features; index0++)
    free (knownChips[index0].features);
  free (knownChips);
}
