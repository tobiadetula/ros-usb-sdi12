#ifndef TEROS12_SDI12_H
#define TEROS12_SDI12_H

/**
 * @file teros12_sdi12.h
 * @brief SDI-12 Command Set for the METER TEROS 12 Sensor.
 * * @note Replace 'a' with the sensor address (default is '0').
 * @note All responses are terminated with <CR><LF>.
 * * @reference METER Group TEROS 11/12 Integrator Guide:
 * https://publications.metergroup.com/Integrator%20Guide/18224%20TEROS%2011-12%20Integrator%20Guide.pdf
 */
namespace Teros12 {

    // --- BASIC SYSTEM COMMANDS ---
    
    /// @brief Acknowledge Active: Confirms sensor is online.
    /// @expected_output a<CR><LF> (where 'a' is the sensor address)
    const char* CMD_ACK_ACTIVE = "a!";
    
    /// @brief Send Identification: Returns sensor info.
    /// @expected_output a13METER   TER12 vvvxx...xx<CR><LF> 
    /// (13 = SDI-12 v1.3, vvv = firmware version, xx...xx = sensor specifics/serial)
    const char* CMD_SEND_ID = "aI!";
    
    /// @brief Address Query: Finds address when only ONE sensor is connected to the bus.
    /// @expected_output a<CR><LF>
    const char* CMD_ADDRESS_QUERY = "?!";
    
    /// @brief Change Address: Changes 'a' to 'b'. Replace 'b' with the new address.
    /// @expected_output b<CR><LF> (where 'b' is the new address)
    const char* CMD_CHANGE_ADDR = "aAb!";


    // --- MEASUREMENT COMMANDS ---
    
    /// @brief Start Measurement: Initiates VWC, Temperature, and EC reading.
    /// @expected_output atttn<CR><LF> 
    /// (ttt = max time in seconds to wait, n = number of values. For TEROS 12, n=3)
    const char* CMD_MEASURE = "aM!";
    
    /// @brief Start Concurrent Measurement: Same as M but for multiple sensors.
    /// @expected_output atttnn<CR><LF> 
    /// (nn = number of values. For TEROS 12, nn=03)
    const char* CMD_MEASURE_CONCURRENT = "aC!";

    /// @brief Start Measurement (M1): Alternative measurement (often raw values).
    /// @expected_output atttn<CR><LF>
    const char* CMD_MEASURE_RAW = "aM1!";

    /// @brief Get Data: Used after M, C, or V commands when the sensor is ready.
    /// @expected_output a+<VWC>±<Temp>+<EC><CR><LF>
    /// Example: 0+2105.32+23.1+0.102<CR><LF>
    const char* CMD_GET_DATA = "aD0!";


    // --- CONTINUOUS / METER SPECIFIC COMMANDS ---

    /// @brief Read DDI String: Returns the proprietary METER DDI serial string.
    /// @expected_output a<DDI_String><CR><LF> (Format depends on METER's DDI spec)
    const char* CMD_READ_DDI = "aR0!";


    // --- EXTENDED SETTINGS (X-COMMANDS) ---

    /// @brief Verification: Initiates sensor self-test.
    /// @expected_output atttn<CR><LF> (Use aD0! to read the results of the test)
    const char* CMD_VERIFY = "aV!";

} // namespace Teros12

#endif // TEROS12_SDI12_H