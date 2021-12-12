#include "_Plugin_Helper.h"
#ifdef USES_P117

// #######################################################################################################
// ############ Plugin 117 SCD30 I2C CO2, Humidity and Temperature Sensor ################################
// #######################################################################################################
// development version
// by: V0JT4
// this plugin is based on the Frogmore42 library
// written based code from https://github.com/Frogmore42/Sonoff-Tasmota/tree/development/lib/FrogmoreScd30

// Changelog:
//
// 2021-11-20 tonhuisman: Implement multi-instance support (using PluginStruct)
// 2021-09 tonhuisman: Moved from ESPEasyPluginPlayground to main repository

// Commands:
//   SCDGETABC - shows automatic calibration period in days, 0 = disable
//   SCDGETALT - shows altitude compensation configuration in meters above sea level
//   SCDGETTMP - shows temperature offset in degrees C

# define PLUGIN_117
# define PLUGIN_ID_117         117
# define PLUGIN_NAME_117       "Gases - CO2 SCD30 [TESTING]"
# define PLUGIN_VALUENAME1_117 "CO2"
# define PLUGIN_VALUENAME2_117 "Humidity"
# define PLUGIN_VALUENAME3_117 "Temperature"
# define PLUGIN_VALUENAME4_117 "CO2raw"

# include "./src/PluginStructs/P117_data_struct.h"

boolean Plugin_117(uint8_t function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number           = PLUGIN_ID_117;
      Device[deviceCount].Type               = DEVICE_TYPE_I2C;
      Device[deviceCount].VType              = Sensor_VType::SENSOR_TYPE_QUAD;
      Device[deviceCount].Ports              = 0;
      Device[deviceCount].PullUpOption       = false;
      Device[deviceCount].InverseLogicOption = false;
      Device[deviceCount].FormulaOption      = true;
      Device[deviceCount].ValueCount         = 4;
      Device[deviceCount].SendDataOption     = true;
      Device[deviceCount].TimerOption        = true;
      Device[deviceCount].GlobalSyncOption   = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_117);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_117));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_117));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_117));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_117));
      break;
    }

    case PLUGIN_I2C_HAS_ADDRESS:
    {
      success = event->Par1 == 0x61;
      break;
    }

    case PLUGIN_SET_DEFAULTS:
    {
      ExtraTaskSettings.TaskDeviceValueDecimals[0] = 0; // CO2 values are integers
      ExtraTaskSettings.TaskDeviceValueDecimals[3] = 0;
      break;
    }

    case PLUGIN_WEBFORM_SHOW_I2C_PARAMS:
    {
      html_TR_TD();
      html_TD();
      addHtml(F("<span style=\"color:red\">Tools->Advanced->I2C ClockStretchLimit should be set in range 20 to 150 msec.</span>"));
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      addFormNumericBox(F("Altitude"), F("plugin_117_SCD30_alt"), PCONFIG(0), 0, 2000);
      addUnit(F("0..2000 m"));
      addFormTextBox(F("Temp offset"), F("plugin_117_SCD30_tmp"), String(PCONFIG_FLOAT(0), 2), 5);
      addUnit(F("&deg;C"));
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      uint16_t alt = getFormItemInt(F("plugin_117_SCD30_alt"));

      if (alt > 2000) { alt = 2000; }
      PCONFIG(0)       = alt;
      PCONFIG_FLOAT(0) = getFormItemFloat(F("plugin_117_SCD30_tmp"));
      success          = true;
      break;
    }
    case PLUGIN_INIT:
    {
      initPluginTaskData(event->TaskIndex, new (std::nothrow) P117_data_struct(PCONFIG(0), PCONFIG_FLOAT(0)));
      P117_data_struct *P117_data = static_cast<P117_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr == P117_data) {
        return success;
      }
      success = true;
      break;
    }
    case PLUGIN_READ:
    {
      P117_data_struct *P117_data = static_cast<P117_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr == P117_data) {
        return success;
      }

      uint16_t scd30_CO2     = 0u;
      uint16_t scd30_CO2EAvg = 0u;
      float    scd30_Humid   = 0.0f;
      float    scd30_Temp    = 0.0f;

      switch (P117_data->read_sensor(&scd30_CO2, &scd30_CO2EAvg, &scd30_Temp, &scd30_Humid))
      {
        case ERROR_SCD30_NO_ERROR:
          UserVar[event->BaseVarIndex]     = scd30_CO2EAvg;
          UserVar[event->BaseVarIndex + 1] = scd30_Humid;
          UserVar[event->BaseVarIndex + 2] = scd30_Temp;
          UserVar[event->BaseVarIndex + 3] = scd30_CO2;

          if (scd30_CO2EAvg > 5000)
          {
            addLog(LOG_LEVEL_INFO, F("SCD30: Sensor saturated! > 5000 ppm"));
          }
          break;
        case ERROR_SCD30_NO_DATA:
        case ERROR_SCD30_CRC_ERROR:
        case ERROR_SCD30_CO2_ZERO:
          break;
        default:
        {
          P117_data->softReset();
          break;
        }
      }
      success = true;
      break;
    }
    case PLUGIN_WRITE:
    {
      P117_data_struct *P117_data = static_cast<P117_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr == P117_data) {
        return success;
      }
      String   command = parseString(string, 1);
      uint16_t value   = 0;
      String   log;
      float    temp;

      if (command.equals(F("scdgetabc")))
      {
        P117_data->getCalibrationType(&value);
        log    += F("ABC: ");
        log    += value;
        success = true;
      } else if (command.equals(F("scdgetalt")))
      {
        P117_data->getAltitudeCompensation(&value);
        log    += F("Altitude: ");
        log    += value;
        success = true;
      } else if (command.equals(F("scdgettmp")))
      {
        P117_data->getTemperatureOffset(&temp);
        log    += F("Temp offset: ");
        log    += String(temp, 2);
        success = true;
      }

      if (success) {
        SendStatus(event, log);
      } else {
        addLog(LOG_LEVEL_ERROR, F("SCD30: Unknown command."));
      }
      break;
    }
  }
  return success;
}

#endif // ifdef USES_P117
